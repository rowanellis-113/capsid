// §9.4 weighted capacity ledger — the single accounting authority for the
// worker-count budget.
//
//   workers_total:            the steady-state budget. Every SERVING pool
//                             worker counts against it.
//   activation_surge_workers: the extra budget for the new-warming +
//                             old-draining OVERLAP of a replacement
//                             deploy. v1 default 0: without a surge
//                             budget a replacement deploy is refused
//                             (zero-downtime is physically impossible),
//                             never silently over-spawned.
//   absolute ceiling:         workers_total + activation_surge_workers.
//
// Every live worker process is in EXACTLY one category:
//   steady — serving (an active pool's workers),
//   surge  — overlap (a replacement pool warming, or a replaced/retired
//            pool draining).
//
// Deploy sequence (caller contract):
//   reserve_fresh / reserve_replace BEFORE any spawn — the reserve is the
//   gate and the count is held from that moment (concurrent reserves see
//   it), so a denied reserve means the operation must not proceed;
//   commit_fresh / commit_replace when the operation settles with a live
//   pool; abort_reserve when it settles without one (failed warm, failed
//   persist, refused activation).
// Retire sequence: begin_retire moves the serving pool into surge
// (draining) — the process count only shrinks, so no budget gate applies —
// and release_drained returns the count when the reaper finished.
//
// Replace accounting: the old pool keeps its steady count while it
// serves; the new pool warms under surge; the surge budget must fit the
// OVERLAP PEAK max(new, old) (the warm side, then the drain side).
// commit_replace moves the new pool to steady and the old pool to surge.
// Startup concurrency and worker count are independent limits (the
// startups-concurrent gate lives in the listener; this ledger is only the
// count).

#ifndef CAPSID_HOST_WORKER_CAPACITY_LEDGER_H
#define CAPSID_HOST_WORKER_CAPACITY_LEDGER_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace capsid::host {

class WorkerCapacityLedger {
  public:
    WorkerCapacityLedger(std::uint64_t workers_total, std::uint64_t activation_surge_workers)
        : workers_total_(workers_total), surge_workers_(activation_surge_workers) {}

    // Fresh deploy (no serving pool): reserves `pool_size` against the
    // steady-state committed budget. False = budget exhausted; nothing held.
    bool reserve_fresh(const std::string& application, std::uint64_t pool_size);

    // Replacement deploy over a SERVING pool (`pool_size` new committed
    // workers): the steady state after the swap must fit workers_total and
    // the overlap peak max(new, old) must fit the surge budget.
    bool reserve_replace(const std::string& application, std::uint64_t new_pool_size);

    // Settles a successful reserve. commit_fresh confirms the active
    // holding; commit_replace moves the new pool from surge to steady and
    // the old pool into surge (draining).
    void commit_fresh(const std::string& application);
    void commit_replace(const std::string& application, std::uint64_t new_pool_size,
                        std::uint64_t old_pool_size);

    // Rolls a reserve back when the operation settles without a live pool.
    void abort_reserve(const std::string& application, std::uint64_t pool_size, bool replacement);

    // Retire: the serving pool leaves steady for surge (draining).
    void begin_retire(const std::string& application, std::uint64_t pool_size);

    // Reaper finished: `drained` surge workers are gone.
    void release_drained(const std::string& application, std::uint64_t drained);
    void release_active(const std::string& application, std::uint64_t reaped);

    // Dynamic elastic slots (workers above minReady, up to global workersTotal).
    bool try_acquire_elastic(const std::string& application);
    void release_elastic(const std::string& application, std::uint64_t count = 1);

    // True when the App has an active serving pool (including minReady == 0).
    bool holds(const std::string& application) const;

    std::uint64_t steady_used() const;
    std::uint64_t surge_used() const;
    std::uint64_t elastic_used() const;
    std::uint64_t steady_of(const std::string& application) const;
    std::uint64_t surge_of(const std::string& application) const;
    std::uint64_t elastic_of(const std::string& application) const;

  private:
    struct Entry {
        bool active = false;
        std::uint64_t steady = 0;  // serving committed workers
        std::uint64_t surge = 0;   // overlap workers (warming/draining)
        std::uint64_t elastic = 0; // elastic runtime workers above minReady
    };
    std::map<std::string, Entry> entries_;
    std::uint64_t steady_used_ = 0;
    std::uint64_t surge_used_ = 0;
    std::uint64_t elastic_used_ = 0;
    const std::uint64_t workers_total_;
    const std::uint64_t surge_workers_;
    mutable std::mutex mutex_;
};

// One pool incarnation, including its retired executors. Reap may race a
// commit, a retire, or an aborted prepare; the stage chooses which held
// category is actually released, exactly once. Warm reserves stay owned by
// the coordinator until activation or abort settles them.
class WorkerPoolCapacityLease {
  public:
    WorkerPoolCapacityLease(WorkerCapacityLedger* ledger, std::string application,
                            std::uint64_t workers);
    void activate_fresh();
    void activate_replacement(const std::shared_ptr<WorkerPoolCapacityLease>& old);
    void abort(bool replacement);
    void begin_drain();
    void reaped();

  private:
    enum class Stage { kPrepared, kActive, kDraining, kReleased };
    void release_reaped_locked();
    WorkerCapacityLedger* ledger_;
    const std::string application_;
    const std::uint64_t workers_;
    std::mutex mutex_;
    Stage stage_ = Stage::kPrepared;
    bool reaped_ = false;
};

} // namespace capsid::host

#endif
