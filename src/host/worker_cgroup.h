#ifndef CAPSID_HOST_WORKER_CGROUP_H
#define CAPSID_HOST_WORKER_CGROUP_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct capsid_worker;

namespace capsid::host {

// RAII ownership of a per-worker leaf cgroup directory created by the Host
// beneath isolation.cgroupRoot (Linux only). Destructor removes the leaf
// directory after the worker process has been reaped and cgroup.procs is empty.
struct WorkerCgroupLease {
    std::string path;

    WorkerCgroupLease() = default;
    explicit WorkerCgroupLease(std::string cgroup_path) : path(std::move(cgroup_path)) {}
    ~WorkerCgroupLease();

    WorkerCgroupLease(const WorkerCgroupLease&) = delete;
    WorkerCgroupLease& operator=(const WorkerCgroupLease&) = delete;
    WorkerCgroupLease(WorkerCgroupLease&& other) noexcept : path(std::move(other.path)) {
        other.path.clear();
    }
    WorkerCgroupLease& operator=(WorkerCgroupLease&& other) noexcept {
        if (this != &other) {
            release();
            path = std::move(other.path);
            other.path.clear();
        }
        return *this;
    }

    void release();
};

// Creates a unique per-worker leaf directory (<cgroup_root>/worker-<pid>-<seq>)
// on Linux. Returns nullptr with *error set on non-Linux or mkdir failure.
std::shared_ptr<WorkerCgroupLease> create_worker_cgroup(const std::string& cgroup_root,
                                                        std::string* error);

// Maps host.json isolation.required feature strings to
// CAPSID_SANDBOX_FEATURE_* bitflags. Returns false on unrecognized names.
bool parse_isolation_required_features(const std::vector<std::string>& required,
                                       std::uint32_t* features_out, std::string* error);

// Short-lived handoff table bridging spawn_loaded_worker() -> WorkerExecutor
// (adopt/start) or direct destroy paths before executor adoption.
void register_worker_cgroup(capsid_worker* worker, std::shared_ptr<WorkerCgroupLease> lease);
std::shared_ptr<WorkerCgroupLease> take_worker_cgroup(capsid_worker* worker);
void release_worker_cgroup(capsid_worker* worker);

} // namespace capsid::host

#endif
