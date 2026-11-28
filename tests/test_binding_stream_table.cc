// Binding RPC v2 (v0.4.0) M4B unit tests: credit-based stream proxy state
// machine, backpressure credit refill, per-stream and 3-tier ledger ceilings,
// producer error / consumer cancel transitions, and deterministic cleanup.

#include "binding_rpc_ledger.h"
#include "binding_stream_table.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
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

    // Strict credit conservation (F5): 600B must be rejected when only 424B
    // credit remains; exact 424B succeeds and reduces credit to 0.
    require(!table.enqueue_bytes(id, chunk.data(), chunk.size(), &err) &&
                err == "insufficient stream credit",
            "over-credit enqueue must be rejected");
    require(table.find(id)->credit_bytes == 424 && ledger.worker_counts().bytes == 600,
            "rejected enqueue must not mutate credit or ledger");
    require(table.enqueue_bytes(id, chunk.data(), 424, &err), "enqueue exact remaining 424B");
    require(table.find(id)->credit_bytes == 0, "credit exactly 0");
    require(!table.enqueue_bytes(id, chunk.data(), 1, &err), "cannot enqueue at 0 credit");

    // Pop first frame and grant credit -> producer resumes!
    capsid::BindingStreamFrame frame;
    require(table.pop_frame(id, &frame) && frame.type == capsid::BindingStreamFrame::Type::kBytes &&
                frame.size() == 600 && frame.credit_bytes == 600,
            "pop first frame");
    require(ledger.worker_counts().frames == 1 && ledger.worker_counts().bytes == 424,
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
    require(table.pop_frame(id, &frame) && frame.type == capsid::BindingStreamFrame::Type::kBytes &&
                frame.size() == 424,
            "pop second data frame");
    require(table.pop_frame(id, &frame) && frame.type == capsid::BindingStreamFrame::Type::kEnd,
            "pop end frame");
    require(table.find(id)->state == capsid::BindingStreamState::kClosed, "closed after end");

    require(table.close_and_erase(id), "close_and_erase");
    require(table.empty() && ledger.empty(), "table and ledger completely empty");
}

void test_staged_native_accounting_and_credit_slicing() {
    capsid::BindingRpcLedger ledger;
    capsid::BindingStreamTable table(&ledger);

    std::uint64_t id = 0;
    capsid::BindingQuotaError qerr = capsid::BindingQuotaError::kOk;
    require(table.open_stream(capsid::BindingStreamKind::kWritable,
                              capsid::BindingStreamSide::kUser, 99, "svc", 0, &id, &qerr,
                              capsid::kBindingStreamInitialCreditBytes),
            "open writable stream");

    // Stage a 150 KiB native write split into 3 independent <=64 KiB backing blocks.
    const std::size_t s1 = 64U * 1024U;
    const std::size_t s2 = 64U * 1024U;
    const std::size_t s3 = 22U * 1024U;
    const std::size_t total_len = s1 + s2 + s3;
    std::shared_ptr<const std::vector<std::uint8_t>> blk1(new std::vector<std::uint8_t>(s1, 0x7A));
    std::shared_ptr<const std::vector<std::uint8_t>> blk2(new std::vector<std::uint8_t>(s2, 0x7B));
    std::string err;
    require(table.stage_pending(id, total_len, 3, &err), "stage 150 KiB");
    require(ledger.worker_counts().bytes == total_len && ledger.worker_counts().frames == 3,
            "ledger immediately accounted full staged 150 KiB / 3 frames");
    require(table.find(id)->staged_bytes == total_len && table.find(id)->queued_bytes == 0,
            "stream record tracks staged bytes");

    // Attempting to stage > 2 MiB total fails closed without mutating ledger.
    require(!table.stage_pending(id, capsid::kBindingStreamMaxBufferedBytes, 1, &err),
            "per-stream 2 MiB cap includes staged bytes");
    require(ledger.worker_counts().bytes == total_len && ledger.worker_counts().frames == 3,
            "rejected stage did not mutate ledger");

    // Admit block 1 (64 KiB, completes blk1) -> credit becomes 0, NO double-charging!
    require(table.admit_staged_shared_bytes(id, blk1, 0, s1, s1, 1, &err), "admit 64 KiB block 1");
    blk1.reset();
    require(table.find(id)->credit_bytes == 0, "credit exhausted after 64 KiB");
    require(ledger.worker_counts().bytes == total_len && ledger.worker_counts().frames == 3,
            "admitting staged slice did not double-charge ledger");
    require(!table.admit_staged_shared_bytes(id, blk2, 0, s2, s2, 1, &err),
            "block 2 blocked at 0 credit");

    // Consumer pops block 1 and grants 64 KiB credit -> ledger drops by 64 KiB / 1 frame.
    {
        capsid::BindingStreamFrame frame;
        require(table.pop_frame(id, &frame) && frame.size() == s1 && frame.credit_bytes == s1,
                "pop block 1");
    }
    require(ledger.worker_counts().bytes == total_len - s1 && ledger.worker_counts().frames == 2,
            "pop released block 1 bytes");
    require(table.grant_credit(id, s1), "grant 64 KiB credit");

    // Cancel remaining staged bytes -> ledger returns cleanly to 0!
    require(table.cancel_from_consumer(
                id, capsid::RemoteErrorInfo::make_dom_exception("AbortError", "cancel")),
            "cancel with staged bytes");
    require(ledger.worker_counts().bytes == 0 && ledger.worker_counts().frames == 0,
            "cancel released remaining staged bytes and frames");
    require(table.close_and_erase(id) && table.empty() && ledger.empty(),
            "staged cleanup zero leak");
}

// R2/F3: Mid-block partial confirmation must keep the full backing allocation
// accounted across all 3 ledger tiers until its final sub-slice pops and drops
// the last shared_ptr reference; also covers enqueue_value, admit_staged_value,
// and release_staged directly.
void test_shared_backing_partial_delivery_and_value_staging() {
    capsid::BindingRpcLedger ledger;
    capsid::BindingStreamTable table(&ledger);

    std::uint64_t id = 0;
    capsid::BindingQuotaError qerr = capsid::BindingQuotaError::kOk;
    const std::size_t window = 16U * 1024U; // 16 KiB flow-control credit window
    require(table.open_stream(capsid::BindingStreamKind::kReadable,
                              capsid::BindingStreamSide::kBinding, 77, "svc", 0, &id, &qerr,
                              window),
            "open small-window stream");

    const std::size_t blk0_len = 64U * 1024U;
    const std::size_t blk1_len = 36U * 1024U;
    const std::size_t total_len = blk0_len + blk1_len;
    std::shared_ptr<const std::vector<std::uint8_t>> blk0(
        new std::vector<std::uint8_t>(blk0_len, 0x33));
    std::shared_ptr<const std::vector<std::uint8_t>> blk1(
        new std::vector<std::uint8_t>(blk1_len, 0x44));
    std::weak_ptr<const std::vector<std::uint8_t>> wp0(blk0);
    std::weak_ptr<const std::vector<std::uint8_t>> wp1(blk1);

    auto live_native_bytes = [&]() -> std::size_t {
        return (wp0.expired() ? 0 : blk0_len) + (wp1.expired() ? 0 : blk1_len);
    };
    auto assert_all_tiers_equal = [&](std::size_t expected_bytes, const char* msg) {
        require(ledger.worker_counts().bytes == expected_bytes &&
                    ledger.binding_counts("svc").bytes == expected_bytes &&
                    ledger.request_counts(77).bytes == expected_bytes,
                msg);
    };

    std::string err;
    require(table.stage_pending(id, total_len, 2, &err), "stage 100 KiB");
    assert_all_tiers_equal(live_native_bytes(), "staged bytes match live backing RAM");

    // Deliver blk0 in four 16 KiB sub-slices. During the first 3 partial
    // deliveries, blk0 remains alive in staged storage, so ALL 3 ledger tiers
    // MUST continue reporting the full 100 KiB (zero under-reporting!).
    for (std::size_t slice = 0; slice < 3; ++slice) {
        require(table.admit_staged_shared_bytes(id, blk0, slice * window, window, 0, 0, &err),
                "admit intermediate 16 KiB sub-slice");
        assert_all_tiers_equal(total_len, "intermediate admit holds full 100 KiB");
        {
            capsid::BindingStreamFrame f;
            require(table.pop_frame(id, &f) && f.size() == window && f.credit_bytes == window &&
                        f.accounted_bytes == 0 && f.data()[0] == 0x33,
                    "pop intermediate sub-slice");
        }
        require(!wp0.expired(), "blk0 still alive mid-delivery");
        assert_all_tiers_equal(live_native_bytes(),
                               "post-pop mid-block ledger matches full live 100 KiB");
        require(table.grant_credit(id, window), "refill 16 KiB credit");
    }

    // Fourth (final) sub-slice of blk0 transfers blk0's 64 KiB backing ownership
    // into the queued frame; caller drops staged blk0 reference.
    require(table.admit_staged_shared_bytes(id, blk0, 3 * window, window, blk0_len, 1, &err),
            "admit final sub-slice of blk0");
    blk0.reset();
    require(!wp0.expired(), "queued frame keeps blk0 alive until popped");
    assert_all_tiers_equal(total_len, "pre-pop final sub-slice still accounts 100 KiB");
    {
        capsid::BindingStreamFrame f;
        require(table.pop_frame(id, &f) && f.size() == window && f.credit_bytes == window &&
                    f.accounted_bytes == blk0_len,
                "pop final sub-slice of blk0");
    }
    // Frame destroyed -> blk0 freed AT THE EXACT SAME STEP ledger dropped by 64 KiB!
    require(wp0.expired() && !wp1.expired(), "blk0 freed on final sub-slice pop, blk1 alive");
    assert_all_tiers_equal(live_native_bytes(), "ledger equals remaining 36 KiB blk1 exactly");
    require(table.grant_credit(id, window), "refill credit for blk1");

    // Deliver blk1 (36 KiB = 16 + 16 + 4 KiB).
    for (std::size_t off = 0; off < blk1_len;) {
        const std::size_t piece = (blk1_len - off < window) ? (blk1_len - off) : window;
        const bool last = (off + piece == blk1_len);
        require(table.admit_staged_shared_bytes(id, blk1, off, piece, last ? blk1_len : 0,
                                                last ? 1 : 0, &err),
                "admit blk1 slice");
        if (last) {
            blk1.reset();
        }
        {
            capsid::BindingStreamFrame f;
            require(table.pop_frame(id, &f) && f.size() == piece && f.credit_bytes == piece &&
                        f.data()[0] == 0x44,
                    "pop blk1 slice");
        }
        assert_all_tiers_equal(live_native_bytes(), "blk1 slice ledger matches live RAM");
        if (!last) {
            require(table.grant_credit(id, piece), "refill credit");
        }
        off += piece;
    }
    require(wp1.expired(), "blk1 freed after final slice");
    assert_all_tiers_equal(0, "all tiers zero after draining all blocks");

    // Direct coverage of enqueue_value, stage_pending + release_staged, and
    // admit_staged_value on a value-mode stream.
    require(table.grant_credit(id, window) || table.find(id)->credit_bytes > 0, "credit available");
    capsid::NeutralValue v1;
    v1.kind = capsid::NeutralValue::Kind::kString;
    v1.string = "hello-value";
    const std::size_t w1 = capsid::estimate_neutral_value_bytes(v1);
    require(table.enqueue_value(id, std::move(v1), w1, &err), "enqueue_value");
    assert_all_tiers_equal(w1, "enqueue_value accounted");
    {
        capsid::BindingStreamFrame f;
        require(table.pop_frame(id, &f) && f.type == capsid::BindingStreamFrame::Type::kValue &&
                    f.value.string == "hello-value" && f.credit_bytes == w1,
                "pop enqueued value");
    }
    assert_all_tiers_equal(0, "enqueue_value released");

    // Stage + release_staged rollback path (e.g. promise allocation failure).
    require(table.stage_pending(id, 512, 2, &err), "stage for release_staged");
    assert_all_tiers_equal(512, "staged 512B");
    table.release_staged(id, 512, 2);
    assert_all_tiers_equal(0, "release_staged returned ledger to 0");

    // Stage + admit_staged_value path.
    capsid::NeutralValue v2;
    v2.kind = capsid::NeutralValue::Kind::kString;
    v2.string = "staged-value";
    const std::size_t w2 = capsid::estimate_neutral_value_bytes(v2);
    require(table.stage_pending(id, w2, 1, &err), "stage value");
    require(table.admit_staged_value(id, std::move(v2), w2, &err), "admit_staged_value");
    assert_all_tiers_equal(w2, "admit_staged_value preserved exact weight");
    {
        capsid::BindingStreamFrame f;
        require(table.pop_frame(id, &f) && f.type == capsid::BindingStreamFrame::Type::kValue &&
                    f.value.string == "staged-value",
                "pop staged value");
    }
    assert_all_tiers_equal(0, "staged value released");

    require(table.close_and_erase(id) && table.empty() && ledger.empty(),
            "value & shared-backing test completely clean");
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
    test_staged_native_accounting_and_credit_slicing();
    test_shared_backing_partial_delivery_and_value_staging();
    test_limits_error_cancel_and_request_teardown();
    std::cout << "test-binding-stream-table: PASS (4 suites)" << std::endl;
    return 0;
}
