// Typed host.json model implementation — see host_config_model.h.
//
// This parser runs AFTER validate_config_json; every field the schema
// accepts must be mapped here or the document is rejected. Both capsid/host-v1
// (frozen) and capsid/host-v2 (orthogonal schema) map into the unified typed
// ParsedHostConfig model cleanly.

#include "host/host_config_model.h"

#include "host/config.h"
#include "host/units.h"
#include "protocol.h"
#include "host/listener_cors.h"
#include "host/request_normalization.h"

#include <jansson.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <utility>

namespace capsid::host {

namespace {

// ---- jansson helpers ------------------------------------------------------

std::string json_string_field(json_t* object, const char* key) {
    json_t* value = json_object_get(object, key);
    return json_is_string(value) ? json_string_value(value) : std::string();
}

bool parse_string_array(json_t* parent, const char* key, std::vector<std::string>* out) {
    json_t* value = json_object_get(parent, key);
    if (value == nullptr) {
        return true;
    }
    if (!json_is_array(value)) {
        return false;
    }
    std::size_t index = 0;
    json_t* item = nullptr;
    json_array_foreach(value, index, item) {
        if (!json_is_string(item)) {
            return false;
        }
        out->push_back(json_string_value(item));
    }
    return true;
}

// ---- frozen text grammars (error-returning; no fail()/exit) ---------------

// "256MiB" style size with explicit suffix (the frozen size grammar).
bool parse_size_bytes_text(const std::string& text, std::uint64_t* out) {
    return units::parse_size_bytes(text, out, /*allow_bare_bytes=*/false);
}

// "250ms" / "5s" / "1m" duration (the frozen duration grammar); unit is
// required and the value must be a positive integer.
bool parse_duration_ms_text(const std::string& text, std::uint64_t* out) {
    std::uint64_t parsed = 0;
    if (!units::parse_duration_ms(text, &parsed, /*allow_hours=*/false) || parsed == 0) {
        return false;
    }
    *out = parsed;
    return true;
}

// "jitter": a percent text like "20%" (the design's contract, §5.1) or a
// plain decimal basis-points text like "1000" (the pool's contract); both
// resolve to basis points in 0..10000 ("20%" = 2000). A bare percent
// (no digits) or a value over the range is rejected.
bool parse_jitter_basis_points_text(const std::string& text, std::uint64_t* out) {
    std::string::size_type digits_end = 0;
    for (std::string::size_type index = 0; index < text.size(); ++index) {
        const char c = text[index];
        if (c < '0' || c > '9') {
            break;
        }
        digits_end = index + 1;
    }
    if (digits_end == 0) {
        return false;
    }
    const std::string suffix = text.substr(digits_end);
    if (suffix != "" && suffix != "%") {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const std::string numeric_text = text.substr(0, digits_end);
    const unsigned long long parsed = std::strtoull(numeric_text.c_str(), &end, 10);
    const unsigned long long limit = suffix == "%" ? 100ULL : 10000ULL;
    const unsigned long long multiplier = suffix == "%" ? 100ULL : 1ULL;
    if (errno != 0 || end == nullptr || *end != '\0' || parsed > limit ||
        parsed > std::numeric_limits<std::uint64_t>::max() / multiplier) {
        return false;
    }
    *out = static_cast<std::uint64_t>(parsed) * multiplier;
    return true;
}

// ---- typed field readers --------------------------------------------------

// Non-negative integer member (the schema allows negative integers for
// ceiling fields; the parse must reject them).
bool parse_uint_field(json_t* object, const char* key, std::uint64_t* out) {
    json_t* value = json_object_get(object, key);
    if (value == nullptr) {
        return true;
    }
    if (!json_is_integer(value)) {
        return false;
    }
    const json_int_t parsed = json_integer_value(value);
    if (parsed < 0) {
        return false;
    }
    *out = static_cast<std::uint64_t>(parsed);
    return true;
}

bool parse_size_text_field(json_t* object, const char* key, std::uint64_t* out) {
    json_t* value = json_object_get(object, key);
    if (value == nullptr) {
        return true;
    }
    if (!json_is_string(value)) {
        return false;
    }
    return parse_size_bytes_text(json_string_value(value), out);
}

bool parse_duration_text_field(json_t* object, const char* key, std::uint64_t* out) {
    json_t* value = json_object_get(object, key);
    if (value == nullptr) {
        return true;
    }
    if (!json_is_string(value)) {
        return false;
    }
    return parse_duration_ms_text(json_string_value(value), out);
}

// ---- section parsers ------------------------------------------------------

bool parse_listeners(json_t* root, std::vector<ListenerConfig>* out, bool is_v2) {
    json_t* listeners = json_object_get(root, "listeners");
    if (listeners == nullptr) {
        return !is_v2;
    }
    if (!json_is_array(listeners)) {
        return false;
    }
    if (is_v2 && json_array_size(listeners) == 0) {
        return false;
    }
    std::size_t index = 0;
    json_t* listener = nullptr;
    json_array_foreach(listeners, index, listener) {
        if (!json_is_object(listener)) {
            return false;
        }
        ListenerConfig config;
        config.name = json_string_field(listener, "name");
        config.tcp = json_string_field(listener, "tcp");
        if (is_v2) {
            const std::string origin = json_string_field(listener, "publicOrigin");
            if (!origin.empty()) {
                if (!parse_public_origin(origin, &config.public_scheme, &config.public_authority)) {
                    return false;
                }
            } else {
                config.public_scheme = "http";
                config.public_authority = config.tcp;
                config.public_origin_auto = true;
            }
        } else {
            config.public_scheme = json_string_field(listener, "publicScheme");
            config.public_authority = json_string_field(listener, "publicAuthority");
        }
        const json_t* trusted = json_object_get(listener, "trusted");
        config.trusted = json_is_true(trusted); // absent == false
        json_t* routing = json_object_get(listener, "routing");
        if (json_is_object(routing)) {
            config.routing.mode = json_string_field(routing, "mode");
            config.routing.suffix = json_string_field(routing, "suffix");
            config.routing.header = json_string_field(routing, "header");
        }
        json_t* limits = json_object_get(listener, "limits");
        if (json_is_object(limits)) {
            if (!parse_uint_field(limits, "connections", &config.limits.connections) ||
                !parse_size_text_field(limits, is_v2 ? "headerSize" : "headerBytes",
                                       &config.limits.header_bytes) ||
                !parse_duration_text_field(limits, "headerTimeout",
                                           &config.limits.header_timeout_ms) ||
                !parse_duration_text_field(limits, "bodyIdleTimeout",
                                           &config.limits.body_idle_timeout_ms) ||
                !parse_duration_text_field(limits, "streamIdleTimeout",
                                           &config.limits.stream_idle_timeout_ms) ||
                !parse_duration_text_field(limits, "writeTimeout",
                                           &config.limits.write_timeout_ms)) {
                return false;
            }
            if (!parse_uint_field(limits, "channels", &config.limits.channels) ||
                !config.limits.channels || (is_v2 && config.limits.channels > 1024)) {
                return false;
            }
            if (!is_v2 && (!parse_size_text_field(limits, "channelQueueBytes",
                                                  &config.limits.channel_queue_bytes) ||
                           !config.limits.channel_queue_bytes)) {
                return false;
            }
        }
        json_t* cors = json_object_get(listener, "cors");
        if (json_is_object(cors)) {
            config.cors.configured = true;
            if (!parse_string_array(cors, "allowedOrigins", &config.cors.allowed_origins) ||
                !parse_string_array(cors, "allowedMethods", &config.cors.allowed_methods) ||
                !parse_string_array(cors, "allowedHeaders", &config.cors.allowed_headers) ||
                !parse_duration_text_field(cors, "maxAge", &config.cors.max_age_ms)) {
                return false;
            }
            if (config.cors.allowed_origins.empty() || config.cors.allowed_methods.empty()) {
                return false;
            }
            if (config.cors.allowed_origins.size() > 1 &&
                std::find(config.cors.allowed_origins.begin(), config.cors.allowed_origins.end(),
                          "*") != config.cors.allowed_origins.end()) {
                return false;
            }
            for (const std::string& origin : config.cors.allowed_origins) {
                if (!valid_cors_origin(origin)) {
                    return false;
                }
            }
            for (std::string& method : config.cors.allowed_methods) {
                if (!valid_cors_method_token(method)) {
                    return false;
                }
                for (char& c : method) {
                    if (c >= 'a' && c <= 'z') {
                        c = static_cast<char>(c - 'a' + 'A');
                    }
                }
            }
            for (std::string& header : config.cors.allowed_headers) {
                if (!valid_cors_header_token(header)) {
                    return false;
                }
                for (char& c : header) {
                    if (c >= 'A' && c <= 'Z') {
                        c = static_cast<char>(c - 'A' + 'a');
                    }
                }
            }
        }
        out->push_back(std::move(config));
    }
    return true;
}

bool parse_permissions(json_t* root, HostPolicy* policy, bool is_v2) {
    json_t* permissions = json_object_get(root, "permissions");
    if (permissions == nullptr) {
        return true;
    }
    if (!json_is_object(permissions)) {
        return false;
    }
    if (!parse_string_array(permissions, "modules", &policy->module_allowlist) ||
        !parse_string_array(permissions, "environmentNames", &policy->env_patterns) ||
        !parse_string_array(permissions, "fsReadRoots", &policy->fs_read_roots) ||
        !parse_string_array(permissions, "storageNamespaces", &policy->storage_namespaces) ||
        !parse_string_array(permissions, "stdioStreams", &policy->stdio_streams)) {
        return false;
    }
    policy->storage_allowed = !policy->storage_namespaces.empty();
    policy->stdio_allowed = !policy->stdio_streams.empty();
    std::vector<std::string> targets;
    if (!parse_string_array(permissions, "fetchTargets", &targets)) {
        return false;
    }
    for (const std::string& target : targets) {
        FetchTarget parsed;
        const bool ok = is_v2 ? parse_outbound_target_text(target, &parsed)
                              : parse_fetch_target_text(target, &parsed);
        if (!ok) {
            return false;
        }
        policy->fetch_targets.push_back(std::move(parsed));
    }
    return true;
}

bool parse_isolation(json_t* root, std::string* mode, std::vector<std::string>* required,
                     std::string* cgroup_root) {
    json_t* isolation = json_object_get(root, "isolation");
    if (isolation == nullptr) {
        return true;
    }
    if (!json_is_object(isolation)) {
        return false;
    }
    *mode = json_string_field(isolation, "mode");
    if (!mode->empty() && *mode != "strict") {
        return false; // semantic gate, reported by the caller
    }
    if (!parse_string_array(isolation, "required", required)) {
        return false;
    }
    *cgroup_root = json_string_field(isolation, "cgroupRoot");
    return true;
}

bool parse_trusted_bytecode_keys(json_t* root, std::vector<TrustedKeyDescriptor>* out) {
    json_t* keys = json_object_get(root, "trustedBytecodeKeys");
    if (keys == nullptr) {
        return true;
    }
    if (!json_is_object(keys)) {
        return false;
    }
    const char* key = nullptr;
    json_t* value = nullptr;
    json_object_foreach(keys, key, value) {
        if (!json_is_string(value)) {
            return false;
        }
        TrustedKeyDescriptor descriptor;
        descriptor.key_id = key;
        descriptor.key_path = json_string_value(value);
        out->push_back(std::move(descriptor));
    }
    return true;
}

bool parse_worker_tier(json_t* tier, WorkerTierConfig* out, bool is_v2) {
    json_t* worker = json_object_get(tier, "worker");
    if (worker == nullptr) {
        return true;
    }
    if (!json_is_object(worker)) {
        return false;
    }
    json_t* js_heap = json_object_get(worker, "jsHeap");
    if (json_is_string(js_heap)) {
        std::uint64_t ignored = 0;
        if (!parse_size_bytes_text(json_string_value(js_heap), &ignored)) {
            return false;
        }
        out->js_heap = json_string_value(js_heap);
    }
    if (!is_v2) {
        json_t* address = json_object_get(worker, "processAddressSpace");
        if (json_is_string(address)) {
            std::uint64_t ignored = 0;
            if (!parse_size_bytes_text(json_string_value(address), &ignored)) {
                return false;
            }
            out->process_address_space = json_string_value(address);
        }
    }
    json_t* memory = json_object_get(worker, "memoryMax");
    if (json_is_string(memory)) {
        std::uint64_t ignored = 0;
        if (!parse_size_bytes_text(json_string_value(memory), &ignored)) {
            return false;
        }
        out->memory_max = json_string_value(memory);
    }
    if (!parse_uint_field(worker, "fileDescriptors", &out->file_descriptors)) {
        return false;
    }
    if (!is_v2 && !parse_uint_field(worker, "pidsMax", &out->pids_max)) {
        return false;
    }
    return true;
}

bool parse_request_tier(json_t* tier, RequestTierConfig* out, bool is_v2) {
    json_t* request = json_object_get(tier, "request");
    if (request == nullptr) {
        return true;
    }
    if (!json_is_object(request)) {
        return false;
    }
    json_t* timeout = json_object_get(request, "timeout");
    if (json_is_string(timeout)) {
        std::uint64_t ignored = 0;
        if (!parse_duration_ms_text(json_string_value(timeout), &ignored) || ignored == 0 ||
            ignored > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            return false;
        }
        out->timeout = json_string_value(timeout);
    }
    if (!parse_uint_field(request, is_v2 ? "maxInflight" : "maxInflightPerWorker",
                          &out->max_inflight_per_worker)) {
        return false;
    }
    if (is_v2) {
        return true;
    }
    return parse_uint_field(request, "maxStreamingInflightPerWorker",
                            &out->max_streaming_inflight_per_worker) &&
           parse_uint_field(request, "streamIdleTimeoutMs", &out->stream_idle_timeout_ms) &&
           parse_uint_field(request, "writeTimeoutMs", &out->write_timeout_ms);
}

bool parse_pool_tier(json_t* tier, PoolTierConfig* out, bool is_v2) {
    json_t* pool = json_object_get(tier, "pool");
    if (pool == nullptr) {
        return true;
    }
    if (!json_is_object(pool)) {
        return false;
    }
    if (is_v2) {
        json_t* workers = json_object_get(pool, "workers");
        json_t* min_ready = json_object_get(pool, "minReady");
        json_t* max_workers = json_object_get(pool, "maxWorkers");
        json_t* idle_timeout = json_object_get(pool, "idleTimeout");
        if (workers != nullptr &&
            (min_ready != nullptr || max_workers != nullptr || idle_timeout != nullptr)) {
            return false;
        }
        if (json_is_integer(workers)) {
            const json_int_t value = json_integer_value(workers);
            if (value <= 0 || value > static_cast<json_int_t>(kMaxStaticPoolWorkers)) {
                return false;
            }
            out->workers = static_cast<std::uint32_t>(value);
        }
        if (json_is_integer(min_ready)) {
            const json_int_t value = json_integer_value(min_ready);
            if (value < 0 || value > static_cast<json_int_t>(kMaxStaticPoolWorkers)) {
                return false;
            }
            out->has_min_ready = true;
            out->min_ready = static_cast<std::uint32_t>(value);
        }
        if (json_is_integer(max_workers)) {
            const json_int_t value = json_integer_value(max_workers);
            if (value <= 0 || value > static_cast<json_int_t>(kMaxStaticPoolWorkers)) {
                return false;
            }
            out->has_max_workers = true;
            out->max_workers = static_cast<std::uint32_t>(value);
        }
        if (out->has_min_ready && out->has_max_workers && out->min_ready > out->max_workers) {
            return false;
        }
        if (!parse_duration_text_field(pool, "idleTimeout", &out->idle_timeout_ms)) {
            return false;
        }
    }
    json_t* queue_requests = json_object_get(pool, "queueRequests");
    if (queue_requests != nullptr) {
        out->has_queue_requests = true;
    }
    if (!parse_uint_field(pool, "queueRequests", &out->queue_requests) ||
        !parse_duration_text_field(pool, "queueTimeout", &out->queue_timeout_ms)) {
        return false;
    }
    if (!is_v2 && !parse_size_text_field(pool, "queueHeaderBytes", &out->queue_header_bytes)) {
        return false;
    }
    return true;
}

bool parse_tier(json_t* tier, TierConfig* out, bool is_v2) {
    if (!json_is_object(tier)) {
        return false;
    }
    return parse_worker_tier(tier, &out->worker, is_v2) &&
           parse_request_tier(tier, &out->request, is_v2) &&
           parse_pool_tier(tier, &out->pool, is_v2);
}

bool parse_channels(json_t* root, ChannelsConfig* out) {
    json_t* channels = json_object_get(root, "channels");
    if (channels == nullptr) {
        return true;
    }
    if (!json_is_object(channels)) {
        return false;
    }
    out->configured = true;
    // read every field; zero keeps the deployment default
    struct Field {
        const char* name;
        std::uint64_t* target;
    };
    std::uint64_t max_channels = 0;
    std::uint64_t close_ms = 0;
    std::uint64_t dial_ms = 0;
    std::uint64_t idle_ms = 0;
    std::uint64_t queue_bytes = 0;
    Field fields[] = {
        {"maxChannels", &max_channels},
        {"maxMessageBytes", &out->max_message_bytes},
        {"reassemblyBudgetBytes", &out->reassembly_budget_bytes},
        {"sendBudgetBytes", &out->send_budget_bytes},
        {"closeTimeoutMs", &close_ms},
        {"dialTimeoutMs", &dial_ms},
        {"idleTimeoutMs", &idle_ms},
        {"perChannelQueueBytes", &queue_bytes},
    };
    for (Field& field : fields) {
        json_t* value = json_object_get(channels, field.name);
        if (json_is_integer(value)) {
            const json_int_t parsed = json_integer_value(value);
            // json_int_t is a signed 64-bit type: a non-negative
            // value always fits the uint64 target
            if (parsed < 0) {
                return false;
            }
            *field.target = static_cast<std::uint64_t>(parsed);
        }
    }
    if (max_channels > std::numeric_limits<std::uint32_t>::max() ||
        close_ms > std::numeric_limits<std::uint32_t>::max() ||
        dial_ms > std::numeric_limits<std::uint32_t>::max() ||
        idle_ms > std::numeric_limits<std::uint32_t>::max() ||
        queue_bytes > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    out->max_channels = static_cast<std::uint32_t>(max_channels);
    out->close_timeout_ms = static_cast<std::uint32_t>(close_ms);
    out->dial_timeout_ms = static_cast<std::uint32_t>(dial_ms);
    out->idle_timeout_ms = static_cast<std::uint32_t>(idle_ms);
    out->per_channel_queue_bytes = static_cast<std::uint32_t>(queue_bytes);
    // The frozen A2 bounds are the authority: defaults filled in for
    // unset fields, then the whole set must validate.
    capsid::protocol::ChannelLimits limits;
    limits.max_channels = out->max_channels != 0 ? out->max_channels : 128;
    limits.max_message_bytes = out->max_message_bytes != 0 ? out->max_message_bytes : 1024u * 1024u;
    limits.reassembly_budget_bytes =
        out->reassembly_budget_bytes != 0 ? out->reassembly_budget_bytes : 16u * 1024u * 1024u;
    limits.send_budget_bytes =
        out->send_budget_bytes != 0 ? out->send_budget_bytes : 16u * 1024u * 1024u;
    limits.close_timeout_ms = out->close_timeout_ms != 0 ? out->close_timeout_ms : 5000;
    limits.dial_timeout_ms = out->dial_timeout_ms != 0 ? out->dial_timeout_ms : 10000;
    limits.idle_timeout_ms = out->idle_timeout_ms;
    limits.per_channel_queue_bytes =
        out->per_channel_queue_bytes != 0 ? out->per_channel_queue_bytes : 4u * 1024u * 1024u;
    if (!capsid::protocol::validate_channel_limits(limits)) {
        return false;
    }
    return true;
}

bool parse_capacity(json_t* root, CapacityConfig* out) {
    json_t* capacity = json_object_get(root, "capacity");
    if (capacity == nullptr) {
        return true;
    }
    if (!json_is_object(capacity)) {
        return false;
    }
    // workersTotal is schema-positive; the Host limit gate mirrors the
    // coordinator's int budget.
    json_t* workers = json_object_get(capacity, "workersTotal");
    if (json_is_integer(workers)) {
        const json_int_t value = json_integer_value(workers);
        if (value <= 0 || value > static_cast<json_int_t>(std::numeric_limits<int>::max())) {
            return false;
        }
        out->workers_total = static_cast<std::uint64_t>(value);
    }
    // §9.4: activationSurgeWorkers is non-negative and defaults to 0
    // (absent = no zero-downtime replaces, see the schema member).
    if (!parse_uint_field(capacity, "channelsTotal", &out->channels.channels) ||
        !parse_uint_field(capacity, "pendingDialsTotal", &out->channels.pending_dials) ||
        !parse_size_text_field(capacity, "channelQueueBytesTotal", &out->channels.queue_bytes) ||
        !out->channels.channels || !out->channels.pending_dials || !out->channels.queue_bytes)
        return false;
    return parse_uint_field(capacity, "activationSurgeWorkers", &out->activation_surge_workers) &&
           parse_uint_field(capacity, "startupsConcurrent", &out->startups_concurrent) &&
           parse_uint_field(capacity, "queuedRequestsTotal", &out->queued_requests_total) &&
           parse_size_text_field(capacity, "queuedHeaderBytesTotal",
                                 &out->queued_header_bytes_total) &&
           parse_size_text_field(capacity, "workerMemoryCommitTotal",
                                 &out->worker_memory_commit_total);
}

bool parse_capacity_v2(json_t* root, CapacityConfig* out, ChannelsConfig* channels_out) {
    json_t* capacity = json_object_get(root, "capacity");
    std::uint64_t workers_total = 1;
    std::uint64_t channels_total = 1024;
    std::uint64_t max_msg = 1024ULL * 1024ULL;
    std::uint64_t idle_ms = 60000;
    if (capacity != nullptr) {
        if (!json_is_object(capacity)) {
            return false;
        }
        json_t* workers = json_object_get(capacity, "workersTotal");
        if (json_is_integer(workers)) {
            const json_int_t value = json_integer_value(workers);
            if (value <= 0 || value > static_cast<json_int_t>(std::numeric_limits<int>::max())) {
                return false;
            }
            workers_total = static_cast<std::uint64_t>(value);
        }
        if (!parse_uint_field(capacity, "channelsTotal", &channels_total) || channels_total == 0 ||
            channels_total > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        if (!parse_size_text_field(capacity, "maxChannelMessageSize", &max_msg) ||
            max_msg < capsid::protocol::kMinMessageBytes ||
            max_msg > capsid::protocol::kMaxMessageBytes) {
            return false;
        }
        if (!parse_duration_text_field(capacity, "channelIdleTimeout", &idle_ms) || idle_ms == 0 ||
            idle_ms > 86400000ULL) {
            return false;
        }
        json_t* memory = json_object_get(capacity, "memory");
        if (memory != nullptr) {
            out->memory.configured = true;
            if (json_is_string(memory)) {
                if (!parse_size_bytes_text(json_string_value(memory), &out->memory.limit_bytes) ||
                    out->memory.limit_bytes == 0) {
                    return false;
                }
            } else if (json_is_object(memory)) {
                json_t* limit = json_object_get(memory, "limit");
                if (!json_is_string(limit) ||
                    !parse_size_bytes_text(json_string_value(limit), &out->memory.limit_bytes) ||
                    out->memory.limit_bytes == 0) {
                    return false;
                }
                json_t* high_node = json_object_get(memory, "highWatermarkPercent");
                if (high_node != nullptr) {
                    if (!json_is_integer(high_node)) {
                        return false;
                    }
                    const json_int_t v = json_integer_value(high_node);
                    if (v < 1 || v > 100) {
                        return false;
                    }
                    out->memory.high_watermark_percent = static_cast<std::uint32_t>(v);
                }
                json_t* low_node = json_object_get(memory, "lowWatermarkPercent");
                if (low_node != nullptr) {
                    if (!json_is_integer(low_node)) {
                        return false;
                    }
                    const json_int_t v = json_integer_value(low_node);
                    if (v < 1 || v > 100) {
                        return false;
                    }
                    out->memory.low_watermark_percent = static_cast<std::uint32_t>(v);
                }
                if (out->memory.low_watermark_percent >= out->memory.high_watermark_percent) {
                    return false;
                }
                json_t* residency = json_object_get(memory, "minResidency");
                if (residency != nullptr) {
                    std::uint64_t ms = 0;
                    if (!json_is_string(residency) ||
                        !units::parse_duration_ms(json_string_value(residency), &ms,
                                                  /*allow_hours=*/false) ||
                        ms > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                        return false;
                    }
                    out->memory.min_residency_ms = ms;
                }
                json_t* interval = json_object_get(memory, "checkInterval");
                if (interval != nullptr) {
                    std::uint64_t ms = 0;
                    if (!json_is_string(interval) ||
                        !units::parse_duration_ms(json_string_value(interval), &ms,
                                                  /*allow_hours=*/false) ||
                        ms > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                        return false;
                    }
                    out->memory.check_interval_ms = ms;
                }
            } else {
                return false;
            }
        }
    }
    out->workers_total = workers_total;
    out->activation_surge_workers = std::max<std::uint64_t>(2, workers_total / 4);
    out->max_channel_message_size = max_msg;
    out->channel_idle_timeout_ms = idle_ms;
    out->channels.channels = channels_total;
    out->channels.pending_dials = std::clamp<std::uint64_t>(channels_total / 8, 16ULL, 1024ULL);
    out->channels.queue_bytes = std::max<std::uint64_t>(16ULL * 1024ULL * 1024ULL, max_msg * 16ULL);

    channels_out->configured = true;
    channels_out->max_message_bytes = max_msg;
    channels_out->reassembly_budget_bytes =
        std::clamp<std::uint64_t>(std::max<std::uint64_t>(4ULL * 1024ULL * 1024ULL, max_msg * 4ULL),
                                  1024ULL * 1024ULL, 256ULL * 1024ULL * 1024ULL);
    channels_out->send_budget_bytes =
        std::clamp<std::uint64_t>(std::max<std::uint64_t>(4ULL * 1024ULL * 1024ULL, max_msg * 4ULL),
                                  1024ULL * 1024ULL, 256ULL * 1024ULL * 1024ULL);
    channels_out->per_channel_queue_bytes = static_cast<std::uint32_t>(
        std::clamp<std::uint64_t>(max_msg * 2ULL, capsid::protocol::kInitialChannelCredit,
                                  capsid::protocol::kMaxBufferedBytes));
    channels_out->idle_timeout_ms = static_cast<std::uint32_t>(idle_ms);
    channels_out->dial_timeout_ms = 10000;
    channels_out->close_timeout_ms = 5000;
    channels_out->max_channels = 128;

    capsid::protocol::ChannelLimits limits;
    limits.max_channels = channels_out->max_channels;
    limits.max_message_bytes = channels_out->max_message_bytes;
    limits.reassembly_budget_bytes = channels_out->reassembly_budget_bytes;
    limits.send_budget_bytes = channels_out->send_budget_bytes;
    limits.close_timeout_ms = channels_out->close_timeout_ms;
    limits.dial_timeout_ms = channels_out->dial_timeout_ms;
    limits.idle_timeout_ms = channels_out->idle_timeout_ms;
    limits.per_channel_queue_bytes = channels_out->per_channel_queue_bytes;
    return capsid::protocol::validate_channel_limits(limits);
}

bool parse_tenant_quota(json_t* object, TenantQuotaConfig* out) {
    if (!json_is_object(object)) {
        return false;
    }
    json_t* max_workers = json_object_get(object, "maxWorkers");
    if (max_workers != nullptr) {
        if (!json_is_integer(max_workers)) {
            return false;
        }
        const json_int_t value = json_integer_value(max_workers);
        if (value <= 0 || value > static_cast<json_int_t>(kMaxStaticPoolWorkers)) {
            return false;
        }
        out->max_workers = static_cast<std::uint32_t>(value);
    }
    json_t* min_soft = json_object_get(object, "minSoftMemory");
    if (min_soft != nullptr) {
        if (!json_is_string(min_soft) ||
            !parse_size_bytes_text(json_string_value(min_soft), &out->min_soft_memory_bytes) ||
            out->min_soft_memory_bytes == 0) {
            return false;
        }
    }
    json_t* hard_memory = json_object_get(object, "hardMemory");
    if (hard_memory != nullptr) {
        if (!json_is_string(hard_memory) ||
            !parse_size_bytes_text(json_string_value(hard_memory), &out->hard_memory_bytes) ||
            out->hard_memory_bytes == 0) {
            return false;
        }
    }
    if (out->hard_memory_bytes != 0 && out->min_soft_memory_bytes > out->hard_memory_bytes) {
        return false;
    }
    return true;
}

bool parse_tenancy_v2(json_t* root, TenancyConfig* out) {
    json_t* tenancy = json_object_get(root, "tenancy");
    if (tenancy == nullptr) {
        return true;
    }
    if (!json_is_object(tenancy)) {
        return false;
    }
    out->configured = true;
    json_t* default_quota = json_object_get(tenancy, "defaultQuota");
    if (default_quota != nullptr && !parse_tenant_quota(default_quota, &out->default_quota)) {
        return false;
    }
    json_t* overrides = json_object_get(tenancy, "overrides");
    if (overrides != nullptr) {
        if (!json_is_object(overrides)) {
            return false;
        }
        const char* key = nullptr;
        json_t* value = nullptr;
        json_object_foreach(overrides, key, value) {
            if (!valid_tenant_id(key)) {
                return false;
            }
            TenantQuotaConfig quota;
            if (!parse_tenant_quota(value, &quota)) {
                return false;
            }
            const std::uint64_t effective_min_soft = quota.min_soft_memory_bytes != 0
                                                         ? quota.min_soft_memory_bytes
                                                         : out->default_quota.min_soft_memory_bytes;
            const std::uint64_t effective_hard = quota.hard_memory_bytes != 0
                                                     ? quota.hard_memory_bytes
                                                     : out->default_quota.hard_memory_bytes;
            if (effective_hard != 0 && effective_min_soft > effective_hard) {
                return false;
            }
            out->overrides[key] = quota;
        }
    }
    return true;
}

bool parse_recovery(json_t* root, RecoveryConfig* out, bool is_v2) {
    json_t* recovery = json_object_get(root, "recovery");
    if (is_v2) {
        out->active_health_interval_ms = 30000;
        out->active_health_failures = 3;
    }
    if (recovery == nullptr) {
        return true;
    }
    if (!json_is_object(recovery)) {
        return false;
    }
    json_t* crash_budget = json_object_get(recovery, "crashBudget");
    if (crash_budget != nullptr) {
        if (!json_is_object(crash_budget) ||
            !parse_uint_field(crash_budget, "maxEvents", &out->crash_budget.max_events) ||
            !parse_duration_text_field(crash_budget, "window", &out->crash_budget.window_ms)) {
            return false;
        }
    }
    if (is_v2) {
        json_t* health = json_object_get(recovery, "healthCheck");
        if (health != nullptr) {
            if (!json_is_object(health) ||
                !parse_duration_text_field(health, "interval", &out->active_health_interval_ms) ||
                !parse_uint_field(health, "failures", &out->active_health_failures) ||
                out->active_health_interval_ms == 0 || out->active_health_failures == 0) {
                return false;
            }
        }
        return true;
    }
    json_t* restart_backoff = json_object_get(recovery, "restartBackoff");
    if (restart_backoff != nullptr) {
        if (!json_is_object(restart_backoff) ||
            !parse_duration_text_field(restart_backoff, "initial",
                                       &out->restart_backoff.initial_ms) ||
            !parse_duration_text_field(restart_backoff, "maximum",
                                       &out->restart_backoff.maximum_ms)) {
            return false;
        }
        json_t* jitter = json_object_get(restart_backoff, "jitter");
        if (json_is_string(jitter)) {
            std::uint64_t ignored = 0;
            if (!parse_jitter_basis_points_text(json_string_value(jitter), &ignored)) {
                return false;
            }
            out->restart_backoff.jitter = json_string_value(jitter);
        }
    }
    return parse_uint_field(recovery, "replacementsConcurrentPerApp",
                            &out->replacements_concurrent_per_app) &&
           parse_duration_text_field(recovery, "activeHealthInterval",
                                     &out->active_health_interval_ms) &&
           parse_uint_field(recovery, "activeHealthFailures", &out->active_health_failures);
}

} // namespace

bool parse_host_config(std::string_view json, ParsedHostConfig* out, std::string* error) {
    json_error_t parse_error;
    json_t* root = json_loadb(json.data(), json.size(), JSON_REJECT_DUPLICATES, &parse_error);
    if (root == nullptr || !json_is_object(root)) {
        if (error != nullptr) {
            *error = "invalid host.json";
        }
        if (root != nullptr) {
            json_decref(root);
        }
        return false;
    }

    auto reject = [&](const char* message) {
        if (error != nullptr) {
            *error = message;
        }
        json_decref(root);
        return false;
    };

    const bool is_v2 = json_string_field(root, "apiVersion") == "capsid/host-v2";
    ParsedHostConfig config;
    config.is_v2 = is_v2;
    config.policy.is_v2 = is_v2;
    if (is_v2) {
        json_t* paths = json_object_get(root, "paths");
        if (!json_is_object(paths)) {
            return reject("host.json is missing paths");
        }
        config.applications_root = json_string_field(paths, "applications");
        config.state_root = json_string_field(paths, "state");
        config.secret_root_template = json_string_field(paths, "secrets");
        config.bindings_root = json_string_field(paths, "bindings");
    } else {
        config.applications_root = json_string_field(root, "applicationsRoot");
        config.state_root = json_string_field(root, "stateRoot");
        config.secret_root_template = json_string_field(root, "secretRootTemplate");
        config.bindings_root = json_string_field(root, "bindingsRoot");
    }
    json_t* admin = json_object_get(root, "admin");
    if (json_is_object(admin)) {
        config.admin_unix_path = json_string_field(admin, "unix");
        const std::string mode_text = json_string_field(admin, "mode");
        if (is_v2) {
            if (json_object_get(admin, "mode") != nullptr) {
                return reject("admin.mode is unsupported in host-v2");
            }
            config.admin_mode = 0600;
        } else if (!mode_text.empty()) {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(mode_text.c_str(), &end, 8);
            if (end == nullptr || *end != '\0' || parsed > 07777 ||
                static_cast<unsigned>(parsed) != 0600) {
                return reject("admin.mode must be 0600");
            }
            config.admin_mode = static_cast<unsigned>(parsed);
        }
    }

    if (!parse_listeners(root, &config.listeners, is_v2)) {
        return reject("invalid host.json listeners");
    }
    if (!parse_permissions(root, &config.policy, is_v2)) {
        return reject("invalid host.json permissions");
    }
    if (!parse_isolation(root, &config.isolation_mode, &config.isolation_required,
                         &config.isolation_cgroup_root)) {
        return reject("isolation.mode must be strict");
    }
    if (is_v2 && config.isolation_mode.empty()) {
        config.isolation_mode = "strict";
    }
    config.policy.isolation_required = config.isolation_required;
    config.policy.isolation_cgroup_root = config.isolation_cgroup_root;
    if (!parse_trusted_bytecode_keys(root, &config.trusted_keys)) {
        return reject("invalid host.json trustedBytecodeKeys");
    }

    json_t* defaults = json_object_get(root, "defaults");
    if (defaults != nullptr && !parse_tier(defaults, &config.defaults, is_v2)) {
        return reject("invalid host.json defaults");
    }
    if (!config.defaults.worker.js_heap.empty() &&
        !parse_size_bytes_text(config.defaults.worker.js_heap,
                               &config.policy.default_js_heap_bytes)) {
        return reject("invalid defaults.worker.jsHeap");
    }
    if (!config.defaults.worker.memory_max.empty() &&
        !parse_size_bytes_text(config.defaults.worker.memory_max,
                               &config.policy.default_worker_memory_bytes)) {
        return reject("invalid defaults.worker.memoryMax");
    }
    config.policy.default_file_descriptors = config.defaults.worker.file_descriptors;
    if (!config.defaults.request.timeout.empty()) {
        if (!parse_duration_ms_text(config.defaults.request.timeout,
                                    &config.policy.default_request_timeout_ms)) {
            return reject("invalid defaults.request.timeout");
        }
    }
    config.policy.default_requests_per_worker = config.defaults.request.max_inflight_per_worker;
    config.policy.default_max_streaming_inflight_per_worker =
        config.defaults.request.max_streaming_inflight_per_worker;
    config.policy.default_stream_idle_timeout_ms = config.defaults.request.stream_idle_timeout_ms;
    config.policy.default_write_timeout_ms = config.defaults.request.write_timeout_ms;
    config.policy.default_workers = config.defaults.pool.workers;
    config.policy.has_default_min_ready = config.defaults.pool.has_min_ready;
    config.policy.default_min_ready = config.defaults.pool.min_ready;
    config.policy.default_max_workers = config.defaults.pool.max_workers;
    config.policy.default_idle_timeout_ms = config.defaults.pool.idle_timeout_ms;
    config.policy.has_default_queue_requests = config.defaults.pool.has_queue_requests;
    config.policy.default_queue_requests = config.defaults.pool.queue_requests;
    config.policy.default_queue_header_bytes = config.defaults.pool.queue_header_bytes;
    config.policy.default_queue_timeout_ms = config.defaults.pool.queue_timeout_ms;

    json_t* maximums = json_object_get(root, "maximums");
    if (maximums != nullptr) {
        if (!parse_tier(maximums, &config.maximums, is_v2)) {
            return reject("invalid host.json maximums");
        }
        config.policy.max_requests_per_worker = config.maximums.request.max_inflight_per_worker;
        if (!config.maximums.request.timeout.empty()) {
            if (!parse_duration_ms_text(config.maximums.request.timeout,
                                        &config.policy.max_request_timeout_ms)) {
                return reject("invalid maximums.request.timeout");
            }
        }
        config.policy.max_streaming_inflight_per_worker =
            config.maximums.request.max_streaming_inflight_per_worker;
        config.policy.max_stream_idle_timeout_ms = config.maximums.request.stream_idle_timeout_ms;
        config.policy.max_write_timeout_ms = config.maximums.request.write_timeout_ms;
        if (!config.maximums.worker.js_heap.empty()) {
            if (!parse_size_bytes_text(config.maximums.worker.js_heap,
                                       &config.policy.max_js_heap_bytes)) {
                return reject("invalid maximums.worker.jsHeap");
            }
        }
        if (!config.maximums.worker.memory_max.empty()) {
            std::uint64_t bytes = 0;
            if (!parse_size_bytes_text(config.maximums.worker.memory_max, &bytes)) {
                return reject("invalid maximums.worker.memoryMax");
            }
            config.policy.max_worker_memory_bytes = bytes;
        }
        config.policy.max_file_descriptors = config.maximums.worker.file_descriptors;
        config.policy.max_pool_workers = config.maximums.pool.has_max_workers
                                             ? config.maximums.pool.max_workers
                                             : config.maximums.pool.workers;
        if (config.maximums.pool.has_min_ready) {
            config.policy.has_max_min_ready = true;
            config.policy.max_min_ready = config.maximums.pool.min_ready;
        } else if (!config.maximums.pool.has_max_workers && config.maximums.pool.workers != 0) {
            config.policy.has_max_min_ready = true;
            config.policy.max_min_ready = config.maximums.pool.workers;
        }
        config.policy.max_idle_timeout_ms = config.maximums.pool.idle_timeout_ms;
        config.policy.max_queue_requests = config.maximums.pool.queue_requests;
        config.policy.max_queue_header_bytes = config.maximums.pool.queue_header_bytes;
        config.policy.max_queue_timeout_ms = config.maximums.pool.queue_timeout_ms;
    }
    if (config.policy.has_max_min_ready && config.policy.max_pool_workers != 0 &&
        config.policy.max_min_ready > config.policy.max_pool_workers) {
        return reject("maximums.pool minReady exceeds maxWorkers");
    }
    const std::uint32_t default_pool_max = config.policy.default_max_workers != 0
                                               ? config.policy.default_max_workers
                                               : config.policy.default_workers;
    if ((config.policy.max_request_timeout_ms != 0 &&
         config.policy.default_request_timeout_ms > config.policy.max_request_timeout_ms) ||
        (config.policy.max_worker_memory_bytes != 0 &&
         config.policy.default_worker_memory_bytes > config.policy.max_worker_memory_bytes) ||
        (config.policy.max_js_heap_bytes != 0 &&
         config.policy.default_js_heap_bytes > config.policy.max_js_heap_bytes) ||
        (config.policy.max_file_descriptors != 0 &&
         config.policy.default_file_descriptors > config.policy.max_file_descriptors) ||
        (config.policy.max_requests_per_worker != 0 &&
         config.policy.default_requests_per_worker > config.policy.max_requests_per_worker) ||
        (config.policy.max_pool_workers != 0 &&
         default_pool_max > config.policy.max_pool_workers) ||
        (config.policy.has_default_min_ready && config.policy.max_pool_workers != 0 &&
         config.policy.default_min_ready > config.policy.max_pool_workers) ||
        (config.policy.has_max_min_ready && config.policy.has_default_min_ready &&
         config.policy.default_min_ready > config.policy.max_min_ready) ||
        (config.policy.has_max_min_ready && !config.policy.has_default_min_ready &&
         config.policy.default_workers != 0 &&
         config.policy.default_workers > config.policy.max_min_ready) ||
        (config.policy.max_idle_timeout_ms != 0 &&
         config.policy.default_idle_timeout_ms > config.policy.max_idle_timeout_ms) ||
        (config.policy.max_queue_requests != 0 &&
         config.policy.default_queue_requests > config.policy.max_queue_requests) ||
        (config.policy.max_queue_header_bytes != 0 &&
         config.policy.default_queue_header_bytes > config.policy.max_queue_header_bytes) ||
        (config.policy.max_queue_timeout_ms != 0 &&
         config.policy.default_queue_timeout_ms > config.policy.max_queue_timeout_ms) ||
        (config.policy.max_streaming_inflight_per_worker != 0 &&
         config.policy.default_max_streaming_inflight_per_worker >
             config.policy.max_streaming_inflight_per_worker) ||
        (config.policy.max_stream_idle_timeout_ms != 0 &&
         config.policy.default_stream_idle_timeout_ms > config.policy.max_stream_idle_timeout_ms) ||
        (config.policy.max_write_timeout_ms != 0 &&
         config.policy.default_write_timeout_ms > config.policy.max_write_timeout_ms)) {
        return reject("host.json default value exceeds the Host maximum");
    }
    if (is_v2) {
        if (!parse_capacity_v2(root, &config.capacity, &config.channels)) {
            return reject("invalid host.json capacity");
        }
        if (!parse_tenancy_v2(root, &config.tenancy)) {
            return reject("invalid host.json tenancy");
        }
        config.channels.max_channels = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(config.listeners[0].limits.channels, 1024ULL));
        for (ListenerConfig& listener : config.listeners) {
            listener.limits.channel_queue_bytes = config.capacity.channels.queue_bytes;
        }
    } else {
        if (!parse_capacity(root, &config.capacity)) {
            return reject("invalid host.json capacity");
        }
        if (!parse_channels(root, &config.channels)) {
            return reject("invalid host.json channels: limits outside the "
                          "frozen A2 bounds");
        }
    }
    if (config.capacity.workers_total > kMaxStaticPoolWorkers) {
        return reject("capacity.workersTotal exceeds the static pool worker limit");
    }
    config.policy.max_workers = static_cast<std::uint32_t>(config.capacity.workers_total);
    if (!parse_recovery(root, &config.recovery, is_v2)) {
        return reject("invalid host.json recovery");
    }

    if (config.applications_root.empty() || config.state_root.empty() ||
        config.admin_unix_path.empty() ||
        config.secret_root_template.find("{application}") == std::string::npos) {
        return reject("host.json is missing a required root or admin path");
    }

    *out = std::move(config);
    json_decref(root);
    return true;
}

ResolvedRecoveryPolicy resolve_recovery_policy(const RecoveryConfig& config) {
    // Host defaults: a zero (absent) field maps to the value the strict
    // GenerationPool::create_adopted validation expects for a running Host.
    // The jitter text is a percent or decimal-basis-points grammar; the
    // schema validated it, so an unparseable value here is a startup error.
    ResolvedRecoveryPolicy out;
    const std::uint64_t max_events =
        config.crash_budget.max_events > 0 ? config.crash_budget.max_events : 5;
    if (max_events > kMaxTrackedInstabilityEvents) {
        out.error = "crash budget exceeds the tracked-event window";
        return out;
    }
    out.policy.max_events = static_cast<std::uint32_t>(max_events);
    out.policy.window_ms =
        config.crash_budget.window_ms > 0 ? config.crash_budget.window_ms : 60000;
    out.policy.backoff_initial_ms =
        config.restart_backoff.initial_ms > 0 ? config.restart_backoff.initial_ms : 100;
    out.policy.backoff_maximum_ms =
        config.restart_backoff.maximum_ms > 0 ? config.restart_backoff.maximum_ms : 10000;
    if (out.policy.backoff_maximum_ms < out.policy.backoff_initial_ms) {
        out.error = "restart backoff maximum precedes its initial delay";
        return out;
    }
    // "20%" = 2000 basis points, "1000" = 1000 basis points (10%);
    // absent/empty text takes the Host default. The schema validated the
    // grammar, so an unparseable value here is a startup error.
    const std::string& jitter_text = config.restart_backoff.jitter;
    if (jitter_text.empty()) {
        out.policy.jitter_basis_points = 1000;
    } else {
        std::uint64_t parsed = 0;
        if (!parse_jitter_basis_points_text(jitter_text, &parsed) ||
            parsed > std::numeric_limits<std::uint32_t>::max()) {
            out.error = "invalid restart backoff jitter";
            return out;
        }
        out.policy.jitter_basis_points = static_cast<std::uint32_t>(parsed);
    }
    // host.json has no stability-window field (health interval is the
    // PROBE cadence, not the backoff reset); the Host default always
    // applies.
    out.policy.stable_reset_ms = 60000;
    const std::uint64_t replacements =
        config.replacements_concurrent_per_app > 0 ? config.replacements_concurrent_per_app : 1;
    if (replacements > std::numeric_limits<std::uint32_t>::max()) {
        out.error = "replacementsConcurrentPerApp exceeds uint32 range";
        return out;
    }
    out.policy.replacements_concurrent_per_app = static_cast<std::uint32_t>(replacements);
    out.ok = true;
    return out;
}

TenantQuotaConfig TenancyConfig::quota_for(const std::string& tenant) const {
    TenantQuotaConfig resolved = default_quota;
    const auto found = overrides.find(tenant);
    if (found != overrides.end()) {
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

} // namespace capsid::host
