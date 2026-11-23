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
    std::uint64_t bytes = 0;
    std::uint64_t last_active_at_ms = 0;
};

struct FleetMemoryState {
    std::uint64_t total_bytes = 0;
    std::unordered_map<std::string, std::uint64_t> tenant_bytes;
    std::unordered_map<std::string, std::size_t> tenant_live_workers;
    std::vector<EvictionCandidate> candidates;
};

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

} // namespace

GlobalEvictionCoordinator::GlobalEvictionCoordinator(MemoryBudgetConfig memory,
                                                     TenancyConfig tenancy,
                                                     WorkerCapacityLedger* ledger,
                                                     MemorySampler* sampler,
                                                     MetricsRegistry* metrics)
    : memory_(std::move(memory)), tenancy_(std::move(tenancy)), ledger_(ledger), sampler_(sampler),
      metrics_(metrics) {}

GlobalEvictionCoordinator::~GlobalEvictionCoordinator() { stop_and_join(); }

void GlobalEvictionCoordinator::register_pool(const std::shared_ptr<GenerationPool>& pool) {
    if (!pool) {
        return;
    }
    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto it = pools_.begin(); it != pools_.end();) {
        const auto existing = it->lock();
        if (!existing || existing->state() != GenerationPool::State::kActive) {
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

TenantQuotaConfig GlobalEvictionCoordinator::resolve_tenant_quota(std::string_view tenant) const {
    TenantQuotaConfig resolved = tenancy_.default_quota;
    const auto found = tenancy_.overrides.find(std::string(tenant));
    if (found != tenancy_.overrides.end()) {
        if (found->second.max_workers != 0) {
            resolved.max_workers = found->second.max_workers;
        }
        if (found->second.min_soft_memory_bytes != 0) {
            resolved.min_soft_memory_bytes = found->second.min_soft_memory_bytes;
        }
        if (found->second.hard_memory_bytes != 0) {
            resolved.hard_memory_bytes = found->second.hard_memory_bytes;
        }
    }
    return resolved;
}

std::vector<std::shared_ptr<GenerationPool>>
GlobalEvictionCoordinator::snapshot_active_pools() const {
    std::vector<std::shared_ptr<GenerationPool>> active;
    std::lock_guard<std::mutex> lock(registry_mutex_);
    for (auto it = pools_.begin(); it != pools_.end();) {
        auto pool = it->lock();
        if (!pool || pool->state() != GenerationPool::State::kActive) {
            it = pools_.erase(it);
            continue;
        }
        active.push_back(std::move(pool));
        ++it;
    }
    return active;
}

bool GlobalEvictionCoordinator::check_deploy_quota(std::string_view application,
                                                   std::string_view tenant,
                                                   std::uint32_t new_min_ready,
                                                   std::string* error) {
    const std::string effective_tenant(tenant.empty() ? application : tenant);
    const TenantQuotaConfig quota = resolve_tenant_quota(effective_tenant);
    if (quota.max_workers == 0) {
        return true;
    }
    std::lock_guard<std::mutex> lock(eviction_mutex_);
    const auto pools = snapshot_active_pools();
    std::uint64_t other_committed = 0;
    std::uint64_t other_live = 0;
    std::vector<std::shared_ptr<GenerationPool>> tenant_pools;
    for (const auto& pool : pools) {
        if (pool->application_id() == application) {
            continue;
        }
        if (pool->tenant_id() == effective_tenant) {
            other_committed += pool->min_ready();
            other_live += pool->active_or_spawning_workers();
            tenant_pools.push_back(pool);
        }
    }
    if (other_committed + new_min_ready > quota.max_workers) {
        if (metrics_ != nullptr) {
            metrics_->count_pool_rejection(std::string(application), effective_tenant,
                                           "tenant_max_workers");
        }
        if (error != nullptr) {
            *error = "tenant maxWorkers quota exceeded";
        }
        return false;
    }
    const std::uint64_t needed_slots = std::max<std::uint32_t>(1U, new_min_ready);
    while (other_live + needed_slots > quota.max_workers) {
        std::shared_ptr<GenerationPool> best_pool;
        std::uint64_t best_active_at = std::numeric_limits<std::uint64_t>::max();
        const std::uint64_t now = steady_now_ms();
        for (const auto& pool : tenant_pools) {
            const auto workers = pool->snapshot_workers();
            if (workers.size() <= pool->min_ready()) {
                continue;
            }
            for (const auto& w : workers) {
                if (w.idle && worker_past_residency(w, memory_.min_residency_ms, now) &&
                    w.last_active_at_ms < best_active_at) {
                    best_active_at = w.last_active_at_ms;
                    best_pool = pool;
                }
            }
        }
        if (!best_pool ||
            !best_pool->evict_idle_worker(memory_.min_residency_ms, "tenant_quota_swap")) {
            break;
        }
        --other_live;
    }
    return true;
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
    const TenantQuotaConfig quota = resolve_tenant_quota(tenant);

    const auto collect_tenant_state =
        [&](std::vector<std::shared_ptr<GenerationPool>>* tenant_pools,
            std::size_t* tenant_active_workers, std::uint64_t* tenant_bytes) {
            tenant_pools->clear();
            *tenant_active_workers = 0;
            *tenant_bytes = 0;
            const auto pools = snapshot_active_pools();
            bool saw_requester = false;
            for (const auto& pool : pools) {
                if (pool.get() == requester) {
                    saw_requester = true;
                }
                if (pool->tenant_id() != tenant) {
                    continue;
                }
                tenant_pools->push_back(pool);
                *tenant_active_workers += pool->active_or_spawning_workers();
                if (sampler_ != nullptr) {
                    for (const auto& w : pool->snapshot_workers()) {
                        *tenant_bytes += sampler_->sample_worker_bytes(w.pid, w.cgroup_path, false);
                    }
                }
            }
            if (!saw_requester) {
                *tenant_active_workers += requester->active_or_spawning_workers();
                if (sampler_ != nullptr) {
                    for (const auto& w : requester->snapshot_workers()) {
                        *tenant_bytes += sampler_->sample_worker_bytes(w.pid, w.cgroup_path, false);
                    }
                }
            }
        };

    const auto try_evict_within_tenant =
        [&](const std::vector<std::shared_ptr<GenerationPool>>& tenant_pools) -> bool {
        const std::uint64_t now = steady_now_ms();
        std::shared_ptr<GenerationPool> best_pool;
        std::uint64_t best_active_at = std::numeric_limits<std::uint64_t>::max();
        for (const auto& pool : tenant_pools) {
            const auto workers = pool->snapshot_workers();
            if (workers.size() <= pool->min_ready()) {
                continue;
            }
            for (const auto& w : workers) {
                if (w.idle && worker_past_residency(w, memory_.min_residency_ms, now) &&
                    w.last_active_at_ms < best_active_at) {
                    best_active_at = w.last_active_at_ms;
                    best_pool = pool;
                }
            }
        }
        return best_pool &&
               best_pool->evict_idle_worker(memory_.min_residency_ms, "tenant_quota_swap");
    };

    // 1. Tenant hardMemory quota check (with synchronous sibling idle swap).
    if (quota.hard_memory_bytes > 0) {
        std::vector<std::shared_ptr<GenerationPool>> tenant_pools;
        std::size_t tenant_workers = 0;
        std::uint64_t tenant_bytes = 0;
        for (;;) {
            collect_tenant_state(&tenant_pools, &tenant_workers, &tenant_bytes);
            if (tenant_bytes < quota.hard_memory_bytes) {
                break;
            }
            if (!try_evict_within_tenant(tenant_pools)) {
                if (rejection_reason != nullptr) {
                    *rejection_reason = "tenant_hard_memory";
                }
                return false;
            }
        }
    }

    // 2. Tenant maxWorkers quota check (note requester->active_or_spawning_workers() already
    // includes +1 for the pending spawn slot claimed in request_scale_out).
    if (quota.max_workers > 0) {
        std::vector<std::shared_ptr<GenerationPool>> tenant_pools;
        std::size_t tenant_workers = 0;
        std::uint64_t tenant_bytes = 0;
        for (;;) {
            collect_tenant_state(&tenant_pools, &tenant_workers, &tenant_bytes);
            if (tenant_workers <= quota.max_workers) {
                break;
            }
            if (!try_evict_within_tenant(tenant_pools)) {
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
        (void)run_eviction_cycle_locked(/*refresh_samples=*/false, /*require_elastic_held=*/false);
        std::uint64_t total_bytes = 0;
        if (sampler_ != nullptr) {
            for (const auto& pool : snapshot_active_pools()) {
                for (const auto& w : pool->snapshot_workers()) {
                    total_bytes += sampler_->sample_worker_bytes(w.pid, w.cgroup_path, false);
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

    // 4. Global elastic worker-slot ledger check (evict an elastic idle worker on contention).
    if (ledger_ != nullptr && !ledger_->try_acquire_elastic(app)) {
        // Attempt two-phase eviction (tenant_overshare, then global_lru) over elastic_held
        // idle workers so an elastic ledger slot becomes available synchronously.
        const auto evict_one_elastic = [&]() -> bool {
            const auto pools = snapshot_active_pools();
            const std::uint64_t now = steady_now_ms();
            std::unordered_map<std::string, std::uint64_t> tenant_bytes;
            std::unordered_map<std::string, std::size_t> tenant_live;
            std::vector<EvictionCandidate> candidates;
            std::uint64_t total_bytes = 0;
            for (const auto& pool : pools) {
                const auto workers = pool->snapshot_workers();
                const std::string pool_tenant(pool->tenant_id());
                tenant_live[pool_tenant] += workers.size();
                const bool can_evict_pool = workers.size() > pool->min_ready();
                for (const auto& w : workers) {
                    const std::uint64_t b =
                        sampler_ != nullptr
                            ? sampler_->sample_worker_bytes(w.pid, w.cgroup_path, false)
                            : 0;
                    tenant_bytes[pool_tenant] += b;
                    total_bytes += b;
                    if (can_evict_pool && w.idle && w.elastic_held &&
                        worker_past_residency(w, memory_.min_residency_ms, now)) {
                        candidates.push_back(
                            EvictionCandidate{pool, pool_tenant, b, w.last_active_at_ms});
                    }
                }
            }
            if (candidates.empty()) {
                return false;
            }
            std::size_t active_tenants = 0;
            for (const auto& [tid, live] : tenant_live) {
                (void)tid;
                if (live > 0) {
                    ++active_tenants;
                }
            }
            const std::uint64_t budget =
                (memory_.configured && memory_.limit_bytes > 0) ? memory_.limit_bytes : total_bytes;
            const std::uint64_t fair_share =
                active_tenants > 0 ? std::max<std::uint64_t>(1ULL, budget / active_tenants) : 1ULL;

            std::string chosen_reason = "global_lru";
            const EvictionCandidate* chosen = nullptr;
            if (budget > 0) {
                double max_ratio = 1.0;
                for (const auto& c : candidates) {
                    const TenantQuotaConfig q = resolve_tenant_quota(c.tenant);
                    const std::uint64_t soft_share =
                        std::max<std::uint64_t>(fair_share, q.min_soft_memory_bytes);
                    const std::uint64_t used = tenant_bytes[c.tenant];
                    if (used <= soft_share) {
                        continue;
                    }
                    const double ratio =
                        static_cast<double>(used) /
                        static_cast<double>(std::max<std::uint64_t>(1ULL, soft_share));
                    if (ratio > max_ratio ||
                        (ratio == max_ratio &&
                         (chosen == nullptr || c.last_active_at_ms < chosen->last_active_at_ms))) {
                        max_ratio = ratio;
                        chosen = &c;
                        chosen_reason = "tenant_overshare";
                    }
                }
            }
            if (chosen == nullptr) {
                for (const auto& c : candidates) {
                    if (chosen == nullptr || c.last_active_at_ms < chosen->last_active_at_ms) {
                        chosen = &c;
                    }
                }
            }
            return chosen != nullptr &&
                   chosen->pool->evict_idle_worker(memory_.min_residency_ms, chosen_reason,
                                                   /*require_elastic_held=*/true);
        };

        for (;;) {
            if (!evict_one_elastic()) {
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
    return run_eviction_cycle_locked(refresh_samples, /*require_elastic_held=*/false);
}

std::size_t GlobalEvictionCoordinator::run_eviction_cycle_locked(bool refresh_samples,
                                                                 bool require_elastic_held) {
    if (!memory_.configured || memory_.limit_bytes == 0 || sampler_ == nullptr) {
        return 0;
    }
    const std::uint64_t high_bytes =
        watermark_bytes(memory_.limit_bytes, memory_.high_watermark_percent);
    const std::uint64_t low_bytes =
        watermark_bytes(memory_.limit_bytes, memory_.low_watermark_percent);

    const auto gather_state = [&](bool force_refresh) -> FleetMemoryState {
        FleetMemoryState state;
        const auto pools = snapshot_active_pools();
        const std::uint64_t now = steady_now_ms();
        std::unordered_set<std::int64_t> live_pids;
        for (const auto& pool : pools) {
            const auto workers = pool->snapshot_workers();
            const std::string tenant(pool->tenant_id());
            state.tenant_live_workers[tenant] += workers.size();
            const bool pool_above_min = workers.size() > pool->min_ready();
            for (const auto& w : workers) {
                if (w.pid > 0) {
                    live_pids.insert(w.pid);
                }
                const std::uint64_t bytes =
                    sampler_->sample_worker_bytes(w.pid, w.cgroup_path, force_refresh);
                state.total_bytes += bytes;
                state.tenant_bytes[tenant] += bytes;
                if (pool_above_min && w.idle && (!require_elastic_held || w.elastic_held) &&
                    worker_past_residency(w, memory_.min_residency_ms, now)) {
                    state.candidates.push_back(
                        EvictionCandidate{pool, tenant, bytes, w.last_active_at_ms});
                }
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
        std::size_t active_tenants = 0;
        for (const auto& [tid, live] : state.tenant_live_workers) {
            (void)tid;
            if (live > 0) {
                ++active_tenants;
            }
        }
        const std::uint64_t fair_share =
            active_tenants > 0 ? std::max<std::uint64_t>(1ULL, memory_.limit_bytes / active_tenants)
                               : memory_.limit_bytes;

        // Phase 1: tenant_overshare — select the overshared tenant with the largest
        // (tenant_bytes / soft_share) ratio that still has an evictable idle worker, and
        // evict its oldest-active idle worker.
        std::string phase1_tenant;
        double max_ratio = 1.0;
        for (const auto& c : state.candidates) {
            const TenantQuotaConfig q = resolve_tenant_quota(c.tenant);
            const std::uint64_t soft_share =
                std::max<std::uint64_t>(fair_share, q.min_soft_memory_bytes);
            const std::uint64_t used = state.tenant_bytes[c.tenant];
            if (used <= soft_share) {
                continue;
            }
            const double ratio = static_cast<double>(used) /
                                 static_cast<double>(std::max<std::uint64_t>(1ULL, soft_share));
            if (phase1_tenant.empty() || ratio > max_ratio) {
                max_ratio = ratio;
                phase1_tenant = c.tenant;
            }
        }

        const EvictionCandidate* victim = nullptr;
        std::string reason;
        if (!phase1_tenant.empty()) {
            reason = "tenant_overshare";
            for (const auto& c : state.candidates) {
                if (c.tenant != phase1_tenant) {
                    continue;
                }
                if (victim == nullptr || c.last_active_at_ms < victim->last_active_at_ms) {
                    victim = &c;
                }
            }
        } else {
            // Phase 2: global_lru — all tenants with evictable workers are within soft share;
            // fall back to global oldest last_active_at_ms across remaining idle workers.
            reason = "global_lru";
            for (const auto& c : state.candidates) {
                if (victim == nullptr || c.last_active_at_ms < victim->last_active_at_ms) {
                    victim = &c;
                }
            }
        }

        if (victim == nullptr) {
            break;
        }
        if (!victim->pool->evict_idle_worker(memory_.min_residency_ms, reason,
                                             require_elastic_held)) {
            break;
        }
        ++evicted_count;
        // Re-evaluate from sampler cache (zero extra procfs syscalls) so per-pool minReady
        // and updated tenant/total working-set bytes remain exact.
        state = gather_state(/*force_refresh=*/false);
    }

    return evicted_count;
}

void GlobalEvictionCoordinator::start() {
    if (!memory_.configured || memory_.limit_bytes == 0 || memory_.check_interval_ms == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    if (monitor_running_) {
        return;
    }
    monitor_stop_ = false;
    monitor_running_ = true;
    monitor_thread_ = std::thread([this] {
        std::unique_lock<std::mutex> lock(monitor_mutex_);
        while (!monitor_stop_) {
            monitor_cv_.wait_for(lock, std::chrono::milliseconds(memory_.check_interval_ms),
                                 [this] { return monitor_stop_; });
            if (monitor_stop_) {
                break;
            }
            lock.unlock();
            (void)run_eviction_cycle(/*refresh_samples=*/true);
            lock.lock();
        }
    });
}

void GlobalEvictionCoordinator::stop_and_join() {
    std::thread to_join;
    {
        std::lock_guard<std::mutex> lock(monitor_mutex_);
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
