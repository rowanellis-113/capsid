// LocalDevHost implementation (docs/config-v2-design.md §5).
// Composes GenerationPool + ManagedListener + RoutingTable on all platforms.

#if defined(_WIN32) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0A00
#endif

#include "host/local_dev_host.h"

#include "capsid/runtime.h"
#include "client_ipc_metrics.h"
#include "host/active_state.h"
#include "host/binding_compile.h"
#include "host/binding_registry.h"
#include "host/config.h"
#include "host/generation_identity.h"
#include "host/generation_pool.h"
#include "host/managed_listener.h"
#include "host/metrics.h"
#include "host/policy_compiler.h"
#include "host/process_snapshot.h"
#include "host/routing_snapshot.h"
#include "host/structured_log.h"
#include "host/units.h"
#include "host/worker_executor.h"
#include "host/worker_recovery.h"
#include "json_escape.h"
#include "net_target.h"
#include "win32_compat.h"

#include <jansson.h>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <fcntl.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace capsid::host {

namespace asio = boost::asio;

namespace {

constexpr std::size_t kMaxLocalConfigBytes = 1024U * 1024U;
constexpr std::size_t kMaxLocalBundleBytes = 64U * 1024U * 1024U;
constexpr std::size_t kMaxLocalSecretBytes = 16U * 1024U;
constexpr std::size_t kMaxLocalSecretKeyIdBytes = 128U;

#if defined(_WIN32)
std::atomic<HANDLE> g_local_dev_term_event{nullptr};

BOOL WINAPI local_dev_console_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT: {
        const HANDLE event = g_local_dev_term_event.load(std::memory_order_acquire);
        if (event != nullptr) {
            SetEvent(event);
        }
        return TRUE;
    }
    default:
        return FALSE;
    }
}
#endif

enum class ReadOutcome { kOk, kMissing, kFailed };

#if defined(__APPLE__)
#define CAPSID_DEV_MTIME_SEC(st) ((st).st_mtimespec.tv_sec)
#define CAPSID_DEV_MTIME_NSEC(st) ((st).st_mtimespec.tv_nsec)
#define CAPSID_DEV_CTIME_SEC(st) ((st).st_ctimespec.tv_sec)
#define CAPSID_DEV_CTIME_NSEC(st) ((st).st_ctimespec.tv_nsec)
#elif defined(_WIN32)
#define CAPSID_DEV_MTIME_SEC(st) ((st).st_mtime)
#define CAPSID_DEV_MTIME_NSEC(st) 0
#define CAPSID_DEV_CTIME_SEC(st) ((st).st_ctime)
#define CAPSID_DEV_CTIME_NSEC(st) 0
#else
#define CAPSID_DEV_MTIME_SEC(st) ((st).st_mtim.tv_sec)
#define CAPSID_DEV_MTIME_NSEC(st) ((st).st_mtim.tv_nsec)
#define CAPSID_DEV_CTIME_SEC(st) ((st).st_ctim.tv_sec)
#define CAPSID_DEV_CTIME_NSEC(st) ((st).st_ctim.tv_nsec)
#endif

template <typename Stat> bool same_file_identity(const Stat& before, const Stat& after) {
    return before.st_dev == after.st_dev && before.st_ino == after.st_ino &&
           before.st_size == after.st_size &&
           CAPSID_DEV_MTIME_SEC(before) == CAPSID_DEV_MTIME_SEC(after) &&
           CAPSID_DEV_MTIME_NSEC(before) == CAPSID_DEV_MTIME_NSEC(after) &&
           CAPSID_DEV_CTIME_SEC(before) == CAPSID_DEV_CTIME_SEC(after) &&
           CAPSID_DEV_CTIME_NSEC(before) == CAPSID_DEV_CTIME_NSEC(after);
}

ReadOutcome read_local_file_safe(const std::string& path, std::size_t max_bytes,
                                 const char* cap_label, std::vector<std::uint8_t>* out,
                                 std::string* error) {
    if (path.empty() || path.find('\0') != std::string::npos) {
        *error = "invalid file path";
        return ReadOutcome::kFailed;
    }
    int fd = -1;
#if defined(_WIN32)
    const int wide_size =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, nullptr, 0);
    if (wide_size <= 0) {
        *error = "cannot encode " + path;
        return ReadOutcome::kFailed;
    }
    std::wstring wide_path(static_cast<std::size_t>(wide_size - 1), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, &wide_path[0],
                            wide_size) <= 0) {
        *error = "cannot encode " + path;
        return ReadOutcome::kFailed;
    }
    const HANDLE handle = CreateFileW(
        wide_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD saved = GetLastError();
        if (saved == ERROR_FILE_NOT_FOUND || saved == ERROR_PATH_NOT_FOUND) {
            return ReadOutcome::kMissing;
        }
        *error = "cannot open " + path;
        return ReadOutcome::kFailed;
    }
    FILE_ATTRIBUTE_TAG_INFO attributes = {};
    if (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &attributes,
                                      sizeof(attributes))) {
        CloseHandle(handle);
        *error = "cannot inspect " + path;
        return ReadOutcome::kFailed;
    }
    if ((attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        CloseHandle(handle);
        *error = path + " is not a regular file";
        return ReadOutcome::kFailed;
    }
    fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_RDONLY | _O_BINARY);
    if (fd < 0) {
        CloseHandle(handle);
        *error = "cannot open " + path;
        return ReadOutcome::kFailed;
    }
#else
    fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return ReadOutcome::kMissing;
        }
        *error = (errno == ELOOP) ? path + " is not a regular file" : "cannot open " + path;
        return ReadOutcome::kFailed;
    }
#endif

#if defined(_WIN32)
    struct _stat64 before = {};
    const bool stat_ok = _fstat64(fd, &before) == 0;
    const bool regular = stat_ok && (before.st_mode & _S_IFREG) != 0;
#else
    struct stat before = {};
    const bool stat_ok = fstat(fd, &before) == 0;
    const bool regular = stat_ok && S_ISREG(before.st_mode);
    const bool owned = stat_ok && (before.st_uid == geteuid() || before.st_uid == 0);
#endif
    if (!stat_ok) {
        close(fd);
        *error = "cannot stat " + path;
        return ReadOutcome::kFailed;
    }
    if (!regular) {
        close(fd);
        *error = path + " is not a regular file";
        return ReadOutcome::kFailed;
    }
#if !defined(_WIN32)
    if (!owned) {
        close(fd);
        *error = path + " is not owned by the current user";
        return ReadOutcome::kFailed;
    }
#endif
    if (before.st_size < 0 || static_cast<std::uint64_t>(before.st_size) > max_bytes) {
        close(fd);
        *error = path + " exceeds the " + cap_label;
        return ReadOutcome::kFailed;
    }

    out->resize(static_cast<std::size_t>(before.st_size));
    std::size_t offset = 0;
    while (offset < out->size()) {
#if defined(_WIN32)
        const std::size_t chunk = std::min<std::size_t>(out->size() - offset, 16U * 1024U * 1024U);
        const ssize_t count = read(fd, out->data() + offset, static_cast<unsigned int>(chunk));
#else
        const ssize_t count = read(fd, out->data() + offset, out->size() - offset);
#endif
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            *error = "cannot read " + path;
            return ReadOutcome::kFailed;
        }
        if (count == 0) {
            break;
        }
        offset += static_cast<std::size_t>(count);
    }
    if (offset != out->size()) {
        close(fd);
        *error = path + " changed while being read";
        return ReadOutcome::kFailed;
    }

#if defined(_WIN32)
    struct _stat64 after = {};
    const bool recheck_ok = _fstat64(fd, &after) == 0;
#else
    struct stat after = {};
    const bool recheck_ok = fstat(fd, &after) == 0;
#endif
    close(fd);
    if (!recheck_ok || !same_file_identity(before, after)) {
        *error = path + " changed while being read";
        return ReadOutcome::kFailed;
    }
    return ReadOutcome::kOk;
}

bool valid_application_id(const std::string& application) {
    ActiveStateDocument probe;
    probe.state = ActiveServiceState::kActive;
    probe.application = application;
    probe.version = "v0";
    probe.generation = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
    return encode_active_state_json(probe).ok;
}

bool valid_local_secret_key_id(const std::string& key_id) {
    if (key_id.empty() || key_id.size() > kMaxLocalSecretKeyIdBytes) {
        return false;
    }
    for (const unsigned char c : key_id) {
        const bool alnum =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!alnum && c != '.' && c != '_' && c != '-') {
            return false;
        }
    }
    return key_id.find("..") == std::string::npos;
}

bool valid_local_secret_value(const std::vector<std::uint8_t>& value) {
    if (value.size() > kMaxLocalSecretBytes) {
        return false;
    }
    const std::string_view bytes(reinterpret_cast<const char*>(value.data()), value.size());
    if (bytes.find('\0') != std::string_view::npos) {
        return false;
    }
    std::size_t index = 0;
    while (index < bytes.size()) {
        const unsigned char lead = static_cast<unsigned char>(bytes[index]);
        if (lead < 0x80) {
            ++index;
            continue;
        }
        std::size_t continuation = 0;
        std::uint32_t code_point = 0;
        if (lead >= 0xc2 && lead <= 0xdf) {
            continuation = 1;
            code_point = lead & 0x1f;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            continuation = 2;
            code_point = lead & 0x0f;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            continuation = 3;
            code_point = lead & 0x07;
        } else {
            return false;
        }
        if (index + continuation >= bytes.size()) {
            return false;
        }
        for (std::size_t step = 1; step <= continuation; ++step) {
            const unsigned char ch = static_cast<unsigned char>(bytes[index + step]);
            if ((ch & 0xc0) != 0x80) {
                return false;
            }
            code_point = (code_point << 6) | (ch & 0x3f);
        }
        const bool overlong = (continuation == 1 && code_point < 0x80) ||
                              (continuation == 2 && code_point < 0x800) ||
                              (continuation == 3 && code_point < 0x10000);
        if (overlong || code_point > 0x10ffff || (code_point >= 0xd800 && code_point <= 0xdfff)) {
            return false;
        }
        index += continuation + 1;
    }
    return true;
}

bool valid_local_entry_name(const std::string& entry) {
    if (entry.empty() || entry.size() > 255 || entry == "." || entry == ".." ||
        entry.find('/') != std::string::npos || entry.find('\\') != std::string::npos ||
        entry.find(':') != std::string::npos || entry.find('\0') != std::string::npos) {
        return false;
    }
    return entry.find("..") == std::string::npos;
}

bool read_secret_literal(const std::string& secrets_dir, const std::string& key_id,
                         const std::string& context_label, std::string* out_value,
                         std::string* error) {
    if (!valid_local_secret_key_id(key_id)) {
        *error = context_label + ": invalid valueFrom key id";
        return false;
    }
    const std::string secret_path = (std::filesystem::path(secrets_dir) / key_id).string();
    std::vector<std::uint8_t> secret_bytes;
    const ReadOutcome outcome =
        read_local_file_safe(secret_path, kMaxLocalSecretBytes,
                             "16 KiB limit of NUL-free UTF-8 text", &secret_bytes, error);
    if (outcome == ReadOutcome::kMissing) {
        *error = context_label + ": cannot find secret \"" + key_id + "\" in " + secrets_dir;
        return false;
    }
    if (outcome != ReadOutcome::kOk) {
        *error = context_label + ": valueFrom \"" + key_id + "\": " + *error;
        return false;
    }
    if (!valid_local_secret_value(secret_bytes)) {
        *error = context_label + ": valueFrom \"" + key_id +
                 "\": value must be at most 16384 bytes of NUL-free UTF-8 text";
        return false;
    }
    out_value->assign(reinterpret_cast<const char*>(secret_bytes.data()), secret_bytes.size());
    return true;
}

HostPolicy make_permissive_local_host(const AppRequest& app, bool strict_sandbox,
                                      std::uint32_t max_workers) {
    HostPolicy host;
    host.is_v2 = app.is_v2;
    host.strict_sandbox = strict_sandbox;
    host.module_allowlist = app.modules;
    for (const AppRequest::EnvRequest& request : app.env) {
        host.env_patterns.push_back(request.name);
    }
    host.fs_read_roots = app.fs_read;
    host.fetch_targets = app.fetch;
    host.storage_allowed = app.storage;
    host.storage_namespaces = app.storage_namespaces;
    host.stdio_allowed = app.stdio;
    host.stdio_streams = app.stdio_streams;
    host.max_workers = std::max<std::uint32_t>(1U, max_workers);
    return host;
}

std::string make_file_url_from_path(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(path, ec);
    if (ec) {
        abs = path;
    }
    std::string url_path = abs.generic_string();
    return std::string("file://") + (!url_path.empty() && url_path.front() == '/' ? "" : "/") +
           url_path;
}

std::string extract_host_from_tcp(std::string_view tcp_text) {
    std::string host;
    std::uint16_t port = 0;
    if (!tcp_text.empty() && tcp_text.front() == '[' && tcp_text.back() == ']') {
        return std::string(tcp_text.substr(1, tcp_text.size() - 2));
    }
    if (capsid::parse_tcp_endpoint(std::string(tcp_text), &host, &port,
                                   /*allow_zero_port=*/true)) {
        return host;
    }
    return std::string(tcp_text);
}

std::string format_host_port(std::string_view host, std::uint16_t port) {
    if (host.find(':') != std::string_view::npos && (host.empty() || host.front() != '[')) {
        return "[" + std::string(host) + "]:" + std::to_string(port);
    }
    return std::string(host) + ":" + std::to_string(port);
}

std::string probe_connect_address(std::string_view bound_host) {
    if (bound_host == "0.0.0.0" || bound_host.empty()) {
        return "127.0.0.1";
    }
    if (bound_host == "::") {
        return "::1";
    }
    return std::string(bound_host);
}

bool probe_health_once(const std::string& address, std::uint16_t port, const std::string& target,
                       const std::string& host_header, const std::string& header_name,
                       const std::string& header_value, std::uint64_t timeout_ms,
                       std::string* error) {
    boost::system::error_code ec;
    const asio::ip::address probe_address = asio::ip::make_address(address, ec);
    if (ec) {
        if (error != nullptr) {
            *error = "invalid probe address: " + address;
        }
        return false;
    }
    asio::io_context probe_io;
    asio::ip::tcp::socket socket(probe_io);
    socket.open(probe_address.is_v4() ? asio::ip::tcp::v4() : asio::ip::tcp::v6(), ec);
    if (ec) {
        if (error != nullptr) {
            *error = "probe socket open failed";
        }
        return false;
    }
    socket.non_blocking(true, ec);
    const asio::ip::tcp::endpoint endpoint(probe_address, port);
    socket.connect(endpoint, ec);
    if (ec && ec != asio::error::in_progress && ec != asio::error::would_block) {
        if (error != nullptr) {
            *error = "probe connect failed";
        }
        return false;
    }
    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    const auto remaining_ms = [&deadline]() -> long {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        return left.count() > 0 ? static_cast<long>(left.count()) : 0;
    };
    if (ec) {
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(socket.native_handle(), &write_fds);
        timeval timeout = {};
        timeout.tv_sec = remaining_ms() / 1000L;
        timeout.tv_usec = (remaining_ms() % 1000L) * 1000L;
        const int select_result = select(static_cast<int>(socket.native_handle()) + 1, nullptr,
                                         &write_fds, nullptr, &timeout);
        if (select_result <= 0) {
            if (error != nullptr) {
                *error =
                    select_result == 0 ? "probe connect timed out" : "probe connect select failed";
            }
            return false;
        }
        socket.connect(endpoint, ec);
        if (ec && ec != asio::error::already_connected) {
            if (error != nullptr) {
                *error = "probe connect failed";
            }
            return false;
        }
    }
    std::string request = "GET " + target + " HTTP/1.1\r\n";
    request += "Host: " + host_header + "\r\n";
    request += "Connection: close\r\n";
    if (!header_name.empty() && !header_value.empty()) {
        request += header_name + ": " + header_value + "\r\n";
    }
    request += "\r\n";
    socket.non_blocking(false, ec);
    asio::write(socket, asio::buffer(request), ec);
    if (ec) {
        if (error != nullptr) {
            *error = "probe request write failed";
        }
        return false;
    }
    std::string received;
    char buffer[512];
    while (received.find("\r\n") == std::string::npos) {
        const long left = remaining_ms();
        if (left <= 0) {
            if (error != nullptr) {
                *error = "probe response timed out";
            }
            return false;
        }
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket.native_handle(), &read_fds);
        timeval timeout = {};
        timeout.tv_sec = left / 1000L;
        timeout.tv_usec = (left % 1000L) * 1000L;
        const int select_result = select(static_cast<int>(socket.native_handle()) + 1, &read_fds,
                                         nullptr, nullptr, &timeout);
        if (select_result <= 0) {
            if (error != nullptr) {
                *error = select_result == 0 ? "probe response timed out"
                                            : "probe response select failed";
            }
            return false;
        }
        boost::system::error_code read_ec;
        const std::size_t received_bytes =
            socket.read_some(asio::buffer(buffer, sizeof(buffer)), read_ec);
        if (read_ec == asio::error::eof) {
            break;
        }
        if (read_ec) {
            if (error != nullptr) {
                *error = "probe response read failed";
            }
            return false;
        }
        received.append(buffer, received_bytes);
    }
    const std::string::size_type first_space = received.find(' ');
    if (first_space == std::string::npos) {
        if (error != nullptr) {
            *error = "probe response has no status line";
        }
        return false;
    }
    const std::string::size_type second_space = received.find(' ', first_space + 1);
    const std::string status_text = received.substr(
        first_space + 1,
        second_space == std::string::npos ? std::string::npos : second_space - first_space - 1);
    if (status_text.size() != 3 || status_text[0] != '2') {
        if (error != nullptr) {
            *error = "health check returned " + status_text;
        }
        return false;
    }
    return true;
}

bool write_ready_line(int ready_fd, const std::string& application, const std::string& address,
                      std::uint16_t port, std::string* error) {
    if (ready_fd < 0) {
        return true;
    }
    const std::string line = "{\"schema\":\"capsid-host-ready-v1\",\"app\":\"" +
                             capsid::json_escape(application) + "\",\"address\":\"" +
                             capsid::json_escape(address) + "\",\"port\":" + std::to_string(port) +
                             "}\n";
    std::size_t offset = 0;
    while (offset < line.size()) {
#if defined(_WIN32)
        const ssize_t written =
            capsid::win32::write_any_fd(ready_fd, line.data() + offset, line.size() - offset);
#else
        const ssize_t written = ::write(ready_fd, line.data() + offset, line.size() - offset);
#endif
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            if (error != nullptr) {
                *error = "failed to publish READY record to --ready-fd";
            }
            return false;
        }
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

struct WorkerLaunchSpec {
    std::string worker_path;
    std::string source_name;
    ProtocolLaunch protocol;
    bool ipc_metrics_enabled = false;
    std::vector<std::uint8_t> bundle;
    std::vector<EffectiveBinding> bindings;
    RuntimePolicy runtime_policy;
    capsid_resource_limits limits{};
    capsid_worker_config config{};
};

void emit_log(StructuredLog* log, LogLane lane, LogFields fields) {
    if (log != nullptr) {
        log->log(lane, std::move(fields));
    }
}

} // namespace

bool discover_sibling_worker_path(std::string* out_path) {
    if (out_path == nullptr) {
        return false;
    }
    std::filesystem::path self_path;
#if defined(_WIN32)
    std::wstring wide_buf(32768, L'\0');
    const DWORD len =
        GetModuleFileNameW(nullptr, wide_buf.data(), static_cast<DWORD>(wide_buf.size()));
    if (len == 0 || len >= wide_buf.size()) {
        return false;
    }
    wide_buf.resize(len);
    const int utf8_len =
        WideCharToMultiByte(CP_UTF8, 0, wide_buf.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8_len <= 1) {
        return false;
    }
    std::string utf8_self(static_cast<std::size_t>(utf8_len - 1), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide_buf.c_str(), -1, &utf8_self[0], utf8_len, nullptr,
                            nullptr) <= 0) {
        return false;
    }
    self_path = std::filesystem::path(utf8_self);
    const std::filesystem::path candidate = self_path.parent_path() / "capsid-worker.exe";
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0) {
        return false;
    }
    std::string buf(size, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) != 0) {
        return false;
    }
    std::error_code ec;
    self_path = std::filesystem::weakly_canonical(std::filesystem::path(buf.c_str()), ec);
    if (ec) {
        self_path = std::filesystem::path(buf.c_str());
    }
    const std::filesystem::path candidate = self_path.parent_path() / "capsid-worker";
#else
    char buf[PATH_MAX];
    const ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) {
        return false;
    }
    buf[len] = '\0';
    self_path = std::filesystem::path(buf);
    const std::filesystem::path candidate = self_path.parent_path() / "capsid-worker";
#endif
    std::error_code ec;
    if (!std::filesystem::is_regular_file(candidate, ec) || ec) {
        return false;
    }
    *out_path = candidate.string();
    return true;
}

class LocalDevHostImpl : public std::enable_shared_from_this<LocalDevHostImpl> {
  public:
    explicit LocalDevHostImpl(LocalDevHostOptions options)
        : options_(std::move(options)), routing_(std::make_shared<RoutingTable>()) {}

    ~LocalDevHostImpl() {
        request_stop();
        std::string ignored;
        wait(&ignored);
    }

    bool start(const std::vector<std::uint8_t>* inline_bundle, std::string* error);
    void request_stop();
    void begin_drain(std::uint64_t deadline_ms);
    bool wait(std::string* error);
    int run(const std::vector<std::uint8_t>* inline_bundle);

    std::uint16_t bound_port() const { return bound_port_.load(std::memory_order_acquire); }

    std::size_t active_workers() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return pool_ ? static_cast<std::size_t>(pool_->ready_workers()) : 0U;
    }

    std::vector<capsid::WorkerExitStatus> worker_exit_statuses() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return pool_ ? pool_->worker_exit_statuses() : cached_exit_statuses_;
    }

    LocalDevHost::DrainMetrics drain_metrics() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        LocalDevHost::DrainMetrics metrics;
        metrics.draining = draining_;
        metrics.finished = drain_finished_;
        metrics.forced_cancellations =
            draining_ ? (pool_ ? pool_->forced_drain_cancellations() : cached_forced_cancellations_)
                      : 0;
        if (draining_) {
            const auto end = drain_finished_ ? drain_ended_at_ : std::chrono::steady_clock::now();
            metrics.total_ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(end - drain_began_at_)
                    .count());
        }
        return metrics;
    }

    bool stop_requested() const { return stop_requested_.load(std::memory_order_acquire); }
    StructuredLog* log() const { return options_.log; }

  private:
    bool fail_start(std::string* out_error, std::string message) {
        if (out_error != nullptr) {
            *out_error = std::move(message);
        }
        return false;
    }

    LocalDevHostOptions options_;
    std::shared_ptr<RoutingTable> routing_;

    mutable std::mutex state_mutex_;
    bool started_ = false;
    bool draining_ = false;
    bool drain_finished_ = false;
    bool drain_had_live_workers_ = false;
    std::chrono::steady_clock::time_point drain_began_at_{};
    std::chrono::steady_clock::time_point drain_ended_at_{};
    std::uint64_t drain_deadline_ms_ = 0;

    std::shared_ptr<GenerationPool> pool_;
    std::unique_ptr<ManagedListener> listener_;
    std::vector<capsid::WorkerExitStatus> cached_exit_statuses_;
    std::uint64_t cached_forced_cancellations_ = 0;
    std::atomic<std::uint16_t> bound_port_{0};
    std::atomic<bool> stop_requested_{false};
    std::once_flag wait_once_;
    bool wait_result_ = true;
    int start_exit_code_ = 2;
};

bool LocalDevHostImpl::start(const std::vector<std::uint8_t>* inline_bundle, std::string* error) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (started_ || stop_requested_.load(std::memory_order_acquire)) {
            return fail_start(error, "local host already started or stopped");
        }
        started_ = true;
    }

    const std::string app_id =
        options_.application.empty() ? std::string("local") : options_.application;
    if (!valid_application_id(app_id)) {
        return fail_start(error, "invalid application identifier: " + app_id);
    }

    std::string worker_path = options_.worker_path;
    if (worker_path.empty() && !discover_sibling_worker_path(&worker_path)) {
        return fail_start(error, "cannot locate capsid-worker; pass --worker <path>");
    }

    // 1. Read and schema-validate capsid.json (inline override or app_dir/capsid.json).
    std::vector<std::uint8_t> capsid_bytes;
    std::string capsid_label;
    std::filesystem::path resolved_json_dir = options_.app_dir.empty()
                                                  ? std::filesystem::path(".")
                                                  : std::filesystem::path(options_.app_dir);
    if (options_.capsid_json_text.has_value()) {
        capsid_bytes.assign(options_.capsid_json_text->begin(), options_.capsid_json_text->end());
        capsid_label =
            !options_.capsid_json_path.empty() ? options_.capsid_json_path : "capsid.json";
    } else {
        const std::filesystem::path json_path =
            !options_.capsid_json_path.empty() ? std::filesystem::path(options_.capsid_json_path)
                                               : (resolved_json_dir / "capsid.json");
        capsid_label = json_path.string();
        std::string read_error;
        const ReadOutcome outcome =
            read_local_file_safe(capsid_label, kMaxLocalConfigBytes, "1 MiB capsid.json cap",
                                 &capsid_bytes, &read_error);
        if (outcome == ReadOutcome::kMissing) {
            if (options_.capsid_json_required) {
                return fail_start(error, "cannot find " + capsid_label);
            }
            static constexpr std::string_view kDefaultAppV2 =
                R"json({"apiVersion":"capsid/app-v2"})json";
            capsid_bytes.assign(kDefaultAppV2.begin(), kDefaultAppV2.end());
        } else if (outcome != ReadOutcome::kOk) {
            return fail_start(error, read_error);
        }
        if (json_path.has_parent_path()) {
            resolved_json_dir = json_path.parent_path();
        }
    }

    const std::string capsid_text(capsid_bytes.begin(), capsid_bytes.end());
    const ConfigValidationResult app_valid =
        validate_config_json(ConfigDocument::kApplication, capsid_text);
    if (!app_valid.ok) {
        return fail_start(error, capsid_label + " rejected at " + app_valid.error.path + ": " +
                                     app_valid.error.message);
    }

    // 2. Extract optional "entry" and resolve bundle bytes safely.
    bool has_entry = false;
    std::string entry_name = "bundle.mjs";
    {
        json_error_t parse_err;
        json_t* root =
            json_loadb(capsid_text.data(), capsid_text.size(), JSON_REJECT_DUPLICATES, &parse_err);
        if (root == nullptr || !json_is_object(root)) {
            if (root != nullptr) {
                json_decref(root);
            }
            return fail_start(error, capsid_label + ": invalid capsid.json");
        }
        json_t* entry = json_object_get(root, "entry");
        if (json_is_string(entry)) {
            has_entry = true;
            entry_name = json_string_value(entry);
            if (!valid_local_entry_name(entry_name)) {
                json_decref(root);
                return fail_start(error,
                                  capsid_label +
                                      ": entry must be a plain file name inside the app directory");
            }
        }
        json_decref(root);
    }
    (void)has_entry;

    AppRequest app;
    std::string app_parse_error;
    if (!parse_app_request(capsid_bytes, &app, &app_parse_error)) {
        return fail_start(error, capsid_label + ": " + app_parse_error);
    }

    std::vector<AppBindingRequest> binding_requests;
    if (!parse_app_bindings(capsid_bytes, &binding_requests, &app_parse_error)) {
        return fail_start(error, capsid_label + ": " + app_parse_error);
    }

    std::vector<std::uint8_t> bundle_bytes;
    const std::filesystem::path bundle_path = resolved_json_dir / entry_name;
    if (inline_bundle != nullptr) {
        bundle_bytes = *inline_bundle;
    } else {
        std::string bundle_error;
        const ReadOutcome bundle_outcome =
            read_local_file_safe(bundle_path.string(), kMaxLocalBundleBytes, "64 MiB bundle cap",
                                 &bundle_bytes, &bundle_error);
        if (bundle_outcome == ReadOutcome::kMissing) {
            return fail_start(error, "cannot find bundle " + bundle_path.string());
        }
        if (bundle_outcome != ReadOutcome::kOk) {
            return fail_start(error, bundle_error);
        }
    }

    // 3. Load optional --host-config or construct permissive local Host defaults.
    bool has_host_config = !options_.host_config_path.empty();
    ParsedHostConfig host_cfg;
    ListenerConfig listener_cfg;
    std::string effective_bindings_dir = options_.bindings_dir;
    ChannelBudgetLimits service_limits = options_.channel_service_limits;
    RecoveryConfig recovery_cfg;

    if (has_host_config) {
        std::vector<std::uint8_t> host_bytes;
        std::string host_read_err;
        const ReadOutcome host_outcome =
            read_local_file_safe(options_.host_config_path, kMaxLocalConfigBytes,
                                 "1 MiB host.json cap", &host_bytes, &host_read_err);
        if (host_outcome == ReadOutcome::kMissing) {
            return fail_start(error, "cannot find " + options_.host_config_path);
        }
        if (host_outcome != ReadOutcome::kOk) {
            return fail_start(error, host_read_err);
        }
        const std::string host_text(host_bytes.begin(), host_bytes.end());
        const ConfigValidationResult host_valid =
            validate_config_json(ConfigDocument::kHost, host_text);
        if (!host_valid.ok) {
            return fail_start(error, options_.host_config_path + " rejected at " +
                                         host_valid.error.path + ": " + host_valid.error.message);
        }
        std::string parse_err;
        if (!parse_host_config(host_text, &host_cfg, &parse_err)) {
            return fail_start(error, options_.host_config_path + ": " + parse_err);
        }
        // In local dev mode, sandbox strictness stays controlled by --strict-sandbox
        // so production host-v2 docs can run unchanged on macOS/Windows/dev Linux.
        host_cfg.policy.strict_sandbox = options_.strict_sandbox;
        if (effective_bindings_dir.empty()) {
            effective_bindings_dir = host_cfg.bindings_root;
        }
        service_limits = host_cfg.capacity.channels;
        recovery_cfg = host_cfg.recovery;

        if (!options_.listener_override.has_value()) {
            if (host_cfg.listeners.size() != 1) {
                return fail_start(error, options_.host_config_path +
                                             ": local mode requires exactly one listener");
            }
            listener_cfg = host_cfg.listeners[0];
        }
    }

    if (options_.listener_override.has_value()) {
        listener_cfg = *options_.listener_override;
    } else if (!has_host_config) {
        listener_cfg.name = "local";
        listener_cfg.tcp = options_.listen;
        listener_cfg.public_scheme = "http";
        listener_cfg.public_authority = options_.listen;
        listener_cfg.public_origin_auto = true;
        listener_cfg.routing.mode = "single";
        listener_cfg.routing.single_app = app_id;
    }

    if (options_.listen_explicit && !options_.listener_override.has_value()) {
        listener_cfg.tcp = options_.listen;
        if (listener_cfg.public_origin_auto || !has_host_config) {
            listener_cfg.public_scheme = "http";
            listener_cfg.public_authority = options_.listen;
            listener_cfg.public_origin_auto = true;
        }
    }
    if (listener_cfg.routing.mode == "single" && listener_cfg.routing.single_app.empty()) {
        listener_cfg.routing.single_app = app_id;
    }

    if (options_.workers_override > 0) {
        constexpr std::uint32_t kMaxLocalWorkers = 256U;
        if (options_.workers_override > kMaxLocalWorkers) {
            return fail_start(error, "--workers exceeds the local worker limit (" +
                                         std::to_string(kMaxLocalWorkers) + ")");
        }
        if (has_host_config) {
            if ((host_cfg.policy.max_workers != 0 &&
                 options_.workers_override > host_cfg.policy.max_workers) ||
                (host_cfg.policy.max_pool_workers != 0 &&
                 options_.workers_override > host_cfg.policy.max_pool_workers)) {
                return fail_start(error, "--workers exceeds host.json ceiling");
            }
        }
        app.has_workers = true;
        app.workers = options_.workers_override;
        app.min_ready = options_.workers_override;
    }

    const HostPolicy host_policy =
        has_host_config
            ? host_cfg.policy
            : make_permissive_local_host(app, options_.strict_sandbox,
                                         std::max<std::uint32_t>(1U, options_.workers_override > 0
                                                                         ? options_.workers_override
                                                                         : app.workers));

    // 4. Resolve env valueFrom against --secrets-dir.
    std::vector<EffectiveEnvEntry> resolved_env;
    resolved_env.reserve(app.env.size());
    for (const AppRequest::EnvRequest& request : app.env) {
        EffectiveEnvEntry entry;
        entry.name = request.name;
        if (request.from_secret) {
            if (options_.secrets_dir.empty()) {
                return fail_start(error, capsid_label +
                                             ": env valueFrom is unavailable in local mode "
                                             "without --secrets-dir");
            }
            if (!read_secret_literal(options_.secrets_dir, request.secret_key_id,
                                     capsid_label + ": env " + request.name, &entry.literal,
                                     error)) {
                return false;
            }
            entry.from_secret = false;
        } else {
            entry.literal = request.literal;
        }
        resolved_env.push_back(std::move(entry));
    }

    // 5. Compile policy through the authoritative compiler.
    AppRequest compile_input = app;
    for (AppRequest::EnvRequest& request : compile_input.env) {
        if (request.from_secret) {
            for (const EffectiveEnvEntry& resolved : resolved_env) {
                if (resolved.name == request.name) {
                    request.from_secret = false;
                    request.secret_key_id.clear();
                    request.literal = resolved.literal;
                    break;
                }
            }
        }
    }
    const PolicyCompileResult compiled = compile_policy(host_policy, compile_input, resolved_env);
    if (!compiled.ok) {
        return fail_start(error, capsid_label + ": " + compiled.error);
    }

    // 6. Compile declared bindings against --bindings-dir (or host.json paths.bindings).
    std::vector<EffectiveBinding> compiled_bindings;
    if (!binding_requests.empty()) {
        std::shared_ptr<const BindingRegistrySnapshot> registry = options_.binding_registry;
        if (registry == nullptr) {
            if (effective_bindings_dir.empty()) {
                return fail_start(error, capsid_label +
                                             ": bindings require --bindings-dir (or host "
                                             "paths.bindings)");
            }
            auto scanned = std::make_shared<BindingRegistrySnapshot>();
            std::string scan_error;
#if defined(_WIN32)
            const std::vector<BindingOwnerId> allowed_owners = {0};
#else
            const std::vector<BindingOwnerId> allowed_owners = {0, geteuid()};
#endif
            if (!scan_bindings_root(effective_bindings_dir, allowed_owners, scanned.get(),
                                    &scan_error)) {
                return fail_start(error, "bindings directory rejected: " + scan_error);
            }
            registry = std::move(scanned);
        }
        BindingCompileResult binding_result =
            compile_effective_bindings(*registry, binding_requests);
        if (!binding_result.ok) {
            return fail_start(error, capsid_label + ": " + binding_result.error);
        }
        compiled_bindings = std::move(binding_result.bindings);
        for (EffectiveBinding& binding : compiled_bindings) {
            for (const BindingSecretRef& ref : binding.request.secrets) {
                if (options_.secrets_dir.empty()) {
                    return fail_start(error, capsid_label +
                                                 ": binding secrets are unavailable in local mode "
                                                 "without --secrets-dir: " +
                                                 binding.id);
                }
                EffectiveBinding::SecretValue secret_val;
                secret_val.name = ref.name;
                secret_val.key_id = ref.key_id;
                if (!read_secret_literal(options_.secrets_dir, ref.key_id,
                                         capsid_label + ": binding " + binding.id,
                                         &secret_val.value, error)) {
                    return false;
                }
                binding.secret_values.push_back(std::move(secret_val));
            }
        }
    }

    const ResolvedRecoveryPolicy recovery_resolved = resolve_recovery_policy(recovery_cfg);
    if (!recovery_resolved.ok) {
        return fail_start(error, "invalid recovery policy: " + recovery_resolved.error);
    }

    // 7. Build long-lived WorkerLaunchSpec for GenerationPool creation & replacements.
    auto spec = std::make_shared<WorkerLaunchSpec>();
    spec->worker_path = worker_path;
    spec->source_name =
        !options_.source_name.empty() ? options_.source_name : make_file_url_from_path(bundle_path);
    spec->protocol =
        (!options_.protocol_explicit && has_host_config && host_cfg.channels.configured)
            ? ProtocolLaunch::from(host_cfg.channels)
            : options_.protocol;
    const char* metrics_env = std::getenv("CAPSID_HOST_IPC_METRICS");
    spec->ipc_metrics_enabled = metrics_env != nullptr && std::strcmp(metrics_env, "1") == 0;
    spec->bundle = std::move(bundle_bytes);
    spec->bindings = std::move(compiled_bindings);

    std::vector<std::pair<std::string, std::string>> env_values;
    env_values.reserve(compiled.effective.env.size());
    for (const EffectiveEnvEntry& entry : compiled.effective.env) {
        env_values.emplace_back(entry.name, entry.literal);
    }
    std::string policy_build_err;
    if (!build_runtime_policy(compiled.effective, env_values, &spec->runtime_policy,
                              &policy_build_err)) {
        return fail_start(error, capsid_label + ": " + policy_build_err);
    }

    std::uint32_t stream_window = options_.initial_stream_window;
    if (stream_window == 65536U) {
        if (const char* win_env = std::getenv("CAPSID_INITIAL_STREAM_WINDOW")) {
            errno = 0;
            char* end = nullptr;
            const unsigned long long parsed = std::strtoull(win_env, &end, 10);
            if (win_env[0] >= '0' && win_env[0] <= '9' && errno == 0 && end != nullptr &&
                *end == '\0' && parsed > 0 &&
                parsed <=
                    static_cast<unsigned long long>(std::numeric_limits<std::uint32_t>::max())) {
                stream_window = static_cast<std::uint32_t>(parsed);
            }
        }
    }

    capsid_worker_config_init(&spec->config);
    capsid_resource_limits_init(&spec->limits);
    spec->config.worker_path = spec->worker_path.c_str();
    spec->config.request_timeout_ms = compiled.effective.request_timeout_ms;
    spec->config.initial_stream_window = stream_window;
    spec->config.strict_sandbox = compiled.effective.strict_sandbox ? 1U : 0U;
    spec->config.max_inflight_requests = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        compiled.effective.requests_per_worker, std::numeric_limits<std::uint32_t>::max()));
    spec->runtime_policy.apply(&spec->config);
    if (compiled.effective.js_heap_bytes > 0) {
        spec->config.js_heap_limit = compiled.effective.js_heap_bytes;
    }
#if defined(CAPSID_ASAN_BUILD) || defined(CAPSID_TSAN_BUILD) || defined(__SANITIZE_ADDRESS__) ||   \
    defined(__SANITIZE_THREAD__)
    const bool forward_address_space_limit = false;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
    const bool forward_address_space_limit = false;
#else
    const bool forward_address_space_limit = true;
#endif
#else
    const bool forward_address_space_limit = true;
#endif
    if (forward_address_space_limit && compiled.effective.process_address_bytes > 0) {
        spec->config.process_memory_limit = compiled.effective.process_address_bytes;
    }
    if (compiled.effective.file_descriptors > 0) {
        if (compiled.effective.file_descriptors > std::numeric_limits<std::uint32_t>::max()) {
            return fail_start(error, "file descriptor limit exceeds worker range");
        }
        spec->limits.enabled_fields |= CAPSID_RESOURCE_LIMIT_FILE_DESCRIPTORS;
        spec->limits.file_descriptors =
            static_cast<std::uint32_t>(compiled.effective.file_descriptors);
        spec->config.resource_limits = &spec->limits;
    }

    WorkerExecutor::WorkerFactory factory = [spec](capsid_worker** out_worker,
                                                   std::string* factory_error) -> bool {
        capsid_worker* worker = nullptr;
        const capsid_result spawn_result =
            spawn_with_protocol(&spec->config, spec->protocol, &worker);
        if (spawn_result != CAPSID_OK) {
            if (factory_error != nullptr) {
                *factory_error =
                    "worker spawn failed: " + std::string(capsid_result_string(spawn_result));
            }
            return false;
        }
        if (!load_effective_bindings_into_worker(worker, spec->bindings, factory_error)) {
            capsid_worker_destroy(worker);
            return false;
        }
        const capsid_result load_result = capsid_worker_load_bundle_named(
            worker, spec->bundle.data(), spec->bundle.size(), spec->source_name.c_str());
        if (load_result != CAPSID_OK) {
            if (factory_error != nullptr) {
                *factory_error =
                    "bundle load failed: " + std::string(capsid_result_string(load_result));
            }
            capsid_worker_destroy(worker);
            return false;
        }
        const capsid_result flush_result = capsid_worker_flush(worker);
        if (flush_result != CAPSID_OK && flush_result != CAPSID_WOULD_BLOCK) {
            if (factory_error != nullptr) {
                *factory_error =
                    "bundle flush failed: " + std::string(capsid_result_string(flush_result));
            }
            capsid_worker_destroy(worker);
            return false;
        }
        if (spec->ipc_metrics_enabled) {
            client_ipc_metrics_enable(worker, true);
        }
        *out_worker = worker;
        return true;
    };
    factory.expected_ready = expected_worker_ready_proof(spec->bindings, spec->config);

    // 8. Start GenerationPool and ManagedListener, publish route, run startup health probe.
    std::shared_ptr<ServiceChannelBudget> channel_budget =
        options_.channel_service_budget ? options_.channel_service_budget
                                        : std::make_shared<ServiceChannelBudget>(service_limits);
    if (options_.metrics != nullptr) {
        options_.metrics->observe_channel_budget(channel_budget);
    }

    const std::uint64_t body_limit = options_.max_request_body_bytes_override.value_or(
        effective_max_request_body_bytes(compiled.effective.memory_bytes));

    GenerationPoolOptions pool_options;
    pool_options.application_id = app_id;
    pool_options.version = "local";
    pool_options.generation_digest = sha256_hex(spec->bundle);
    pool_options.workers = compiled.effective.workers;
    pool_options.factory = std::move(factory);
    pool_options.channel_budget = channel_budget;
    pool_options.recovery = recovery_resolved.policy;
    pool_options.max_inflight_per_worker = compiled.effective.requests_per_worker;
    pool_options.max_queued_requests = compiled.effective.queue_requests;
    pool_options.queue_header_bytes = compiled.effective.queue_header_bytes;
    pool_options.queue_timeout_ms = compiled.effective.queue_timeout_ms;
    pool_options.max_streaming_inflight_per_worker =
        compiled.effective.max_streaming_inflight_per_worker;
    pool_options.stream_idle_timeout_ms = compiled.effective.stream_idle_timeout_ms;
    pool_options.write_timeout_ms = compiled.effective.write_timeout_ms;
    pool_options.max_request_body_bytes = body_limit;
    pool_options.replace_exited_workers = options_.replace_exited_workers;
    pool_options.round_robin_tie_break = true;
    pool_options.drain_deadline = options_.drain_deadline;
    pool_options.log = options_.log;
    pool_options.metrics = options_.metrics;
    pool_options.before_forced_shutdown = [weak = weak_from_this()] {
        if (const std::shared_ptr<LocalDevHostImpl> self = weak.lock()) {
            ManagedListener* listener_ptr = nullptr;
            {
                std::lock_guard<std::mutex> lock(self->state_mutex_);
                listener_ptr = self->listener_.get();
            }
            if (listener_ptr != nullptr) {
                auto done = std::make_shared<std::promise<void>>();
                auto settled = std::make_shared<std::atomic<bool>>(false);
                std::future<void> future = done->get_future();
                listener_ptr->request_stop([done, settled] {
                    if (!settled->exchange(true)) {
                        done->set_value();
                    }
                });
                future.wait_for(std::chrono::seconds(2));
            }
        }
    };
    pool_options.on_drain_complete = [weak = weak_from_this()] {
        if (const std::shared_ptr<LocalDevHostImpl> self = weak.lock()) {
            ManagedListener* listener_ptr = nullptr;
            {
                std::lock_guard<std::mutex> lock(self->state_mutex_);
                if (self->draining_) {
                    if (self->drain_had_live_workers_) {
                        self->drain_finished_ = true;
                        self->drain_ended_at_ = std::chrono::steady_clock::now();
                    }
                    listener_ptr = self->listener_.get();
                }
            }
            if (listener_ptr != nullptr) {
                listener_ptr->request_stop();
            }
        }
    };

    std::string pool_error;
    std::shared_ptr<GenerationPool> pool =
        GenerationPool::create(std::move(pool_options), &pool_error);
    if (pool == nullptr) {
        return fail_start(error, pool_error);
    }
    if (stop_requested_.load(std::memory_order_acquire)) {
        pool->stop_and_join();
        return fail_start(error, "local host stopped during startup");
    }

    routing_->publish(RoutingSnapshot::build({{app_id, pool}}));

    if (options_.metrics != nullptr) {
        std::weak_ptr<RoutingTable> weak_routing = routing_;
        options_.metrics->set_process_snapshot_provider(process_snapshot_provider([weak_routing] {
            if (const auto rt = weak_routing.lock()) {
                return static_cast<pid_t>(rt->worker_process_id());
            }
            return static_cast<pid_t>(0);
        }));
    }

    if (!has_host_config && !options_.listener_override.has_value()) {
        // Inherit compiled listener-facing limits when no host.json configured them.
        if (listener_cfg.limits.stream_idle_timeout_ms == 0) {
            listener_cfg.limits.stream_idle_timeout_ms = compiled.effective.stream_idle_timeout_ms;
        }
        if (listener_cfg.limits.write_timeout_ms == 0) {
            listener_cfg.limits.write_timeout_ms = compiled.effective.write_timeout_ms;
        }
        if (options_.channel_listener_limits.channels != 0) {
            listener_cfg.limits.channels = options_.channel_listener_limits.channels;
        }
        if (options_.channel_listener_limits.queue_bytes != 0) {
            listener_cfg.limits.channel_queue_bytes = options_.channel_listener_limits.queue_bytes;
        }
    }

    ManagedListenerOptions listener_options;
    listener_options.config = listener_cfg;
    listener_options.routing = routing_;
    listener_options.expected_application = app_id;
    listener_options.log = options_.log;
    listener_options.metrics = options_.metrics;
    listener_options.channel_service_budget = channel_budget;
    listener_options.initial_stream_window = stream_window;
    listener_options.max_request_body_bytes = body_limit;
    auto listener = std::make_unique<ManagedListener>(std::move(listener_options));
    std::string listener_error;
    if (!listener->start(&listener_error)) {
        pool->stop_and_join();
        return fail_start(error, listener_error);
    }

    const std::uint16_t port = listener->bound_port();
    const std::string listen_host = extract_host_from_tcp(listener_cfg.tcp);

    if (compiled.effective.health_check.configured) {
        std::string target = compiled.effective.health_check.path;
        std::string host_hdr = format_host_port(probe_connect_address(listen_host), port);
        std::string extra_hdr_name;
        std::string extra_hdr_value;
        if (listener_cfg.routing.mode == "path") {
            target = "/@capsid/" + app_id + compiled.effective.health_check.path;
        } else if (listener_cfg.routing.mode == "subdomain") {
            host_hdr = app_id + listener_cfg.routing.suffix + ":" + std::to_string(port);
        } else if (listener_cfg.routing.mode == "header") {
            extra_hdr_name =
                listener_cfg.routing.header.empty() ? "capsid-app" : listener_cfg.routing.header;
            extra_hdr_value = app_id;
        }
        std::string probe_err;
        if (!probe_health_once(probe_connect_address(listen_host), port, target, host_hdr,
                               extra_hdr_name, extra_hdr_value,
                               compiled.effective.health_check.timeout_ms, &probe_err)) {
            listener->request_stop();
            std::string ignored;
            listener->wait(&ignored);
            pool->stop_and_join();
            start_exit_code_ = 1;
            return fail_start(error, "startup health check failed: " + probe_err);
        }
    }

    if (stop_requested_.load(std::memory_order_acquire)) {
        listener->request_stop();
        std::string ignored;
        listener->wait(&ignored);
        pool->stop_and_join();
        return fail_start(error, "local host stopped during startup");
    }

    if (options_.write_ready_record && options_.ready_fd >= 0) {
        std::string ready_error;
        if (!write_ready_line(options_.ready_fd, app_id, listen_host, port, &ready_error)) {
            listener->request_stop();
            std::string ignored;
            listener->wait(&ignored);
            pool->stop_and_join();
            start_exit_code_ = 1;
            return fail_start(error, ready_error);
        }
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        pool_ = std::move(pool);
        listener_ = std::move(listener);
        bound_port_.store(port, std::memory_order_release);
    }
    if (stop_requested_.load(std::memory_order_acquire)) {
        request_stop();
    }
    emit_log(options_.log, LogLane::kControl,
             {.event = log_events::kStartup,
              .app = app_id,
              .result = "ok",
              .message = "listening on " + format_host_port(listen_host, port)});
    return true;
}

void LocalDevHostImpl::request_stop() {
    stop_requested_.store(true, std::memory_order_release);
#if defined(_WIN32)
    const HANDLE event = g_local_dev_term_event.load(std::memory_order_acquire);
    if (event != nullptr) {
        SetEvent(event);
    }
#endif
    std::shared_ptr<GenerationPool> pool;
    ManagedListener* listener_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!draining_) {
            pool = pool_;
        }
        listener_ptr = listener_.get();
    }
    if (listener_ptr != nullptr) {
        listener_ptr->request_stop([pool] {
            if (pool != nullptr) {
                pool->request_stop();
            }
        });
    } else if (pool != nullptr) {
        pool->request_stop();
    }
}

void LocalDevHostImpl::begin_drain(std::uint64_t deadline_ms) {
    ManagedListener* listener_ptr = nullptr;
    std::shared_ptr<GenerationPool> pool;
    bool no_live_fixed_workers = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!started_ || draining_) {
            return;
        }
        draining_ = true;
        no_live_fixed_workers =
            !options_.replace_exited_workers && pool_ != nullptr && pool_->ready_workers() == 0;
        drain_had_live_workers_ = !no_live_fixed_workers;
        drain_began_at_ = std::chrono::steady_clock::now();
        drain_deadline_ms_ = deadline_ms;
        listener_ptr = listener_.get();
        pool = pool_;
    }
    if (listener_ptr != nullptr) {
        if (no_live_fixed_workers) {
            listener_ptr->request_stop();
        } else {
            listener_ptr->stop_accepting();
        }
    }
    if (pool != nullptr) {
        pool->request_drain(std::chrono::milliseconds(deadline_ms));
    }
}

bool LocalDevHostImpl::wait(std::string* error) {
    std::call_once(wait_once_, [this] {
        bool was_draining = false;
        ManagedListener* listener_ptr = nullptr;
        std::shared_ptr<GenerationPool> pool;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            was_draining = draining_;
            listener_ptr = listener_.get();
            pool = pool_;
        }
        if (was_draining) {
            if (pool != nullptr) {
                std::string pool_err;
                wait_result_ = pool->wait(&pool_err) && wait_result_;
            }
            if (listener_ptr != nullptr) {
                listener_ptr->request_stop();
                std::string listener_err;
                wait_result_ = listener_ptr->wait(&listener_err) && wait_result_;
            }
        } else {
            stop_requested_.store(true, std::memory_order_release);
            if (listener_ptr != nullptr) {
                listener_ptr->request_stop([pool] {
                    if (pool != nullptr) {
                        pool->request_stop();
                    }
                });
                std::string listener_err;
                wait_result_ = listener_ptr->wait(&listener_err) && wait_result_;
            } else if (pool != nullptr) {
                pool->request_stop();
            }
            if (pool != nullptr) {
                std::string pool_err;
                wait_result_ = pool->wait(&pool_err) && wait_result_;
            }
        }
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (pool_ != nullptr) {
            cached_exit_statuses_ = pool_->worker_exit_statuses();
            cached_forced_cancellations_ = pool_->forced_drain_cancellations();
        }
        listener_.reset();
        pool_.reset();
        bound_port_.store(0, std::memory_order_release);
    });
    if (error != nullptr) {
        *error = wait_result_ ? "stopped" : "local host wait failed";
    }
    return wait_result_;
}

int LocalDevHostImpl::run(const std::vector<std::uint8_t>* inline_bundle) {
    std::string error;
#if !defined(_WIN32)
    asio::io_context signal_io;
    asio::signal_set signals(signal_io, SIGTERM, SIGINT);
    signals.async_wait([this](const boost::system::error_code& ec, int) {
        if (!ec) {
            request_stop();
        }
    });
#else
    HANDLE term_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_local_dev_term_event.store(term_event, std::memory_order_release);
    if (term_event == nullptr || SetConsoleCtrlHandler(&local_dev_console_handler, TRUE) == 0) {
        emit_log(options_.log, LogLane::kControl,
                 {.event = log_events::kStartup,
                  .result = "fail",
                  .message = "cannot install stop handler"});
        if (term_event != nullptr) {
            g_local_dev_term_event.store(nullptr, std::memory_order_release);
            CloseHandle(term_event);
        }
        return 2;
    }
#endif

    if (!start(inline_bundle, &error)) {
#if !defined(_WIN32)
        signals.cancel();
#else
        SetConsoleCtrlHandler(&local_dev_console_handler, FALSE);
        g_local_dev_term_event.store(nullptr, std::memory_order_release);
        CloseHandle(term_event);
#endif
        emit_log(options_.log, LogLane::kControl,
                 {.event = log_events::kStartup, .result = "fail", .message = error});
        return start_exit_code_;
    }

#if defined(_WIN32)
    bool wait_failed = false;
    while (!stop_requested_.load(std::memory_order_acquire)) {
        const DWORD res = WaitForSingleObject(term_event, 50);
        if (res == WAIT_OBJECT_0) {
            break;
        }
        if (res != WAIT_TIMEOUT) {
            wait_failed = true;
            break;
        }
    }
    SetConsoleCtrlHandler(&local_dev_console_handler, FALSE);
    g_local_dev_term_event.store(nullptr, std::memory_order_release);
    CloseHandle(term_event);
#else
    while (!stop_requested_.load(std::memory_order_acquire)) {
        signal_io.run_for(std::chrono::milliseconds(50));
    }
    signals.cancel();
#endif

    request_stop();
    if (!wait(&error)) {
        emit_log(options_.log, LogLane::kControl,
                 {.event = log_events::kShutdown, .result = "fail", .message = error});
        return 1;
    }
#if defined(_WIN32)
    if (wait_failed) {
        emit_log(options_.log, LogLane::kControl,
                 {.event = log_events::kShutdown,
                  .result = "fail",
                  .message = "cannot wait for stop event"});
        return 1;
    }
#endif
    return 0;
}

LocalDevHost::LocalDevHost(LocalDevHostOptions options)
    : impl_(std::make_shared<LocalDevHostImpl>(std::move(options))) {}

LocalDevHost::~LocalDevHost() {
    if (impl_) {
        impl_->request_stop();
        std::string ignored;
        impl_->wait(&ignored);
    }
}

bool LocalDevHost::start(std::string* error) { return impl_->start(nullptr, error); }

bool LocalDevHost::start(const std::vector<std::uint8_t>& bundle, std::string* error) {
    return impl_->start(&bundle, error);
}

void LocalDevHost::request_stop() { impl_->request_stop(); }

void LocalDevHost::begin_drain(std::uint64_t deadline_ms) { impl_->begin_drain(deadline_ms); }

bool LocalDevHost::wait(std::string* error) { return impl_->wait(error); }

int LocalDevHost::run() { return impl_->run(nullptr); }

int LocalDevHost::run(const std::vector<std::uint8_t>& bundle) { return impl_->run(&bundle); }

std::uint16_t LocalDevHost::bound_port() const { return impl_->bound_port(); }

std::size_t LocalDevHost::active_workers() const { return impl_->active_workers(); }

std::vector<capsid::WorkerExitStatus> LocalDevHost::worker_exit_statuses() const {
    return impl_->worker_exit_statuses();
}

LocalDevHost::DrainMetrics LocalDevHost::drain_metrics() const { return impl_->drain_metrics(); }

} // namespace capsid::host
