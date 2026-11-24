#include "host/worker_cgroup.h"

#include "capsid/runtime.h"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <map>
#include <mutex>

#if defined(__linux__)
#include "cgroup_io.h"
#include "host/config.h"
#include <linux/magic.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>
#endif

namespace capsid::host {

namespace {

#if defined(__linux__)
// Serialize creation of the first leaf with removal of the last leaf for the same tenant.
std::mutex g_cgroup_tree_mutex;

bool configure_tenant_memory(int directory, std::uint64_t bytes) {
    struct statfs fs{};
    if (::fstatfs(directory, &fs) != 0 ||
        static_cast<unsigned long>(fs.f_type) != CGROUP2_SUPER_MAGIC) {
        return false;
    }
    const std::string desired = std::to_string(bytes);
    std::string actual;
    if (!cgroup_read_at(directory, "memory.max", &actual)) {
        return false;
    }
    if (!cgroup_value_matches("memory.max", desired, actual)) {
        // An empty group may survive a Host restart. Never change an occupied group's
        // budget while admitting another generation.
        std::string events;
        if (!cgroup_read_at(directory, "cgroup.events", &events) ||
            ("\n" + events + "\n").find("\npopulated 0\n") == std::string::npos ||
            !cgroup_write_at(directory, "memory.max", desired) ||
            !cgroup_read_at(directory, "memory.max", &actual) ||
            !cgroup_value_matches("memory.max", desired, actual)) {
            return false;
        }
    }
    return cgroup_write_at(directory, "cgroup.subtree_control", "+memory");
}
#endif

std::mutex g_worker_cgroup_mutex;
std::map<capsid_worker*, std::shared_ptr<WorkerCgroupLease>> g_worker_cgroups;

} // namespace

WorkerCgroupLease::~WorkerCgroupLease() { release(); }

void WorkerCgroupLease::release() {
#if defined(__linux__)
    if (!path.empty()) {
        std::lock_guard<std::mutex> lock(g_cgroup_tree_mutex);
        ::rmdir(path.c_str());
        if (!tenant_path.empty()) {
            ::rmdir(tenant_path.c_str()); // remains busy until its last worker leaf is gone
        }
    }
#endif
    path.clear();
    tenant_path.clear();
}

std::shared_ptr<WorkerCgroupLease> create_worker_cgroup(const std::string& cgroup_root,
                                                        std::string* error,
                                                        const std::string& tenant,
                                                        std::uint64_t hard_memory_bytes) {
#if !defined(__linux__)
    (void)cgroup_root;
    (void)tenant;
    (void)hard_memory_bytes;
    if (error != nullptr) {
        *error = "cgroup v2 isolation requires Linux";
    }
    return nullptr;
#else
    if (cgroup_root.empty() || cgroup_root.front() != '/') {
        if (error != nullptr) {
            *error = "invalid isolation.cgroupRoot path";
        }
        return nullptr;
    }
    std::string base = cgroup_root;
    while (base.size() > 1 && base.back() == '/') {
        base.pop_back();
    }
    std::lock_guard<std::mutex> lock(g_cgroup_tree_mutex);
    std::string tenant_path;
    if (hard_memory_bytes != 0) {
        if (!valid_tenant_id(tenant)) {
            if (error)
                *error = "invalid cgroup tenant id";
            return nullptr;
        }
        tenant_path = base + "/tenant-" + tenant;
        const bool created = ::mkdir(tenant_path.c_str(), 0755) == 0;
        if (!created && errno != EEXIST) {
            if (error)
                *error = "cannot create tenant cgroup";
            return nullptr;
        }
        const int fd = ::open(tenant_path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        const bool configured = fd >= 0 && configure_tenant_memory(fd, hard_memory_bytes);
        if (fd >= 0)
            ::close(fd);
        if (!configured) {
            if (created)
                ::rmdir(tenant_path.c_str());
            if (error)
                *error = "cannot enforce tenant hardMemory in delegated cgroup";
            return nullptr;
        }
        base = tenant_path;
    }
    static std::atomic<std::uint64_t> counter{0};
    const std::uint64_t seq = counter.fetch_add(1, std::memory_order_relaxed);
    std::string leaf = base + "/worker-" + std::to_string(::getpid()) + "-" + std::to_string(seq);
    if (::mkdir(leaf.c_str(), 0755) != 0) {
        if (error != nullptr) {
            *error = std::string("cannot create worker leaf cgroup: ") + std::strerror(errno);
        }
        if (!tenant_path.empty())
            ::rmdir(tenant_path.c_str());
        return nullptr;
    }
    return std::make_shared<WorkerCgroupLease>(std::move(leaf), std::move(tenant_path));
#endif
}

bool parse_isolation_required_features(const std::vector<std::string>& required,
                                       std::uint32_t* features_out, std::string* error) {
    std::uint32_t mask = 0;
    for (const std::string& entry : required) {
        if (entry == "strict" || entry == "strict-base" || entry == "strict_base") {
            mask |= CAPSID_SANDBOX_FEATURE_STRICT_BASE;
        } else if (entry == "rlimits") {
            mask |= CAPSID_SANDBOX_FEATURE_RLIMITS;
        } else if (entry == "no_new_privs" || entry == "no-new-privs") {
            mask |= CAPSID_SANDBOX_FEATURE_NO_NEW_PRIVS;
        } else if (entry == "seccomp") {
            mask |= CAPSID_SANDBOX_FEATURE_SECCOMP;
        } else if (entry == "landlock") {
            mask |= CAPSID_SANDBOX_FEATURE_LANDLOCK;
        } else if (entry == "user_namespace" || entry == "user-ns" || entry == "userns") {
            mask |= CAPSID_SANDBOX_FEATURE_USER_NAMESPACE;
        } else if (entry == "mount_namespace" || entry == "mount-ns" || entry == "mountns") {
            mask |= CAPSID_SANDBOX_FEATURE_MOUNT_NAMESPACE;
        } else if (entry == "ipc_namespace" || entry == "ipc-ns" || entry == "ipcns") {
            mask |= CAPSID_SANDBOX_FEATURE_IPC_NAMESPACE;
        } else if (entry == "uts_namespace" || entry == "uts-ns" || entry == "utsns") {
            mask |= CAPSID_SANDBOX_FEATURE_UTS_NAMESPACE;
        } else if (entry == "cgroup-v2" || entry == "cgroup_v2" || entry == "cgroup") {
            mask |= CAPSID_SANDBOX_FEATURE_CGROUP_V2;
        } else if (entry == "network_namespace" || entry == "net-ns" || entry == "netns") {
            mask |= CAPSID_SANDBOX_FEATURE_NETWORK_NAMESPACE;
        } else if (entry == "pid" || entry == "pid_namespace" || entry == "pid-ns") {
            // PID isolation is always applied when supported by strict spawn.
            continue;
        } else {
            if (error != nullptr) {
                *error = "unsupported isolation.required feature: " + entry;
            }
            return false;
        }
    }
    if (features_out != nullptr) {
        *features_out = mask;
    }
    return true;
}

void register_worker_cgroup(capsid_worker* worker, std::shared_ptr<WorkerCgroupLease> lease) {
    if (worker == nullptr || !lease) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_worker_cgroup_mutex);
    g_worker_cgroups[worker] = std::move(lease);
}

std::shared_ptr<WorkerCgroupLease> take_worker_cgroup(capsid_worker* worker) {
    if (worker == nullptr) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(g_worker_cgroup_mutex);
    const auto it = g_worker_cgroups.find(worker);
    if (it == g_worker_cgroups.end()) {
        return nullptr;
    }
    std::shared_ptr<WorkerCgroupLease> lease = std::move(it->second);
    g_worker_cgroups.erase(it);
    return lease;
}

void release_worker_cgroup(capsid_worker* worker) { (void)take_worker_cgroup(worker); }

} // namespace capsid::host
