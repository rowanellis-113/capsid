// GenerationPool implementation — see generation_pool.h. One pump thread
// owns every slot executor's event drain; a replacement spawn runs on its
// own thread so the pump never blocks on a spawn or on the process-global
// startup-permit queue (direction A: the queue is the Host-wired
// StartupPermitCoordinator, shared with the deploy path).

#include "host/generation_pool.h"

#include "host/managed_admin_backend.h"
#include "host/metrics.h"
#include "host/structured_log.h"
#include "host/worker_cgroup.h"
#include "host/worker_recovery.h"
#include "host/worker_capacity_ledger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <limits>
#include <utility>

namespace capsid::host {

namespace {

using SteadyClock = std::chrono::steady_clock;

constexpr std::uint64_t kNoDue = 0;

std::uint64_t steady_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(SteadyClock::now().time_since_epoch())
            .count());
}

// M2 item 7 (design §12.2): single write path for every pool event.
// Null log (unit fixtures without the process-wide instance) is a no-op.
void emit_log(StructuredLog* log, LogLane lane, LogFields fields) {
    if (log != nullptr) {
        log->log(lane, std::move(fields));
    }
}

void count_event(MetricsRegistry* metrics, const std::string& event, const std::string& app,
                 const std::string& generation) {
    if (metrics != nullptr) {
        metrics->count_worker_event(event, app, generation);
    }
}

} // namespace

GenerationPool::GenerationPool(GenerationPoolOptions options)
    : options_(std::move(options)), policy_(options_.recovery),
      min_ready_(options_.has_min_ready ? options_.min_ready : options_.workers) {
    recovery_state_.application = options_.application_id;
    recovery_state_.generation = options_.generation_digest;
    lifecycle_.phase = ServiceLifecyclePhase::kActive;
    ActiveStateDocument document;
    document.state = ActiveServiceState::kActive;
    document.application = options_.application_id;
    document.version = options_.version;
    document.generation = options_.generation_digest;
    lifecycle_.document = document;
}

GenerationPool::~GenerationPool() {
    // Same bounded teardown as the facades: drain, then reap everything
    // (every executor thread joins; no detached thread is ever left).
    request_drain();
    std::string error;
    wait(&error);
}

std::shared_ptr<GenerationPool> GenerationPool::create(GenerationPoolOptions options,
                                                       std::string* error) {
    if (options.workers == 0) {
        if (error != nullptr) {
            *error = "generation pool requires at least one worker";
        }
        return nullptr;
    }
    const std::uint32_t min_ready = options.has_min_ready ? options.min_ready : options.workers;
    if (min_ready > options.workers) {
        if (error != nullptr) {
            *error = "generation pool minReady cannot exceed workers";
        }
        return nullptr;
    }
    if (options.application_id.empty() || options.generation_digest.empty() ||
        options.version.empty()) {
        if (error != nullptr) {
            *error = "generation pool identity is incomplete";
        }
        return nullptr;
    }
    // The recovery controller validates the full identifier contract at the
    // first instability; this cheap gate mirrors the controller's policy
    // contract (worker_recovery.cc valid_policy) so a misconfigured pool
    // fails here with a clear message instead of silently serving N-1
    // forever (a zero field would make every decision an error).
    if (options.recovery.max_events == 0U || options.recovery.window_ms == 0U ||
        options.recovery.backoff_initial_ms == 0U ||
        options.recovery.backoff_maximum_ms < options.recovery.backoff_initial_ms ||
        options.recovery.jitter_basis_points > 10000U || options.recovery.stable_reset_ms == 0U ||
        options.recovery.replacements_concurrent_per_app == 0U) {
        if (error != nullptr) {
            *error = "invalid recovery policy (mirrors the controller's "
                     "contract: max_events/window_ms/backoff_initial_ms/"
                     "stable_reset_ms/replacements_concurrent_per_app "
                     "non-zero, backoff_maximum_ms >= backoff_initial_ms, "
                     "jitter_basis_points <= 10000)";
        }
        return nullptr;
    }
    if (!options.factory) {
        if (error != nullptr) {
            *error = "generation pool requires a worker factory";
        }
        return nullptr;
    }

    std::shared_ptr<GenerationPool> pool(new GenerationPool(std::move(options)));

    // minReady→READY barrier (§8.2): spawn min_ready_ initial workers; any
    // remaining slots up to options.workers stay empty for elastic scale-out.
    pool->slots_.resize(pool->options_.workers);
    std::string spawn_error;
    for (std::uint32_t index = 0; index < pool->min_ready_; ++index) {
        std::shared_ptr<WorkerExecutor> executor(new WorkerExecutor());
        executor->set_channel_budget(pool->options_.channel_budget);
        executor->set_log_identity(pool->options_.application_id, pool->options_.generation_digest);
        // A temporary weak.lock() can become the last owner and invoke
        // the pool destructor on this executor thread, which then waits
        // for its own exit. The pool owns and joins the executor instead.
        executor->set_event_notifier([owner = pool.get()] { owner->notify_executor_event(); });
        if (!executor->start(pool->options_.factory, &spawn_error)) {
            if (error != nullptr) {
                *error = "generation pool worker " + std::to_string(index + 1) + "/" +
                         std::to_string(pool->min_ready_) + " failed to start: " + spawn_error;
            }
            // Reap exactly the started slots (including this one's partial
            // state — a failed start leaves nothing behind by contract).
            for (const Slot& slot : pool->slots_) {
                if (slot.executor != nullptr) {
                    slot.executor->stop_and_join();
                }
            }
            // The pump never started, so nothing will ever drive this pool
            // to kDead; the destructor's drain+wait must not block forever
            // on it. Mark the pool dead up front — it never served.
            pool->state_ = State::kDead;
            return nullptr;
        }
        pool->slots_[index].executor = std::move(executor);
        pool->slots_[index].ready = true;
        if (pool->options_.on_worker_started) {
            pool->options_.on_worker_started(pool->slots_[index].executor.get());
        }
    }
    pool->replacement_due_ms_.assign(pool->slots_.size(), kNoDue);
    pool->state_ = State::kActive;
    pool->stable_since_ms_ = pool->min_ready_ > 0 ? steady_ms() : 0;
    // The pump thread captures the BARE pointer, never a shared_ptr: a
    // self-held reference would keep the pool alive forever, so an
    // abandoned pool (e.g. an aborted §9.3 activation) could never be
    // destroyed — its drain would never run and every worker would leak.
    // Lifetime is safe: wait() joins the pump before the pool is freed,
    // and every pump exit runs inside finalize_drain.
    pool->pump_thread_ = std::thread([target = pool.get()] { target->pump_loop(); });
    return pool;
}

std::shared_ptr<GenerationPool> GenerationPool::create_adopted(GenerationPoolOptions options,
                                                               std::vector<capsid_worker*> warmed,
                                                               std::string* error) {
    const std::uint32_t min_ready = options.has_min_ready ? options.min_ready : options.workers;
    if (options.workers == 0 || min_ready > options.workers || warmed.size() < min_ready ||
        warmed.size() > options.workers) {
        if (error != nullptr) {
            *error = (min_ready == options.workers)
                         ? "adopted generation pool requires exactly workers pre-warmed workers"
                         : "adopted generation pool requires between minReady and workers "
                           "pre-warmed workers";
        }
        // Mismatched input: destroy every worker handed in — the caller
        // transferred ownership by calling create_adopted.
        for (capsid_worker* worker : warmed) {
            capsid_worker_destroy(worker);
            release_worker_cgroup(worker);
        }
        return nullptr;
    }
    if (options.application_id.empty() || options.generation_digest.empty() ||
        options.version.empty()) {
        if (error != nullptr) {
            *error = "generation pool identity is incomplete";
        }
        for (capsid_worker* worker : warmed) {
            capsid_worker_destroy(worker);
            release_worker_cgroup(worker);
        }
        return nullptr;
    }
    // Same recovery-policy gate as create() (worker_recovery.cc valid_policy).
    if (options.recovery.max_events == 0U || options.recovery.window_ms == 0U ||
        options.recovery.backoff_initial_ms == 0U ||
        options.recovery.backoff_maximum_ms < options.recovery.backoff_initial_ms ||
        options.recovery.jitter_basis_points > 10000U || options.recovery.stable_reset_ms == 0U ||
        options.recovery.replacements_concurrent_per_app == 0U) {
        if (error != nullptr) {
            *error = "invalid recovery policy (mirrors the controller's "
                     "contract: max_events/window_ms/backoff_initial_ms/"
                     "stable_reset_ms/replacements_concurrent_per_app "
                     "non-zero, backoff_maximum_ms >= backoff_initial_ms, "
                     "jitter_basis_points <= 10000)";
        }
        for (capsid_worker* worker : warmed) {
            capsid_worker_destroy(worker);
            release_worker_cgroup(worker);
        }
        return nullptr;
    }
    if (!options.factory) {
        if (error != nullptr) {
            *error = "generation pool requires a worker factory "
                     "(for replacements)";
        }
        for (capsid_worker* worker : warmed) {
            capsid_worker_destroy(worker);
            release_worker_cgroup(worker);
        }
        return nullptr;
    }

    std::shared_ptr<GenerationPool> pool(new GenerationPool(std::move(options)));

    // No READY barrier: the adopter consumed it. Warmed slots start READY;
    // any remaining slots up to options.workers start empty for scale-out.
    pool->slots_.resize(pool->options_.workers);
    for (std::size_t index = 0; index < warmed.size(); ++index) {
        std::shared_ptr<WorkerExecutor> executor(new WorkerExecutor());
        executor->set_channel_budget(pool->options_.channel_budget);
        executor->set_log_identity(pool->options_.application_id, pool->options_.generation_digest);
        executor->set_event_notifier([owner = pool.get()] { owner->notify_executor_event(); });
        if (!executor->adopt(warmed[index], error)) {
            // Ownership of the entire warmed fleet transferred at entry.
            // adopt() did not take this worker or the unvisited suffix;
            // destroy all of them. The adopted prefix has its own reapers.
            for (std::size_t remaining = index; remaining < warmed.size(); ++remaining) {
                capsid_worker_destroy(warmed[remaining]);
                release_worker_cgroup(warmed[remaining]);
            }
            for (const Slot& slot : pool->slots_) {
                if (slot.executor != nullptr) {
                    slot.executor->stop_and_join();
                }
            }
            pool->state_ = State::kDead; // the pump never started
            return nullptr;
        }
        pool->slots_[index].executor = std::move(executor);
        pool->slots_[index].ready = true;
        pool->slots_[index].elastic_held =
            (index >= pool->min_ready_) && static_cast<bool>(pool->options_.release_elastic_slot);
        if (pool->options_.on_worker_started) {
            pool->options_.on_worker_started(pool->slots_[index].executor.get());
        }
    }
    pool->replacement_due_ms_.assign(pool->slots_.size(), kNoDue);
    pool->state_ = State::kActive;
    pool->stable_since_ms_ = pool->min_ready_ > 0 ? steady_ms() : 0;
    // Bare pointer, same lifetime contract as create() (see above): never
    // self-hold the pool, or an aborted §9.3 activation leaks every worker.
    pool->pump_thread_ = std::thread([target = pool.get()] { target->pump_loop(); });
    return pool;
}

void GenerationPool::request_drain(std::optional<std::chrono::milliseconds> deadline_override) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (state_ == State::kDead || (state_ == State::kDraining && !quarantine_waiting_)) {
            return; // idempotent
        }
        quarantine_waiting_ = false;
        count_forced_drain_ = true;
        before_forced_shutdown_ran_ = false;
        state_ = State::kDraining;
        active_flag_.store(false, std::memory_order_relaxed);
        stable_since_ms_ = 0;
        const std::chrono::milliseconds effective_deadline =
            deadline_override.value_or(options_.drain_deadline);
        drain_until_ = SteadyClock::now() + effective_deadline;
        for (Slot& slot : slots_) {
            if (slot.executor == nullptr) {
                continue;
            }
            const bool idle = slot.executor->is_idle();
            if (idle || (effective_deadline.count() == 0 && !options_.before_forced_shutdown)) {
                if (!slot.shutdown_issued && !idle) {
                    const std::uint64_t active = slot.executor->inflight();
                    forced_drain_cancellations_.fetch_add(std::max<std::uint64_t>(1, active),
                                                          std::memory_order_acq_rel);
                }
                slot.shutdown_issued = true;
                slot.executor->request_shutdown();
            }
        }
    }
    // The pump re-runs its drain sweep; startup-permit waiters re-check
    // the pool's active flag (the abort predicate) and abandon.
    cv_.notify_all();
}

void GenerationPool::request_stop() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (state_ == State::kDead || (state_ == State::kDraining && !quarantine_waiting_)) {
            return; // idempotent
        }
        quarantine_waiting_ = false;
        count_forced_drain_ = false;
        state_ = State::kDraining;
        active_flag_.store(false, std::memory_order_relaxed);
        stable_since_ms_ = 0;
        drain_until_ = SteadyClock::now();
        for (Slot& slot : slots_) {
            if (slot.executor == nullptr) {
                continue;
            }
            slot.shutdown_issued = true;
            slot.executor->request_shutdown();
        }
    }
    cv_.notify_all();
}

void GenerationPool::stop_and_join() {
    request_drain();
    std::string error;
    wait(&error);
}

bool GenerationPool::wait(std::string* error) {
    request_drain(); // wait() implies the drain request (idempotent)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return state_ == State::kDead; });
    }
    // Every thread finished its pool-mutex work before kDead was set
    // (finalize requires replacements_in_flight_ == 0 and every slot
    // exited); joining outside the mutex is safe and never self-deadlocks.
    std::call_once(wait_once_, [this] {
        if (pump_thread_.joinable()) {
            pump_thread_.join();
        }
        for (ReplacementWorker& entry : replacement_threads_) {
            if (entry.thread.joinable()) {
                entry.thread.join();
            }
        }
        replacement_threads_.clear();
    });
    if (error != nullptr) {
        *error = "generation pool stopped";
    }
    return true;
}

std::int64_t GenerationPool::worker_process_id() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != State::kActive)
        return 0;
    for (const Slot& slot : slots_) {
        if (slot.ready && slot.executor != nullptr && slot.executor->available()) {
            return slot.executor->process_id();
        }
    }
    return 0;
}

std::shared_ptr<WorkerExecutor>
GenerationPool::select_best_worker_locked(std::size_t* best_index_out, bool enforce_max_inflight,
                                          bool* saw_ready_worker_out) const {
    if (saw_ready_worker_out != nullptr) {
        *saw_ready_worker_out = false;
    }
    const std::size_t count = slots_.size();
    if (state_ != State::kActive || count == 0) {
        return nullptr;
    }
    const std::size_t start = options_.round_robin_tie_break ? (next_pick_index_ % count) : 0;
    std::shared_ptr<WorkerExecutor> best;
    std::size_t best_index = start;
    std::uint64_t best_load = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t step = 0; step < count; ++step) {
        const std::size_t index = (start + step) % count;
        const Slot& slot = slots_[index];
        if (!slot.ready || slot.shutdown_issued || slot.executor == nullptr) {
            continue;
        }
        WorkerExecutor* candidate = slot.executor.get();
        // §8.3: a dead worker is never routed to — ready is pool-maintained
        // (set false when its kExit is processed) and available() is the
        // executor's live exit state; both must hold.
        if (!candidate->available()) {
            continue;
        }
        if (saw_ready_worker_out != nullptr) {
            *saw_ready_worker_out = true;
        }
        if (enforce_max_inflight && options_.max_inflight_per_worker != 0 &&
            candidate->active_http_requests() >= options_.max_inflight_per_worker) {
            continue;
        }
        // §8.2 load: inflight + connection_owners + pending_dispatches +
        // pending client bytes (session-layer hook; 0 when unset).
        std::uint64_t load = candidate->load_score();
        if (options_.client_bytes_loader) {
            load += options_.client_bytes_loader(candidate);
        }
        if (load < best_load) {
            best_load = load;
            best = slot.executor;
            best_index = index;
        }
    }
    if (best_index_out != nullptr) {
        *best_index_out = best_index;
    }
    return best;
}

GenerationPool::DispatchLease GenerationPool::acquire_worker(bool* at_max_inflight) {
    if (at_max_inflight != nullptr) {
        *at_max_inflight = false;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    std::size_t best_index = 0;
    bool saw_ready = false;
    std::shared_ptr<WorkerExecutor> best =
        select_best_worker_locked(&best_index, /*enforce_max_inflight=*/true, &saw_ready);
    if (!best) {
        if (at_max_inflight != nullptr && saw_ready && options_.max_inflight_per_worker != 0) {
            *at_max_inflight = true;
        }
        return DispatchLease{};
    }
    if (options_.round_robin_tie_break) {
        next_pick_index_ = (best_index + 1) % slots_.size();
    }
    return DispatchLease(std::move(best));
}

WorkerExecutor* GenerationPool::pick_worker() {
    std::unique_lock<std::mutex> lock(mutex_);
    std::size_t best_index = 0;
    std::shared_ptr<WorkerExecutor> best =
        select_best_worker_locked(&best_index, /*enforce_max_inflight=*/false);
    if (best != nullptr && options_.round_robin_tie_break) {
        next_pick_index_ = (best_index + 1) % slots_.size();
    }
    return best.get();
}

WorkerExecutor* GenerationPool::current_worker() { return current_worker_snapshot().get(); }

std::shared_ptr<WorkerExecutor> GenerationPool::current_worker_snapshot() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (state_ != State::kActive) {
        return nullptr;
    }
    for (Slot& slot : slots_) {
        if (slot.ready && slot.executor != nullptr && slot.executor->available()) {
            return slot.executor;
        }
    }
    return nullptr;
}

bool GenerationPool::recycle_worker(WorkerExecutor* target) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (state_ != State::kActive) {
        return false;
    }
    std::size_t slot = slots_.size();
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        if (slots_[index].executor != nullptr && slots_[index].executor.get() == target &&
            slots_[index].ready) {
            slot = index;
            break;
        }
    }
    if (slot == slots_.size()) {
        return false; // stale target: already removed or replaced
    }
    Slot& s = slots_[slot];
    // The worker is still ALIVE: the recycle must retire it. The
    // shutdown-issued marker makes its later EXIT an expected lifecycle
    // event, so handle_executor_exit never records it a second time (the
    // budget is charged exactly once, here). §8.3: the recycled worker
    // leaves the READY set immediately — the controller decides on the
    // capacity AFTER the removal, or a full fleet would never schedule a
    // replacement (1 >= 1).
    s.shutdown_issued = true;
    s.ready = false;
    const std::uint64_t now = steady_ms();
    std::uint32_t ready_after = 0;
    for (const Slot& other : slots_) {
        if (other.ready) {
            ++ready_after;
        }
    }
    WorkerInstabilityObservation observation;
    observation.kind = WorkerInstabilityKind::kHealthRecycle;
    observation.worker_generation = options_.generation_digest;
    observation.now_ms = now;
    observation.ready_workers_after_removal = ready_after;
    observation.target_ready_workers = min_ready_;
    observation.replacements_in_flight_for_app =
        static_cast<std::uint32_t>(replacements_scheduled_);
    observation.chosen_jitter_basis_points = 0; // deterministic v1
    count_event(options_.metrics, "unhealthy", options_.application_id, options_.generation_digest);
    record_instability(slot, observation, "unhealthy");
    bool release_elastic = false;
    std::function<void()> release_cb;
    if (!quarantine_waiting_) {
        s.executor->request_shutdown();
        if (!s.replacement_in_flight && !s.needs_replacement && ready_after >= min_ready_) {
            retired_.push_back(std::move(s.executor));
            if (settle_evicted_slot_elastic_locked(s)) {
                release_elastic = true;
                release_cb = options_.release_elastic_slot;
            }
        }
    }
    if (release_elastic && release_cb) {
        lock.unlock();
        release_cb();
    }
    return true;
}

GenerationPool::ScaleOutResult GenerationPool::request_scale_out(std::string* rejection_reason) {
    constexpr std::size_t kNoSlot = std::numeric_limits<std::size_t>::max();
    std::size_t target_slot = kNoSlot;
    bool needs_elastic = false;
    bool spawning_already = false;
    bool run_admission = false;
    std::function<bool()> acquire_cb;
    std::function<bool(const GenerationPool*, std::string*)> acquire_slot_cb;
    std::function<void()> release_cb;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (state_ != State::kActive) {
            return ScaleOutResult::kNotActive;
        }
        std::uint32_t ready_count = 0;
        std::size_t spawning_count = 0;
        std::uint32_t non_elastic_active = 0;
        for (std::size_t index = 0; index < slots_.size(); ++index) {
            const Slot& s = slots_[index];
            if (s.ready) {
                ++ready_count;
            }
            if (s.replacement_in_flight) {
                ++spawning_count;
            }
            if ((s.executor != nullptr || s.replacement_in_flight) && !s.elastic_held) {
                ++non_elastic_active;
            }
            if (target_slot == kNoSlot && !s.ready && !s.replacement_in_flight &&
                s.executor == nullptr) {
                target_slot = index;
            }
        }
        spawning_already = spawning_count > 0;
        // Cold-start singleflight: when 0 workers are READY and a wakeup
        // spawn is already running, let concurrent requests queue behind it
        // rather than launching N parallel cold starts on one burst.
        if (ready_count == 0 && spawning_already) {
            return ScaleOutResult::kSpawnInFlight;
        }
        if (target_slot == kNoSlot) {
            if (spawning_already) {
                return ScaleOutResult::kSpawnInFlight;
            }
            if (rejection_reason != nullptr) {
                *rejection_reason = "app_max_workers";
            }
            if (options_.metrics != nullptr) {
                options_.metrics->count_pool_rejection(options_.application_id,
                                                       std::string(tenant_id()), "app_max_workers");
            }
            return ScaleOutResult::kAtAppMax;
        }
        needs_elastic = non_elastic_active >= min_ready_;
        slots_[target_slot].replacement_in_flight = true;
        slots_[target_slot].scale_out_spawn = true;
        ++replacements_scheduled_;
        acquire_cb = options_.acquire_elastic_slot;
        acquire_slot_cb = options_.acquire_worker_slot;
        release_cb = options_.release_elastic_slot;
        run_admission = needs_elastic && (acquire_slot_cb || acquire_cb);
        if (!run_admission) {
            slots_[target_slot].elastic_held = false;
            replacement_due_ms_[target_slot] = steady_ms();
            cv_.notify_all();
            return ScaleOutResult::kSpawnInFlight;
        }
        ++scale_admissions_in_flight_;
    }

    std::string reason = "host_capacity_exhausted";
    const bool acquired = acquire_slot_cb ? acquire_slot_cb(this, &reason) : acquire_cb();
    if (!acquired) {
        if (reason.empty()) {
            reason = "host_capacity_exhausted";
        }
        bool active_spawn = false;
        bool not_active = false;
        std::function<void(const std::string&)> failed_cb;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (scale_admissions_in_flight_ > 0) {
                --scale_admissions_in_flight_;
            }
            if (target_slot < slots_.size()) {
                Slot& s = slots_[target_slot];
                s.replacement_in_flight = false;
                s.scale_out_spawn = false;
            }
            if (replacements_scheduled_ > 0) {
                --replacements_scheduled_;
            }
            schedule_deferred_floor_locked();
            not_active = state_ != State::kActive;
            active_spawn = !not_active && replacements_scheduled_ > 0;
            if (not_active || !active_spawn) {
                failed_cb = on_scale_out_failed_;
            }
            cv_.notify_all();
        }
        if (failed_cb) {
            failed_cb(not_active ? "pool_draining" : reason);
        }
        if (not_active) {
            return ScaleOutResult::kNotActive;
        }
        if (active_spawn) {
            return ScaleOutResult::kSpawnInFlight;
        }
        if (rejection_reason != nullptr) {
            *rejection_reason = reason;
        }
        if (options_.metrics != nullptr) {
            options_.metrics->count_pool_rejection(options_.application_id,
                                                   std::string(tenant_id()), reason);
        }
        return ScaleOutResult::kCapacityRejected;
    }

    bool roll_back_elastic = false;
    std::function<void(const std::string&)> failed_cb;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (scale_admissions_in_flight_ > 0) {
            --scale_admissions_in_flight_;
        }
        if (state_ != State::kActive || target_slot >= slots_.size()) {
            if (target_slot < slots_.size()) {
                slots_[target_slot].replacement_in_flight = false;
                slots_[target_slot].scale_out_spawn = false;
            }
            if (replacements_scheduled_ > 0) {
                --replacements_scheduled_;
            }
            roll_back_elastic = true;
            failed_cb = on_scale_out_failed_;
            cv_.notify_all();
        } else {
            slots_[target_slot].elastic_held = static_cast<bool>(release_cb);
            replacement_due_ms_[target_slot] = steady_ms();
            cv_.notify_all();
            return ScaleOutResult::kSpawnInFlight;
        }
    }
    if (roll_back_elastic && release_cb) {
        release_cb();
    }
    if (failed_cb) {
        failed_cb("pool_draining");
    }
    return ScaleOutResult::kNotActive;
}

bool GenerationPool::scale_out_in_flight() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Slot& slot : slots_) {
        if (slot.replacement_in_flight && slot.scale_out_spawn) {
            return true;
        }
    }
    return false;
}

std::size_t GenerationPool::active_or_spawning_workers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != State::kActive) {
        return 0;
    }
    std::size_t count = 0;
    for (const Slot& slot : slots_) {
        if ((slot.ready && !slot.shutdown_issued) || slot.replacement_in_flight) {
            ++count;
        }
    }
    return count;
}

std::vector<GenerationPool::WorkerSlotSnapshot> GenerationPool::snapshot_workers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WorkerSlotSnapshot> out;
    if (state_ == State::kPrepared || state_ == State::kDead) {
        return out;
    }
    out.reserve(slots_.size() + retired_.size());
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        const Slot& slot = slots_[index];
        if (slot.executor == nullptr || slot.executor->exited()) {
            continue;
        }
        WorkerSlotSnapshot snap;
        snap.slot_index = index;
        snap.pid = slot.executor->process_id();
        snap.cgroup_path = slot.executor->cgroup_path();
        snap.started_at_ms = slot.executor->started_at_ms();
        snap.last_active_at_ms = slot.executor->last_active_at_ms();
        snap.evictable = (state_ == State::kActive) && slot.ready && !slot.shutdown_issued &&
                         !slot.replacement_in_flight;
        snap.idle = snap.evictable && slot.executor->is_idle();
        snap.elastic_held = slot.elastic_held;
        out.push_back(std::move(snap));
    }
    for (const auto& retired : retired_) {
        if (retired == nullptr || retired->exited()) {
            continue;
        }
        WorkerSlotSnapshot snap;
        snap.slot_index = slots_.size();
        snap.pid = retired->process_id();
        snap.cgroup_path = retired->cgroup_path();
        snap.started_at_ms = retired->started_at_ms();
        snap.last_active_at_ms = retired->last_active_at_ms();
        snap.evictable = false;
        snap.idle = false;
        snap.elastic_held = false;
        out.push_back(std::move(snap));
    }
    return out;
}

void GenerationPool::set_scale_callbacks(
    std::function<void()> on_worker_ready,
    std::function<void(const std::string& reason)> on_scale_out_failed) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_worker_ready_ = std::move(on_worker_ready);
    on_scale_out_failed_ = std::move(on_scale_out_failed);
}

bool GenerationPool::evict_idle_worker(std::uint64_t min_residency_ms, const std::string& reason,
                                       bool require_elastic_held, std::int64_t* evicted_pid_out) {
    if (evicted_pid_out != nullptr) {
        *evicted_pid_out = 0;
    }
    bool release_elastic = false;
    std::function<void()> release_cb;
    std::function<void(const std::string&)> eviction_cb;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (state_ != State::kActive || ready_workers_locked() <= min_ready_) {
            return false;
        }
        const std::uint64_t now = steady_ms();
        std::size_t best_slot = slots_.size();
        std::uint64_t oldest_active_ms = std::numeric_limits<std::uint64_t>::max();
        for (std::size_t rev = slots_.size(); rev > 0; --rev) {
            const std::size_t index = rev - 1;
            const Slot& s = slots_[index];
            if (!s.ready || s.shutdown_issued || s.executor == nullptr || s.replacement_in_flight) {
                continue;
            }
            if (require_elastic_held && !s.elastic_held) {
                continue;
            }
            if (!s.executor->is_idle()) {
                continue;
            }
            if (min_residency_ms != 0) {
                const std::uint64_t started = s.executor->started_at_ms();
                if (started == 0 || now < started + min_residency_ms) {
                    continue;
                }
            }
            const std::uint64_t active_at = s.executor->last_active_at_ms();
            if (active_at < oldest_active_ms) {
                oldest_active_ms = active_at;
                best_slot = index;
            }
        }
        if (best_slot == slots_.size()) {
            return false;
        }
        Slot& victim = slots_[best_slot];
        if (evicted_pid_out != nullptr) {
            *evicted_pid_out = victim.executor->process_id();
        }
        victim.ready = false;
        victim.shutdown_issued = true;
        victim.executor->request_shutdown();
        retired_.push_back(std::move(victim.executor));
        release_elastic = settle_evicted_slot_elastic_locked(victim);
        if (ready_workers_locked() < min_ready_) {
            stable_since_ms_ = 0;
        }
        release_cb = options_.release_elastic_slot;
        eviction_cb = options_.on_eviction;
        cv_.notify_all();
    }
    if (release_elastic && release_cb) {
        release_cb();
    }
    if (options_.metrics != nullptr) {
        options_.metrics->count_pool_eviction(options_.application_id, std::string(tenant_id()),
                                              reason);
    }
    if (eviction_cb) {
        eviction_cb(reason);
    }
    return true;
}

void GenerationPool::schedule_deferred_floor_locked() {
    if (state_ != State::kActive || quarantine_waiting_)
        return;
    const auto ready = ready_workers_locked();
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        if (ready + replacements_scheduled_ >= min_ready_ ||
            replacements_scheduled_ >= policy_.replacements_concurrent_per_app)
            break;
        Slot& slot = slots_[index];
        if (slot.needs_replacement && !slot.ready && !slot.replacement_in_flight) {
            slot.needs_replacement = false;
            slot.replacement_in_flight = true;
            slot.scale_out_spawn = false;
            ++replacements_scheduled_;
            replacement_due_ms_[index] = steady_ms();
        }
    }
}

void GenerationPool::record_instability(std::size_t slot, WorkerInstabilityObservation observation,
                                        const char* reason) {
    // Under the pool mutex.
    const WorkerRecoveryDecision decision =
        record_worker_instability(recovery_state_, policy_, lifecycle_, observation);
    if (!decision.ok) {
        // A malformed decision is an operator-facing failure, not a silent
        // one: keep serving at N-1 (or 0) without a replacement.
        std::fprintf(stderr,
                     "capsid-host: generation pool %s: recovery controller "
                     "rejected the instability record (%s)\n",
                     options_.application_id.c_str(), decision.error.message.c_str());
        return;
    }
    recovery_state_ = decision.state;
    // §12.1 recovery family: the remaining instability budget after this
    // counted event, and the backoff the decision chose.
    if (options_.metrics != nullptr) {
        const std::uint64_t remaining =
            decision.events_in_window >= policy_.max_events
                ? 0
                : static_cast<std::uint64_t>(policy_.max_events - decision.events_in_window);
        options_.metrics->set_recovery_instability_budget_remaining(options_.application_id,
                                                                    remaining);
        options_.metrics->set_recovery_backoff_ms(options_.application_id,
                                                  decision.replacement_delay_ms);
    }
    if (decision.begin_quarantine) {
        begin_quarantine();
        return;
    }
    if (decision.schedule_replacement) {
        Slot& s = slots_[slot];
        s.replacement_in_flight = true;
        s.scale_out_spawn = false;
        s.elastic_held = false;
        s.needs_replacement = false;
        ++replacements_scheduled_;
        s.replacement_reason = reason;
        replacement_due_ms_[slot] = observation.now_ms + decision.replacement_delay_ms;
        cv_.notify_all(); // the pump re-evaluates its deadline wait
    } else if (decision.replacement_singleflight_exists) {
        slots_[slot].needs_replacement = true;
    }
}

void GenerationPool::begin_quarantine() {
    // Under the pool mutex.
    if (state_ != State::kActive)
        return;
    LogFields fields;
    fields.level = "error";
    fields.event = log_events::kQuarantine;
    fields.app = options_.application_id;
    fields.generation = options_.generation_digest;
    fields.result = "crash_budget_exceeded";
    fields.message = "quarantined: crash budget exceeded";
    emit_log(options_.log, LogLane::kControl, std::move(fields));
    if (options_.metrics != nullptr) {
        options_.metrics->count_recovery_quarantine(options_.application_id);
    }
    // Stop routing/replacement immediately. Dispatch persistence from the
    // pump without mutex_: App operations take that mutex in the opposite
    // order when committing or aborting an activation.
    quarantine_waiting_ = true;
    quarantine_notification_pending_ = true;
    state_ = State::kDraining;
    active_flag_.store(false, std::memory_order_relaxed);
    stable_since_ms_ = 0;
    cv_.notify_all();
}

void GenerationPool::finish_quarantine_locked() {
    quarantine_waiting_ = false;
    for (Slot& slot : slots_) {
        if (slot.executor == nullptr) {
            continue;
        }
        slot.shutdown_issued = true;
        slot.executor->request_shutdown();
    }
    cv_.notify_all();
}

void GenerationPool::set_event_sink(std::function<void(const WorkerExecutor*, WorkerEvent)> sink) {
    // The pump reads options_.event_sink under the same mutex, so the swap
    // is atomic with respect to every drain sweep.
    std::lock_guard<std::mutex> lock(mutex_);
    options_.event_sink = std::move(sink);
}

void GenerationPool::set_shared_event_sink(
    std::function<void(std::shared_ptr<WorkerExecutor>, WorkerEvent)> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    options_.shared_event_sink = std::move(sink);
}

std::uint64_t GenerationPool::inflight() const {
    std::unique_lock<std::mutex> lock(mutex_);
    std::uint64_t total = 0;
    for (const Slot& slot : slots_) {
        if (slot.executor != nullptr) {
            total += slot.executor->inflight();
        }
    }
    for (const auto& retired : retired_) {
        total += retired->inflight();
    }
    return total;
}

std::uint32_t GenerationPool::ready_workers() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return ready_workers_locked();
}

std::uint32_t GenerationPool::ready_workers_locked() const {
    std::uint32_t count = 0;
    for (const Slot& slot : slots_) {
        if (slot.ready) {
            ++count;
        }
    }
    return count;
}

GenerationPool::State GenerationPool::state() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return state_;
}

std::string_view GenerationPool::application_id() const { return options_.application_id; }

std::string_view GenerationPool::version() const { return options_.version; }

std::string_view GenerationPool::generation_digest() const { return options_.generation_digest; }

void GenerationPool::notify_executor_event() {
    if (options_.on_executor_notify)
        options_.on_executor_notify();
    std::lock_guard<std::mutex> lock(mutex_);
    events_pending_ = true;
    cv_.notify_all();
}

void GenerationPool::pump_loop() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (;;) {
        if (quarantine_notification_pending_) {
            quarantine_notification_pending_ = false;
            lock.unlock();
            if (options_.on_quarantine)
                options_.on_quarantine();
            lock.lock();
        }
        if (quarantine_waiting_ &&
            (!options_.quarantine_completion || options_.quarantine_completion->load())) {
            finish_quarantine_locked();
        }
        // Drain sweep: re-issue shutdowns for slots swapped in after
        // request_drain() (a replacement READY while draining must also be
        // shut down), and complete the drain when the fleet is empty.
        if (state_ == State::kDraining && !quarantine_waiting_) {
            const bool expired = drain_until_ && SteadyClock::now() >= *drain_until_;
            bool needs_before_forced_hook = false;
            for (Slot& slot : slots_) {
                if (slot.executor == nullptr) {
                    continue;
                }
                const bool idle = slot.executor->is_idle();
                if (!slot.executor->exited() && (idle || expired)) {
                    if (count_forced_drain_ && expired && !idle && !slot.shutdown_issued) {
                        const std::uint64_t active = slot.executor->inflight();
                        forced_drain_cancellations_.fetch_add(std::max<std::uint64_t>(1, active),
                                                              std::memory_order_acq_rel);
                    }
                    if (expired && !idle && !slot.shutdown_issued &&
                        options_.before_forced_shutdown && !before_forced_shutdown_ran_) {
                        needs_before_forced_hook = true;
                    }
                }
            }
            if (needs_before_forced_hook) {
                before_forced_shutdown_ran_ = true;
                lock.unlock();
                options_.before_forced_shutdown();
                lock.lock();
            }
            const bool expired_after = drain_until_ && SteadyClock::now() >= *drain_until_;
            for (Slot& slot : slots_) {
                if (slot.executor == nullptr) {
                    continue;
                }
                const bool idle = slot.executor->is_idle();
                if (!slot.executor->exited() && (idle || expired_after)) {
                    slot.shutdown_issued = true;
                    slot.executor->request_shutdown();
                }
            }
            drain_check_cycles_.fetch_add(1, std::memory_order_release);
            if (fleet_exited() && replacements_in_flight_ == 0 &&
                scale_admissions_in_flight_ == 0) {
                // finalize_drain takes the mutex itself; release first.
                lock.unlock();
                finalize_drain();
                return;
            }
        }
        // Continuous stability resets the replacement backoff (v1: the
        // pool-wide base — the fleet's last full-READY epoch).
        if (stable_since_ms_ != 0 && policy_.stable_reset_ms != 0) {
            const std::uint64_t now = steady_ms();
            if (now - stable_since_ms_ >= policy_.stable_reset_ms) {
                const WorkerStabilityResult stability =
                    observe_worker_stability(recovery_state_, policy_, stable_since_ms_, now);
                if (stability.ok) {
                    recovery_state_ = stability.state;
                }
                stable_since_ms_ = 0;
            }
        }
        // Idle reaper & replacement launch (§8.3): evict idle slots above
        // min_ready_ into retired_ (async reaping), freeing elastic slots
        // immediately without blocking on waitpid, then spawn due replacements.
        std::uint64_t next_idle_due = std::numeric_limits<std::uint64_t>::max();
        if (state_ == State::kActive && options_.idle_timeout_ms > 0) {
            const std::uint64_t now = steady_ms();
            std::size_t idle_evictions = 0;
            std::size_t idle_elastic_releases = 0;
            std::function<void()> release_cb;
            std::function<void(const std::string&)> eviction_cb;
            for (std::size_t rev = slots_.size(); rev > 0; --rev) {
                if (ready_workers_locked() <= min_ready_) {
                    break;
                }
                Slot& s = slots_[rev - 1];
                if (!s.ready || s.executor == nullptr || s.replacement_in_flight) {
                    continue;
                }
                if (!s.executor->is_idle()) {
                    continue;
                }
                const std::uint64_t active_at = s.executor->last_active_at_ms();
                if (active_at == 0) {
                    continue;
                }
                const std::uint64_t due = active_at + options_.idle_timeout_ms;
                if (now >= due) {
                    s.ready = false;
                    s.shutdown_issued = true;
                    s.executor->request_shutdown();
                    retired_.push_back(std::move(s.executor));
                    if (settle_evicted_slot_elastic_locked(s)) {
                        ++idle_elastic_releases;
                    }
                    ++idle_evictions;
                    if (ready_workers_locked() < min_ready_) {
                        stable_since_ms_ = 0;
                    }
                } else if (due < next_idle_due) {
                    next_idle_due = due;
                }
            }
            if (idle_evictions > 0) {
                release_cb = options_.release_elastic_slot;
                eviction_cb = options_.on_eviction;
                lock.unlock();
                for (std::size_t k = 0; k < idle_elastic_releases && release_cb; ++k) {
                    release_cb();
                }
                for (std::size_t k = 0; k < idle_evictions; ++k) {
                    if (options_.metrics != nullptr) {
                        options_.metrics->count_pool_eviction(
                            options_.application_id, std::string(tenant_id()), "idle_timeout");
                    }
                    if (eviction_cb) {
                        eviction_cb("idle_timeout");
                    }
                }
                lock.lock();
            }
        }
        // Launch due replacements (§8.3): a scheduled slot spawns at its
        // backoff deadline; the spawn runs on its own thread so the pump
        // never blocks on the semaphore or the factory.
        if (state_ == State::kActive) {
            const std::uint64_t now = steady_ms();
            for (std::size_t index = 0; index < slots_.size(); ++index) {
                if (replacement_due_ms_[index] != kNoDue && replacement_due_ms_[index] <= now) {
                    replacement_due_ms_[index] = kNoDue;
                    ++replacements_in_flight_;
                    auto done = std::make_shared<std::atomic<bool>>(false);
                    replacement_threads_.push_back(ReplacementWorker{
                        std::thread([owner = this, index, done] {
                            struct FinishGuard {
                                std::shared_ptr<std::atomic<bool>> flag;
                                ~FinishGuard() {
                                    if (flag) {
                                        flag->store(true, std::memory_order_release);
                                    }
                                }
                            } guard{done};
                            // wait() joins replacement producers too;
                            // they must not release the last pool owner
                            // on a thread that its destructor would join.
                            owner->run_replacement(index);
                        }),
                        done});
                }
            }
        }
        // Reap finished replacement threads and exited retired executors out
        // from under the mutex so long-lived pools do not accumulate threads.
        {
            std::vector<std::thread> finished_replacements;
            for (std::size_t i = 0; i < replacement_threads_.size();) {
                if (replacement_threads_[i].finished &&
                    replacement_threads_[i].finished->load(std::memory_order_acquire)) {
                    finished_replacements.push_back(std::move(replacement_threads_[i].thread));
                    replacement_threads_.erase(replacement_threads_.begin() +
                                               static_cast<std::ptrdiff_t>(i));
                } else {
                    ++i;
                }
            }
            std::vector<std::shared_ptr<WorkerExecutor>> join_retired;
            for (auto it = retired_.begin(); it != retired_.end();) {
                if (!*it) {
                    it = retired_.erase(it);
                    continue;
                }
                if ((*it)->exited()) {
                    forward_executor_events_locked(*it);
                    join_retired.push_back(std::move(*it));
                    if (options_.on_worker_started) {
                        observed_retired_.push_back(join_retired.back());
                    }
                    it = retired_.erase(it);
                } else {
                    ++it;
                }
            }
            if (!finished_replacements.empty() || !join_retired.empty()) {
                lock.unlock();
                for (std::thread& thread : finished_replacements) {
                    if (thread.joinable()) {
                        thread.join();
                    }
                }
                for (const auto& executor : join_retired) {
                    executor->stop_and_join();
                }
                // Release our shared_ptrs outside mutex_ so WorkerExecutor and
                // its POSIX event wake descriptors close immediately.
                join_retired.clear();
                lock.lock();
            }
        }
        // Wait for work: an event notification, a replacement/idle deadline,
        // or the drain completing.
        constexpr std::uint64_t kMaxDue = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t next_due = next_idle_due;
        for (const std::uint64_t due : replacement_due_ms_) {
            if (due != kNoDue && due < next_due) {
                next_due = due;
            }
        }
        if (state_ == State::kActive && options_.idle_timeout_ms > 0 &&
            ready_workers_locked() > min_ready_ && next_due == kMaxDue) {
            next_due = steady_ms() + std::min<std::uint64_t>(options_.idle_timeout_ms, 500ULL);
        }
        if (!events_pending_ &&
            !(state_ == State::kDraining && !quarantine_waiting_ && fleet_exited() &&
              replacements_in_flight_ == 0 && scale_admissions_in_flight_ == 0)) {
            // request_drain changes state without adding an event. Wake
            // an ACTIVE wait on that transition even when live channels
            // still pin the fleet, so the next iteration can enforce the
            // drain deadline. Waiting for fleet_exited here would leave a
            // quiet WebSocket generation asleep indefinitely.
            //
            // The no-deadline wait's predicate additionally covers "a
            // replacement was scheduled" (any due now present): a schedule
            // notify can arrive from another thread — the spawn thread's
            // run_replacement failure path — when no deadline was being
            // waited on, and without the due_pending clause that notify
            // would find the predicate false and the wait would sleep
            // through the new deadline forever (a timeout-less predicate
            // wait re-sleeps instead of returning). With the clause, the
            // wait returns, the loop recomputes next_due, and the
            // deadline wait below sleeps until the replacement is due.
            const auto wake = [this] {
                return events_pending_ || due_pending_locked() || state_ == State::kDraining;
            };
            if (state_ == State::kDraining) {
                // A host ACK can return the final channel slot without a
                // response event. Recheck liveness and the absolute bound.
                cv_.wait_for(lock, std::chrono::milliseconds(50));
            } else if (next_due == kMaxDue) {
                cv_.wait(lock, wake);
            } else {
                // The deadline is the timeout: expiry returns even when the
                // predicate is false. Only a strictly earlier replacement due
                // (never <= next_due) can wake wait_until early so we cannot
                // busy-loop while still reacting immediately to scale-out/crash
                // scheduling during a longer idleTimeout wait.
                cv_.wait_until(
                    lock,
                    SteadyClock::time_point(std::chrono::duration_cast<SteadyClock::duration>(
                        std::chrono::milliseconds(next_due))),
                    [this, next_due] {
                        if (events_pending_ || state_ == State::kDraining) {
                            return true;
                        }
                        for (const std::uint64_t due : replacement_due_ms_) {
                            if (due != kNoDue && due < next_due) {
                                return true;
                            }
                        }
                        return false;
                    });
            }
        }
        events_pending_ = false;
        // Drain every slot's event queue (this is the owner thread for all
        // of them) and handle exits. Non-kExit events are forwarded to the
        // §9.2 event sink (the listener's sessions); kExit stays pool-owned
        // for the replacement machinery but is forwarded too, so the
        // listener can fail the requests pinned to a dead worker (the
        // executor emits no per-request failure for a worker exit).
        std::vector<std::size_t> exited;
        for (std::size_t index = 0; index < slots_.size(); ++index) {
            if (slots_[index].executor && forward_executor_events_locked(slots_[index].executor))
                exited.push_back(index);
        }
        for (const auto& executor : retired_)
            forward_executor_events_locked(executor);
        std::size_t exit_elastic_releases = 0;
        for (const std::size_t index : exited) {
            if (handle_executor_exit(index)) {
                ++exit_elastic_releases;
            }
        }
        if (exit_elastic_releases > 0 && options_.release_elastic_slot) {
            std::function<void()> release_cb = options_.release_elastic_slot;
            lock.unlock();
            for (std::size_t k = 0; k < exit_elastic_releases; ++k) {
                release_cb();
            }
            lock.lock();
        }
    }
}

bool GenerationPool::fleet_exited() const {
    for (const Slot& slot : slots_) {
        if (slot.executor != nullptr && !slot.executor->exited()) {
            return false;
        }
    }
    for (const auto& retired : retired_) {
        if (retired != nullptr && !retired->exited())
            return false;
    }
    return true;
}

// Any replacement scheduled (a due present), regardless of whether its
// deadline has elapsed. Called from the pump's wait predicate under the
// pool mutex: it turns a schedule notify into a wake even when the pump
// was sleeping on the no-deadline wait — the wait returns, the deadline
// is recomputed, and the pump sleeps until the replacement is due.
bool GenerationPool::due_pending_locked() const {
    for (const std::uint64_t due : replacement_due_ms_) {
        if (due != kNoDue) {
            return true;
        }
    }
    return false;
}

bool GenerationPool::settle_evicted_slot_elastic_locked(Slot& victim) {
    if (victim.elastic_held) {
        victim.elastic_held = false;
        return true;
    }
    // Prefer transferring floor ownership from an already-READY worker first.
    for (Slot& other : slots_) {
        if (&other != &victim && other.elastic_held && other.ready && other.executor != nullptr) {
            other.elastic_held = false;
            return true;
        }
    }
    for (Slot& other : slots_) {
        if (&other != &victim && other.elastic_held && other.replacement_in_flight) {
            other.elastic_held = false;
            return true;
        }
    }
    return false;
}

bool GenerationPool::forward_executor_events_locked(
    const std::shared_ptr<WorkerExecutor>& executor) {
    bool exited = false;
    for (WorkerEvent& event : executor->drain_events()) {
        if (event.type == WorkerEvent::Type::kExit) {
            // Each executor queues one exit. Deliver it even if a health recycle
            // already removed this executor from the READY set.
            exited = true;
            if (options_.on_worker_exited)
                options_.on_worker_exited(executor.get());
        }
        if (options_.event_sink)
            options_.event_sink(executor.get(),
                                options_.shared_event_sink ? event : std::move(event));
        if (options_.shared_event_sink)
            options_.shared_event_sink(executor, std::move(event));
    }
    return exited;
}

bool GenerationPool::handle_executor_exit(std::size_t slot) {
    // Under the pool mutex (pump thread).
    Slot& s = slots_[slot];
    if (!s.ready) {
        // Already removed (a repeated exit bookkeeping pass); count nothing
        // twice.
        return false;
    }
    s.ready = false; // §8.3: EXIT removes the slot from the READY set now
    stable_since_ms_ = 0;
    bool recorded_instability = false;
    if (!s.shutdown_issued && options_.replace_exited_workers) {
        const std::uint64_t now = steady_ms();
        std::uint32_t ready_after = 0;
        for (const Slot& other : slots_) {
            if (other.ready) {
                ++ready_after;
            }
        }
        WorkerInstabilityObservation observation;
        observation.kind = WorkerInstabilityKind::kUnexpectedExit;
        observation.worker_generation = options_.generation_digest;
        observation.now_ms = now;
        observation.ready_workers_after_removal = ready_after;
        observation.target_ready_workers = min_ready_;
        observation.replacements_in_flight_for_app =
            static_cast<std::uint32_t>(replacements_scheduled_);
        observation.chosen_jitter_basis_points = 0; // deterministic v1
        count_event(options_.metrics, "crash", options_.application_id, options_.generation_digest);
        record_instability(slot, observation, "crashed");
        recorded_instability = true;
    }
    // During active operation, if an elastic slot above minReady exits without
    // scheduling a replacement, retire the dead executor and return true so the
    // caller invokes release_elastic_slot outside mutex_. During kDraining,
    // keep exited slots in slots_ so finalize_drain() preserves final_exit_statuses_.
    if (state_ == State::kActive && !s.replacement_in_flight && !s.needs_replacement &&
        (!recorded_instability || !quarantine_waiting_)) {
        if (s.executor != nullptr && ready_workers_locked() >= min_ready_) {
            retired_.push_back(std::move(s.executor));
            return settle_evicted_slot_elastic_locked(s);
        }
    }
    return false;
}

void GenerationPool::run_replacement(std::size_t slot) {
    // Spawn thread. Never runs on the pump.
    bool is_scale_out = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        is_scale_out = slots_[slot].scale_out_spawn;
    }
    auto abandon = [this, slot, is_scale_out](const char* reason) {
        bool release_elastic = false;
        std::function<void()> release_cb;
        std::function<void(const std::string&)> failed_cb;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            Slot& s = slots_[slot];
            s.replacement_in_flight = false;
            s.scale_out_spawn = false;
            s.needs_replacement = false;
            if (s.elastic_held) {
                s.elastic_held = false;
                release_elastic = true;
                release_cb = options_.release_elastic_slot;
            }
            if (replacements_scheduled_ > 0) {
                --replacements_scheduled_;
            }
            --replacements_in_flight_;
            if (state_ == State::kActive && !quarantine_waiting_ &&
                ready_workers_locked() + replacements_scheduled_ < min_ready_ &&
                replacements_scheduled_ < policy_.replacements_concurrent_per_app) {
                std::size_t recovery_slot = slots_.size();
                for (std::size_t index = 0; index < slots_.size(); ++index) {
                    if (!slots_[index].ready && !slots_[index].replacement_in_flight &&
                        slots_[index].needs_replacement) {
                        recovery_slot = index;
                        break;
                    }
                }
                if (recovery_slot == slots_.size() && !s.ready && !s.replacement_in_flight) {
                    recovery_slot = slot;
                }
                if (recovery_slot < slots_.size()) {
                    Slot& target = slots_[recovery_slot];
                    target.needs_replacement = false;
                    target.replacement_in_flight = true;
                    target.scale_out_spawn = false;
                    target.elastic_held = false;
                    ++replacements_scheduled_;
                    const std::uint64_t delay_ms =
                        std::max<std::uint64_t>(policy_.backoff_initial_ms, 10ULL);
                    replacement_due_ms_[recovery_slot] = steady_ms() + delay_ms;
                }
            }
            failed_cb = is_scale_out ? on_scale_out_failed_ : nullptr;
            cv_.notify_all();
        }
        if (release_elastic && release_cb) {
            release_cb();
        }
        if (failed_cb) {
            failed_cb(reason);
        }
    };

    // §8.3: a generation that is no longer active never starts a
    // replacement.
    if (!active_flag_.load(std::memory_order_relaxed)) {
        abandon("pool_draining");
        return;
    }

    // Direction A: the replacement joins the process-global fair
    // startup-permit queue (design §10.5.6) and holds its grant across
    // the respawn, so a crash-looping App cannot persistently queue ahead
    // of another App's deploy. The abort predicate abandons the wait when
    // this pool's drain begins, so the pump's replacements_in_flight_ == 0
    // wait can never deadlock behind a permit nobody will hand out.
    bool grant_held = false;
    if (options_.startup_permits != nullptr) {
        StartupPermitRequest request;
        request.application = options_.application_id;
        request.generation = options_.generation_digest;
        request.lane = StartupPermitLane::kReplacement;
        const auto aborted = [this] { return !active_flag_.load(std::memory_order_relaxed); };
        if (!options_.startup_permits->enqueue_and_wait(request, aborted)) {
            // Queue full, singleflight, stop, or the drain aborted the
            // wait: nothing was granted; the pool-side bookkeeping
            // releases and notifies cold-start waiters outside mutex_.
            abandon("startup_permit_refused");
            return;
        }
        grant_held = true;
        if (options_.metrics != nullptr) {
            options_.metrics->count_recovery_startup_permit_grant(options_.application_id);
        }
    }

    // Spawn and READY through the same factory (same artifact, same
    // effective config — §8.3) into a fresh executor.
    std::shared_ptr<WorkerExecutor> replacement(new WorkerExecutor());
    replacement->set_channel_budget(options_.channel_budget);
    replacement->set_log_identity(options_.application_id, options_.generation_digest);
    replacement->set_event_notifier([this] { notify_executor_event(); });
    std::string spawn_error;
    const bool ready = replacement->start(options_.factory, &spawn_error);
    if (grant_held) {
        // The permit was consumed by this spawn/READY window; hand it to
        // the next waiter regardless of the outcome.
        options_.startup_permits->release_grant();
    }

    if (!ready) {
        // A failed replacement is an instability: record it (the crash
        // budget applies) and re-schedule with the next backoff step.
        // Elastic scale-out failures notify the waiting listener and do NOT
        // auto-respawn in the background unless min_ready_ requires it.
        std::fprintf(stderr, "capsid-host: generation pool %s: %s spawn failed: %s\n",
                     options_.application_id.c_str(), is_scale_out ? "scale-out" : "replacement",
                     spawn_error.c_str());
        bool release_elastic = false;
        std::function<void()> release_cb;
        std::function<void(const std::string&)> failed_cb;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            Slot& s = slots_[slot];
            s.replacement_in_flight = false;
            s.scale_out_spawn = false;
            s.needs_replacement = false;
            if (is_scale_out && s.elastic_held) {
                s.elastic_held = false;
                release_elastic = true;
                release_cb = options_.release_elastic_slot;
            }
            if (replacements_scheduled_ > 0) {
                --replacements_scheduled_;
            }
            --replacements_in_flight_;
            failed_cb = is_scale_out ? on_scale_out_failed_ : nullptr;
            std::size_t recovery_slot = slot;
            if (is_scale_out) {
                for (std::size_t index = 0; index < slots_.size(); ++index) {
                    if (!slots_[index].ready && !slots_[index].replacement_in_flight &&
                        slots_[index].needs_replacement) {
                        recovery_slot = index;
                        break;
                    }
                }
            }
            WorkerInstabilityObservation observation;
            observation.kind = WorkerInstabilityKind::kReplacementStartupFailure;
            observation.worker_generation = options_.generation_digest;
            observation.now_ms = steady_ms();
            observation.ready_workers_after_removal = ready_workers_locked();
            observation.target_ready_workers = min_ready_;
            observation.replacements_in_flight_for_app =
                static_cast<std::uint32_t>(replacements_scheduled_);
            observation.chosen_jitter_basis_points = 0;
            count_event(options_.metrics, "crash", options_.application_id,
                        options_.generation_digest);
            record_instability(recovery_slot, observation, "crashed");
            cv_.notify_all();
        }
        if (release_elastic && release_cb) {
            release_cb();
        }
        if (failed_cb) {
            failed_cb("spawn_failed");
        }
        return;
    }

    install_replacement(slot, std::move(replacement));
}

void GenerationPool::install_replacement(std::size_t slot,
                                         std::shared_ptr<WorkerExecutor> replacement) {
    WorkerExecutor* installed = replacement.get();
    std::string reason;
    bool was_scale_out = false;
    std::size_t elastic_releases = 0;
    std::function<void()> release_cb;
    std::function<void()> ready_cb;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        Slot& s = slots_[slot];
        if (s.executor != nullptr) {
            retired_.push_back(std::move(s.executor));
        }
        s.executor = std::move(replacement);
        s.ready = true; // slot READY
        was_scale_out = s.scale_out_spawn;
        s.scale_out_spawn = false;
        s.shutdown_issued = false;
        s.needs_replacement = false;
        if (state_ == State::kDraining && !quarantine_waiting_) {
            s.executor->request_shutdown();
        }
        s.replacement_in_flight = false;
        if (replacements_scheduled_ > 0) {
            --replacements_scheduled_;
        }
        reason = std::move(s.replacement_reason);
        s.replacement_reason.clear();
        const std::uint32_t ready_now = ready_workers_locked();
        if (min_ready_ > 0 && ready_now >= min_ready_) {
            stable_since_ms_ = steady_ms(); // full-READY epoch base
        }
        if (state_ == State::kActive && !quarantine_waiting_) {
            schedule_deferred_floor_locked();
            if (ready_now >= min_ready_) {
                for (Slot& other : slots_) {
                    if (!other.ready && !other.replacement_in_flight && other.needs_replacement) {
                        other.needs_replacement = false;
                        if (other.executor != nullptr) {
                            retired_.push_back(std::move(other.executor));
                        }
                        if (settle_evicted_slot_elastic_locked(other)) {
                            ++elastic_releases;
                        }
                    }
                }
                release_cb = options_.release_elastic_slot;
            }
        }
        ready_cb = on_worker_ready_;
        cv_.notify_all();
    }
    for (std::size_t k = 0; k < elastic_releases && release_cb; ++k) {
        release_cb();
    }
    if (was_scale_out) {
        count_event(options_.metrics, "ready", options_.application_id, options_.generation_digest);
    } else {
        LogFields fields;
        fields.event = log_events::kWorkerReplaced;
        fields.app = options_.application_id;
        fields.generation = options_.generation_digest;
        fields.result = "replaced";
        fields.message =
            (reason == "unhealthy") ? "replaced unhealthy worker" : "replaced crashed worker";
        emit_log(options_.log, LogLane::kControl, std::move(fields));
        count_event(options_.metrics, "replacement", options_.application_id,
                    options_.generation_digest);
    }
    if (options_.on_worker_started) {
        options_.on_worker_started(installed);
    }
    if (ready_cb) {
        ready_cb();
    }
    // The observer runs outside mutex_, but is still a replacement
    // producer. Keep finalization from releasing its executor (and from
    // calling on_drain_complete) until the observer has returned.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        --replacements_in_flight_;
        cv_.notify_all();
    }
}

std::vector<capsid::WorkerExitStatus> GenerationPool::worker_exit_statuses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::kDead) {
        return final_exit_statuses_;
    }
    std::vector<capsid::WorkerExitStatus> statuses;
    statuses.reserve(slots_.size());
    for (const Slot& slot : slots_) {
        if (slot.executor != nullptr) {
            statuses.push_back(slot.executor->exit_status());
        }
    }
    return statuses;
}

void GenerationPool::finalize_drain() {
    // Under the pool mutex (pump thread).
    std::vector<std::shared_ptr<WorkerExecutor>> reaped;
    std::size_t live_slot_count = 0;
    std::size_t elastic_to_release = 0;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        for (Slot& slot : slots_) {
            if (slot.elastic_held) {
                slot.elastic_held = false;
                ++elastic_to_release;
            }
            if (slot.executor != nullptr) {
                forward_executor_events_locked(slot.executor);
                reaped.push_back(std::move(slot.executor));
                ++live_slot_count;
            }
        }
        slots_.clear();
        for (auto& retired : retired_) {
            if (retired != nullptr) {
                forward_executor_events_locked(retired);
                reaped.push_back(std::move(retired));
            }
        }
        retired_.clear();
        for (auto& observed : observed_retired_) {
            if (observed != nullptr) {
                reaped.push_back(std::move(observed));
            }
        }
        observed_retired_.clear();
    }
    for (std::size_t k = 0; k < elastic_to_release && options_.release_elastic_slot; ++k) {
        options_.release_elastic_slot();
    }
    for (const auto& executor : reaped) {
        executor->stop_and_join();
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        final_exit_statuses_.clear();
        final_exit_statuses_.reserve(live_slot_count);
        for (std::size_t index = 0; index < live_slot_count; ++index) {
            final_exit_statuses_.push_back(reaped[index]->exit_status());
        }
        state_ = State::kDead;
        cv_.notify_all();
    }
    // §9.4: the reaper finished — the drained generation's capacity count
    // releases now. Outside the pool mutex (the callback must be able to
    // take its own locks), after every executor thread has joined.
    if (options_.capacity_lease)
        options_.capacity_lease->reaped();
    // A child disappearing from /proc can precede this point: the sole
    // reaper still has executor bookkeeping to publish. Expose the actual
    // generation completion after its capacity category has been released.
    count_event(options_.metrics, "generation_drained", options_.application_id,
                options_.generation_digest);
    if (options_.on_drain_complete) {
        options_.on_drain_complete();
    }
    // Release the pool's last executor owners outside the mutex. External
    // snapshots may outlive these, but no executor producer remains live.
}

} // namespace capsid::host
