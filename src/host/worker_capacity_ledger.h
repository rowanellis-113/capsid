// §9.4 weighted capacity ledger — the single accounting authority for the
// worker-count budget.
//
//   workers_total:            the steady-state budget. Every SERVING pool
//                             worker counts against it.
//   activation_surge_workers: the extra budget for the new-warming +
//                             old-draining OVERLAP of a replacement
//                             deploy. Without a surge
//                             budget a replacement deploy is refused
//                             (zero-downtime is physically impossible),
//                             never silently over-spawned.
//   reaping_workers:          bounded asynchronous reaping headroom for
//                             evicted workers and live predecessors of
//                             floor replacements until their reaper finishes.
//   absolute ceiling:         workers_total + activation_surge_workers
//                             + reaping_workers.
//
// Every live worker process is in EXACTLY one category:
//   steady  — guaranteed serving slots (an active pool's minReady floor),
//   elastic — dynamic on-demand slots above minReady (within workers_total),
//   surge   — overlap (a replacement pool warming, or a replaced/retired
//             pool draining),
//   reaping — exiting workers whose serving slot can already be reused.
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
// StartupPermitCoordinator gate is owned by the managed Host and shared with
// ManagedAdminBackend and GenerationPool; this ledger tracks only the counts).

#ifndef CAPSID_HOST_WORKER_CAPACITY_LEDGER_H
#define CAPSID_HOST_WORKER_CAPACITY_LEDGER_H

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace capsid::host {

class WorkerCapacityLedger {
  public:
    WorkerCapacityLedger(std::uint64_t workers_total, std::uint64_t activation_surge_workers,
                         std::optional<std::uint64_t> reaping_workers = std::nullopt)
        : workers_total_(workers_total), surge_workers_(activation_surge_workers),
          reaping_workers_(reaping_workers.value_or(std::max<std::uint64_t>(
              1, activation_surge_workers > 0 ? activation_surge_workers
                                              : std::max<std::uint64_t>(1, workers_total / 4)))) {}

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

    // Bounded asynchronous reaping handoff: transfers one held elastic slot
    // into the bounded reaping set so a replacement can spawn in parallel.
    // Returns false (keeping the elastic slot charged until process exit)
    // when reaping_used() == reaping_limit().
    bool handoff_elastic_to_reaping(const std::string& application);
    bool try_acquire_reaping(const std::string& application);
    void finish_reaping(const std::string& application, std::uint64_t count = 1);
    bool reaping_available() const;

    // True when the App has an active serving pool (including minReady == 0).
    bool holds(const std::string& application) const;

    std::uint64_t steady_used() const;
    std::uint64_t surge_used() const;
    std::uint64_t elastic_used() const;
    std::uint64_t reaping_used() const;
    std::uint64_t reaping_limit() const noexcept { return reaping_workers_; }
    std::uint64_t steady_of(const std::string& application) const;
    std::uint64_t surge_of(const std::string& application) const;
    std::uint64_t elastic_of(const std::string& application) const;
    std::uint64_t reaping_of(const std::string& application) const;

  private:
    std::uint64_t available_steady_or_elastic_locked() const;
    struct Entry {
        bool active = false;
        std::uint64_t steady = 0;               // serving committed workers
        std::uint64_t surge = 0;                // overlap workers (warming/draining)
        std::uint64_t elastic = 0;              // elastic runtime workers above minReady
        std::uint64_t reaping = 0;              // exiting workers using reaping headroom
        bool replace_pending = false;           // in-flight replacement reservation
        std::uint64_t replace_growth = 0;       // additional steady slots promised on commit
        std::uint64_t replace_surge_growth = 0; // additional draining surge promised on shrink
    };
    std::map<std::string, Entry> entries_;
    std::uint64_t steady_used_ = 0;
    std::uint64_t surge_used_ = 0;
    std::uint64_t elastic_used_ = 0;
    std::uint64_t reaping_used_ = 0;
    std::uint64_t pending_steady_growth_ = 0;
    std::uint64_t pending_surge_growth_ = 0;
    const std::uint64_t workers_total_;
    const std::uint64_t surge_workers_;
    const std::uint64_t reaping_workers_;
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
