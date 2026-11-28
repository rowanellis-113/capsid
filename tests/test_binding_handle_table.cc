// Unit tests for Binding RPC v2 M4D BindingHandleTable (leases, idempotent
// disposal, 3-tier handle quotas, and request cleanup).

#include "binding_handle_table.h"
#include "binding_rpc_ledger.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

void test_basic_lifecycle_and_leases() {
    capsid::BindingRpcLedger ledger;
    capsid::BindingHandleTable table(&ledger);

    std::uint64_t id1 = 0;
    capsid::BindingQuotaError qerr = capsid::BindingQuotaError::kOk;
    require(table.open_handle(10, "db", 0, 1000, {"next", "count"}, &id1, &qerr), "open handle 1");
    require(id1 != 0 && table.size() == 1, "handle 1 recorded");
    require(table.has_method(id1, "next") && table.has_method(id1, "count") &&
                !table.has_method(id1, "secret"),
            "method membership check");
    require(table.is_active_at(id1, 999), "active before deadline");
    require(!table.is_active_at(id1, 1000), "inactive at deadline");

    std::uint64_t id2 = 0;
    require(table.open_handle(10, "db", 0, 2000, {"read"}, &id2, &qerr), "open handle 2");
    require(id2 != id1 && table.size() == 2, "distinct handle id");

    const std::vector<std::uint64_t> exp = table.collect_expired(1500);
    require(exp.size() == 1 && exp[0] == id1, "only id1 expired at 1500");

    require(table.mark_disposed(id1), "first mark_disposed succeeds");
    require(!table.mark_disposed(id1), "second mark_disposed is idempotent no-op");
    require(!table.is_active_at(id1, 500), "disposed handle inactive even before deadline");

    require(table.close_and_erase(id1), "erase id1");
    require(!table.close_and_erase(id1), "second erase id1 returns false");
    require(table.size() == 1, "size decremented");

    const std::vector<std::uint64_t> req10 = table.collect_for_request(10);
    require(req10.size() == 1 && req10[0] == id2, "collect_for_request finds id2");
    require(table.close_and_erase(id2), "erase id2");
    require(table.empty() && ledger.worker_counts().handles == 0, "clean zero leak");
}

void test_three_tier_handle_quotas() {
    capsid::BindingRpcLimits limits;
    limits.max_handles_per_request = 3;
    limits.max_handles_per_binding = 5;
    limits.max_handles_per_worker = 7;

    capsid::BindingRpcLedger ledger(limits);
    capsid::BindingHandleTable table(&ledger);

    // Exhaust per-request limit (3).
    std::vector<std::uint64_t> ids;
    capsid::BindingQuotaError qerr = capsid::BindingQuotaError::kOk;
    for (int i = 0; i < 3; ++i) {
        std::uint64_t id = 0;
        require(table.open_handle(1, "b1", 0, 5000, {"m"}, &id, &qerr), "req1 open");
        ids.push_back(id);
    }
    std::uint64_t overflow_id = 0;
    require(!table.open_handle(1, "b1", 0, 5000, {"m"}, &overflow_id, &qerr),
            "per-request handle limit enforced");
    require(qerr == capsid::BindingQuotaError::kRequestHandleLimit, "per-request error kind");

    // Exhaust per-Binding limit (5) using req 2 (2 more).
    for (int i = 0; i < 2; ++i) {
        std::uint64_t id = 0;
        require(table.open_handle(2, "b1", 0, 5000, {"m"}, &id, &qerr), "req2 b1 open");
        ids.push_back(id);
    }
    require(!table.open_handle(2, "b1", 0, 5000, {"m"}, &overflow_id, &qerr),
            "per-binding handle limit enforced");
    require(qerr == capsid::BindingQuotaError::kBindingHandleLimit, "per-binding error kind");

    // Exhaust per-worker limit (7) using b2 (2 more).
    for (int i = 0; i < 2; ++i) {
        std::uint64_t id = 0;
        require(table.open_handle(3, "b2", 1, 5000, {"m"}, &id, &qerr), "req3 b2 open");
        ids.push_back(id);
    }
    require(!table.open_handle(3, "b2", 1, 5000, {"m"}, &overflow_id, &qerr),
            "per-worker handle limit enforced");
    require(qerr == capsid::BindingQuotaError::kWorkerHandleLimit, "per-worker error kind");

    // Empty / >128 method list rejected without consuming quota.
    require(table.close_and_erase(ids.back()), "free 1 slot");
    ids.pop_back();
    std::vector<std::string> too_many(129, "m");
    require(!table.open_handle(3, "b2", 1, 5000, {}, &overflow_id, &qerr),
            "empty methods rejected");
    require(!table.open_handle(3, "b2", 1, 5000, too_many, &overflow_id, &qerr),
            ">128 methods rejected");
    require(ledger.worker_counts().handles == 6, "rejected calls did not leak quota");

    for (std::size_t i = 0; i < ids.size(); ++i) {
        require(table.close_and_erase(ids[i]), "cleanup erase");
    }
    require(table.empty() && ledger.worker_counts().handles == 0, "all quotas refilled");
}

} // namespace

int main() {
    test_basic_lifecycle_and_leases();
    test_three_tier_handle_quotas();
    return 0;
}
