// WP-05 PR-08: typed host.json model tests. The schema validation
// (test_host_config.cc) is the authority on shape; this suite pins the typed
// projection and the semantic gates that live in the parse:
//   - every schema-accepted field maps into the model (nothing silently
//     ignored);
//   - admin.mode is exactly 0600;
//   - isolation.mode is exactly "strict";
//   - capacity.workersTotal is positive and within the Host int limit;
//   - the frozen size/duration/jitter grammars fail closed.
// A document that passes validate_config_json must parse here; a document
// that passes here must be a faithful projection of the schema fields.

#include "host/binding_compile.h"
#include "host/config.h"
#include "host/host_config_model.h"
#include "host/listener_cors.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using capsid::host::ConfigDocument;
using capsid::host::ParsedHostConfig;
using capsid::host::parse_host_config;
using capsid::host::validate_config_json;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test-host-config-model: " << message << std::endl;
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

// The full feature surface, schema-valid, with every accepted field present.
constexpr std::string_view kFullDocument = R"json({
  "apiVersion": "capsid/host-v1",
  "applicationsRoot": "/srv/apps",
  "stateRoot": "/var/lib/capsid",
  "secretRootTemplate": "/var/lib/capsid/secrets/{application}",
  "admin": {"unix": "/run/capsid/admin.sock", "mode": "0600"},
  "listeners": [{
    "name": "public",
    "tcp": "0.0.0.0:8443",
    "publicScheme": "https",
    "publicAuthority": "apps.example.com",
    "routing": {"mode": "path", "suffix": "x-capsid-app"},
    "limits": {
      "connections": 128,
      "headerBytes": "64KiB",
      "headerTimeout": "5s",
      "bodyIdleTimeout": "30s",
      "streamIdleTimeout": "60s"
    },
    "cors": {
      "allowedOrigins": ["*"],
      "allowedMethods": ["get", "POST"],
      "allowedHeaders": ["Content-Type", "x-request-id"],
      "maxAge": "86400s"
    }
  }],
  "permissions": {
    "modules": ["http", "net"],
    "environmentNames": ["NODE_ENV", "PUBLIC_*"],
    "fsReadRoots": ["/usr/share/data"],
    "fetchTargets": ["api.internal:443,80", "upstream.example.com"],
    "storageNamespaces": ["assets"],
    "stdioStreams": ["stdout"]
  },
  "isolation": {"mode": "strict", "required": ["pid"], "cgroupRoot": "/sys/fs/cgroup"},
  "trustedBytecodeKeys": {
    "release-key": "/etc/capsid/keys/release.raw",
    "ci-key": "/etc/capsid/keys/ci.raw"
  },
  "defaults": {
    "worker": {"jsHeap": "64MiB", "processAddressSpace": "256MiB",
               "memoryMax": "128MiB", "fileDescriptors": 64, "pidsMax": 32},
    "request": {"timeout": "30s", "maxInflightPerWorker": 8,
                "maxStreamingInflightPerWorker": 2,
                "streamIdleTimeoutMs": 60000, "writeTimeoutMs": 15000},
    "pool": {"queueRequests": 64, "queueHeaderBytes": "64KiB",
             "queueTimeout": "5s"}
  },
  "maximums": {
    "worker": {"jsHeap": "256MiB", "processAddressSpace": "1GiB",
               "memoryMax": "512MiB", "fileDescriptors": 256, "pidsMax": 128},
    "request": {"timeout": "60s", "maxInflightPerWorker": 32,
                "maxStreamingInflightPerWorker": 4,
                "streamIdleTimeoutMs": 120000, "writeTimeoutMs": 60000},
    "pool": {"queueRequests": 256, "queueHeaderBytes": "256KiB",
             "queueTimeout": "10s"}
  },
  "capacity": {
    "workersTotal": 4,
    "activationSurgeWorkers": 2,
    "startupsConcurrent": 2,
    "queuedRequestsTotal": 128,
    "queuedHeaderBytesTotal": "128KiB",
    "workerMemoryCommitTotal": "2GiB"
  },
  "recovery": {
    "crashBudget": {"maxEvents": 3, "window": "60s"},
    "restartBackoff": {"initial": "250ms", "maximum": "30s", "jitter": "1000"},
    "replacementsConcurrentPerApp": 1,
    "activeHealthInterval": "15s",
    "activeHealthFailures": 2
  }
})json";

void require_valid_parse(std::string_view json, const char* label, ParsedHostConfig* out) {
    const auto validation = validate_config_json(ConfigDocument::kHost, json);
    require(validation.ok, std::string(label) + " is not schema-valid: '" + validation.error.path +
                               "': " + validation.error.message);
    std::string error;
    require(parse_host_config(json, out, &error),
            std::string(label) + " was rejected by the typed parse: " + error);
}

void require_parse_error(std::string_view json, const char* label) {
    // The document must be schema-valid: the gate under test lives in the
    // typed parse, not in validate_config_json.
    const auto validation = validate_config_json(ConfigDocument::kHost, json);
    require(validation.ok, std::string(label) + " is not schema-valid: '" + validation.error.path +
                               "': " + validation.error.message);
    ParsedHostConfig config;
    std::string error;
    require(!parse_host_config(json, &config, &error),
            std::string(label) + " was accepted by the typed parse");
    require(!error.empty(), std::string(label) + " produced no error text");
}

void test_full_document_maps_every_field() {
    ParsedHostConfig config;
    require_valid_parse(kFullDocument, "full document", &config);

    require(config.applications_root == "/srv/apps", "applicationsRoot did not map");
    require(config.state_root == "/var/lib/capsid", "stateRoot did not map");
    require(config.secret_root_template == "/var/lib/capsid/secrets/{application}",
            "secretRootTemplate did not map");
    require(config.admin_unix_path == "/run/capsid/admin.sock", "admin.unix did not map");
    require(config.admin_mode == 0600, "admin.mode did not map");

    require(config.listeners.size() == 1, "listeners did not map");
    const auto& listener = config.listeners[0];
    require(listener.name == "public", "listener.name did not map");
    require(listener.tcp == "0.0.0.0:8443", "listener.tcp did not map");
    require(listener.public_scheme == "https", "listener.publicScheme did not map");
    require(listener.public_authority == "apps.example.com",
            "listener.publicAuthority did not map");
    require(listener.routing.mode == "path", "routing.mode did not map");
    require(listener.routing.suffix == "x-capsid-app", "routing.suffix did not map");
    require(listener.limits.connections == 128, "limits.connections did not map");
    require(listener.limits.header_bytes == 64U * 1024U,
            "limits.headerBytes did not convert to bytes");
    require(listener.limits.header_timeout_ms == 5000,
            "limits.headerTimeout did not convert to ms");
    require(listener.limits.body_idle_timeout_ms == 30000,
            "limits.bodyIdleTimeout did not convert to ms");
    require(listener.cors.configured, "cors did not map");
    require(listener.cors.allowed_origins.size() == 1 && listener.cors.allowed_origins[0] == "*",
            "cors.allowedOrigins did not map");
    require(listener.cors.allowed_methods.size() == 2 &&
                listener.cors.allowed_methods[0] == "GET" &&
                listener.cors.allowed_methods[1] == "POST",
            "cors.allowedMethods did not normalize to uppercase");
    require(listener.cors.allowed_headers.size() == 2 &&
                listener.cors.allowed_headers[0] == "content-type" &&
                listener.cors.allowed_headers[1] == "x-request-id",
            "cors.allowedHeaders did not normalize to lowercase");
    require(listener.cors.max_age_ms == 86400000, "cors.maxAge did not convert to ms");
    require(listener.limits.stream_idle_timeout_ms == 60000,
            "limits.streamIdleTimeout did not convert to ms");

    require(config.policy.module_allowlist.size() == 2, "permissions.modules did not map");
    require(config.policy.env_patterns.size() == 2, "permissions.environmentNames did not map");
    require(config.policy.fs_read_roots.size() == 1, "permissions.fsReadRoots did not map");
    require(config.policy.fetch_targets.size() == 2, "permissions.fetchTargets did not map");
    require(config.policy.fetch_targets[0].host == "api.internal" &&
                config.policy.fetch_targets[0].ports.size() == 2,
            "fetch target ports did not map");
    require(config.policy.fetch_targets[1].ports.empty(), "any-port fetch target gained ports");
    require(config.policy.storage_allowed, "storage_allowed did not flip");
    require(config.policy.storage_namespaces.size() == 1,
            "permissions.storageNamespaces did not map");
    require(config.policy.stdio_allowed, "stdio_allowed did not flip");
    require(config.policy.stdio_streams.size() == 1, "permissions.stdioStreams did not map");

    require(config.isolation_mode == "strict", "isolation.mode did not map");
    require(config.isolation_required.size() == 1 && config.isolation_required[0] == "pid",
            "isolation.required did not map");
    require(config.isolation_cgroup_root == "/sys/fs/cgroup", "isolation.cgroupRoot did not map");

    require(config.trusted_keys.size() == 2, "trustedBytecodeKeys did not map");
    require(config.trusted_keys[0].key_id == "release-key" &&
                config.trusted_keys[0].key_path == "/etc/capsid/keys/release.raw",
            "trusted key descriptor did not map");
    require(config.trusted_keys[1].key_id == "ci-key" &&
                config.trusted_keys[1].key_path == "/etc/capsid/keys/ci.raw",
            "second trusted key descriptor did not map");

    require(config.defaults.worker.js_heap == "64MiB", "defaults.worker.jsHeap did not map");
    require(config.defaults.worker.process_address_space == "256MiB",
            "defaults.worker.processAddressSpace did not map");
    require(config.defaults.worker.memory_max == "128MiB", "defaults.worker.memoryMax did not map");
    require(config.defaults.worker.file_descriptors == 64,
            "defaults.worker.fileDescriptors did not map");
    require(config.defaults.worker.pids_max == 32, "defaults.worker.pidsMax did not map");
    require(config.defaults.request.timeout == "30s", "defaults.request.timeout did not map");
    require(config.policy.default_request_timeout_ms == 30000 &&
                config.policy.max_request_timeout_ms == 60000,
            "request timeout tiers did not reach the deployment policy");
    require(config.defaults.request.max_inflight_per_worker == 8,
            "defaults.request.maxInflightPerWorker did not map");
    require(config.defaults.request.max_streaming_inflight_per_worker == 2,
            "defaults.request.maxStreamingInflightPerWorker did not map");
    require(config.defaults.request.stream_idle_timeout_ms == 60000,
            "defaults.request.streamIdleTimeoutMs did not map");
    require(config.defaults.request.write_timeout_ms == 15000,
            "defaults.request.writeTimeoutMs did not map");
    require(config.defaults.pool.queue_requests == 64, "defaults.pool.queueRequests did not map");
    require(config.defaults.pool.queue_header_bytes == 64U * 1024U,
            "defaults.pool.queueHeaderBytes did not convert");
    require(config.defaults.pool.queue_timeout_ms == 5000,
            "defaults.pool.queueTimeout did not convert");

    require(config.maximums.worker.memory_max == "512MiB", "maximums.worker.memoryMax did not map");
    require(config.policy.max_worker_memory_bytes == 512U * 1024U * 1024U,
            "maximums.worker.memoryMax did not reach the policy");
    require(config.policy.max_requests_per_worker == 32,
            "maximums.request.maxInflightPerWorker did not reach the policy");
    require(config.policy.max_streaming_inflight_per_worker == 4,
            "maximums.request.maxStreamingInflightPerWorker did not reach "
            "the policy");
    require(config.policy.max_stream_idle_timeout_ms == 120000,
            "maximums.request.streamIdleTimeoutMs did not reach the policy");
    require(config.policy.max_write_timeout_ms == 60000,
            "maximums.request.writeTimeoutMs did not reach the policy");
    require(config.policy.max_queue_requests == 256,
            "maximums.pool.queueRequests did not reach the policy");
    require(config.policy.max_queue_header_bytes == 256U * 1024U,
            "maximums.pool.queueHeaderBytes did not reach the policy");
    require(config.policy.max_queue_timeout_ms == 10000,
            "maximums.pool.queueTimeout did not reach the policy");

    require(config.capacity.workers_total == 4, "capacity.workersTotal did not map");
    require(config.policy.max_workers == 4, "capacity.workersTotal did not reach the policy");
    require(config.capacity.activation_surge_workers == 2,
            "capacity.activationSurgeWorkers did not map");
    require(config.capacity.startups_concurrent == 2, "capacity.startupsConcurrent did not map");
    require(config.capacity.queued_requests_total == 128,
            "capacity.queuedRequestsTotal did not map");
    require(config.capacity.queued_header_bytes_total == 128U * 1024U,
            "capacity.queuedHeaderBytesTotal did not convert");
    require(config.capacity.worker_memory_commit_total == 2U * 1024U * 1024U * 1024U,
            "capacity.workerMemoryCommitTotal did not convert");

    require(config.recovery.crash_budget.max_events == 3,
            "recovery.crashBudget.maxEvents did not map");
    require(config.recovery.crash_budget.window_ms == 60000,
            "recovery.crashBudget.window did not convert");
    require(config.recovery.restart_backoff.initial_ms == 250,
            "recovery.restartBackoff.initial did not convert");
    require(config.recovery.restart_backoff.maximum_ms == 30000,
            "recovery.restartBackoff.maximum did not convert");
    require(config.recovery.restart_backoff.jitter == "1000",
            "recovery.restartBackoff.jitter did not map");
    require(config.recovery.replacements_concurrent_per_app == 1,
            "recovery.replacementsConcurrentPerApp did not map");
    require(config.recovery.active_health_interval_ms == 15000,
            "recovery.activeHealthInterval did not convert");
    require(config.recovery.active_health_failures == 2,
            "recovery.activeHealthFailures did not map");
}

void test_channels_section_maps_and_gates() {
    // GLM T08: the channels section maps into the model and the frozen
    // A2 bounds are enforced at the typed parse (schema-valid,
    // semantically illegal documents are startup errors).
    ParsedHostConfig mapped;
    require_valid_parse(R"json({
  "apiVersion": "capsid/host-v1",
  "applicationsRoot": "/srv/apps",
  "stateRoot": "/var/lib/capsid",
  "secretRootTemplate": "/secrets/{application}",
  "admin": {"unix": "/run/capsid/admin.sock"},
  "channels": {
    "maxChannels": 64,
    "maxMessageBytes": 2097152,
    "reassemblyBudgetBytes": 33554432,
    "sendBudgetBytes": 33554432,
    "perChannelQueueBytes": 2097152
  }
})json",
                        "channels-mapped", &mapped);
    require(mapped.channels.configured, "channels section flag");
    require(mapped.channels.max_channels == 64 && mapped.channels.max_message_bytes == 2097152 &&
                mapped.channels.reassembly_budget_bytes == 33554432 &&
                mapped.channels.send_budget_bytes == 33554432 &&
                mapped.channels.per_channel_queue_bytes == 2097152,
            "channels fields mapped");
    // defaults where absent
    require(mapped.channels.close_timeout_ms == 0 && mapped.channels.dial_timeout_ms == 0,
            "absent timeouts keep zero = deployment default");

    // illegal limits: message cap below the A2 floor
    require_parse_error(R"json({
  "apiVersion": "capsid/host-v1",
  "applicationsRoot": "/srv/apps",
  "stateRoot": "/var/lib/capsid",
  "secretRootTemplate": "/secrets/{application}",
  "admin": {"unix": "/run/capsid/admin.sock"},
  "channels": {"maxMessageBytes": 512}
})json",
                        "channels-message-below-floor");
    // illegal limits: channel count over the A2 ceiling
    require_parse_error(R"json({
  "apiVersion": "capsid/host-v1",
  "applicationsRoot": "/srv/apps",
  "stateRoot": "/var/lib/capsid",
  "secretRootTemplate": "/secrets/{application}",
  "admin": {"unix": "/run/capsid/admin.sock"},
  "channels": {"maxChannels": 4096}
})json",
                        "channels-count-over-ceiling");
    // illegal limits: per-channel queue over the 4 MiB cap
    require_parse_error(R"json({
  "apiVersion": "capsid/host-v1",
  "applicationsRoot": "/srv/apps",
  "stateRoot": "/var/lib/capsid",
  "secretRootTemplate": "/secrets/{application}",
  "admin": {"unix": "/run/capsid/admin.sock"},
  "channels": {"perChannelQueueBytes": 8388608}
})json",
                        "channels-queue-over-cap");
    // minimal document keeps channels unconfigured (v4 carriage)
    ParsedHostConfig minimal;
    require_valid_parse(
        R"json({"apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"}})json",
        "channels-absent", &minimal);
    require(!minimal.channels.configured, "absent channels stay off");
}

void test_minimal_document_parses() {
    ParsedHostConfig config;
    require_valid_parse(
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"}
        })json",
        "minimal document", &config);
    require(config.admin_mode == 0600, "absent admin.mode must keep the 0600 default");
    require(config.listeners.empty(), "absent listeners must stay empty");
    require(config.policy.module_allowlist.empty(), "absent permissions must stay empty");
    require(!config.policy.storage_allowed && !config.policy.stdio_allowed,
            "absent permissions must not flip the allow flags");
    require(config.isolation_mode.empty(), "absent isolation.mode must stay empty");
    require(config.trusted_keys.empty(), "absent keys must stay empty");
    require(config.capacity.workers_total == 1,
            "absent capacity.workersTotal must keep the default");
    require(config.policy.max_workers == 1, "absent capacity must keep max_workers 1");
    require(config.capacity.activation_surge_workers == 0,
            "absent capacity.activationSurgeWorkers must default to 0");
}

std::string replace_first(std::string text, const std::string& needle,
                          const std::string& replacement) {
    const std::size_t pos = text.find(needle);
    require(pos != std::string::npos, "missing mutation target in maximal v2 test: " + needle);
    text.replace(pos, needle.size(), replacement);
    return text;
}

std::string host_v2_signature(const ParsedHostConfig& c) {
    std::ostringstream out;
    out << c.is_v2 << '|' << c.applications_root << '|' << c.state_root << '|'
        << c.secret_root_template << '|' << c.bindings_root << '|' << c.admin_unix_path << '|'
        << c.admin_mode << '|' << c.isolation_mode << '|' << c.isolation_cgroup_root << '|';
    for (const auto& s : c.isolation_required) {
        out << s << ',';
    }
    out << '|';
    for (const auto& k : c.trusted_keys) {
        out << k.key_id << '=' << k.key_path << ';';
    }
    for (const auto& l : c.listeners) {
        out << "|L:" << l.name << ',' << l.tcp << ',' << l.public_scheme << ','
            << l.public_authority << ',' << l.public_origin_auto << ',' << l.trusted << ','
            << l.routing.mode << ',' << l.routing.suffix << ',' << l.routing.header << ','
            << l.limits.connections << ',' << l.limits.header_bytes << ','
            << l.limits.header_timeout_ms << ',' << l.limits.body_idle_timeout_ms << ','
            << l.limits.stream_idle_timeout_ms << ',' << l.limits.write_timeout_ms << ','
            << l.limits.channels << ',' << l.limits.channel_queue_bytes << ',' << l.cors.configured
            << ',' << l.cors.max_age_ms << ';';
        for (const auto& v : l.cors.allowed_origins)
            out << v << ',';
        out << ';';
        for (const auto& v : l.cors.allowed_methods)
            out << v << ',';
        out << ';';
        for (const auto& v : l.cors.allowed_headers)
            out << v << ',';
    }
    const auto& p = c.policy;
    out << "|P:" << p.is_v2;
    for (const auto& v : p.module_allowlist)
        out << v << ',';
    out << ';';
    for (const auto& v : p.env_patterns)
        out << v << ',';
    out << ';';
    for (const auto& v : p.fs_read_roots)
        out << v << ',';
    out << ';';
    for (const auto& t : p.fetch_targets) {
        out << t.host << ':';
        for (auto pt : t.ports)
            out << pt << ',';
    }
    out << ';';
    for (const auto& v : p.storage_namespaces)
        out << v << ',';
    out << ';';
    for (const auto& v : p.stdio_streams)
        out << v << ',';
    out << "|D:" << p.default_worker_memory_bytes << ',' << p.default_js_heap_bytes << ','
        << p.default_file_descriptors << ',' << p.default_request_timeout_ms << ','
        << p.default_requests_per_worker << ',' << p.default_workers << ','
        << p.has_default_min_ready << ',' << p.default_min_ready << ',' << p.default_max_workers
        << ',' << p.default_idle_timeout_ms << ',' << p.has_default_queue_requests << ','
        << p.default_queue_requests << ',' << p.default_queue_timeout_ms
        << "|M:" << p.max_worker_memory_bytes << ',' << p.max_js_heap_bytes << ','
        << p.max_file_descriptors << ',' << p.max_request_timeout_ms << ','
        << p.max_requests_per_worker << ',' << p.max_pool_workers << ',' << p.max_idle_timeout_ms
        << ',' << p.max_queue_requests << ',' << p.max_queue_timeout_ms
        << "|C:" << c.capacity.workers_total << ',' << c.capacity.activation_surge_workers << ','
        << c.capacity.channels.channels << ',' << c.capacity.channels.pending_dials << ','
        << c.capacity.channels.queue_bytes << ',' << c.capacity.max_channel_message_size << ','
        << c.capacity.channel_idle_timeout_ms << ',' << c.channels.configured << ','
        << c.channels.max_channels << ',' << c.channels.max_message_bytes << ','
        << c.channels.reassembly_budget_bytes << ',' << c.channels.send_budget_bytes << ','
        << c.channels.idle_timeout_ms << ',' << c.channels.per_channel_queue_bytes
        << "|R:" << c.recovery.crash_budget.max_events << ',' << c.recovery.crash_budget.window_ms
        << ',' << c.recovery.active_health_interval_ms << ',' << c.recovery.active_health_failures;
    return out.str();
}

// 100%-schema-field-consumption gate test for orthogonal host-v2 + app-v2
// (docs/config-v2-design.md §3.2): every single schema field must alter the
// parsed/compiled typed model when mutated.
void test_host_v2_orthogonal_and_full_consumption() {
    const std::string max_host_v2 = R"json({
      "apiVersion": "capsid/host-v2",
      "paths": {
        "applications": "/srv/capsid/applications",
        "state": "/srv/capsid/state",
        "secrets": "/srv/capsid/secrets/{application}",
        "bindings": "/etc/capsid/bindings"
      },
      "admin": { "unix": "/run/capsid/admin.sock" },
      "isolation": {
        "mode": "strict",
        "required": ["cgroup-v2"],
        "cgroupRoot": "/sys/fs/cgroup/capsid"
      },
      "trustedBytecodeKeys": {
        "2026-11": "/etc/capsid/keys/ed25519-2026-11.pub"
      },
      "listeners": [{
        "name": "public",
        "tcp": "0.0.0.0:8080",
        "publicOrigin": "https://apps.example.com",
        "trusted": false,
        "routing": {
          "mode": "header",
          "suffix": ".apps.example.com",
          "header": "x-capsid-app"
        },
        "limits": {
          "connections": 512,
          "headerSize": "32KiB",
          "headerTimeout": "5s",
          "bodyIdleTimeout": "30s",
          "streamIdleTimeout": "60s",
          "writeTimeout": "10s",
          "channels": 128
        },
        "cors": {
          "allowedOrigins": ["https://dev.example.com"],
          "allowedMethods": ["GET", "POST"],
          "allowedHeaders": ["content-type", "x-capsid-app"],
          "maxAge": "86400s"
        }
      }],
      "permissions": {
        "modules": ["capsid:env", "capsid:fs", "capsid:storage", "capsid:stdio"],
        "environmentNames": ["APP_MODE", "DATABASE_*"],
        "fsReadRoots": ["/srv/capsid/config"],
        "fetchTargets": ["api.example.com:443", "10.0.0.0/8:3306,3307", "[2001:db8::/32]:443"],
        "storageNamespaces": ["session"],
        "stdioStreams": ["stdout", "stderr"]
      },
      "defaults": {
        "worker": { "memoryMax": "256MiB", "jsHeap": "64MiB", "fileDescriptors": 64 },
        "request": { "timeout": "5s", "maxInflight": 64 },
        "pool": { "workers": 2, "queueRequests": 256, "queueTimeout": "10s" }
      },
      "maximums": {
        "worker": { "memoryMax": "512MiB", "jsHeap": "256MiB", "fileDescriptors": 256 },
        "request": { "timeout": "30s", "maxInflight": 128 },
        "pool": { "workers": 16, "queueRequests": 1024, "queueTimeout": "30s" }
      },
      "capacity": {
        "workersTotal": 64,
        "channelsTotal": 1024,
        "maxChannelMessageSize": "1MiB",
        "channelIdleTimeout": "60s"
      },
      "recovery": {
        "crashBudget": { "maxEvents": 5, "window": "60s" },
        "healthCheck": { "interval": "30s", "failures": 3 }
      }
    })json";

    ParsedHostConfig base_host;
    require_valid_parse(max_host_v2, "maximal host-v2", &base_host);
    require(base_host.is_v2 && base_host.policy.is_v2 && base_host.admin_mode == 0600 &&
                base_host.bindings_root == "/etc/capsid/bindings" &&
                base_host.capacity.activation_surge_workers == 16 && base_host.channels.configured,
            "maximal host-v2 projection incomplete");
    const std::string base_host_sig = host_v2_signature(base_host);

    const std::vector<std::pair<std::string, std::string>> host_mutations = {
        {R"("applications": "/srv/capsid/applications")", R"("applications": "/srv/capsid/apps2")"},
        {R"("state": "/srv/capsid/state")", R"("state": "/srv/capsid/state2")"},
        {R"("secrets": "/srv/capsid/secrets/{application}")",
         R"("secrets": "/srv/capsid/sec2/{application}")"},
        {R"("bindings": "/etc/capsid/bindings")", R"("bindings": "/etc/capsid/bindings2")"},
        {R"("unix": "/run/capsid/admin.sock")", R"("unix": "/run/capsid/admin2.sock")"},
        {R"("required": ["cgroup-v2"])", R"("required": ["pid"])"},
        {R"("cgroupRoot": "/sys/fs/cgroup/capsid")", R"("cgroupRoot": "/sys/fs/cgroup/capsid2")"},
        {R"("2026-11": "/etc/capsid/keys/ed25519-2026-11.pub")",
         R"("2026-12": "/etc/capsid/keys/ed25519-2026-12.pub")"},
        {R"("name": "public")", R"("name": "edge")"},
        {R"("tcp": "0.0.0.0:8080")", R"("tcp": "127.0.0.1:9090")"},
        {R"("publicOrigin": "https://apps.example.com")",
         R"("publicOrigin": "http://alt.example.com")"},
        {R"("trusted": false)", R"("trusted": true)"},
        {R"("mode": "header")", R"("mode": "subdomain")"},
        {R"("suffix": ".apps.example.com")", R"("suffix": ".alt.example.com")"},
        {R"("header": "x-capsid-app")", R"("header": "x-alt-app")"},
        {R"("connections": 512)", R"("connections": 256)"},
        {R"("headerSize": "32KiB")", R"("headerSize": "64KiB")"},
        {R"("headerTimeout": "5s")", R"("headerTimeout": "6s")"},
        {R"("bodyIdleTimeout": "30s")", R"("bodyIdleTimeout": "31s")"},
        {R"("streamIdleTimeout": "60s")", R"("streamIdleTimeout": "61s")"},
        {R"("writeTimeout": "10s")", R"("writeTimeout": "11s")"},
        {R"("channels": 128)", R"("channels": 64)"},
        {R"("allowedOrigins": ["https://dev.example.com"])",
         R"("allowedOrigins": ["https://alt.example.com"])"},
        {R"("allowedMethods": ["GET", "POST"])", R"("allowedMethods": ["GET", "PUT"])"},
        {R"("allowedHeaders": ["content-type", "x-capsid-app"])",
         R"("allowedHeaders": ["content-type", "x-other"])"},
        {R"("maxAge": "86400s")", R"("maxAge": "3600s")"},
        {R"("modules": ["capsid:env", "capsid:fs", "capsid:storage", "capsid:stdio"])",
         R"("modules": ["capsid:env"])"},
        {R"("environmentNames": ["APP_MODE", "DATABASE_*"])",
         R"("environmentNames": ["APP_MODE"])"},
        {R"("fsReadRoots": ["/srv/capsid/config"])", R"("fsReadRoots": ["/srv/capsid/cfg2"])"},
        {R"("fetchTargets": ["api.example.com:443", "10.0.0.0/8:3306,3307", "[2001:db8::/32]:443"])",
         R"("fetchTargets": ["api.example.com:8443"])"},
        {R"("storageNamespaces": ["session"])", R"("storageNamespaces": ["cache"])"},
        {R"("stdioStreams": ["stdout", "stderr"])", R"("stdioStreams": ["stdout"])"},
        {R"("worker": { "memoryMax": "256MiB", "jsHeap": "64MiB", "fileDescriptors": 64 })",
         R"("worker": { "memoryMax": "300MiB", "jsHeap": "64MiB", "fileDescriptors": 64 })"},
        {R"("jsHeap": "64MiB", "fileDescriptors": 64)",
         R"("jsHeap": "96MiB", "fileDescriptors": 64)"},
        {R"("fileDescriptors": 64)", R"("fileDescriptors": 96)"},
        {R"("request": { "timeout": "5s", "maxInflight": 64 })",
         R"("request": { "timeout": "6s", "maxInflight": 64 })"},
        {R"("maxInflight": 64)", R"("maxInflight": 32)"},
        {R"("pool": { "workers": 2, "queueRequests": 256, "queueTimeout": "10s" })",
         R"("pool": { "workers": 3, "queueRequests": 256, "queueTimeout": "10s" })"},
        {R"("queueRequests": 256, "queueTimeout": "10s")",
         R"("queueRequests": 128, "queueTimeout": "10s")"},
        {R"("queueTimeout": "10s")", R"("queueTimeout": "12s")"},
        {R"("worker": { "memoryMax": "512MiB", "jsHeap": "256MiB", "fileDescriptors": 256 })",
         R"("worker": { "memoryMax": "640MiB", "jsHeap": "256MiB", "fileDescriptors": 256 })"},
        {R"("jsHeap": "256MiB", "fileDescriptors": 256)",
         R"("jsHeap": "300MiB", "fileDescriptors": 256)"},
        {R"("fileDescriptors": 256)", R"("fileDescriptors": 300)"},
        {R"("request": { "timeout": "30s", "maxInflight": 128 })",
         R"("request": { "timeout": "35s", "maxInflight": 128 })"},
        {R"("maxInflight": 128)", R"("maxInflight": 192)"},
        {R"("pool": { "workers": 16, "queueRequests": 1024, "queueTimeout": "30s" })",
         R"("pool": { "workers": 20, "queueRequests": 1024, "queueTimeout": "30s" })"},
        {R"("queueRequests": 1024, "queueTimeout": "30s")",
         R"("queueRequests": 2048, "queueTimeout": "30s")"},
        {R"("queueTimeout": "30s")", R"("queueTimeout": "45s")"},
        {R"("workersTotal": 64)", R"("workersTotal": 32)"},
        {R"("channelsTotal": 1024)", R"("channelsTotal": 512)"},
        {R"("maxChannelMessageSize": "1MiB")", R"("maxChannelMessageSize": "2MiB")"},
        {R"("channelIdleTimeout": "60s")", R"("channelIdleTimeout": "90s")"},
        {R"("maxEvents": 5)", R"("maxEvents": 7)"},
        {R"("window": "60s")", R"("window": "120s")"},
        {R"("interval": "30s")", R"("interval": "15s")"},
        {R"("failures": 3)", R"("failures": 4)"},
    };
    for (const auto& mutation : host_mutations) {
        const std::string mutated_json =
            replace_first(max_host_v2, mutation.first, mutation.second);
        ParsedHostConfig mutated;
        require_valid_parse(mutated_json, ("host-v2 mutation: " + mutation.first).c_str(),
                            &mutated);
        require(host_v2_signature(mutated) != base_host_sig,
                "silent dead host-v2 field detected for: " + mutation.first);
    }

    // publicOrigin omission defaults to http://<tcp> with auto port substitution.
    ParsedHostConfig auto_origin_cfg;
    require_valid_parse(
        R"json({"apiVersion":"capsid/host-v2","paths":{"applications":"/a","state":"/s","secrets":"/k/{application}"},"admin":{"unix":"/u.sock"},"listeners":[{"name":"p","tcp":"127.0.0.1:0","routing":{"mode":"path"}}]})json",
        "host-v2 omitted publicOrigin", &auto_origin_cfg);
    require(auto_origin_cfg.listeners.size() == 1 &&
                auto_origin_cfg.listeners[0].public_scheme == "http" &&
                auto_origin_cfg.listeners[0].public_authority == "127.0.0.1:0" &&
                auto_origin_cfg.listeners[0].public_origin_auto,
            "host-v2 omitted publicOrigin did not default to http://<tcp>");

    // Also prove 100% consumption of maximal capsid/app-v2 through parse_app_request +
    // compile_policy + parse_app_bindings.
    const std::string max_app_v2 = R"json({
      "apiVersion": "capsid/app-v2",
      "entry": "bundle.mjs",
      "permissions": {
        "modules": ["capsid:env", "capsid:fs", "capsid:storage", "capsid:stdio"],
        "env": {
          "APP_MODE": { "value": "production" },
          "DATABASE_URL": { "valueFrom": "db-url" }
        },
        "fs": {
          "read": {
            "allow": ["/srv/capsid/config"],
            "deny": ["/srv/capsid/config/private.json"]
          }
        },
        "fetch": {
          "allow": ["api.example.com:443", "10.0.0.0/8:3306"]
        },
        "storage": { "namespaces": ["session"] },
        "stdio": ["stdout", "stderr"]
      },
      "bindings": {
        "redis": {
          "permissions": {
            "net": { "allow": ["10.0.1.15:6379"] },
            "fs": { "read": ["/srv/capsid/config/ca.pem"], "write": ["/srv/capsid/config/w"] },
            "env": ["APP_MODE"],
            "stdio": ["stdout"]
          },
          "config": { "keyPrefix": "orders:" },
          "secrets": { "password": { "valueFrom": "redis-password" } }
        }
      },
      "worker": { "memoryMax": "256MiB", "jsHeap": "64MiB", "fileDescriptors": 64 },
      "request": { "timeout": "5s", "maxInflight": 64 },
      "pool": { "workers": 2, "queueRequests": 256, "queueTimeout": "10s" },
      "healthCheck": { "path": "/health", "timeout": "2s" }
    })json";

    auto app_signature = [&](const std::string& json) {
        const auto v = validate_config_json(ConfigDocument::kApplication, json);
        require(v.ok, "maximal app-v2 failed schema validation at " + v.error.path + ": " +
                          v.error.message);
        const std::vector<std::uint8_t> bytes(json.begin(), json.end());
        capsid::host::AppRequest req;
        std::string err;
        require(capsid::host::parse_app_request(bytes, &req, &err),
                "parse_app_request failed on app-v2: " + err);
        std::vector<capsid::host::AppBindingRequest> bindings;
        require(capsid::host::parse_app_bindings(bytes, &bindings, &err),
                "parse_app_bindings failed on app-v2: " + err);
        std::vector<capsid::host::EffectiveEnvEntry> resolved;
        for (const auto& e : req.env) {
            capsid::host::EffectiveEnvEntry out;
            out.name = e.name;
            out.from_secret = e.from_secret;
            out.secret_key_id = e.secret_key_id;
            out.secret_revision = e.from_secret ? "rev-1" : "";
            out.literal = e.literal;
            resolved.push_back(std::move(out));
        }
        capsid::host::HostPolicy host = base_host.policy;
        host.fs_read_roots.push_back("/srv/capsid/config");
        const auto compiled = capsid::host::compile_policy(host, req, resolved);
        require(compiled.ok, "compile_policy failed on maximal app-v2: " + compiled.error);
        std::ostringstream sig;
        sig << compiled.effective.effective_digest << '|' << compiled.effective.app_config_digest
            << "|envLits:";
        for (const auto& e : resolved)
            sig << e.name << '=' << e.literal << ';';
        sig << "|B:";
        for (const auto& b : bindings) {
            sig << b.id << ':' << b.config_json << ';';
            for (const auto& r : b.net_rules)
                sig << "n:" << r << ',';
            for (const auto& r : b.fs_read)
                sig << "fr:" << r << ',';
            for (const auto& r : b.fs_write)
                sig << "fw:" << r << ',';
            for (const auto& r : b.env)
                sig << "e:" << r << ',';
            for (const auto& r : b.stdio)
                sig << "s:" << r << ',';
            for (const auto& s : b.secrets)
                sig << "sec:" << s.name << '=' << s.key_id << ',';
        }
        return sig.str();
    };
    const std::string base_app_sig = app_signature(max_app_v2);
    const std::vector<std::pair<std::string, std::string>> app_mutations = {
        {R"("modules": ["capsid:env", "capsid:fs", "capsid:storage", "capsid:stdio"])",
         R"("modules": ["capsid:env"])"},
        {R"("APP_MODE": { "value": "production" })", R"("APP_MODE": { "value": "staging" })"},
        {R"("DATABASE_URL": { "valueFrom": "db-url" })",
         R"("DATABASE_URL": { "valueFrom": "db-url-2" })"},
        {R"("allow": ["/srv/capsid/config"])", R"("allow": ["/srv/capsid/config/sub"])"},
        {R"("deny": ["/srv/capsid/config/private.json"])",
         R"("deny": ["/srv/capsid/config/other.json"])"},
        {R"("allow": ["api.example.com:443", "10.0.0.0/8:3306"])",
         R"("allow": ["api.example.com:443"])"},
        {R"("namespaces": ["session"])", R"("namespaces": [])"},
        {R"("stdio": ["stdout", "stderr"])", R"("stdio": ["stdout"])"},
        {R"("net": { "allow": ["10.0.1.15:6379"] })", R"("net": { "allow": ["10.0.1.16:6379"] })"},
        {R"("read": ["/srv/capsid/config/ca.pem"])", R"("read": ["/srv/capsid/config/ca2.pem"])"},
        {R"("write": ["/srv/capsid/config/w"])", R"("write": ["/srv/capsid/config/w2"])"},
        {R"("env": ["APP_MODE"])", R"("env": ["OTHER_ENV"])"},
        {R"("stdio": ["stdout"])", R"("stdio": ["stderr"])"},
        {R"("config": { "keyPrefix": "orders:" })", R"("config": { "keyPrefix": "items:" })"},
        {R"("password": { "valueFrom": "redis-password" })",
         R"("password": { "valueFrom": "redis-password-2" })"},
        {R"("memoryMax": "256MiB")", R"("memoryMax": "300MiB")"},
        {R"("jsHeap": "64MiB")", R"("jsHeap": "96MiB")"},
        {R"("fileDescriptors": 64)", R"("fileDescriptors": 96)"},
        {R"("timeout": "5s")", R"("timeout": "7s")"},
        {R"("maxInflight": 64)", R"("maxInflight": 32)"},
        {R"("workers": 2)", R"("workers": 3)"},
        {R"("queueRequests": 256)", R"("queueRequests": 128)"},
        {R"("queueTimeout": "10s")", R"("queueTimeout": "12s")"},
        {R"("path": "/health")", R"("path": "/ready")"},
        {R"("timeout": "2s")", R"("timeout": "3s")"},
    };
    for (const auto& mutation : app_mutations) {
        const std::string mutated = replace_first(max_app_v2, mutation.first, mutation.second);
        require(app_signature(mutated) != base_app_sig,
                "silent dead app-v2 field detected for: " + mutation.first);
    }

    // Also prove elastic pool fields (minReady, maxWorkers, idleTimeout) on app-v2 and
    // host-v2 defaults/maximums alter compiled models and digests.
    const std::string elastic_host_v2 = R"json({
      "apiVersion": "capsid/host-v2",
      "paths": {"applications": "/srv/apps", "state": "/var/lib/capsid", "secrets": "/secrets/{application}"},
      "admin": {"unix": "/run/capsid/admin.sock"},
      "listeners": [{"name": "p", "tcp": "127.0.0.1:8080", "routing": {"mode": "path"}}],
      "defaults": {"pool": {"minReady": 0, "maxWorkers": 4, "idleTimeout": "30s"}},
      "maximums": {"pool": {"maxWorkers": 8, "idleTimeout": "120s"}},
      "capacity": {"workersTotal": 16}
    })json";
    ParsedHostConfig elastic_host;
    require_valid_parse(elastic_host_v2, "elastic host-v2 defaults", &elastic_host);
    require(elastic_host.policy.has_default_min_ready &&
                elastic_host.policy.default_min_ready == 0 &&
                elastic_host.policy.default_max_workers == 4 &&
                elastic_host.policy.default_idle_timeout_ms == 30000 &&
                elastic_host.policy.max_pool_workers == 8 &&
                elastic_host.policy.max_idle_timeout_ms == 120000,
            "elastic host-v2 defaults/maximums did not map");
    const std::string elastic_app_v2 = R"json({
      "apiVersion": "capsid/app-v2",
      "pool": {"minReady": 0, "maxWorkers": 4, "idleTimeout": "45s"}
    })json";
    const std::string elastic_sig = app_signature(elastic_app_v2);
    for (const auto& m : std::vector<std::pair<std::string, std::string>>{
             {R"("minReady": 0)", R"("minReady": 1)"},
             {R"("maxWorkers": 4)", R"("maxWorkers": 6)"},
             {R"("idleTimeout": "45s")", R"("idleTimeout": "90s")"}}) {
        require(app_signature(replace_first(elastic_app_v2, m.first, m.second)) != elastic_sig,
                "silent dead elastic app-v2 field: " + m.first);
    }
}

void test_semantic_gates() {
    std::string admin_mode =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock", "mode": "0640"}
        })json";
    require_parse_error(admin_mode, "admin.mode 0640");

    std::string isolation_mode =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "isolation": {"mode": "loose"}
        })json";
    require_parse_error(isolation_mode, "isolation.mode loose");

    // capacity.workersTotal 0 is rejected by the schema itself (positive
    // integer, min 1) — the schema owns that gate; only the Host int-max
    // gate lives in the typed parse.

    std::string giant_workers =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "capacity": {"workersTotal": 4294967296}
        })json";
    require_parse_error(giant_workers, "capacity.workersTotal over int max");

    // §9.4: activationSurgeWorkers is non-negative — a negative surge
    // budget is a capacity misconfiguration, not a reasonable request.
    std::string negative_surge =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "capacity": {"workersTotal": 4,
                       "activationSurgeWorkers": -1}
        })json";
    require_parse_error(negative_surge, "negative capacity.activationSurgeWorkers");

    std::string missing_root =
        R"json({
          "apiVersion": "capsid/host-v1",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"}
        })json";
    require_parse_error(missing_root, "missing applicationsRoot");

    std::string missing_template =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/app",
          "admin": {"unix": "/run/capsid/admin.sock"}
        })json";
    require_parse_error(missing_template, "secretRootTemplate without {application}");
}

void test_grammar_gates() {
    std::string bad_duration =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "maximums": {"pool": {"queueTimeout": "5sX"}}
        })json";
    require_parse_error(bad_duration, "queueTimeout bad unit");

    std::string zero_duration =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "recovery": {"restartBackoff": {"initial": "0s"}}
        })json";
    require_parse_error(zero_duration, "restartBackoff.initial zero");

    std::string bad_size =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "maximums": {"worker": {"memoryMax": "512Mi"}}
        })json";
    require_parse_error(bad_size, "memoryMax bad suffix");

    std::string bad_jitter =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "recovery": {"restartBackoff": {"jitter": "10001"}}
        })json";
    require_parse_error(bad_jitter, "restartBackoff.jitter over 10000");

    std::string negative_integer =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "listeners": [{"limits": {"connections": -1}}]
        })json";
    require_parse_error(negative_integer, "negative limit integer");

    std::string bad_listener_duration =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "listeners": [{"limits": {"headerTimeout": "5"}}]
        })json";
    require_parse_error(bad_listener_duration, "headerTimeout without unit");

    std::string bad_cors_origin =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "listeners": [{"cors": {
            "allowedOrigins": ["http://"],
            "allowedMethods": ["GET"],
            "allowedHeaders": ["x"]
          }}]
        })json";
    require_parse_error(bad_cors_origin, "cors origin without an authority");

    std::string bad_cors_method =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "listeners": [{"cors": {
            "allowedOrigins": ["*"],
            "allowedMethods": ["GE T"],
            "allowedHeaders": ["x"]
          }}]
        })json";
    require_parse_error(bad_cors_method, "cors method with a space");

    std::string empty_cors_origins =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "listeners": [{"cors": {
            "allowedOrigins": [],
            "allowedMethods": ["GET"],
            "allowedHeaders": ["x"]
          }}]
        })json";
    require_parse_error(empty_cors_origins, "cors with an empty origin list");

    std::string mixed_wildcard_origins =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock"},
          "listeners": [{"cors": {
            "allowedOrigins": ["*", "https://example.com"],
            "allowedMethods": ["GET"],
            "allowedHeaders": ["x"]
          }}]
        })json";
    require_parse_error(mixed_wildcard_origins, "cors wildcard mixed with exact origins");
}

void test_cors_tokens_use_the_http_tchar_grammar() {
    for (const std::string token : {"M#", "M$", "M%", "M&", "M^", "M`", "M|"}) {
        require(capsid::host::valid_cors_method_token(token),
                "valid HTTP method tchar was rejected: " + token);
        require(capsid::host::valid_cors_header_token(token),
                "valid HTTP field-name tchar was rejected: " + token);
    }
    for (const std::string token : {"M,", "M:", "M[", "M]", "M@"}) {
        require(!capsid::host::valid_cors_method_token(token),
                "HTTP separator was accepted in method token: " + token);
        require(!capsid::host::valid_cors_header_token(token),
                "HTTP separator was accepted in field-name token: " + token);
    }
}

void test_cors_preflight_without_requested_headers() {
    capsid::host::ListenerCorsConfig config;
    config.configured = true;
    config.allowed_origins = {"http://allowed.test"};
    config.allowed_methods = {"POST"};
    capsid::host::ListenerCors cors(config);

    boost::beast::http::request<boost::beast::http::string_body> request;
    request.method(boost::beast::http::verb::options);
    request.set(boost::beast::http::field::origin, "http://allowed.test");
    request.set(boost::beast::http::field::access_control_request_method, "POST");
    require(cors.prepare(request) == capsid::host::CorsDecision::kPreflightAllowed,
            "headerless CORS preflight was rejected");

    boost::beast::http::response<boost::beast::http::string_body> response;
    cors.build_preflight(response);
    require(response.result() == boost::beast::http::status::no_content,
            "headerless CORS preflight did not produce 204");
}

void test_request_timeout_tiers_are_finite_and_ordered() {
    const auto document = [](const std::string& tiers) {
        return std::string(
                   R"json({"apiVersion":"capsid/host-v1","applicationsRoot":"/srv/apps","stateRoot":"/var/lib/capsid","secretRootTemplate":"/secrets/{application}","admin":{"unix":"/run/capsid/admin.sock","mode":"0600"},)json") +
               tiers + "}";
    };
    ParsedHostConfig config;
    std::string error;
    require(
        parse_host_config(
            document(
                R"("defaults":{"request":{"timeout":"100ms"}},"maximums":{"request":{"timeout":"120ms"}})"),
            &config, &error),
        "valid finite timeout tiers rejected: " + error);
    require(config.policy.default_request_timeout_ms == 100 &&
                config.policy.max_request_timeout_ms == 120,
            "finite timeout tiers were discarded");
    for (
        const auto& tiers : std::vector<std::string>{
            R"("defaults":{"request":{"timeout":"0ms"}})",
            R"("maximums":{"request":{"timeout":"0ms"}})",
            R"("maximums":{"request":{"timeout":"9223372036854775808ms"}})",
            R"("defaults":{"request":{"timeout":"200ms"}},"maximums":{"request":{"timeout":"120ms"}})"}) {
        require(!parse_host_config(document(tiers), &config, &error),
                "invalid timeout tiers accepted: " + tiers);
    }
}

void test_error_is_stable_and_safe() {
    ParsedHostConfig config;
    std::string error;
    std::string document =
        R"json({
          "apiVersion": "capsid/host-v1",
          "applicationsRoot": "/srv/apps",
          "stateRoot": "/var/lib/capsid",
          "secretRootTemplate": "/secrets/{application}",
          "admin": {"unix": "/run/capsid/admin.sock", "mode": "0640"}
        })json";
    require(!parse_host_config(document, &config, &error), "bad-mode document was accepted");
    require(error == "admin.mode must be 0600", "stable error text changed: '" + error + "'");
    require(error.find('/') == std::string::npos, "error text leaks a path: '" + error + "'");
}

void test_ipv6_fetch_authorities_reach_the_host_policy() {
    const auto document = [](const std::string& authority) {
        return std::string(
                   R"({"apiVersion":"capsid/host-v1","applicationsRoot":"/srv/apps","stateRoot":"/var/lib/capsid","secretRootTemplate":"/secrets/{application}","admin":{"unix":"/run/capsid/admin.sock","mode":"0600"},"permissions":{"fetchTargets":[")") +
               authority + R"("]}})";
    };
    for (const auto& authority :
         std::vector<std::string>{"[::1]:443,8443", "[0:0:0:0:0:0:0:1]:443,8443"}) {
        ParsedHostConfig config;
        std::string error;
        require(parse_host_config(document(authority), &config, &error),
                "bracketed IPv6 Host fetch authority rejected: " + error);
        require(config.policy.fetch_targets.size() == 1 &&
                    config.policy.fetch_targets[0].host == "::1" &&
                    config.policy.fetch_targets[0].ports == std::vector<std::uint16_t>({443, 8443}),
                "IPv6 Host fetch authority gained or lost ports");
    }
    for (const auto& authority :
         std::vector<std::string>{"[::1]:", "[::1]:443,", "[::1]:65536", "[::1]extra:443",
                                  "[fe80::1%1]:443", "[fe80::1%lo0]:443", "[fe80::1%25lo0]:443",
                                  "[::ffff:127.0.0.1%1]:443", "[::1%1]", "::1"}) {
        ParsedHostConfig config;
        std::string error;
        require(!parse_host_config(document(authority), &config, &error),
                "malformed Host fetch authority accepted: " + authority);
    }
}

} // namespace

int main() {
    test_full_document_maps_every_field();
    test_channels_section_maps_and_gates();
    test_minimal_document_parses();
    test_host_v2_orthogonal_and_full_consumption();
    test_semantic_gates();
    test_grammar_gates();
    test_cors_tokens_use_the_http_tchar_grammar();
    test_cors_preflight_without_requested_headers();
    test_error_is_stable_and_safe();
    test_request_timeout_tiers_are_finite_and_ordered();
    test_ipv6_fetch_authorities_reach_the_host_policy();
    std::cout << "test-host-config-model: all tests passed" << std::endl;
    return 0;
}
