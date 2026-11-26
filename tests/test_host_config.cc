#include "host/config.h"
#include "host/policy_compiler.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

using capsid::host::ConfigDocument;
using capsid::host::ConfigErrorCode;
using capsid::host::ConfigValidationResult;
using capsid::host::kMaxConfigBytes;
using capsid::host::kMaxConfigNesting;
using capsid::host::validate_config_json;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test-host-config: " << message << std::endl;
    std::exit(1);
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

void require_valid(ConfigDocument document, std::string_view json, const char* label) {
    const ConfigValidationResult result = validate_config_json(document, json);
    require(result.ok, std::string(label) + " was rejected at '" + result.error.path +
                           "': " + result.error.message);
    require(result.error.code == ConfigErrorCode::kNone,
            std::string(label) + " succeeded with a non-empty error code");
    require(result.error.path.empty(), std::string(label) + " succeeded with an error path");
    require(result.error.message.empty(), std::string(label) + " succeeded with an error message");
}

void require_unknown_field(ConfigDocument document, std::string_view json,
                           const char* expected_path, const char* label) {
    const ConfigValidationResult result = validate_config_json(document, json);
    require(!result.ok, std::string(label) + " was accepted");
    require(result.error.code == ConfigErrorCode::kUnknownField,
            std::string(label) + " did not report kUnknownField");
    require(result.error.path == expected_path, std::string(label) + " reported path '" +
                                                    result.error.path + "' instead of '" +
                                                    expected_path + "'");
    require(!result.error.message.empty(),
            std::string(label) + " did not provide a safe diagnostic");
}

void require_error(ConfigDocument document, std::string_view json, ConfigErrorCode expected_code,
                   const char* expected_path, const char* label) {
    const ConfigValidationResult result = validate_config_json(document, json);
    require(!result.ok, std::string(label) + " was accepted");
    require(result.error.code == expected_code,
            std::string(label) + " reported the wrong error code");
    require(result.error.path == expected_path, std::string(label) + " reported path '" +
                                                    result.error.path + "' instead of '" +
                                                    expected_path + "'");
    require(!result.error.message.empty(),
            std::string(label) + " did not provide a safe diagnostic");
}

std::string host_v2_with(std::string_view extra_fields = {}) {
    std::string out =
        R"json({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/var/lib/capsid/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock"},"listeners":[{"name":"public","tcp":"127.0.0.1:8080","routing":{"mode":"path"}}])json";
    if (!extra_fields.empty()) {
        out += ',';
        out += extra_fields;
    }
    out += '}';
    return out;
}

void test_minimal_documents_are_valid() {
    require_valid(ConfigDocument::kHost, host_v2_with(), "minimal Host config");
    require_valid(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","entry":"bundle.mjs","pool":{"workers":1}})json",
        "minimal App config");
}

void test_complete_v2_document_shapes_remain_recursively_fail_closed() {
    require_unknown_field(
        ConfigDocument::kHost,
        R"json({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/var/lib/capsid/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock"},"listeners":[{"name":"public","tcp":"127.0.0.1:8080","publicOrigin":"http://public.example","routing":{"mode":"path"},"limits":{"connections":1,"headerSize":"1KiB","headerTimeout":"1s","bodyIdleTimeout":"1s","streamIdleTimeout":"1s","surprise":true}}]})json",
        "/listeners/0/limits/surprise", "unknown listener limit");
    require_unknown_field(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","permissions":{"env":{"TOKEN":{"valueFrom":"token","path":"/tmp/token"}}},"pool":{"workers":1}})json",
        "/permissions/env/TOKEN/path", "unknown App environment entry field");
}

void test_listener_public_origin_is_typed() {
    require_valid(
        ConfigDocument::kHost,
        R"json({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/var/lib/capsid/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock"},"listeners":[{"name":"public","tcp":"127.0.0.1:8080","publicOrigin":"https://api.example.com:443","routing":{"mode":"path"}}]})json",
        "path listener with fixed public origin");
    require_error(
        ConfigDocument::kHost,
        R"json({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/var/lib/capsid/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock"},"listeners":[{"name":"public","tcp":"127.0.0.1:8080","publicOrigin":443,"routing":{"mode":"path"}}]})json",
        ConfigErrorCode::kInvalidValue, "/listeners/0/publicOrigin", "non-string public origin");
}

void test_complete_v2_collections_and_limits_are_typed() {
    require_error(
        ConfigDocument::kHost,
        R"json({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/var/lib/capsid/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock"},"listeners":{}})json",
        ConfigErrorCode::kInvalidValue, "/listeners", "non-array Host listeners");
    require_error(
        ConfigDocument::kHost,
        R"json({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/var/lib/capsid/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock"},"listeners":["public"]})json",
        ConfigErrorCode::kInvalidValue, "/listeners/0", "non-object Host listener");
    require_error(ConfigDocument::kHost,
                  host_v2_with(R"json("trustedBytecodeKeys":{"release/2026~a":7})json"),
                  ConfigErrorCode::kInvalidValue, "/trustedBytecodeKeys/release~12026~0a",
                  "non-string trusted bytecode public key path");
    require_error(ConfigDocument::kHost,
                  host_v2_with(R"json("permissions":{"modules":["capsid:stdio",7]})json"),
                  ConfigErrorCode::kInvalidValue, "/permissions/modules/1",
                  "non-string Host module allowlist item");
    require_error(ConfigDocument::kHost, host_v2_with(R"json("capacity":{"workersTotal":0})json"),
                  ConfigErrorCode::kInvalidValue, "/capacity/workersTotal",
                  "zero Host worker capacity");
    require_error(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","worker":{"fileDescriptors":0},"pool":{"workers":1}})json",
        ConfigErrorCode::kInvalidValue, "/worker/fileDescriptors",
        "zero App file descriptor limit");
    require_error(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","request":{"maxInflight":0},"pool":{"workers":1}})json",
        ConfigErrorCode::kInvalidValue, "/request/maxInflight", "zero App request inflight limit");
}

std::string app_with_environment_entry(std::string_view name, std::string_view entry_json) {
    return "{\"apiVersion\":\"capsid/app-v2\",\"permissions\":{\"env\":{\"" + std::string(name) +
           "\":" + std::string(entry_json) + "}},\"pool\":{\"workers\":1}}";
}

void test_environment_request_schema_is_fail_closed() {
    require_valid(
        ConfigDocument::kHost,
        host_v2_with(R"json("permissions":{"environmentNames":["APP_MODE","API_*"]})json"),
        "valid Host environment allowlist");
    require_valid(ConfigDocument::kApplication,
                  app_with_environment_entry("APP_MODE", R"json({"value":""})json"),
                  "empty literal environment value");
    require_valid(
        ConfigDocument::kApplication,
        app_with_environment_entry("API_TOKEN", R"json({"valueFrom":"orders-api-token"})json"),
        "secret environment request");

    require_error(ConfigDocument::kHost,
                  host_v2_with(R"json("permissions":{"environmentNames":["API-*"]})json"),
                  ConfigErrorCode::kInvalidValue, "/permissions/environmentNames/0",
                  "invalid Host environment pattern");
    require_error(ConfigDocument::kApplication,
                  app_with_environment_entry("1TOKEN", R"json({"value":"x"})json"),
                  ConfigErrorCode::kInvalidValue, "/permissions/env/1TOKEN",
                  "environment name beginning with a digit");
    require_error(ConfigDocument::kApplication,
                  app_with_environment_entry("API_*", R"json({"value":"x"})json"),
                  ConfigErrorCode::kInvalidValue, "/permissions/env/API_*",
                  "wildcard App environment name");
    require_error(ConfigDocument::kApplication, app_with_environment_entry("API_TOKEN", "{}"),
                  ConfigErrorCode::kInvalidValue, "/permissions/env/API_TOKEN",
                  "environment request without value source");
    require_error(ConfigDocument::kApplication,
                  app_with_environment_entry(
                      "API_TOKEN", R"json({"value":"literal","valueFrom":"orders-api-token"})json"),
                  ConfigErrorCode::kInvalidValue, "/permissions/env/API_TOKEN",
                  "environment request with two value sources");
    require_error(ConfigDocument::kApplication,
                  app_with_environment_entry("API_TOKEN", R"json({"valueFrom":""})json"),
                  ConfigErrorCode::kInvalidValue, "/permissions/env/API_TOKEN/valueFrom",
                  "empty secret key ID");
    require_error(
        ConfigDocument::kApplication,
        app_with_environment_entry("API_TOKEN", R"json({"valueFrom":"../orders/token"})json"),
        ConfigErrorCode::kInvalidValue, "/permissions/env/API_TOKEN/valueFrom",
        "secret key path escape");
    require_error(
        ConfigDocument::kApplication,
        app_with_environment_entry("API_TOKEN", R"json({"valueFrom":"orders..token"})json"),
        ConfigErrorCode::kInvalidValue, "/permissions/env/API_TOKEN/valueFrom",
        "secret key with an empty component");
    require_error(
        ConfigDocument::kApplication,
        app_with_environment_entry("API_TOKEN", R"json({"valueFrom":"orders$token"})json"),
        ConfigErrorCode::kInvalidValue, "/permissions/env/API_TOKEN/valueFrom",
        "secret key with non-contract punctuation");
    require_error(
        ConfigDocument::kApplication,
        app_with_environment_entry("API_TOKEN", R"json({"value":"prefix\u0000suffix"})json"),
        ConfigErrorCode::kInvalidValue, "/permissions/env/API_TOKEN/value",
        "NUL literal environment value");
    require_error(
        ConfigDocument::kApplication,
        app_with_environment_entry("API_TOKEN", R"json({"valueFrom":"token\u0000suffix"})json"),
        ConfigErrorCode::kInvalidValue, "/permissions/env/API_TOKEN/valueFrom",
        "NUL secret key ID");
    require_error(
        ConfigDocument::kApplication,
        app_with_environment_entry(R"json(API_TOKEN\u0000shadow)json", R"json({"value":"x"})json"),
        ConfigErrorCode::kInvalidJson, "", "NUL App environment member name");
    require_error(ConfigDocument::kHost,
                  R"json({"apiVersion":"capsid/host-v2","mystery\u0000suffix":true})json",
                  ConfigErrorCode::kInvalidJson, "", "NUL Host object member name");
    require_error(
        ConfigDocument::kHost,
        R"json({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps\u0000shadow","state":"/var/lib/capsid","secrets":"/var/lib/capsid/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock"},"listeners":[{"name":"public","tcp":"127.0.0.1:8080","routing":{"mode":"path"}}]})json",
        ConfigErrorCode::kInvalidValue, "/paths/applications",
        "NUL in a general configuration string");

    const std::string exact_limit_value(16U * 1024U, 'x');
    require_valid(
        ConfigDocument::kApplication,
        app_with_environment_entry("LARGE_VALUE", "{\"value\":\"" + exact_limit_value + "\"}"),
        "literal environment value exactly at the limit");
    const std::string over_limit_value(16U * 1024U + 1U, 'x');
    require_error(
        ConfigDocument::kApplication,
        app_with_environment_entry("LARGE_VALUE", "{\"value\":\"" + over_limit_value + "\"}"),
        ConfigErrorCode::kResourceLimit, "/permissions/env/LARGE_VALUE/value",
        "literal environment value over the limit");

    std::string too_many = R"json({"apiVersion":"capsid/app-v2","permissions":{"env":{)json";
    for (std::size_t i = 0; i < 257; ++i) {
        if (i != 0) {
            too_many += ',';
        }
        too_many += "\"ENV_" + std::to_string(i) + "\":{\"value\":\"x\"}";
    }
    too_many += R"json(}},"pool":{"workers":1}})json";
    require_error(ConfigDocument::kApplication, too_many, ConfigErrorCode::kResourceLimit,
                  "/permissions/env", "too many App environment entries");
}

void test_network_namespace_is_not_in_either_schema() {
    require_unknown_field(ConfigDocument::kHost,
                          host_v2_with(R"json("networkNamespace":"inherit")json"),
                          "/networkNamespace", "Host root networkNamespace");
    require_unknown_field(ConfigDocument::kHost,
                          host_v2_with(R"json("isolation":{"networkNamespace":"inherit"})json"),
                          "/isolation/networkNamespace", "Host isolation networkNamespace");
    require_unknown_field(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","entry":"bundle.mjs","pool":{"workers":1},"networkNamespace":"inherit"})json",
        "/networkNamespace", "App root networkNamespace");
    require_unknown_field(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","entry":"bundle.mjs","worker":{"networkNamespace":"inherit"},"pool":{"workers":1}})json",
        "/worker/networkNamespace", "App worker networkNamespace");
}

void test_unrelated_unknown_fields_use_the_same_fail_closed_path() {
    require_unknown_field(ConfigDocument::kHost, host_v2_with(R"json("mystery":true)json"),
                          "/mystery", "Host unrelated unknown field");
    require_unknown_field(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","entry":"bundle.mjs","pool":{"workers":1,"elastic":true}})json",
        "/pool/elastic", "App pool unrelated unknown field");
}

void test_json_envelope_is_strict() {
    require_error(ConfigDocument::kHost,
                  R"json({"apiVersion":"capsid/host-v2","apiVersion":"capsid/host-v2"})json",
                  ConfigErrorCode::kDuplicateKey, "", "duplicate Host root key");
    require_error(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","pool":{"minReady":1,"minReady":1,"maxWorkers":1}})json",
        ConfigErrorCode::kDuplicateKey, "", "duplicate nested App key");
    require_error(ConfigDocument::kHost, R"json({"apiVersion":"capsid/host-v2",})json",
                  ConfigErrorCode::kInvalidJson, "", "trailing comma");
    require_error(ConfigDocument::kHost, R"json({"apiVersion":"capsid/host-v2" /* comment */})json",
                  ConfigErrorCode::kInvalidJson, "", "JSON comment");
    require_error(ConfigDocument::kHost, R"json({"apiVersion":"capsid/host-v2"} trailing)json",
                  ConfigErrorCode::kInvalidJson, "", "trailing JSON input");
    require_error(ConfigDocument::kHost, R"json({"apiVersion":NaN})json",
                  ConfigErrorCode::kInvalidJson, "", "non-standard NaN");
    require_error(ConfigDocument::kHost, "[]", ConfigErrorCode::kInvalidValue, "",
                  "non-object Host root");
}

void test_api_versions_are_exact() {
    require_error(ConfigDocument::kHost, "{}", ConfigErrorCode::kInvalidValue, "/apiVersion",
                  "missing Host apiVersion");
    require_error(ConfigDocument::kApplication, R"json({"pool":{"workers":1}})json",
                  ConfigErrorCode::kInvalidValue, "/apiVersion", "missing App apiVersion");
    require_error(ConfigDocument::kHost, R"json({"apiVersion":7})json",
                  ConfigErrorCode::kInvalidValue, "/apiVersion", "non-string Host apiVersion");
    // Config v1 (capsid/host-v1 and capsid/app-v1) is retired; only v2 is accepted.
    require_error(ConfigDocument::kHost, R"json({"apiVersion":"capsid/host-v1"})json",
                  ConfigErrorCode::kInvalidValue, "/apiVersion", "retired Host v1 apiVersion");
    require_error(ConfigDocument::kApplication,
                  R"json({"apiVersion":"capsid/app-v1","pool":{"minReady":1,"maxWorkers":1}})json",
                  ConfigErrorCode::kInvalidValue, "/apiVersion", "retired App v1 apiVersion");
    require_error(ConfigDocument::kHost, R"json({"apiVersion":"capsid/host-v3"})json",
                  ConfigErrorCode::kInvalidValue, "/apiVersion", "unsupported Host apiVersion");
    require_error(ConfigDocument::kApplication,
                  R"json({"apiVersion":"capsid/app-v3","pool":{"workers":1}})json",
                  ConfigErrorCode::kInvalidValue, "/apiVersion", "unsupported App apiVersion");
    require_error(ConfigDocument::kHost, R"json({"apiVersion":"capsid/app-v2"})json",
                  ConfigErrorCode::kInvalidValue, "/apiVersion",
                  "App apiVersion used as Host config");
    {
        const std::string legacy_app =
            R"json({"apiVersion":"capsid/app-v1","pool":{"minReady":1,"maxWorkers":1}})json";
        capsid::host::AppRequest parsed;
        std::string error;
        require(
            !capsid::host::parse_app_request(
                std::vector<std::uint8_t>(legacy_app.begin(), legacy_app.end()), &parsed, &error),
            "parse_app_request accepted retired capsid/app-v1");
    }
}

void test_unknown_fields_precede_value_validation() {
    require_unknown_field(ConfigDocument::kHost, R"json({"apiVersion":7,"mystery":true})json",
                          "/mystery", "Host unknown field beside invalid value");
    require_unknown_field(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","pool":{"minReady":"one","maxWorkers":1,"elastic":true}})json",
        "/pool/elastic", "nested App unknown field beside invalid value");
}

void test_pool_bounds_are_validated() {
    require_error(ConfigDocument::kApplication,
                  R"json({"apiVersion":"capsid/app-v2","pool":{"workers":0}})json",
                  ConfigErrorCode::kInvalidValue, "/pool/workers", "zero-sized App pool.workers");
    require_error(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","pool":{"minReady":-1,"maxWorkers":-1}})json",
        ConfigErrorCode::kInvalidValue, "/pool/minReady", "negative App pool size");
    require_error(ConfigDocument::kApplication,
                  R"json({"apiVersion":"capsid/app-v2","pool":{"minReady":1,"maxWorkers":0}})json",
                  ConfigErrorCode::kInvalidValue, "/pool/maxWorkers", "zero App maxWorkers");
    require_error(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","pool":{"minReady":1.5,"maxWorkers":1}})json",
        ConfigErrorCode::kInvalidValue, "/pool/minReady", "fractional App minReady");
    require_error(ConfigDocument::kApplication,
                  R"json({"apiVersion":"capsid/app-v2","pool":{"minReady":2,"maxWorkers":1}})json",
                  ConfigErrorCode::kInvalidValue, "/pool/minReady", "inverted App pool bounds");
    require_valid(ConfigDocument::kApplication,
                  R"json({"apiVersion":"capsid/app-v2","pool":{"minReady":4,"maxWorkers":4}})json",
                  "four-worker App pool");
    require_error(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","pool":{"minReady":4097,"maxWorkers":4097}})json",
        ConfigErrorCode::kResourceLimit, "/pool/minReady",
        "App pool beyond the fixed worker limit");
}

std::string host_document_with_array_nesting(std::size_t array_count) {
    std::string document = R"json({"mystery":)json";
    document.append(array_count, '[');
    document += "null";
    document.append(array_count, ']');
    document += '}';
    return document;
}

void test_config_resource_limits_precede_json_dom_work() {
    const std::string minimal_host = host_v2_with();
    require(minimal_host.size() < kMaxConfigBytes,
            "minimal Host fixture exceeds the configured byte limit");

    std::string at_byte_limit = minimal_host;
    at_byte_limit.resize(kMaxConfigBytes, ' ');
    require_valid(ConfigDocument::kHost, at_byte_limit, "Host config exactly at the byte limit");

    std::string over_byte_limit = at_byte_limit;
    over_byte_limit.push_back(' ');
    require_error(ConfigDocument::kHost, over_byte_limit, ConfigErrorCode::kResourceLimit, "",
                  "Host config over the byte limit");

    std::string over_app_byte_limit =
        R"json({"apiVersion":"capsid/app-v2","pool":{"workers":1}})json";
    over_app_byte_limit.resize(kMaxConfigBytes + 1, ' ');
    require_error(ConfigDocument::kApplication, over_app_byte_limit,
                  ConfigErrorCode::kResourceLimit, "", "App config over the byte limit");

    // The root object and innermost null leaf each consume one value level.
    // The remaining levels may be arrays; reaching the declared parser limit
    // must still proceed to the ordinary unknown-field phase.
    require(kMaxConfigNesting >= 3, "JSON nesting limit is too small for an object member");
    const std::string at_nesting_limit = host_document_with_array_nesting(kMaxConfigNesting - 2);
    require_unknown_field(ConfigDocument::kHost, at_nesting_limit, "/mystery",
                          "Host config exactly at the JSON nesting limit");

    const std::string over_nesting_limit = host_document_with_array_nesting(kMaxConfigNesting - 1);
    require_error(ConfigDocument::kHost, over_nesting_limit, ConfigErrorCode::kResourceLimit, "",
                  "Host config over the JSON nesting limit");

    std::string bracket_text(kMaxConfigNesting * 2, '[');
    bracket_text.append(kMaxConfigNesting * 2, ']');
    const std::string app_with_brackets_in_string =
        R"json({"apiVersion":"capsid/app-v2","permissions":{"env":{"BRACKETS":{"value":")json" +
        bracket_text + R"json("}}},"pool":{"workers":1}})json";
    require_valid(ConfigDocument::kApplication, app_with_brackets_in_string,
                  "brackets inside an App string");
}

void test_request_timeout_reaches_typed_app_request() {
    capsid::host::AppRequest app;
    std::string error;
    const auto document = [](const std::string& request) {
        const std::string text =
            std::string(
                R"json({"apiVersion":"capsid/app-v2","entry":"bundle.mjs","pool":{"workers":1},"request":)json") +
            request + "}";
        return std::vector<std::uint8_t>(text.begin(), text.end());
    };
    require(capsid::host::parse_app_request(document(R"({"timeout":"120ms"})"), &app, &error),
            "typed request timeout rejected: " + error);
    require(app.request_timeout_ms == 120,
            "accepted request.timeout was discarded by typed parsing");
    for (const auto& duration :
         std::vector<std::string>{"0ms", "invalid", "9223372036854775808ms"}) {
        require(!capsid::host::parse_app_request(document("{\"timeout\":\"" + duration + "\"}"),
                                                 &app, &error),
                "invalid request timeout accepted: " + duration);
    }
    app = {};
    require(capsid::host::parse_app_request(document("{}"), &app, &error),
            "omitted request timeout rejected");
    require(app.request_timeout_ms == 0, "omitted request timeout acquired an override");
}

void test_ipv6_fetch_authorities_reach_the_app_policy() {
    using capsid::host::FetchTarget;
    using capsid::host::parse_outbound_target_text;
    const auto document = [](const std::string& authority) {
        const auto text =
            std::string(
                R"({"apiVersion":"capsid/app-v2","entry":"bundle.mjs","pool":{"workers":1},"permissions":{"fetch":{"allow":[")") +
            authority + R"("]}}})";
        return std::vector<std::uint8_t>(text.begin(), text.end());
    };
    for (const auto& authority : std::vector<std::string>{
             "[::1]:443,8443", "[0000:0000:0000:0000:0000:0000:0000:0001]:443,8443"}) {
        capsid::host::AppRequest app;
        std::string error;
        require(capsid::host::parse_app_request(document(authority), &app, &error),
                "bracketed IPv6 App fetch authority rejected: " + error);
        require(app.fetch.size() == 1 && app.fetch[0].host == "::1" &&
                    app.fetch[0].ports == std::vector<std::uint16_t>({443, 8443}),
                "IPv6 fetch authority or explicit ports changed in App policy");
    }
    FetchTarget target;
    require(parse_outbound_target_text("[::1]", &target) && target.host == "::1" &&
                target.ports.empty(),
            "bare bracketed IPv6 authority lost any-port semantics");
    require(parse_outbound_target_text("api.example:443", &target) &&
                target.host == "api.example" && target.ports == std::vector<std::uint16_t>({443}),
            "existing hostname authority changed");
    require(parse_outbound_target_text("api.example:0000443", &target) &&
                target.ports == std::vector<std::uint16_t>({443}),
            "existing decimal leading-zero port changed");
    {
        capsid::host::AppRequest cidr_app;
        std::string cidr_error;
        require(capsid::host::parse_app_request(document("[::1/128]:443"), &cidr_app, &cidr_error),
                "v2 App fetch.allow rejected valid IPv6 CIDR target: " + cidr_error);
    }
    for (const auto& authority :
         std::vector<std::string>{"",
                                  "::1",
                                  "[::1",
                                  "[localhost]:443",
                                  "[::1]extra:443",
                                  "[::1]:",
                                  "[::1]:0",
                                  "[::1]:65536",
                                  "[::1]:443,",
                                  "[::1]:443,,8443",
                                  "[::1]: 443",
                                  "[::1]:+443",
                                  "[::1]:-1",
                                  "[fe80::1%1]:443",
                                  "[fe80::1%lo0]:443",
                                  "[fe80::1%25lo0]:443",
                                  "[::ffff:127.0.0.1%1]:443",
                                  "[::1%1]",
                                  "[::1/129]:443",
                                  "api.example:443,",
                                  std::string("[::1") + '\0' + "ignored]:443",
                                  std::string("api.example") + '\0' + ":443"}) {
        FetchTarget retained{"unchanged.example", {123}};
        require(!parse_outbound_target_text(authority, &retained) &&
                    retained.host == "unchanged.example" &&
                    retained.ports == std::vector<std::uint16_t>({123}),
                "malformed fetch authority changed the policy destination: " + authority);
        capsid::host::AppRequest app;
        std::string error;
        require(!capsid::host::parse_app_request(document(authority), &app, &error),
                "malformed App fetch authority accepted: " + authority);
    }
}

void test_v2_orthogonal_host_and_app_validation() {
    require_valid(ConfigDocument::kHost,
                  R"json({
          "apiVersion":"capsid/host-v2",
          "paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/var/lib/capsid/secrets/{application}"},
          "admin":{"unix":"/run/capsid/admin.sock"},
          "listeners":[{"name":"public","tcp":"0.0.0.0:8080","routing":{"mode":"path"}}]
        })json",
                  "minimal orthogonal Host v2");
    require_valid(ConfigDocument::kHost,
                  R"json({
          "apiVersion":"capsid/host-v2",
          "paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/var/lib/capsid/secrets/{application}","bindings":"/etc/capsid/bindings"},
          "admin":{"unix":"/run/capsid/admin.sock"},
          "isolation":{"mode":"strict","required":["cgroup-v2"],"cgroupRoot":"/sys/fs/cgroup/capsid"},
          "trustedBytecodeKeys":{"2026-11":"/etc/capsid/keys/ed25519.pub"},
          "listeners":[{
            "name":"public",
            "tcp":"0.0.0.0:8080",
            "publicOrigin":"https://apps.example.com",
            "trusted":false,
            "routing":{"mode":"subdomain","suffix":".apps.example.com"},
            "limits":{"connections":512,"headerSize":"32KiB","headerTimeout":"5s","bodyIdleTimeout":"30s","streamIdleTimeout":"60s","writeTimeout":"10s","channels":128},
            "cors":{"allowedOrigins":["https://dev.example.com"],"allowedMethods":["GET","POST"],"allowedHeaders":["content-type"],"maxAge":"86400s"}
          }],
          "permissions":{
            "modules":["capsid:env","capsid:fs"],
            "environmentNames":["APP_MODE","DATABASE_*"],
            "fsReadRoots":["/srv/config"],
            "fetchTargets":["api.example.com:443","10.0.0.0/8:3306,3307","[2001:db8::/32]:443"],
            "storageNamespaces":["session"],
            "stdioStreams":["stdout","stderr"]
          },
          "defaults":{"worker":{"memoryMax":"256MiB","jsHeap":"64MiB","fileDescriptors":64},"request":{"timeout":"5s","maxInflight":64},"pool":{"workers":2,"queueRequests":0,"queueTimeout":"10s"}},
          "maximums":{"worker":{"memoryMax":"512MiB","jsHeap":"256MiB","fileDescriptors":256},"request":{"timeout":"30s","maxInflight":128},"pool":{"workers":16,"queueRequests":1024,"queueTimeout":"30s"}},
          "capacity":{"workersTotal":64,"channelsTotal":1024,"maxChannelMessageSize":"1MiB","channelIdleTimeout":"60s"},
          "recovery":{"crashBudget":{"maxEvents":5,"window":"60s"},"healthCheck":{"interval":"30s","failures":3}}
        })json",
                  "maximal orthogonal Host v2");
    require_valid(ConfigDocument::kApplication, R"json({"apiVersion":"capsid/app-v2"})json",
                  "minimal orthogonal App v2");
    require_valid(ConfigDocument::kApplication,
                  R"json({
          "apiVersion":"capsid/app-v2",
          "entry":"bundle.mjs",
          "permissions":{
            "modules":["capsid:env"],
            "env":{"APP_MODE":{"value":"prod"},"DB":{"valueFrom":"db-url"}},
            "fs":{"read":{"allow":["/srv/config"],"deny":["/srv/config/secret.json"]}},
            "fetch":{"allow":["api.example.com:443,8443","10.0.0.0/8"]},
            "storage":{"namespaces":["session"]},
            "stdio":["stdout"]
          },
          "worker":{"memoryMax":"256MiB","jsHeap":"64MiB","fileDescriptors":64},
          "request":{"timeout":"5s","maxInflight":64},
          "pool":{"workers":2,"queueRequests":0,"queueTimeout":"10s"},
          "healthCheck":{"path":"/health","timeout":"2s"}
        })json",
                  "maximal orthogonal App v2");
    require_error(
        ConfigDocument::kHost,
        R"json({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock"},"listeners":[]})json",
        ConfigErrorCode::kInvalidValue, "/listeners", "Host v2 rejects empty listeners");
    require_unknown_field(
        ConfigDocument::kHost,
        R"json({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock","mode":"0600"},"listeners":[{"name":"p","tcp":"127.0.0.1:8080","routing":{"mode":"path"}}]})json",
        "/admin/mode", "Host v2 rejects removed admin.mode");
    require_unknown_field(ConfigDocument::kApplication,
                          R"json({"apiVersion":"capsid/app-v2","tenant":"acme"})json", "/tenant",
                          "App v2 rejects tenant");
    require_unknown_field(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","worker":{"processAddressSpace":"256MiB"}})json",
        "/worker/processAddressSpace", "App v2 rejects removed worker.processAddressSpace");
    require_valid(ConfigDocument::kApplication,
                  R"json({"apiVersion":"capsid/app-v2","pool":{"minReady":1,"maxWorkers":1}})json",
                  "App v2 elastic fixed-bounds pool");
    require_valid(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","pool":{"minReady":0,"maxWorkers":8,"idleTimeout":"60s"}})json",
        "App v2 scale-to-zero elastic pool");
    require_error(ConfigDocument::kApplication,
                  R"json({"apiVersion":"capsid/app-v2","pool":{"workers":2,"minReady":1}})json",
                  ConfigErrorCode::kInvalidValue, "/pool",
                  "App v2 rejects mixing workers with minReady");
    require_error(
        ConfigDocument::kApplication,
        R"json({"apiVersion":"capsid/app-v2","pool":{"workers":2,"idleTimeout":"30s"}})json",
        ConfigErrorCode::kInvalidValue, "/pool", "App v2 rejects mixing workers with idleTimeout");
    require_error(ConfigDocument::kApplication,
                  R"json({"apiVersion":"capsid/app-v2","pool":{"minReady":4,"maxWorkers":2}})json",
                  ConfigErrorCode::kInvalidValue, "/pool/minReady",
                  "App v2 rejects minReady > maxWorkers");
    require_error(
        ConfigDocument::kHost,
        R"json({"apiVersion":"capsid/host-v2","paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/secrets/{application}"},"admin":{"unix":"/run/capsid/admin.sock"},"listeners":[{"name":"p","tcp":"127.0.0.1:8080","routing":{"mode":"path"}}],"defaults":{"pool":{"workers":2,"minReady":1}}})json",
        ConfigErrorCode::kInvalidValue, "/defaults/pool",
        "Host v2 rejects mixing defaults.pool.workers with minReady");
    require_error(
        ConfigDocument::kApplication, R"json({"apiVersion":"capsid/app-v2","healthCheck":{}})json",
        ConfigErrorCode::kInvalidValue, "/healthCheck/path", "App v2 healthCheck requires path");
    require_error(ConfigDocument::kApplication,
                  R"json({"apiVersion":"capsid/app-v2","pool":{"workers":4097}})json",
                  ConfigErrorCode::kResourceLimit, "/pool/workers",
                  "App v2 pool.workers exceeds limit");
    // Stage 3: host-v2 capacity.memory (string shorthand + object) & tenancy.
    require_valid(ConfigDocument::kHost,
                  R"json({
          "apiVersion":"capsid/host-v2",
          "paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/secrets/{application}"},
          "admin":{"unix":"/run/capsid/admin.sock"},
          "listeners":[{"name":"p","tcp":"127.0.0.1:8080","routing":{"mode":"path"}}],
          "capacity":{"workersTotal":64,"memory":"2GiB"},
          "tenancy":{
            "defaultQuota":{"maxWorkers":16,"hardMemory":"512MiB"},
            "overrides":{"vip-acme":{"maxWorkers":64,"minSoftMemory":"256MiB","hardMemory":"2GiB"}}
          }
        })json",
                  "Host v2 capacity.memory string + tenancy");
    require_valid(ConfigDocument::kHost,
                  R"json({
          "apiVersion":"capsid/host-v2",
          "paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/secrets/{application}"},
          "admin":{"unix":"/run/capsid/admin.sock"},
          "listeners":[{"name":"p","tcp":"127.0.0.1:8080","routing":{"mode":"path"}}],
          "capacity":{"workersTotal":64,"memory":{"limit":"2GiB","highWatermarkPercent":90,"lowWatermarkPercent":70,"minResidency":"0s","checkInterval":"2s"}}
        })json",
                  "Host v2 capacity.memory object");
    require_error(ConfigDocument::kHost,
                  R"json({
          "apiVersion":"capsid/host-v2",
          "paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/secrets/{application}"},
          "admin":{"unix":"/run/capsid/admin.sock"},
          "listeners":[{"name":"p","tcp":"127.0.0.1:8080","routing":{"mode":"path"}}],
          "capacity":{"workersTotal":64,"memory":{"limit":"2GiB","highWatermarkPercent":80,"lowWatermarkPercent":80}}
        })json",
                  ConfigErrorCode::kInvalidValue, "/capacity/memory/lowWatermarkPercent",
                  "Host v2 rejects lowWatermarkPercent >= highWatermarkPercent");
    require_error(ConfigDocument::kHost,
                  R"json({
          "apiVersion":"capsid/host-v2",
          "paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/secrets/{application}"},
          "admin":{"unix":"/run/capsid/admin.sock"},
          "listeners":[{"name":"p","tcp":"127.0.0.1:8080","routing":{"mode":"path"}}],
          "tenancy":{"overrides":{"Bad_Tenant":{"maxWorkers":4}}}
        })json",
                  ConfigErrorCode::kInvalidValue, "/tenancy/overrides/Bad_Tenant",
                  "Host v2 rejects invalid tenant ID grammar");
    require_error(ConfigDocument::kHost,
                  R"json({
          "apiVersion":"capsid/host-v2",
          "paths":{"applications":"/srv/apps","state":"/var/lib/capsid","secrets":"/secrets/{application}"},
          "admin":{"unix":"/run/capsid/admin.sock"},
          "listeners":[{"name":"p","tcp":"127.0.0.1:8080","routing":{"mode":"path"}}],
          "tenancy":{"defaultQuota":{"minSoftMemory":"512MiB","hardMemory":"256MiB"}}
        })json",
                  ConfigErrorCode::kInvalidValue, "/tenancy/defaultQuota/minSoftMemory",
                  "Host v2 rejects minSoftMemory > hardMemory");
}

} // namespace

int main() {
    test_minimal_documents_are_valid();
    test_complete_v2_document_shapes_remain_recursively_fail_closed();
    test_listener_public_origin_is_typed();
    test_complete_v2_collections_and_limits_are_typed();
    test_environment_request_schema_is_fail_closed();
    test_network_namespace_is_not_in_either_schema();
    test_unrelated_unknown_fields_use_the_same_fail_closed_path();
    test_json_envelope_is_strict();
    test_api_versions_are_exact();
    test_unknown_fields_precede_value_validation();
    test_pool_bounds_are_validated();
    test_config_resource_limits_precede_json_dom_work();
    test_request_timeout_reaches_typed_app_request();
    test_ipv6_fetch_authorities_reach_the_app_policy();
    test_v2_orthogonal_host_and_app_validation();
    return 0;
}
