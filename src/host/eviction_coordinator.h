#ifndef CAPSID_HOST_EVICTION_COORDINATOR_H
#define CAPSID_HOST_EVICTION_COORDINATOR_H

#include "host/generation_pool.h"
#include "host/host_config_model.h"
#include "host/memory_sampler.h"
#include "host/metrics.h"
#include "host/worker_capacity_ledger.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace capsid::host {

// Global user-space memory watermark & soft tenant fairness coordinator
// (docs/config-v2-design.md §§7-8).
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
//     Phase 2 ("global_lru"): when no overshared tenant has an evictable idle
//       worker, evict the globally oldest idle worker.
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

    TenantQuotaConfig resolve_tenant_quota(std::string_view tenant) const;

    // Pre-deploy / pre-recover gate checked before reserving steady minReady in ledger.
    // May synchronously evict idle elastic workers of sibling apps in the same tenant
    // ("tenant_quota_swap") to keep total live workers within quota.max_workers.
    bool check_deploy_quota(std::string_view application, std::string_view tenant,
                            std::uint32_t new_min_ready, std::string* error = nullptr);

    // Synchronous elastic scale-out admission hook installed into GenerationPool.
    bool acquire_worker_slot(const GenerationPool* requester, std::string* rejection_reason);

    // Runs one watermark check/eviction cycle; returns count of evicted workers.
    std::size_t run_eviction_cycle(bool refresh_samples = false);

    // Optional background periodic check loop (active when memory.configured and
    // memory.check_interval_ms > 0). Safe to call even if disabled; stop_and_join is
    // idempotent and called automatically in ~GlobalEvictionCoordinator().
    void start();
    void stop_and_join();

  private:
    std::size_t run_eviction_cycle_locked(bool refresh_samples, bool require_elastic_held);
    std::vector<std::shared_ptr<GenerationPool>> snapshot_active_pools() const;

    const MemoryBudgetConfig memory_;
    const TenancyConfig tenancy_;
    WorkerCapacityLedger* ledger_ = nullptr;
    MemorySampler* sampler_ = nullptr;
    MetricsRegistry* metrics_ = nullptr;

    mutable std::mutex registry_mutex_;
    mutable std::vector<std::weak_ptr<GenerationPool>> pools_;

    std::mutex eviction_mutex_;

    std::mutex monitor_mutex_;
    std::condition_variable monitor_cv_;
    bool monitor_running_ = false;
    bool monitor_stop_ = false;
    std::thread monitor_thread_;
};

} // namespace capsid::host

#endif
