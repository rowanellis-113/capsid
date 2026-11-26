// Stage 3 Global Memory Watermark Eviction & Soft Tenant Fairness Tests:
//   - phase1_overshare_protects_small_tenant:
//       Multi-tenant fleet above highWatermark (85%): noisy tenant exceeding fair
//       share loses idle LRU workers with reason="tenant_overshare" down to
//       lowWatermark (75%), while small tenant below fair share and VIP tenant
//       with minSoftMemory stay protected.
//   - phase2_global_lru_and_min_residency:
//       Checks MemorySampler cgroup primary + zero-syscall cached PSS fallback,
//       minResidency protection, minReady floor, busy SSE protection, and
//       fallback to reason="global_lru" when all tenants comply with soft shares.
//   - tenant_quota_swap_and_rejections:
//       Verifies tenant_quota_swap across sibling apps + all 7 rejection reason
//       counters (queue_disabled, queue_full, queue_timeout, app_max_workers,
//       tenant_max_workers, tenant_hard_memory, host_capacity_exhausted).
//   - admin_deploy_tenant_and_persistence (POSIX):
//       End-to-end POST /v1/deploy tenant binding, GET /v1/apps/<app>, v1-compatible
//       omission in active.json when omitted, recovery, retire, and quota gate.

#include "win32_compat.h"
#if !defined(_WIN32)
#include "host/active_state.h"
#include "host/admin_api.h"
#include "host/managed_admin_backend.h"
#endif
#include "host/eviction_coordinator.h"
#include "host/generation_pool.h"
#include "host/host_config_model.h"
#include "host/managed_host.h"
#include "host/managed_listener.h"
#include "host/memory_sampler.h"
#include "host/metrics.h"
#include "host/protocol_launch.h"
#include "host/routing_snapshot.h"
#include "host/structured_log.h"
#include "host/worker_capacity_ledger.h"
#include "host/worker_cgroup.h"

#include "capsid/runtime.h"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void close_socket_handle(SocketHandle handle) { closesocket(handle); }
bool is_connection_reset_error() {
    const int err = WSAGetLastError();
    return err == WSAECONNRESET || err == WSAECONNABORTED;
}
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
void close_socket_handle(SocketHandle handle) { close(handle); }
bool is_connection_reset_error() { return errno == ECONNRESET || errno == EPIPE; }
#endif

using capsid::host::GenerationPool;
using capsid::host::GenerationPoolOptions;
using capsid::host::GlobalEvictionCoordinator;
using capsid::host::ListenerConfig;
using capsid::host::ManagedHostOptions;
using capsid::host::ManagedListener;
using capsid::host::ManagedListenerOptions;
using capsid::host::MemoryBudgetConfig;
using capsid::host::MemorySampler;
using capsid::host::MetricsRegistry;
using capsid::host::RoutingSnapshot;
using capsid::host::RoutingTable;
using capsid::host::TenancyConfig;
using capsid::host::TenantQuotaConfig;
using capsid::host::WorkerCapacityLedger;
using capsid::host::WorkerExecutor;
using capsid::host::WorkerRecoveryPolicy;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test-host-eviction-coordinator: FAIL: " << message << std::endl;
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

bool wait_for(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    return predicate();
}

const char* kWorkerBundle =
    "export default {\n"
    "  async fetch(request) {\n"
    "    const url = new URL(request.url);\n"
    "    if (url.pathname === '/sse') {\n"
    "      const hold = Number(url.searchParams.get('hold') ?? '300');\n"
    "      const enc = new TextEncoder();\n"
    "      const body = new ReadableStream({\n"
    "        async start(controller) {\n"
    "          controller.enqueue(enc.encode('data: open\\n\\n'));\n"
    "          await new Promise((r) => setTimeout(r, hold));\n"
    "          controller.enqueue(enc.encode('data: done\\n\\n'));\n"
    "          controller.close();\n"
    "        }\n"
    "      });\n"
    "      return new Response(body, { headers: { 'Content-Type': 'text/event-stream' } });\n"
    "    }\n"
    "    const delay = Number(url.searchParams.get('delay') ?? '0');\n"
    "    if (delay > 0) await new Promise((r) => setTimeout(r, delay));\n"
    "    return new Response('ok');\n"
    "  }\n"
    "};\n";

WorkerExecutor::WorkerFactory spawn_factory(const std::string& worker_path,
                                            std::uint32_t request_timeout_ms,
                                            const std::string& bundle) {
    return [worker_path, request_timeout_ms, bundle](capsid_worker** out,
                                                     std::string* factory_error) -> bool {
        capsid_worker_config config;
        capsid_worker_config_init(&config);
        config.worker_path = worker_path.c_str();
        config.request_timeout_ms = request_timeout_ms;
        capsid_worker* worker = nullptr;
        const capsid::host::ProtocolLaunch launch;
        if (capsid::host::spawn_with_protocol(&config, launch, &worker) != CAPSID_OK) {
            *factory_error = "worker spawn failed";
            return false;
        }
        if (capsid_worker_load_bundle(worker, reinterpret_cast<const std::uint8_t*>(bundle.data()),
                                      bundle.size()) != CAPSID_OK) {
            capsid_worker_destroy(worker);
            *factory_error = "bundle load failed";
            return false;
        }
        if (capsid_worker_flush(worker) != CAPSID_OK) {
            capsid_worker_destroy(worker);
            *factory_error = "bundle flush failed";
            return false;
        }
        *out = worker;
        return true;
    };
}

WorkerRecoveryPolicy default_recovery_policy() {
    WorkerRecoveryPolicy policy;
    policy.max_events = 3;
    policy.window_ms = 60000;
    policy.backoff_initial_ms = 20;
    policy.backoff_maximum_ms = 500;
    policy.jitter_basis_points = 0;
    policy.stable_reset_ms = 60000;
    policy.replacements_concurrent_per_app = 1;
    return policy;
}

ListenerConfig loopback_path_listener() {
    ListenerConfig config;
    config.name = "eviction-test";
    config.tcp = "127.0.0.1:0";
    config.public_scheme = "http";
    config.public_authority = "localhost";
    config.routing.mode = "path";
    return config;
}

void set_socket_recv_timeout(SocketHandle fd, int seconds) {
#if defined(_WIN32)
    const DWORD timeout_ms = static_cast<DWORD>(seconds * 1000);
    require(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms),
                       sizeof(timeout_ms)) == 0,
            "cannot set socket timeout");
#else
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    require(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0,
            "cannot set socket timeout");
#endif
}

SocketHandle connect_listener(std::uint16_t port) {
    const SocketHandle fd = socket(AF_INET, SOCK_STREAM, 0);
    require(fd != kInvalidSocket, "cannot create HTTP socket");
    set_socket_recv_timeout(fd, 15);
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    require(inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1,
            "cannot format loopback address");
    require(connect(fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) == 0,
            "cannot connect to listener");
    return fd;
}

void send_raw_request(SocketHandle fd, const std::string& request) {
    std::size_t sent = 0;
    while (sent < request.size()) {
#if defined(_WIN32)
        const int count =
            send(fd, request.data() + sent, static_cast<int>(request.size() - sent), 0);
#else
        const ssize_t count = send(fd, request.data() + sent, request.size() - sent, 0);
#endif
        if (count <= 0) {
            fail("cannot send HTTP request");
        }
        sent += static_cast<std::size_t>(count);
    }
}

struct HttpResponse {
    int status = 0;
    std::string head;
    std::string body;
};

HttpResponse read_http_response(SocketHandle fd) {
    std::string wire;
    char buffer[4096];
    bool done = false;
    bool chunked = false;
    std::string chunked_body;
    while (!done) {
#if defined(_WIN32)
        const int count = recv(fd, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
        const ssize_t count = recv(fd, buffer, sizeof(buffer), 0);
#endif
        if (count < 0) {
            if (is_connection_reset_error() && wire.empty()) {
                break;
            }
            fail("cannot read HTTP response");
        }
        if (count == 0) {
            break;
        }
        wire.append(buffer, static_cast<std::size_t>(count));
        const std::string::size_type head_end = wire.find("\r\n\r\n");
        if (head_end == std::string::npos) {
            continue;
        }
        const std::string head = wire.substr(0, head_end);
        std::string::size_type offset = head.find("Content-Length:");
        if (offset == std::string::npos) {
            offset = head.find("content-length:");
        }
        if (offset != std::string::npos) {
            offset += std::string("content-length:").size();
            while (offset < head.size() && head[offset] == ' ') {
                ++offset;
            }
            const std::string::size_type end = head.find("\r\n", offset);
            const std::size_t length = static_cast<std::size_t>(
                std::strtoull(head.substr(offset, end - offset).c_str(), nullptr, 10));
            if (wire.size() >= head_end + 4 + length) {
                wire = wire.substr(0, head_end + 4 + length);
                done = true;
            }
            continue;
        }
        offset = head.find("Transfer-Encoding:");
        if (offset == std::string::npos) {
            offset = head.find("transfer-encoding:");
        }
        if (offset != std::string::npos && head.find("chunked", offset) != std::string::npos) {
            chunked = true;
            std::string::size_type pos = head_end + 4;
            chunked_body.clear();
            for (;;) {
                const std::string::size_type line_end = wire.find("\r\n", pos);
                if (line_end == std::string::npos) {
                    break;
                }
                const std::string size_field = wire.substr(pos, line_end - pos);
                const std::string::size_type ext = size_field.find(';');
                const std::uint64_t size =
                    std::strtoull(size_field.substr(0, ext).c_str(), nullptr, 16);
                if (size == 0) {
                    if (wire.size() < line_end + 4) {
                        break;
                    }
                    done = true;
                    break;
                }
                if (wire.size() < line_end + 2 + size + 2) {
                    break;
                }
                chunked_body.append(wire, line_end + 2, static_cast<std::size_t>(size));
                pos = line_end + 2 + static_cast<std::size_t>(size) + 2;
            }
        }
    }
    HttpResponse response;
    const std::string::size_type head_end = wire.find("\r\n\r\n");
    if (head_end == std::string::npos) {
        return response;
    }
    response.head = wire.substr(0, head_end);
    response.body = chunked ? std::move(chunked_body) : wire.substr(head_end + 4);
    const std::string::size_type space = response.head.find(' ');
    if (space != std::string::npos) {
        response.status = std::atoi(response.head.c_str() + space + 1);
    }
    return response;
}

HttpResponse do_http_get(std::uint16_t port, const std::string& path) {
    const SocketHandle fd = connect_listener(port);
    send_raw_request(fd,
                     "GET " + path + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    const HttpResponse response = read_http_response(fd);
    close_socket_handle(fd);
    return response;
}

std::shared_ptr<GenerationPool>
make_elastic_pool(const std::string& worker_path, const std::string& app, const std::string& tenant,
                  std::uint32_t min_ready, std::uint32_t max_workers, std::uint64_t queue_requests,
                  std::uint64_t queue_timeout_ms, std::uint64_t max_inflight,
                  WorkerCapacityLedger* ledger, GlobalEvictionCoordinator* coordinator,
                  MetricsRegistry* metrics, const std::string& bundle = kWorkerBundle,
                  std::function<void()> on_executor_notify = {}) {
    GenerationPoolOptions opts;
    opts.application_id = app;
    opts.tenant_id = tenant;
    opts.version = "v1";
    opts.generation_digest = "sha256:" + std::string(64, 'a');
    opts.workers = max_workers;
    opts.has_min_ready = true;
    opts.min_ready = min_ready;
    opts.idle_timeout_ms = 0;
    opts.max_inflight_per_worker = max_inflight;
    opts.max_queued_requests = queue_requests;
    opts.queue_timeout_ms = queue_timeout_ms;
    opts.factory = spawn_factory(worker_path, 5000, bundle);
    opts.on_executor_notify = std::move(on_executor_notify);
    opts.recovery = default_recovery_policy();
    opts.metrics = metrics;
    opts.acquire_worker_slot = [coordinator](const GenerationPool* req, std::string* reason) {
        return coordinator->acquire_worker_slot(req, reason);
    };
    opts.release_elastic_slot = [ledger, app] { ledger->release_elastic(app, 1); };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "GenerationPool::create failed for " + app + ": " + error);
    coordinator->register_pool(pool);
    return pool;
}

void assign_pid_footprints(const std::shared_ptr<GenerationPool>& pool,
                           std::uint64_t bytes_per_worker, std::mutex* map_mutex,
                           std::unordered_map<std::int64_t, std::uint64_t>* pid_bytes) {
    std::lock_guard<std::mutex> lock(*map_mutex);
    for (const auto& w : pool->snapshot_workers()) {
        (*pid_bytes)[w.pid] = bytes_per_worker;
    }
}

void run_deploy_with_protected_sibling(const std::string& worker_path) {
    MemoryBudgetConfig memory;
    memory.min_residency_ms = 60000;
    TenancyConfig tenancy;
    tenancy.configured = true;
    tenancy.overrides["team"].max_workers = 2;
    WorkerCapacityLedger ledger(8, 2);
    GlobalEvictionCoordinator coordinator(memory, tenancy, &ledger, nullptr);
    require(ledger.reserve_fresh("orders", 1), "reserve the sibling's floor");
    ledger.commit_fresh("orders");
    auto pool = make_elastic_pool(worker_path, "orders", "team", 1, 2, 0, 0, 1, &ledger,
                                  &coordinator, nullptr);
    std::string error;
    require(coordinator.check_deploy_quota("billing", "team", 1, &error),
            "both floors should fit before scale-out");
    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "could not fill the tenant's second slot");
    require(wait_for([&] { return pool->ready_workers() == 2; }, std::chrono::seconds(5)),
            "sibling did not reach two ready workers");
    const bool other_tenant = coordinator.check_deploy_quota("reports", "other", 1, &error);
    const bool admitted = coordinator.check_deploy_quota("billing", "team", 1, &error);
    const auto ready = pool->ready_workers();
    const auto elastic = ledger.elastic_used();
    pool->stop_and_join();
    ledger.release_active("orders", 1);
    require(other_tenant, "one full tenant blocked an unrelated tenant");
    require(!admitted, "deploy admitted a third tenant worker while both slots were protected");
    require(ready == 2 && elastic == 1, "quota check disturbed a protected sibling");
    require(coordinator.check_deploy_quota("billing", "team", 1, &error),
            "deploy was still blocked after the sibling drained");
}

void run_deploy_at_tenant_memory_limit(const std::string& worker_path) {
    constexpr std::uint64_t limit = 16 * 1024 * 1024;
    TenancyConfig tenancy;
    tenancy.configured = true;
    tenancy.overrides["team"].hard_memory_bytes = limit;
    MemorySampler sampler({}, [](std::int64_t, std::uint64_t* bytes) {
        *bytes = limit;
        return true;
    });
    WorkerCapacityLedger ledger(8, 0);
    GlobalEvictionCoordinator coordinator({}, tenancy, &ledger, &sampler);
    std::string error;
    require(coordinator.check_deploy_quota("billing", "team", 1, &error),
            "empty tenant should have memory headroom");
    require(ledger.reserve_fresh("orders", 1), "reserve the serving floor");
    ledger.commit_fresh("orders");
    auto pool = make_elastic_pool(worker_path, "orders", "team", 1, 1, 0, 0, 1, &ledger,
                                  &coordinator, nullptr);
    const bool other_tenant = coordinator.check_deploy_quota("reports", "other", 1, &error);
    const bool admitted = coordinator.check_deploy_quota("billing", "team", 1, &error);
    pool->stop_and_join();
    ledger.release_active("orders", 1);
    require(other_tenant, "memory charged to the wrong tenant");
    require(!admitted, "deploy passed at hardMemory with no evictable sibling or maxWorkers cap");
    require(coordinator.check_deploy_quota("billing", "team", 1, &error),
            "tenant footprint survived its last worker");
}

void run_tenant_memory_recovers_without_global_budget(const std::string& worker_path) {
    constexpr std::uint64_t limit = 16 * 1024 * 1024;
    std::atomic<std::uint64_t> footprint{limit};
    MemorySampler sampler({}, [&](std::int64_t, std::uint64_t* bytes) {
        *bytes = footprint.load();
        return true;
    });
    TenancyConfig tenancy;
    tenancy.configured = true;
    tenancy.overrides["team"].hard_memory_bytes = limit;
    WorkerCapacityLedger ledger(2, 0);
    require(ledger.reserve_fresh("orders", 1), "reserve the serving floor");
    ledger.commit_fresh("orders");
    MemoryBudgetConfig memory;
    GlobalEvictionCoordinator coordinator(memory, tenancy, &ledger, &sampler);
    auto pool = make_elastic_pool(worker_path, "orders", "team", 1, 2, 0, 0, 1, &ledger,
                                  &coordinator, nullptr);
    coordinator.start();
    std::string reason;
    const auto first = pool->request_scale_out(&reason);
    require(first == GenerationPool::ScaleOutResult::kCapacityRejected &&
                reason == "tenant_hard_memory" && ledger.elastic_used() == 0,
            "initial footprint at hardMemory should block the second worker");
    footprint.store(limit / 4);
    // Allow two default sampling periods. The same live PID's footprint fell, so admission
    // must recover even when only a tenant budget is configured.
    const bool recovered = wait_for(
        [&] { return pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight; },
        std::chrono::milliseconds(2 * memory.check_interval_ms + 1000));
    const bool ready =
        recovered && wait_for([&] { return pool->ready_workers() == 2; }, std::chrono::seconds(5));
    coordinator.stop_and_join();
    pool->stop_and_join();
    ledger.release_active("orders", 1);
    require(ready, "tenant-only memory admission did not observe the decreased footprint");
    require(ledger.elastic_used() == 0, "elastic slot remained after drain");
}

void run_shrink_to_floor_returns_elastic_slot(const std::string& worker_path) {
    WorkerCapacityLedger ledger(2, 0);
    require(ledger.reserve_fresh("orders", 1), "reserve serving worker");
    ledger.commit_fresh("orders");
    GlobalEvictionCoordinator coordinator({}, {}, &ledger, nullptr);
    auto pool = make_elastic_pool(worker_path, "orders", "team", 1, 2, 0, 0, 1, &ledger,
                                  &coordinator, nullptr);
    // LRU timestamps have millisecond precision. Start the elastic worker on a later
    // tick so an arbitrary tie cannot hide an accounting error when the floor is older.
    const auto floor_activity = pool->snapshot_workers().at(0).last_active_at_ms;
    require(wait_for(
                [&] {
                    const auto now = std::chrono::steady_clock::now().time_since_epoch();
                    const auto ms =
                        std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
                    return static_cast<std::uint64_t>(ms) > floor_activity;
                },
                std::chrono::seconds(2)),
            "clock did not advance past the floor worker's last activity");
    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "could not allocate the remaining elastic slot");
    require(wait_for([&] { return pool->ready_workers() == 2; }, std::chrono::seconds(5)),
            "elastic worker did not become ready");
    require(ledger.elastic_of("orders") == 1, "missing elastic charge before eviction");
    require(pool->evict_idle_worker(0, "global_lru"), "expected one idle worker above floor");
    const auto ready = pool->ready_workers();
    const auto charged = ledger.elastic_of("orders");
    const bool reused = ledger.try_acquire_elastic("billing");
    if (reused) {
        ledger.release_elastic("billing");
    }
    pool->stop_and_join();
    ledger.release_active("orders", 1);
    require(ready == 1, "eviction did not preserve the serving floor");
    require(charged == 0 && reused, "only the floor remains but a peer cannot reuse the slot");
}

void run_draining_generation_counted_in_memory(const std::string& worker_path) {
    constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
    MemoryBudgetConfig mem;
    mem.configured = true;
    mem.limit_bytes = 25ULL * kMiB;
    mem.high_watermark_percent = 85;
    mem.low_watermark_percent = 75;
    mem.min_residency_ms = 0;
    mem.check_interval_ms = 0;

    TenancyConfig tenancy;
    tenancy.configured = true;
    tenancy.overrides["team"].hard_memory_bytes = 25ULL * kMiB;

    std::mutex fp_mutex;
    std::unordered_map<std::int64_t, std::uint64_t> pid_bytes;
    MemorySampler sampler({}, [&](std::int64_t pid, std::uint64_t* bytes_out) {
        std::lock_guard<std::mutex> lock(fp_mutex);
        const auto found = pid_bytes.find(pid);
        if (found == pid_bytes.end()) {
            return false;
        }
        *bytes_out = found->second;
        return true;
    });

    WorkerCapacityLedger ledger(4, 2);
    MetricsRegistry metrics;
    GlobalEvictionCoordinator coordinator(mem, tenancy, &ledger, &sampler, &metrics);

    require(ledger.reserve_fresh("orders", 1), "reserve orders floor");
    ledger.commit_fresh("orders");
    auto old_pool = make_elastic_pool(worker_path, "orders", "team", 1, 1, 0, 0, 1, &ledger,
                                      &coordinator, &metrics);
    auto peer_pool = make_elastic_pool(worker_path, "peer", "other", 0, 1, 0, 0, 1, &ledger,
                                       &coordinator, &metrics);

    auto routing = std::make_shared<RoutingTable>();
    routing->publish(RoutingSnapshot::build({{"orders", old_pool}, {"peer", peer_pool}}));

    ManagedListenerOptions lopts;
    lopts.config = loopback_path_listener();
    lopts.routing = routing;
    lopts.metrics = &metrics;
    ManagedListener listener(lopts);
    std::string error;
    require(listener.start(&error), "listener start failed: " + error);
    const std::uint16_t port = listener.bound_port();

    std::thread sse_thread([port] {
        const HttpResponse sse = do_http_get(port, "/@capsid/orders/sse?hold=700");
        require(sse.status == 200 && sse.body.find("data: done") != std::string::npos,
                "orders draining SSE failed");
    });
    require(wait_for([&] { return old_pool->ready_workers() == 1 && old_pool->inflight() == 1; },
                     std::chrono::seconds(4)),
            "orders pool did not enter busy SSE");
    assign_pid_footprints(old_pool, 25ULL * kMiB, &fp_mutex, &pid_bytes);

    old_pool->request_drain();
    require(old_pool->state() == GenerationPool::State::kDraining,
            "old pool did not enter kDraining");

    // While old_pool is still draining an active SSE stream, its footprint must continue
    // to block both same-tenant deploy hardMemory and cross-tenant global limit scale-out.
    std::string deploy_err;
    require(!coordinator.check_deploy_quota("orders-next", "team", 1, &deploy_err),
            "draining generation was omitted from tenant hardMemory deploy gate");
    std::string reason;
    require(peer_pool->request_scale_out(&reason) ==
                    GenerationPool::ScaleOutResult::kCapacityRejected &&
                reason == "host_capacity_exhausted",
            "draining generation was omitted from global memory limit gate, got: " + reason);

    sse_thread.join();
    old_pool->stop_and_join();
    ledger.release_active("orders", 1);

    require(coordinator.check_deploy_quota("orders-next", "team", 1, &deploy_err),
            "deploy quota stayed blocked after draining generation exited: " + deploy_err);
    require(peer_pool->request_scale_out(&reason) == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "global headroom did not recover after draining generation exited: " + reason);
    require(wait_for([&] { return peer_pool->ready_workers() == 1; }, std::chrono::seconds(4)),
            "peer pool did not become ready after draining exit");

    listener.request_stop();
    listener.wait(&error);
    peer_pool->stop_and_join();
    std::cout << "PASS: draining_generation_counted_in_memory" << std::endl;
}

void run_active_retiring_worker_counted_in_soft_watermark(const std::string& worker_path) {
    constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
    MemoryBudgetConfig mem;
    mem.configured = true;
    mem.limit_bytes = 100ULL * kMiB;
    mem.high_watermark_percent = 85;
    mem.low_watermark_percent = 75;
    mem.min_residency_ms = 0;
    mem.check_interval_ms = 0;

    std::mutex fp_mutex;
    std::unordered_map<std::int64_t, std::uint64_t> pid_bytes;
    MemorySampler sampler({}, [&](std::int64_t pid, std::uint64_t* bytes_out) {
        std::lock_guard<std::mutex> lock(fp_mutex);
        const auto found = pid_bytes.find(pid);
        if (found == pid_bytes.end()) {
            return false;
        }
        *bytes_out = found->second;
        return true;
    });

    WorkerCapacityLedger ledger(8, 0);
    MetricsRegistry metrics;
    GlobalEvictionCoordinator coordinator(mem, {}, &ledger, &sampler, &metrics);

    const std::string slow_exit_bundle =
        "export default {\n"
        "  fetch(request) {\n"
        "    const url = new URL(request.url);\n"
        "    const spin = Number(url.searchParams.get('spin') ?? '0');\n"
        "    if (spin > 0) {\n"
        "      const end = Date.now() + spin;\n"
        "      while (Date.now() < end) {}\n"
        "    }\n"
        "    return new Response('ok');\n"
        "  }\n"
        "};\n";

    auto pool_a = make_elastic_pool(worker_path, "retiring-app", "tenant-a", 0, 2, 32, 5000, 1,
                                    &ledger, &coordinator, &metrics, slow_exit_bundle);
    auto pool_b = make_elastic_pool(worker_path, "idle-peer", "tenant-b", 0, 2, 32, 5000, 1,
                                    &ledger, &coordinator, &metrics);

    auto routing = std::make_shared<RoutingTable>();
    routing->publish(RoutingSnapshot::build({{"retiring-app", pool_a}, {"idle-peer", pool_b}}));

    ManagedListenerOptions lopts;
    lopts.config = loopback_path_listener();
    lopts.routing = routing;
    lopts.metrics = &metrics;
    ManagedListener listener(lopts);
    std::string error;
    require(listener.start(&error), "listener start failed: " + error);
    const std::uint16_t port = listener.bound_port();

    // Start one worker in pool_b (idle, 80 MiB < 85 MiB highWatermark).
    require(do_http_get(port, "/@capsid/idle-peer/").status == 200, "idle-peer warmup failed");
    require(pool_b->ready_workers() == 1, "idle-peer did not warm");
    assign_pid_footprints(pool_b, 80ULL * kMiB, &fp_mutex, &pid_bytes);

    // Start a busy worker in active pool_a (40 MiB) spinning on JS execution so it remains
    // alive in pool_a's retired_ list briefly after health-recycle issues shutdown.
    std::thread busy_thread([port] { (void)do_http_get(port, "/@capsid/retiring-app/?spin=800"); });
    require(wait_for([&] { return pool_a->ready_workers() == 1 && pool_a->inflight() == 1; },
                     std::chrono::seconds(4)),
            "retiring-app did not enter busy request");
    assign_pid_footprints(pool_a, 40ULL * kMiB, &fp_mutex, &pid_bytes);
    auto retiring_worker = pool_a->current_worker_snapshot();
    require(retiring_worker != nullptr && pool_a->recycle_worker(retiring_worker.get()),
            "could not recycle active retiring-app worker into retired_");
    require(pool_a->ready_workers() == 0 && pool_a->state() == GenerationPool::State::kActive,
            "pool_a should remain active with its retired worker still alive");
    require(!pool_a->snapshot_workers().empty(),
            "retiring worker exited before soft-watermark cycle could sample it");

    // 80 MiB (pool_b ready) + 40 MiB (pool_a active retired_) = 120 MiB >= 85 MiB highWatermark.
    // A fresh soft-watermark cycle must count the still-alive retired worker and evict pool_b.
    const std::size_t evicted = coordinator.run_eviction_cycle(/*refresh_samples=*/true);
    require(evicted == 1 && pool_b->ready_workers() == 0,
            "active pool retired worker was omitted from soft-watermark eviction");

    busy_thread.join();
    listener.request_stop();
    listener.wait(&error);
    pool_a->stop_and_join();
    pool_b->stop_and_join();
    std::cout << "PASS: active_retiring_worker_counted_in_soft_watermark" << std::endl;
}

void run_on_demand_watermark_counts_previous_victim(const std::string& worker_path) {
    using namespace std::chrono_literals;
    constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
    MemoryBudgetConfig mem;
    mem.configured = true;
    mem.limit_bytes = 100 * kMiB;
    mem.high_watermark_percent = 85;
    mem.low_watermark_percent = 75;
    mem.min_residency_ms = 0;
    mem.check_interval_ms = 0;
    std::mutex mutex;
    std::unordered_map<std::int64_t, std::uint64_t> footprints;
    MemorySampler sampler({}, [&](std::int64_t pid, std::uint64_t* bytes) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto it = footprints.find(pid);
        if (it == footprints.end())
            return false;
        *bytes = it->second;
        return true;
    });
    WorkerCapacityLedger ledger(5, 0);
    MetricsRegistry metrics;
    GlobalEvictionCoordinator coordinator(mem, {}, &ledger, &sampler, &metrics);
    std::atomic<bool> pause_io{false};
    std::promise<void> paused;
    std::promise<void> resume;
    auto gate = resume.get_future();
    auto victim = make_elastic_pool(worker_path, "victim", "team", 0, 1, 0, 0, 1, &ledger,
                                    &coordinator, &metrics, kWorkerBundle, [&] {
                                        if (pause_io.exchange(false)) {
                                            paused.set_value();
                                            gate.wait();
                                        }
                                    });
    auto peer = make_elastic_pool(worker_path, "peer", "team", 0, 1, 0, 0, 1, &ledger, &coordinator,
                                  &metrics);
    auto requester = make_elastic_pool(worker_path, "requester", "team", 0, 1, 0, 0, 1, &ledger,
                                       &coordinator, &metrics);
    for (const auto& pool : {victim, peer}) {
        require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight &&
                    wait_for([&] { return pool->ready_workers() == 1; }, 5s),
                "fixture worker did not become ready");
    }
    auto worker = victim->current_worker_snapshot();
    auto lease = peer->acquire_worker();
    require(worker && lease, "missing victim worker or peer lease");
    assign_pid_footprints(victim, 25 * kMiB, &mutex, &footprints);
    assign_pid_footprints(peer, 65 * kMiB, &mutex, &footprints);

    // Hold the executor at a notification while its child is alive. A probe wakes
    // the I/O thread without taking an HTTP dispatch lease or affecting eligibility.
    pause_io.store(true);
    worker->submit_probe("http://localhost/health");
    require(paused.get_future().wait_for(5s) == std::future_status::ready,
            "victim did not reach I/O barrier");
    std::string reason;
    require(coordinator.acquire_worker_slot(requester.get(), &reason),
            "first admission failed: " + reason);
    ledger.release_elastic("requester");
    const auto retired = victim->snapshot_workers();
    require(victim->ready_workers() == 0 && retired.size() == 1 &&
                retired.front().pid == worker->process_id() && retired.front().pid > 0 &&
                !retired.front().evictable && peer->ready_workers() == 1,
            "first admission did not retire just the 25 MiB victim");

    // A new admission sees 25 + 70 MiB. The retired PID is no longer a
    // candidate, but its sample still puts the host above the 85 MiB watermark.
    lease.release();
    assign_pid_footprints(peer, 70 * kMiB, &mutex, &footprints);
    const bool admitted = coordinator.acquire_worker_slot(requester.get(), &reason);
    const auto peer_ready = peer->ready_workers();
    const auto peer_charge = ledger.elastic_of("peer");
    if (admitted)
        ledger.release_elastic("requester");
    resume.set_value();
    victim->stop_and_join();
    peer->stop_and_join();
    requester->stop_and_join();
    require(admitted, "second admission failed below the 100 MiB hard limit: " + reason);
    require(peer_ready == 0 && peer_charge == 0,
            "second admission omitted the previous victim: peer ready=" +
                std::to_string(peer_ready) + " elastic=" + std::to_string(peer_charge));
}

void run_fair_share_boundary(const std::string& worker_path, bool slot_contention, bool overshare) {
    constexpr std::uint64_t mib = 1024ULL * 1024;
    MemoryBudgetConfig memory;
    memory.configured = true;
    memory.limit_bytes = 100 * mib;
    // The same 90 MiB must stay below the watermark in the slot-contention cases.
    memory.high_watermark_percent = slot_contention ? 95 : 85;
    memory.low_watermark_percent = 75;
    memory.min_residency_ms = 0;
    memory.check_interval_ms = 0;
    std::mutex mutex;
    std::unordered_map<std::int64_t, std::uint64_t> readings;
    MemorySampler sampler({}, [&](std::int64_t pid, std::uint64_t* bytes) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = readings.find(pid);
        if (found == readings.end()) {
            return false;
        }
        *bytes = found->second;
        return true;
    });
    WorkerCapacityLedger ledger(2, 0);
    MetricsRegistry metrics;
    GlobalEvictionCoordinator coordinator(memory, {}, &ledger, &sampler, &metrics);
    require(ledger.reserve_fresh("floor", 1), "reserve the protected floor");
    ledger.commit_fresh("floor");
    auto floor = make_elastic_pool(worker_path, "floor", "a", 1, 1, 0, 0, 1, &ledger, &coordinator,
                                   &metrics);
    auto peer =
        make_elastic_pool(worker_path, "peer", "b", 0, 1, 0, 0, 1, &ledger, &coordinator, &metrics);
    auto requester = make_elastic_pool(worker_path, "requester", "b", 0, 1, 0, 0, 1, &ledger,
                                       &coordinator, &metrics);
    require(peer->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "peer failed to wake");
    require(wait_for([&] { return peer->ready_workers() == 1; }, std::chrono::seconds(5)),
            "peer did not become ready");
    const auto floor_before = floor->snapshot_workers();
    const auto peer_before = peer->snapshot_workers();
    require(floor_before.size() == 1 && peer_before.size() == 1 && peer_before[0].idle &&
                floor_before[0].pid > 0 && peer_before[0].pid > 0 &&
                floor_before[0].pid != peer_before[0].pid && ledger.elastic_used() == 1,
            "expected two resident workers and one occupied elastic slot");
    assign_pid_footprints(floor, (overshare ? 80 : 45) * mib, &mutex, &readings);
    assign_pid_footprints(peer, (overshare ? 10 : 45) * mib, &mutex, &readings);

    bool decision_matches;
    std::string reason;
    if (slot_contention) {
        const auto result = requester->request_scale_out(&reason);
        decision_matches =
            overshare ? result == GenerationPool::ScaleOutResult::kCapacityRejected &&
                            reason == "host_capacity_exhausted" && requester->ready_workers() == 0
                      : result == GenerationPool::ScaleOutResult::kSpawnInFlight &&
                            wait_for([&] { return requester->ready_workers() == 1; },
                                     std::chrono::seconds(5));
    } else {
        decision_matches = coordinator.run_eviction_cycle(true) == (overshare ? 0U : 1U);
    }
    const auto floor_after = floor->snapshot_workers();
    const auto peer_after = peer->snapshot_workers();
    const auto peer_ready = peer->ready_workers();
    const bool floor_preserved = floor->ready_workers() == 1 && floor_after.size() == 1 &&
                                 floor_after[0].pid == floor_before[0].pid;
    const bool peer_preserved =
        peer_ready == 1 && peer_after.size() == 1 && peer_after[0].pid == peer_before[0].pid;
    const auto elastic = ledger.elastic_used();
    const std::string counters = metrics.render_prometheus_text();
    requester->stop_and_join();
    peer->stop_and_join();
    floor->stop_and_join();
    require(floor_preserved, "the original floor worker must survive");
    require(decision_matches && (overshare ? peer_preserved : peer_ready == 0),
            "wrong fairness decision: peer READY=" + std::to_string(peer_ready) +
                ", rejection=" + reason + "\n" + counters);
    require(elastic == (slot_contention || overshare ? 1U : 0U), "incorrect elastic slot count");
    std::istringstream series(counters);
    unsigned nonzero_evictions = 0;
    for (std::string line; std::getline(series, line);) {
        if (line.starts_with("capsid_pool_evictions_total{") && !line.ends_with(" 0")) {
            require(
                line ==
                    "capsid_pool_evictions_total{app=\"peer\",tenant=\"b\",reason=\"global_lru\"} 1",
                "incorrect eviction counter: " + line);
            ++nonzero_evictions;
        }
    }
    require(nonzero_evictions == (overshare ? 0U : 1U), "incorrect eviction counters: " + counters);
}

void run_phase1_overshare_protects_small_tenant(const std::string& worker_path) {
    constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
    MemoryBudgetConfig mem;
    mem.configured = true;
    mem.limit_bytes = 100ULL * kMiB;
    mem.high_watermark_percent = 85;
    mem.low_watermark_percent = 75;
    mem.min_residency_ms = 0;
    mem.check_interval_ms = 0;

    TenancyConfig tenancy;
    tenancy.configured = true;
    tenancy.overrides["tenant-vip"].min_soft_memory_bytes = 50ULL * kMiB;

    std::mutex fp_mutex;
    std::unordered_map<std::int64_t, std::uint64_t> pid_bytes;
    MemorySampler sampler({}, [&](std::int64_t pid, std::uint64_t* bytes_out) {
        std::lock_guard<std::mutex> lock(fp_mutex);
        const auto found = pid_bytes.find(pid);
        if (found == pid_bytes.end()) {
            return false;
        }
        *bytes_out = found->second;
        return true;
    });

    WorkerCapacityLedger ledger(16, 0);
    MetricsRegistry metrics;
    GlobalEvictionCoordinator coordinator(mem, tenancy, &ledger, &sampler, &metrics);

    auto pool_a = make_elastic_pool(worker_path, "app-a", "tenant-a", 0, 4, 32, 5000, 1, &ledger,
                                    &coordinator, &metrics);
    auto pool_b = make_elastic_pool(worker_path, "app-b", "tenant-b", 0, 2, 32, 5000, 1, &ledger,
                                    &coordinator, &metrics);
    auto pool_c = make_elastic_pool(worker_path, "app-c", "tenant-vip", 0, 4, 32, 5000, 1, &ledger,
                                    &coordinator, &metrics);

    auto routing = std::make_shared<RoutingTable>();
    routing->publish(
        RoutingSnapshot::build({{"app-a", pool_a}, {"app-b", pool_b}, {"app-c", pool_c}}));

    ManagedListenerOptions lopts;
    lopts.config = loopback_path_listener();
    lopts.routing = routing;
    lopts.metrics = &metrics;
    ManagedListener listener(lopts);
    std::string error;
    require(listener.start(&error), "listener start failed: " + error);
    const std::uint16_t port = listener.bound_port();

    // Wake tenant-b first (oldest lastActiveAt, 10 MiB <= FairShare 33.3 MiB).
    require(do_http_get(port, "/@capsid/app-b/").status == 200, "app-b request failed");
    require(pool_b->ready_workers() == 1, "app-b did not warm 1 worker");
    assign_pid_footprints(pool_b, 10ULL * kMiB, &fp_mutex, &pid_bytes);

    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // Wake tenant-vip with 2 concurrent workers (19 MiB each = 38 MiB <= VIP minSoftMemory 50 MiB).
    std::thread c1(
        [port] { require(do_http_get(port, "/@capsid/app-c/?delay=700").status == 200, "c1"); });
    require(wait_for([&] { return pool_c->ready_workers() == 1 && pool_c->inflight() == 1; },
                     std::chrono::milliseconds(4000)),
            "app-c slot 0");
    std::thread c2(
        [port] { require(do_http_get(port, "/@capsid/app-c/?delay=700").status == 200, "c2"); });
    c1.join();
    c2.join();
    require(pool_c->ready_workers() == 2, "app-c did not scale to 2 workers");
    assign_pid_footprints(pool_c, 19ULL * kMiB, &fp_mutex, &pid_bytes);

    // Wake tenant-a with 4 concurrent workers (12 MiB each = 48 MiB > FairShare 33.3 MiB).
    // Total across 3 tenants = 10 + 38 + 48 = 96 MiB >= 85 MiB highWatermark.
    std::thread a1(
        [port] { require(do_http_get(port, "/@capsid/app-a/?delay=1100").status == 200, "a1"); });
    require(wait_for([&] { return pool_a->ready_workers() == 1 && pool_a->inflight() == 1; },
                     std::chrono::milliseconds(4000)),
            "app-a slot 0");
    std::thread a2(
        [port] { require(do_http_get(port, "/@capsid/app-a/?delay=1100").status == 200, "a2"); });
    require(wait_for([&] { return pool_a->ready_workers() == 2 && pool_a->inflight() == 2; },
                     std::chrono::milliseconds(4000)),
            "app-a slot 1");
    std::thread a3(
        [port] { require(do_http_get(port, "/@capsid/app-a/?delay=1100").status == 200, "a3"); });
    require(wait_for([&] { return pool_a->ready_workers() == 3 && pool_a->inflight() == 3; },
                     std::chrono::milliseconds(4000)),
            "app-a slot 2");
    std::thread a4(
        [port] { require(do_http_get(port, "/@capsid/app-a/?delay=1100").status == 200, "a4"); });
    a1.join();
    a2.join();
    a3.join();
    a4.join();
    require(pool_a->ready_workers() == 4, "app-a did not scale to 4 workers");
    assign_pid_footprints(pool_a, 12ULL * kMiB, &fp_mutex, &pid_bytes);

    const std::size_t evicted = coordinator.run_eviction_cycle(/*refresh_samples=*/true);
    require(evicted == 2,
            "expected 2 Phase-1 overshare evictions to drop from 96 MiB to 72 MiB <= 75 MiB");
    require(pool_a->ready_workers() == 2, "app-a was not trimmed from 4 to 2 workers");
    require(pool_b->ready_workers() == 1,
            "small tenant-b was unfairly evicted instead of noisy neighbor");
    require(pool_c->ready_workers() == 2,
            "VIP tenant-vip was evicted despite minSoftMemory protection");

    // Hysteresis check: 72 MiB + 12 MiB = 84 MiB (between 75% low and 85% high) must NOT evict.
    require(coordinator.run_eviction_cycle(false) == 0, "eviction ran below highWatermark");
    // An independent admission re-samples retired PIDs. Establish the 72 MiB
    // starting point before measuring hysteresis at 84 MiB.
    require(
        wait_for([&] { return pool_a->snapshot_workers().size() == 2; }, std::chrono::seconds(5)),
        "evicted workers still counted in the 72 MiB fixture");
    std::string reason;
    require(pool_a->request_scale_out(&reason) == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "scale-out to 84 MiB rejected: " + reason);
    require(wait_for([&] { return pool_a->ready_workers() == 3; }, std::chrono::milliseconds(4000)),
            "app-a hysteresis scale-out failed");
    assign_pid_footprints(pool_a, 12ULL * kMiB, &fp_mutex, &pid_bytes);
    require(coordinator.run_eviction_cycle(true) == 0,
            "eviction fired inside 75%-85% hysteresis band");

    const std::string prom = metrics.render_prometheus_text();
    require(
        prom.find(
            "capsid_pool_evictions_total{app=\"app-a\",tenant=\"tenant-a\",reason=\"tenant_overshare\"} 2") !=
            std::string::npos,
        "missing tenant_overshare metric for app-a: " + prom);
    require(prom.find("reason=\"global_lru\"") == std::string::npos,
            "unexpected global_lru eviction during Phase 1");

    listener.request_stop();
    listener.wait(&error);
    pool_a->stop_and_join();
    pool_b->stop_and_join();
    pool_c->stop_and_join();
    std::cout << "PASS: phase1_overshare_protects_small_tenant" << std::endl;
}

void run_phase2_global_lru_and_min_residency(const std::string& worker_path) {
    // 1. Verify MemorySampler dual backend: cgroup primary and zero-syscall procfs cache.
    {
        int cg_calls = 0;
        int fp_calls = 0;
        MemorySampler unit_sampler(
            [&](std::string_view cg, std::uint64_t* out) {
                ++cg_calls;
                if (cg == "/sys/fs/cgroup/w1") {
                    *out = 4096ULL;
                    return true;
                }
                return false;
            },
            [&](std::int64_t pid, std::uint64_t* out) {
                ++fp_calls;
                *out = static_cast<std::uint64_t>(pid) * 1024ULL;
                return true;
            });
        require(unit_sampler.sample_worker_bytes(10, "/sys/fs/cgroup/w1") == 4096ULL &&
                    cg_calls == 1 && fp_calls == 0,
                "cgroup primary backend should bypass footprint reader");
        require(unit_sampler.sample_worker_bytes(22, "") == 22ULL * 1024ULL && fp_calls == 1,
                "initial fallback sample failed");
        require(unit_sampler.sample_worker_bytes(22, "", false) == 22ULL * 1024ULL && fp_calls == 1,
                "cached fallback sample unexpectedly issued OS probe");
        require(unit_sampler.sample_worker_bytes(22, "", true) == 22ULL * 1024ULL && fp_calls == 2,
                "forced refresh did not invoke footprint reader");
    }

    constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
    MemoryBudgetConfig mem;
    mem.configured = true;
    mem.limit_bytes = 100ULL * kMiB;
    mem.high_watermark_percent = 85;
    mem.low_watermark_percent = 75;
    mem.min_residency_ms = 500;
    mem.check_interval_ms = 0;

    TenancyConfig tenancy;
    std::mutex fp_mutex;
    std::unordered_map<std::int64_t, std::uint64_t> pid_bytes;
    MemorySampler sampler({}, [&](std::int64_t pid, std::uint64_t* bytes_out) {
        std::lock_guard<std::mutex> lock(fp_mutex);
        const auto found = pid_bytes.find(pid);
        if (found == pid_bytes.end()) {
            return false;
        }
        *bytes_out = found->second;
        return true;
    });

    WorkerCapacityLedger ledger(8, 0);
    MetricsRegistry metrics;
    GlobalEvictionCoordinator coordinator(mem, tenancy, &ledger, &sampler, &metrics);

    // Three single-worker pools all under dynamic fair share (100 / 3 = 33.3 MiB; each is 30 MiB):
    //   - app1: minReady=1 (protected by minReady floor even when oldest)
    //   - app2: minReady=0 idle (eligible for global_lru once minResidency=500ms elapses)
    //   - app3: minReady=0 busy SSE (protected while streaming)
    require(ledger.reserve_fresh("app1", 1), "reserve app1 failed");
    ledger.commit_fresh("app1");
    auto pool1 = make_elastic_pool(worker_path, "app1", "app1", 1, 1, 16, 5000, 1, &ledger,
                                   &coordinator, &metrics);
    auto pool2 = make_elastic_pool(worker_path, "app2", "app2", 0, 1, 16, 5000, 1, &ledger,
                                   &coordinator, &metrics);
    auto pool3 = make_elastic_pool(worker_path, "app3", "app3", 0, 1, 16, 5000, 1, &ledger,
                                   &coordinator, &metrics);

    auto routing = std::make_shared<RoutingTable>();
    routing->publish(RoutingSnapshot::build({{"app1", pool1}, {"app2", pool2}, {"app3", pool3}}));

    ManagedListenerOptions lopts;
    lopts.config = loopback_path_listener();
    lopts.routing = routing;
    lopts.metrics = &metrics;
    ManagedListener listener(lopts);
    std::string error;
    require(listener.start(&error), "listener start failed: " + error);
    const std::uint16_t port = listener.bound_port();

    require(do_http_get(port, "/@capsid/app2/").status == 200, "app2 wake failed");
    require(pool2->ready_workers() == 1, "app2 did not warm");

    // Start long SSE on app3 so it stays non-idle across minResidency check & eviction.
    std::thread sse_thread([port] {
        const HttpResponse sse = do_http_get(port, "/@capsid/app3/sse?hold=1200");
        require(sse.status == 200 && sse.body.find("data: done") != std::string::npos,
                "app3 SSE failed");
    });
    require(wait_for([&] { return pool3->ready_workers() == 1 && pool3->inflight() == 1; },
                     std::chrono::milliseconds(4000)),
            "app3 SSE never became inflight");

    assign_pid_footprints(pool1, 30ULL * kMiB, &fp_mutex, &pid_bytes);
    assign_pid_footprints(pool2, 30ULL * kMiB, &fp_mutex, &pid_bytes);
    assign_pid_footprints(pool3, 30ULL * kMiB, &fp_mutex, &pid_bytes);

    // Total = 90 MiB >= 85 MiB highWatermark, but app2 is younger than minResidency (500ms).
    require(coordinator.run_eviction_cycle(true) == 0,
            "worker younger than minResidency was prematurely evicted");

    // Wait past minResidency=500ms while app3 is still mid-SSE.
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    const std::size_t evicted = coordinator.run_eviction_cycle(true);
    require(evicted == 1, "expected 1 Phase-2 global_lru eviction after minResidency");
    require(pool1->ready_workers() == 1, "minReady=1 floor on app1 was violated");
    require(pool2->ready_workers() == 0, "idle app2 above minReady should have been evicted");
    require(pool3->ready_workers() == 1, "active SSE stream on app3 was interrupted");

    sse_thread.join();

    const std::string prom = metrics.render_prometheus_text();
    require(
        prom.find(
            "capsid_pool_evictions_total{app=\"app2\",tenant=\"app2\",reason=\"global_lru\"} 1") !=
            std::string::npos,
        "missing global_lru metric: " + prom);

    listener.request_stop();
    listener.wait(&error);
    pool1->stop_and_join();
    pool2->stop_and_join();
    pool3->stop_and_join();
    ledger.release_active("app1", 1);
    std::cout << "PASS: phase2_global_lru_and_min_residency" << std::endl;
}

void run_tenant_quota_swap_and_rejections(const std::string& worker_path) {
    constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
    MemoryBudgetConfig mem;
    mem.configured = true;
    mem.limit_bytes = 500ULL * kMiB;
    mem.high_watermark_percent = 85;
    mem.low_watermark_percent = 75;
    mem.min_residency_ms = 0;
    mem.check_interval_ms = 0;

    TenancyConfig tenancy;
    tenancy.configured = true;
    tenancy.overrides["acme"].max_workers = 2;
    tenancy.overrides["acme"].hard_memory_bytes = 25ULL * kMiB;
    tenancy.overrides["tightmem"].max_workers = 4;
    tenancy.overrides["tightmem"].hard_memory_bytes = 15ULL * kMiB;

    std::mutex fp_mutex;
    std::unordered_map<std::int64_t, std::uint64_t> pid_bytes;
    MemorySampler sampler({}, [&](std::int64_t pid, std::uint64_t* bytes_out) {
        std::lock_guard<std::mutex> lock(fp_mutex);
        const auto found = pid_bytes.find(pid);
        if (found == pid_bytes.end()) {
            return false;
        }
        *bytes_out = found->second;
        return true;
    });

    WorkerCapacityLedger ledger(3, 0);
    MetricsRegistry metrics;
    GlobalEvictionCoordinator coordinator(mem, tenancy, &ledger, &sampler, &metrics);

    require(ledger.reserve_fresh("qdis", 1), "reserve qdis failed");
    ledger.commit_fresh("qdis");
    auto qdis_pool = make_elastic_pool(worker_path, "qdis", "other3", 1, 1, 0, 0, 1, &ledger,
                                       &coordinator, &metrics);
    auto acme_a = make_elastic_pool(worker_path, "acme-a", "acme", 0, 2, 16, 5000, 1, &ledger,
                                    &coordinator, &metrics);
    auto acme_b = make_elastic_pool(worker_path, "acme-b", "acme", 0, 2, 16, 5000, 1, &ledger,
                                    &coordinator, &metrics);
    auto qfull_pool = make_elastic_pool(worker_path, "qfull", "other", 0, 1, 1, 5000, 1, &ledger,
                                        &coordinator, &metrics);
    auto qto_pool = make_elastic_pool(worker_path, "qto", "other2", 0, 1, 2, 80, 1, &ledger,
                                      &coordinator, &metrics);
    auto tight_pool = make_elastic_pool(worker_path, "tight", "tightmem", 0, 2, 16, 5000, 1,
                                        &ledger, &coordinator, &metrics);

    auto routing = std::make_shared<RoutingTable>();
    routing->publish(RoutingSnapshot::build({{"acme-a", acme_a},
                                             {"acme-b", acme_b},
                                             {"qdis", qdis_pool},
                                             {"qfull", qfull_pool},
                                             {"qto", qto_pool},
                                             {"tight", tight_pool}}));

    ManagedListenerOptions lopts;
    lopts.config = loopback_path_listener();
    lopts.routing = routing;
    lopts.metrics = &metrics;
    ManagedListener listener(lopts);
    std::string error;
    require(listener.start(&error), "listener start failed: " + error);
    const std::uint16_t port = listener.bound_port();

    // 1. Scale acme-a to 2 idle workers (= acme.maxWorkers), then send request to acme-b:
    // synchronous tenant_quota_swap evicts one acme-a worker and lets acme-b serve 200.
    std::thread r1(
        [port] { require(do_http_get(port, "/@capsid/acme-a/?delay=700").status == 200, "r1"); });
    require(wait_for([&] { return acme_a->ready_workers() == 1 && acme_a->inflight() == 1; },
                     std::chrono::milliseconds(4000)),
            "acme-a slot 0");
    std::thread r2(
        [port] { require(do_http_get(port, "/@capsid/acme-a/?delay=700").status == 200, "r2"); });
    r1.join();
    r2.join();
    require(acme_a->ready_workers() == 2, "acme-a did not warm 2 workers");
    assign_pid_footprints(acme_a, 10ULL * kMiB, &fp_mutex, &pid_bytes);

    require(do_http_get(port, "/@capsid/acme-b/").status == 200,
            "acme-b did not succeed via tenant_quota_swap");
    require(acme_a->ready_workers() == 1 && acme_b->ready_workers() == 1,
            "tenant_quota_swap did not rebalance sibling pools");

    // 2. Make both acme-a and acme-b busy -> scaling acme-a hits tenant_max_workers;
    // make qdis_pool busy -> 2nd request hits 429 queue_disabled.
    std::thread busy_a([port] {
        require(do_http_get(port, "/@capsid/acme-a/sse?hold=500").status == 200, "busy_a");
    });
    std::thread busy_b([port] {
        require(do_http_get(port, "/@capsid/acme-b/sse?hold=500").status == 200, "busy_b");
    });
    std::thread qdis_busy([port] {
        require(do_http_get(port, "/@capsid/qdis/sse?hold=500").status == 200, "qdis_busy");
    });
    require(wait_for(
                [&] {
                    return acme_a->inflight() == 1 && acme_b->inflight() == 1 &&
                           qdis_pool->inflight() == 1;
                },
                std::chrono::milliseconds(4000)),
            "acme/qdis workers did not enter busy SSE");
    std::string reason;
    require(acme_a->request_scale_out(&reason) ==
                    GenerationPool::ScaleOutResult::kCapacityRejected &&
                reason == "tenant_max_workers",
            "expected tenant_max_workers rejection, got: " + reason);
    const HttpResponse qdis = do_http_get(port, "/@capsid/qdis/");
    require(qdis.status == 429, "expected 429 queue_disabled on qdis");
    busy_a.join();
    busy_b.join();
    qdis_busy.join();
    while (acme_a->evict_idle_worker(0, "global_lru")) {
    }
    while (acme_b->evict_idle_worker(0, "global_lru")) {
    }

    // 3. tenant_hard_memory (tightmem hardMemory = 15 MiB; 1 busy worker = 15 MiB -> 2nd spawn fails).
    std::thread tight_busy([port] {
        require(do_http_get(port, "/@capsid/tight/sse?hold=1000").status == 200, "tight SSE");
    });
    require(
        wait_for([&] { return tight_pool->ready_workers() == 1 && tight_pool->inflight() == 1; },
                 std::chrono::milliseconds(4000)),
        "tight pool did not become busy");
    assign_pid_footprints(tight_pool, 15ULL * kMiB, &fp_mutex, &pid_bytes);
    require(tight_pool->request_scale_out(&reason) ==
                    GenerationPool::ScaleOutResult::kCapacityRejected &&
                reason == "tenant_hard_memory",
            "expected tenant_hard_memory rejection, got: " + reason);

    // 4. host_capacity_exhausted + app_max_workers + queue_full + queue_timeout:
    // tight_pool uses 1 busy global slot; fill the 2nd global slot with qfull_pool (maxWorkers=1).
    std::thread qfull_busy([port] {
        require(do_http_get(port, "/@capsid/qfull/sse?hold=900").status == 200, "qfull busy");
    });
    require(
        wait_for([&] { return qfull_pool->ready_workers() == 1 && qfull_pool->inflight() == 1; },
                 std::chrono::milliseconds(4000)),
        "qfull pool did not become busy");

    // qfull_pool is at maxWorkers=1 (busy) -> app_max_workers.
    require(qfull_pool->request_scale_out(&reason) == GenerationPool::ScaleOutResult::kAtAppMax &&
                reason == "app_max_workers",
            "expected app_max_workers rejection, got: " + reason);

    // qto_pool wants an elastic slot while ledger(2,0) is full of busy workers -> host_capacity_exhausted.
    require(qto_pool->request_scale_out(&reason) ==
                    GenerationPool::ScaleOutResult::kCapacityRejected &&
                reason == "host_capacity_exhausted",
            "expected host_capacity_exhausted rejection, got: " + reason);

    // qfull_pool has queueRequests=1, 1 worker busy: enqueue req A and verify req B overflows -> 429 queue_full.
    std::thread qfull_queued([port] {
        const HttpResponse resp = do_http_get(port, "/@capsid/qfull/");
        require(resp.status == 200, "queued request on qfull failed");
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    const HttpResponse overflow = do_http_get(port, "/@capsid/qfull/");
    require(overflow.status == 429, "qfull did not trigger 429 queue_full");
    qfull_queued.join();
    qfull_busy.join();
    tight_busy.join();

    while (tight_pool->evict_idle_worker(0, "global_lru")) {
    }
    while (qfull_pool->evict_idle_worker(0, "global_lru")) {
    }

    // queue_timeout: qto_pool has 1 busy worker (350ms) and queueTimeout=80ms -> queued gets 504.
    std::thread qto_busy([port] {
        require(do_http_get(port, "/@capsid/qto/sse?hold=350").status == 200, "qto busy");
    });
    require(wait_for([&] { return qto_pool->ready_workers() == 1 && qto_pool->inflight() == 1; },
                     std::chrono::milliseconds(4000)),
            "qto did not start busy SSE");
    // Occupy the remaining global slot so qto doesn't scale further, wait for timeout on qto (already at maxWorkers=1 anyway!).
    const HttpResponse qto_resp = do_http_get(port, "/@capsid/qto/");
    require(qto_resp.status == 504, "expected 504 queue_timeout on qto");
    qto_busy.join();

    const std::string prom = metrics.render_prometheus_text();
    for (
        const std::string& required : {
            std::string(
                "capsid_pool_evictions_total{app=\"acme-a\",tenant=\"acme\",reason=\"tenant_quota_swap\"}"),
            std::string(
                "capsid_pool_rejections_total{app=\"acme-a\",tenant=\"acme\",reason=\"tenant_max_workers\"}"),
            std::string(
                "capsid_pool_rejections_total{app=\"qdis\",tenant=\"other3\",reason=\"queue_disabled\"}"),
            std::string(
                "capsid_pool_rejections_total{app=\"tight\",tenant=\"tightmem\",reason=\"tenant_hard_memory\"}"),
            std::string(
                "capsid_pool_rejections_total{app=\"qfull\",tenant=\"other\",reason=\"app_max_workers\"}"),
            std::string(
                "capsid_pool_rejections_total{app=\"qto\",tenant=\"other2\",reason=\"host_capacity_exhausted\"}"),
            std::string(
                "capsid_pool_rejections_total{app=\"qfull\",tenant=\"other\",reason=\"queue_full\"}"),
            std::string(
                "capsid_pool_rejections_total{app=\"qto\",tenant=\"other2\",reason=\"queue_timeout\"}"),
        }) {
        require(prom.find(required) != std::string::npos,
                "missing metric substring [" + required + "] in:\n" + prom);
    }

    listener.request_stop();
    listener.wait(&error);
    qdis_pool->stop_and_join();
    acme_a->stop_and_join();
    acme_b->stop_and_join();
    qfull_pool->stop_and_join();
    qto_pool->stop_and_join();
    tight_pool->stop_and_join();
    ledger.release_active("qdis", 1);
    std::cout << "PASS: tenant_quota_swap_and_rejections" << std::endl;
}

#if !defined(_WIN32)
std::string create_temp_dir(const char* prefix) {
    std::string tmpl = std::string("/tmp/") + prefix + "-XXXXXX";
    require(mkdtemp(&tmpl[0]) != nullptr, "cannot create temp directory");
    return tmpl;
}

void write_text_file(const std::string& path, const std::string& content) {
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    require(fd >= 0, "cannot create file: " + path);
    require(content.empty() ||
                write(fd, content.data(), content.size()) == static_cast<ssize_t>(content.size()),
            "cannot write file: " + path);
    close(fd);
}

std::string read_text_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
#endif

void run_overlapping_tenant_warmups() {
    TenancyConfig tenancy;
    tenancy.configured = true;
    tenancy.overrides["shared"].max_workers = 1;
    WorkerCapacityLedger ledger(4, 0);
    MemorySampler sampler;
    GlobalEvictionCoordinator coordinator({}, tenancy, &ledger, &sampler);
    std::string error;
    require(coordinator.reserve_deploy_quota("first", "shared", 0, 1, &error),
            "first cold app could not reserve its probe");
    require(!coordinator.reserve_deploy_quota("second", "shared", 0, 1, &error) &&
                error.find("maxWorkers") != std::string::npos,
            "second probe consumed the first warm-up reservation: " + error);
    coordinator.release_deploy_quota("first");
    require(coordinator.reserve_deploy_quota("second", "shared", 0, 1, &error),
            "released probe quota was not reusable");
    coordinator.release_deploy_quota("second");
}

void run_admin_deploy_tenant_and_persistence(const std::string& worker_path) {
#if defined(_WIN32)
    (void)worker_path;
    std::cout << "SKIP: admin_deploy_tenant_and_persistence requires POSIX managed_host"
              << std::endl;
    std::exit(77);
#else
    const std::string apps_root = create_temp_dir("capsid-tenant-apps");
    const std::string state_root = create_temp_dir("capsid-tenant-state");

    for (const auto& [app, min_ready] :
         std::vector<std::pair<std::string, int>>{{"orders", 1}, {"plain", 1}, {"payments", 2}}) {
        require(mkdir((apps_root + "/" + app).c_str(), 0700) == 0, "mkdir app failed");
        const std::string v1 = apps_root + "/" + app + "/v1";
        require(mkdir(v1.c_str(), 0700) == 0, "mkdir v1 failed");
        write_text_file(
            v1 + "/capsid.json",
            "{\"apiVersion\":\"capsid/app-v2\",\"entry\":\"bundle.mjs\",\"pool\":{\"minReady\":" +
                std::to_string(min_ready) + ",\"maxWorkers\":4,\"idleTimeout\":\"60s\"}}");
        write_text_file(v1 + "/bundle.mjs", kWorkerBundle);
    }

    const int apps_fd = open(apps_root.c_str(), O_RDONLY | O_DIRECTORY);
    require(apps_fd >= 0, "open apps_root failed");

    capsid_build_info build_info;
    capsid_build_info_init(&build_info);
    require(capsid_runtime_build_info(&build_info) == CAPSID_OK &&
                build_info.compatibility_id != nullptr,
            "cannot query runtime compatibility id");

    MemoryBudgetConfig mem;
    mem.configured = true;
    mem.limit_bytes = 512ULL * 1024ULL * 1024ULL;
    mem.min_residency_ms = 0;
    TenancyConfig tenancy;
    tenancy.configured = true;
    tenancy.overrides["vip-acme"].max_workers = 2;

    WorkerCapacityLedger ledger(8, 2);
    MemorySampler sampler;
    MetricsRegistry metrics;
    GlobalEvictionCoordinator coordinator(mem, tenancy, &ledger, &sampler, &metrics);

    std::vector<std::shared_ptr<GenerationPool>> retired_pools;
    std::map<std::string, std::shared_ptr<GenerationPool>> active_pools;
    auto make_opts = [&](const std::string& app) {
        auto opts = std::make_unique<ManagedHostOptions>();
        opts->applications_root_fd = apps_fd;
        opts->state_root = state_root;
        opts->application = app;
        opts->worker_path = worker_path;
        opts->runtime_compatibility_id = build_info.compatibility_id;
        opts->host_policy.max_workers = 8;
        opts->host_policy.max_requests_per_worker = 64;
        opts->host_policy.max_worker_memory_bytes = 256ULL * 1024ULL * 1024ULL;
        opts->ledger = &ledger;
        opts->metrics = &metrics;
        opts->reserve_tenant_deploy = [&coordinator, app](const std::string& tenant,
                                                          std::uint32_t floor,
                                                          std::uint32_t warming, std::string* err) {
            return coordinator.reserve_deploy_quota(app, tenant, floor, warming, err);
        };
        opts->release_tenant_deploy = [&coordinator, app] {
            coordinator.release_deploy_quota(app);
        };
        opts->prepare_activation =
            [&](const std::string& app_id, const capsid::host::DeployOutcome& prepared,
                std::string* err) -> std::unique_ptr<capsid::host::ActivationPlan> {
            GenerationPoolOptions popts;
            popts.application_id = app_id;
            popts.tenant_id = prepared.tenant;
            popts.version = prepared.version;
            popts.generation_digest = prepared.generation_digest;
            popts.workers = prepared.effective.workers;
            popts.has_min_ready = true;
            popts.min_ready = prepared.effective.min_ready;
            popts.factory = prepared.generation_factory;
            popts.recovery = default_recovery_policy();
            popts.metrics = &metrics;
            auto draining = std::make_shared<std::weak_ptr<GenerationPool>>();
            popts.on_drain_complete = [&coordinator, draining] {
                if (auto pool = draining->lock())
                    coordinator.unregister_pool(pool.get());
            };
            auto pool = GenerationPool::create_adopted(std::move(popts), prepared.workers, err);
            if (!pool) {
                return nullptr;
            }
            *draining = pool;
            auto plan = std::make_unique<capsid::host::ActivationPlan>();
            plan->application = app_id;
            plan->new_pool = std::move(pool);
            return plan;
        };
        opts->commit_activation = [&](capsid::host::ActivationPlan* plan) {
            active_pools[plan->application] = plan->new_pool;
            coordinator.register_pool(plan->new_pool);
        };
        opts->abort_activation = [&](capsid::host::ActivationPlan* plan) { (void)plan; };
        opts->prepare_retire = [&](const std::string& app_id,
                                   std::string* err) -> std::unique_ptr<capsid::host::RetirePlan> {
            (void)err;
            auto plan = std::make_unique<capsid::host::RetirePlan>();
            plan->application = app_id;
            if (const auto it = active_pools.find(app_id); it != active_pools.end()) {
                plan->pool = it->second;
            }
            return plan;
        };
        opts->commit_retire = [&](capsid::host::RetirePlan* plan) {
            if (plan->pool) {
                retired_pools.push_back(plan->pool);
                plan->pool->request_drain();
            }
            active_pools.erase(plan->application);
        };
        opts->abort_retire = [&](capsid::host::RetirePlan* plan) { (void)plan; };
        return opts;
    };

    auto opts_orders = make_opts("orders");
    auto opts_plain = make_opts("plain");
    auto opts_payments = make_opts("payments");
    std::vector<ManagedHostOptions*> all_opts = {opts_orders.get(), opts_plain.get(),
                                                 opts_payments.get()};
    capsid::host::ManagedAdminBackend backend(all_opts);

    capsid::host::AdminApiOptions api_opts;
    api_opts.authorization.allowed_uid = static_cast<std::uint64_t>(geteuid());
    api_opts.max_header_bytes = 4096;
    api_opts.max_body_bytes = 4096;
    capsid::host::AdminPeerCredentials peer;
    peer.uid = static_cast<std::uint64_t>(geteuid());
    peer.gid = static_cast<std::uint64_t>(getegid());

    // 1. Deploy orders with explicit tenant "vip-acme".
    capsid::host::AdminRequest deploy_orders;
    deploy_orders.method = "POST";
    deploy_orders.target = "/v1/deploy";
    deploy_orders.content_type = "application/json";
    deploy_orders.body = R"({"app":"orders","version":"v1","tenant":"vip-acme"})";
    deploy_orders.header_bytes = 128;
    const capsid::host::AdminResponse resp_orders =
        capsid::host::handle_admin_request(api_opts, peer, deploy_orders, &backend);
    require(resp_orders.status == 202, "Admin deploy orders failed: " + resp_orders.body);

    const std::string orders_active_json = read_text_file(state_root + "/apps/orders/active.json");
    require(orders_active_json.find("\"tenant\":\"vip-acme\"") != std::string::npos,
            "active.json did not persist tenant: " + orders_active_json);

    capsid::host::AdminRequest status_orders;
    status_orders.method = "GET";
    status_orders.target = "/v1/apps/orders";
    status_orders.header_bytes = 64;
    const capsid::host::AdminResponse get_orders =
        capsid::host::handle_admin_request(api_opts, peer, status_orders, &backend);
    require(get_orders.status == 200 &&
                get_orders.body.find("\"tenant\":\"vip-acme\"") != std::string::npos,
            "GET /v1/apps/orders omitted tenant: " + get_orders.body);

    // 2. Deploy plain without tenant: omits "tenant" from active.json and defaults recovery to "plain".
    capsid::host::AdminRequest deploy_plain;
    deploy_plain.method = "POST";
    deploy_plain.target = "/v1/deploy";
    deploy_plain.content_type = "application/json";
    deploy_plain.body = R"({"app":"plain","version":"v1"})";
    deploy_plain.header_bytes = 128;
    require(capsid::host::handle_admin_request(api_opts, peer, deploy_plain, &backend).status ==
                202,
            "Admin deploy plain failed");
    const std::string plain_active_json = read_text_file(state_root + "/apps/plain/active.json");
    require(plain_active_json.find("\"tenant\"") == std::string::npos,
            "omitted tenant polluted v1 active.json: " + plain_active_json);

    // 3. Tenant maxWorkers deploy gate: vip-acme allows 2, orders already holds 1, payments wants 2 -> fail!
    capsid::host::OperationStatus pay_status;
    const capsid::host::DeployOutcome pay_outcome =
        capsid::host::managed_deploy(opts_payments.get(), "v1", "vip-acme", &pay_status);
    require(!pay_outcome.ok && pay_status.state == capsid::host::OperationState::kFailed &&
                pay_outcome.error.find("maxWorkers") != std::string::npos,
            "tenant maxWorkers deploy gate did not reject over-committed deploy: " +
                pay_outcome.error);
    require(
        metrics.render_prometheus_text().find(
            "capsid_pool_rejections_total{app=\"payments\",tenant=\"vip-acme\",reason=\"tenant_max_workers\"} 1") !=
            std::string::npos,
        "missing tenant_max_workers deploy rejection metric");

    // 4. Recovery preserves tenanted and default-tenant identities cleanly (use a fresh ledger).
    WorkerCapacityLedger recover_ledger(8, 2);
    opts_orders->ledger = &recover_ledger;
    opts_plain->ledger = &recover_ledger;
    capsid::host::OperationStatus rec_orders_status;
    const capsid::host::DeployOutcome rec_orders =
        capsid::host::managed_recover(opts_orders.get(), &rec_orders_status);
    require(rec_orders.ok && rec_orders.tenant == "vip-acme", "managed_recover lost custom tenant");
    for (capsid_worker* w : rec_orders.workers) {
        capsid_worker_destroy(w);
    }
    capsid::host::OperationStatus rec_plain_status;
    const capsid::host::DeployOutcome rec_plain =
        capsid::host::managed_recover(opts_plain.get(), &rec_plain_status);
    require(rec_plain.ok && rec_plain.tenant == "plain",
            "managed_recover did not default omitted tenant to app");
    for (capsid_worker* w : rec_plain.workers) {
        capsid_worker_destroy(w);
    }

    // 5. Retire orders preserves tenant on tombstone.
    opts_orders->ledger = &ledger;
    capsid::host::AdminRequest retire_orders;
    retire_orders.method = "POST";
    retire_orders.target = "/v1/apps/orders/retire";
    retire_orders.header_bytes = 64;
    require(capsid::host::handle_admin_request(api_opts, peer, retire_orders, &backend).status ==
                202,
            "Admin retire orders failed");
    const std::string retired_json = read_text_file(state_root + "/apps/orders/active.json");
    require(retired_json.find("\"state\":\"retired\"") != std::string::npos &&
                retired_json.find("\"tenant\":\"vip-acme\"") != std::string::npos,
            "retired tombstone did not preserve tenant: " + retired_json);

    for (auto& [name, pool] : active_pools) {
        (void)name;
        if (pool) {
            pool->stop_and_join();
        }
    }
    for (const auto& pool : retired_pools)
        pool->stop_and_join();
    close(apps_fd);
    std::cout << "PASS: admin_deploy_tenant_and_persistence" << std::endl;
#endif
}

void run_real_cgroup_memory_current(const std::string& worker_path) {
#if !defined(__linux__)
    (void)worker_path;
    std::cout << "SKIP: real_cgroup_memory_current requires Linux cgroup v2" << std::endl;
    std::exit(77);
#else
    // Use the delegated root shared by the native and container sandbox probes.
    // Only the workers created by this test may be moved to a new cgroup.
    const std::string workers_cg =
        "/sys/fs/cgroup/capsid-cgtest-workers-" + std::to_string(getpid());
    if (mkdir(workers_cg.c_str(), 0755) != 0) {
        if (errno == EACCES || errno == EPERM || errno == EROFS || errno == ENOENT) {
            std::cout << "SKIP: cgroup v2 delegation unavailable (" << std::strerror(errno) << ")"
                      << std::endl;
            std::exit(77);
        }
        fail(std::string("cannot create worker cgroup: ") + std::strerror(errno));
    }
    std::ifstream controllers(workers_cg + "/cgroup.controllers");
    std::string name;
    bool memory_delegated = false;
    while (controllers >> name) {
        if (name == "memory")
            memory_delegated = true;
    }
    if (!memory_delegated) {
        require(rmdir(workers_cg.c_str()) == 0, "cannot remove empty test cgroup");
        std::cout << "SKIP: memory controller is not delegated to " << workers_cg << std::endl;
        std::exit(77);
    }
    std::ofstream control(workers_cg + "/cgroup.subtree_control");
    control << "+memory\n";
    control.close();
    require(!control.fail(), "cannot enable delegated memory controller at " + workers_cg);

    // Install nullptr CgroupReader (real <cgroup>/memory.current) and a trap FootprintReader
    // so any accidental fallback to /proc/<pid> fails the test immediately.
    MemorySampler sampler(nullptr, [](std::int64_t, std::uint64_t*) -> bool {
        fail("MemorySampler fell back to /proc PID footprint despite active cgroup v2");
        return false;
    });

    WorkerCapacityLedger ledger(4, 0);
    TenancyConfig tenancy;
    MemoryBudgetConfig setup_mem;
    setup_mem.configured = false;
    setup_mem.min_residency_ms = 0;

    GlobalEvictionCoordinator setup_coord(setup_mem, tenancy, &ledger, &sampler);
    require(ledger.reserve_fresh("cgapp", 1), "reserve cgapp failed");
    ledger.commit_fresh("cgapp");

    GenerationPoolOptions popts;
    popts.application_id = "cgapp";
    popts.tenant_id = "cgtenant";
    popts.version = "v1";
    popts.generation_digest = "sha256:" + std::string(64, 'c');
    popts.workers = 2;
    popts.has_min_ready = true;
    popts.min_ready = 1;
    popts.idle_timeout_ms = 60000;
    popts.max_inflight_per_worker = 1;
    popts.max_queued_requests = 16;
    popts.queue_timeout_ms = 5000;
    popts.recovery = default_recovery_policy();
    const std::string bundle_str(kWorkerBundle);
    popts.factory = [worker_path, workers_cg, bundle_str](capsid_worker** out,
                                                          std::string* factory_error) -> bool {
        std::string cg_err;
        auto lease = capsid::host::create_worker_cgroup(workers_cg, &cg_err);
        if (!lease) {
            *factory_error = cg_err;
            return false;
        }
        capsid_worker_config config;
        capsid_worker_config_init(&config);
        config.worker_path = worker_path.c_str();
        config.strict_sandbox = 1;
        config.sandbox_cgroup_path = lease->path.c_str();
        config.request_timeout_ms = 5000;
        capsid_worker* worker = nullptr;
        const capsid::host::ProtocolLaunch launch;
        if (capsid::host::spawn_with_protocol(&config, launch, &worker) != CAPSID_OK) {
            *factory_error = "worker spawn with cgroup failed";
            return false;
        }
        if (capsid_worker_load_bundle(worker,
                                      reinterpret_cast<const std::uint8_t*>(bundle_str.data()),
                                      bundle_str.size()) != CAPSID_OK ||
            capsid_worker_flush(worker) != CAPSID_OK) {
            capsid_worker_destroy(worker);
            *factory_error = "bundle load/flush failed";
            return false;
        }
        capsid::host::register_worker_cgroup(worker, lease);
        *out = worker;
        return true;
    };
    popts.acquire_worker_slot = [&setup_coord](const GenerationPool* req, std::string* reason) {
        return setup_coord.acquire_worker_slot(req, reason);
    };
    popts.release_elastic_slot = [&ledger] { ledger.release_elastic("cgapp", 1); };
    std::string error;
    auto pool = GenerationPool::create(std::move(popts), &error);
    require(pool != nullptr, "GenerationPool::create with cgroup v2 failed: " + error);
    setup_coord.register_pool(pool);

    auto routing = std::make_shared<RoutingTable>();
    routing->publish(RoutingSnapshot::build({{"cgapp", pool}}));
    ManagedListenerOptions lopts;
    lopts.config = loopback_path_listener();
    lopts.routing = routing;
    ManagedListener listener(lopts);
    require(listener.start(&error), "listener start failed: " + error);
    const std::uint16_t port = listener.bound_port();

    std::thread req1([port] {
        const HttpResponse r = do_http_get(port, "/@capsid/cgapp/?delay=250");
        require(r.status == 200, "cgapp req1 failed");
    });
    std::thread req2([port] {
        const HttpResponse r = do_http_get(port, "/@capsid/cgapp/?delay=250");
        require(r.status == 200, "cgapp req2 failed");
    });
    req1.join();
    req2.join();
    require(wait_for([&] { return pool->ready_workers() == 2 && pool->inflight() == 0; },
                     std::chrono::seconds(5)),
            "cgapp did not scale out to 2 idle workers");

    const auto snaps = pool->snapshot_workers();
    require(snaps.size() == 2, "expected 2 worker snapshots");
    std::uint64_t total_cgroup_bytes = 0;
    for (const auto& w : snaps) {
        require(!w.cgroup_path.empty() && w.cgroup_path.rfind(workers_cg + "/", 0) == 0,
                "worker snapshot missing leaf cgroup path: " + w.cgroup_path);
        const std::uint64_t b = sampler.sample_worker_bytes(w.pid, w.cgroup_path, true);
        require(b > 0, "real kernel cgroup memory.current returned 0");
        total_cgroup_bytes += b;
    }

    MemoryBudgetConfig budget;
    budget.configured = true;
    budget.limit_bytes = total_cgroup_bytes;
    budget.high_watermark_percent = 80;
    budget.low_watermark_percent = 50;
    budget.min_residency_ms = 0;
    GlobalEvictionCoordinator coordinator(budget, tenancy, &ledger, &sampler);
    coordinator.register_pool(pool);
    const std::size_t evicted = coordinator.run_eviction_cycle(/*refresh_samples=*/true);
    require(evicted == 1 && pool->ready_workers() == 1,
            "real cgroup eviction did not evict exactly 1 worker (evicted=" +
                std::to_string(evicted) + ")");
    require(wait_for([&] { return ledger.elastic_used() == 0; }, std::chrono::seconds(5)),
            "elastic slot not returned after cgroup eviction");

    listener.request_stop();
    listener.wait(&error);
    pool->stop_and_join();
    ledger.release_active("cgapp", 1);
    require(wait_for([&] { return rmdir(workers_cg.c_str()) == 0 || errno == ENOENT; },
                     std::chrono::seconds(3)),
            "worker leaf cgroups did not clean up cleanly");
    std::cout << "PASS: real_cgroup_memory_current (" << total_cgroup_bytes << " bytes)"
              << std::endl;
#endif
}

void run_three_surface_reason_parity(const std::string& worker_path) {
    // C1: verify eviction and rejection reasons are recorded simultaneously
    // across all three operational surfaces: /metrics, Admin app status
    // diagnostics (MetricsRegistry::app_pool_diagnostics), and StructuredLog.
    constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
    MemoryBudgetConfig mem;
    mem.configured = true;
    mem.limit_bytes = 100ULL * kMiB;
    mem.high_watermark_percent = 80;
    mem.low_watermark_percent = 50;
    mem.min_residency_ms = 0;
    mem.check_interval_ms = 0;

    TenancyConfig tenancy;
    tenancy.configured = true;
    tenancy.overrides["tenant-b"].max_workers = 1;

    std::mutex fp_mutex;
    std::unordered_map<std::int64_t, std::uint64_t> pid_bytes;
    MemorySampler sampler({}, [&](std::int64_t pid, std::uint64_t* bytes_out) {
        std::lock_guard<std::mutex> lock(fp_mutex);
        const auto found = pid_bytes.find(pid);
        if (found == pid_bytes.end()) {
            return false;
        }
        *bytes_out = found->second;
        return true;
    });

    std::mutex log_mutex;
    std::vector<std::string> log_lines;
    capsid::host::StructuredLog structured_log([&](const std::string& line) {
        std::lock_guard<std::mutex> lock(log_mutex);
        log_lines.push_back(line);
    });

    WorkerCapacityLedger ledger(4, 0);
    MetricsRegistry metrics;
    GlobalEvictionCoordinator coordinator(mem, tenancy, &ledger, &sampler, &metrics,
                                          &structured_log);

    auto make_logged_pool = [&](const std::string& app, const std::string& tenant,
                                std::uint32_t max_workers) {
        GenerationPoolOptions opts;
        opts.application_id = app;
        opts.tenant_id = tenant;
        opts.version = "v1";
        opts.generation_digest = "sha256:" + std::string(64, app.empty() ? 'x' : app.back());
        opts.workers = max_workers;
        opts.has_min_ready = true;
        opts.min_ready = 0;
        opts.idle_timeout_ms = 60000;
        opts.max_inflight_per_worker = 1;
        opts.max_queued_requests = 0;
        opts.queue_timeout_ms = 5000;
        opts.metrics = &metrics;
        opts.log = &structured_log;
        opts.factory = spawn_factory(worker_path, 5000, kWorkerBundle);
        opts.recovery = default_recovery_policy();
        opts.acquire_worker_slot = [&coordinator](const GenerationPool* req, std::string* reason) {
            return coordinator.acquire_worker_slot(req, reason);
        };
        opts.release_elastic_slot = [&ledger, app] { ledger.release_elastic(app, 1); };
        std::string err;
        auto pool = GenerationPool::create(std::move(opts), &err);
        require(pool != nullptr, "failed to create pool " + app + ": " + err);
        coordinator.register_pool(pool);
        return pool;
    };

    auto pool_a = make_logged_pool("app-a", "tenant-a", 2);
    auto pool_b = make_logged_pool("app-b", "tenant-b", 2);

    // Warm 1 worker in app-a and 1 worker in app-b.
    require(pool_a->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight &&
                wait_for([&] { return pool_a->ready_workers() == 1; }, std::chrono::seconds(5)),
            "app-a did not warm");
    require(pool_b->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight &&
                wait_for([&] { return pool_b->ready_workers() == 1; }, std::chrono::seconds(5)),
            "app-b did not warm");

    // Trigger tenant_max_workers rejection on app-b (maxWorkers=1 already occupied
    // by a busy worker so tenant_quota_swap cannot evict it).
    std::string reject_reason;
    {
        auto busy_b = pool_b->acquire_worker();
        require(static_cast<bool>(busy_b), "failed to acquire busy lease on app-b");
        require(pool_b->request_scale_out(&reject_reason) ==
                        GenerationPool::ScaleOutResult::kCapacityRejected &&
                    reject_reason == "tenant_max_workers",
                "expected tenant_max_workers rejection on app-b, got: " + reject_reason);
    }

    // Push app-a above watermark and trigger global_lru eviction.
    assign_pid_footprints(pool_a, 45ULL * kMiB, &fp_mutex, &pid_bytes);
    assign_pid_footprints(pool_b, 45ULL * kMiB, &fp_mutex, &pid_bytes);
    require(coordinator.run_eviction_cycle(/*refresh_samples=*/true) == 1 &&
                pool_a->ready_workers() == 0,
            "expected global_lru eviction of app-a");

    pool_a->stop_and_join();
    pool_b->stop_and_join();
    structured_log.flush();

    // Surface 1: /metrics
    const std::string prom = metrics.render_prometheus_text();
    require(prom.find("capsid_pool_evictions_total{app=\"app-a\",tenant=\"tenant-a\","
                      "reason=\"global_lru\"} 1") != std::string::npos,
            "/metrics missing app-a global_lru eviction:\n" + prom);
    require(prom.find("capsid_pool_rejections_total{app=\"app-b\",tenant=\"tenant-b\","
                      "reason=\"tenant_max_workers\"} 1") != std::string::npos,
            "/metrics missing app-b tenant_max_workers rejection:\n" + prom);

    // Surface 2: Admin App status diagnostics
    const auto diag_a = metrics.app_pool_diagnostics("app-a");
    const auto diag_b = metrics.app_pool_diagnostics("app-b");
    require(diag_a.evictions.count("global_lru") == 1 && diag_a.evictions.at("global_lru") == 1 &&
                diag_a.last_eviction_reason == "global_lru",
            "Admin diagnostics missing app-a global_lru eviction");
    require(diag_b.rejections.count("tenant_max_workers") == 1 &&
                diag_b.rejections.at("tenant_max_workers") == 1 &&
                diag_b.last_rejection_reason == "tenant_max_workers",
            "Admin diagnostics missing app-b tenant_max_workers rejection");

    // Surface 3: StructuredLog JSONL
    bool logged_eviction = false;
    bool logged_rejection = false;
    for (const std::string& line : log_lines) {
        if (line.find("\"event\":\"pool_eviction\"") != std::string::npos &&
            line.find("\"app\":\"app-a\"") != std::string::npos &&
            line.find("\"tenant\":\"tenant-a\"") != std::string::npos &&
            line.find("\"reason\":\"global_lru\"") != std::string::npos) {
            logged_eviction = true;
        }
        if (line.find("\"event\":\"pool_rejection\"") != std::string::npos &&
            line.find("\"app\":\"app-b\"") != std::string::npos &&
            line.find("\"tenant\":\"tenant-b\"") != std::string::npos &&
            line.find("\"reason\":\"tenant_max_workers\"") != std::string::npos) {
            logged_rejection = true;
        }
    }
    require(logged_eviction, "StructuredLog missing canonical pool_eviction entry");
    require(logged_rejection, "StructuredLog missing canonical pool_rejection entry");
    std::cout << "PASS: three_surface_reason_parity" << std::endl;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        fail("usage: test-host-eviction-coordinator <capsid-worker> <case>");
    }
#if defined(_WIN32)
    require(capsid::win32::ensure_winsock(), "cannot initialize Winsock");
#endif
    const std::string worker_path = argv[1];
    const std::string mode = argv[2];
    if (mode == "watermark_protected_overshare" || mode == "watermark_within_share" ||
        mode == "slot_protected_overshare" || mode == "slot_within_share") {
        run_fair_share_boundary(worker_path, mode.starts_with("slot_"),
                                mode.ends_with("_overshare"));
        return 0;
    }
    if (mode == "on_demand_watermark_counts_previous_victim") {
        run_on_demand_watermark_counts_previous_victim(worker_path);
        return 0;
    }
    if (mode == "deploy_with_protected_sibling") {
        run_deploy_with_protected_sibling(worker_path);
        return 0;
    }
    if (mode == "deploy_at_tenant_memory_limit") {
        run_deploy_at_tenant_memory_limit(worker_path);
        return 0;
    }
    if (mode == "tenant_memory_recovers_without_global_budget") {
        run_tenant_memory_recovers_without_global_budget(worker_path);
        return 0;
    }
    if (mode == "shrink_to_floor_returns_elastic_slot") {
        run_shrink_to_floor_returns_elastic_slot(worker_path);
        return 0;
    }
    if (mode == "draining_generation_counted_in_memory") {
        run_draining_generation_counted_in_memory(worker_path);
        return 0;
    }
    if (mode == "active_retiring_worker_counted_in_soft_watermark") {
        run_active_retiring_worker_counted_in_soft_watermark(worker_path);
        return 0;
    }
    if (mode == "phase1_overshare_protects_small_tenant") {
        run_phase1_overshare_protects_small_tenant(worker_path);
        return 0;
    }
    if (mode == "phase2_global_lru_and_min_residency") {
        run_phase2_global_lru_and_min_residency(worker_path);
        return 0;
    }
    if (mode == "tenant_quota_swap_and_rejections") {
        run_tenant_quota_swap_and_rejections(worker_path);
        return 0;
    }
    if (mode == "admin_deploy_tenant_and_persistence") {
        run_admin_deploy_tenant_and_persistence(worker_path);
        return 0;
    }
    if (mode == "overlapping_tenant_warmups") {
        run_overlapping_tenant_warmups();
        return 0;
    }
    if (mode == "real_cgroup_memory_current") {
        run_real_cgroup_memory_current(worker_path);
        return 0;
    }
    if (mode == "three_surface_reason_parity") {
        run_three_surface_reason_parity(worker_path);
        return 0;
    }
    fail("unknown eviction coordinator test case: " + mode);
}
