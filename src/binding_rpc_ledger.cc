// Binding RPC v2 (v0.4.0): three-tier quota ledger implementation.

#include "binding_rpc_ledger.h"

namespace capsid {

const char* binding_quota_error_message(BindingQuotaError error) {
    switch (error) {
    case BindingQuotaError::kOk:
        return "";
    case BindingQuotaError::kWorkerCallLimit:
        return "binding call quota exceeded";
    case BindingQuotaError::kBindingCallLimit:
        return "binding instance call quota exceeded";
    case BindingQuotaError::kRequestCallLimit:
        return "binding request quota exceeded";
    case BindingQuotaError::kWorkerStreamLimit:
        return "binding stream quota exceeded";
    case BindingQuotaError::kBindingStreamLimit:
        return "binding instance stream quota exceeded";
    case BindingQuotaError::kRequestStreamLimit:
        return "binding request stream quota exceeded";
    case BindingQuotaError::kWorkerHandleLimit:
        return "binding handle quota exceeded";
    case BindingQuotaError::kBindingHandleLimit:
        return "binding instance handle quota exceeded";
    case BindingQuotaError::kRequestHandleLimit:
        return "binding request handle quota exceeded";
    case BindingQuotaError::kWorkerFrameLimit:
        return "binding queued frame quota exceeded";
    case BindingQuotaError::kBindingFrameLimit:
        return "binding instance queued frame quota exceeded";
    case BindingQuotaError::kRequestFrameLimit:
        return "binding request queued frame quota exceeded";
    case BindingQuotaError::kWorkerByteLimit:
        return "binding queued byte quota exceeded";
    case BindingQuotaError::kBindingByteLimit:
        return "binding instance queued byte quota exceeded";
    case BindingQuotaError::kRequestByteLimit:
        return "binding request queued byte quota exceeded";
    }
    return "binding quota exceeded";
}

BindingRpcLedger::BindingRpcLedger(const BindingRpcLimits& limits) : limits_(limits) {}

bool BindingRpcLedger::reserve_call(std::uint64_t request_id, const std::string& binding_id,
                                    BindingQuotaError* error) {
    if (error != nullptr) {
        *error = BindingQuotaError::kOk;
    }
    if (would_exceed(worker_.calls, 1, limits_.max_calls_per_worker)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kWorkerCallLimit;
        }
        return false;
    }
    const BindingRpcCounts req = request_counts(request_id);
    if (would_exceed(req.calls, 1, limits_.max_calls_per_request)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kRequestCallLimit;
        }
        return false;
    }
    const BindingRpcCounts bnd = binding_counts(binding_id);
    if (would_exceed(bnd.calls, 1, limits_.max_calls_per_binding)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kBindingCallLimit;
        }
        return false;
    }
    ++worker_.calls;
    ++by_request_[request_id].calls;
    ++by_binding_[binding_id].calls;
    return true;
}

void BindingRpcLedger::release_call(std::uint64_t request_id, const std::string& binding_id) {
    decrement_clamped(&worker_.calls, 1);
    std::map<std::uint64_t, BindingRpcCounts>::iterator rit = by_request_.find(request_id);
    if (rit != by_request_.end()) {
        decrement_clamped(&rit->second.calls, 1);
    }
    std::map<std::string, BindingRpcCounts>::iterator bit = by_binding_.find(binding_id);
    if (bit != by_binding_.end()) {
        decrement_clamped(&bit->second.calls, 1);
    }
    gc_entries(request_id, binding_id);
}

bool BindingRpcLedger::reserve_stream(std::uint64_t request_id, const std::string& binding_id,
                                      BindingQuotaError* error) {
    if (error != nullptr) {
        *error = BindingQuotaError::kOk;
    }
    if (would_exceed(worker_.streams, 1, limits_.max_streams_per_worker)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kWorkerStreamLimit;
        }
        return false;
    }
    const BindingRpcCounts req = request_counts(request_id);
    if (would_exceed(req.streams, 1, limits_.max_streams_per_request)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kRequestStreamLimit;
        }
        return false;
    }
    const BindingRpcCounts bnd = binding_counts(binding_id);
    if (would_exceed(bnd.streams, 1, limits_.max_streams_per_binding)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kBindingStreamLimit;
        }
        return false;
    }
    ++worker_.streams;
    ++by_request_[request_id].streams;
    ++by_binding_[binding_id].streams;
    return true;
}

void BindingRpcLedger::release_stream(std::uint64_t request_id, const std::string& binding_id) {
    decrement_clamped(&worker_.streams, 1);
    std::map<std::uint64_t, BindingRpcCounts>::iterator rit = by_request_.find(request_id);
    if (rit != by_request_.end()) {
        decrement_clamped(&rit->second.streams, 1);
    }
    std::map<std::string, BindingRpcCounts>::iterator bit = by_binding_.find(binding_id);
    if (bit != by_binding_.end()) {
        decrement_clamped(&bit->second.streams, 1);
    }
    gc_entries(request_id, binding_id);
}

bool BindingRpcLedger::reserve_handle(std::uint64_t request_id, const std::string& binding_id,
                                      BindingQuotaError* error) {
    if (error != nullptr) {
        *error = BindingQuotaError::kOk;
    }
    if (would_exceed(worker_.handles, 1, limits_.max_handles_per_worker)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kWorkerHandleLimit;
        }
        return false;
    }
    const BindingRpcCounts req = request_counts(request_id);
    if (would_exceed(req.handles, 1, limits_.max_handles_per_request)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kRequestHandleLimit;
        }
        return false;
    }
    const BindingRpcCounts bnd = binding_counts(binding_id);
    if (would_exceed(bnd.handles, 1, limits_.max_handles_per_binding)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kBindingHandleLimit;
        }
        return false;
    }
    ++worker_.handles;
    ++by_request_[request_id].handles;
    ++by_binding_[binding_id].handles;
    return true;
}

void BindingRpcLedger::release_handle(std::uint64_t request_id, const std::string& binding_id) {
    decrement_clamped(&worker_.handles, 1);
    std::map<std::uint64_t, BindingRpcCounts>::iterator rit = by_request_.find(request_id);
    if (rit != by_request_.end()) {
        decrement_clamped(&rit->second.handles, 1);
    }
    std::map<std::string, BindingRpcCounts>::iterator bit = by_binding_.find(binding_id);
    if (bit != by_binding_.end()) {
        decrement_clamped(&bit->second.handles, 1);
    }
    gc_entries(request_id, binding_id);
}

bool BindingRpcLedger::reserve_frame(std::uint64_t request_id, const std::string& binding_id,
                                     std::size_t bytes, BindingQuotaError* error) {
    if (error != nullptr) {
        *error = BindingQuotaError::kOk;
    }
    if (would_exceed(worker_.frames, 1, limits_.max_frames_per_worker)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kWorkerFrameLimit;
        }
        return false;
    }
    if (would_exceed(worker_.bytes, bytes, limits_.max_bytes_per_worker)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kWorkerByteLimit;
        }
        return false;
    }
    const BindingRpcCounts req = request_counts(request_id);
    if (would_exceed(req.frames, 1, limits_.max_frames_per_request)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kRequestFrameLimit;
        }
        return false;
    }
    if (would_exceed(req.bytes, bytes, limits_.max_bytes_per_request)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kRequestByteLimit;
        }
        return false;
    }
    const BindingRpcCounts bnd = binding_counts(binding_id);
    if (would_exceed(bnd.frames, 1, limits_.max_frames_per_binding)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kBindingFrameLimit;
        }
        return false;
    }
    if (would_exceed(bnd.bytes, bytes, limits_.max_bytes_per_binding)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kBindingByteLimit;
        }
        return false;
    }
    ++worker_.frames;
    worker_.bytes += bytes;
    BindingRpcCounts& req_slot = by_request_[request_id];
    ++req_slot.frames;
    req_slot.bytes += bytes;
    BindingRpcCounts& bnd_slot = by_binding_[binding_id];
    ++bnd_slot.frames;
    bnd_slot.bytes += bytes;
    return true;
}

void BindingRpcLedger::release_frame(std::uint64_t request_id, const std::string& binding_id,
                                     std::size_t bytes) {
    decrement_clamped(&worker_.frames, 1);
    decrement_clamped(&worker_.bytes, bytes);
    std::map<std::uint64_t, BindingRpcCounts>::iterator rit = by_request_.find(request_id);
    if (rit != by_request_.end()) {
        decrement_clamped(&rit->second.frames, 1);
        decrement_clamped(&rit->second.bytes, bytes);
    }
    std::map<std::string, BindingRpcCounts>::iterator bit = by_binding_.find(binding_id);
    if (bit != by_binding_.end()) {
        decrement_clamped(&bit->second.frames, 1);
        decrement_clamped(&bit->second.bytes, bytes);
    }
    gc_entries(request_id, binding_id);
}

bool BindingRpcLedger::reserve_bytes(std::uint64_t request_id, const std::string& binding_id,
                                     std::size_t bytes, BindingQuotaError* error) {
    if (error != nullptr) {
        *error = BindingQuotaError::kOk;
    }
    if (bytes == 0) {
        return true;
    }
    if (would_exceed(worker_.bytes, bytes, limits_.max_bytes_per_worker)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kWorkerByteLimit;
        }
        return false;
    }
    const BindingRpcCounts req = request_counts(request_id);
    if (would_exceed(req.bytes, bytes, limits_.max_bytes_per_request)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kRequestByteLimit;
        }
        return false;
    }
    const BindingRpcCounts bnd = binding_counts(binding_id);
    if (would_exceed(bnd.bytes, bytes, limits_.max_bytes_per_binding)) {
        if (error != nullptr) {
            *error = BindingQuotaError::kBindingByteLimit;
        }
        return false;
    }
    worker_.bytes += bytes;
    by_request_[request_id].bytes += bytes;
    by_binding_[binding_id].bytes += bytes;
    return true;
}

void BindingRpcLedger::release_bytes(std::uint64_t request_id, const std::string& binding_id,
                                     std::size_t bytes) {
    if (bytes == 0) {
        return;
    }
    decrement_clamped(&worker_.bytes, bytes);
    std::map<std::uint64_t, BindingRpcCounts>::iterator rit = by_request_.find(request_id);
    if (rit != by_request_.end()) {
        decrement_clamped(&rit->second.bytes, bytes);
    }
    std::map<std::string, BindingRpcCounts>::iterator bit = by_binding_.find(binding_id);
    if (bit != by_binding_.end()) {
        decrement_clamped(&bit->second.bytes, bytes);
    }
    gc_entries(request_id, binding_id);
}

BindingRpcCounts BindingRpcLedger::request_counts(std::uint64_t request_id) const {
    std::map<std::uint64_t, BindingRpcCounts>::const_iterator it = by_request_.find(request_id);
    return it == by_request_.end() ? BindingRpcCounts() : it->second;
}

BindingRpcCounts BindingRpcLedger::binding_counts(const std::string& binding_id) const {
    std::map<std::string, BindingRpcCounts>::const_iterator it = by_binding_.find(binding_id);
    return it == by_binding_.end() ? BindingRpcCounts() : it->second;
}

void BindingRpcLedger::clear() {
    worker_ = BindingRpcCounts();
    by_request_.clear();
    by_binding_.clear();
}

void BindingRpcLedger::gc_entries(std::uint64_t request_id, const std::string& binding_id) {
    std::map<std::uint64_t, BindingRpcCounts>::iterator rit = by_request_.find(request_id);
    if (rit != by_request_.end() && rit->second.is_zero()) {
        by_request_.erase(rit);
    }
    std::map<std::string, BindingRpcCounts>::iterator bit = by_binding_.find(binding_id);
    if (bit != by_binding_.end() && bit->second.is_zero()) {
        by_binding_.erase(bit);
    }
}

} // namespace capsid
