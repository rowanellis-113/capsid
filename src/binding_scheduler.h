// Binding RPC v2 (v0.4.0): fair round-robin multi-runtime scheduler and
// immediate shared-loop wake coordinator.
//
// Replaces the Binding v1 10 ms 1-call-per-tick timer dispatch with:
//   - per-Binding FIFO call queues drained round-robin across Binding tables
//     so one busy Binding cannot head-of-line starve another Binding;
//   - bounded per-turn dispatch/result/control quanta;
//   - strict reentrancy guard so callbacks, abort listeners, or finalizers
//     never recursively re-enter a turn;
//   - unreffed uv_prepare/uv_idle/uv_check handles that wake immediately when
//     RPC work is queued and stop cleanly when idle (zero overhead when no
//     Bindings are loaded).

#ifndef CAPSID_BINDING_SCHEDULER_H
#define CAPSID_BINDING_SCHEDULER_H

#include <uv.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <vector>

namespace capsid {

// F2/R1: Absolute 100 ms disposal deadline state shared across synchronous
// cleanup and asynchronous Promise/timer continuations captured by Binding
// async context hooks. Uses explicit `now_ns` so unit tests verify deadline
// inheritance and expiry deterministically with a controllable clock.
constexpr std::uint64_t kBindingDisposalBudgetNs = UINT64_C(100000000);

// T2/R5: Authoritative dispatch-boundary deadline check shared by
// WorkerRuntime::pump_binding_calls() and controllable-clock unit tests.
// A queued call whose effective deadline has already elapsed must never enter
// its Binding provider method even if the turn's control budget was exhausted.
inline bool is_binding_call_expired(std::uint64_t now_ns, std::uint64_t deadline_ns) {
    return deadline_ns != 0 && now_ns >= deadline_ns;
}

struct BindingDisposalState {
    std::uint64_t deadline_ns = 0;
    bool interrupted = false;

    std::uint64_t enter_scope(std::uint64_t now_ns,
                              std::uint64_t budget_ns = kBindingDisposalBudgetNs) {
        const std::uint64_t prev = deadline_ns;
        const std::uint64_t max_u64 = (std::numeric_limits<std::uint64_t>::max)();
        const std::uint64_t target =
            (max_u64 - now_ns >= budget_ns) ? (now_ns + budget_ns) : max_u64;
        deadline_ns = (prev != 0 && prev < target) ? prev : target;
        interrupted = false;
        return prev;
    }

    bool check_interrupt(std::uint64_t now_ns) {
        if (deadline_ns != 0 && !interrupted && now_ns >= deadline_ns) {
            interrupted = true;
            return true;
        }
        return false;
    }

    std::uint64_t capture_deadline() const { return deadline_ns; }

    std::uint64_t enter_async(std::uint64_t captured_deadline_ns) {
        const std::uint64_t prev = deadline_ns;
        deadline_ns = captured_deadline_ns;
        interrupted = false;
        return prev;
    }

    bool leave_async(std::uint64_t prev_deadline_ns) {
        const bool tripped = interrupted;
        deadline_ns = prev_deadline_ns;
        interrupted = false;
        return tripped;
    }
};

struct BindingSchedulerConfig {
    std::size_t max_dispatches_per_turn = 16;
    std::size_t max_results_per_turn = 32;
    std::size_t max_control_frames_per_turn = 32;
    std::size_t max_jobs_per_runtime_turn = 64;
};

class BindingScheduler {
  public:
    typedef void (*TurnCallback)(void* opaque);
    // F10/R5: Control-stage pump callback invoked under one shared per-turn
    // control budget (`max_control_frames_per_turn`). Decrements `*budget` for
    // every dequeued/expired unit and returns true if backlog remains.
    typedef bool (*ControlStageCallback)(std::size_t* budget, void* opaque);

    explicit BindingScheduler(const BindingSchedulerConfig& config = BindingSchedulerConfig());
    ~BindingScheduler();

    BindingScheduler(const BindingScheduler&) = delete;
    BindingScheduler& operator=(const BindingScheduler&) = delete;

    const BindingSchedulerConfig& config() const { return config_; }

    // Configure the number of Binding runtimes participating in round-robin
    // call dispatch. May be called once during worker startup after Binding
    // runtimes are created.
    void configure_bindings(std::size_t binding_count);

    // Attach/detach unreffed libuv wake handles on the shared worker loop.
    // Zero-Binding workers never call attach().
    bool attach(uv_loop_t* loop, TurnCallback callback, void* opaque);
    void detach();
    bool is_attached() const { return attached_; }

    // Enqueue / cancel / pop queued RPC calls (round-robin across bindings).
    void enqueue_call(std::size_t table_index, std::uint64_t call_id);
    bool cancel_call(std::size_t table_index, std::uint64_t call_id);
    bool pop_next_call(std::uint64_t* out_call_id);
    std::size_t pending_calls() const { return total_queued_calls_; }

    // Enqueue / pop settled Binding call results for User delivery.
    void enqueue_result(std::uint64_t call_id);
    bool pop_next_result(std::uint64_t* out_call_id);
    std::size_t pending_results() const { return result_queue_.size(); }

    // Enqueue / pop stream or handle control items (M4B/M4D).
    void enqueue_control(std::uint64_t item_id);
    bool pop_next_control(std::uint64_t* out_item_id);
    std::size_t pending_controls() const { return control_queue_.size(); }

    bool has_pending_work() const {
        return total_queued_calls_ > 0 || !result_queue_.empty() || !control_queue_.empty() ||
               reschedule_needed_;
    }

    // Arm the immediate loop wake handle (no-op if not attached or detached).
    void wake();

    // Executes up to `config_.max_control_frames_per_turn` control units across
    // `stage_count` stages using a rotating start cursor. Calls `wake()`
    // whenever any stage reports remaining backlog and returns total units
    // consumed during this turn.
    std::size_t run_control_stages(const ControlStageCallback* stages, std::size_t stage_count,
                                   void* opaque);

    // Clear all queues (used on worker shutdown / teardown).
    void clear_queues();

    // Reentrancy guard for executing one scheduler turn. If a turn is already
    // active, entering returns false and marks reschedule_needed_ so queued
    // work executes on the very next libuv turn.
    bool begin_turn();
    void end_turn();
    bool in_turn() const { return in_turn_; }

    // Convenience runner that enforces begin_turn()/end_turn() and updates
    // the idle handle after running the configured callback.
    void run_turn();

  private:
    static void on_prepare(uv_prepare_t* handle);
    static void on_idle(uv_idle_t* handle);
    static void on_check(uv_check_t* handle);
    void update_idle_state();

    BindingSchedulerConfig config_;
    std::vector<std::deque<std::uint64_t>> call_queues_;
    std::size_t rr_cursor_ = 0;
    std::size_t control_stage_cursor_ = 0;
    std::size_t total_queued_calls_ = 0;
    std::deque<std::uint64_t> result_queue_;
    std::deque<std::uint64_t> control_queue_;

    bool attached_ = false;
    bool handles_initialized_ = false;
    bool idle_active_ = false;
    bool in_turn_ = false;
    bool reschedule_needed_ = false;
    TurnCallback callback_ = nullptr;
    void* opaque_ = nullptr;
    uv_prepare_t prepare_;
    uv_idle_t idle_;
    uv_check_t check_;
};

} // namespace capsid

#endif
