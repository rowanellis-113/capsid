// Cross-platform local development Host shell (docs/config-v2-design.md §5).
// Mounts an isomorphic application directory (capsid.json + bundle) onto
// GenerationPool + ManagedListener + RoutingTable on Linux, macOS, and Windows.

#ifndef CAPSID_HOST_LOCAL_DEV_HOST_H
#define CAPSID_HOST_LOCAL_DEV_HOST_H

#include "host/host_config_model.h"
#include "host/protocol_launch.h"
#include "worker_exit_status.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace capsid::host {

class LocalDevHostImpl;
class MetricsRegistry;
class StructuredLog;
struct BindingRegistrySnapshot;

struct LocalDevHostOptions {
    std::string app_dir = ".";
    std::string application = "local";
    std::string host_config_path;
    std::string listen = "127.0.0.1:8080";
    bool listen_explicit = false;
    // Empty = auto-discover sibling capsid-worker executable next to capsid-host.
    std::string worker_path;
    // 0 = honor compiled policy pool.workers; >0 overrides worker count for local runs.
    std::uint32_t workers_override = 0;
    std::string secrets_dir;
    std::string bindings_dir;
    std::shared_ptr<const BindingRegistrySnapshot> binding_registry;
    bool strict_sandbox = false;
    int ready_fd = -1;
    bool write_ready_record = true;
    bool replace_exited_workers = true;
    std::chrono::milliseconds drain_deadline = std::chrono::seconds(5);
    capsid::host::ProtocolLaunch protocol;
    bool protocol_explicit = false;
    ChannelBudgetLimits channel_service_limits{1024, 128, 16ULL * 1024ULL * 1024ULL};
    ChannelBudgetLimits channel_listener_limits{128, 0, 16ULL * 1024ULL * 1024ULL};
    std::shared_ptr<ServiceChannelBudget> channel_service_budget;
    std::uint32_t initial_stream_window = 65536U;
    StructuredLog* log = nullptr;
    MetricsRegistry* metrics = nullptr;

    // Optional overrides used by C++ integration/contract tests while still
    // validating documents through validate_config_json + compile_policy.
    std::string capsid_json_path;
    std::optional<std::string> capsid_json_text;
    bool capsid_json_required = true;
    std::string source_name;
    std::optional<ListenerConfig> listener_override;
    std::optional<std::uint64_t> max_request_body_bytes_override;
};

class LocalDevHost {
  public:
    struct DrainMetrics {
        bool draining = false;
        bool finished = false;
        std::uint64_t forced_cancellations = 0;
        std::uint64_t total_ms = 0;
    };

    explicit LocalDevHost(LocalDevHostOptions options);
    ~LocalDevHost();

    LocalDevHost(const LocalDevHost&) = delete;
    LocalDevHost& operator=(const LocalDevHost&) = delete;

    bool start(std::string* error);
    bool start(const std::vector<std::uint8_t>& bundle, std::string* error);
    void request_stop();
    void begin_drain(std::uint64_t deadline_ms);
    bool wait(std::string* error);
    int run();
    int run(const std::vector<std::uint8_t>& bundle);

    std::uint16_t bound_port() const;
    std::size_t active_workers() const;
    std::vector<capsid::WorkerExitStatus> worker_exit_statuses() const;
    DrainMetrics drain_metrics() const;

  private:
    std::shared_ptr<LocalDevHostImpl> impl_;
};

// Discovers a sibling capsid-worker executable next to the running process.
bool discover_sibling_worker_path(std::string* out_path);

} // namespace capsid::host

#endif
