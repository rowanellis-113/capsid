// Independent review fixture linked against the same libraries as test-host-elastic-pool.
#define main original_elastic_pool_test_main
#include "test_host_elastic_pool.cc"
#undef main

namespace {
using namespace std::chrono_literals;

void check_health_release(const std::string& worker) {
    WorkerCapacityLedger ledger(2, 0);
    require(ledger.reserve_fresh("cold", 1), "reserve floor");
    ledger.commit_fresh("cold");
    auto opts = cold_pool_options();
    opts.min_ready = 1;
    opts.workers = 2;
    opts.factory = spawn_factory(worker, 5000, kBurstBundle);
    opts.acquire_elastic_slot = [&] { return ledger.try_acquire_elastic("cold"); };
    opts.release_elastic_slot = [&] { ledger.release_elastic("cold"); };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, error);
    auto floor = pool->current_worker_snapshot();
    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight, "expand");
    require(wait_for([&] { return pool->ready_workers() == 2; }, 5s), "second worker not ready");
    require(pool->recycle_worker(floor.get()), "health recycle refused");
    const auto ready = pool->ready_workers();
    const auto charged = ledger.elastic_of("cold");
    const bool peer = ledger.try_acquire_elastic("peer");
    if (peer)
        ledger.release_elastic("peer");
    pool->stop_and_join();
    std::cout << "ready=" << ready << " elastic=" << charged << " peer=" << peer << std::endl;
    require(ready == 1 && charged == 0 && peer, "floor consumed the peer's slot");
}

void check_floor_recovery(const std::string& worker) {
    auto opts = cold_pool_options();
    opts.min_ready = 1;
    opts.workers = 2;
    std::atomic<unsigned> calls{0};
    std::promise<void> entered, release, failure;
    auto gate = release.get_future();
    auto spawn = spawn_factory(worker, 5000, kBurstBundle);
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
    require(pool != nullptr, error);
    pool->set_scale_callbacks({}, [&](const std::string&) { failure.set_value(); });
    auto floor = pool->current_worker_snapshot();
    require(pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight, "expand");
    require(entered.get_future().wait_for(5s) == std::future_status::ready, "factory not entered");
    require(pool->recycle_worker(floor.get()), "health recycle refused");
    release.set_value();
    require(failure.get_future().wait_for(5s) == std::future_status::ready,
            "missing failure callback");
    const bool restored = wait_for([&] { return pool->ready_workers() >= 1; }, 2s);
    std::cout << "ready=" << pool->ready_workers() << " calls=" << calls.load()
              << " scale_in_flight=" << pool->scale_out_in_flight() << std::endl;
    pool->stop_and_join();
    require(restored, "floor was not restored after scale-out settled");
}

void check_http_selection(const std::string& worker) {
    auto opts = cold_pool_options();
    opts.workers = 2;
    opts.min_ready = 2;
    opts.max_inflight_per_worker = 1;
    const WorkerExecutor* first = nullptr;
    opts.client_bytes_loader = [&](const WorkerExecutor* w) -> std::uint64_t {
        return first && w != first ? 2 : 0;
    };
    opts.factory = spawn_factory(worker, 5000, kBurstBundle);
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, error);
    bool okay;
    {
        auto a = pool->acquire_worker();
        require(a.get() != nullptr, "first lease");
        first = a.get();
        bool full = false;
        auto b = pool->acquire_worker(&full);
        std::cout << "ready=" << pool->ready_workers() << " second_lease=" << bool(b)
                  << " at_max=" << full << std::endl;
        okay = b.get() && b.get() != first && !full;
    }
    pool->stop_and_join();
    require(okay, "second worker had a free HTTP slot");
}

void check_surge() {
    WorkerCapacityLedger ledger(4, 3);
    require(ledger.reserve_fresh("a", 2) && ledger.reserve_fresh("b", 2), "reserve two floors");
    ledger.commit_fresh("a");
    ledger.commit_fresh("b");
    require(ledger.reserve_replace("a", 1), "first shrink");
    const bool second = ledger.reserve_replace("b", 1);
    ledger.commit_replace("a", 1, 2);
    if (second)
        ledger.commit_replace("b", 1, 2);
    std::cout << "second_reserved=" << second << " surge=" << ledger.surge_used()
              << " configured_surge=3" << std::endl;
    require(ledger.surge_used() <= 3, "draining category exceeded the configured cap");
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3)
        return 2;
    const std::string mode = argv[2];
    if (mode == "health")
        check_health_release(argv[1]);
    else if (mode == "floor")
        check_floor_recovery(argv[1]);
    else if (mode == "selection")
        check_http_selection(argv[1]);
    else if (mode == "surge")
        check_surge();
    else
        return 2;
    return 0;
}
