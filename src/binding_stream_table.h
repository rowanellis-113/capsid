// Binding RPC v2 (v0.4.0) M4B: pure C++11 bidirectional credit-based stream
// proxy state machine.
//
// Owns stream ids, lifecycle states (Open -> HalfClosed -> Closed / Errored /
// Canceled), byte credit windows, per-stream buffering bounds, and atomic
// accounting with BindingRpcLedger. Independent of QuickJS and libuv so every
// backpressure, credit refill, error, cancel, and teardown invariant is
// unit-testable directly.

#ifndef CAPSID_BINDING_STREAM_TABLE_H
#define CAPSID_BINDING_STREAM_TABLE_H

#include "binding_rpc.h"
#include "binding_rpc_ledger.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace capsid {

constexpr std::size_t kBindingStreamInitialCreditBytes = 64U * 1024U;
constexpr std::size_t kBindingStreamMaxChunkBytes = 64U * 1024U;
constexpr std::size_t kBindingStreamMaxBufferedBytes = 2U * 1024U * 1024U;
constexpr std::size_t kBindingStreamMaxQueuedFrames = 256U;

enum class BindingStreamKind {
    kReadable = 0,
    kWritable = 1,
};

enum class BindingStreamSide {
    kUser = 0,
    kBinding = 1,
};

enum class BindingStreamState {
    kOpen = 0,
    kHalfClosed = 1, // producer sent END; queued frames still draining to consumer
    kClosed = 2,
    kErrored = 3,
    kCanceled = 4,
};

struct BindingStreamFrame {
    enum class Type {
        kBytes = 0,
        kValue = 1,
        kEnd = 2,
        kError = 3,
    };

    Type type = Type::kBytes;
    std::vector<std::uint8_t> bytes;
    NeutralValue value;
    RemoteErrorInfo error;
    std::size_t accounted_bytes = 0;
};

struct BindingStreamRecord {
    std::uint64_t stream_id = 0;
    BindingStreamKind kind = BindingStreamKind::kReadable;
    BindingStreamSide producer_side = BindingStreamSide::kUser;
    BindingStreamSide consumer_side = BindingStreamSide::kBinding;
    std::uint64_t owner_request_id = 0;
    std::string owner_binding_id;
    std::size_t table_index = 0;

    BindingStreamState state = BindingStreamState::kOpen;
    std::size_t credit_bytes = kBindingStreamInitialCreditBytes;
    std::size_t max_credit_bytes = kBindingStreamInitialCreditBytes;
    std::size_t queued_bytes = 0;
    std::size_t queued_frames = 0;

    bool producer_pull_inflight = false;
    bool consumer_waiting = false;
    bool cancel_notified = false;
    bool has_terminal_error = false;
    RemoteErrorInfo terminal_error;

    std::deque<BindingStreamFrame> queue;
};

class BindingStreamTable {
  public:
    explicit BindingStreamTable(BindingRpcLedger* ledger);

    // Allocates a stream id, reserves a stream slot in the 3-tier ledger, and
    // initializes its credit window to `initial_credit_bytes`.
    bool open_stream(BindingStreamKind kind, BindingStreamSide producer_side,
                     std::uint64_t owner_request_id, const std::string& owner_binding_id,
                     std::size_t table_index, std::uint64_t* out_stream_id,
                     BindingQuotaError* quota_error = nullptr,
                     std::size_t initial_credit_bytes = kBindingStreamInitialCreditBytes);

    BindingStreamRecord* find(std::uint64_t stream_id);
    const BindingStreamRecord* find(std::uint64_t stream_id) const;

    // Enqueues a byte chunk from producer -> consumer, enforcing per-stream
    // and 3-tier ledger frame/byte caps and deducting available byte credit.
    bool enqueue_bytes(std::uint64_t stream_id, const std::uint8_t* data, std::size_t size,
                       std::string* error = nullptr);

    // Enqueues a neutral value chunk from producer -> consumer with an
    // explicit accounted byte weight.
    bool enqueue_value(std::uint64_t stream_id, NeutralValue value, std::size_t accounted_bytes,
                       std::string* error = nullptr);

    // Producer marks normal EOF (Open -> HalfClosed).
    bool enqueue_end(std::uint64_t stream_id);

    // Producer marks abnormal error (Open -> Errored, clears queued data frames
    // and queues a terminal kError frame for the consumer).
    bool fail_from_producer(std::uint64_t stream_id, const RemoteErrorInfo& error);

    // Consumer pops the next queued frame, releasing its frame/byte accounting
    // in both the stream record and the 3-tier ledger.
    bool pop_frame(std::uint64_t stream_id, BindingStreamFrame* out_frame);

    // Consumer grants byte credit back after draining delivered bytes.
    // Credit saturates at `max_credit_bytes`. Returns true if the producer was
    // credit-starved and can now resume pulling/writing.
    bool grant_credit(std::uint64_t stream_id, std::size_t bytes);

    // Consumer cancels (ReadableStream) or aborts (WritableStream). Drops
    // queued frames and transitions to kCanceled.
    bool cancel_from_consumer(std::uint64_t stream_id, const RemoteErrorInfo& reason);

    // Marks the stream completely finished and releases its ledger stream slot
    // and any remaining queued frame accounting. Safe to call repeatedly.
    bool close_and_erase(std::uint64_t stream_id);

    // Collects all stream ids owned by `request_id` (or all streams when
    // request_id == 0) for deterministic teardown on cancel/timeout/poison.
    std::vector<std::uint64_t> collect_for_request(std::uint64_t request_id) const;
    std::vector<std::uint64_t> collect_all() const;

    std::size_t active_streams() const { return streams_.size(); }
    bool empty() const { return streams_.empty(); }
    void clear();

  private:
    std::uint64_t allocate_stream_id();
    void release_queued_frames(BindingStreamRecord* rec);

    BindingRpcLedger* ledger_;
    std::uint64_t next_stream_id_ = 1;
    std::map<std::uint64_t, BindingStreamRecord> streams_;
};

} // namespace capsid

#endif
