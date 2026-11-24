#ifndef CAPSID_HOST_EVICTION_COORDINATOR_H
#define CAPSID_HOST_EVICTION_COORDINATOR_H

#include "host/generation_pool.h"
#include "host/host_config_model.h"
#include "host/memory_sampler.h"
#include "host/metrics.h"
#include "host/worker_capacity_ledger.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace capsid::host {

// Global user-space memory watermark & soft tenant fairness coordinator
// (docs/design/config-v2.md §§7-8).
//
// Invariants:
// - Never evicts workers with inflight requests or active channels/WS.
// - Never drops any pool below its minReady floor or evicts a worker younger
//   than capacity.memory.minResidency.
// - Runs a two-phase eviction pass whenever memory >= highWatermark until
//   memory <= lowWatermark:
//     Phase 1 ("tenant_overshare"): evict LRU idle workers from the tenant with
//       highest (tenant_bytes / soft_share), where
//       soft_share = max(limit / N_active_tenants, minSoftMemory).
//     Phase 2 ("global_lru"): when all resident tenants are within their soft
//       shares, evict the globally oldest idle worker.
// - Synchronous slot acquisition (acquire_worker_slot) enforces tenant
//   hardMemory / maxWorkers (via "tenant_quota_swap") and global memory /
//   elastic slot ceilings, returning explicit rejection reason tags.
class GlobalEvictionCoordinator {
  public:
    GlobalEvictionCoordinator(MemoryBudgetConfig memory, TenancyConfig tenancy,
                              WorkerCapacityLedger* ledger, MemorySampler* sampler,
                              MetricsRegistry* metrics = nullptr);
    ~GlobalEvictionCoordinator();

    GlobalEvictionCoordinator(const GlobalEvictionCoordinator&) = delete;
    GlobalEvictionCoordinator& operator=(const GlobalEvictionCoordinator&) = delete;

    void register_pool(const std::shared_ptr<GenerationPool>& pool);
    void unregister_pool(const GenerationPool* pool);

    // Pre-deploy / pre-recover gate checked before reserving steady minReady in ledger.
    // May synchronously evict idle workers above minReady of sibling apps in the same tenant
    // ("tenant_quota_swap") to satisfy quota.hard_memory_bytes / quota.max_workers.
    bool check_deploy_quota(std::string_view application, std::string_view tenant,
                            std::uint32_t new_min_ready, std::string* error = nullptr);
    // Reserves tenant quota slots for an in-flight deploy warm-up (held until activation
    // registers the pool or the deploy aborts) so concurrent sibling scale-outs/deploys
    // observe the committed + warming footprint.
    bool reserve_deploy_quota(std::string_view application, std::string_view tenant,
                              std::uint32_t new_min_ready, std::uint32_t warm_workers,
                              std::string* error = nullptr);
    void release_deploy_quota(std::string_view application);

    // Synchronous elastic scale-out admission hook installed into GenerationPool.
    bool acquire_worker_slot(const GenerationPool* requester, std::string* rejection_reason);
    // Synchronously reclaims one idle worker above minReady across active pools
    // to free a ledger slot during deploy admission contention.
    bool reclaim_one_idle_elastic();

    // Runs one watermark check/eviction cycle; returns count of evicted workers.
    std::size_t run_eviction_cycle(bool refresh_samples = false);

    // Optional background periodic check loop (active when memory.configured and
    // memory.check_interval_ms > 0). Safe to call even if disabled; stop_and_join is
    // idempotent and called automatically in ~GlobalEvictionCoordinator().
    void start();
    void stop_and_join();

  private:
    struct PendingDeploy {
        std::string tenant;
        std::uint32_t warm_workers = 0;
        std::uint32_t min_ready = 0;
    };

    bool has_tenant_memory_limits() const;
    bool check_or_reserve_deploy_quota_locked(std::string_view application, std::string_view tenant,
                                              std::uint32_t new_min_ready,
                                              std::uint32_t warm_workers, bool record_reservation,
                                              std::string* error);
    bool evict_one_elastic_locked();
    std::size_t run_eviction_cycle_locked(bool refresh_samples);
    std::vector<std::shared_ptr<GenerationPool>> snapshot_tracked_pools() const;

    const MemoryBudgetConfig memory_;
    const TenancyConfig tenancy_;
    WorkerCapacityLedger* ledger_ = nullptr;
    MemorySampler* sampler_ = nullptr;
    MetricsRegistry* metrics_ = nullptr;

    mutable std::mutex registry_mutex_;
    mutable std::vector<std::weak_ptr<GenerationPool>> pools_;

    std::mutex eviction_mutex_;
    std::unordered_map<std::string, PendingDeploy> pending_deploys_;
    // PIDs already chosen for async idle eviction in the current or immediately
    // preceding cached pass; cleared at the start of every fresh sampling cycle
    // (refresh_samples=true) so still-living retired processes are re-metered.
    std::unordered_set<std::int64_t> recently_evicted_pids_;

    std::mutex monitor_mutex_;
    std::condition_variable monitor_cv_;
    std::atomic<bool> monitor_active_{false};
    bool monitor_running_ = false;
    bool monitor_stop_ = false;
    std::thread monitor_thread_;
};

} // namespace capsid::host

#endif
