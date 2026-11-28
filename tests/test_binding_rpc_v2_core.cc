// Binding RPC v2 (v0.4.0) M4A unit tests:
//   - Three-tier quota ledger (per-request / per-Binding / per-worker) across
//     calls, streams, handles, queued frames, and queued bytes, including
//     overflow protection, atomic rollback, map GC, and slot refill.
//   - Fair round-robin BindingScheduler across multiple Binding queues,
//     cancellation of queued calls, strict reentrancy guard, and immediate
//     libuv loop wakeup + idle stop + clean loop close.

#include "binding_rpc_ledger.h"
#include "binding_scheduler.h"

#include <uv.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "test-binding-rpc-v2-core: FAIL: " << message << std::endl;
        std::exit(1);
    }
}

void test_quota_ledger_calls_and_three_tiers() {
    capsid::BindingRpcLimits limits;
    limits.max_calls_per_request = 2;
    limits.max_calls_per_binding = 3;
    limits.max_calls_per_worker = 4;
    capsid::BindingRpcLedger ledger(limits);

    capsid::BindingQuotaError err = capsid::BindingQuotaError::kOk;
    // Request 1 -> binding A: 2 calls allowed, 3rd hits request limit.
    require(ledger.reserve_call(1, "a", &err) && err == capsid::BindingQuotaError::kOk,
            "req1 call1 reserve");
    require(ledger.reserve_call(1, "a", &err) && err == capsid::BindingQuotaError::kOk,
            "req1 call2 reserve");
    require(!ledger.reserve_call(1, "a", &err) && err == capsid::BindingQuotaError::kRequestCallLimit,
            "req1 call3 must hit request limit");
    require(std::strcmp(capsid::binding_quota_error_message(err),
                        "binding request quota exceeded") == 0,
            "v1 request quota message preserved");

    // Request 2 -> binding A: 1 more allowed (binding A now at 3), next hits binding limit.
    require(ledger.reserve_call(2, "a", &err), "req2 call1 on binding a");
    require(!ledger.reserve_call(2, "a", &err) && err == capsid::BindingQuotaError::kBindingCallLimit,
            "req2 call2 on binding a must hit binding limit");

    // Request 2 -> binding B: 1 more allowed (worker now at 4), next hits worker limit.
    require(ledger.reserve_call(2, "b", &err), "req2 call1 on binding b");
    require(!ledger.reserve_call(3, "b", &err) && err == capsid::BindingQuotaError::kWorkerCallLimit,
            "req3 call1 must hit worker limit");
    require(std::strcmp(capsid::binding_quota_error_message(err),
                        "binding call quota exceeded") == 0,
            "v1 worker quota message preserved");

    // Release and verify refill + map entry GC.
    ledger.release_call(1, "a");
    ledger.release_call(1, "a");
    require(ledger.tracked_requests() == 1, "req1 zeroed entry must be GC'd");
    require(ledger.reserve_call(3, "b", &err), "worker slot refilled after release");

    ledger.release_call(2, "a");
    ledger.release_call(2, "b");
    ledger.release_call(3, "b");
    require(ledger.empty(), "ledger must return to empty after all releases");
}

void test_quota_ledger_streams_handles_frames_bytes_and_overflow() {
    capsid::BindingRpcLimits limits;
    limits.max_streams_per_request = 1;
    limits.max_streams_per_binding = 2;
    limits.max_streams_per_worker = 3;
    limits.max_handles_per_request = 1;
    limits.max_handles_per_binding = 2;
    limits.max_handles_per_worker = 2;
    limits.max_frames_per_request = 2;
    limits.max_frames_per_binding = 3;
    limits.max_frames_per_worker = 4;
    limits.max_bytes_per_request = 100;
    limits.max_bytes_per_binding = 150;
    limits.max_bytes_per_worker = 180;

    capsid::BindingRpcLedger ledger(limits);
    capsid::BindingQuotaError err = capsid::BindingQuotaError::kOk;

    // Streams
    require(ledger.reserve_stream(10, "cache", &err), "stream reserve 1");
    require(!ledger.reserve_stream(10, "cache", &err) &&
                err == capsid::BindingQuotaError::kRequestStreamLimit,
            "stream per-request limit");
    require(ledger.reserve_stream(11, "cache", &err), "stream reserve 2");
    require(!ledger.reserve_stream(12, "cache", &err) &&
                err == capsid::BindingQuotaError::kBindingStreamLimit,
            "stream per-binding limit");
    require(ledger.reserve_stream(12, "tcp", &err), "stream reserve 3 on other binding");
    require(!ledger.reserve_stream(13, "tcp", &err) &&
                err == capsid::BindingQuotaError::kWorkerStreamLimit,
            "stream per-worker limit");

    // Handles
    require(ledger.reserve_handle(10, "cache", &err), "handle reserve 1");
    require(!ledger.reserve_handle(10, "cache", &err) &&
                err == capsid::BindingQuotaError::kRequestHandleLimit,
            "handle per-request limit");
    require(ledger.reserve_handle(11, "tcp", &err), "handle reserve 2");
    require(!ledger.reserve_handle(12, "tcp", &err) &&
                err == capsid::BindingQuotaError::kWorkerHandleLimit,
            "handle per-worker limit");

    // Frames + bytes: atomic check (byte failure must NOT leak a frame count!)
    require(ledger.reserve_frame(10, "cache", 60, &err), "frame 1 (60B)");
    require(!ledger.reserve_frame(10, "cache", 50, &err) &&
                err == capsid::BindingQuotaError::kRequestByteLimit,
            "frame 2 (50B) exceeds 100B request byte cap");
    require(ledger.request_counts(10).frames == 1 && ledger.worker_counts().frames == 1,
            "failed byte reserve must not leak frame count");
    // Overflow safety: SIZE_MAX bytes must fail cleanly without wrapping.
    require(!ledger.reserve_frame(10, "cache", std::numeric_limits<std::size_t>::max(), &err) &&
                err == capsid::BindingQuotaError::kWorkerByteLimit,
            "SIZE_MAX bytes must fail closed");
    require(ledger.reserve_frame(10, "cache", 40, &err), "frame 2 (40B) fits exactly 100B");
    require(!ledger.reserve_frame(10, "cache", 0, &err) &&
                err == capsid::BindingQuotaError::kRequestFrameLimit,
            "frame 3 exceeds request frame cap");
    require(!ledger.reserve_frame(11, "cache", 60, &err) &&
                err == capsid::BindingQuotaError::kBindingByteLimit,
            "binding byte cap 150B enforced");
    require(ledger.reserve_frame(11, "cache", 50, &err), "binding byte cap exact 150B");
    require(!ledger.reserve_frame(12, "tcp", 40, &err) &&
                err == capsid::BindingQuotaError::kWorkerByteLimit,
            "worker byte cap 180B enforced");

    // Clean release everything
    ledger.release_frame(10, "cache", 60);
    ledger.release_frame(10, "cache", 40);
    ledger.release_frame(11, "cache", 50);
    ledger.release_handle(10, "cache");
    ledger.release_handle(11, "tcp");
    ledger.release_stream(10, "cache");
    ledger.release_stream(11, "cache");
    ledger.release_stream(12, "tcp");
    require(ledger.empty(), "all released -> ledger completely empty");
}

void test_scheduler_round_robin_and_cancel() {
    capsid::BindingScheduler sched;
    sched.configure_bindings(3);

    // Flood binding 0 with 4 calls, then enqueue 2 on binding 1 and 1 on binding 2.
    sched.enqueue_call(0, 101);
    sched.enqueue_call(0, 102);
    sched.enqueue_call(0, 103);
    sched.enqueue_call(0, 104);
    sched.enqueue_call(1, 201);
    sched.enqueue_call(1, 202);
    sched.enqueue_call(2, 301);

    // Cancel call 102 before dispatch.
    require(sched.cancel_call(0, 102), "cancel queued call 102");
    require(!sched.cancel_call(0, 999), "cancel absent call returns false");
    require(sched.pending_calls() == 6, "pending calls count updated");

    // Round-robin must interleave 0 -> 1 -> 2 -> 0 -> 1 -> 0 instead of draining all of 0 first!
    std::vector<std::uint64_t> order;
    std::uint64_t id = 0;
    while (sched.pop_next_call(&id)) {
        order.push_back(id);
    }
    const std::uint64_t expected[] = {101, 201, 301, 103, 202, 104};
    require(order.size() == 6, "popped all 6 calls");
    for (std::size_t i = 0; i < 6; ++i) {
        require(order[i] == expected[i], "round-robin dispatch order matches");
    }
    require(!sched.has_pending_work(), "scheduler idle after draining");
}

struct LoopTestState {
    capsid::BindingScheduler* sched = nullptr;
    int turns = 0;
    int reentrant_blocked = 0;
    std::vector<std::uint64_t> dispatched;
    std::vector<std::uint64_t> settled;
};

void loop_turn_cb(void* opaque) {
    LoopTestState* st = static_cast<LoopTestState*>(opaque);
    ++st->turns;
    // Attempting a nested turn inside callback must be blocked by reentrancy
    // guard. Only probe on the first 3 turns so the final drain turn lets the
    // scheduler transition cleanly back to idle.
    if (st->turns <= 3) {
        if (!st->sched->begin_turn()) {
            ++st->reentrant_blocked;
        } else {
            require(false, "reentrant begin_turn must fail");
        }
    }
    std::size_t count = 0;
    std::uint64_t id = 0;
    while (count < st->sched->config().max_dispatches_per_turn && st->sched->pop_next_call(&id)) {
        st->dispatched.push_back(id);
        st->sched->enqueue_result(id);
        ++count;
    }
    std::size_t res_count = 0;
    while (res_count < st->sched->config().max_results_per_turn &&
           st->sched->pop_next_result(&id)) {
        st->settled.push_back(id);
        ++res_count;
    }
}

void test_scheduler_immediate_libuv_wake_and_turn_budget() {
    uv_loop_t loop;
    require(uv_loop_init(&loop) == 0, "uv_loop_init");

    capsid::BindingSchedulerConfig cfg;
    cfg.max_dispatches_per_turn = 2;
    cfg.max_results_per_turn = 2;
    capsid::BindingScheduler sched(cfg);
    sched.configure_bindings(2);

    LoopTestState st;
    st.sched = &sched;
    require(sched.attach(&loop, loop_turn_cb, &st), "scheduler attach");
    // All scheduler handles must be unreffed so they never pin an idle worker loop.
    require(uv_loop_alive(&loop) == 0, "scheduler handles must be unreffed");

    // Keep the test loop alive (modeling the worker's reffed IPC poll handle).
    uv_async_t keepalive;
    require(uv_async_init(&loop, &keepalive, nullptr) == 0, "uv_async_init");

    // Enqueue 5 calls; with max_dispatches_per_turn=2, requires 3 libuv ticks
    // and must complete immediately (<50ms) WITHOUT any timer armed!
    sched.enqueue_call(0, 1);
    sched.enqueue_call(0, 2);
    sched.enqueue_call(1, 3);
    sched.enqueue_call(1, 4);
    sched.enqueue_call(0, 5);

    const std::uint64_t start_ns = uv_hrtime();
    for (int i = 0; i < 20 && (st.settled.size() < 5 || sched.has_pending_work()); ++i) {
        uv_run(&loop, UV_RUN_ONCE);
    }
    const std::uint64_t elapsed_ms = (uv_hrtime() - start_ns) / 1000000ULL;

    require(st.dispatched.size() == 5, "all 5 calls dispatched");
    require(st.settled.size() == 5, "all 5 results settled");
    require(st.turns >= 3, "bounded per-turn dispatch took >= 3 turns");
    require(st.reentrant_blocked >= 3, "reentrancy guard blocked every nested call");
    require(elapsed_ms < 50, "immediate libuv wake completed in <50ms without 10ms timer delay");
    require(!sched.has_pending_work(), "scheduler idle after completion");

    uv_close(reinterpret_cast<uv_handle_t*>(&keepalive), nullptr);
    sched.detach();
    for (int i = 0; i < 5; ++i) {
        uv_run(&loop, UV_RUN_NOWAIT);
    }
    require(uv_loop_close(&loop) == 0, "uv_loop_close clean after detach");
}

} // namespace

int main() {
    test_quota_ledger_calls_and_three_tiers();
    test_quota_ledger_streams_handles_frames_bytes_and_overflow();
    test_scheduler_round_robin_and_cancel();
    test_scheduler_immediate_libuv_wake_and_turn_budget();
    std::cout << "test-binding-rpc-v2-core: PASS (4 suites)" << std::endl;
    return 0;
}
