// Stage 2 Elastic Worker Pool & Scale-to-Zero Integration Tests:
//   - scale_to_zero_burst_and_reclaim:
//       Starts with 0 workers (minReady=0), burst of 16 requests wakes pool &
//       queues cleanly without 503, WorkerSupervisor probes run during active
//       lifetime without resetting lastActiveAt, idle workers scale back to 0.
//   - active_stream_and_ws_protected:
//       Workers serving an active SSE stream or open WebSocket connection are
//       never idle-evicted; once clients disconnect/complete, idleTimeout runs.
//   - two_tier_reaping_slot_transfer:
//       Two apps sharing global workersTotal via WorkerCapacityLedger:
//       evicting an idle slot immediately frees elastic quota before reap
//       Wait completes, allowing immediate peer scale-out.
//   - disk_backed_generation_tamper_fails (POSIX managed_host):
//       Deploy with minReady=0 warms 1 probe worker, commits generation to disk,
//       and tampering with bundle.bin causes replacement/scale-out spawn to fail
//       closed on SHA-256 digest mismatch.

#include "win32_compat.h"
#include "host/channel_ledger.h"
#include "host/generation_pool.h"
#include "host/host_config_model.h"
#include "host/managed_admin_backend.h"
#include "host/managed_host.h"
#include "host/managed_listener.h"
#include "host/protocol_launch.h"
#include "host/routing_snapshot.h"
#include "host/worker_capacity_ledger.h"
#include "host/worker_supervisor.h"

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
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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
using capsid::host::HealthCheckConfig;
using capsid::host::ListenerConfig;
using capsid::host::ManagedHostOptions;
using capsid::host::ManagedListener;
using capsid::host::ManagedListenerOptions;
using capsid::host::RoutingSnapshot;
using capsid::host::RoutingTable;
using capsid::host::WorkerCapacityLedger;
using capsid::host::WorkerExecutor;
using capsid::host::WorkerRecoveryPolicy;
using capsid::host::WorkerSupervisor;
using capsid::host::WorkerSupervisorOptions;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test-host-elastic-pool: FAIL: " << message << std::endl;
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

const char* kBurstBundle = "export default {\n"
                           "  async fetch(request) {\n"
                           "    const url = new URL(request.url);\n"
                           "    if (url.pathname === '/_health') return new Response('ok');\n"
                           "    const delay = Number(url.searchParams.get('delay') ?? '0');\n"
                           "    if (delay > 0) await new Promise((r) => setTimeout(r, delay));\n"
                           "    return new Response('elastic-ok');\n"
                           "  }\n"
                           "};\n";

const char* kStreamAndWsBundle =
    "export default {\n"
    "  async fetch(request) {\n"
    "    if ((request.headers.get('upgrade') || '').toLowerCase() === 'websocket') {\n"
    "      const pair = new WebSocketPair();\n"
    "      const [client, server] = Object.values(pair);\n"
    "      server.accept();\n"
    "      server.addEventListener('message', (e) => server.send(e.data));\n"
    "      server.addEventListener('close', () => server.close());\n"
    "      return new Response(null, { status: 101, webSocket: client });\n"
    "    }\n"
    "    const url = new URL(request.url);\n"
    "    const hold = Number(url.searchParams.get('hold') ?? '0');\n"
    "    const enc = new TextEncoder();\n"
    "    const body = new ReadableStream({\n"
    "      async start(controller) {\n"
    "        controller.enqueue(enc.encode('data: open\\n\\n'));\n"
    "        if (hold > 0) await new Promise((r) => setTimeout(r, hold));\n"
    "        controller.enqueue(enc.encode('data: done\\n\\n'));\n"
    "        controller.close();\n"
    "      }\n"
    "    });\n"
    "    return new Response(body, { headers: { 'Content-Type': 'text/event-stream' } });\n"
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
    config.name = "elastic-test";
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
    const std::string::size_type code_start = wire.find(' ');
    if (code_start != std::string::npos) {
        response.status = std::atoi(wire.c_str() + code_start + 1);
    }
    const std::string::size_type head_end = wire.find("\r\n\r\n");
    if (head_end != std::string::npos) {
        response.head = wire.substr(0, head_end);
        response.body = chunked ? chunked_body : wire.substr(head_end + 4);
    }
    return response;
}

HttpResponse do_http_get(std::uint16_t port, const std::string& target) {
    const SocketHandle fd = connect_listener(port);
    const std::string req =
        "GET " + target + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    send_raw_request(fd, req);
    HttpResponse resp = read_http_response(fd);
    close_socket_handle(fd);
    return resp;
}

void run_scale_to_zero_burst_and_reclaim(const std::string& worker_path) {
    WorkerCapacityLedger ledger(4, 2);
    require(ledger.reserve_fresh("elastic-app", 0), "minReady=0 ledger reserve failed");
    ledger.commit_fresh("elastic-app");

    std::atomic<int> idle_evictions{0};
    GenerationPoolOptions pool_opts;
    pool_opts.application_id = "elastic-app";
    pool_opts.version = "v1";
    pool_opts.generation_digest = "sha256:" + std::string(64, 'a');
    pool_opts.workers = 4;
    pool_opts.has_min_ready = true;
    pool_opts.min_ready = 0;
    pool_opts.idle_timeout_ms = 250;
    pool_opts.acquire_elastic_slot = [&ledger] {
        return ledger.try_acquire_elastic("elastic-app");
    };
    pool_opts.release_elastic_slot = [&ledger] { ledger.release_elastic("elastic-app", 1); };
    pool_opts.on_eviction = [&idle_evictions](const std::string& reason) {
        if (reason == "idle_timeout") {
            idle_evictions.fetch_add(1, std::memory_order_relaxed);
        }
    };
    pool_opts.max_inflight_per_worker = 2;
    pool_opts.max_queued_requests = 64;
    pool_opts.queue_timeout_ms = 5000;
    pool_opts.factory = spawn_factory(worker_path, 5000, kBurstBundle);
    pool_opts.recovery = default_recovery_policy();

    std::string error;
    std::shared_ptr<GenerationPool> pool = GenerationPool::create(pool_opts, &error);
    require(pool != nullptr, "elastic pool create failed: " + error);
    require(pool->ready_workers() == 0 && pool->worker_process_id() == 0,
            "scale-to-zero pool did not start with 0 workers");

    auto routing = std::make_shared<RoutingTable>();
    routing->publish(RoutingSnapshot::build({{"elastic-app", pool}}));

    ManagedListenerOptions listener_opts;
    listener_opts.config = loopback_path_listener();
    listener_opts.routing = routing;
    ManagedListener listener(listener_opts);
    require(listener.start(&error), "listener start failed: " + error);

    // Wire WorkerSupervisor with frequent health probes (60ms) to prove:
    //   1) sleeping 0-worker pool stays at 0 without waking or quarantine;
    //   2) active health probes do NOT reset lastActiveAt and never keep idle
    //      workers alive past idleTimeout (250ms).
    std::atomic<bool> stop_supervisor{false};
    ManagedHostOptions supervisor_target;
    supervisor_target.application = "elastic-app";
    WorkerSupervisorOptions sup_opts;
    sup_opts.managed_options = &supervisor_target;
    sup_opts.policy = default_recovery_policy();
    sup_opts.current_pool = [pool] { return pool; };
    sup_opts.health_check_reader = [](const std::string&) {
        HealthCheckConfig cfg;
        cfg.configured = true;
        cfg.path = "/_health";
        cfg.timeout_ms = 1000;
        return cfg;
    };
    sup_opts.stop_requested = &stop_supervisor;
    sup_opts.active_health_interval_ms = 60;
    sup_opts.active_health_failures = 1;
    WorkerSupervisor supervisor(std::move(sup_opts));

    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    require(pool->ready_workers() == 0 && pool->state() == GenerationPool::State::kActive,
            "supervisor disturbed sleeping 0-worker pool");

    // Burst 16 concurrent HTTP requests against cold 0-worker pool.
    constexpr int kBurstCount = 16;
    std::atomic<int> ok_count{0};
    std::vector<std::thread> clients;
    clients.reserve(kBurstCount);
    const std::uint16_t port = listener.bound_port();
    for (int i = 0; i < kBurstCount; ++i) {
        clients.emplace_back([port, &ok_count] {
            const HttpResponse resp = do_http_get(port, "/@capsid/elastic-app/?delay=40");
            if (resp.status == 200 && resp.body == "elastic-ok") {
                ok_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (std::thread& t : clients) {
        t.join();
    }
    require(ok_count.load() == kBurstCount, "cold-start burst dropped or rejected requests (" +
                                                std::to_string(ok_count.load()) + "/" +
                                                std::to_string(kBurstCount) + ")");
    require(pool->ready_workers() >= 1 && pool->ready_workers() <= 4,
            "unexpected ready_workers count after burst");

    // Despite probes firing every 60ms, idleTimeout=250ms must reclaim all workers to 0.
    require(wait_for([&] { return pool->ready_workers() == 0 && ledger.elastic_used() == 0; },
                     std::chrono::milliseconds(5000)),
            "pool did not scale back to 0 within idleTimeout (probes kept it alive or leaked)");
    require(idle_evictions.load() >= 1, "idle_timeout eviction callback never fired");

    // Re-wake after scaling back to 0, then verify scale back to 0 once more.
    const HttpResponse rewoke = do_http_get(port, "/@capsid/elastic-app/");
    require(rewoke.status == 200 && rewoke.body == "elastic-ok", "post-reclaim wakeup failed");
    require(wait_for([&] { return pool->ready_workers() == 0 && ledger.elastic_used() == 0; },
                     std::chrono::milliseconds(5000)),
            "pool did not scale back to 0 after second wakeup");

    stop_supervisor.store(true);
    supervisor.request_stop();
    supervisor.join();
    listener.request_stop();
    listener.wait(&error);
    pool->stop_and_join();
    std::cout << "PASS: scale_to_zero_burst_and_reclaim" << std::endl;
}

void run_active_stream_and_ws_protected(const std::string& worker_path) {
    WorkerCapacityLedger ledger(2, 2);
    auto channel_budget = std::make_shared<capsid::host::ServiceChannelBudget>();
    GenerationPoolOptions pool_opts;
    pool_opts.application_id = "stream-ws-app";
    pool_opts.version = "v1";
    pool_opts.generation_digest = "sha256:" + std::string(64, 'b');
    pool_opts.workers = 2;
    pool_opts.has_min_ready = true;
    pool_opts.min_ready = 0;
    pool_opts.idle_timeout_ms = 200;
    pool_opts.acquire_elastic_slot = [&ledger] {
        return ledger.try_acquire_elastic("stream-ws-app");
    };
    pool_opts.release_elastic_slot = [&ledger] { ledger.release_elastic("stream-ws-app", 1); };
    pool_opts.channel_budget = channel_budget;
    pool_opts.max_inflight_per_worker = 4;
    pool_opts.max_queued_requests = 16;
    pool_opts.queue_timeout_ms = 5000;
    pool_opts.factory = spawn_factory(worker_path, 5000, kStreamAndWsBundle);
    pool_opts.recovery = default_recovery_policy();

    std::string error;
    std::shared_ptr<GenerationPool> pool = GenerationPool::create(pool_opts, &error);
    require(pool != nullptr, "stream pool create failed: " + error);

    auto routing = std::make_shared<RoutingTable>();
    routing->publish(RoutingSnapshot::build({{"stream-ws-app", pool}}));

    ManagedListenerOptions listener_opts;
    listener_opts.config = loopback_path_listener();
    listener_opts.routing = routing;
    listener_opts.channel_service_budget = channel_budget;
    ManagedListener listener(listener_opts);
    require(listener.start(&error), "listener start failed: " + error);
    const std::uint16_t port = listener.bound_port();

    // 1. Open SSE stream that holds for 500ms (> 200ms idleTimeout).
    {
        const SocketHandle fd = connect_listener(port);
        send_raw_request(fd, "GET /@capsid/stream-ws-app/?hold=500 HTTP/1.1\r\nHost: localhost\r\n"
                             "Connection: close\r\n\r\n");
        std::string head_and_first;
        char buf[1024];
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (head_and_first.find("data: open") == std::string::npos &&
               std::chrono::steady_clock::now() < deadline) {
#if defined(_WIN32)
            const int n = recv(fd, buf, static_cast<int>(sizeof(buf)), 0);
#else
            const ssize_t n = recv(fd, buf, sizeof(buf), 0);
#endif
            require(n > 0, "SSE handshake interrupted");
            head_and_first.append(buf, static_cast<std::size_t>(n));
        }
        require(head_and_first.find("data: open") != std::string::npos, "SSE open not received");
        std::this_thread::sleep_for(std::chrono::milliseconds(320));
        require(pool->ready_workers() == 1,
                "active SSE worker was prematurely evicted despite inflight stream");
        // Drain rest of stream cleanly.
        while (head_and_first.find("data: done") == std::string::npos &&
               std::chrono::steady_clock::now() < deadline) {
#if defined(_WIN32)
            const int n = recv(fd, buf, static_cast<int>(sizeof(buf)), 0);
#else
            const ssize_t n = recv(fd, buf, sizeof(buf), 0);
#endif
            if (n <= 0) {
                break;
            }
            head_and_first.append(buf, static_cast<std::size_t>(n));
        }
        require(head_and_first.find("data: done") != std::string::npos, "SSE done not received");
        close_socket_handle(fd);

        require(wait_for([&] { return pool->ready_workers() == 0 && ledger.elastic_used() == 0; },
                         std::chrono::milliseconds(4000)),
                "idle worker did not evict after SSE stream completed");
    }

    // 2. Open WebSocket upgrade connection and hold idle for 400ms (> 200ms idleTimeout).
    {
        const SocketHandle ws = connect_listener(port);
        send_raw_request(ws, "GET /@capsid/stream-ws-app/ HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "Upgrade: websocket\r\n"
                             "Connection: Upgrade\r\n"
                             "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                             "Sec-WebSocket-Version: 13\r\n\r\n");
        std::string hs;
        char buf[1024];
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (hs.find("\r\n\r\n") == std::string::npos &&
               std::chrono::steady_clock::now() < deadline) {
#if defined(_WIN32)
            const int n = recv(ws, buf, static_cast<int>(sizeof(buf)), 0);
#else
            const ssize_t n = recv(ws, buf, sizeof(buf), 0);
#endif
            require(n > 0, "WebSocket handshake interrupted");
            hs.append(buf, static_cast<std::size_t>(n));
        }
        require(hs.find("101") != std::string::npos, "WebSocket 101 handshake failed: " + hs);

        std::this_thread::sleep_for(std::chrono::milliseconds(380));
        require(pool->ready_workers() == 1,
                "active WebSocket worker was evicted while connection stayed open");
        send_raw_request(ws, std::string("\x88\x82\x12\x34\x56\x78\x11\xdc", 8));
        {
            char close_buf[64];
#if defined(_WIN32)
            (void)recv(ws, close_buf, static_cast<int>(sizeof(close_buf)), 0);
#else
            (void)recv(ws, close_buf, sizeof(close_buf), 0);
#endif
        }
        close_socket_handle(ws);

        require(wait_for([&] { return pool->ready_workers() == 0 && ledger.elastic_used() == 0; },
                         std::chrono::milliseconds(4000)),
                "worker did not evict after WebSocket client closed");
    }

    listener.request_stop();
    listener.wait(&error);
    pool->stop_and_join();
    std::cout << "PASS: active_stream_and_ws_protected" << std::endl;
}

void run_two_tier_reaping_slot_transfer(const std::string& worker_path) {
    WorkerCapacityLedger ledger(2, 2);
    require(ledger.reserve_fresh("app-a", 0) && ledger.reserve_fresh("app-b", 0),
            "minReady=0 apps must reserve with 0 committed slots");
    ledger.commit_fresh("app-a");
    ledger.commit_fresh("app-b");

    GenerationPoolOptions opts_a;
    opts_a.application_id = "app-a";
    opts_a.version = "v1";
    opts_a.generation_digest = "sha256:" + std::string(64, 'c');
    opts_a.workers = 2;
    opts_a.has_min_ready = true;
    opts_a.min_ready = 0;
    opts_a.idle_timeout_ms = 60000;
    opts_a.acquire_elastic_slot = [&ledger] { return ledger.try_acquire_elastic("app-a"); };
    opts_a.release_elastic_slot = [&ledger] { ledger.release_elastic("app-a", 1); };
    opts_a.max_inflight_per_worker = 1;
    opts_a.max_queued_requests = 16;
    opts_a.queue_timeout_ms = 5000;
    opts_a.factory = spawn_factory(worker_path, 5000, kBurstBundle);
    opts_a.recovery = default_recovery_policy();

    GenerationPoolOptions opts_b = opts_a;
    opts_b.application_id = "app-b";
    opts_b.generation_digest = "sha256:" + std::string(64, 'd');
    opts_b.acquire_elastic_slot = [&ledger] { return ledger.try_acquire_elastic("app-b"); };
    opts_b.release_elastic_slot = [&ledger] { ledger.release_elastic("app-b", 1); };

    std::string error;
    std::shared_ptr<GenerationPool> pool_a = GenerationPool::create(opts_a, &error);
    std::shared_ptr<GenerationPool> pool_b = GenerationPool::create(opts_b, &error);
    require(pool_a && pool_b, "elastic pools failed to create: " + error);

    auto routing = std::make_shared<RoutingTable>();
    routing->publish(RoutingSnapshot::build({{"app-a", pool_a}, {"app-b", pool_b}}));

    ManagedListenerOptions listener_opts;
    listener_opts.config = loopback_path_listener();
    listener_opts.routing = routing;
    ManagedListener listener(listener_opts);
    require(listener.start(&error), "listener start failed: " + error);
    const std::uint16_t port = listener.bound_port();

    // Scale app-a to all 2 global elastic slots using slow parallel requests.
    std::thread req1([port] {
        const HttpResponse r = do_http_get(port, "/@capsid/app-a/?delay=250");
        require(r.status == 200, "app-a req1 failed");
    });
    require(wait_for([&] { return pool_a->ready_workers() == 1; }, std::chrono::milliseconds(4000)),
            "app-a did not warm slot 1");
    std::thread req2([port] {
        const HttpResponse r = do_http_get(port, "/@capsid/app-a/?delay=250");
        require(r.status == 200, "app-a req2 failed");
    });
    req1.join();
    req2.join();
    require(pool_a->ready_workers() == 2 && ledger.elastic_used() == 2,
            "app-a did not occupy both elastic slots");
    require(pool_b->request_scale_out() == GenerationPool::ScaleOutResult::kCapacityRejected,
            "app-b scale-out must be rejected while workersTotal=2 is full");

    // Asynchronous reaping transfer: evicting 1 idle worker from app-a moves
    // it to retired_ and immediately frees 1 elastic quota slot for app-b.
    require(pool_a->evict_idle_worker(0, "global_lru"), "evict_idle_worker on app-a failed");
    require(pool_a->ready_workers() == 1 && ledger.elastic_used() == 1,
            "async reaping did not release elastic quota immediately");

    const HttpResponse resp_b = do_http_get(port, "/@capsid/app-b/");
    require(resp_b.status == 200 && resp_b.body == "elastic-ok",
            "app-b did not acquire transferred slot cleanly");
    require(pool_b->ready_workers() == 1 && ledger.elastic_used() == 2,
            "transferred elastic slot accounting mismatch");

    listener.request_stop();
    listener.wait(&error);
    pool_a->stop_and_join();
    pool_b->stop_and_join();
    require(ledger.elastic_used() == 0, "elastic slots leaked after drain");
    std::cout << "PASS: two_tier_reaping_slot_transfer" << std::endl;
}

GenerationPoolOptions cold_pool_options() {
    GenerationPoolOptions opts;
    opts.application_id = "cold";
    opts.version = "v1";
    opts.generation_digest = "sha256:" + std::string(64, 'a');
    opts.workers = 1;
    opts.has_min_ready = true;
    opts.min_ready = 0;
    opts.recovery = default_recovery_policy();
    return opts;
}

void run_cold_start_permit_refusal() {
    capsid::host::StartupPermitCoordinator permits(nullptr, 0);
    WorkerCapacityLedger ledger(1, 0);
    unsigned factory_calls = 0;
    unsigned failures = 0;
    std::string failure_reason;
    auto opts = cold_pool_options();
    opts.startup_permits = &permits;
    opts.acquire_elastic_slot = [&] { return ledger.try_acquire_elastic("cold"); };
    opts.release_elastic_slot = [&] { ledger.release_elastic("cold"); };
    opts.factory = [&](capsid_worker**, std::string*) {
        ++factory_calls;
        return false;
    };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create cold pool: " + error);
    pool->set_scale_callbacks({}, [&](const std::string& reason) {
        ++failures;
        failure_reason = reason;
    });
    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "expected asynchronous scale-out");
    const bool settled =
        wait_for([&] { return !pool->scale_out_in_flight(); }, std::chrono::seconds(5));
    pool->stop_and_join();
    require(settled && factory_calls == 0 && ledger.elastic_used() == 0,
            "permit refusal must settle without spawning or holding capacity");
    require(failures == 1 && !failure_reason.empty(),
            "cold-start waiter must receive one failure notification when its permit is refused");
}

void run_cold_start_retry_after_spawn_failure(const std::string& worker_path) {
    WorkerCapacityLedger ledger(1, 0);
    std::atomic<unsigned> attempts{0};
    std::atomic<unsigned> failures{0};
    std::atomic<unsigned> ready{0};
    auto spawn = spawn_factory(worker_path, 5000, kBurstBundle);
    auto opts = cold_pool_options();
    opts.acquire_elastic_slot = [&] { return ledger.try_acquire_elastic("cold"); };
    opts.release_elastic_slot = [&] { ledger.release_elastic("cold"); };
    opts.factory = [&](capsid_worker** worker, std::string* error) {
        if (++attempts == 1) {
            *error = "test factory refused the first start";
            return false;
        }
        return spawn(worker, error);
    };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create cold pool: " + error);
    pool->set_scale_callbacks([&] { ++ready; }, [&](const std::string&) { ++failures; });
    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "first cold start was not scheduled");
    require(wait_for([&] { return failures.load() == 1; }, std::chrono::seconds(5)),
            "factory failure never reached the waiter");
    require(pool->ready_workers() == 0 && ledger.elastic_used() == 0 && ready.load() == 0,
            "failed start published a worker or retained the slot");
    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "retry could not acquire the released slot");
    require(wait_for([&] { return ready.load() == 1; }, std::chrono::seconds(5)),
            "retry never reached READY");
    require(attempts.load() == 2 && failures.load() == 1 && pool->ready_workers() == 1 &&
                ledger.elastic_used() == 1,
            "retry did not produce exactly one serving worker");
    pool->stop_and_join();
    require(ledger.elastic_used() == 0 &&
                pool->request_scale_out() == GenerationPool::ScaleOutResult::kNotActive,
            "a drained pool retained capacity or accepted new work");
}

void run_ws_upgrade_pumps_parked_queue(const std::string& worker_path) {
    static const char* kGatedWsBundle = R"js(
        let released = false;
        export default {
            async fetch(request) {
                if (request.headers.get('upgrade')?.toLowerCase() !== 'websocket')
                    return new Response('parked-ok');
                const control = new URL(request.url).searchParams.get('control') === 'yes';
                while (!control && !released) await new Promise(resolve => setTimeout(resolve, 5));
                const [client, server] = Object.values(new WebSocketPair());
                server.accept();
                if (control) server.addEventListener('message', () => { released = true; });
                server.addEventListener('close', () => server.close());
                return new Response(null, { status: 101, webSocket: client });
            }
        };
    )js";

    WorkerCapacityLedger ledger(1, 0);
    require(ledger.reserve_fresh("ws-queue", 1), "reserve ws-queue floor");
    ledger.commit_fresh("ws-queue");
    auto channel_budget = std::make_shared<capsid::host::ServiceChannelBudget>();

    GenerationPoolOptions pool_opts;
    pool_opts.application_id = "ws-queue";
    pool_opts.version = "v1";
    pool_opts.generation_digest = "sha256:" + std::string(64, 'e');
    pool_opts.workers = 1;
    pool_opts.has_min_ready = true;
    pool_opts.min_ready = 1;
    pool_opts.channel_budget = channel_budget;
    pool_opts.max_inflight_per_worker = 1;
    pool_opts.max_queued_requests = 1;
    pool_opts.queue_timeout_ms = 15000;
    pool_opts.factory = spawn_factory(worker_path, 15000, kGatedWsBundle);
    pool_opts.recovery = default_recovery_policy();

    std::string error;
    std::shared_ptr<GenerationPool> pool = GenerationPool::create(pool_opts, &error);
    require(pool != nullptr, "ws-queue pool create failed: " + error);

    auto routing = std::make_shared<RoutingTable>();
    routing->publish(RoutingSnapshot::build({{"ws-queue", pool}}));

    ManagedListenerOptions listener_opts;
    listener_opts.config = loopback_path_listener();
    listener_opts.routing = routing;
    listener_opts.channel_service_budget = channel_budget;
    ManagedListener listener(listener_opts);
    require(listener.start(&error), "listener start failed: " + error);
    const std::uint16_t port = listener.bound_port();

    const auto start_upgrade = [port](const std::string& query) {
        const SocketHandle fd = connect_listener(port);
        send_raw_request(fd, "GET /@capsid/ws-queue/" + query +
                                 " HTTP/1.1\r\n"
                                 "Host: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                                 "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                                 "Sec-WebSocket-Version: 13\r\n\r\n");
        return fd;
    };
    const auto read_upgrade = [](SocketHandle fd, const std::string& name) {
        std::string headers;
        char buf[1024];
        while (headers.find("\r\n\r\n") == std::string::npos) {
            const auto n = recv(fd, buf, sizeof(buf), 0);
            require(n > 0, "WebSocket handshake interrupted: " + headers);
            headers.append(buf, static_cast<std::size_t>(n));
        }
        require(headers.rfind("HTTP/1.1 101 ", 0) == 0, name + " upgrade failed: " + headers);
    };

    const SocketHandle control = start_upgrade("?control=yes");
    read_upgrade(control, "control");
    const SocketHandle ws = start_upgrade("");
    require(wait_for([&] { return pool->inflight() == 1; }, std::chrono::seconds(4)),
            "subject upgrade never occupied the request slot");

    const auto get = [port] { return do_http_get(port, "/@capsid/ws-queue/"); };
    auto first = std::async(std::launch::async, get);
    auto second = std::async(std::launch::async, get);
    const auto ready = [](auto& future) {
        return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    };
    require(wait_for([&] { return ready(first) || ready(second); }, std::chrono::seconds(5)),
            "second GET never encountered a full queue during the pending upgrade");
    const bool first_done = ready(first);
    auto& rejected = first_done ? first : second;
    auto& parked = first_done ? second : first;
    const HttpResponse full = rejected.get();
    require(full.status == 429 && full.body == "request queue full" && !ready(parked),
            "expected one rejected GET and one parked GET before releasing the upgrade");
    // A masked one-byte client text frame ("g") releases the subject's gate.
    send_raw_request(control, std::string("\x81\x81\x01\x02\x03\x04\x66", 7));
    read_upgrade(ws, "subject");
    const HttpResponse parked_resp = parked.get();
    require(parked_resp.status == 200 && parked_resp.body == "parked-ok",
            "parked request failed after 101 upgrade: status=" +
                std::to_string(parked_resp.status) + " body=" + parked_resp.body);

    const std::string close_frame("\x88\x82\x12\x34\x56\x78\x11\xdc", 8);
    send_raw_request(ws, close_frame);
    send_raw_request(control, close_frame);
    close_socket_handle(ws);
    close_socket_handle(control);

    listener.request_stop();
    listener.wait(&error);
    pool->stop_and_join();
    ledger.release_active("ws-queue", 1);
    std::cout << "PASS: ws_upgrade_pumps_parked_queue" << std::endl;
}

void run_drain_during_slot_admission() {
    for (const bool grant_slot : {true, false}) {
        WorkerCapacityLedger ledger(1, 0);
        std::promise<void> entered_admission;
        auto entered = entered_admission.get_future();
        std::promise<void> resume_admission;
        auto resume = resume_admission.get_future();
        std::atomic<unsigned> factory_calls{0};

        auto opts = cold_pool_options();
        opts.acquire_worker_slot = [&](const GenerationPool*, std::string* reason) -> bool {
            const bool acquired = grant_slot ? ledger.try_acquire_elastic("cold") : false;
            if (!acquired && reason != nullptr) {
                *reason = "host_capacity_exhausted";
            }
            entered_admission.set_value();
            (void)resume.wait_for(std::chrono::seconds(5));
            return acquired;
        };
        opts.release_elastic_slot = [&] { ledger.release_elastic("cold", 1); };
        opts.factory = [&](capsid_worker**, std::string* err) {
            ++factory_calls;
            if (err != nullptr) {
                *err = "unexpected spawn during drain";
            }
            return false;
        };

        std::string error;
        auto pool = GenerationPool::create(std::move(opts), &error);
        require(pool != nullptr, "create cold pool: " + error);

        std::string scale_reason;
        auto pending =
            std::async(std::launch::async, [&] { return pool->request_scale_out(&scale_reason); });
        require(entered.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
                "acquire_worker_slot never entered");
        require(ledger.elastic_used() == (grant_slot ? 1U : 0U),
                "unexpected elastic ledger occupancy before drain");

        pool->request_drain();
        require(wait_for([&] { return pool->drain_check_cycles() >= 2; }, std::chrono::seconds(5)),
                "pump never evaluated drain completion while slot admission was in flight");
        require(pool->state() == GenerationPool::State::kDraining,
                "pump finalized drain before in-flight slot admission returned");

        resume_admission.set_value();
        require(pending.get() == GenerationPool::ScaleOutResult::kNotActive,
                "request_scale_out must abort with kNotActive when pool drained mid-admission");
        pool->stop_and_join();

        require(pool->state() == GenerationPool::State::kDead && factory_calls.load() == 0,
                "drained pool either stayed alive or launched a factory after admission race");
        require(ledger.elastic_used() == 0 && ledger.try_acquire_elastic("peer"),
                "capacity leaked after draining mid-admission");
        ledger.release_elastic("peer", 1);
    }
    std::cout << "PASS: drain_during_slot_admission" << std::endl;
}

void run_drain_during_worker_start(const std::string& worker_path) {
    WorkerCapacityLedger ledger(1, 0);
    std::promise<void> entered_factory;
    auto entered = entered_factory.get_future();
    std::promise<void> resume_factory;
    auto resume = resume_factory.get_future();
    std::atomic<bool> spawned{false};
    auto spawn = spawn_factory(worker_path, 5000, kBurstBundle);
    auto opts = cold_pool_options();
    opts.acquire_elastic_slot = [&] { return ledger.try_acquire_elastic("cold"); };
    opts.release_elastic_slot = [&] { ledger.release_elastic("cold"); };
    opts.factory = [&](capsid_worker** worker, std::string* error) {
        entered_factory.set_value();
        if (resume.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
            *error = "timed out waiting for drain signal";
            return false;
        }
        spawned.store(spawn(worker, error));
        return spawned.load();
    };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create cold pool: " + error);
    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "could not schedule cold start");
    require(entered.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
            "worker factory did not start");
    require(ledger.elastic_used() == 1, "factory started without a capacity slot");
    pool->request_drain();
    const bool refused = pool->request_scale_out() == GenerationPool::ScaleOutResult::kNotActive;
    resume_factory.set_value();
    pool->stop_and_join();
    require(spawned.load(), "fixture did not complete a successful in-progress startup");
    require(refused && pool->state() == GenerationPool::State::kDead && !pool->pick_worker(),
            "drained generation accepted or published more work");
    require(ledger.elastic_used() == 0 && ledger.try_acquire_elastic("peer"),
            "completed drain did not return the in-progress worker's capacity");
    ledger.release_elastic("peer");
}

#if defined(__linux__)
void run_cold_cycles_release_descriptors(const std::string& worker_path) {
    const auto open_fds = [] {
        return std::distance(std::filesystem::directory_iterator("/proc/self/fd"),
                             std::filesystem::directory_iterator());
    };
    auto opts = cold_pool_options();
    opts.factory = spawn_factory(worker_path, 5000, kBurstBundle);
    std::string error;
    // Initialize process-wide runtime resources before measuring per-worker descriptors.
    {
        auto warm = opts;
        warm.min_ready = 1;
        auto pool = GenerationPool::create(std::move(warm), &error);
        require(pool != nullptr, "warm up runtime: " + error);
        pool->stop_and_join();
    }
    for (const bool with_started_observer : {false, true}) {
        auto run_opts = opts;
        std::atomic<unsigned> started_count{0};
        if (with_started_observer) {
            run_opts.on_worker_started = [&](const WorkerExecutor*) { ++started_count; };
        }
        auto pool = GenerationPool::create(std::move(run_opts), &error);
        require(pool != nullptr, "create cold pool: " + error);
        const auto baseline = open_fds();
        for (int cycle = 0; cycle < 3; ++cycle) {
            require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
                    "cold wakeup did not schedule a worker");
            require(wait_for([&] { return pool->ready_workers() == 1; }, std::chrono::seconds(5)),
                    "cold worker did not become ready");
            require(pool->evict_idle_worker(0, "global_lru"), "could not release idle worker");
        }
        const bool reclaimed =
            wait_for([&] { return open_fds() <= baseline; }, std::chrono::seconds(5));
        const auto remaining = open_fds();
        pool->stop_and_join();
        require(!with_started_observer || started_count.load() == 3,
                "on_worker_started did not fire for all cold cycles");
        require(reclaimed,
                std::string("live generation retained descriptors after three cold cycles") +
                    (with_started_observer ? " (with on_worker_started)" : "") +
                    ": before=" + std::to_string(baseline) + " after=" + std::to_string(remaining));
    }
}
#endif

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
#endif

void run_disk_backed_generation_tamper_fails(const std::string& worker_path) {
#if defined(_WIN32)
    (void)worker_path;
    std::cout << "SKIP: disk_backed_generation_tamper_fails requires POSIX managed_host"
              << std::endl;
    std::exit(77);
#else
    const std::string apps_root = create_temp_dir("capsid-elastic-apps");
    const std::string state_root = create_temp_dir("capsid-elastic-state");
    require(mkdir((apps_root + "/coldapp").c_str(), 0700) == 0, "mkdir coldapp failed");
    const std::string v1 = apps_root + "/coldapp/v1";
    require(mkdir(v1.c_str(), 0700) == 0, "mkdir v1 failed");
    write_text_file(
        v1 + "/capsid.json",
        R"json({"apiVersion":"capsid/app-v2","entry":"bundle.mjs","pool":{"minReady":0,"maxWorkers":2,"idleTimeout":"60s"}})json");
    write_text_file(v1 + "/bundle.mjs", kBurstBundle);

    const int apps_fd = open(apps_root.c_str(), O_RDONLY | O_DIRECTORY);
    require(apps_fd >= 0, "open apps_root failed");

    capsid_build_info build_info;
    capsid_build_info_init(&build_info);
    require(capsid_runtime_build_info(&build_info) == CAPSID_OK &&
                build_info.compatibility_id != nullptr,
            "cannot query runtime compatibility id");

    WorkerCapacityLedger ledger(4, 2);
    ManagedHostOptions opts;
    opts.applications_root_fd = apps_fd;
    opts.state_root = state_root;
    opts.application = "coldapp";
    opts.worker_path = worker_path;
    opts.runtime_compatibility_id = build_info.compatibility_id;
    opts.host_policy.is_v2 = true;
    opts.host_policy.max_workers = 4;
    opts.host_policy.max_requests_per_worker = 64;
    opts.host_policy.max_worker_memory_bytes = 256ULL * 1024 * 1024;
    opts.ledger = &ledger;

    capsid::host::OperationStatus status;
    capsid::host::DeployOutcome deployed = capsid::host::managed_deploy(&opts, "v1", &status);
    require(deployed.ok && deployed.generation_factory,
            "minReady=0 managed deploy failed: " + deployed.error);
    require(deployed.effective.min_ready == 0 && deployed.effective.workers == 2 &&
                deployed.effective.queue_requests == 64 &&
                deployed.effective.queue_timeout_ms == 5000,
            "deployed effective config mismatch");
    require(deployed.workers.size() == 1, "deploy must warm 1 probe worker even when minReady=0");
    require(ledger.steady_used() == 0 && ledger.holds("coldapp"),
            "minReady=0 app must hold active identity with 0 committed steady slots");
    for (capsid_worker* w : deployed.workers) {
        capsid_worker_destroy(w);
    }

    // Tamper committed bundle.bin on disk; disk-backed generation factory must
    // re-read & verify SHA-256 and fail closed instead of serving tampered bytes.
    const std::string bundle_path =
        state_root + "/apps/coldapp/generations/" + deployed.generation_digest + "/bundle.bin";
    write_text_file(bundle_path,
                    "export default { fetch() { return new Response('tampered'); } };");

    capsid_worker* spawned = nullptr;
    std::string factory_error;
    require(!deployed.generation_factory(&spawned, &factory_error) && spawned == nullptr,
            "tampered disk bundle was accepted by generation_factory");
    require(factory_error.find("digest mismatch") != std::string::npos,
            "unexpected factory error for tampered bundle: " + factory_error);

    close(apps_fd);
    std::cout << "PASS: disk_backed_generation_tamper_fails" << std::endl;
#endif
}

void run_dispatch_lease_blocks_eviction(const std::string& worker_path) {
    WorkerCapacityLedger ledger(2, 0);
    auto opts = cold_pool_options();
    opts.workers = 2;
    opts.min_ready = 0;
    opts.max_inflight_per_worker = 1;
    opts.factory = spawn_factory(worker_path, 5000, kBurstBundle);
    opts.acquire_elastic_slot = [&] { return ledger.try_acquire_elastic("cold"); };
    opts.release_elastic_slot = [&] { ledger.release_elastic("cold", 1); };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create elastic pool: " + error);

    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "could not warm first elastic worker");
    require(wait_for([&] { return pool->ready_workers() == 1; }, std::chrono::seconds(5)),
            "worker never reached ready");

    GenerationPool::DispatchLease lease = pool->acquire_worker();
    require(static_cast<bool>(lease) && lease->pending_dispatches() == 1 && !lease->is_idle(),
            "acquire_worker did not arm pending dispatch protection");

    bool at_max_inflight = false;
    GenerationPool::DispatchLease second = pool->acquire_worker(&at_max_inflight);
    require(!second && at_max_inflight,
            "second acquire_worker did not observe pending dispatch against max_inflight");

    const auto snaps = pool->snapshot_workers();
    require(snaps.size() == 1 && !snaps[0].idle,
            "snapshot reported pending-dispatch worker as idle");
    require(!pool->evict_idle_worker(0, "global_lru"),
            "idle eviction evicted a worker holding an active DispatchLease");
    require(pool->ready_workers() == 1, "worker disappeared while DispatchLease was held");

    lease.release();
    require(pool->evict_idle_worker(0, "global_lru"),
            "worker could not be evicted once DispatchLease was released");
    require(pool->ready_workers() == 0 && ledger.elastic_used() == 0,
            "releasing DispatchLease and evicting did not return elastic slot");
    pool->stop_and_join();
    std::cout << "PASS: dispatch_lease_blocks_eviction" << std::endl;
}

void run_floor_recovery_after_scale_out_failure(const std::string& worker_path) {
    auto opts = cold_pool_options();
    opts.min_ready = 1;
    opts.workers = 2;
    std::atomic<unsigned> calls{0};
    std::promise<void> entered;
    std::promise<void> release;
    std::promise<void> failed;
    auto gate = release.get_future();
    auto spawn = spawn_factory(worker_path, 5000, kBurstBundle);
    opts.factory = [&](capsid_worker** out, std::string* error) {
        if (++calls == 2) {
            entered.set_value();
            gate.wait();
            *error = "controlled scale-out failure";
            return false;
        }
        return spawn(out, error);
    };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create floor-recovery pool: " + error);
    pool->set_scale_callbacks({}, [&](const std::string&) { failed.set_value(); });
    auto floor = pool->current_worker_snapshot();
    require(floor != nullptr, "missing initial floor worker");
    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "scale-out did not start");
    require(entered.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready,
            "scale-out factory never entered");
    require(pool->recycle_worker(floor.get()), "health recycle of floor worker refused");
    release.set_value();
    require(failed.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready,
            "missing scale-out failure callback");
    // Without any new incoming request, the pool must restore its minReady=1 floor.
    const bool restored =
        wait_for([&] { return pool->ready_workers() >= 1; }, std::chrono::seconds(5));
    const auto ready = pool->ready_workers();
    const auto total_calls = calls.load();
    pool->stop_and_join();
    require(restored && ready == 1 && total_calls >= 3,
            "floor was not automatically restored after scale-out failure: ready=" +
                std::to_string(ready) + " calls=" + std::to_string(total_calls));
    std::cout << "PASS: floor_recovery_after_scale_out_failure" << std::endl;
}

void run_floor_recovery_after_admission_refusal(const std::string& worker_path) {
    using namespace std::chrono_literals;
    WorkerCapacityLedger ledger(2, 0);
    require(ledger.reserve_fresh("cold", 1), "reserve floor");
    ledger.commit_fresh("cold");
    require(ledger.try_acquire_elastic("peer"), "occupy remaining slot");

    std::promise<void> admitting;
    std::promise<void> resume;
    auto gate = resume.get_future().share();
    std::atomic<unsigned> attempts{0};
    std::atomic<unsigned> denials{0};
    auto opts = cold_pool_options();
    opts.min_ready = 1;
    opts.workers = 2;
    opts.factory = spawn_factory(worker_path, 5000, kBurstBundle);
    opts.acquire_elastic_slot = [&] {
        if (++attempts == 1) {
            admitting.set_value();
            gate.wait();
        }
        const bool granted = ledger.try_acquire_elastic("cold");
        if (!granted)
            ++denials;
        return granted;
    };
    opts.release_elastic_slot = [&] { ledger.release_elastic("cold"); };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create floor pool: " + error);
    auto floor = pool->current_worker_snapshot();
    require(floor != nullptr, "missing initial floor worker");
    auto scaling = std::async(std::launch::async, [&] { return pool->request_scale_out(); });
    require(admitting.get_future().wait_for(5s) == std::future_status::ready,
            "elastic admission did not start");
    require(pool->recycle_worker(floor.get()), "floor recycle refused during admission");
    resume.set_value();
    scaling.get();
    require(denials.load() >= 1, "fixture did not reject elastic admission");

    // No more requests or scale-out calls. The active floor is already paid for.
    const bool recovered = wait_for([&] { return pool->ready_workers() == 1; }, 5s);
    auto replacement = pool->current_worker_snapshot();
    const bool settled = ledger.steady_of("cold") == 1 && ledger.elastic_of("cold") == 0 &&
                         ledger.elastic_of("peer") == 1;
    pool->stop_and_join();
    ledger.release_elastic("peer");
    require(recovered && replacement && replacement != floor,
            "admission refusal left minReady=1 without a replacement worker");
    require(settled, "floor recovery changed the peer's capacity or borrowed an elastic slot");
}

void run_exit_observer_during_floor_replacement(const std::string& worker_path) {
    using namespace std::chrono_literals;
    WorkerCapacityLedger ledger(2, 0);
    require(ledger.reserve_fresh("cold", 1), "reserve floor");
    ledger.commit_fresh("cold");
    std::promise<void> spawning;
    std::promise<void> resume;
    std::promise<void> exit_delivered;
    auto gate = resume.get_future();
    std::atomic<unsigned> calls{0};
    std::atomic<const WorkerExecutor*> original{nullptr};
    std::atomic<unsigned> old_exits{0};
    auto opts = cold_pool_options();
    opts.min_ready = 1;
    opts.workers = 2;
    auto spawn = spawn_factory(worker_path, 5000, kBurstBundle);
    opts.factory = [&](capsid_worker** out, std::string* error) {
        if (++calls == 2) {
            spawning.set_value();
            gate.wait();
        }
        return spawn(out, error);
    };
    opts.on_worker_exited = [&](const WorkerExecutor* worker) {
        if (worker == original.load())
            ++old_exits;
    };
    opts.acquire_elastic_slot = [&] { return ledger.try_acquire_elastic("cold"); };
    opts.release_elastic_slot = [&] { ledger.release_elastic("cold"); };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create pool: " + error);
    auto floor = pool->current_worker_snapshot();
    require(floor != nullptr, "missing initial worker");
    original = floor.get();
    pool->set_shared_event_sink(
        [&](std::shared_ptr<WorkerExecutor> worker, capsid::host::WorkerEvent event) {
            if (worker == floor && event.type == capsid::host::WorkerEvent::Type::kExit)
                exit_delivered.set_value();
        });
    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "scale-out did not start");
    require(spawning.get_future().wait_for(5s) == std::future_status::ready &&
                ledger.elastic_of("cold") == 1,
            "elastic worker did not reach its factory with a reserved slot");
    require(pool->recycle_worker(floor.get()), "floor recycle refused");
    require(exit_delivered.get_future().wait_for(5s) == std::future_status::ready,
            "original worker exit did not reach the event sink before replacement");
    resume.set_value();
    require(
        wait_for([&] { return pool->ready_workers() == 1 && ledger.elastic_of("cold") == 0; }, 5s),
        "replacement failed to take over floor and release its elastic slot");
    const bool peer = ledger.try_acquire_elastic("peer");
    const bool notified = wait_for([&] { return old_exits.load() >= 1; }, 5s);
    pool->set_shared_event_sink({});
    pool->stop_and_join();
    if (peer)
        ledger.release_elastic("peer");
    require(peer, "peer could not reuse elastic slot");
    require(notified && old_exits.load() == 1,
            "expected exactly one exit notification for the original worker before drain; total=" +
                std::to_string(old_exits.load()));
}

void run_begin_after_exit(const std::string& worker_path, bool leased) {
    using namespace std::chrono_literals;
    auto opts = cold_pool_options();
    opts.min_ready = 1;
    opts.factory = spawn_factory(worker_path, 5000, kBurstBundle);
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create pool: " + error);
    auto floor = pool->current_worker_snapshot();
    require(floor != nullptr, "missing initial worker");
    GenerationPool::DispatchLease lease;
    if (leased) {
        lease = pool->acquire_worker();
        require(lease.get() == floor.get() && floor->pending_dispatches() == 1,
                "lease did not claim original worker");
    }
    require(pool->recycle_worker(floor.get()), "floor recycle refused");
    require(wait_for([&] { return floor->exited(); }, 5s), "original worker did not exit");
    capsid::host::Command begin;
    begin.type = capsid::host::CommandType::kBeginRequest;
    begin.request_id = 11;
    begin.method = "GET";
    begin.url = "http://localhost/";
    begin.end_request = true;
    const bool accepted =
        leased ? lease.submit_begin(std::move(begin)) : floor->submit(std::move(begin));

    const auto pending = floor->pending_dispatches();
    const auto inflight = floor->inflight();
    const auto claims = floor->active_http_requests();
    pool->stop_and_join();
    require(!accepted, "caller was told that an exited worker accepted the begin");
    require(pending == 0 && inflight == 0 && claims == 0,
            "begin submitted after exit left pending=" + std::to_string(pending) +
                " inflight=" + std::to_string(inflight) + " claims=" + std::to_string(claims));
}

void run_floor_recovery_stops_at_quarantine(const std::string& worker_path) {
    using namespace std::chrono_literals;
    auto opts = cold_pool_options();
    opts.min_ready = 1;
    opts.recovery.max_events = 3;
    auto spawn = spawn_factory(worker_path, 5000, kBurstBundle);
    std::atomic<unsigned> starts{0};
    std::atomic<unsigned> quarantines{0};
    opts.factory = [&](capsid_worker** out, std::string* error) {
        if (++starts == 1)
            return spawn(out, error);
        *error = "controlled replacement failure";
        return false;
    };
    opts.on_quarantine = [&] { ++quarantines; };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create pool: " + error);
    auto floor = pool->current_worker_snapshot();
    require(floor && pool->recycle_worker(floor.get()), "could not recycle initial floor");
    const bool dead = wait_for([&] { return pool->state() == GenerationPool::State::kDead; }, 5s);
    const auto ready = pool->ready_workers();
    pool->stop_and_join();
    require(dead && ready == 0 && quarantines.load() == 1 && starts.load() == 4,
            "floor recovery did not settle after three failures: starts=" +
                std::to_string(starts.load()) +
                " quarantine callbacks=" + std::to_string(quarantines.load()));
}

void run_health_recycle_returns_elastic_slot(const std::string& worker_path) {
    WorkerCapacityLedger ledger(2, 0);
    require(ledger.reserve_fresh("cold", 1), "reserve floor");
    ledger.commit_fresh("cold");
    auto opts = cold_pool_options();
    opts.min_ready = 1;
    opts.workers = 2;
    opts.factory = spawn_factory(worker_path, 5000, kBurstBundle);
    opts.acquire_elastic_slot = [&] { return ledger.try_acquire_elastic("cold"); };
    opts.release_elastic_slot = [&] { ledger.release_elastic("cold"); };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create health-recycle pool: " + error);
    auto floor = pool->current_worker_snapshot();
    require(floor != nullptr, "missing initial floor worker");
    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "could not scale to 2 workers");
    require(wait_for([&] { return pool->ready_workers() == 2; }, std::chrono::seconds(5)),
            "second worker never became ready");
    require(ledger.elastic_of("cold") == 1, "expected 1 elastic slot before recycle");
    require(pool->recycle_worker(floor.get()), "health recycle refused");
    const auto ready = pool->ready_workers();
    const auto charged = ledger.elastic_of("cold");
    const bool peer_acquired = ledger.try_acquire_elastic("peer");
    if (peer_acquired) {
        ledger.release_elastic("peer");
    }
    pool->stop_and_join();
    ledger.release_active("cold", 1);
    require(ready == 1 && charged == 0 && peer_acquired,
            "health recycle of floor worker failed to return elastic slot to peer");
    std::cout << "PASS: health_recycle_returns_elastic_slot" << std::endl;
}

void run_http_selection_skips_full_worker(const std::string& worker_path) {
    auto opts = cold_pool_options();
    opts.workers = 2;
    opts.min_ready = 2;
    opts.max_inflight_per_worker = 1;
    const WorkerExecutor* first = nullptr;
    opts.client_bytes_loader = [&](const WorkerExecutor* w) -> std::uint64_t {
        return (first != nullptr && w != first) ? 2ULL : 0ULL;
    };
    opts.factory = spawn_factory(worker_path, 5000, kBurstBundle);
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create two-worker pool: " + error);
    std::mutex sink_mutex;
    std::condition_variable sink_cv;
    const WorkerExecutor* served_second_by = nullptr;
    std::uint16_t second_status = 0;
    std::string second_body;
    bool second_done = false;
    pool->set_shared_event_sink(
        [&](std::shared_ptr<WorkerExecutor> exec, capsid::host::WorkerEvent ev) {
            if (ev.request_id != 202) {
                return;
            }
            std::lock_guard<std::mutex> lock(sink_mutex);
            if (ev.type == capsid::host::WorkerEvent::Type::kResponseHead) {
                served_second_by = exec.get();
                second_status = ev.status;
            } else if (ev.type == capsid::host::WorkerEvent::Type::kResponseBody) {
                second_body.append(ev.body.begin(), ev.body.end());
            } else if (ev.type == capsid::host::WorkerEvent::Type::kResponseEnd) {
                second_done = true;
                sink_cv.notify_all();
            }
        });
    {
        auto lease_a = pool->acquire_worker();
        require(static_cast<bool>(lease_a), "first lease failed");
        first = lease_a.get();
        bool at_max = false;
        auto lease_b = pool->acquire_worker(&at_max);
        require(static_cast<bool>(lease_b) && lease_b.get() != first && !at_max,
                "scheduler reported pool full even though second worker had a free HTTP slot");
        const WorkerExecutor* second_worker = lease_b.get();
        bool third_full = false;
        auto lease_c = pool->acquire_worker(&third_full);
        require(!lease_c && third_full,
                "scheduler did not report at_max once all ready workers reached max_inflight");

        capsid::host::Command begin;
        begin.type = capsid::host::CommandType::kBeginRequest;
        begin.request_id = 202;
        begin.method = "GET";
        begin.url = "http://localhost/second";
        begin.end_request = true;
        lease_b.submit_begin(std::move(begin));
        {
            std::unique_lock<std::mutex> lock(sink_mutex);
            require(sink_cv.wait_for(lock, std::chrono::seconds(5), [&] { return second_done; }),
                    "request dispatched to second worker timed out");
            require(served_second_by == second_worker && second_status == 200 &&
                        second_body == "elastic-ok",
                    "second request was not served cleanly by second worker");
        }
    }
    pool->stop_and_join();
    std::cout << "PASS: http_selection_skips_full_worker" << std::endl;
}

void run_dispatch_handoff_blocks_eviction(const std::string& worker_path) {
    WorkerCapacityLedger ledger(1, 0);
    auto opts = cold_pool_options();
    opts.workers = 1;
    opts.min_ready = 0;
    opts.factory = spawn_factory(worker_path, 5000, kBurstBundle);
    opts.acquire_elastic_slot = [&] { return ledger.try_acquire_elastic("cold"); };
    opts.release_elastic_slot = [&] { ledger.release_elastic("cold", 1); };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, "create handoff pool: " + error);

    std::mutex resp_mutex;
    std::condition_variable resp_cv;
    std::uint16_t status = 0;
    std::string body;
    bool ended = false;
    bool failed = false;
    pool->set_shared_event_sink([&](std::shared_ptr<WorkerExecutor>, capsid::host::WorkerEvent ev) {
        if (ev.request_id != 42) {
            return;
        }
        std::lock_guard<std::mutex> lock(resp_mutex);
        if (ev.type == capsid::host::WorkerEvent::Type::kResponseHead) {
            status = ev.status;
        } else if (ev.type == capsid::host::WorkerEvent::Type::kResponseBody) {
            body.append(ev.body.begin(), ev.body.end());
        } else if (ev.type == capsid::host::WorkerEvent::Type::kResponseEnd) {
            ended = true;
            resp_cv.notify_all();
        } else if (ev.type == capsid::host::WorkerEvent::Type::kRequestFailure ||
                   ev.type == capsid::host::WorkerEvent::Type::kRequestTimeout ||
                   ev.type == capsid::host::WorkerEvent::Type::kExit) {
            failed = true;
            resp_cv.notify_all();
        }
    });

    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "could not warm handoff worker");
    require(wait_for([&] { return pool->ready_workers() == 1; }, std::chrono::seconds(5)),
            "handoff worker never reached ready");

    GenerationPool::DispatchLease lease = pool->acquire_worker();
    require(static_cast<bool>(lease) && lease->pending_dispatches() == 1 && lease->inflight() == 0,
            "lease did not claim pending dispatch before handoff");
    std::shared_ptr<WorkerExecutor> target = lease.executor();

    std::promise<void> evictor_in_idle_window;
    std::promise<void> handoff_finished;
    auto handoff_future = handoff_finished.get_future();
    std::atomic<bool> hook_fired{false};
    target->set_idle_check_hook_for_test([&] {
        if (!hook_fired.exchange(true)) {
            evictor_in_idle_window.set_value();
            (void)handoff_future.wait_for(std::chrono::seconds(5));
        }
    });

    std::atomic<bool> evicted_during_handoff{false};
    std::thread evictor(
        [&] { evicted_during_handoff.store(pool->evict_idle_worker(0, "global_lru")); });

    require(evictor_in_idle_window.get_future().wait_for(std::chrono::seconds(5)) ==
                std::future_status::ready,
            "eviction thread never entered idle-check window");

    capsid::host::Command begin;
    begin.type = capsid::host::CommandType::kBeginRequest;
    begin.request_id = 42;
    begin.method = "GET";
    begin.url = "http://localhost/handoff";
    begin.end_request = true;
    lease.submit_begin(std::move(begin));
    require(target->pending_dispatches() == 0 && target->inflight() == 1,
            "submit_begin did not transfer lease to inflight");
    handoff_finished.set_value();

    evictor.join();
    target->set_idle_check_hook_for_test({});
    require(!evicted_during_handoff.load(),
            "eviction falsely observed idle during DispatchLease-to-inflight handoff");

    {
        std::unique_lock<std::mutex> lock(resp_mutex);
        require(resp_cv.wait_for(lock, std::chrono::seconds(5), [&] { return ended || failed; }),
                "in-flight request timed out after handoff");
        require(ended && !failed && status == 200 && body == "elastic-ok",
                "request in handoff did not receive complete 200 response: status=" +
                    std::to_string(status) + " body=" + body);
    }
    require(pool->ready_workers() == 1, "worker left ready set during handoff request");
    require(pool->evict_idle_worker(0, "global_lru"),
            "worker could not be evicted after request completed");
    require(pool->ready_workers() == 0 && ledger.elastic_used() == 0,
            "post-response eviction did not return elastic slot");
    pool->stop_and_join();
    std::cout << "PASS: dispatch_handoff_blocks_eviction" << std::endl;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        fail("usage: test-host-elastic-pool <capsid-worker> <case>");
    }
#if defined(_WIN32)
    require(capsid::win32::ensure_winsock(), "cannot initialize Winsock");
#endif
    const std::string worker_path = argv[1];
    const std::string mode = argv[2];
    if (mode == "cold_start_permit_refusal") {
        run_cold_start_permit_refusal();
        return 0;
    }
    if (mode == "cold_start_retry_after_spawn_failure") {
        run_cold_start_retry_after_spawn_failure(worker_path);
        return 0;
    }
    if (mode == "drain_during_worker_start") {
        run_drain_during_worker_start(worker_path);
        return 0;
    }
    if (mode == "drain_during_slot_admission") {
        run_drain_during_slot_admission();
        return 0;
    }
    if (mode == "ws_upgrade_pumps_parked_queue") {
        run_ws_upgrade_pumps_parked_queue(worker_path);
        return 0;
    }
#if defined(__linux__)
    if (mode == "cold_cycles_release_descriptors") {
        run_cold_cycles_release_descriptors(worker_path);
        return 0;
    }
#endif
    if (mode == "scale_to_zero_burst_and_reclaim") {
        run_scale_to_zero_burst_and_reclaim(worker_path);
        return 0;
    }
    if (mode == "active_stream_and_ws_protected") {
        run_active_stream_and_ws_protected(worker_path);
        return 0;
    }
    if (mode == "two_tier_reaping_slot_transfer") {
        run_two_tier_reaping_slot_transfer(worker_path);
        return 0;
    }
    if (mode == "disk_backed_generation_tamper_fails") {
        run_disk_backed_generation_tamper_fails(worker_path);
        return 0;
    }
    if (mode == "dispatch_lease_blocks_eviction") {
        run_dispatch_lease_blocks_eviction(worker_path);
        return 0;
    }
    if (mode == "floor_recovery_after_scale_out_failure") {
        run_floor_recovery_after_scale_out_failure(worker_path);
        return 0;
    }
    if (mode == "floor_recovery_after_admission_refusal") {
        run_floor_recovery_after_admission_refusal(worker_path);
        return 0;
    }
    if (mode == "floor_recovery_stops_at_quarantine") {
        run_floor_recovery_stops_at_quarantine(worker_path);
        return 0;
    }
    if (mode == "exit_observer_during_floor_replacement") {
        run_exit_observer_during_floor_replacement(worker_path);
        return 0;
    }
    if (mode == "leased_begin_after_exit" || mode == "direct_begin_after_exit") {
        run_begin_after_exit(worker_path, mode == "leased_begin_after_exit");
        return 0;
    }
    if (mode == "health_recycle_returns_elastic_slot") {
        run_health_recycle_returns_elastic_slot(worker_path);
        return 0;
    }
    if (mode == "http_selection_skips_full_worker") {
        run_http_selection_skips_full_worker(worker_path);
        return 0;
    }
    if (mode == "dispatch_handoff_blocks_eviction") {
        run_dispatch_handoff_blocks_eviction(worker_path);
        return 0;
    }
    fail("unknown elastic pool test case: " + mode);
}
