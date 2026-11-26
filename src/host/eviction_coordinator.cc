#include "host/eviction_coordinator.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace capsid::host {

namespace {

struct EvictionCandidate {
    std::shared_ptr<GenerationPool> pool;
    std::string tenant;
    std::uint64_t last_active_at_ms = 0;
};

struct FleetMemoryState {
    std::uint64_t total_bytes = 0;
    std::unordered_map<std::string, std::uint64_t> tenant_bytes;
    std::vector<EvictionCandidate> candidates;
};

// Both eviction paths use every resident tenant to decide whether global LRU is allowed.
std::pair<const EvictionCandidate*, const char*>
select_victim(const FleetMemoryState& state, std::uint64_t budget, const TenancyConfig& tenancy) {
    if (state.candidates.empty()) {
        return {nullptr, nullptr};
    }
    const auto fair_share = std::max<std::uint64_t>(1, budget / state.tenant_bytes.size());
    std::unordered_map<std::string, double> overshare;
    for (const auto& [tenant, used] : state.tenant_bytes) {
        const auto share = std::max(fair_share, tenancy.quota_for(tenant).min_soft_memory_bytes);
        if (used > share) {
            overshare.emplace(tenant, static_cast<double>(used) / static_cast<double>(share));
        }
    }
    const EvictionCandidate* best = nullptr;
    double best_ratio = 0;
    for (const auto& candidate : state.candidates) {
        const auto it = overshare.find(candidate.tenant);
        if (!overshare.empty() && it == overshare.end()) {
            continue;
        }
        const double ratio = it == overshare.end() ? 0 : it->second;
        if (!best || ratio > best_ratio ||
            (ratio == best_ratio && candidate.last_active_at_ms < best->last_active_at_ms)) {
            best = &candidate;
            best_ratio = ratio;
        }
    }
    return {best, overshare.empty() ? "global_lru" : "tenant_overshare"};
}

bool worker_past_residency(const GenerationPool::WorkerSlotSnapshot& w,
                           std::uint64_t min_residency_ms, std::uint64_t now_ms) {
    if (min_residency_ms == 0) {
        return true;
    }
    return w.started_at_ms != 0 && now_ms >= w.started_at_ms + min_residency_ms;
}

std::uint64_t steady_now_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

std::uint64_t watermark_bytes(std::uint64_t limit_bytes, std::uint32_t percent) {
    if (percent >= 100) {
        return limit_bytes;
    }
    if (limit_bytes <= std::numeric_limits<std::uint64_t>::max() / percent) {
        return (limit_bytes * percent) / 100ULL;
    }
    return (limit_bytes / 100ULL) * percent;
}

std::uint64_t sat_add_u64(std::uint64_t a, std::uint64_t b) noexcept {
    const std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
    return (a > max - b) ? max : (a + b);
}

std::size_t count_evictable(const std::vector<GenerationPool::WorkerSlotSnapshot>& workers) {
    std::size_t count = 0;
    for (const auto& w : workers) {
        if (w.evictable) {
            ++count;
        }
    }
    return count;
}

} // namespace

GlobalEvictionCoordinator::GlobalEvictionCoordinator(MemoryBudgetConfig memory,
                                                     TenancyConfig tenancy,
                                                     WorkerCapacityLedger* ledger,
                                                     MemorySampler* sampler,
                                                     MetricsRegistry* metrics, StructuredLog* log)
    : memory_(std::move(memory)), tenancy_(std::move(tenancy)), ledger_(ledger), sampler_(sampler),
      metrics_(metrics), log_(log) {
    if (sampler_ != nullptr && memory_.check_interval_ms > 0) {
        sampler_->set_max_age_ms(memory_.check_interval_ms);
    }
}

GlobalEvictionCoordinator::~GlobalEvictionCoordinator() { stop_and_join(); }

void GlobalEvictionCoordinator::register_pool(const std::shared_ptr<GenerationPool>& pool) {
    if (!pool) {
        return;
    }
    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto it = pools_.begin(); it != pools_.end();) {
        const auto existing = it->lock();
        if (!existing || existing->state() == GenerationPool::State::kDead) {
            it = pools_.erase(it);
            continue;
        }
        if (existing.get() == pool.get()) {
            return;
        }
        ++it;
    }
    pools_.push_back(pool);
}

void GlobalEvictionCoordinator::unregister_pool(const GenerationPool* pool) {
    if (pool == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto it = pools_.begin(); it != pools_.end();) {
        const auto existing = it->lock();
        if (!existing || existing.get() == pool) {
            it = pools_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<std::shared_ptr<GenerationPool>>
GlobalEvictionCoordinator::snapshot_tracked_pools() const {
    std::vector<std::shared_ptr<GenerationPool>> tracked;
    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto it = pools_.begin(); it != pools_.end();) {
        auto pool = it->lock();
        if (!pool || pool->state() == GenerationPool::State::kDead) {
            it = pools_.erase(it);
            continue;
        }
        tracked.push_back(std::move(pool));
        ++it;
    }
    return tracked;
}

bool GlobalEvictionCoordinator::has_tenant_memory_limits() const {
    if (tenancy_.default_quota.hard_memory_bytes > 0) {
        return true;
    }
    for (const auto& [tid, q] : tenancy_.overrides) {
        (void)tid;
        if (q.hard_memory_bytes > 0) {
            return true;
        }
    }
    return false;
}

bool GlobalEvictionCoordinator::check_or_reserve_deploy_quota_locked(
    std::string_view application, std::string_view tenant, std::uint32_t new_min_ready,
    std::uint32_t warm_workers, bool record_reservation, std::string* error) {
    const std::string effective_tenant(tenant.empty() ? application : tenant);
    const TenantQuotaConfig quota = tenancy_.quota_for(effective_tenant);
    if (warm_workers == 0 && new_min_ready == 0) {
        if (record_reservation) {
            pending_deploys_[std::string(application)] =
                PendingDeploy{effective_tenant, warm_workers, new_min_ready};
        }
        return true;
    }
    const bool force_sample = !monitor_active_.load(std::memory_order_acquire);

    const auto collect = [&](const std::unordered_set<std::int64_t>& skip_pids,
                             std::vector<std::shared_ptr<GenerationPool>>* tenant_active_pools,
                             std::uint64_t* tenant_bytes, std::uint64_t* other_committed,
                             std::uint64_t* other_live) {
        tenant_active_pools->clear();
        *tenant_bytes = 0;
        *other_committed = 0;
        *other_live = 0;
        for (const auto& pool : snapshot_tracked_pools()) {
            if (pool->tenant_id() != effective_tenant) {
                continue;
            }
            const bool is_active = (pool->state() == GenerationPool::State::kActive);
            if (is_active) {
                tenant_active_pools->push_back(pool);
                if (pool->application_id() != application) {
                    *other_committed += pool->min_ready();
                    *other_live += pool->active_or_spawning_workers();
                }
            }
            if (sampler_ != nullptr) {
                for (const auto& w : pool->snapshot_workers()) {
                    if (w.pid > 0 && skip_pids.count(w.pid) != 0) {
                        continue;
                    }
                    const std::uint64_t b =
                        sampler_->sample_worker_bytes(w.pid, w.cgroup_path, force_sample);
                    *tenant_bytes = sat_add_u64(*tenant_bytes, b);
                }
            }
        }
        for (const auto& [pending_app, pd] : pending_deploys_) {
            if (pending_app != application && pd.tenant == effective_tenant) {
                *other_committed += pd.min_ready;
                *other_live += std::max<std::uint64_t>(pd.warm_workers, pd.min_ready);
            }
        }
    };

    const auto evict_oldest_idle_sibling =
        [&](const std::vector<std::shared_ptr<GenerationPool>>& tenant_active_pools,
            bool exclude_requester_app, std::int64_t* evicted_pid_out) -> bool {
        const std::uint64_t now = steady_now_ms();
        std::shared_ptr<GenerationPool> best_pool;
        std::uint64_t best_active_at = std::numeric_limits<std::uint64_t>::max();
        for (const auto& pool : tenant_active_pools) {
            if (exclude_requester_app && pool->application_id() == application) {
                continue;
            }
            const auto workers = pool->snapshot_workers();
            if (count_evictable(workers) <= pool->min_ready()) {
                continue;
            }
            for (const auto& w : workers) {
                if (w.evictable && w.idle &&
                    worker_past_residency(w, memory_.min_residency_ms, now) &&
                    w.last_active_at_ms < best_active_at) {
                    best_active_at = w.last_active_at_ms;
                    best_pool = pool;
                }
            }
        }
        return best_pool &&
               best_pool->evict_idle_worker(memory_.min_residency_ms, "tenant_quota_swap",
                                            /*require_elastic_held=*/false, evicted_pid_out);
    };

    if (quota.hard_memory_bytes > 0 && warm_workers > 0) {
        std::unordered_set<std::int64_t> evicted_pids;
        std::vector<std::shared_ptr<GenerationPool>> tenant_active_pools;
        std::uint64_t tenant_bytes = 0;
        std::uint64_t other_committed = 0;
        std::uint64_t other_live = 0;
        for (;;) {
            collect(evicted_pids, &tenant_active_pools, &tenant_bytes, &other_committed,
                    &other_live);
            if (tenant_bytes < quota.hard_memory_bytes) {
                break;
            }
            std::int64_t evicted_pid = 0;
            if (!evict_oldest_idle_sibling(tenant_active_pools, /*exclude_requester_app=*/false,
                                           &evicted_pid)) {
                record_pool_rejection(metrics_, log_, std::string(application), effective_tenant,
                                      {}, "tenant_hard_memory");
                if (error != nullptr) {
                    *error = "tenant hardMemory quota exceeded";
                }
                return false;
            }
            if (evicted_pid > 0) {
                evicted_pids.insert(evicted_pid);
            }
        }
    }

    if (quota.max_workers > 0) {
        std::unordered_set<std::int64_t> empty_skip;
        std::vector<std::shared_ptr<GenerationPool>> tenant_active_pools;
        std::uint64_t tenant_bytes = 0;
        std::uint64_t other_committed = 0;
        std::uint64_t other_live = 0;
        collect(empty_skip, &tenant_active_pools, &tenant_bytes, &other_committed, &other_live);
        if (other_committed + new_min_ready > quota.max_workers) {
            record_pool_rejection(metrics_, log_, std::string(application), effective_tenant, {},
                                  "tenant_max_workers");
            if (error != nullptr) {
                *error = "tenant maxWorkers quota exceeded";
            }
            return false;
        }
        const std::uint64_t needed_slots = std::max<std::uint64_t>(warm_workers, new_min_ready);
        while (other_live + needed_slots > quota.max_workers) {
            std::int64_t evicted_pid = 0;
            if (!evict_oldest_idle_sibling(tenant_active_pools, /*exclude_requester_app=*/true,
                                           &evicted_pid)) {
                break;
            }
            --other_live;
        }
        if (other_live + needed_slots > quota.max_workers) {
            record_pool_rejection(metrics_, log_, std::string(application), effective_tenant, {},
                                  "tenant_max_workers");
            if (error != nullptr) {
                *error = "tenant maxWorkers quota exceeded";
            }
            return false;
        }
    }

    if (record_reservation) {
        pending_deploys_[std::string(application)] =
            PendingDeploy{effective_tenant, warm_workers, new_min_ready};
    }
    return true;
}

bool GlobalEvictionCoordinator::check_deploy_quota(std::string_view application,
                                                   std::string_view tenant,
                                                   std::uint32_t new_min_ready,
                                                   std::string* error) {
    std::lock_guard<std::mutex> lock(eviction_mutex_);
    return check_or_reserve_deploy_quota_locked(application, tenant, new_min_ready,
                                                std::max<std::uint32_t>(1U, new_min_ready),
                                                /*record_reservation=*/false, error);
}

bool GlobalEvictionCoordinator::reserve_deploy_quota(std::string_view application,
                                                     std::string_view tenant,
                                                     std::uint32_t new_min_ready,
                                                     std::uint32_t warm_workers,
                                                     std::string* error) {
    std::lock_guard<std::mutex> lock(eviction_mutex_);
    return check_or_reserve_deploy_quota_locked(application, tenant, new_min_ready, warm_workers,
                                                /*record_reservation=*/true, error);
}

void GlobalEvictionCoordinator::release_deploy_quota(std::string_view application) {
    std::lock_guard<std::mutex> lock(eviction_mutex_);
    pending_deploys_.erase(std::string(application));
}

bool GlobalEvictionCoordinator::evict_one_elastic_locked() {
    const auto pools = snapshot_tracked_pools();
    const std::uint64_t now = steady_now_ms();
    FleetMemoryState state;
    for (const auto& pool : pools) {
        const auto workers = pool->snapshot_workers();
        const std::string pool_tenant(pool->tenant_id());
        const bool can_evict_pool = pool->state() == GenerationPool::State::kActive &&
                                    count_evictable(workers) > pool->min_ready();
        for (const auto& w : workers) {
            const std::uint64_t b = sampler_ != nullptr
                                        ? sampler_->sample_worker_bytes(w.pid, w.cgroup_path, false)
                                        : 0;
            state.tenant_bytes[pool_tenant] = sat_add_u64(state.tenant_bytes[pool_tenant], b);
            state.total_bytes = sat_add_u64(state.total_bytes, b);
            if (can_evict_pool && w.evictable && w.idle &&
                worker_past_residency(w, memory_.min_residency_ms, now)) {
                state.candidates.push_back(
                    EvictionCandidate{pool, pool_tenant, w.last_active_at_ms});
            }
        }
    }
    const auto budget =
        memory_.configured && memory_.limit_bytes ? memory_.limit_bytes : state.total_bytes;
    const auto [victim, reason] = select_victim(state, budget, tenancy_);
    return victim && victim->pool->evict_idle_worker(memory_.min_residency_ms, reason);
}

bool GlobalEvictionCoordinator::reclaim_one_idle_elastic() {
    std::lock_guard<std::mutex> lock(eviction_mutex_);
    return evict_one_elastic_locked();
}

bool GlobalEvictionCoordinator::acquire_worker_slot(const GenerationPool* requester,
                                                    std::string* rejection_reason) {
    if (requester == nullptr) {
        if (rejection_reason != nullptr) {
            *rejection_reason = "host_capacity_exhausted";
        }
        return false;
    }
    std::lock_guard<std::mutex> lock(eviction_mutex_);
    const std::string app(requester->application_id());
    const std::string tenant(requester->tenant_id());
    const TenantQuotaConfig quota = tenancy_.quota_for(tenant);
    const bool force_sample = !monitor_active_.load(std::memory_order_acquire);

    const auto collect_tenant_state =
        [&](const std::unordered_set<std::int64_t>& skip_pids,
            std::vector<std::shared_ptr<GenerationPool>>* tenant_pools,
            std::size_t* tenant_active_workers, std::uint64_t* tenant_bytes) {
            tenant_pools->clear();
            *tenant_active_workers = 0;
            *tenant_bytes = 0;
            const auto pools = snapshot_tracked_pools();
            bool saw_requester = false;
            for (const auto& pool : pools) {
                if (pool.get() == requester) {
                    saw_requester = true;
                }
                if (pool->tenant_id() != tenant) {
                    continue;
                }
                const bool is_active = (pool->state() == GenerationPool::State::kActive);
                if (is_active) {
                    tenant_pools->push_back(pool);
                    *tenant_active_workers += pool->active_or_spawning_workers();
                }
                if (sampler_ != nullptr) {
                    for (const auto& w : pool->snapshot_workers()) {
                        if (w.pid > 0 && skip_pids.count(w.pid) != 0) {
                            continue;
                        }
                        *tenant_bytes = sat_add_u64(
                            *tenant_bytes,
                            sampler_->sample_worker_bytes(w.pid, w.cgroup_path, force_sample));
                    }
                }
            }
            if (!saw_requester) {
                *tenant_active_workers += requester->active_or_spawning_workers();
                if (sampler_ != nullptr) {
                    for (const auto& w : requester->snapshot_workers()) {
                        if (w.pid > 0 && skip_pids.count(w.pid) != 0) {
                            continue;
                        }
                        *tenant_bytes = sat_add_u64(
                            *tenant_bytes,
                            sampler_->sample_worker_bytes(w.pid, w.cgroup_path, force_sample));
                    }
                }
            }
            for (const auto& [pending_app, pd] : pending_deploys_) {
                (void)pending_app;
                if (pd.tenant == tenant) {
                    *tenant_active_workers +=
                        std::max<std::uint64_t>(pd.warm_workers, pd.min_ready);
                }
            }
        };

    const auto try_evict_within_tenant =
        [&](const std::vector<std::shared_ptr<GenerationPool>>& tenant_pools,
            std::int64_t* evicted_pid_out) -> bool {
        const std::uint64_t now = steady_now_ms();
        std::shared_ptr<GenerationPool> best_pool;
        std::uint64_t best_active_at = std::numeric_limits<std::uint64_t>::max();
        for (const auto& pool : tenant_pools) {
            if (pool->state() != GenerationPool::State::kActive) {
                continue;
            }
            const auto workers = pool->snapshot_workers();
            if (count_evictable(workers) <= pool->min_ready()) {
                continue;
            }
            for (const auto& w : workers) {
                if (w.evictable && w.idle &&
                    worker_past_residency(w, memory_.min_residency_ms, now) &&
                    w.last_active_at_ms < best_active_at) {
                    best_active_at = w.last_active_at_ms;
                    best_pool = pool;
                }
            }
        }
        return best_pool &&
               best_pool->evict_idle_worker(memory_.min_residency_ms, "tenant_quota_swap",
                                            /*require_elastic_held=*/false, evicted_pid_out);
    };

    // 1. Tenant hardMemory quota check (with synchronous sibling idle swap).
    if (quota.hard_memory_bytes > 0) {
        std::unordered_set<std::int64_t> evicted_pids;
        std::vector<std::shared_ptr<GenerationPool>> tenant_pools;
        std::size_t tenant_workers = 0;
        std::uint64_t tenant_bytes = 0;
        for (;;) {
            collect_tenant_state(evicted_pids, &tenant_pools, &tenant_workers, &tenant_bytes);
            if (tenant_bytes < quota.hard_memory_bytes) {
                break;
            }
            std::int64_t evicted_pid = 0;
            if (!try_evict_within_tenant(tenant_pools, &evicted_pid)) {
                if (rejection_reason != nullptr) {
                    *rejection_reason = "tenant_hard_memory";
                }
                return false;
            }
            if (evicted_pid > 0) {
                evicted_pids.insert(evicted_pid);
            }
        }
    }

    // 2. Tenant maxWorkers quota check (note requester->active_or_spawning_workers() already
    // includes +1 for the pending spawn slot claimed in request_scale_out).
    if (quota.max_workers > 0) {
        std::unordered_set<std::int64_t> empty_skip;
        std::vector<std::shared_ptr<GenerationPool>> tenant_pools;
        std::size_t tenant_workers = 0;
        std::uint64_t tenant_bytes = 0;
        for (;;) {
            collect_tenant_state(empty_skip, &tenant_pools, &tenant_workers, &tenant_bytes);
            if (tenant_workers <= quota.max_workers) {
                break;
            }
            if (!try_evict_within_tenant(tenant_pools, nullptr)) {
                if (rejection_reason != nullptr) {
                    *rejection_reason = "tenant_max_workers";
                }
                return false;
            }
        }
    }

    // 3. Global memory budget check (run watermark eviction if >= high watermark, refuse if
    // still >= limit_bytes).
    if (memory_.configured && memory_.limit_bytes > 0) {
        (void)run_eviction_cycle_locked(force_sample);
        std::uint64_t total_bytes = 0;
        if (sampler_ != nullptr) {
            for (const auto& pool : snapshot_tracked_pools()) {
                for (const auto& w : pool->snapshot_workers()) {
                    total_bytes = sat_add_u64(total_bytes, sampler_->sample_worker_bytes(
                                                               w.pid, w.cgroup_path, force_sample));
                }
            }
        }
        if (total_bytes >= memory_.limit_bytes) {
            if (rejection_reason != nullptr) {
                *rejection_reason = "host_capacity_exhausted";
            }
            return false;
        }
    }

    // 4. Global elastic worker-slot ledger check (evict an idle worker above
    // minReady on contention only while async reaping headroom is available).
    if (ledger_ != nullptr && !ledger_->try_acquire_elastic(app)) {
        while (ledger_->reaping_available()) {
            if (!evict_one_elastic_locked()) {
                break;
            }
            if (ledger_->try_acquire_elastic(app)) {
                return true;
            }
        }
        if (rejection_reason != nullptr) {
            *rejection_reason = "host_capacity_exhausted";
        }
        return false;
    }

    return true;
}

std::size_t GlobalEvictionCoordinator::run_eviction_cycle(bool refresh_samples) {
    std::lock_guard<std::mutex> lock(eviction_mutex_);
    return run_eviction_cycle_locked(refresh_samples);
}

std::size_t GlobalEvictionCoordinator::run_eviction_cycle_locked(bool refresh_samples) {
    if (!memory_.configured || memory_.limit_bytes == 0 || sampler_ == nullptr) {
        return 0;
    }
    const std::uint64_t high_bytes =
        watermark_bytes(memory_.limit_bytes, memory_.high_watermark_percent);
    const std::uint64_t low_bytes =
        watermark_bytes(memory_.limit_bytes, memory_.low_watermark_percent);

    if (refresh_samples) {
        recently_evicted_pids_.clear();
    }
    const auto gather_state = [&](bool force_refresh) -> FleetMemoryState {
        FleetMemoryState state;
        const auto pools = snapshot_tracked_pools();
        const std::uint64_t now = steady_now_ms();
        std::unordered_set<std::int64_t> live_pids;
        for (const auto& pool : pools) {
            const auto workers = pool->snapshot_workers();
            const std::string tenant(pool->tenant_id());
            const bool pool_can_evict = pool->state() == GenerationPool::State::kActive &&
                                        count_evictable(workers) > pool->min_ready();
            for (const auto& w : workers) {
                if (w.pid > 0) {
                    live_pids.insert(w.pid);
                }
                // An async victim still belongs to its tenant until the process exits.
                const std::uint64_t bytes =
                    sampler_->sample_worker_bytes(w.pid, w.cgroup_path, force_refresh);
                state.tenant_bytes[tenant] = sat_add_u64(state.tenant_bytes[tenant], bytes);
                if (w.pid > 0 && recently_evicted_pids_.count(w.pid) != 0) {
                    continue; // Its reclaim is already credited toward the host's low watermark.
                }
                state.total_bytes = sat_add_u64(state.total_bytes, bytes);
                if (pool_can_evict && w.evictable && w.idle &&
                    worker_past_residency(w, memory_.min_residency_ms, now)) {
                    state.candidates.push_back(
                        EvictionCandidate{pool, tenant, w.last_active_at_ms});
                }
            }
        }
        for (auto it = recently_evicted_pids_.begin(); it != recently_evicted_pids_.end();) {
            if (live_pids.count(*it) == 0) {
                it = recently_evicted_pids_.erase(it);
            } else {
                ++it;
            }
        }
        sampler_->prune_pids(live_pids);
        return state;
    };

    FleetMemoryState state = gather_state(refresh_samples);
    if (state.total_bytes < high_bytes) {
        return 0;
    }

    std::size_t evicted_count = 0;
    while (state.total_bytes > low_bytes && !state.candidates.empty()) {
        const auto [victim, reason] = select_victim(state, memory_.limit_bytes, tenancy_);
        if (victim == nullptr) {
            break;
        }
        std::int64_t evicted_pid = 0;
        if (!victim->pool->evict_idle_worker(memory_.min_residency_ms, reason, false,
                                             &evicted_pid)) {
            GenerationPool* failed_pool = victim->pool.get();
            state.candidates.erase(std::remove_if(state.candidates.begin(), state.candidates.end(),
                                                  [failed_pool](const EvictionCandidate& c) {
                                                      return c.pool.get() == failed_pool;
                                                  }),
                                   state.candidates.end());
            continue;
        }
        if (evicted_pid > 0) {
            recently_evicted_pids_.insert(evicted_pid);
        }
        ++evicted_count;
        // Re-evaluate from sampler cache (zero extra procfs syscalls) so per-pool minReady
        // and updated tenant/total working-set bytes remain exact.
        state = gather_state(/*force_refresh=*/false);
    }

    return evicted_count;
}

void GlobalEvictionCoordinator::start() {
    const bool monitor_global = memory_.configured && memory_.limit_bytes > 0;
    const bool monitor_tenant = has_tenant_memory_limits();
    if ((!monitor_global && !monitor_tenant) || memory_.check_interval_ms == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    if (monitor_running_) {
        return;
    }
    monitor_stop_ = false;
    monitor_running_ = true;
    monitor_active_.store(true, std::memory_order_release);
    monitor_thread_ = std::thread([this, monitor_global] {
        std::unique_lock<std::mutex> lock(monitor_mutex_);
        while (!monitor_stop_) {
            monitor_cv_.wait_for(lock, std::chrono::milliseconds(memory_.check_interval_ms),
                                 [this] { return monitor_stop_; });
            if (monitor_stop_) {
                break;
            }
            lock.unlock();
            if (monitor_global) {
                (void)run_eviction_cycle(/*refresh_samples=*/true);
            } else if (sampler_ != nullptr) {
                std::lock_guard<std::mutex> evict_lock(eviction_mutex_);
                std::unordered_set<std::int64_t> live_pids;
                for (const auto& pool : snapshot_tracked_pools()) {
                    for (const auto& w : pool->snapshot_workers()) {
                        if (w.pid > 0) {
                            live_pids.insert(w.pid);
                        }
                        (void)sampler_->sample_worker_bytes(w.pid, w.cgroup_path,
                                                            /*force_refresh=*/true);
                    }
                }
                sampler_->prune_pids(live_pids);
            }
            lock.lock();
        }
    });
}

void GlobalEvictionCoordinator::stop_and_join() {
    std::thread to_join;
    {
        std::lock_guard<std::mutex> lock(monitor_mutex_);
        monitor_active_.store(false, std::memory_order_release);
        if (!monitor_running_) {
            return;
        }
        monitor_stop_ = true;
        monitor_running_ = false;
        to_join = std::move(monitor_thread_);
    }
    monitor_cv_.notify_all();
    if (to_join.joinable()) {
        to_join.join();
    }
}

} // namespace capsid::host
