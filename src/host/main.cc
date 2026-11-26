// capsid-host executable entry point.
//
// Two execution shapes:
//   - Local dev mode (default, cross-platform): mounts an isomorphic app dir
//     (--app-dir <dir>) onto LocalDevHost (GenerationPool + ManagedListener).
//   - Managed production mode (--mode managed, Linux only): reads host.json,
//     recovers durable state, and runs the Unix-socket Admin coordinator.
// stdout never carries readiness or logs; diagnostics go to stderr.

#include <iostream>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

#include "build_identity.h"
#include "capsid/runtime.h"
#if !defined(_WIN32)
#include "host/admin_service.h"
#include "host/eviction_coordinator.h"
#include "host/host_config_model.h"
#include "host/managed_admin_backend.h"
#include "host/managed_listener.h"
#include "host/managed_registry.h"
#include "host/memory_sampler.h"
#include "host/quarantine_dispatcher.h"
#include "host/routing_snapshot.h"
#endif
#include "host/config.h"
#include "host/generation_pool.h"
#include "host/generation_reclaimer.h"
#include "host/local_dev_host.h"
#include "host/metrics.h"
#include "host/parent_stop_monitor.h"
#include "host/process_snapshot.h"
#include "host/structured_log.h"
#include "host/trusted_key_store.h"
#include "host/units.h"
#include "host/worker_supervisor.h"
#include "json_escape.h"
#include "net_target.h"

#include <jansson.h>

#include <boost/asio/ip/address.hpp>
#include <boost/system/error_code.hpp>

#if defined(_WIN32)
#include "win32_compat.h"
#else
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "host/active_state.h"
#include "host/request_normalization.h"

namespace {

constexpr std::string_view kProbeGeneration =
    "sha256:0000000000000000000000000000000000000000000000000000000000000000";

void fail(const std::string& message) {
    // M2 item 7 (§12.2): a startup failure is a structured line on stderr
    // — one JSON object with the fixed field set, message JSON-escaped.
    // There is no StructuredLog instance yet at this point (a failure may
    // precede its construction), so the line is written directly.
    const std::string escaped = capsid::json_escape(message);
    const std::uint64_t timestamp_ms =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count());
    std::fprintf(stderr,
                 "{\"timestamp\":\"%llu\",\"level\":\"error\","
                 "\"event\":\"startup\",\"result\":\"fail\","
                 "\"message\":\"%s\"}\n",
                 static_cast<unsigned long long>(timestamp_ms), escaped.c_str());
    std::exit(2);
}

#if defined(_WIN32)
// `_get_osfhandle` treats a descriptor whose CRT slot is not FOPEN as an
// invalid parameter; with the default handler that terminates the process
// (Node's `stdio: 'ignore'` for an extra fd leaves exactly such a slot).
// Install a no-op invalid-parameter handler just for this lookup so an
// unusable --ready-fd reports "not an open descriptor" instead of crashing.
void windows_invalid_parameter_noop(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int,
                                    std::uintptr_t) {}

bool ready_fd_is_open(int fd) {
    const _invalid_parameter_handler previous =
        _set_invalid_parameter_handler(windows_invalid_parameter_noop);
    const bool open = _get_osfhandle(fd) != -1;
    _set_invalid_parameter_handler(previous);
    return open;
}
#endif

bool valid_application_id(const std::string& application) {
    capsid::host::ActiveStateDocument probe;
    probe.state = capsid::host::ActiveServiceState::kActive;
    probe.application = application;
    probe.version = "v0";
    probe.generation = std::string(kProbeGeneration);
    return capsid::host::encode_active_state_json(probe).ok;
}

std::uint64_t parse_positive_integer(const std::string& value, const char* name) {
    if (value.empty()) {
        fail(std::string("--") + name + " requires a positive integer");
    }
    for (const char c : value) {
        if (c < '0' || c > '9') {
            fail(std::string("--") + name + " requires a positive integer: " + value);
        }
    }
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(value.c_str(), &end, 10);
    if (errno != 0 || end == nullptr || *end != '\0' || parsed == 0 ||
        parsed > static_cast<unsigned long long>(std::numeric_limits<std::int64_t>::max())) {
        fail(std::string("--") + name + " requires a positive integer: " + value);
    }
    return static_cast<std::uint64_t>(parsed);
}

// host:port or [ipv6]:port; decimal ports in [0, 65535].
void parse_listen(const std::string& value, std::string* out_address, std::uint16_t* out_port) {
    if (!capsid::parse_tcp_endpoint(value, out_address, out_port, /*allow_zero_port=*/true)) {
        fail("--listen requires host:port with decimal port in [0, 65535]: " + value);
    }
}

// ---- managed mode: host.json authority, real coordinator, Admin service ----
#if defined(_WIN32)
// The managed coordinator is POSIX-only (dirfd-relative openat/mkdirat
// state walks, uid-based verification, UDS admin plane). Windows builds
// ship the local dev Host (LocalDevHost) only; see
// docs/guides/build-on-windows.md for the full capability matrix.
#else

// Process-level stop signal. SIGTERM is blocked process-wide and waited
// for with sigwait on the main thread, so no C++ object is ever touched
// inside a signal handler.
std::atomic<bool> g_stop{false};

// Safe open of a Host-owned directory: O_NOFOLLOW, directory, euid owner,
// no group/other bits.
int open_verified_root(const std::string& path, const char* what) {
    const int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        fail(std::string("cannot open ") + what);
    }
    struct stat st = {};
    if (fstat(fd, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != geteuid() ||
        (st.st_mode & 0077) != 0) {
        close(fd);
        fail(std::string("unverified ") + what);
    }
    return fd;
}

// Valid App ID (frozen lowercase grammar).
bool valid_managed_app_id(const std::string& value) {
    if (value.empty() || value.size() > 63) {
        return false;
    }
    if (!((value[0] >= 'a' && value[0] <= 'z') || (value[0] >= '0' && value[0] <= '9'))) {
        return false;
    }
    for (const char c : value) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' ||
              c == '-')) {
            return false;
        }
    }
    return true;
}

// Discovers configured Apps beneath the verified applications root.
std::vector<std::string> discover_applications(int apps_fd) {
    std::vector<std::string> applications;
    DIR* dir = fdopendir(dup(apps_fd));
    if (dir == nullptr) {
        fail("cannot enumerate applications root");
    }
    for (;;) {
        errno = 0;
        struct dirent* entry = readdir(dir);
        if (entry == nullptr) {
            if (errno != 0 && errno != EINTR) {
                fail("cannot enumerate applications root");
            }
            break;
        }
        const std::string name(entry->d_name);
        if (!valid_managed_app_id(name)) {
            continue;
        }
        struct stat st = {};
        if (fstatat(apps_fd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISDIR(st.st_mode)) {
            applications.push_back(name);
        }
    }
    closedir(dir);
    return applications;
}

// Cross-platform stat timestamp accessors: Apple spells the fields
// st_mtimespec/st_ctimespec; other POSIX systems use st_mtim/st_ctim
// (same pattern as the coordinator's platform macros).
#if defined(__APPLE__)
#define CAPSID_MAIN_MTIME_SEC(st) ((st).st_mtimespec.tv_sec)
#define CAPSID_MAIN_MTIME_NSEC(st) ((st).st_mtimespec.tv_nsec)
#define CAPSID_MAIN_CTIME_SEC(st) ((st).st_ctimespec.tv_sec)
#define CAPSID_MAIN_CTIME_NSEC(st) ((st).st_ctimespec.tv_nsec)
#else
#define CAPSID_MAIN_MTIME_SEC(st) ((st).st_mtim.tv_sec)
#define CAPSID_MAIN_MTIME_NSEC(st) ((st).st_mtim.tv_nsec)
#define CAPSID_MAIN_CTIME_SEC(st) ((st).st_ctim.tv_sec)
#define CAPSID_MAIN_CTIME_NSEC(st) ((st).st_ctim.tv_nsec)
#endif

// Bounded read of the host.json document: O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,
// only a Host-owned regular file, a size cap checked before reading and a
// device/inode/size re-check after reading (a FIFO or symlink never blocks
// or redirects startup, and a concurrent swap is rejected).
std::string read_host_config(const std::string& path) {
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) {
        fail("cannot open --host-config: " + path);
    }
    struct stat before = {};
    if (fstat(fd, &before) != 0 || !S_ISREG(before.st_mode) || before.st_uid != geteuid() ||
        before.st_size < 0 || static_cast<std::uint64_t>(before.st_size) > 1024U * 1024U) {
        close(fd);
        fail("--host-config is not a Host-owned regular file under 1 MiB");
    }
    std::string json;
    char buffer[4096];
    for (;;) {
        const ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count < 0) {
            close(fd);
            fail("cannot read --host-config");
        }
        if (count == 0) {
            break;
        }
        if (json.size() + static_cast<std::size_t>(count) > 1024U * 1024U) {
            close(fd);
            fail("--host-config exceeds 1 MiB");
        }
        json.append(buffer, static_cast<std::size_t>(count));
    }
    struct stat after = {};
    if (fstat(fd, &after) != 0 || after.st_dev != before.st_dev || after.st_ino != before.st_ino ||
        after.st_size != before.st_size ||
        CAPSID_MAIN_MTIME_SEC(after) != CAPSID_MAIN_MTIME_SEC(before) ||
        CAPSID_MAIN_MTIME_NSEC(after) != CAPSID_MAIN_MTIME_NSEC(before) ||
        CAPSID_MAIN_CTIME_SEC(after) != CAPSID_MAIN_CTIME_SEC(before) ||
        CAPSID_MAIN_CTIME_NSEC(after) != CAPSID_MAIN_CTIME_NSEC(before)) {
        close(fd);
        fail("--host-config changed while it was read");
    }
    close(fd);
    return json;
}

[[maybe_unused]] int run_managed(const std::string& host_config_path,
                                 const std::string& worker_path) {
    // SIGTERM is blocked FIRST, before any thread exists: every thread
    // inherits the mask, and the main thread waits with sigwait below.
    sigset_t term_set;
    sigemptyset(&term_set);
    sigaddset(&term_set, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &term_set, nullptr) != 0) {
        fail("cannot block SIGTERM");
    }
    const std::string host_json = read_host_config(host_config_path);
    // The authoritative schema boundary runs first; no second lax parser.
    const capsid::host::ConfigValidationResult validated =
        capsid::host::validate_config_json(capsid::host::ConfigDocument::kHost, host_json);
    if (!validated.ok) {
        fail("host.json rejected at " + validated.error.path + ": " + validated.error.message);
    }
    capsid::host::ParsedHostConfig config;
    std::string config_error;
    if (!parse_host_config(host_json, &config, &config_error)) {
        fail(config_error);
    }
    // Load the trusted bytecode keys BEFORE any worker spawns: attestation
    // verification is fail-closed, and an empty key set means every
    // trusted-bytecode deployment is rejected (never silently accepted).
    // The store owns the key bytes; the options below hold views into it,
    // so the store must outlive every ManagedHostOptions.
    capsid::host::TrustedKeyStore trusted_keys =
        capsid::host::TrustedKeyStore::load(config.trusted_keys, &config_error);
    if (trusted_keys.size() != config.trusted_keys.size()) {
        fail(config_error);
    }
    const std::vector<capsid::host::TrustedBytecodeKey> trusted_key_views(
        trusted_keys.keys().begin(), trusted_keys.keys().end());
    // Safe-open the roots; the secret template's parent is the dirfd the
    // coordinator opens App subdirectories from.
    const int apps_fd = open_verified_root(config.applications_root, "applications root");
    // The template must be EXACTLY <parent>/{application}: a trailing
    // suffix would be silently misinterpreted as part of the parent root.
    const std::string secret_template = config.secret_root_template;
    const std::string::size_type placeholder = secret_template.find("/{application}");
    const std::string secret_parent = secret_template.substr(0, placeholder);
    if (placeholder == std::string::npos || secret_parent.empty() ||
        secret_template.size() != secret_parent.size() + std::string("/{application}").size()) {
        fail("secretRootTemplate must be exactly "
             "<root>/{application}");
    }
    const int secrets_fd = open_verified_root(secret_parent, "secret root template");
    // Fail-early validation only: the coordinator re-opens the state root
    // per operation (verified the same way); this open proves the root is
    // present and well-formed before any worker spawns.
    const int state_fd = open_verified_root(config.state_root, "state root");
    const std::vector<std::string> applications = discover_applications(apps_fd);
    if (applications.empty()) {
        fail("applications root contains no configured Apps");
    }
    // M2 item 7 (design §12): the process-wide structured log and metrics
    // registry. Created after the config is validated so their lifetime
    // covers every option object below; injected into the coordinator,
    // supervisors and the Admin API.
    auto structured_log =
        std::make_unique<capsid::host::StructuredLog>([](const std::string& line) {
            // One JSON object per line on stderr (design §12.2); a torn
            // partial write is impossible per call (single write below the
            // PIPE_BUF ceiling on regular stderr).
            const ssize_t written = ::write(STDERR_FILENO, line.data(), line.size());
            (void)written;
        });
    auto metrics = std::make_unique<capsid::host::MetricsRegistry>();
    auto channel_budget =
        std::make_shared<capsid::host::ServiceChannelBudget>(config.capacity.channels);
    metrics->observe_channel_budget(channel_budget);
    const capsid::host::ResolvedRecoveryPolicy recovery_resolved =
        capsid::host::resolve_recovery_policy(config.recovery);
    if (!recovery_resolved.ok) {
        fail("invalid host.json recovery policy: " + recovery_resolved.error);
    }
    // Stable ownership for every App's options (the coordinator stores
    // pointers to them).
    std::vector<std::unique_ptr<capsid::host::ManagedHostOptions>> owned;
    std::vector<capsid::host::ManagedHostOptions*> app_options;
    for (const std::string& application : applications) {
        auto options = std::make_unique<capsid::host::ManagedHostOptions>();
        options->channels = config.channels;
        options->applications_root_fd = apps_fd;
        options->secret_root_template_fd = secrets_fd;
        options->state_root = config.state_root;
        options->bindings_root = config.bindings_root;
        if (!config.bindings_root.empty()) {
            std::string scan_error;
            if (!capsid::host::scan_bindings_root(config.bindings_root, {0, geteuid()},
                                                  &options->binding_registry, &scan_error)) {
                std::cerr << "capsid-host: bindingsRoot rejected: " << scan_error << std::endl;
                return EXIT_FAILURE;
            }
            options->binding_registry_loaded = true;
        }
        options->application = application;
        options->worker_path = worker_path;
        options->host_policy = config.policy;
        options->recovery_policy = recovery_resolved.policy;
        options->trusted_keys = trusted_key_views;
        options->runtime_compatibility_id = CAPSID_BUILD_COMPATIBILITY_ID;
        options->stop_requested = &g_stop;
        options->log = structured_log.get();
        options->metrics = metrics.get();
        app_options.push_back(options.get());
        owned.push_back(std::move(options));
    }
    // ---- PR-09c §9.3: the Managed data plane ----
    // The routing snapshot maps each App to its ACTIVE GenerationPool; the
    // listener routes every request through one atomic snapshot load and
    // pins the pool it found. Pools are ADOPTED from the warmed fleet — the
    // §8.3 replacement factory and the generation identity travel with the
    // deploy/recover outcome — so replacements respawn the same artifact
    // and effective config, and the route identity survives replacement.
    auto routing = std::make_shared<capsid::host::RoutingTable>();
    metrics->set_process_snapshot_provider(capsid::host::process_snapshot_provider(
        [routing] { return static_cast<pid_t>(routing->worker_process_id()); }));
    // App -> ACTIVE pool (the only pool a new request may route to).
    std::map<std::string, std::shared_ptr<capsid::host::GenerationPool>> active_pools;
    // Retired/replaced pools stay pinned until their completion callback hands
    // them to the reclaimer. Joining never runs on their own pump thread.
    std::vector<std::shared_ptr<capsid::host::GenerationPool>> draining_pools;
    // §9.6-6 retired Apps: the name keeps a route-level tombstone (no
    // pool) so the router answers 404 after retire — the durable retire
    // tombstone of the coordinator is mirrored here, and a redeploy
    // revives the route. Never-deployed Apps stay unrouted (503).
    std::set<std::string> retired_apps;
    std::mutex pools_mutex;
    // The listener is created AFTER startup recovery (see below); pools
    // activated during recovery are wired by its start(), which wires every
    // pool in the current snapshot. Pools activated later are wired by the
    // §9.3 prepare callback (adopt_generation) before any commit publishes
    // a snapshot.
    std::unique_ptr<capsid::host::ManagedListener> data_plane;
    // Builds a fresh snapshot over the current view (live pools +
    // tombstones). Caller holds pools_mutex. Allocation allowed —
    // prepare-time only, never inside a commit.
    const auto build_snapshot = [&]() {
        std::vector<std::pair<std::string, std::shared_ptr<capsid::host::GenerationPool>>> routes;
        routes.reserve(active_pools.size() + retired_apps.size());
        for (const auto& entry : active_pools) {
            routes.emplace_back(entry.first, entry.second);
        }
        for (const std::string& application : retired_apps) {
            // Tombstone: routed name, no pool — the router's 404.
            routes.emplace_back(application, nullptr);
        }
        return capsid::host::RoutingSnapshot::build(std::move(routes));
    };
    // Publishes a fresh snapshot over the current view (the routing table
    // itself is lock-free; the lock makes the active_pools view atomic for
    // its writers). Allocation-free — also used by the commit callbacks.
    const auto publish_snapshot = [&]() { routing->publish(build_snapshot()); };
    // The process-global weighted capacity ledger (§9.4): workersTotal is
    // the steady-state budget, activationSurgeWorkers the overlapping
    // replacement budget. Every reserve happens BEFORE any spawn, inside
    // the coordinator; the ledger is wired into every App's options below
    // and into every adopted pool's drain-complete hook.
    capsid::host::WorkerCapacityLedger ledger(config.capacity.workers_total,
                                              config.capacity.activation_surge_workers);
    // Ownership handoff for a warmed generation: adopt the fleet into a
    // GenerationPool (create_adopted destroys everything on failure — no
    // worker escapes) and install the event sink. A generation without its
    // replacement factory is rejected (factory null) — §8.3 replacement is
    // not optional. The pool is not yet routed, so it has no in-flight
    // requests when the sink is installed.
    // M2 item 5b: the process-global fair startup-permit queue (design
    // §10.5.6). Both startup paths (deploy via the Admin backend,
    // replacement via the generation pools) share this single instance;
    // the queue decides order and fairness, capacity still decides
    // concurrency. The bound matches the Admin pending-queue bound.
    capsid::host::StartupPermitCoordinator startup_permits(&g_stop, 8);
    capsid::host::GenerationReclaimer generation_reclaimer;
    capsid::host::QuarantineDispatcher quarantine_dispatcher;
    capsid::host::MemorySampler memory_sampler;
    capsid::host::GlobalEvictionCoordinator eviction_coordinator(
        config.capacity.memory, config.tenancy, &ledger, &memory_sampler, metrics.get(),
        structured_log.get());
    const auto adopt_generation =
        [&](const std::string& application, const std::string& tenant,
            const std::vector<capsid_worker*>& workers,
            const capsid::host::WorkerExecutor::WorkerFactory& factory, const std::string& version,
            const std::string& generation_digest, const capsid::host::EffectiveConfig& effective,
            std::uint32_t pre_acquired_elastic =
                0) -> std::shared_ptr<capsid::host::GenerationPool> {
        if (!factory) {
            for (capsid_worker* worker : workers) {
                capsid_worker_destroy(worker);
                capsid::host::release_worker_cgroup(worker);
            }
            return nullptr;
        }
        capsid::host::GenerationPoolOptions pool_options;
        pool_options.application_id = application;
        pool_options.tenant_id = tenant.empty() ? application : tenant;
        pool_options.version = version;
        pool_options.generation_digest = generation_digest;
        pool_options.workers = effective.workers;
        pool_options.has_min_ready = true;
        pool_options.min_ready = effective.min_ready;
        pool_options.idle_timeout_ms = effective.idle_timeout_ms;
        pool_options.acquire_worker_slot =
            [&eviction_coordinator](const capsid::host::GenerationPool* requester,
                                    std::string* rejection_reason) {
                return eviction_coordinator.acquire_worker_slot(requester, rejection_reason);
            };
        pool_options.release_elastic_slot = [&ledger, application] {
            ledger.release_elastic(application, 1);
        };
        pool_options.handoff_elastic_to_reaping = [&ledger, application] {
            return ledger.handoff_elastic_to_reaping(application);
        };
        pool_options.try_acquire_reaping_slot = [&ledger, application] {
            return ledger.try_acquire_reaping(application);
        };
        pool_options.release_reaping_slot = [&ledger, application] {
            ledger.finish_reaping(application, 1);
        };
        pool_options.max_inflight_per_worker = effective.requests_per_worker;
        pool_options.max_queued_requests = effective.queue_requests;
        pool_options.queue_timeout_ms = effective.queue_timeout_ms;
        pool_options.max_request_body_bytes =
            capsid::host::effective_max_request_body_bytes(effective.memory_bytes);
        pool_options.factory = factory;
        pool_options.channel_budget = channel_budget;
        pool_options.recovery = recovery_resolved.policy;
        // Direction A: the pool is the only recovery engine, so it
        // owns the shared fair startup-permit queue for its
        // replacement spawns. Process metrics read the published
        // routing snapshot and never borrow a worker handle.
        pool_options.startup_permits = &startup_permits;
        pool_options.log = structured_log.get();
        pool_options.metrics = metrics.get();
        pool_options.on_worker_exited =
            [&memory_sampler](const capsid::host::WorkerExecutor* executor) {
                if (executor != nullptr) {
                    memory_sampler.erase_pid(executor->process_id());
                }
            };
        const std::uint32_t pool_size = effective.min_ready;
        const auto capacity = std::make_shared<capsid::host::WorkerPoolCapacityLease>(
            &ledger, application, pool_size);
        pool_options.capacity_lease = capacity;
        std::vector<capsid_worker*> adopted_workers;
        adopted_workers.reserve(workers.size());
        std::size_t consumed_pre_acquired = 0;
        std::size_t extra_elastic_acquired = 0;
        for (std::size_t index = 0; index < workers.size(); ++index) {
            if (index < effective.min_ready) {
                adopted_workers.push_back(workers[index]);
            } else if (consumed_pre_acquired < pre_acquired_elastic) {
                adopted_workers.push_back(workers[index]);
                ++consumed_pre_acquired;
            } else if (ledger.try_acquire_elastic(application)) {
                adopted_workers.push_back(workers[index]);
                ++extra_elastic_acquired;
            } else {
                capsid_worker_destroy(workers[index]);
                capsid::host::release_worker_cgroup(workers[index]);
            }
        }
        // Links this drain hook to the pool created from the same
        // prepare call (filled right after create_adopted). WEAK by
        // design: create_adopted requires no self-held strong
        // reference (see the pump-thread note there) — an aborted
        // activation must be able to destroy its never-published
        // pool through plain refcount, and a pool already under
        // destruction must not re-arm itself.
        const auto pool_box = std::make_shared<std::weak_ptr<capsid::host::GenerationPool>>();
        capsid::host::ManagedHostOptions* target = nullptr;
        for (auto* options : app_options) {
            if (options->application == application) {
                target = options;
                break;
            }
        }
        const std::weak_ptr<capsid::host::RoutingTable> observed_routes = routing;
        const auto quarantine = std::make_shared<capsid::host::QuarantineRequest>(
            target, generation_digest,
            [observed_routes, pool_box, application, &pools_mutex, capacity] {
                bool current_instance = false;
                {
                    std::lock_guard<std::mutex> lock(pools_mutex);
                    const auto table = observed_routes.lock();
                    const auto snapshot = table ? table->load() : nullptr;
                    const auto current = snapshot ? snapshot->find(application) : nullptr;
                    // Ownership comparison does not pin an obsolete
                    // pool whose destructor could join a waiting pump.
                    current_instance = current && !pool_box->owner_before(current) &&
                                       !current.owner_before(*pool_box);
                }
                // Still inside the App operation lock, before the
                // barrier releases quarantine shutdowns. Never hold
                // pools_mutex while settling capacity.
                if (current_instance)
                    capacity->begin_drain();
                return current_instance;
            });
        pool_options.quarantine_completion =
            std::shared_ptr<const std::atomic<bool>>(quarantine, &quarantine->completed);
        pool_options.on_quarantine = [&quarantine_dispatcher, quarantine] {
            (void)quarantine_dispatcher.enqueue(quarantine);
        };
        // §9.4: the reaper-finished instant releases the pool's
        // capacity count and (P1, review §12.2) hands the Host's
        // parked reference from draining_pools to the reclaimer.
        // Locking FIRST: if the weak box is gone the pool is under
        // destruction on the abort path, where abort_reserve already
        // settled the ledger — no release, no parking. The hook
        // touches the ledger and pools_mutex only, never the pool
        // itself (its destructor joins this very thread).
        pool_options.on_drain_complete = [&draining_pools, &generation_reclaimer, &pools_mutex,
                                          &eviction_coordinator, pool_box] {
            std::shared_ptr<capsid::host::GenerationPool> pool;
            {
                std::lock_guard<std::mutex> lock(pools_mutex);
                pool = pool_box->lock();
            }
            if (pool == nullptr) {
                return;
            }
            eviction_coordinator.unregister_pool(pool.get());
            std::lock_guard<std::mutex> lock(pools_mutex);
            for (auto it = draining_pools.begin(); it != draining_pools.end();) {
                if (*it == pool) {
                    it = draining_pools.erase(it);
                } else {
                    ++it;
                }
            }
            generation_reclaimer.enqueue(pool);
        };
        std::string pool_error;
        std::shared_ptr<capsid::host::GenerationPool> pool =
            capsid::host::GenerationPool::create_adopted(std::move(pool_options),
                                                         std::move(adopted_workers), &pool_error);
        {
            std::lock_guard<std::mutex> lock(pools_mutex);
            *pool_box = pool;
        }
        if (pool == nullptr) {
            if (extra_elastic_acquired > 0) {
                ledger.release_elastic(application, extra_elastic_acquired);
            }
            return nullptr;
        }
        if (consumed_pre_acquired < pre_acquired_elastic) {
            ledger.release_elastic(application, pre_acquired_elastic - consumed_pre_acquired);
        }
        // The sink must be installed before the pool starts serving:
        // a request pinned before wiring would lose its response
        // events (wire_pool precondition: no in-flight requests).
        if (data_plane != nullptr) {
            data_plane->wire_pool(pool);
        }
        return pool;
    };
    // ---- §9.3 activation transaction callbacks ----
    // The coordinator runs these around the durable active.json write
    // (persist). prepare builds the ENTIRE new world (pool + view +
    // snapshot); commit is a single atomic publication (no allocation, no
    // I/O, no lock waiting); abort rolls the view back. The Async Admin
    // worker is single-threaded, so a whole transaction is never observed
    // mid-flight by another transaction.
    const auto prepare_activation =
        [&](const std::string& application, const capsid::host::DeployOutcome& prepared,
            std::string* error) -> std::unique_ptr<capsid::host::ActivationPlan> {
        std::unique_ptr<capsid::host::ActivationPlan> plan(new capsid::host::ActivationPlan());
        plan->application = application;
        plan->version = prepared.version;
        plan->generation_digest = prepared.generation_digest;
        plan->new_workers = prepared.effective.min_ready;
        std::shared_ptr<capsid::host::GenerationPool> pool = adopt_generation(
            application, prepared.tenant, prepared.workers, prepared.generation_factory,
            prepared.version, prepared.generation_digest, prepared.effective,
            prepared.pre_acquired_elastic);
        if (pool == nullptr) {
            *error = "cannot activate the new generation";
            return nullptr;
        }
        plan->new_pool = pool;
        {
            std::lock_guard<std::mutex> lock(pools_mutex);
            const auto existing = active_pools.find(application);
            if (existing != active_pools.end()) {
                plan->old_pool = existing->second;
                plan->old_workers = existing->second->min_ready();
                // The replaced generation stops serving only at
                // COMMIT; the reference is parked here so commit
                // needs no allocation.
                draining_pools.push_back(existing->second);
            }
            // View pre-insert: commit's find+assign never allocates,
            // and the snapshot below already reflects the new
            // generation. A redeploy revives a retired App: the live
            // route wins over the tombstone, so the publish must not
            // emit both.
            active_pools[application] = pool;
            retired_apps.erase(application);
            // The COMPLETE new route map is built at prepare time so
            // commit is a single atomic snapshot swap.
            plan->new_snapshot = build_snapshot();
        }
        return plan;
    };
    const auto commit_activation = [&](capsid::host::ActivationPlan* plan) {
        std::lock_guard<std::mutex> lock(pools_mutex);
        // The new pool is already in the view (prepared): commit only
        // publishes and signals the drain. No allocation, no file I/O.
        if (plan->old_pool != nullptr) {
            // Drain signal only: new requests route to the new pool,
            // in-flight requests finish on the old one, and a drained
            // generation never starts a replacement. Keep registered in
            // eviction_coordinator until on_drain_complete so live draining
            // workers continue to count toward host and tenant memory budgets.
            plan->old_pool->request_drain();
        }
        eviction_coordinator.register_pool(plan->new_pool);
        const auto entry = active_pools.find(plan->application);
        if (entry != active_pools.end()) {
            entry->second = plan->new_pool;
        }
        retired_apps.erase(plan->application);
        routing->publish(plan->new_snapshot);
    };
    const auto abort_activation = [&](capsid::host::ActivationPlan* plan) {
        std::lock_guard<std::mutex> lock(pools_mutex);
        // The persist failed: the disk still points at the old generation,
        // so the memory must too. The node check is defensive — the
        // single-threaded Async worker guarantees no other transaction
        // touched the view since prepare.
        const auto entry = active_pools.find(plan->application);
        if (entry != active_pools.end() && entry->second == plan->new_pool) {
            if (plan->old_pool != nullptr) {
                entry->second = plan->old_pool;
            } else {
                active_pools.erase(entry);
            }
        }
        // Prepare parked the still-serving old pool in draining_pools for
        // the commit that never came: undo that reference or repeated
        // failed activations accumulate parked copies (review §2).
        if (plan->old_pool != nullptr) {
            for (auto it = draining_pools.begin(); it != draining_pools.end();) {
                if (*it == plan->old_pool) {
                    it = draining_pools.erase(it);
                } else {
                    ++it;
                }
            }
        }
        // The plan (and with it the never-published pool) dies on return:
        // no strong self-hold exists (the drain hook's box is weak), so
        // the plain refcount drop runs ~GenerationPool on THIS
        // coordinator thread — request_drain + wait reap the fleet and
        // join the pump without any parking. Its ledger count was
        // already returned by abort_reserve, and the weak box makes the
        // late drain hook a no-op instead of a second release.
    };
    // ---- §9.3 retire transaction callbacks ----
    const auto prepare_retire =
        [&](const std::string& application,
            std::string* error) -> std::unique_ptr<capsid::host::RetirePlan> {
        (void)error;
        std::unique_ptr<capsid::host::RetirePlan> plan(new capsid::host::RetirePlan());
        plan->application = application;
        {
            std::lock_guard<std::mutex> lock(pools_mutex);
            const auto existing = active_pools.find(application);
            if (existing != active_pools.end()) {
                plan->pool = existing->second;
                plan->workers = existing->second->min_ready();
                // The route becomes a TOMBSTONE at prepare; commit
                // only publishes. New requests get 404 (§9.6-6) while
                // old in-flight requests finish on their pinned pool.
                active_pools.erase(existing);
                retired_apps.insert(application);
                draining_pools.push_back(plan->pool);
                plan->view_mutated = true;
            } else if (retired_apps.find(application) == retired_apps.end()) {
                // Idempotent retire of a never-deployed App: the
                // durable tombstone is (re)persisted; the route keeps
                // its current shape (tombstone pre-inserted so commit
                // has nothing to allocate).
                retired_apps.insert(application);
                plan->view_mutated = true;
            }
            plan->new_snapshot = build_snapshot();
        }
        return plan;
    };
    const auto commit_retire = [&](capsid::host::RetirePlan* plan) {
        std::lock_guard<std::mutex> lock(pools_mutex);
        if (plan->pool != nullptr) {
            // Drain signal only; keep registered in eviction_coordinator until
            // on_drain_complete so in-flight draining workers stay metered.
            plan->pool->request_drain();
        }
        routing->publish(plan->new_snapshot);
    };
    const auto abort_retire = [&](capsid::host::RetirePlan* plan) {
        std::lock_guard<std::mutex> lock(pools_mutex);
        if (!plan->view_mutated) {
            return;
        }
        const auto entry = active_pools.find(plan->application);
        if (plan->pool != nullptr) {
            // Restore the still-serving generation (may allocate — abort
            // runs only in the already-failed path).
            if (entry != active_pools.end()) {
                entry->second = plan->pool;
            } else {
                active_pools[plan->application] = plan->pool;
            }
            retired_apps.erase(plan->application);
            // And undo prepare's parking reference for the same reason
            // as the activation abort.
            for (auto it = draining_pools.begin(); it != draining_pools.end();) {
                if (*it == plan->pool) {
                    it = draining_pools.erase(it);
                } else {
                    ++it;
                }
            }
        } else {
            retired_apps.erase(plan->application);
        }
    };
    const auto reclaim_workers = [&]() {
        std::vector<std::shared_ptr<capsid::host::GenerationPool>> all;
        {
            std::lock_guard<std::mutex> lock(pools_mutex);
            for (const auto& entry : active_pools) {
                all.push_back(entry.second);
            }
            active_pools.clear();
            all.insert(all.end(), draining_pools.begin(), draining_pools.end());
            draining_pools.clear();
        }
        for (const auto& pool : all) {
            pool->stop_and_join();
        }
        // Every producer has left its completion hook. Drain queued joins
        // before the ledger, observer maps, listener and log can disappear.
        generation_reclaimer.stop_and_join();
    };
    // Wire the §9.3 transaction callbacks and the §9.4 ledger into every
    // App's coordinator options.
    for (capsid::host::ManagedHostOptions* options : app_options) {
        options->ledger = &ledger;
        options->tenancy = &config.tenancy;
        options->reclaim_elastic_slot = [&eviction_coordinator] {
            return eviction_coordinator.reclaim_one_idle_elastic();
        };
        options->prepare_activation = prepare_activation;
        options->commit_activation = commit_activation;
        options->abort_activation = abort_activation;
        options->prepare_retire = prepare_retire;
        options->commit_retire = commit_retire;
        options->abort_retire = abort_retire;
        options->check_tenant_deploy = [&eviction_coordinator, application = options->application](
                                           const std::string& tenant, std::uint32_t min_ready,
                                           std::string* error) {
            return eviction_coordinator.check_deploy_quota(application, tenant, min_ready, error);
        };
        options->reserve_tenant_deploy =
            [&eviction_coordinator,
             application = options->application](const std::string& tenant, std::uint32_t min_ready,
                                                 std::uint32_t warm_workers, std::string* error) {
                return eviction_coordinator.reserve_deploy_quota(application, tenant, min_ready,
                                                                 warm_workers, error);
            };
        options->release_tenant_deploy = [&eviction_coordinator,
                                          application = options->application] {
            eviction_coordinator.release_deploy_quota(application);
        };
    }
    // Startup recovery: a durable active App is revalidated and its
    // replacement worker reaches READY before Admin readiness is
    // published. The §9.4 reserve for the recovered fleet happens INSIDE
    // run_recover_operation — the coordinator learns the pool size only
    // after re-compiling the effective policy — and before any spawn: an
    // active-generation count beyond capacity fails closed at startup
    // instead of overspawning first.
    for (capsid::host::ManagedHostOptions* options : app_options) {
        capsid::host::OperationStatus status;
        const capsid::host::DeployOutcome recovered =
            capsid::host::managed_recover(options, &status);
        if (!recovered.ok) {
            fail("cannot recover active application " + options->application);
        }
        if (!recovered.workers.empty() ||
            recovered.recovery_action == capsid::host::ActiveStateRecoveryAction::kActivate) {
            // Keep a recovered pool's publication in the same App lock
            // domain as queued quarantine identity checks.
            capsid::host::AppOperationLock recovery_activation(options->application);
            // The reserve was committed inside run_recover_operation
            // (reserve_fresh before any spawn); a fresh adoption has
            // nothing further to settle.
            const std::shared_ptr<capsid::host::GenerationPool> pool =
                adopt_generation(options->application, recovered.tenant, recovered.workers,
                                 recovered.generation_factory, recovered.version,
                                 recovered.generation_digest, recovered.effective);
            if (pool == nullptr) {
                fail("cannot activate the recovered pool for application " + options->application);
            }
            pool->capacity_lease()->activate_fresh();
            {
                std::lock_guard<std::mutex> lock(pools_mutex);
                active_pools[options->application] = pool;
                eviction_coordinator.register_pool(pool);
                publish_snapshot();
            }
        } else if (recovered.recovery_action ==
                   capsid::host::ActiveStateRecoveryAction::kKeepRetired) {
            // Durable retired tombstone: mirror the coordinator's tombstone in
            // the route so requests continue answering 404 after restart
            // (§9.6-6), with no capacity occupancy. Quarantined or never-
            // deployed Apps stay unrouted (503).
            {
                std::lock_guard<std::mutex> lock(pools_mutex);
                retired_apps.insert(options->application);
                publish_snapshot();
            }
        }
    }
    // §9.2 data plane: 0..1 public listeners, bound all-or-fail BEFORE
    // Admin readiness is published (a configured listener that cannot bind
    // must fail the Host, never serve a degraded control plane). The v1
    // event sink is single-consumer — exactly one listener may own the
    // pools' fan-out — so more than one configured listener fails closed at
    // startup instead of serving with a broken response fan-out.
    if (config.listeners.size() > 1) {
        fail("host.json configures multiple listeners; the v1 data plane "
             "supports exactly one");
    }
    if (config.listeners.size() == 1) {
        capsid::host::ManagedListenerOptions listener_options;
        listener_options.config = config.listeners[0];
        listener_options.routing = routing;
        listener_options.log = structured_log.get();
        listener_options.metrics = metrics.get();
        listener_options.channel_service_budget = channel_budget;
        listener_options.max_request_body_bytes = capsid::host::effective_max_request_body_bytes(
            config.policy.default_worker_memory_bytes);
        data_plane = std::make_unique<capsid::host::ManagedListener>(std::move(listener_options));
        std::string listener_error;
        if (!data_plane->start(&listener_error)) {
            fail("cannot bind the data plane listener: " + listener_error);
        }
    }
    // M2 item 5a (direction A): one supervisor thread per App schedules
    // the active-health probes against the App's generation pool. The
    // pool is the only recovery engine; the supervisor only decides when
    // to probe and when consecutive failures recycle the worker. Created
    // after startup recovery so recovered pools exist; joined on shutdown
    // after the worker reclaim.
    std::vector<std::unique_ptr<capsid::host::WorkerSupervisor>> supervisors;
    for (capsid::host::ManagedHostOptions* options : app_options) {
        capsid::host::WorkerSupervisorOptions supervisor_options;
        supervisor_options.managed_options = options;
        supervisor_options.policy = recovery_resolved.policy;
        supervisor_options.current_pool =
            [&, application =
                    options->application]() -> std::shared_ptr<capsid::host::GenerationPool> {
            std::lock_guard<std::mutex> lock(pools_mutex);
            const auto existing = active_pools.find(application);
            return existing != active_pools.end() ? existing->second : nullptr;
        };
        supervisor_options.stop_requested = &g_stop;
        // M2 item 7: the process-wide log/metrics (probe events are
        // app-lane; the recycle verdicts feed §12.1).
        supervisor_options.log = structured_log.get();
        supervisor_options.metrics = metrics.get();
        // M2 item 6 (design §7.4): the active health probe schedule.
        // Individual Apps opt in with capsid.json healthCheck; the host
        // interval/failures apply to every configured App.
        supervisor_options.active_health_interval_ms = config.recovery.active_health_interval_ms;
        supervisor_options.active_health_failures = config.recovery.active_health_failures;
        supervisors.push_back(
            std::make_unique<capsid::host::WorkerSupervisor>(std::move(supervisor_options)));
    }
    capsid::host::ManagedAdminBackend managed(app_options);
    // M2 item 5b: the shared fair startup-permit queue. Capacity itself is
    // gated by the §9.4 ledger inside the coordinator (see run_deploy_operation);
    // the queue only decides ORDER and fairness across Apps.
    managed.startup_permits = &startup_permits;
    managed.metrics = metrics.get();
    capsid::host::AsyncAdminBackendOptions async_options;
    // Fixed bounded queue; startupsConcurrent is not the Admin ceiling.
    async_options.max_pending_operations = 8;
    async_options.external_stop = &g_stop;
    auto async = std::make_unique<capsid::host::AsyncAdminBackend>(&managed, async_options);
    capsid::host::AdminServiceOptions service_options;
    service_options.socket.path = config.admin_unix_path;
    service_options.socket.mode = config.admin_mode;
    service_options.http.api.authorization.allowed_uid = static_cast<std::uint64_t>(geteuid());
    service_options.http.api.max_header_bytes = 64U * 1024U;
    service_options.http.api.max_body_bytes = 64U * 1024U;
    // M2 item 7: the Admin API renders /metrics from the process-wide
    // registry and logs authorization failures as control-plane events.
    service_options.http.api.log = structured_log.get();
    service_options.http.api.metrics = metrics.get();
    service_options.http.header_timeout_ms = 5000;
    service_options.http.body_timeout_ms = 5000;
    service_options.http.write_timeout_ms = 5000;
    capsid::host::AdminService service(service_options, async.get());
    std::string service_error;
    if (!service.start(&service_error)) {
        fail("cannot start admin service: " + service_error);
    }
    eviction_coordinator.start();
    // Shutdown order: stop the Admin control plane, cancel queued/running
    // deploys, reclaim active workers, then exit. The service's wait
    // removes the socket inode it created.
    int signal_number = 0;
    if (sigwait(&term_set, &signal_number) != 0) {
        fail("cannot wait for SIGTERM");
    }
    g_stop.store(true);
    eviction_coordinator.stop_and_join();
    service.request_stop();
    if (!service.wait(&service_error)) {
        fail("admin service failed: " + service_error);
    }
    // Explicitly wait for the async executor to settle (queued work
    // cancelled, running deploys interrupted) BEFORE reclaiming workers;
    // shutdown never depends on destructor ordering at function return.
    async.reset();
    // The data plane stops accepting and closes its sessions BEFORE the
    // pools drain: no new request can pin a dying pool, and the mailbox
    // (owned by the listener) outlives every pool's sink.
    if (data_plane != nullptr) {
        data_plane->request_stop();
        if (!data_plane->wait(&service_error)) {
            fail("data plane failed: " + service_error);
        }
    }
    reclaim_workers();
    // The reclaim destroyed every observed worker, which closes its IPC fd;
    // the supervisor threads see the channel die and exit. Joined after the
    // reclaim so their stop is a no-op observation, never a counted event.
    for (const auto& supervisor : supervisors) {
        supervisor->join();
    }
    quarantine_dispatcher.stop_and_join();
    close(state_fd);
    // M2 item 7 (§12.2): the shutdown event lands, then both lanes drain
    // (the supervisor join above already closed every observed channel).
    structured_log->log(capsid::host::LogLane::kControl,
                        {.event = capsid::host::log_events::kShutdown});
    structured_log->flush();
    return 0;
}
#endif // !defined(_WIN32) managed mode

// Keep the watcher inside this call so it is joined before the caller's
// host object is destroyed, including when startup or run() throws.
int run_local_with_parent_stop(capsid::host::LocalDevHost& host,
                               std::unique_ptr<capsid::host::ParentStopMonitor> monitor,
                               capsid::host::StructuredLog* log) {
    std::string error;
    if (monitor && !monitor->start([&host] { host.request_stop(); }, &error)) {
        log->log(capsid::host::LogLane::kControl,
                 {.event = capsid::host::log_events::kStartup, .result = "fail", .message = error});
        return 1;
    }
    int result = host.run();
    if (monitor) {
        monitor->cancel();
        if (!monitor->error().empty()) {
            log->log(capsid::host::LogLane::kControl, {.event = capsid::host::log_events::kShutdown,
                                                       .result = "fail",
                                                       .message = monitor->error()});
            result = 1;
        }
    }
    return result;
}
} // namespace

int main(int argc, char** argv) {
#if defined(__GLIBC__)
    // P1 follow-up (review §12.2): transient pool/replacement threads
    // each parked a 64 MiB glibc malloc-arena reservation in the Host's
    // address space for its whole lifetime. Retired generations are now
    // destroyed promptly, so the thread population is small and stable;
    // cap the arena count so address space does not creep per
    // replacement burst. Threads beyond the cap share arenas (no
    // failure mode), only allocation contention.
    mallopt(M_ARENA_MAX, 8);
#endif
    std::map<std::string, std::string> values;
    for (int index = 1; index < argc; index += 2) {
        const char* key = argv[index];
        const char* value = index + 1 < argc ? argv[index + 1] : nullptr;
        if (key == nullptr || key[0] != '-' || key[1] != '-') {
            fail("arguments must be --key value pairs");
        }
        if (value == nullptr || value[0] == '\0') {
            fail(std::string("missing value for ") + key);
        }
        const std::string name = key + 2;
        if (values.find(name) != values.end()) {
            fail("duplicate argument: --" + name);
        }
        values[name] = value;
    }

    const auto optional_value = [&values](const std::string& name) -> const std::string* {
        const auto it = values.find(name);
        return it == values.end() ? nullptr : &it->second;
    };

    if (const std::string* mode = optional_value("mode")) {
        if (*mode == "managed") {
#if defined(__linux__)
            const auto require = [&values](const std::string& name) -> std::string {
                auto it = values.find(name);
                if (it == values.end()) {
                    fail("missing --" + name);
                }
                return it->second;
            };
            // Strict managed CLI: only --host-config and --worker are allowed.
            for (const std::pair<const std::string, std::string>& entry : values) {
                if (entry.first != "mode" && entry.first != "host-config" &&
                    entry.first != "worker") {
                    fail("--mode managed accepts only --host-config and --worker");
                }
            }
            return run_managed(require("host-config"), require("worker"));
#else
            fail("--mode managed is unavailable on this platform: the managed "
                 "coordinator requires Linux strict sandbox (see "
                 "docs/reference/platform-support.md)");
#endif
        }
        fail("unknown --mode \"" + *mode + "\": only --mode managed is supported on Linux");
    }

    // Strict allowlist for local dev CLI (docs/design/config-v2.md §5).
    static constexpr std::string_view known_local_arguments[] = {
        "app-dir",     "app",          "host-config",    "listen",   "worker",  "workers",
        "secrets-dir", "bindings-dir", "strict-sandbox", "ready-fd", "stop-fd",
    };
    for (const auto& argument : values) {
        bool known = false;
        for (const auto name : known_local_arguments) {
            if (argument.first == name) {
                known = true;
                break;
            }
        }
        if (!known) {
            fail("unknown argument: --" + argument.first);
        }
    }

    capsid::host::LocalDevHostOptions options;
    if (const std::string* app_dir = optional_value("app-dir")) {
        options.app_dir = *app_dir;
    }
    if (const std::string* app = optional_value("app")) {
        options.application = *app;
    }
    if (!valid_application_id(options.application)) {
        fail("--app is not a valid App ID");
    }
    if (const std::string* host_cfg = optional_value("host-config")) {
        options.host_config_path = *host_cfg;
    }
    if (const std::string* listen = optional_value("listen")) {
        std::string addr;
        std::uint16_t port = 0;
        parse_listen(*listen, &addr, &port);
        boost::system::error_code address_error;
        const boost::asio::ip::address parsed = boost::asio::ip::make_address(addr, address_error);
        if (address_error) {
            fail("--listen requires an IP address: " + addr);
        }
        (void)parsed;
        options.listen = *listen;
        options.listen_explicit = true;
    }
    if (const std::string* worker = optional_value("worker")) {
        options.worker_path = *worker;
    }
    if (const std::string* workers = optional_value("workers")) {
        const std::uint64_t parsed = parse_positive_integer(*workers, "workers");
        constexpr std::uint64_t kMaxLocalWorkers = 256;
        if (parsed > kMaxLocalWorkers) {
            fail("--workers exceeds the local worker limit (" + std::to_string(kMaxLocalWorkers) +
                 ")");
        }
        options.workers_override = static_cast<std::uint32_t>(parsed);
    }
    if (const std::string* secrets_dir = optional_value("secrets-dir")) {
        options.secrets_dir = *secrets_dir;
    }
    if (const std::string* bindings_dir = optional_value("bindings-dir")) {
        options.bindings_dir = *bindings_dir;
    }
    if (const std::string* sandbox = optional_value("strict-sandbox")) {
        if (*sandbox != "on" && *sandbox != "off") {
            fail("--strict-sandbox must be on or off");
        }
        options.strict_sandbox = (*sandbox == "on");
    }
#if !defined(__linux__)
    if (options.strict_sandbox) {
        fail(
            "--strict-sandbox on requires Linux strict sandbox (see docs/reference/platform-support.md)");
    }
#endif
    std::optional<std::uint64_t> ready_fd;
    if (const std::string* ready_text = optional_value("ready-fd")) {
        const std::uint64_t parsed = parse_positive_integer(*ready_text, "ready-fd");
        if (parsed > static_cast<std::uint64_t>(std::numeric_limits<short>::max())) {
            fail("--ready-fd must be a positive descriptor number");
        }
        ready_fd = parsed;
        options.ready_fd = static_cast<int>(parsed);
#if defined(_WIN32)
        if (!ready_fd_is_open(options.ready_fd)) {
            fail("--ready-fd is not an open descriptor");
        }
#else
        if (fcntl(options.ready_fd, F_GETFD) == -1) {
            fail("--ready-fd is not an open descriptor");
        }
#endif
    }

    std::unique_ptr<capsid::host::ParentStopMonitor> parent_stop;
    if (const std::string* stop_text = optional_value("stop-fd")) {
        const std::uint64_t stop_fd = parse_positive_integer(*stop_text, "stop-fd");
        if (stop_fd < 3 ||
            stop_fd > static_cast<std::uint64_t>(std::numeric_limits<short>::max()) ||
            (ready_fd.has_value() && stop_fd == *ready_fd)) {
            fail("--stop-fd must be a descriptor above 2, distinct from --ready-fd");
        }
        std::string stop_error;
        parent_stop =
            capsid::host::ParentStopMonitor::create(static_cast<int>(stop_fd), &stop_error);
        if (!parent_stop) {
            fail("--stop-fd: " + stop_error);
        }
    }

    auto structured_log =
        std::make_unique<capsid::host::StructuredLog>([](const std::string& line) {
#if defined(_WIN32)
            const ssize_t written =
                ::write(STDERR_FILENO, line.data(), static_cast<unsigned int>(line.size()));
#else
            const ssize_t written = ::write(STDERR_FILENO, line.data(), line.size());
#endif
            (void)written;
        });
    auto metrics = std::make_unique<capsid::host::MetricsRegistry>();
    options.log = structured_log.get();
    options.metrics = metrics.get();

    capsid::host::LocalDevHost host(std::move(options));
    const int exit_code =
        run_local_with_parent_stop(host, std::move(parent_stop), structured_log.get());
    structured_log->log(capsid::host::LogLane::kControl,
                        {.event = capsid::host::log_events::kShutdown});
    structured_log->flush();
    return exit_code;
}
