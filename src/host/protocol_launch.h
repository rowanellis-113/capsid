#ifndef CAPSID_HOST_PROTOCOL_LAUNCH_H
#define CAPSID_HOST_PROTOCOL_LAUNCH_H

#include "capsid/runtime.h"

#include "protocol.h"
#include "host/host_config_model.h"

#include <string>

namespace capsid {
namespace host {

// First-party hosts launch workers through capsid_worker_spawn_ex at wire
// version 4, requesting the default WebSocket/channel capability set
// (DUPLEX_CHANNEL | UPGRADE | CHANNEL_LAYOUT_A2 | CHANNEL_CONFIRM |
// CHANNEL_CLOSE_FINAL | FETCH_UPGRADE | CONNECTION_LIFETIME). When a
// host.json "channels" section is configured, from() additionally requests
// CAPSID_PROTOCOL_CAP_HALF_CLOSE and forwards the configured channel limits.
// A worker that does not answer a capability bit leaves it out of the
// effective set so unsupported operations fail fast and explicitly. The wire
// version is never auto-downgraded.
struct ProtocolLaunch {
    capsid_protocol_options options{};

    // The options descriptor must ALWAYS be initialized (two-way size
    // negotiation stamps the caller size); a raw aggregate default
    // would leave struct_size at zero and spawn_ex would refuse.
    ProtocolLaunch() {
        capsid_protocol_options_init(&options, sizeof(options));
        options.capabilities =
            CAPSID_PROTOCOL_CAP_DUPLEX_CHANNEL | CAPSID_PROTOCOL_CAP_UPGRADE |
            CAPSID_PROTOCOL_CAP_CHANNEL_LAYOUT_A2 | CAPSID_PROTOCOL_CAP_CHANNEL_CONFIRM |
            CAPSID_PROTOCOL_CAP_CHANNEL_CLOSE_FINAL | CAPSID_PROTOCOL_CAP_FETCH_UPGRADE |
            CAPSID_PROTOCOL_CAP_CONNECTION_LIFETIME;
    }

    static ProtocolLaunch from(const ChannelsConfig& channels) {
        ProtocolLaunch launch;
        if (channels.configured) {
            launch.options.capabilities =
                CAPSID_PROTOCOL_CAP_CHANNEL_LAYOUT_A2 | CAPSID_PROTOCOL_CAP_DUPLEX_CHANNEL |
                CAPSID_PROTOCOL_CAP_UPGRADE | CAPSID_PROTOCOL_CAP_HALF_CLOSE |
                CAPSID_PROTOCOL_CAP_CHANNEL_CONFIRM | CAPSID_PROTOCOL_CAP_CHANNEL_CLOSE_FINAL |
                CAPSID_PROTOCOL_CAP_FETCH_UPGRADE | CAPSID_PROTOCOL_CAP_CONNECTION_LIFETIME;
            launch.options.max_channels = channels.max_channels;
            launch.options.max_message_bytes = channels.max_message_bytes;
            launch.options.reassembly_budget_bytes = channels.reassembly_budget_bytes;
            launch.options.send_budget_bytes = channels.send_budget_bytes;
            launch.options.close_timeout_ms = channels.close_timeout_ms;
            launch.options.dial_timeout_ms = channels.dial_timeout_ms;
            launch.options.idle_timeout_ms = channels.idle_timeout_ms;
            launch.options.per_channel_queue_bytes = channels.per_channel_queue_bytes;
        }
        return launch;
    }
};

inline capsid_result spawn_with_protocol(const capsid_worker_config* config,
                                         const ProtocolLaunch& launch, capsid_worker** out) {
    return capsid_worker_spawn_ex(config, &launch.options, out);
}

} // namespace host
} // namespace capsid

#endif
