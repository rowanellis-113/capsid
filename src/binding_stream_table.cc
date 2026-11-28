// Binding RPC v2 (v0.4.0) M4B: bidirectional credit-based stream proxy state
// machine implementation.

#include "binding_stream_table.h"

#include <limits>
#include <utility>

namespace capsid {

BindingStreamTable::BindingStreamTable(BindingRpcLedger* ledger) : ledger_(ledger) {}

std::uint64_t BindingStreamTable::allocate_stream_id() {
    for (std::size_t probe = 0; probe <= streams_.size(); ++probe) {
        const std::uint64_t candidate = next_stream_id_;
        next_stream_id_ =
            candidate == (std::numeric_limits<std::uint64_t>::max)() ? 1 : candidate + 1;
        if (candidate != 0 && streams_.count(candidate) == 0) {
            return candidate;
        }
    }
    return 0;
}

bool BindingStreamTable::open_stream(BindingStreamKind kind, BindingStreamSide producer_side,
                                     std::uint64_t owner_request_id,
                                     const std::string& owner_binding_id, std::size_t table_index,
                                     std::uint64_t* out_stream_id, BindingQuotaError* quota_error,
                                     std::size_t initial_credit_bytes) {
    if (out_stream_id != nullptr) {
        *out_stream_id = 0;
    }
    if (ledger_ == nullptr || owner_request_id == 0 || owner_binding_id.empty()) {
        return false;
    }
    if (!ledger_->reserve_stream(owner_request_id, owner_binding_id, quota_error)) {
        return false;
    }
    const std::uint64_t id = allocate_stream_id();
    if (id == 0) {
        ledger_->release_stream(owner_request_id, owner_binding_id);
        return false;
    }
    BindingStreamRecord rec;
    rec.stream_id = id;
    rec.kind = kind;
    rec.producer_side = producer_side;
    rec.consumer_side = (producer_side == BindingStreamSide::kUser) ? BindingStreamSide::kBinding
                                                                    : BindingStreamSide::kUser;
    rec.owner_request_id = owner_request_id;
    rec.owner_binding_id = owner_binding_id;
    rec.table_index = table_index;
    rec.state = BindingStreamState::kOpen;
    rec.credit_bytes = initial_credit_bytes;
    rec.max_credit_bytes =
        initial_credit_bytes > 0 ? initial_credit_bytes : kBindingStreamInitialCreditBytes;

    streams_.emplace(id, std::move(rec));
    if (out_stream_id != nullptr) {
        *out_stream_id = id;
    }
    return true;
}

BindingStreamRecord* BindingStreamTable::find(std::uint64_t stream_id) {
    std::map<std::uint64_t, BindingStreamRecord>::iterator it = streams_.find(stream_id);
    return it == streams_.end() ? nullptr : &it->second;
}

const BindingStreamRecord* BindingStreamTable::find(std::uint64_t stream_id) const {
    std::map<std::uint64_t, BindingStreamRecord>::const_iterator it = streams_.find(stream_id);
    return it == streams_.end() ? nullptr : &it->second;
}

bool BindingStreamTable::enqueue_bytes(std::uint64_t stream_id, const std::uint8_t* data,
                                       std::size_t size, std::string* error) {
    BindingStreamRecord* rec = find(stream_id);
    if (rec == nullptr || rec->state != BindingStreamState::kOpen) {
        if (error != nullptr) {
            *error = "binding stream is not open";
        }
        return false;
    }
    if (size > kBindingStreamMaxChunkBytes) {
        if (error != nullptr) {
            *error = "binding stream chunk exceeds maximum frame size";
        }
        return false;
    }
    if (size > rec->credit_bytes) {
        if (error != nullptr) {
            *error = "insufficient stream credit";
        }
        return false;
    }
    if (rec->queued_frames + rec->staged_frames >= kBindingStreamMaxQueuedFrames) {
        if (error != nullptr) {
            *error = "binding stream queued frame limit exceeded";
        }
        return false;
    }
    const std::size_t total_buffered = rec->queued_bytes + rec->staged_bytes;
    if (size > kBindingStreamMaxBufferedBytes ||
        total_buffered > kBindingStreamMaxBufferedBytes - size) {
        if (error != nullptr) {
            *error = "binding stream buffered byte limit exceeded";
        }
        return false;
    }
    BindingQuotaError qerr = BindingQuotaError::kOk;
    if (!ledger_->reserve_frame(rec->owner_request_id, rec->owner_binding_id, size, &qerr)) {
        if (error != nullptr) {
            *error = binding_quota_error_message(qerr);
        }
        return false;
    }
    BindingStreamFrame frame;
    frame.type = BindingStreamFrame::Type::kBytes;
    if (size > 0 && data != nullptr) {
        std::shared_ptr<std::vector<std::uint8_t>> owned(
            new std::vector<std::uint8_t>(data, data + size));
        frame.shared_bytes = std::move(owned);
        frame.byte_offset = 0;
        frame.byte_length = size;
    }
    frame.credit_bytes = size;
    frame.accounted_bytes = size;
    rec->queue.push_back(std::move(frame));
    ++rec->queued_frames;
    rec->queued_bytes += size;
    rec->credit_bytes -= size;
    return true;
}

bool BindingStreamTable::enqueue_value(std::uint64_t stream_id, NeutralValue value,
                                       std::size_t accounted_bytes, std::string* error) {
    BindingStreamRecord* rec = find(stream_id);
    if (rec == nullptr || rec->state != BindingStreamState::kOpen) {
        if (error != nullptr) {
            *error = "binding stream is not open";
        }
        return false;
    }
    const std::size_t weight = accounted_bytes > 0 ? accounted_bytes : 1;
    if (weight > kBindingStreamMaxChunkBytes) {
        if (error != nullptr) {
            *error = "binding stream chunk exceeds maximum frame size";
        }
        return false;
    }
    if (weight > rec->credit_bytes) {
        if (error != nullptr) {
            *error = "insufficient stream credit";
        }
        return false;
    }
    if (rec->queued_frames + rec->staged_frames >= kBindingStreamMaxQueuedFrames) {
        if (error != nullptr) {
            *error = "binding stream queued frame limit exceeded";
        }
        return false;
    }
    const std::size_t total_buffered = rec->queued_bytes + rec->staged_bytes;
    if (weight > kBindingStreamMaxBufferedBytes ||
        total_buffered > kBindingStreamMaxBufferedBytes - weight) {
        if (error != nullptr) {
            *error = "binding stream buffered byte limit exceeded";
        }
        return false;
    }
    BindingQuotaError qerr = BindingQuotaError::kOk;
    if (!ledger_->reserve_frame(rec->owner_request_id, rec->owner_binding_id, weight, &qerr)) {
        if (error != nullptr) {
            *error = binding_quota_error_message(qerr);
        }
        return false;
    }
    BindingStreamFrame frame;
    frame.type = BindingStreamFrame::Type::kValue;
    frame.value = std::move(value);
    frame.credit_bytes = weight;
    frame.accounted_bytes = weight;
    rec->queue.push_back(std::move(frame));
    ++rec->queued_frames;
    rec->queued_bytes += weight;
    rec->credit_bytes -= weight;
    return true;
}

bool BindingStreamTable::stage_pending(std::uint64_t stream_id, std::size_t bytes,
                                       std::size_t frames, std::string* error) {
    BindingStreamRecord* rec = find(stream_id);
    if (rec == nullptr || rec->state != BindingStreamState::kOpen) {
        if (error != nullptr) {
            *error = "binding stream is not open";
        }
        return false;
    }
    const std::size_t frame_count = frames > 0 ? frames : 1;
    const std::size_t total_frames = rec->queued_frames + rec->staged_frames;
    if (frame_count > kBindingStreamMaxQueuedFrames ||
        total_frames > kBindingStreamMaxQueuedFrames - frame_count) {
        if (error != nullptr) {
            *error = "binding stream queued frame limit exceeded";
        }
        return false;
    }
    const std::size_t total_buffered = rec->queued_bytes + rec->staged_bytes;
    if (bytes > kBindingStreamMaxBufferedBytes ||
        total_buffered > kBindingStreamMaxBufferedBytes - bytes) {
        if (error != nullptr) {
            *error = "binding stream buffered byte limit exceeded";
        }
        return false;
    }
    // Reserve the full byte payload on the first frame and 0 bytes on any
    // additional slice frames so rollback is atomic and exact.
    BindingQuotaError qerr = BindingQuotaError::kOk;
    if (!ledger_->reserve_frame(rec->owner_request_id, rec->owner_binding_id, bytes, &qerr)) {
        if (error != nullptr) {
            *error = binding_quota_error_message(qerr);
        }
        return false;
    }
    for (std::size_t i = 1; i < frame_count; ++i) {
        if (!ledger_->reserve_frame(rec->owner_request_id, rec->owner_binding_id, 0, &qerr)) {
            ledger_->release_frame(rec->owner_request_id, rec->owner_binding_id, bytes);
            for (std::size_t j = 1; j < i; ++j) {
                ledger_->release_frame(rec->owner_request_id, rec->owner_binding_id, 0);
            }
            if (error != nullptr) {
                *error = binding_quota_error_message(qerr);
            }
            return false;
        }
    }
    rec->staged_bytes += bytes;
    rec->staged_frames += frame_count;
    return true;
}

void BindingStreamTable::release_staged(std::uint64_t stream_id, std::size_t bytes,
                                        std::size_t frames) {
    BindingStreamRecord* rec = find(stream_id);
    if (rec == nullptr) {
        return;
    }
    const std::size_t rel_bytes = (bytes <= rec->staged_bytes) ? bytes : rec->staged_bytes;
    const std::size_t rel_frames = (frames <= rec->staged_frames) ? frames : rec->staged_frames;
    rec->staged_bytes -= rel_bytes;
    rec->staged_frames -= rel_frames;
    if (rel_frames > 0) {
        ledger_->release_frame(rec->owner_request_id, rec->owner_binding_id, rel_bytes);
        for (std::size_t i = 1; i < rel_frames; ++i) {
            ledger_->release_frame(rec->owner_request_id, rec->owner_binding_id, 0);
        }
    } else if (rel_bytes > 0) {
        ledger_->release_bytes(rec->owner_request_id, rec->owner_binding_id, rel_bytes);
    }
}

bool BindingStreamTable::admit_staged_shared_bytes(
    std::uint64_t stream_id, const std::shared_ptr<const std::vector<std::uint8_t>>& backing,
    std::size_t offset, std::size_t length, std::size_t staged_bytes_transferred,
    std::size_t staged_frames_consumed, std::string* error) {
    BindingStreamRecord* rec = find(stream_id);
    if (rec == nullptr || rec->state != BindingStreamState::kOpen) {
        if (error != nullptr) {
            *error = "binding stream is not open";
        }
        return false;
    }
    if (length > kBindingStreamMaxChunkBytes) {
        if (error != nullptr) {
            *error = "binding stream chunk exceeds maximum frame size";
        }
        return false;
    }
    if (length > rec->credit_bytes) {
        if (error != nullptr) {
            *error = "insufficient stream credit";
        }
        return false;
    }
    if (length > 0 && (!backing || offset > backing->size() || length > backing->size() - offset)) {
        if (error != nullptr) {
            *error = "staged stream byte slice out of bounds";
        }
        return false;
    }
    if (staged_bytes_transferred > rec->staged_bytes ||
        staged_frames_consumed > rec->staged_frames) {
        if (error != nullptr) {
            *error = "staged stream accounting underflow";
        }
        return false;
    }
    if (staged_bytes_transferred > 0 && (!backing || staged_bytes_transferred != backing->size())) {
        if (error != nullptr) {
            *error = "staged backing transfer size mismatch";
        }
        return false;
    }
    // Transfer frame ownership from staged -> queued. When partial sub-slicing
    // emits an intermediate frame (`staged_frames_consumed == 0`), reserve one
    // 0-byte frame slot in the ledger while keeping the full backing block
    // accounted in `staged_bytes` until its final sub-slice transfers ownership.
    if (staged_frames_consumed == 0) {
        if (rec->queued_frames + rec->staged_frames >= kBindingStreamMaxQueuedFrames) {
            if (error != nullptr) {
                *error = "binding stream queued frame limit exceeded";
            }
            return false;
        }
        BindingQuotaError qerr = BindingQuotaError::kOk;
        if (!ledger_->reserve_frame(rec->owner_request_id, rec->owner_binding_id, 0, &qerr)) {
            if (error != nullptr) {
                *error = binding_quota_error_message(qerr);
            }
            return false;
        }
    } else if (staged_frames_consumed > 1) {
        for (std::size_t i = 1; i < staged_frames_consumed; ++i) {
            ledger_->release_frame(rec->owner_request_id, rec->owner_binding_id, 0);
        }
    }
    rec->staged_bytes -= staged_bytes_transferred;
    rec->staged_frames -= staged_frames_consumed;
    BindingStreamFrame frame;
    frame.type = BindingStreamFrame::Type::kBytes;
    if (length > 0 && backing) {
        frame.shared_bytes = backing;
        frame.byte_offset = offset;
        frame.byte_length = length;
    }
    frame.credit_bytes = length;
    frame.accounted_bytes = staged_bytes_transferred;
    rec->queue.push_back(std::move(frame));
    ++rec->queued_frames;
    rec->queued_bytes += staged_bytes_transferred;
    rec->credit_bytes -= length;
    return true;
}

bool BindingStreamTable::admit_staged_value(std::uint64_t stream_id, NeutralValue value,
                                            std::size_t accounted_bytes, std::string* error) {
    BindingStreamRecord* rec = find(stream_id);
    if (rec == nullptr || rec->state != BindingStreamState::kOpen) {
        if (error != nullptr) {
            *error = "binding stream is not open";
        }
        return false;
    }
    const std::size_t weight = accounted_bytes > 0 ? accounted_bytes : 1;
    if (weight > kBindingStreamMaxChunkBytes) {
        if (error != nullptr) {
            *error = "binding stream chunk exceeds maximum frame size";
        }
        return false;
    }
    if (weight > rec->credit_bytes) {
        if (error != nullptr) {
            *error = "insufficient stream credit";
        }
        return false;
    }
    if (weight > rec->staged_bytes || rec->staged_frames == 0) {
        if (error != nullptr) {
            *error = "staged stream accounting underflow";
        }
        return false;
    }
    rec->staged_bytes -= weight;
    --rec->staged_frames;
    BindingStreamFrame frame;
    frame.type = BindingStreamFrame::Type::kValue;
    frame.value = std::move(value);
    frame.credit_bytes = weight;
    frame.accounted_bytes = weight;
    rec->queue.push_back(std::move(frame));
    ++rec->queued_frames;
    rec->queued_bytes += weight;
    rec->credit_bytes -= weight;
    return true;
}

bool BindingStreamTable::enqueue_end(std::uint64_t stream_id) {
    BindingStreamRecord* rec = find(stream_id);
    if (rec == nullptr || rec->state != BindingStreamState::kOpen) {
        return false;
    }
    rec->state = BindingStreamState::kHalfClosed;
    BindingStreamFrame frame;
    frame.type = BindingStreamFrame::Type::kEnd;
    frame.accounted_bytes = 0;
    rec->queue.push_back(std::move(frame));
    return true;
}

bool BindingStreamTable::fail_from_producer(std::uint64_t stream_id, const RemoteErrorInfo& error) {
    BindingStreamRecord* rec = find(stream_id);
    if (rec == nullptr || rec->state == BindingStreamState::kClosed ||
        rec->state == BindingStreamState::kErrored || rec->state == BindingStreamState::kCanceled) {
        return false;
    }
    release_queued_frames(rec);
    rec->state = BindingStreamState::kErrored;
    rec->has_terminal_error = true;
    rec->terminal_error = error;
    BindingStreamFrame frame;
    frame.type = BindingStreamFrame::Type::kError;
    frame.error = error;
    frame.accounted_bytes = 0;
    rec->queue.push_back(std::move(frame));
    return true;
}

bool BindingStreamTable::pop_frame(std::uint64_t stream_id, BindingStreamFrame* out_frame) {
    BindingStreamRecord* rec = find(stream_id);
    if (rec == nullptr || rec->queue.empty()) {
        return false;
    }
    BindingStreamFrame front = std::move(rec->queue.front());
    rec->queue.pop_front();
    if (front.type == BindingStreamFrame::Type::kBytes ||
        front.type == BindingStreamFrame::Type::kValue) {
        if (rec->queued_frames > 0) {
            --rec->queued_frames;
        }
        rec->queued_bytes = (rec->queued_bytes >= front.accounted_bytes)
                                ? (rec->queued_bytes - front.accounted_bytes)
                                : 0;
        ledger_->release_frame(rec->owner_request_id, rec->owner_binding_id, front.accounted_bytes);
    } else if (front.type == BindingStreamFrame::Type::kEnd) {
        rec->state = BindingStreamState::kClosed;
    }
    if (out_frame != nullptr) {
        *out_frame = std::move(front);
    }
    return true;
}

bool BindingStreamTable::grant_credit(std::uint64_t stream_id, std::size_t bytes) {
    BindingStreamRecord* rec = find(stream_id);
    if (rec == nullptr || rec->state != BindingStreamState::kOpen || bytes == 0) {
        return false;
    }
    const bool was_starved = (rec->credit_bytes == 0);
    if (bytes >= rec->max_credit_bytes || rec->credit_bytes >= rec->max_credit_bytes - bytes) {
        rec->credit_bytes = rec->max_credit_bytes;
    } else {
        rec->credit_bytes += bytes;
    }
    return was_starved && rec->credit_bytes > 0;
}

bool BindingStreamTable::cancel_from_consumer(std::uint64_t stream_id,
                                              const RemoteErrorInfo& reason) {
    BindingStreamRecord* rec = find(stream_id);
    if (rec == nullptr || rec->state == BindingStreamState::kClosed ||
        rec->state == BindingStreamState::kCanceled) {
        return false;
    }
    release_queued_frames(rec);
    rec->state = BindingStreamState::kCanceled;
    rec->has_terminal_error = true;
    rec->terminal_error = reason;
    return true;
}

bool BindingStreamTable::close_and_erase(std::uint64_t stream_id) {
    std::map<std::uint64_t, BindingStreamRecord>::iterator it = streams_.find(stream_id);
    if (it == streams_.end()) {
        return false;
    }
    release_queued_frames(&it->second);
    const std::uint64_t req_id = it->second.owner_request_id;
    const std::string binding_id = it->second.owner_binding_id;
    streams_.erase(it);
    ledger_->release_stream(req_id, binding_id);
    return true;
}

std::vector<std::uint64_t> BindingStreamTable::collect_for_request(std::uint64_t request_id) const {
    std::vector<std::uint64_t> ids;
    for (std::map<std::uint64_t, BindingStreamRecord>::const_iterator it = streams_.begin();
         it != streams_.end(); ++it) {
        if (request_id == 0 || it->second.owner_request_id == request_id) {
            ids.push_back(it->first);
        }
    }
    return ids;
}

std::vector<std::uint64_t> BindingStreamTable::collect_all() const {
    return collect_for_request(0);
}

void BindingStreamTable::clear() {
    while (!streams_.empty()) {
        close_and_erase(streams_.begin()->first);
    }
}

void BindingStreamTable::release_queued_frames(BindingStreamRecord* rec) {
    if (rec == nullptr) {
        return;
    }
    while (!rec->queue.empty()) {
        const BindingStreamFrame& front = rec->queue.front();
        if (front.type == BindingStreamFrame::Type::kBytes ||
            front.type == BindingStreamFrame::Type::kValue) {
            ledger_->release_frame(rec->owner_request_id, rec->owner_binding_id,
                                   front.accounted_bytes);
        }
        rec->queue.pop_front();
    }
    rec->queued_frames = 0;
    rec->queued_bytes = 0;
    if (rec->staged_frames > 0) {
        ledger_->release_frame(rec->owner_request_id, rec->owner_binding_id, rec->staged_bytes);
        for (std::size_t i = 1; i < rec->staged_frames; ++i) {
            ledger_->release_frame(rec->owner_request_id, rec->owner_binding_id, 0);
        }
    } else if (rec->staged_bytes > 0) {
        ledger_->release_bytes(rec->owner_request_id, rec->owner_binding_id, rec->staged_bytes);
    }
    rec->staged_bytes = 0;
    rec->staged_frames = 0;
}

} // namespace capsid
