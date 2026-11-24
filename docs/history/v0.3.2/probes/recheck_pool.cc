// Reuses the real worker factory and pool setup from the CTest fixture.
#define main original_elastic_pool_test_main
#include "test_host_elastic_pool.cc"
#undef main
using namespace std::chrono_literals;

int main(int argc, char** argv) {
    if (argc != 3)
        return 2;
    const std::string mode = argv[2];
    auto opts = cold_pool_options();
    opts.min_ready = 1;
    opts.workers = 2;
    WorkerCapacityLedger ledger(2, 0);
    require(ledger.reserve_fresh("cold", 1), "reserve");
    ledger.commit_fresh("cold");
    std::promise<void> entered;
    std::promise<void> release;
    auto gate = release.get_future();
    std::atomic<unsigned> calls{0};
    auto spawn = spawn_factory(argv[1], 5000, kBurstBundle);
    std::atomic<unsigned> exit_callbacks{0};
    std::promise<void> exit_delivered;
    std::promise<void> quarantined;
    opts.on_quarantine = [&] { quarantined.set_value(); };
    opts.on_worker_exited = [&](const WorkerExecutor*) { ++exit_callbacks; };
    opts.factory = [&](capsid_worker** out, std::string* err) {
        unsigned n = ++calls;
        if (mode == "quarantine" && n > 1) {
            *err = "controlled failure";
            return false;
        }
        if (mode == "success" && n == 2) {
            entered.set_value();
            gate.wait();
        }
        return spawn(out, err);
    };
    opts.acquire_elastic_slot = [&] {
        if (mode == "admission") {
            entered.set_value();
            gate.wait();
            return false;
        }
        return ledger.try_acquire_elastic("cold");
    };
    opts.release_elastic_slot = [&] { ledger.release_elastic("cold", 1); };
    std::string error;
    auto pool = GenerationPool::create(std::move(opts), &error);
    require(pool != nullptr, error);
    auto floor = pool->current_worker_snapshot();
    if (mode == "success") {
        pool->set_shared_event_sink(
            [&](std::shared_ptr<WorkerExecutor> target, capsid::host::WorkerEvent event) {
                if (target == floor && event.type == capsid::host::WorkerEvent::Type::kExit)
                    exit_delivered.set_value();
            });
    }
    if (mode == "admission" || mode == "success") {
        auto scaling = std::async(std::launch::async, [&] { return pool->request_scale_out(); });
        require(entered.get_future().wait_for(5s) == std::future_status::ready, "gate");
        require(pool->recycle_worker(floor.get()), "recycle");
        if (mode == "success")
            require(exit_delivered.get_future().wait_for(5s) == std::future_status::ready,
                    "old exit not forwarded");
        release.set_value();
        const auto result = scaling.get();
        bool restored =
            wait_for([&] { return pool->ready_workers() >= 1 && ledger.elastic_used() == 0; }, 2s);
        const bool peer = ledger.try_acquire_elastic("other");
        std::cout << "ready=" << pool->ready_workers() << " calls=" << calls
                  << " inflight=" << pool->scale_out_in_flight()
                  << " own_elastic=" << ledger.elastic_of("cold") << " peer=" << peer
                  << " denied=" << (result == GenerationPool::ScaleOutResult::kCapacityRejected)
                  << std::endl;
        if (peer)
            ledger.release_elastic("other");
        const unsigned notifications = exit_callbacks.load();
        std::cout << "exit observer before drain=" << notifications << std::endl;
        pool->set_shared_event_sink({});
        pool->stop_and_join();
        require(restored && peer, "failed to restore floor or return surplus capacity");
        if (mode == "success")
            require(notifications == 1, "missing old-worker exit observer");
    } else if (mode == "late") {
        auto lease = pool->acquire_worker();
        require(lease.get() == floor.get(), "missing lease");
        require(pool->recycle_worker(floor.get()), "recycle");
        require(wait_for([&] { return floor->exited(); }, 5s), "exit did not complete");
        capsid::host::Command command;
        command.type = capsid::host::CommandType::kBeginRequest;
        command.request_id = 11;
        command.method = "GET";
        command.url = "http://localhost/";
        command.end_request = true;
        lease.submit_begin(std::move(command));
        std::cout << "exited=" << floor->exited() << " pending=" << floor->pending_dispatches()
                  << " inflight=" << floor->inflight()
                  << " claims=" << floor->active_http_requests() << std::endl;
        pool->stop_and_join();
        require(floor->inflight() == 0 && floor->active_http_requests() == 0,
                "already reaped executor accepted a new request");
    } else if (mode == "quarantine") {
        require(pool->recycle_worker(floor.get()), "recycle");
        require(quarantined.get_future().wait_for(5s) == std::future_status::ready,
                "no quarantine");
        pool->stop_and_join();
        std::cout << "quarantined ready=" << pool->ready_workers() << " calls=" << calls
                  << " dead=" << (pool->state() == GenerationPool::State::kDead) << std::endl;
        require(calls == 4 && pool->state() == GenerationPool::State::kDead,
                "quarantine did not settle");
    } else if (mode == "mixed-ledger") {
        WorkerCapacityLedger counts(6, 5);
        require(counts.reserve_fresh("a", 2) && counts.reserve_fresh("b", 1), "floors");
        counts.commit_fresh("a");
        counts.commit_fresh("b");
        require(counts.reserve_replace("a", 1) && counts.reserve_replace("b", 3), "mixed");
        counts.abort_reserve("a", 1, true);
        require(counts.reserve_replace("a", 1), "reuse aborted promise");
        counts.commit_replace("b", 3, 1);
        counts.commit_replace("a", 1, 2);
        require(counts.steady_used() == 4 && counts.surge_used() == 3, "mixed swap");
        counts.release_drained("b", 1);
        counts.release_drained("a", 2);
        require(counts.surge_used() == 0 && counts.try_acquire_elastic("peer"), "mixed drain");
        pool->stop_and_join();
        std::cout << "mixed ledger settled" << std::endl;
    } else
        return 2;
    return 0;
}
