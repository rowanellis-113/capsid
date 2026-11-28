// Binding RPC v2 (v0.4.0) M4D: RpcTarget handle table implementation.

#include "binding_handle_table.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace capsid {

BindingHandleTable::BindingHandleTable(BindingRpcLedger* ledger)
    : ledger_(ledger), next_handle_id_(1) {}

std::uint64_t BindingHandleTable::allocate_handle_id() {
    for (std::size_t probe = 0; probe <= handles_.size(); ++probe) {
        const std::uint64_t id = next_handle_id_;
        next_handle_id_ = (id == std::numeric_limits<std::uint64_t>::max()) ? 1 : (id + 1);
        if (id != 0 && handles_.count(id) == 0) {
            return id;
        }
    }
    return 0;
}

bool BindingHandleTable::open_handle(std::uint64_t owner_request_id,
                                     const std::string& owner_binding_id, std::size_t table_index,
                                     std::uint64_t lease_deadline_ns,
                                     std::vector<std::string> method_names,
                                     std::uint64_t* out_handle_id,
                                     BindingQuotaError* out_quota_error) {
    if (out_handle_id != NULL) {
        *out_handle_id = 0;
    }
    if (out_quota_error != NULL) {
        *out_quota_error = BindingQuotaError::kOk;
    }
    if (ledger_ == NULL || owner_request_id == 0 || owner_binding_id.empty() ||
        method_names.empty() || method_names.size() > kBindingHandleMaxMethods) {
        return false;
    }
    if (!ledger_->reserve_handle(owner_request_id, owner_binding_id, out_quota_error)) {
        return false;
    }
    const std::uint64_t id = allocate_handle_id();
    if (id == 0) {
        ledger_->release_handle(owner_request_id, owner_binding_id);
        return false;
    }
    BindingHandleRecord rec;
    rec.handle_id = id;
    rec.owner_request_id = owner_request_id;
    rec.owner_binding_id = owner_binding_id;
    rec.table_index = table_index;
    rec.lease_deadline_ns = lease_deadline_ns;
    rec.method_names = std::move(method_names);
    rec.state = BindingHandleState::kActive;
    handles_.emplace(id, std::move(rec));
    if (out_handle_id != NULL) {
        *out_handle_id = id;
    }
    return true;
}

BindingHandleRecord* BindingHandleTable::find(std::uint64_t handle_id) {
    std::map<std::uint64_t, BindingHandleRecord>::iterator it = handles_.find(handle_id);
    return it == handles_.end() ? NULL : &it->second;
}

const BindingHandleRecord* BindingHandleTable::find(std::uint64_t handle_id) const {
    std::map<std::uint64_t, BindingHandleRecord>::const_iterator it = handles_.find(handle_id);
    return it == handles_.end() ? NULL : &it->second;
}

bool BindingHandleTable::has_method(std::uint64_t handle_id,
                                    const std::string& method_name) const {
    const BindingHandleRecord* rec = find(handle_id);
    if (rec == NULL) {
        return false;
    }
    return std::find(rec->method_names.begin(), rec->method_names.end(), method_name) !=
           rec->method_names.end();
}

bool BindingHandleTable::is_active_at(std::uint64_t handle_id, std::uint64_t now_ns) const {
    const BindingHandleRecord* rec = find(handle_id);
    if (rec == NULL || rec->state != BindingHandleState::kActive) {
        return false;
    }
    if (rec->lease_deadline_ns != 0 && now_ns >= rec->lease_deadline_ns) {
        return false;
    }
    return true;
}

bool BindingHandleTable::mark_disposed(std::uint64_t handle_id) {
    BindingHandleRecord* rec = find(handle_id);
    if (rec == NULL || rec->state != BindingHandleState::kActive) {
        return false;
    }
    rec->state = BindingHandleState::kDisposed;
    return true;
}

bool BindingHandleTable::close_and_erase(std::uint64_t handle_id) {
    std::map<std::uint64_t, BindingHandleRecord>::iterator it = handles_.find(handle_id);
    if (it == handles_.end()) {
        return false;
    }
    const std::uint64_t req_id = it->second.owner_request_id;
    const std::string binding_id = it->second.owner_binding_id;
    handles_.erase(it);
    if (ledger_ != NULL) {
        ledger_->release_handle(req_id, binding_id);
    }
    return true;
}

std::vector<std::uint64_t> BindingHandleTable::collect_expired(std::uint64_t now_ns) const {
    std::vector<std::uint64_t> out;
    for (std::map<std::uint64_t, BindingHandleRecord>::const_iterator it = handles_.begin();
         it != handles_.end(); ++it) {
        if (it->second.lease_deadline_ns != 0 && now_ns >= it->second.lease_deadline_ns) {
            out.push_back(it->first);
        }
    }
    return out;
}

std::vector<std::uint64_t> BindingHandleTable::collect_for_request(std::uint64_t request_id) const {
    std::vector<std::uint64_t> out;
    for (std::map<std::uint64_t, BindingHandleRecord>::const_iterator it = handles_.begin();
         it != handles_.end(); ++it) {
        if (it->second.owner_request_id == request_id) {
            out.push_back(it->first);
        }
    }
    return out;
}

std::vector<std::uint64_t> BindingHandleTable::collect_all() const {
    std::vector<std::uint64_t> out;
    out.reserve(handles_.size());
    for (std::map<std::uint64_t, BindingHandleRecord>::const_iterator it = handles_.begin();
         it != handles_.end(); ++it) {
        out.push_back(it->first);
    }
    return out;
}

} // namespace capsid
