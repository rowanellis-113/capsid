// Binding RPC v2 (v0.4.0) M4D: Pure C++11 RpcTarget handle table with bounded
// leases, idempotent disposal, and 3-tier quota enforcement via BindingRpcLedger.

#ifndef CAPSID_BINDING_HANDLE_TABLE_H
#define CAPSID_BINDING_HANDLE_TABLE_H

#include "binding_rpc_ledger.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace capsid {

constexpr std::size_t kBindingHandleMaxMethods = 128;

enum class BindingHandleState {
    kActive = 0,
    kDisposed,
};

struct BindingHandleRecord {
    std::uint64_t handle_id = 0;
    std::uint64_t owner_request_id = 0;
    std::string owner_binding_id;
    std::size_t table_index = 0;
    std::uint64_t lease_deadline_ns = 0; // clamped <= request_deadline_ns
    std::vector<std::string> method_names;
    BindingHandleState state = BindingHandleState::kActive;
};

class BindingHandleTable {
  public:
    explicit BindingHandleTable(BindingRpcLedger* ledger);

    // Reserves a handle slot in the 3-tier ledger and inserts an active record.
    // Fails closed if quota is exceeded or method_names is empty / > 128.
    bool open_handle(std::uint64_t owner_request_id, const std::string& owner_binding_id,
                     std::size_t table_index, std::uint64_t lease_deadline_ns,
                     std::vector<std::string> method_names, std::uint64_t* out_handle_id,
                     BindingQuotaError* out_quota_error);

    BindingHandleRecord* find(std::uint64_t handle_id);
    const BindingHandleRecord* find(std::uint64_t handle_id) const;

    bool has_method(std::uint64_t handle_id, const std::string& method_name) const;

    // True iff the handle exists, is kActive, and its lease has not elapsed.
    bool is_active_at(std::uint64_t handle_id, std::uint64_t now_ns) const;

    // Transitions kActive -> kDisposed. Returns true only on the first call.
    bool mark_disposed(std::uint64_t handle_id);

    // Erases the handle record and releases its ledger handle slot.
    bool close_and_erase(std::uint64_t handle_id);

    std::vector<std::uint64_t> collect_expired(std::uint64_t now_ns) const;
    std::vector<std::uint64_t> collect_for_request(std::uint64_t request_id) const;
    std::vector<std::uint64_t> collect_all() const;

    std::size_t size() const { return handles_.size(); }
    bool empty() const { return handles_.empty(); }

  private:
    std::uint64_t allocate_handle_id();

    BindingRpcLedger* ledger_;
    std::uint64_t next_handle_id_;
    std::map<std::uint64_t, BindingHandleRecord> handles_;
};

} // namespace capsid

#endif
