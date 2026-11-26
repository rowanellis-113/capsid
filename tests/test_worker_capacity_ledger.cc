// §9.4 weighted capacity ledger unit tests.
//
// Modes (argc == 2):
//   ledger_fresh_budget          — fresh reserve/commit/abort + steady gate
//   ledger_replace_surge_gate    — replacement needs surge; overlap peak;
//                                  commit category switch; absolute ceiling
//   ledger_retire_drain_release  — begin_retire / release_drained
//   ledger_no_surge_refuses      — surge=0 refuses zero-downtime replace
//
// These cover the accounting contract; the integration wiring (reserve
// before spawn, reaper-finished release) is exercised by the managed
// Host suite.

#include "host/worker_capacity_ledger.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <limits>

namespace {

using capsid::host::WorkerCapacityLedger;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test-host-worker-capacity-ledger: " << message << std::endl;
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        fail("expected one ledger test mode");
    }
    const std::string mode = argv[1];
    if (mode == "ledger_fresh_after_elastic") {
        WorkerCapacityLedger ledger(2, 0);
        require(ledger.reserve_fresh("orders", 1), "reserve serving worker");
        ledger.commit_fresh("orders");
        require(ledger.try_acquire_elastic("orders"), "fill last slot with an elastic worker");
        require(!ledger.reserve_fresh("billing", 1),
                "fresh deploy admitted while steady=1, elastic=1, workersTotal=2");
        require(!ledger.holds("billing") && ledger.steady_used() == 1 &&
                    ledger.elastic_of("orders") == 1 && ledger.surge_used() == 0,
                "rejection changed the ledger");
        ledger.release_elastic("orders");
        require(ledger.reserve_fresh("billing", 1), "could not reuse the returned slot");
        ledger.commit_fresh("billing");
        require(ledger.steady_used() == 2 && ledger.elastic_used() == 0, "wrong final occupancy");
        return 0;
    }
    if (mode == "ledger_replace_after_elastic") {
        WorkerCapacityLedger ledger(3, 2);
        require(ledger.reserve_fresh("orders", 1) && ledger.reserve_fresh("billing", 1),
                "reserve both serving apps");
        ledger.commit_fresh("orders");
        ledger.commit_fresh("billing");
        require(ledger.try_acquire_elastic("billing"), "fill last slot in sibling app");
        require(!ledger.reserve_replace("orders", 2),
                "replacement's final steady fleet would displace a sibling's held elastic slot");
        require(ledger.steady_of("orders") == 1 && ledger.elastic_of("billing") == 1 &&
                    ledger.surge_used() == 0,
                "rejected replacement retained a reservation");
        ledger.release_elastic("billing");
        require(ledger.reserve_replace("orders", 2), "replacement should fit after release");
        ledger.commit_replace("orders", 2, 1);
        ledger.release_drained("orders", 1);
        require(ledger.steady_used() == 3 && ledger.elastic_used() == 0 && ledger.surge_used() == 0,
                "replacement did not settle at the steady ceiling");
        return 0;
    }
    if (mode == "ledger_elastic_during_replace") {
        WorkerCapacityLedger ledger(2, 2);
        require(ledger.reserve_fresh("orders", 1) && ledger.reserve_fresh("cold", 0),
                "reserve serving apps");
        ledger.commit_fresh("orders");
        ledger.commit_fresh("cold");
        require(ledger.reserve_replace("orders", 2), "growing replacement should fit");
        require(!ledger.reserve_fresh("peer", 1), "fresh deploy consumed promised steady capacity");
        require(!ledger.try_acquire_elastic("cold"),
                "elastic admission consumed capacity already promised to the new generation");
        require(ledger.elastic_used() == 0 && ledger.surge_used() == 2, "overlap counts changed");
        ledger.abort_reserve("orders", 2, true);
        require(ledger.reserve_fresh("peer", 1), "abort did not return fresh deploy headroom");
        ledger.abort_reserve("peer", 1, false);
        require(ledger.try_acquire_elastic("cold"), "aborted replacement did not return headroom");
        ledger.release_elastic("cold");
        require(ledger.steady_used() == 1 && ledger.elastic_used() == 0 && ledger.surge_used() == 0,
                "abort did not preserve the serving generation");
        return 0;
    }
    if (mode == "ledger_concurrent_shrink_surge") {
        WorkerCapacityLedger ledger(4, 3);
        require(ledger.reserve_fresh("a", 2) && ledger.reserve_fresh("b", 2), "reserve two floors");
        ledger.commit_fresh("a");
        ledger.commit_fresh("b");
        require(ledger.reserve_replace("a", 1), "first shrink should fit within surge=3");
        require(!ledger.reserve_replace("b", 1),
                "second concurrent shrink must not overcommit post-swap surge headroom");
        ledger.commit_replace("a", 1, 2);
        require(ledger.steady_used() == 3 && ledger.surge_used() == 2,
                "committed shrink left unexpected counts");
        require(!ledger.reserve_replace("b", 1),
                "second shrink must still wait while first old generation drains 2 surge slots");
        ledger.release_drained("a", 2);
        require(ledger.reserve_replace("b", 1), "second shrink should fit once surge drains");
        ledger.abort_reserve("b", 1, true);
        require(ledger.reserve_replace("b", 1), "aborted shrink did not release surge promise");
        ledger.commit_replace("b", 1, 2);
        require(ledger.steady_used() == 2 && ledger.surge_used() == 2, "unexpected final counts");
        ledger.release_drained("b", 2);
        require(ledger.surge_used() == 0, "drained surge not cleared");
        return 0;
    }
    if (mode == "ledger_mixed_resize_abort_and_drain") {
        WorkerCapacityLedger ledger(5, 5);
        require(ledger.reserve_fresh("shrinking", 2) && ledger.reserve_fresh("growing", 1),
                "reserve both serving floors");
        ledger.commit_fresh("shrinking");
        ledger.commit_fresh("growing");
        require(ledger.reserve_replace("shrinking", 1) && ledger.reserve_replace("growing", 3),
                "both overlapping warmups should fit");
        ledger.abort_reserve("shrinking", 1, true);
        require(ledger.steady_of("shrinking") == 2 && ledger.surge_used() == 3 &&
                    !ledger.try_acquire_elastic("peer"),
                "aborting shrink must keep old floor and concurrent growth reservation");
        require(ledger.reserve_replace("shrinking", 1), "could not reuse aborted reservation");
        ledger.commit_replace("growing", 3, 1);
        ledger.commit_replace("shrinking", 1, 2);
        require(ledger.steady_of("growing") == 3 && ledger.steady_of("shrinking") == 1 &&
                    ledger.steady_used() == 4 && ledger.surge_used() == 3,
                "swaps must charge both old generations until reaped");
        ledger.release_drained("growing", 1);
        require(ledger.surge_used() == 2, "reaping growing app released another app's charge");
        ledger.release_drained("shrinking", 2);
        require(ledger.surge_used() == 0 && ledger.try_acquire_elastic("peer") &&
                    !ledger.try_acquire_elastic("peer"),
                "expected exactly one reusable steady slot after both drains");
        ledger.release_elastic("peer");
        return 0;
    }
    if (mode == "ledger_retired_headroom") {
        WorkerCapacityLedger ledger(4, 0);
        require(ledger.reserve_fresh("orders", 4), "reserve old actual capacity");
        ledger.commit_fresh("orders");
        ledger.begin_retire("orders", 4);
        require(!ledger.reserve_fresh("invoices", 1),
                "retired workers still occupy the absolute ceiling until reap");
        ledger.release_drained("orders", 4);
        require(ledger.reserve_fresh("invoices", 1), "actual reaped capacity becomes reusable");
        std::cout << "PASS" << std::endl;
        return 0;
    }
    if (mode == "ledger_overflow_limits") {
        WorkerCapacityLedger ledger(4, 2);
        require(ledger.reserve_fresh("orders", 1), "reserve one existing worker");
        require(!ledger.reserve_fresh("invoices", std::numeric_limits<std::uint64_t>::max()),
                "large pool size must not wrap the steady-capacity check");
        require(!ledger.reserve_replace("orders", std::numeric_limits<std::uint64_t>::max()),
                "large replacement must not wrap the steady/surge checks");
        require(ledger.steady_used() == 1 && ledger.surge_used() == 0,
                "rejected inputs cannot change held counts");
        std::cout << "PASS" << std::endl;
        return 0;
    }

    if (mode == "ledger_fresh_budget") {
        // workers_total=4, surge=0 (the v1 default): fresh deploys fit
        // the steady budget, the total is never exceeded.
        WorkerCapacityLedger ledger(4, 0);
        require(ledger.reserve_fresh("orders", 3),
                "3-worker fresh deploy did not fit workersTotal 4");
        require(ledger.steady_used() == 3 && ledger.steady_of("orders") == 3 &&
                    ledger.holds("orders"),
                "fresh reserve did not hold the steady count");
        require(!ledger.reserve_fresh("payments", 2),
                "2-worker fresh deploy exceeded the steady budget");
        require(!ledger.reserve_fresh("orders", 2),
                "fresh reserve over the held count was not refused");
        ledger.commit_fresh("orders");
        require(ledger.steady_used() == 3, "fresh commit changed the held steady count");

        // A failed fresh deploy rolls its reserve back.
        require(ledger.reserve_fresh("billing", 1),
                "1-worker fresh deploy did not fit the remaining budget");
        ledger.abort_reserve("billing", 1, /*replacement=*/false);
        require(ledger.steady_used() == 3 && !ledger.holds("billing"),
                "abort_reserve did not roll the fresh count back");

        // A second App can still fit the remaining steady budget.
        require(ledger.reserve_fresh("payments", 1),
                "1-worker fresh deploy did not fit after the abort");
        ledger.commit_fresh("payments");
        require(ledger.steady_used() == 4, "steady budget was not fully consumed");
        std::cout << "PASS" << std::endl;
        return 0;
    }

    if (mode == "ledger_replace_surge_gate") {
        // workers_total=4, surge=3: a 3→3 replace needs the surge budget
        // for the overlap; the steady state after the swap still fits.
        WorkerCapacityLedger ledger(4, 3);
        require(ledger.reserve_fresh("orders", 3), "serving pool did not fit the steady budget");
        ledger.commit_fresh("orders");
        require(ledger.reserve_replace("orders", 3), "3→3 replace did not fit surge 3");
        require(ledger.surge_used() == 3 && ledger.surge_of("orders") == 3 &&
                    ledger.steady_of("orders") == 3,
                "replacement reserve did not hold the new pool under "
                "surge");
        require(ledger.steady_used() + ledger.surge_used() == 6,
                "overlap exceeded the absolute ceiling "
                "(workersTotal + surge)");
        ledger.commit_replace("orders", /*new=*/3, /*old=*/3);
        require(ledger.steady_of("orders") == 3 && ledger.steady_used() == 3 &&
                    ledger.surge_of("orders") == 3 && ledger.surge_used() == 3,
                "commit_replace did not switch the categories "
                "(new→steady, old→surge)");

        // The old pool's reaper finished: only now its count is released.
        ledger.release_drained("orders", 3);
        require(ledger.surge_used() == 0 && ledger.steady_used() == 3,
                "release_drained did not release the drained pool");

        // The DRAIN side of the overlap can be the peak: a 3→1 replace
        // keeps the new pool small but must still fit the old pool's
        // surge count (max(1, 3) = 3) while the reaper runs.
        require(ledger.reserve_replace("orders", 1),
                "3→1 replace did not fit the overlap peak max(1,3)=3");
        require(ledger.surge_used() == 1 && ledger.steady_used() == 3,
                "3→1 reserve held the new pool under surge");
        ledger.commit_replace("orders", /*new=*/1, /*old=*/3);
        require(ledger.steady_of("orders") == 1 && ledger.steady_used() == 1 &&
                    ledger.surge_of("orders") == 3 && ledger.surge_used() == 3,
                "3→1 commit did not move the old pool into surge");

        // The old pool is still draining (surge 3): a second replacement
        // cannot fit the peak on top of the held surge — the reaper
        // must finish first.
        require(!ledger.reserve_replace("orders", 1),
                "replace over an undrained pool was not refused");
        require(ledger.steady_used() == 1 && ledger.surge_used() == 3,
                "refused replace over an undrained pool held a count");

        // The reaper finished: the drain count is released and the
        // steady budget is free again (the App now serves 1 worker).
        ledger.release_drained("orders", 3);
        require(ledger.surge_used() == 0 && ledger.steady_used() == 1,
                "release_drained did not free the surge budget");
        require(ledger.reserve_fresh("payments", 3), "freed budget did not serve a fresh deploy");
        ledger.commit_fresh("payments");
        require(ledger.steady_used() == 4, "steady budget was not fully consumed after the drain");
        std::cout << "PASS" << std::endl;
        return 0;
    }

    if (mode == "ledger_retire_drain_release") {
        WorkerCapacityLedger ledger(6, 2);
        require(ledger.reserve_fresh("orders", 4), "serving pool did not fit the steady budget");
        ledger.commit_fresh("orders");

        // Retire: the serving pool leaves steady for surge (draining).
        // No budget gate: the process count only shrinks.
        ledger.begin_retire("orders", 4);
        require(ledger.steady_of("orders") == 0 && ledger.steady_used() == 0 &&
                    ledger.surge_of("orders") == 4 && ledger.surge_used() == 4 &&
                    !ledger.holds("orders"),
                "begin_retire did not move the pool into surge");

        // The reaper finishes: only now the count is released.
        ledger.release_drained("orders", 4);
        require(ledger.surge_of("orders") == 0 && ledger.surge_used() == 0,
                "release_drained did not return the surge count");

        // The released budget serves a fresh deploy again.
        require(ledger.reserve_fresh("payments", 5), "released budget did not become available");
        ledger.commit_fresh("payments");
        require(ledger.steady_used() == 5, "fresh deploy after retire did not settle");
        std::cout << "PASS" << std::endl;
        return 0;
    }

    if (mode == "ledger_no_surge_refuses") {
        // v1 default surge = 0: a replace of a serving pool is refused —
        // the zero-downtime overlap cannot exist — and nothing is held.
        WorkerCapacityLedger ledger(4, 0);
        require(ledger.reserve_fresh("orders", 2), "serving pool did not fit the steady budget");
        ledger.commit_fresh("orders");
        require(!ledger.reserve_replace("orders", 2),
                "replace without any surge budget was not refused");
        require(ledger.steady_used() == 2 && ledger.surge_used() == 0,
                "refused replace held a count");
        require(ledger.steady_of("orders") == 2, "refused replace disturbed the serving pool");

        // A replace whose steady swap fits but whose peak does not is
        // also refused (surge 1: peak 2 > 1).
        WorkerCapacityLedger tight(4, 1);
        require(tight.reserve_fresh("orders", 2), "serving pool did not fit the tight budget");
        tight.commit_fresh("orders");
        require(!tight.reserve_replace("orders", 2),
                "replace whose overlap peak exceeded surge was not "
                "refused");

        // And the refused replace left the serving pool untouched.
        require(tight.steady_of("orders") == 2 && !tight.holds("other"),
                "refused replace mutated the ledger");
        std::cout << "PASS" << std::endl;
        return 0;
    }

    if (mode == "ledger_elastic_two_tier") {
        // Committed floor (minReady=0) vs global elastic pool (workersTotal=3).
        WorkerCapacityLedger ledger(3, 2);
        require(ledger.reserve_fresh("cold-a", 0), "minReady=0 fresh reserve should succeed");
        ledger.commit_fresh("cold-a");
        require(ledger.holds("cold-a") && ledger.steady_used() == 0,
                "minReady=0 app should hold active presence with 0 steady");
        require(ledger.reserve_fresh("warm-b", 1), "minReady=1 app should reserve 1 steady");
        ledger.commit_fresh("warm-b");
        require(ledger.try_acquire_elastic("cold-a"), "first elastic slot should fit");
        require(ledger.try_acquire_elastic("cold-a"), "second elastic slot should fit");
        require(!ledger.try_acquire_elastic("cold-a"),
                "steady(1) + elastic(2) == workersTotal(3) must reject 3rd elastic slot");
        require(ledger.elastic_used() == 2 && ledger.elastic_of("cold-a") == 2,
                "elastic counts mismatch");
        ledger.release_elastic("cold-a", 1);
        require(ledger.elastic_used() == 1 && ledger.elastic_of("cold-a") == 1,
                "release_elastic did not return 1 slot");
        require(ledger.reserve_replace("cold-a", 1), "0 -> 1 replacement should succeed");
        ledger.commit_replace("cold-a", 1, 0);
        require(ledger.holds("cold-a") && ledger.steady_of("cold-a") == 1,
                "0 -> 1 commit_replace did not update steady");
        ledger.begin_retire("cold-a", 1);
        require(!ledger.holds("cold-a"), "begin_retire should clear active hold");
        ledger.release_drained("cold-a", 1);
        ledger.release_elastic("cold-a", 1);
        require(ledger.elastic_used() == 0 && ledger.steady_used() == 1 && ledger.surge_used() == 0,
                "ledger counts did not settle cleanly");
        std::cout << "PASS" << std::endl;
        return 0;
    }

    if (mode == "ledger_reaping_budget") {
        // B1: bounded asynchronous reaping handoff. Explicit reaping ceiling
        // prevents unbounded concurrent exiting processes after elastic slots
        // are returned for reuse.
        WorkerCapacityLedger ledger(3, 1, 2);
        require(ledger.reaping_limit() == 2 && ledger.reaping_used() == 0 &&
                    ledger.reaping_available(),
                "unexpected explicit reaping budget initialization");
        require(ledger.reserve_fresh("steady-a", 1), "reserve steady floor");
        ledger.commit_fresh("steady-a");
        require(ledger.try_acquire_elastic("el-b") && ledger.try_acquire_elastic("el-b"),
                "acquire two elastic slots");
        require(ledger.elastic_used() == 2 && !ledger.try_acquire_elastic("el-c"),
                "elastic slots should fill remaining workersTotal");

        require(ledger.handoff_elastic_to_reaping("el-b"), "first reaping handoff should fit");
        require(ledger.elastic_used() == 1 && ledger.reaping_used() == 1 &&
                    ledger.reaping_of("el-b") == 1 && ledger.reaping_available(),
                "first handoff accounting mismatch");
        require(ledger.handoff_elastic_to_reaping("el-b"), "second reaping handoff should fit");
        require(ledger.elastic_used() == 0 && ledger.reaping_used() == 2 &&
                    ledger.reaping_of("el-b") == 2 && !ledger.reaping_available(),
                "second handoff should saturate reaping limit");

        // Reaping is saturated: elastic handoff must refuse and keep the slot
        // charged under elastic until reaping headroom frees up.
        require(ledger.try_acquire_elastic("el-c"), "reused elastic slot should fit");
        require(!ledger.handoff_elastic_to_reaping("el-c"),
                "handoff must fail closed when reaping budget is full");
        require(ledger.elastic_used() == 1 && ledger.elastic_of("el-c") == 1 &&
                    ledger.reaping_used() == 2 && ledger.reaping_of("el-c") == 0,
                "failed handoff mutated elastic or reaping counts");

        ledger.finish_reaping("el-b", 1);
        require(ledger.reaping_used() == 1 && ledger.reaping_of("el-b") == 1 &&
                    ledger.reaping_available(),
                "finish_reaping did not restore reaping headroom");
        require(ledger.handoff_elastic_to_reaping("el-c"),
                "handoff did not succeed after reaping headroom returned");
        require(ledger.elastic_used() == 0 && ledger.reaping_used() == 2 &&
                    ledger.reaping_of("el-c") == 1,
                "post-reap handoff accounting mismatch");

        ledger.finish_reaping("el-b", 1);
        ledger.finish_reaping("el-c", 1);
        require(ledger.reaping_used() == 0 && ledger.reaping_of("el-b") == 0 &&
                    ledger.reaping_of("el-c") == 0,
                "finish_reaping left residual reaping count");

        // Default reaping limit derives from surge when positive, else max(1, workersTotal / 4).
        WorkerCapacityLedger default_surge(8, 3);
        WorkerCapacityLedger default_no_surge(8, 0);
        WorkerCapacityLedger tiny(1, 0);
        require(default_surge.reaping_limit() == 3 && default_no_surge.reaping_limit() == 2 &&
                    tiny.reaping_limit() == 1,
                "default reaping limit derivation mismatch");
        std::cout << "PASS" << std::endl;
        return 0;
    }

    fail("unknown ledger test mode: " + mode);
}
