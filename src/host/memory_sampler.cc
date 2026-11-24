#include "host/memory_sampler.h"

#include "host/process_snapshot.h"

#if defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <charconv>
#include <chrono>
#include <string>
#include <utility>

namespace capsid::host {

namespace {

std::uint64_t steady_now_ms() noexcept {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

bool default_read_cgroup_current(std::string_view cgroup_path, std::uint64_t* bytes_out) {
    if (cgroup_path.empty() || bytes_out == nullptr) {
        return false;
    }
#if defined(__linux__)
    std::string path(cgroup_path);
    if (path.back() != '/') {
        path.push_back('/');
    }
    path += "memory.current";
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    char buffer[64] = {};
    ssize_t total = 0;
    while (total < static_cast<ssize_t>(sizeof(buffer) - 1)) {
        const ssize_t n =
            ::read(fd, buffer + total, sizeof(buffer) - 1 - static_cast<size_t>(total));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            ::close(fd);
            return false;
        }
        if (n == 0) {
            break;
        }
        total += n;
    }
    ::close(fd);
    std::string_view text(buffer, static_cast<std::size_t>(total));
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
        text.remove_suffix(1);
    }
    if (text.empty()) {
        return false;
    }
    std::uint64_t value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        return false;
    }
    *bytes_out = value;
    return true;
#else
    (void)cgroup_path;
    (void)bytes_out;
    return false;
#endif
}

} // namespace

MemorySampler::MemorySampler()
    : cgroup_reader_(default_read_cgroup_current),
      footprint_reader_([](std::int64_t pid, std::uint64_t* bytes_out) {
          return read_process_footprint_bytes(pid, bytes_out);
      }) {}

MemorySampler::MemorySampler(CgroupReader cgroup_reader, FootprintReader footprint_reader)
    : cgroup_reader_(cgroup_reader ? std::move(cgroup_reader) : default_read_cgroup_current),
      footprint_reader_(footprint_reader ? std::move(footprint_reader)
                                         : [](std::int64_t pid, std::uint64_t* bytes_out) {
                                               return read_process_footprint_bytes(pid, bytes_out);
                                           }) {}

std::uint64_t MemorySampler::sample_worker_bytes(std::int64_t pid, std::string_view cgroup_path,
                                                 bool force_refresh) {
    std::uint64_t bytes = 0;
    if (!cgroup_path.empty() && cgroup_reader_ && cgroup_reader_(cgroup_path, &bytes)) {
        return bytes;
    }
    if (pid <= 0) {
        return 0;
    }
    const std::uint64_t max_age = max_age_ms_.load(std::memory_order_acquire);
    const std::uint64_t now_ms = steady_now_ms();
    if (!force_refresh && max_age > 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = cached_pid_bytes_.find(pid);
        if (found != cached_pid_bytes_.end() && now_ms >= found->second.sampled_at_ms &&
            (now_ms - found->second.sampled_at_ms) < max_age) {
            return found->second.bytes;
        }
    }
    if (footprint_reader_ && footprint_reader_(pid, &bytes)) {
        std::lock_guard<std::mutex> lock(mutex_);
        cached_pid_bytes_[pid] = CachedSample{bytes, steady_now_ms()};
        return bytes;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    cached_pid_bytes_.erase(pid);
    return 0;
}

void MemorySampler::refresh_worker(std::int64_t pid, std::string_view cgroup_path) {
    (void)sample_worker_bytes(pid, cgroup_path, /*force_refresh=*/true);
}

void MemorySampler::erase_pid(std::int64_t pid) {
    if (pid <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    cached_pid_bytes_.erase(pid);
}

void MemorySampler::prune_pids(const std::unordered_set<std::int64_t>& live_pids) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = cached_pid_bytes_.begin(); it != cached_pid_bytes_.end();) {
        if (live_pids.find(it->first) == live_pids.end()) {
            it = cached_pid_bytes_.erase(it);
        } else {
            ++it;
        }
    }
}

std::size_t MemorySampler::cached_pid_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cached_pid_bytes_.size();
}

} // namespace capsid::host
