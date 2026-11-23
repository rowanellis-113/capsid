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
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
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
    fail("unknown elastic pool test case: " + mode);
}
