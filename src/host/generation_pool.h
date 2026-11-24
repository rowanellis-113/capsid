// WP-04 §8.2/§8.3: GenerationPool — one generation of one App's worker
// fleet, composed of PR-06 WorkerExecutors. The WP-05 Managed data plane
// routes requests into a pool; this class owns the fleet machinery only
// (no listener, no sessions — those belong to the listener/router layer).
//
// §8.2 responsibilities:
//   - identity: application_id, version, generation_digest (the pool key);
//   - state machine: prepared → active → draining → dead;
//   - the fleet: vector<shared_ptr<WorkerExecutor>> slots plus the
//     generation's immutable effective limits;
//   - the pool inflight counter (the load signal for scheduling);
//   - least-loaded scheduling among READY workers: load = inflight +
//     pending-client-bytes (loader hook — the WP-05 session layer owns the
//     write buffers the pool cannot see) + unhealthy penalty (reserved:
//     below the session layer "unhealthy" only means exited, and exited
//     slots are already excluded from the READY set);
//   - requests pin shared_ptr<GenerationPool> (enable_shared_from_this):
//     a request begun on a slot stays there even when the slot is
//     replaced; the replaced executor is retired and kept alive until the
//     pool dies, so a pinned request never touches freed memory;
//   - draining rejects new requests (pick_worker → nullptr; the caller
//     answers 503). The per-executor shutdown grace (kShutdownGrace) is
//     the drain deadline: on expiry the Runtime cancels the remaining
//     work on the shutdown frame and the worker exits to the reaper
//     (WP-03 semantics).
//
// §8.3 responsibilities (replacement):
//   - any worker EXIT (poisoned or clean — the pool never distinguishes)
//     removes the slot from the READY set immediately; the pool serves at
//     N-1, and at 0 pick_worker returns nullptr so the caller synthesizes
//     503 — a dead worker is never routed to;
//   - the replacement respawns through the SAME factory (same artifact,
//     same effective config) under the worker_recovery controller: crash
//     budget (max_events begins quarantine and suppresses replacement),
//     exponential backoff with jitter, per-App singleflight, and the
//     process-global startup semaphore (FairStartupPermitQueue);
//   - a generation that is no longer active never starts a replacement;
//   - the READY replacement re-enters the fleet (N again), and continuous
//     stability resets the backoff (observe_worker_stability).

#ifndef CAPSID_HOST_GENERATION_POOL_H
#define CAPSID_HOST_GENERATION_POOL_H

#include "host/worker_executor.h"
#include "host/worker_recovery.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace capsid::host {

class StartupPermitCoordinator;
class StructuredLog;
class MetricsRegistry;
class ServiceChannelBudget;
class WorkerPoolCapacityLease;
class GenerationPool;

struct GenerationPoolOptions {
    // Identity (the generation key; application_id must satisfy the
    // active-state identifier contract [a-z0-9][a-z0-9._-]{0,62},
    // generation_digest "sha256:" + 64 hex).
    std::string application_id;
    std::string version;
    std::string generation_digest;
    // Target fleet size N. create() is a N→READY barrier: it returns only
    // after every slot's worker is READY (or fails and reaps everything).
    std::uint32_t workers = 1;
    // Spawns ONE worker (spawn/load/flush; READY handshake is the
    // executor's). Shared by every slot and every replacement, so a
    // replacement is by construction the same artifact and effective
    // config as the worker it replaces (§8.3). The factory must be
    // thread-safe if concurrent pools share it.
    WorkerExecutor::WorkerFactory factory;
    std::shared_ptr<ServiceChannelBudget> channel_budget;
    std::shared_ptr<WorkerPoolCapacityLease> capacity_lease;
    // Crash budget, backoff and startup-permit lane (§8.3). Requires
    // backoff_initial_ms > 0 and backoff_maximum_ms >= backoff_initial_ms
    // (create() rejects an invalid policy).
    WorkerRecoveryPolicy recovery;
    // The generation's immutable effective limits. The WP-05 session layer
    // enforces them per request; the pool records them as policy and
    // exposes them for diagnostics.
    std::uint64_t max_inflight_per_worker = 0;
    std::uint64_t max_queued_requests = 0;
    std::uint64_t queue_header_bytes = 0;
    std::uint64_t queue_timeout_ms = 0;
    std::uint64_t max_streaming_inflight_per_worker = 0;
    std::uint64_t stream_idle_timeout_ms = 0;
    std::uint64_t write_timeout_ms = 0;
    // Per-App HTTP request body limit enforced by ManagedListener after routing.
    std::uint64_t max_request_body_bytes = 16U * 1024U * 1024U;
    // When false (fault-isolation / exit-capacity observation fixtures), unexpected
    // worker exits remove the slot from READY without respawning a replacement.
    bool replace_exited_workers = true;
    // §8.2 drain deadline for wait()/stop_and_join(): the whole-fleet
    // bound. Per-request cancellation is the executor's shutdown grace.
    std::chrono::milliseconds drain_deadline = std::chrono::seconds(10);
    // §8.2 load term "bytes awaiting client consumption": the pool owns no
    // client sockets, so the session layer supplies this per executor
    // (0 when unset). Keyed by the executor identity, NOT the slot index:
    // a replacement installs a different executor into the same slot, and
    // the session layer accounts per executor. Called under the pool mutex
    // (pick_worker), so the loader must never block on the pool.
    std::function<std::uint64_t(const WorkerExecutor*)> client_bytes_loader;
    // WP-05 §9.2: the pool is the sole drainer of every slot executor's
    // event queue, so events other consumers care about (the listener's
    // sessions) are forwarded here. Called on the pump thread, under the
    // pool mutex: the sink must be cheap and must never take a lock that
    // the pool could also take while the io thread holds it. kExit events
    // are forwarded too — the listener uses them to fail the requests
    // pinned to a dead worker (the executor emits no per-request failure
    // for a worker exit); the pool still handles the replacement.
    // Ordering: single producer (the pump), FIFO per executor, and kExit is
    // always the last event of its executor.
    std::function<void(const WorkerExecutor*, WorkerEvent)> event_sink;
    std::function<void(std::shared_ptr<WorkerExecutor>, WorkerEvent)> shared_event_sink;
    // §9.4 reaper-finished notification: called exactly once, on the pump
    // thread, after the pool entered kDead and its workers were reaped
    // (finalize_drain) — the "reaper completed" instant after which the
    // old pool's capacity count may be released. Must be cheap and
    // non-blocking (the ledger release runs here); never take the pool
    // mutex from the callback.
    std::function<void()> on_drain_complete;
    // Direction A (dual-engine resolution): the pool is the ONLY recovery
    // engine, so it owns the process-global fair startup-permit queue for
    // its replacement spawns (design §10.5.6). The Host wires one
    // coordinator instance shared by every pool and every deploy path;
    // null disables the queue (replacements start immediately).
    StartupPermitCoordinator* startup_permits = nullptr;
    // M2 item 7: the process-wide structured log and metrics registry
    // (design §12). Null disables event logging/metrics on this path.
    StructuredLog* log = nullptr;
    MetricsRegistry* metrics = nullptr;
    // Direction A lifecycle observers: the Host's worker map is a pure
    // observer (it never destroys workers — the executor's worker thread
    // is the sole reaper), so the pool reports worker identity changes
    // through these callbacks instead of exposing ownership.
    // on_worker_started fires when an executor became READY (create /
    // create_adopted / install_replacement — pool thread or spawn
    // thread, outside the pool mutex). Drain completion waits for this
    // observer to return; its executor remains alive during the call.
    // on_worker_exited fires when an
    // executor's kExit is processed (pump thread, under the pool mutex).
    // on_quarantine fires once on the pump, outside the pool mutex,
    // after the crash budget is exhausted and routing/replacement stops.
    // It must enqueue management work without waiting for an App lock or
    // I/O. The optional completion flag below gates quarantine shutdown.
    // All observers must be cheap; started/exited must not take the pool
    // mutex (exited still runs under it).
    std::function<void(const WorkerExecutor*)> on_worker_started;
    std::function<void(const WorkerExecutor*)> on_worker_exited;
    std::function<void()> on_quarantine;
    // Optional asynchronous durable-write barrier. on_quarantine only
    // queues the write (outside mutex_); the pool waits for this flag
    // before issuing quarantine shutdowns. Explicit owner drain may
    // supersede that wait, so abort/retire never joins a management job.
    std::shared_ptr<const std::atomic<bool>> quarantine_completion;
    // Optional notification observer, called on the executor thread before
    // it wakes the pump and without the pool mutex. It must not call back
    // into the pool. Also permits controlled teardown interleavings.
    std::function<void()> on_executor_notify;
    // Tie-break between equal-load READY slots using a rotating start index
    // instead of always preferring slot 0. Enabled by LocalDevHost for shard-like
    // round-robin parity; Managed/elastic pools keep lowest-slot consolidation.
    bool round_robin_tie_break = false;
    // Elastic pool configuration (Stage 2): when has_min_ready is false,
    // min_ready defaults to workers (fixed N/N pool). idle_timeout_ms == 0
    // disables idle reclamation.
    bool has_min_ready = false;
    std::uint32_t min_ready = 0;
    std::uint64_t idle_timeout_ms = 0;
    std::string tenant_id;
    // Two-tier elastic capacity callbacks (invoked outside the pool mutex).
    // When acquire_worker_slot is set, it overrides acquire_elastic_slot and
    // can populate *rejection_reason (e.g. "tenant_max_workers").
    std::function<bool()> acquire_elastic_slot;
    std::function<bool(const GenerationPool* requester, std::string* rejection_reason)>
        acquire_worker_slot;
    std::function<void()> release_elastic_slot;
    std::function<void(const std::string& reason)> on_eviction;
    // Optional hook run outside the pool mutex right before forced shutdown on an
    // expired drain deadline with non-idle slots (lets listeners flush kCancel
    // before kShutdown).
    std::function<void()> before_forced_shutdown;
};

// See the file comment for the full contract.
class GenerationPool : public std::enable_shared_from_this<GenerationPool> {
  public:
    enum class State { kPrepared, kActive, kDraining, kDead };

    // N→READY barrier: spawns N workers through the factory, installs the
    // event notifier and starts the pump. Returns nullptr (with *error)
    // when any spawn fails — everything already started is reaped before
    // the call returns. The pool is kActive on success.
    static std::shared_ptr<GenerationPool> create(GenerationPoolOptions options,
                                                  std::string* error);

    // WP-05 §9.2: pool over an ALREADY-WARMED fleet. `warmed` is exactly
    // options.workers pre-warmed workers whose READY handshake (including
    // the compatibility check) was consumed by the adopter — the Managed
    // coordinator's warm-up. Each is wrapped via WorkerExecutor::adopt();
    // nothing is respawned. The factory stays REQUIRED for §8.3
    // replacements (same artifact, same effective config). On failure every
    // warmed worker is destroyed, including the unvisited suffix after a
    // failed adopt (adopted ones through their executors), and nullptr is
    // returned — ownership never transfers back to the caller on failure.
    static std::shared_ptr<GenerationPool> create_adopted(GenerationPoolOptions options,
                                                          std::vector<capsid_worker*> warmed,
                                                          std::string* error);

    ~GenerationPool(); // request_drain() + wait()

    GenerationPool(const GenerationPool&) = delete;
    GenerationPool& operator=(const GenerationPool&) = delete;

    // ---- lifecycle -----------------------------------------------------

    // §8.2 drain: no new requests; every slot gets its shutdown command.
    // The pump reaps the fleet (executor threads exit; retired executors
    // are reaped too) and completes the drain when nothing is left.
    void request_drain(std::optional<std::chrono::milliseconds> deadline_override = std::nullopt);
    // Immediate shutdown without recording forced drain cancellations (used for
    // parent stop / destructor teardown rather than begin_drain).
    void request_stop();
    // Blocks until the pool is kDead (drain completed, every executor
    // thread joined, every replacement finished or abandoned). Idempotent.
    bool wait(std::string* error);
    // request_drain() + wait(). Safe on a never-created pool (nullptr
    // from create()) — the facade returns early.
    void stop_and_join();

    // ---- scheduling (§8.2) ---------------------------------------------

    // Short-lived RAII dispatch claim protecting an acquired READY executor
    // from idle eviction or shutdown between worker selection and kBeginRequest
    // submission.
    class DispatchLease {
      public:
        DispatchLease() = default;
        explicit DispatchLease(std::shared_ptr<WorkerExecutor> executor) noexcept
            : executor_(std::move(executor)) {
            if (executor_) {
                executor_->acquire_dispatch();
            }
        }
        ~DispatchLease() { release(); }
        DispatchLease(const DispatchLease&) = delete;
        DispatchLease& operator=(const DispatchLease&) = delete;
        DispatchLease(DispatchLease&& other) noexcept : executor_(std::move(other.executor_)) {}
        DispatchLease& operator=(DispatchLease&& other) noexcept {
            if (this != &other) {
                release();
                executor_ = std::move(other.executor_);
            }
            return *this;
        }
        void release() noexcept {
            if (executor_) {
                executor_->release_dispatch();
                executor_.reset();
            }
        }
        // Atomically transfers this dispatch claim to an inflight kBeginRequest
        // on the leased executor so occupancy never drops to zero mid-handoff.
        bool submit_begin(Command command) {
            if (executor_) {
                std::shared_ptr<WorkerExecutor> target = std::move(executor_);
                return target->submit_begin_from_dispatch(std::move(command));
            }
            return false;
        }
        const std::shared_ptr<WorkerExecutor>& executor() const noexcept { return executor_; }
        WorkerExecutor* get() const noexcept { return executor_.get(); }
        WorkerExecutor* operator->() const noexcept { return executor_.get(); }
        explicit operator bool() const noexcept { return executor_ != nullptr; }

      private:
        std::shared_ptr<WorkerExecutor> executor_;
    };

    // Least-loaded READY slot (tie: lowest slot index), claiming a short-lived
    // DispatchLease under mutex_ unless at_max_inflight is reached.
    DispatchLease acquire_worker(bool* at_max_inflight = nullptr);

    // Least-loaded READY slot (tie: lowest slot index). Returns nullptr
    // when the generation is not kActive or no worker is READY — the
    // caller synthesizes 503 and must NOT route anywhere. The returned
    // executor stays alive (pool pinning) even if its slot is replaced.
    WorkerExecutor* pick_worker();

    // ---- direction A: active-health recycle -----------------------------

    // The first READY slot's executor — the active-health probe target
    // (probes the pool's lowest READY slot). nullptr when the generation is not
    // kActive or no worker is READY. This is a borrowed pointer: the caller must prevent
    // the pool from completing a drain while using it. Concurrent users
    // should take the independently owned snapshot below instead.
    WorkerExecutor* current_worker();
    // Pins the selected executor independently of the pool's slots. A
    // concurrent completed drain can clear those slots while the caller
    // is blocked, even if the caller still owns the pool itself.
    std::shared_ptr<WorkerExecutor> current_worker_snapshot();
    // M2 item 6 (§7.4) recycle entry: the supervisor probes the worker
    // THROUGH its executor and calls this on consecutive failed verdicts.
    // The pool records a kHealthRecycle instability (the shared budget —
    // §10.5.2), and either begins quarantine (on_quarantine + self
    // drain) or schedules a replacement AND requests the old worker's
    // shutdown — the worker is still ALIVE, so only the pool may retire
    // it (the executor's worker thread owns the destroy; the supervisor
    // never holds a capsid_worker*). Returns true when the recycle was
    // recorded (quarantine or replacement scheduled); false when the
    // target is not a READY slot of this pool (stale — the caller
    // re-syncs without counting).
    bool recycle_worker(WorkerExecutor* target);

    // ---- elastic scale-out & eviction (Stage 2/3) -----------------------

    enum class ScaleOutResult {
        kSpawnInFlight,
        kAtAppMax,
        kCapacityRejected,
        kNotActive,
    };

    struct WorkerSlotSnapshot {
        std::size_t slot_index = 0;
        std::int64_t pid = 0;
        std::string cgroup_path;
        std::uint64_t started_at_ms = 0;
        std::uint64_t last_active_at_ms = 0;
        bool idle = false;
        bool elastic_held = false;
        bool evictable = true;
    };

    std::uint32_t min_ready() const noexcept { return min_ready_; }
    std::uint32_t max_workers() const noexcept { return options_.workers; }
    std::uint64_t idle_timeout_ms() const noexcept { return options_.idle_timeout_ms; }
    std::string_view tenant_id() const noexcept {
        return options_.tenant_id.empty() ? std::string_view(options_.application_id)
                                          : std::string_view(options_.tenant_id);
    }
    bool is_elastic() const noexcept { return min_ready_ < options_.workers; }
    ScaleOutResult request_scale_out(std::string* rejection_reason = nullptr);
    bool scale_out_in_flight() const;
    std::size_t active_or_spawning_workers() const;
    std::vector<WorkerSlotSnapshot> snapshot_workers() const;
    // Monotonic count of drain-completion checks evaluated by the pump thread.
    std::uint64_t drain_check_cycles() const noexcept {
        return drain_check_cycles_.load(std::memory_order_acquire);
    }
    void set_scale_callbacks(std::function<void()> on_worker_ready,
                             std::function<void(const std::string& reason)> on_scale_out_failed);
    bool evict_idle_worker(std::uint64_t min_residency_ms, const std::string& reason,
                           bool require_elastic_held = false,
                           std::int64_t* evicted_pid_out = nullptr);

    // ---- §9.2 event bridge ----------------------------------------------

    // Installs the listener's event sink on this pool: a mutex-guarded
    // swap, and the pump reads options_.event_sink under the same mutex,
    // so the sink never changes mid-drain. Exactly one consumer owns a
    // pool's fan-out at a time — the ManagedListener wires every pool in
    // its snapshot at start(), and the Managed coordinator wires pools it
    // activates after the listener is already running.
    // Precondition: the pool has NO in-flight requests (a request pinned
    // before wiring would lose its response events and hang) — wire before
    // the pool starts serving traffic.
    void set_event_sink(std::function<void(const WorkerExecutor*, WorkerEvent)> sink);
    void
    set_shared_event_sink(std::function<void(std::shared_ptr<WorkerExecutor>, WorkerEvent)> sink);

    // ---- diagnostics / load signal -------------------------------------

    // Total requests owned by the fleet (active slots + retired executors
    // still draining their pinned requests).
    std::uint64_t inflight() const;
    // Live READY capacity: N after a full fleet, N-1 while a replacement
    // is in flight, 0 when the fleet is exhausted.
    std::uint32_t ready_workers() const;
    // One live READY process for render-time metrics, or zero. Copies
    // only cached identity while the slot is pinned under mutex_.
    std::int64_t worker_process_id() const;
    std::shared_ptr<WorkerPoolCapacityLease> capacity_lease() const {
        return options_.capacity_lease;
    }
    // The generation's immutable per-worker inflight ceiling (0 = the
    // session layer imposes no cap beyond the worker's own limit).
    std::uint64_t max_inflight_per_worker() const { return options_.max_inflight_per_worker; }
    std::uint64_t max_queued_requests() const { return options_.max_queued_requests; }
    std::uint64_t queue_header_bytes() const { return options_.queue_header_bytes; }
    std::uint64_t queue_timeout_ms() const { return options_.queue_timeout_ms; }
    std::uint64_t max_streaming_inflight_per_worker() const {
        return options_.max_streaming_inflight_per_worker;
    }
    std::uint64_t stream_idle_timeout_ms() const { return options_.stream_idle_timeout_ms; }
    std::uint64_t write_timeout_ms() const { return options_.write_timeout_ms; }
    std::uint64_t max_request_body_bytes() const { return options_.max_request_body_bytes; }
    std::vector<capsid::WorkerExitStatus> worker_exit_statuses() const;
    std::uint64_t forced_drain_cancellations() const {
        return forced_drain_cancellations_.load(std::memory_order_acquire);
    }
    // The configured fleet size (the count the ledger reserved for this
    // pool; distinct from ready_workers(), which can drop below it while
    // the generation is reaping). PR-10 §9.4.
    std::uint32_t configured_workers() const { return options_.workers; }
    State state() const;
    std::string_view application_id() const;
    std::string_view version() const;
    std::string_view generation_digest() const;

  private:
    explicit GenerationPool(GenerationPoolOptions options);
    // Direction A: the crash budget ran out. Emit the quarantine log and
    // metrics, fire the on_quarantine observer (the Host writes the
    // tombstone), and request this pool's own drain — a quarantined
    // generation never spawns again and its workers exit to the reaper.
    void begin_quarantine();
    void finish_quarantine_locked();
    // Record one instability through the recovery controller; on
    // quarantine the pool drains itself, on schedule_replacement the slot
    // is armed. Shared by handle_executor_exit and recycle_worker.
    // Caller holds mutex_. `reason` names the replacement for the
    // kWorkerReplaced log ("crashed" / "unhealthy").
    void record_instability(std::size_t slot, WorkerInstabilityObservation observation,
                            const char* reason);

    void pump_loop();
    // Non-owning executor callback. wait() joins every producer before the
    // pool's members are destroyed, including aborted adopted fleets.
    void notify_executor_event();
    // kExit on `slot`: remove from the READY set, run the recovery
    // controller, schedule a replacement when the budget allows. Returns
    // true when an elastic slot was released and release_elastic_slot must be
    // invoked outside mutex_.
    bool handle_executor_exit(std::size_t slot);
    bool forward_executor_events_locked(const std::shared_ptr<WorkerExecutor>& executor);
    void schedule_deferred_floor_locked();
    // Spawns the replacement worker (startup permit → factory → READY →
    // slot swap). Runs on its own thread so the pump never blocks.
    void run_replacement(std::size_t slot);
    // Swap a READY replacement into `slot` (pump or replacement thread).
    void install_replacement(std::size_t slot, std::shared_ptr<WorkerExecutor> replacement);
    // kDead + reap everything (slots and retired executors), outside the
    // pool mutex. Pump-only; called with the mutex released.
    void finalize_drain();
    bool fleet_exited() const; // slots + retired, under mutex_
    // Any replacement scheduled (a due present) — the pump's no-deadline
    // wait predicate, under mutex_.
    bool due_pending_locked() const;
    std::shared_ptr<WorkerExecutor>
    select_best_worker_locked(std::size_t* best_index_out, bool enforce_max_inflight,
                              bool* saw_ready_worker_out = nullptr) const;
    // Settles elastic accounting when shrinking a slot above min_ready_: if
    // the victim itself was not marked elastic_held (floor worker evicted as
    // oldest LRU), transfers floor ownership from a remaining elastic_held
    // slot under mutex_ and returns true so the caller frees one slot.
    struct Slot;
    bool settle_evicted_slot_elastic_locked(Slot& victim);

    GenerationPoolOptions options_;
    WorkerRecoveryPolicy policy_; // validated copy
    GenerationRecoveryState recovery_state_;
    ServiceLifecycleState lifecycle_; // kActive for the pool's life

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    struct Slot {
        std::shared_ptr<WorkerExecutor> executor;
        bool ready = false; // READY set membership (§8.3)
        bool replacement_in_flight = false;
        bool scale_out_spawn = false;
        bool elastic_held = false;
        bool needs_replacement = false;
        // True once the pool itself requested this worker's shutdown: its
        // later EXIT is an expected lifecycle event (no crash budget, no
        // replacement) instead of an unexpected failure.
        bool shutdown_issued = false;
        // Direction A: the last instability's replacement reason, read by
        // install_replacement for the kWorkerReplaced message ("crashed"
        // for an unexpected exit, "unhealthy" for a health recycle).
        std::string replacement_reason;
    };
    std::uint32_t min_ready_ = 0;
    std::vector<Slot> slots_;
    // Rotating tie-breaker when multiple ready workers share the same minimum load.
    std::size_t next_pick_index_ = 0;
    // Replaced (dead or asynchronously reaping) executors kept alive until
    // exit + drain complete (§8.2 pinning / §8.3 async reaping buffer).
    std::vector<std::shared_ptr<WorkerExecutor>> retired_;
    // When an external contract observer installs options_.on_worker_started
    // (bare const WorkerExecutor*), keep exited retired executors pinned here
    // (after removing from retired_ and joining) until finalize_drain() finishes
    // running on_drain_complete. Production paths leave on_worker_started unset.
    std::vector<std::shared_ptr<WorkerExecutor>> observed_retired_;
    std::function<void()> on_worker_ready_;
    std::function<void(const std::string& reason)> on_scale_out_failed_;
    // Slots covered by a scheduled-or-running replacement: the per-App
    // singleflight counter the recovery controller sees.
    std::uint64_t replacements_scheduled_ = 0;
    // Threads actually spawning (drives drain completion; the pump never
    // finalizes while one is running).
    std::uint64_t replacements_in_flight_ = 0;
    // Concurrent synchronous slot-admission calls running outside mutex_.
    std::uint64_t scale_admissions_in_flight_ = 0;
    std::atomic<std::uint64_t> drain_check_cycles_{0};
    // Slot → spawn due time (monotonic ms) for the exponential backoff.
    std::vector<std::uint64_t> replacement_due_ms_;
    bool events_pending_ = false;
    bool quarantine_notification_pending_ = false;
    bool quarantine_waiting_ = false;
    // Atomic mirror of "state_ == kActive" for the startup-permit wait
    // abort predicate, which must not take the pool mutex (direction A:
    // a replacement thread abandons its permit wait when the drain
    // begins, so the pump's replacements_in_flight_ == 0 wait can never
    // deadlock behind a permit nobody will hand out).
    std::atomic<bool> active_flag_ = true;

    State state_ = State::kPrepared;
    std::optional<std::chrono::steady_clock::time_point> drain_until_;
    std::uint64_t stable_since_ms_ = 0;         // continuous-READY base
    std::uint32_t ready_workers_locked() const; // caller holds mutex_
    struct ReplacementWorker {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> finished;
    };
    std::thread pump_thread_;
    std::vector<ReplacementWorker> replacement_threads_;
    std::vector<capsid::WorkerExitStatus> final_exit_statuses_;
    bool count_forced_drain_ = true;
    bool before_forced_shutdown_ran_ = false;
    std::atomic<std::uint64_t> forced_drain_cancellations_{0};
    std::once_flag wait_once_;
};

} // namespace capsid::host

#endif
