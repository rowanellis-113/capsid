#ifndef CAPSID_HOST_MEMORY_SAMPLER_H
#define CAPSID_HOST_MEMORY_SAMPLER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace capsid::host {

// Read-only dual-backend worker memory sampler:
//   1. When a delegated cgroup v2 path is present, reads `<cgroup>/memory.current`
//      directly (O(1) read-only stat).
//   2. Otherwise falls back to sampling process PSS/RSS via
//      read_process_footprint_bytes and caching the reading per PID until
//      explicitly refreshed so hot admission paths incur zero procfs/OS syscalls.
class MemorySampler {
  public:
    using CgroupReader =
        std::function<bool(std::string_view cgroup_path, std::uint64_t* bytes_out)>;
    using FootprintReader = std::function<bool(std::int64_t pid, std::uint64_t* bytes_out)>;

    MemorySampler();
    MemorySampler(CgroupReader cgroup_reader, FootprintReader footprint_reader);

    std::uint64_t sample_worker_bytes(std::int64_t pid, std::string_view cgroup_path,
                                      bool force_refresh = false);
    void refresh_worker(std::int64_t pid, std::string_view cgroup_path);
    void erase_pid(std::int64_t pid);
    void prune_pids(const std::unordered_set<std::int64_t>& live_pids);
    std::size_t cached_pid_count() const;
    void set_max_age_ms(std::uint64_t max_age_ms) noexcept {
        max_age_ms_.store(max_age_ms, std::memory_order_release);
    }

  private:
    struct CachedSample {
        std::uint64_t bytes = 0;
        std::uint64_t sampled_at_ms = 0;
    };

    CgroupReader cgroup_reader_;
    FootprintReader footprint_reader_;
    std::atomic<std::uint64_t> max_age_ms_{5000};
    mutable std::mutex mutex_;
    std::unordered_map<std::int64_t, CachedSample> cached_pid_bytes_;
};

} // namespace capsid::host

#endif
