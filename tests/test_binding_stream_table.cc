// Binding RPC v2 (v0.4.0) M4B unit tests: credit-based stream proxy state
// machine, backpressure credit refill, per-stream and 3-tier ledger ceilings,
// producer error / consumer cancel transitions, and deterministic cleanup.

#include "binding_rpc_ledger.h"
#include "binding_stream_table.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "test-binding-stream-table: FAIL: " << message << std::endl;
        std::exit(1);
    }
}

void test_open_credit_flow_and_close() {
    capsid::BindingRpcLedger ledger;
    capsid::BindingStreamTable table(&ledger);

    std::uint64_t id = 0;
    capsid::BindingQuotaError qerr = capsid::BindingQuotaError::kOk;
    require(table.open_stream(capsid::BindingStreamKind::kReadable,
                              capsid::BindingStreamSide::kBinding, 10, "svc", 0, &id, &qerr, 1024),
            "open stream");
    require(id == 1 && table.active_streams() == 1, "stream id and count");
    require(ledger.worker_counts().streams == 1, "ledger reserved stream");

    const std::vector<std::uint8_t> chunk(600, 0x41);
    std::string err;
    require(table.enqueue_bytes(id, chunk.data(), chunk.size(), &err), "enqueue 600B");
    require(table.find(id)->credit_bytes == 424, "credit deducted to 424");
    require(ledger.worker_counts().frames == 1 && ledger.worker_counts().bytes == 600,
            "ledger accounted 1 frame / 600 bytes");

    // Enqueue another 600B -> credit clamps to 0 (producer now paused).
    require(table.enqueue_bytes(id, chunk.data(), chunk.size(), &err), "enqueue second 600B");
    require(table.find(id)->credit_bytes == 0, "credit clamped at 0");

    // Pop first frame and grant credit -> producer resumes!
    capsid::BindingStreamFrame frame;
    require(table.pop_frame(id, &frame) && frame.type == capsid::BindingStreamFrame::Type::kBytes &&
                frame.bytes.size() == 600,
            "pop first frame");
    require(ledger.worker_counts().frames == 1 && ledger.worker_counts().bytes == 600,
            "ledger released popped frame");
    require(table.grant_credit(id, 600), "grant_credit woke starved producer");
    require(table.find(id)->credit_bytes == 600, "credit refilled to 600");
    // Credit saturates at max_credit_bytes (1024).
    require(!table.grant_credit(id, 5000), "already non-starved returns false");
    require(table.find(id)->credit_bytes == 1024, "credit saturated at max_credit_bytes");

    // End + drain -> Closed.
    require(table.enqueue_end(id), "enqueue_end");
    require(table.find(id)->state == capsid::BindingStreamState::kHalfClosed, "half-closed");
    require(!table.enqueue_bytes(id, chunk.data(), 10, &err), "cannot enqueue after end");
    require(table.pop_frame(id, &frame) && frame.type == capsid::BindingStreamFrame::Type::kBytes,
            "pop second data frame");
    require(table.pop_frame(id, &frame) && frame.type == capsid::BindingStreamFrame::Type::kEnd,
            "pop end frame");
    require(table.find(id)->state == capsid::BindingStreamState::kClosed, "closed after end");

    require(table.close_and_erase(id), "close_and_erase");
    require(table.empty() && ledger.empty(), "table and ledger completely empty");
}

void test_limits_error_cancel_and_request_teardown() {
    capsid::BindingRpcLimits limits;
    limits.max_streams_per_request = 2;
    limits.max_bytes_per_request = 1000;
    capsid::BindingRpcLedger ledger(limits);
    capsid::BindingStreamTable table(&ledger);

    std::uint64_t s1 = 0, s2 = 0, s3 = 0;
    capsid::BindingQuotaError qerr = capsid::BindingQuotaError::kOk;
    require(table.open_stream(capsid::BindingStreamKind::kReadable,
                              capsid::BindingStreamSide::kUser, 42, "db", 0, &s1, &qerr),
            "open s1");
    require(table.open_stream(capsid::BindingStreamKind::kWritable,
                              capsid::BindingStreamSide::kBinding, 42, "db", 0, &s2, &qerr),
            "open s2");
    require(!table.open_stream(capsid::BindingStreamKind::kReadable,
                               capsid::BindingStreamSide::kUser, 42, "db", 0, &s3, &qerr) &&
                qerr == capsid::BindingQuotaError::kRequestStreamLimit,
            "per-request stream cap enforced");

    // Chunk > 64 KiB rejected; request byte cap 1000B enforced.
    std::vector<std::uint8_t> big(capsid::kBindingStreamMaxChunkBytes + 1, 1);
    std::string err;
    require(!table.enqueue_bytes(s1, big.data(), big.size(), &err), "chunk > 64 KiB rejected");

    std::vector<std::uint8_t> chunk(600, 2);
    require(table.enqueue_bytes(s1, chunk.data(), chunk.size(), &err), "s1 600B ok");
    require(!table.enqueue_bytes(s2, chunk.data(), chunk.size(), &err),
            "s2 600B hits 1000B request byte cap");

    // Producer failure drops queued frames and queues terminal kError.
    require(table.fail_from_producer(s1, capsid::RemoteErrorInfo::make_type_error("broken pipe")),
            "fail_from_producer");
    require(ledger.worker_counts().frames == 0 && ledger.worker_counts().bytes == 0,
            "fail_from_producer released queued frame bytes");
    capsid::BindingStreamFrame err_frame;
    require(table.pop_frame(s1, &err_frame) &&
                err_frame.type == capsid::BindingStreamFrame::Type::kError &&
                err_frame.error.kind == capsid::RemoteErrorKind::kTypeError &&
                err_frame.error.message == "broken pipe",
            "terminal error frame popped");

    // Consumer cancel on s2 releases queued bytes immediately.
    require(table.enqueue_bytes(s2, chunk.data(), 400, &err), "s2 400B ok");
    require(table.cancel_from_consumer(
                s2, capsid::RemoteErrorInfo::make_dom_exception("AbortError", "consumer stopped")),
            "cancel_from_consumer");
    require(ledger.worker_counts().frames == 0 && ledger.worker_counts().bytes == 0,
            "cancel_from_consumer released queued bytes");

    // Request teardown collects all streams for request 42.
    const std::vector<std::uint64_t> owned = table.collect_for_request(42);
    require(owned.size() == 2, "collect_for_request returned both streams");
    for (std::size_t i = 0; i < owned.size(); ++i) {
        require(table.close_and_erase(owned[i]), "teardown stream");
    }
    require(table.empty() && ledger.empty(), "clean teardown leaves zero leaks");
}

} // namespace

int main() {
    test_open_credit_flow_and_close();
    test_limits_error_cancel_and_request_teardown();
    std::cout << "test-binding-stream-table: PASS (2 suites)" << std::endl;
    return 0;
}
