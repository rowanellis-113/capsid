// Exercise the config-v2 policy compiler and the section 7.2 pool decisions.
#include "host/config.h"
#define main original_eviction_test_main
#include "test_host_eviction_coordinator.cc"
#undef main

void check_memory_inheritance() {
    using namespace capsid::host;
    constexpr std::uint64_t mib = 1024ULL * 1024ULL;
    const std::string host_json = R"({
        "apiVersion":"capsid/host-v2",
        "paths":{"applications":"/apps","state":"/state","secrets":"/secrets/{application}"},
        "admin":{"unix":"/run/capsid.sock"},
        "listeners":[{"name":"public","tcp":"127.0.0.1:8080","routing":{"mode":"path"}}],
        "defaults":{"worker":{"memoryMax":"96MiB","jsHeap":"64MiB"}}
    })";
    ParsedHostConfig host;
    std::string error;
    require(validate_config_json(ConfigDocument::kHost, host_json).ok, "invalid host fixture");
    require(parse_host_config(host_json, &host, &error), "could not parse the host fixture");
    for (const unsigned declared : {72U, 32U}) {
        const std::string json = R"({"apiVersion":"capsid/app-v2","worker":{"memoryMax":")" +
                                 std::to_string(declared) + R"(MiB"}})";
        AppRequest app;
        require(validate_config_json(ConfigDocument::kApplication, json).ok, "invalid app fixture");
        require(parse_app_request({json.begin(), json.end()}, &app, &error), "could not parse app");
        const auto result = compile_policy(host.policy, app, {});
        std::cout << "declared=" << declared << " MiB accepted=" << result.ok
                  << " effective=" << result.effective.memory_bytes / mib << " MiB" << std::endl;
        if (declared == 72) {
            require(result.ok && result.effective.js_heap_bytes == 64 * mib &&
                        result.effective.memory_bytes == 72 * mib,
                    "a compatible inherited heap must preserve the declared worker maximum");
        } else if (!result.ok) {
            require(!result.error.empty(), "rejected memory policy must explain the conflict");
        } else {
            require(result.effective.memory_bytes == declared * mib &&
                        result.effective.js_heap_bytes == 64 * mib,
                    "accepted policy must preserve the declared worker maximum and inherited heap");
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        return 2;
    }
    const std::string mode = argv[2];
    if (mode == "memory-inheritance") {
        check_memory_inheritance();
        return 0;
    }
    const bool overshare = mode == "protected-overshare";
    if (!overshare && mode != "within-share") {
        return 2;
    }
    constexpr std::uint64_t mib = 1024ULL * 1024ULL;
    MemoryBudgetConfig memory;
    memory.configured = true;
    memory.limit_bytes = 100 * mib;
    memory.high_watermark_percent = 85;
    memory.low_watermark_percent = 75;
    memory.min_residency_ms = 0;
    memory.check_interval_ms = 0;
    std::mutex mutex;
    std::unordered_map<std::int64_t, std::uint64_t> bytes_by_pid;
    MemorySampler sampler({}, [&](std::int64_t pid, std::uint64_t* bytes) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = bytes_by_pid.find(pid);
        if (it == bytes_by_pid.end()) {
            return false;
        }
        *bytes = it->second;
        return true;
    });
    WorkerCapacityLedger ledger(2, 0);
    MetricsRegistry metrics;
    GlobalEvictionCoordinator coordinator(memory, {}, &ledger, &sampler, &metrics);
    require(ledger.reserve_fresh("floor", 1), "reserve the protected floor");
    ledger.commit_fresh("floor");
    auto floor = make_elastic_pool(argv[1], "floor", "a", 1, 1, 0, 0, 1, &ledger, &coordinator,
                                   &metrics);
    auto peer = make_elastic_pool(argv[1], "small", "b", 0, 1, 0, 0, 1, &ledger, &coordinator,
                                  &metrics);
    require(peer->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight,
            "wake the elastic peer");
    require(wait_for([&] { return floor->ready_workers() == 1 && peer->ready_workers() == 1; },
                     std::chrono::seconds(5)),
            "both pools must start with one ready worker");
    assign_pid_footprints(floor, (overshare ? 80 : 45) * mib, &mutex, &bytes_by_pid);
    assign_pid_footprints(peer, (overshare ? 10 : 45) * mib, &mutex, &bytes_by_pid);

    const auto evicted = coordinator.run_eviction_cycle(true);
    const auto floor_ready = floor->ready_workers();
    const auto peer_ready = peer->ready_workers();
    const std::string counters = metrics.render_prometheus_text();
    peer->stop_and_join();
    floor->stop_and_join();
    std::cout << mode << " evicted=" << evicted << " ready=" << floor_ready << "," << peer_ready
              << std::endl;
    require(floor_ready == 1, "protected floor changed");
    const std::string peer_counter =
        "capsid_pool_evictions_total{app=\"small\",tenant=\"b\",reason=\"global_lru\"} 1";
    if (overshare) {
        require(evicted == 0 && peer_ready == 1 && counters.find(peer_counter) == std::string::npos,
                "section 7.2 forbids global LRU while a resident tenant exceeds its share");
    } else {
        require(evicted == 1 && peer_ready == 0 && counters.find(peer_counter) != std::string::npos,
                "all tenants within share: reclaim the eligible peer and count one global LRU");
    }
    return 0;
}
