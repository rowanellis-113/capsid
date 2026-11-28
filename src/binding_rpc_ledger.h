// Binding RPC v2 (v0.4.0): three-tier quota ledger accounting for
// per-request, per-Binding, and per-worker limits across inflight calls,
// active streams, active remote handles, queued frames, and queued bytes.
//
// Pure C++11 with no QuickJS or libuv dependency so every boundary,
// overflow check, rollback, and refill invariant is unit-testable directly.

#ifndef CAPSID_BINDING_RPC_LEDGER_H
#define CAPSID_BINDING_RPC_LEDGER_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace capsid {

struct BindingRpcLimits {
    // Per-request limits
    std::size_t max_calls_per_request = 64;
    std::size_t max_streams_per_request = 16;
    std::size_t max_handles_per_request = 32;
    std::size_t max_frames_per_request = 512;
    std::size_t max_bytes_per_request = 4U * 1024U * 1024U;

    // Per-Binding limits
    // Default per-Binding inflight calls match the Binding v1 1,024-call
    // worker ceiling so single-Binding workers preserve the existing
    // 16 requests x 64 calls contract, while streams/handles/frames/bytes
    // enforce strictly tighter per-Binding caps.
    std::size_t max_calls_per_binding = 1024;
    std::size_t max_streams_per_binding = 64;
    std::size_t max_handles_per_binding = 128;
    std::size_t max_frames_per_binding = 2048;
    std::size_t max_bytes_per_binding = 8U * 1024U * 1024U;

    // Per-worker limits
    std::size_t max_calls_per_worker = 1024;
    std::size_t max_streams_per_worker = 256;
    std::size_t max_handles_per_worker = 512;
    std::size_t max_frames_per_worker = 4096;
    std::size_t max_bytes_per_worker = 16U * 1024U * 1024U;
};

struct BindingRpcCounts {
    std::size_t calls = 0;
    std::size_t streams = 0;
    std::size_t handles = 0;
    std::size_t frames = 0;
    std::size_t bytes = 0;

    bool is_zero() const {
        return calls == 0 && streams == 0 && handles == 0 && frames == 0 && bytes == 0;
    }
};

enum class BindingQuotaError {
    kOk = 0,
    kWorkerCallLimit,
    kBindingCallLimit,
    kRequestCallLimit,
    kWorkerStreamLimit,
    kBindingStreamLimit,
    kRequestStreamLimit,
    kWorkerHandleLimit,
    kBindingHandleLimit,
    kRequestHandleLimit,
    kWorkerFrameLimit,
    kBindingFrameLimit,
    kRequestFrameLimit,
    kWorkerByteLimit,
    kBindingByteLimit,
    kRequestByteLimit,
};

// Returns a static diagnostic string for a quota error. Call-limit strings
// preserve the exact Binding v1 diagnostics ("binding call quota exceeded" and
// "binding request quota exceeded") so existing quota contracts remain intact.
const char* binding_quota_error_message(BindingQuotaError error);

class BindingRpcLedger {
  public:
    explicit BindingRpcLedger(const BindingRpcLimits& limits = BindingRpcLimits());

    const BindingRpcLimits& limits() const { return limits_; }

    // Reserve/release one inflight RPC call. Fails closed without mutating any
    // tier if any of worker, binding, or request limits would be exceeded.
    bool reserve_call(std::uint64_t request_id, const std::string& binding_id,
                      BindingQuotaError* error = nullptr);
    void release_call(std::uint64_t request_id, const std::string& binding_id);

    // Reserve/release one active stream proxy.
    bool reserve_stream(std::uint64_t request_id, const std::string& binding_id,
                        BindingQuotaError* error = nullptr);
    void release_stream(std::uint64_t request_id, const std::string& binding_id);

    // Reserve/release one active remote object handle (RpcTarget stub).
    bool reserve_handle(std::uint64_t request_id, const std::string& binding_id,
                        BindingQuotaError* error = nullptr);
    void release_handle(std::uint64_t request_id, const std::string& binding_id);

    // Reserve/release one queued RPC or stream frame with its payload byte
    // count. Checks both frame count and overflow-safe byte ceiling across all
    // three tiers atomically before mutating state.
    bool reserve_frame(std::uint64_t request_id, const std::string& binding_id, std::size_t bytes,
                       BindingQuotaError* error = nullptr);
    void release_frame(std::uint64_t request_id, const std::string& binding_id, std::size_t bytes);

    // Reserve/release payload bytes incrementally across all three tiers
    // without mutating frame count (used during structured clone so native
    // payload growth is bounded before allocation completes).
    bool reserve_bytes(std::uint64_t request_id, const std::string& binding_id, std::size_t bytes,
                       BindingQuotaError* error = nullptr);
    void release_bytes(std::uint64_t request_id, const std::string& binding_id, std::size_t bytes);

    BindingRpcCounts worker_counts() const { return worker_; }
    BindingRpcCounts request_counts(std::uint64_t request_id) const;
    BindingRpcCounts binding_counts(const std::string& binding_id) const;

    std::size_t tracked_requests() const { return by_request_.size(); }
    std::size_t tracked_bindings() const { return by_binding_.size(); }
    bool empty() const { return worker_.is_zero() && by_request_.empty() && by_binding_.empty(); }

    void clear();

  private:
    static bool would_exceed(std::size_t current, std::size_t delta, std::size_t limit) {
        return delta > limit || current > limit - delta;
    }
    static void decrement_clamped(std::size_t* value, std::size_t delta) {
        *value = (*value >= delta) ? (*value - delta) : 0;
    }

    void gc_entries(std::uint64_t request_id, const std::string& binding_id);

    BindingRpcLimits limits_;
    BindingRpcCounts worker_;
    std::map<std::uint64_t, BindingRpcCounts> by_request_;
    std::map<std::string, BindingRpcCounts> by_binding_;
};

} // namespace capsid

#endif
