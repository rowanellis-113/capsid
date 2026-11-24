// WorkerCapacityLedger implementation — see worker_capacity_ledger.h.
// The ledger is the §9.4 single accounting authority: every count change
// happens under one mutex, so concurrent reserves observe each other and
// the absolute ceiling (workers_total + activation_surge_workers) is an
// invariant of the object, not a best-effort check.

#include "host/worker_capacity_ledger.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace capsid::host {

std::uint64_t WorkerCapacityLedger::available_steady_or_elastic_locked() const {
    if (steady_used_ > workers_total_) {
        throw std::logic_error("invalid steady worker count");
    }
    const std::uint64_t surge_total = surge_used_ + pending_surge_growth_;
    const std::uint64_t excess_surge =
        surge_total > surge_workers_ ? (surge_total - surge_workers_) : 0;
    std::uint64_t occupied = steady_used_;
    if (pending_steady_growth_ > workers_total_ - occupied) {
        return 0;
    }
    occupied += pending_steady_growth_;
    if (elastic_used_ > workers_total_ - occupied) {
        return 0;
    }
    occupied += elastic_used_;
    if (excess_surge > workers_total_ - occupied) {
        return 0;
    }
    occupied += excess_surge;
    return workers_total_ - occupied;
}

bool WorkerCapacityLedger::reserve_fresh(const std::string& application, std::uint64_t pool_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::uint64_t available = available_steady_or_elastic_locked();
    if (pool_size > available) {
        return false;
    }
    Entry& entry = entries_[application];
    entry.active = true;
    entry.steady += pool_size;
    steady_used_ += pool_size;
    return true;
}

bool WorkerCapacityLedger::reserve_replace(const std::string& application,
                                           std::uint64_t new_pool_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(application);
    if (found == entries_.end() || (!found->second.active && found->second.steady == 0) ||
        found->second.replace_pending) {
        // Not a valid replacement: either the App has no serving pool, or
        // another replacement reserve is already pending.
        return false;
    }
    const std::uint64_t old_pool_size = found->second.steady;
    if (steady_used_ < old_pool_size) {
        throw std::logic_error("invalid steady worker count");
    }
    // The overlap peak: the new pool warms (surge), then the old pool
    // drains (surge) — the surge budget must fit the larger side, including
    // any post-swap surge delta already promised by concurrent shrink reserves.
    const std::uint64_t overlap_peak = std::max(new_pool_size, old_pool_size);
    const std::uint64_t surge_committed = surge_used_ + pending_surge_growth_;
    if (surge_committed > surge_workers_ || overlap_peak > surge_workers_ - surge_committed) {
        return false;
    }
    const std::uint64_t surge_growth =
        (old_pool_size > new_pool_size) ? (old_pool_size - new_pool_size) : 0;
    // Steady state after the swap must also honor active elastic slots and
    // other pending replacement growth.
    std::uint64_t growth = 0;
    if (new_pool_size >= old_pool_size) {
        growth = new_pool_size - old_pool_size;
        if (growth > available_steady_or_elastic_locked()) {
            return false;
        }
    } else {
        const std::uint64_t base_steady = steady_used_ - old_pool_size;
        if (base_steady > workers_total_ || pending_steady_growth_ > workers_total_ - base_steady ||
            elastic_used_ > workers_total_ - base_steady - pending_steady_growth_ ||
            new_pool_size > workers_total_ - base_steady - pending_steady_growth_ - elastic_used_) {
            return false;
        }
    }
    // The new pool warms under surge from the reserve moment; concurrent
    // reserves and elastic acquisitions see the promised steady/surge growth.
    found->second.surge += new_pool_size;
    surge_used_ += new_pool_size;
    found->second.replace_pending = true;
    found->second.replace_growth = growth;
    found->second.replace_surge_growth = surge_growth;
    pending_steady_growth_ += growth;
    pending_surge_growth_ += surge_growth;
    return true;
}

void WorkerCapacityLedger::commit_fresh(const std::string& application) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_[application].active = true;
}

void WorkerCapacityLedger::commit_replace(const std::string& application,
                                          std::uint64_t new_pool_size,
                                          std::uint64_t old_pool_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    Entry& entry = entries_[application];
    if (entry.surge < new_pool_size || surge_used_ < new_pool_size ||
        entry.steady != old_pool_size || steady_used_ < old_pool_size) {
        throw std::logic_error("replacement capacity category mismatch");
    }
    if (entry.replace_pending) {
        if (pending_steady_growth_ < entry.replace_growth ||
            pending_surge_growth_ < entry.replace_surge_growth) {
            throw std::logic_error("replacement growth mismatch");
        }
        pending_steady_growth_ -= entry.replace_growth;
        pending_surge_growth_ -= entry.replace_surge_growth;
        entry.replace_pending = false;
        entry.replace_growth = 0;
        entry.replace_surge_growth = 0;
    }
    // The new pool leaves surge for steady; the old pool enters surge
    // (draining). The held counts never change sum — only the category.
    entry.active = true;
    entry.surge -= new_pool_size;
    surge_used_ -= new_pool_size;
    entry.steady = new_pool_size;
    steady_used_ -= old_pool_size;
    steady_used_ += new_pool_size;
    entry.surge += old_pool_size;
    surge_used_ += old_pool_size;
}

void WorkerCapacityLedger::abort_reserve(const std::string& application, std::uint64_t pool_size,
                                         bool replacement) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(application);
    if (found == entries_.end()) {
        return;
    }
    if (replacement) {
        if (found->second.surge < pool_size || surge_used_ < pool_size)
            throw std::logic_error("surge reserve already released");
        if (found->second.replace_pending) {
            if (pending_steady_growth_ < found->second.replace_growth ||
                pending_surge_growth_ < found->second.replace_surge_growth) {
                throw std::logic_error("replacement growth mismatch");
            }
            pending_steady_growth_ -= found->second.replace_growth;
            pending_surge_growth_ -= found->second.replace_surge_growth;
            found->second.replace_pending = false;
            found->second.replace_growth = 0;
            found->second.replace_surge_growth = 0;
        }
        found->second.surge -= pool_size;
        surge_used_ -= pool_size;
    } else {
        if (found->second.steady < pool_size || steady_used_ < pool_size)
            throw std::logic_error("steady reserve already released");
        found->second.steady -= pool_size;
        steady_used_ -= pool_size;
        found->second.active = false;
    }
    if (!found->second.active && found->second.steady == 0 && found->second.surge == 0 &&
        found->second.elastic == 0) {
        entries_.erase(found);
    }
}

void WorkerCapacityLedger::begin_retire(const std::string& application, std::uint64_t pool_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    Entry& entry = entries_[application];
    if (entry.steady < pool_size || steady_used_ < pool_size)
        throw std::logic_error("retire capacity category mismatch");
    entry.active = false;
    entry.steady -= pool_size;
    steady_used_ -= pool_size;
    entry.surge += pool_size;
    surge_used_ += pool_size;
}

void WorkerCapacityLedger::release_drained(const std::string& application, std::uint64_t drained) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(application);
    if (found == entries_.end()) {
        return;
    }
    if (found->second.surge < drained || surge_used_ < drained)
        throw std::logic_error("drained capacity category mismatch");
    found->second.surge -= drained;
    surge_used_ -= drained;
    if (!found->second.active && found->second.steady == 0 && found->second.surge == 0 &&
        found->second.elastic == 0) {
        entries_.erase(found);
    }
}

void WorkerCapacityLedger::release_active(const std::string& application, std::uint64_t reaped) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(application);
    if (found == entries_.end() || found->second.steady < reaped || steady_used_ < reaped)
        throw std::logic_error("reaped active capacity category mismatch");
    found->second.steady -= reaped;
    steady_used_ -= reaped;
    if (found->second.steady == 0) {
        found->second.active = false;
    }
    if (!found->second.active && found->second.steady == 0 && found->second.surge == 0 &&
        found->second.elastic == 0) {
        entries_.erase(found);
    }
}

bool WorkerCapacityLedger::try_acquire_elastic(const std::string& application) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (available_steady_or_elastic_locked() < 1) {
        return false;
    }
    entries_[application].elastic += 1;
    elastic_used_ += 1;
    return true;
}

void WorkerCapacityLedger::release_elastic(const std::string& application, std::uint64_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(application);
    if (found == entries_.end() || found->second.elastic < count || elastic_used_ < count) {
        throw std::logic_error("elastic capacity category mismatch");
    }
    found->second.elastic -= count;
    elastic_used_ -= count;
    if (!found->second.active && found->second.steady == 0 && found->second.surge == 0 &&
        found->second.elastic == 0) {
        entries_.erase(found);
    }
}

WorkerPoolCapacityLease::WorkerPoolCapacityLease(WorkerCapacityLedger* ledger,
                                                 std::string application, std::uint64_t workers)
    : ledger_(ledger), application_(std::move(application)), workers_(workers) {}

void WorkerPoolCapacityLease::release_reaped_locked() {
    if (!reaped_)
        return;
    if (stage_ == Stage::kActive)
        ledger_->release_active(application_, workers_);
    else if (stage_ == Stage::kDraining)
        ledger_->release_drained(application_, workers_);
    else
        return;
    stage_ = Stage::kReleased;
}

void WorkerPoolCapacityLease::activate_fresh() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stage_ != Stage::kPrepared)
        throw std::logic_error("pool capacity already settled");
    ledger_->commit_fresh(application_);
    stage_ = Stage::kActive;
    release_reaped_locked();
}

void WorkerPoolCapacityLease::activate_replacement(
    const std::shared_ptr<WorkerPoolCapacityLease>& old) {
    if (!old || old.get() == this)
        throw std::logic_error("replacement pool capacity missing");
    std::scoped_lock lock(mutex_, old->mutex_);
    if (stage_ != Stage::kPrepared)
        throw std::logic_error("pool capacity already settled");
    if (old->ledger_ != ledger_ || old->application_ != application_ ||
        old->stage_ == Stage::kPrepared)
        throw std::logic_error("replacement capacity belongs to another activation");
    const auto active_old = old->stage_ == Stage::kActive ? old->workers_ : 0;
    ledger_->commit_replace(application_, workers_, active_old);
    if (old->stage_ == Stage::kActive)
        old->stage_ = Stage::kDraining;
    stage_ = Stage::kActive;
    release_reaped_locked();
}

void WorkerPoolCapacityLease::abort(bool replacement) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stage_ == Stage::kReleased)
        return;
    if (stage_ != Stage::kPrepared)
        throw std::logic_error("cannot abort committed capacity");
    ledger_->abort_reserve(application_, workers_, replacement);
    stage_ = Stage::kReleased;
}

void WorkerPoolCapacityLease::begin_drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stage_ == Stage::kActive) {
        ledger_->begin_retire(application_, workers_);
        stage_ = Stage::kDraining;
    }
    release_reaped_locked();
}

void WorkerPoolCapacityLease::reaped() {
    std::lock_guard<std::mutex> lock(mutex_);
    reaped_ = true;
    release_reaped_locked();
}

bool WorkerCapacityLedger::holds(const std::string& application) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(application);
    return found != entries_.end() && (found->second.active || found->second.steady > 0);
}

std::uint64_t WorkerCapacityLedger::steady_used() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return steady_used_;
}

std::uint64_t WorkerCapacityLedger::surge_used() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return surge_used_;
}

std::uint64_t WorkerCapacityLedger::elastic_used() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return elastic_used_;
}

std::uint64_t WorkerCapacityLedger::steady_of(const std::string& application) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(application);
    return found == entries_.end() ? 0 : found->second.steady;
}

std::uint64_t WorkerCapacityLedger::surge_of(const std::string& application) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(application);
    return found == entries_.end() ? 0 : found->second.surge;
}

std::uint64_t WorkerCapacityLedger::elastic_of(const std::string& application) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(application);
    return found == entries_.end() ? 0 : found->second.elastic;
}

} // namespace capsid::host
