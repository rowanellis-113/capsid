#include "win32_compat.h"

#include "ipc_validation.h"

#include "capsid/runtime.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <limits>

namespace {

bool reject(std::string* error, const char* message) {
    if (error) {
        *error = message;
    }
    return false;
}

bool contains_nul(const std::string& value) { return value.find('\0') != std::string::npos; }

// --- Binding v1 wire grammars (docs/design/bindings-v1.md §2/§4) ---

// Defined below; the binding blob parser precedes them in this file.
bool read_string16(const uint8_t** cursor, const uint8_t* end, std::string* value);
bool read_string32(const uint8_t** cursor, const uint8_t* end, std::string* value);
//
// The worker validates every LOAD_BINDING descriptor defensively even
// though the Host config layer (host/config.cc) already rejected it: the
// wire is the last trust boundary before Binding code. These grammars must
// stay in sync with the Host schema authority.

const size_t kMaxBindingDescriptorBytes = 1024U * 1024U;   // config + lists
const size_t kMaxBindingSourceBytes = 16U * 1024U * 1024U; // index.js
const size_t kMaxBindingConfigBytes = 256U * 1024U;
const size_t kMaxBindingSecrets = 64;
const size_t kMaxBindingSecretValueBytes = 256U * 1024U;
const size_t kMaxBindingModules = 64;
const size_t kMaxBindingEnvEntries = 256;
const size_t kMaxBindingStdioEntries = 3;
const size_t kMaxBindingPathBytes = 4096;

// [a-z][a-z0-9-]{0,62}, 1..63 bytes.
bool valid_binding_name(const std::string& value) {
    if (value.empty() || value.size() > 63 || value[0] < 'a' || value[0] > 'z') {
        return false;
    }
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-')) {
            return false;
        }
    }
    return true;
}

// [A-Za-z0-9._-], no empty component, 1..256 bytes (mirrors the Host
// secret-key grammar; a stricter worker-side bound is fine).
bool valid_binding_secret_key(const std::string& value) {
    if (value.empty() || value.size() > 256 || value.find("..") != std::string::npos) {
        return false;
    }
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (!(std::isalnum(ch) || ch == '.' || ch == '_' || ch == '-')) {
            return false;
        }
    }
    return true;
}

bool valid_binding_path(const std::string& value) {
    return !value.empty() && value[0] == '/' && value.size() <= kMaxBindingPathBytes &&
           !contains_nul(value);
}

bool valid_binding_env_name(const std::string& value) {
    if (value.empty() || value.size() > 256 || value.find('*') != std::string::npos) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(value[0]);
    if (!(std::isalpha(first) || first == '_')) {
        return false;
    }
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (!(std::isalnum(ch) || ch == '_')) {
            return false;
        }
    }
    return true;
}

bool valid_binding_stdio(const std::string& value) {
    return value == "stdin" || value == "stdout" || value == "stderr";
}

bool valid_binding_port(const std::string& port) {
    if (port.empty() || port.size() > 5) {
        return false;
    }
    unsigned int value = 0;
    for (size_t i = 0; i < port.size(); ++i) {
        const char c = port[i];
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + static_cast<unsigned int>(c - '0');
    }
    return value >= 1 && value <= 65535;
}

bool valid_binding_hostname(const std::string& host) {
    size_t start = 0;
    if (host.size() >= 2 && host[0] == '*' && host[1] == '.') {
        start = 2;
    } else if (host.find('*') != std::string::npos) {
        return false;
    }
    if (start == host.size() || host.size() > 253) {
        return false;
    }
    for (size_t index = start; index <= host.size(); ++index) {
        if (index < host.size() && host[index] != '.') {
            continue;
        }
        const size_t label_size = index - start;
        if (label_size == 0 || label_size > 63 || host[start] == '-' || host[index - 1] == '-') {
            return false;
        }
        for (size_t label_index = start; label_index < index; ++label_index) {
            const unsigned char ch = static_cast<unsigned char>(host[label_index]);
            if (!(std::isalnum(ch) || ch == '-')) {
                return false;
            }
        }
        start = index + 1;
    }
    return true;
}

bool valid_binding_numeric_host(const std::string& host) {
    const size_t slash = host.find('/');
    const std::string address = host.substr(0, slash);
    if (slash != std::string::npos) {
        unsigned int prefix = 0;
        const std::string prefix_text = host.substr(slash + 1);
        if (prefix_text.empty()) {
            return false;
        }
        for (size_t i = 0; i < prefix_text.size(); ++i) {
            const char c = prefix_text[i];
            if (c < '0' || c > '9') {
                return false;
            }
            prefix = prefix * 10 + static_cast<unsigned int>(c - '0');
            if (prefix > 32) {
                return false;
            }
        }
    }
    struct in_addr parsed;
    return inet_pton(AF_INET, address.c_str(), &parsed) == 1;
}

bool valid_binding_host(const std::string& host) {
    if (host.empty()) {
        return false;
    }
    if (host == "*") {
        return true;
    }
    bool numeric = true;
    for (size_t i = 0; i < host.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(host[i]);
        if (!(std::isdigit(ch) || ch == '.' || ch == '/')) {
            numeric = false;
            break;
        }
    }
    if (numeric) {
        return valid_binding_numeric_host(host);
    }
    return valid_binding_hostname(host);
}

bool valid_binding_net_target(const std::string& target) {
    if (target.empty()) {
        return false;
    }
    if (target[0] == '[') {
        const size_t close = target.find(']');
        if (close == std::string::npos || close + 1 >= target.size() || target[close + 1] != ':') {
            return false;
        }
        const std::string inner = target.substr(1, close - 1);
        const size_t slash = inner.find('/');
        struct in6_addr parsed;
        if (inet_pton(AF_INET6, inner.substr(0, slash).c_str(), &parsed) != 1) {
            return false;
        }
        if (slash != std::string::npos) {
            unsigned int prefix = 0;
            for (size_t i = slash + 1; i < inner.size(); ++i) {
                const char c = inner[i];
                if (c < '0' || c > '9') {
                    return false;
                }
                prefix = prefix * 10 + static_cast<unsigned int>(c - '0');
                if (prefix > 128) {
                    return false;
                }
            }
        }
        return valid_binding_port(target.substr(close + 2));
    }
    const size_t colon = target.find(':');
    if (colon == std::string::npos || target.find(':', colon + 1) != std::string::npos) {
        return false;
    }
    return valid_binding_host(target.substr(0, colon)) &&
           valid_binding_port(target.substr(colon + 1));
}

// u32 count followed by count x string16 items, each validated, non-empty
// of duplicates. Error text is static and never contains item content.
typedef bool (*BindingItemCheck)(const std::string&);

bool read_binding_list(const uint8_t** cursor, const uint8_t* end, std::vector<std::string>* out,
                       size_t max_items, BindingItemCheck check, const char* error_label,
                       std::string* error) {
    uint32_t count = 0;
    if (!capsid::protocol::read_u32(cursor, end, &count) || count > max_items) {
        return reject(error, "invalid binding list count");
    }
    out->clear();
    for (uint32_t i = 0; i < count; ++i) {
        std::string item;
        if (!read_string16(cursor, end, &item) || contains_nul(item) || !check(item) ||
            std::find(out->begin(), out->end(), item) != out->end()) {
            return reject(error, (std::string("invalid binding ") + error_label).c_str());
        }
        out->push_back(item);
    }
    return true;
}

bool is_valid_utf8_bytes(const std::string& text) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(text.data());
    const size_t n = text.size();
    size_t i = 0;
    while (i < n) {
        const unsigned char b0 = bytes[i];
        if (b0 <= 0x7Fu) {
            ++i;
            continue;
        }
        if (b0 >= 0xC2u && b0 <= 0xDFu) {
            if (i + 1 >= n || (bytes[i + 1] & 0xC0u) != 0x80u) {
                return false;
            }
            i += 2;
            continue;
        }
        if (b0 >= 0xE0u && b0 <= 0xEFu) {
            if (i + 2 >= n || (bytes[i + 1] & 0xC0u) != 0x80u || (bytes[i + 2] & 0xC0u) != 0x80u) {
                return false;
            }
            const unsigned char b1 = bytes[i + 1];
            if ((b0 == 0xE0u && b1 < 0xA0u) || (b0 == 0xEDu && b1 >= 0xA0u)) {
                return false;
            }
            i += 3;
            continue;
        }
        if (b0 >= 0xF0u && b0 <= 0xF4u) {
            if (i + 3 >= n || (bytes[i + 1] & 0xC0u) != 0x80u || (bytes[i + 2] & 0xC0u) != 0x80u ||
                (bytes[i + 3] & 0xC0u) != 0x80u) {
                return false;
            }
            const unsigned char b1 = bytes[i + 1];
            if ((b0 == 0xF0u && b1 < 0x90u) || (b0 == 0xF4u && b1 > 0x8Fu)) {
                return false;
            }
            i += 4;
            continue;
        }
        return false;
    }
    return true;
}

struct BindingConfigJsonParser {
    const std::string& s;
    size_t pos;
    size_t depth;

    explicit BindingConfigJsonParser(const std::string& input) : s(input), pos(0), depth(0) {}

    void skip_ws() {
        while (pos < s.size()) {
            const char c = s[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos;
            } else {
                break;
            }
        }
    }

    static bool hex4(const char* p, uint16_t* out) {
        uint16_t v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = p[i];
            int d = -1;
            if (c >= '0' && c <= '9')
                d = c - '0';
            else if (c >= 'a' && c <= 'f')
                d = 10 + (c - 'a');
            else if (c >= 'A' && c <= 'F')
                d = 10 + (c - 'A');
            else
                return false;
            v = static_cast<uint16_t>((v << 4) | static_cast<uint16_t>(d));
        }
        *out = v;
        return true;
    }

    bool parse_string() {
        if (pos >= s.size() || s[pos] != '"') {
            return false;
        }
        ++pos;
        while (pos < s.size()) {
            const unsigned char c = static_cast<unsigned char>(s[pos++]);
            if (c == '"') {
                return true;
            }
            if (c < 0x20u) {
                return false;
            }
            if (c == '\\') {
                if (pos >= s.size()) {
                    return false;
                }
                const char esc = s[pos++];
                switch (esc) {
                case '"':
                case '\\':
                case '/':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                    break;
                case 'u': {
                    if (s.size() - pos < 4) {
                        return false;
                    }
                    uint16_t cp = 0;
                    if (!hex4(&s[pos], &cp) || cp == 0) {
                        return false;
                    }
                    pos += 4;
                    if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                        return false; // unpaired low surrogate
                    }
                    if (cp >= 0xD800u && cp <= 0xDBFFu) {
                        if (s.size() - pos < 6 || s[pos] != '\\' || s[pos + 1] != 'u') {
                            return false; // unpaired high surrogate
                        }
                        uint16_t low = 0;
                        if (!hex4(&s[pos + 2], &low) || low < 0xDC00u || low > 0xDFFFu) {
                            return false;
                        }
                        pos += 6;
                    }
                    break;
                }
                default:
                    return false;
                }
            }
        }
        return false;
    }

    bool parse_number() {
        const size_t start = pos;
        bool negative = false;
        if (pos < s.size() && s[pos] == '-') {
            negative = true;
            ++pos;
        }
        if (pos >= s.size()) {
            return false;
        }
        const size_t int_start = pos;
        if (s[pos] == '0') {
            ++pos;
            if (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
                return false;
            }
        } else if (s[pos] >= '1' && s[pos] <= '9') {
            ++pos;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
                ++pos;
            }
        } else {
            return false;
        }
        const size_t int_end = pos;
        bool is_float = false;
        if (pos < s.size() && s[pos] == '.') {
            is_float = true;
            ++pos;
            if (pos >= s.size() || s[pos] < '0' || s[pos] > '9') {
                return false;
            }
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
                ++pos;
            }
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            is_float = true;
            ++pos;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) {
                ++pos;
            }
            if (pos >= s.size() || s[pos] < '0' || s[pos] > '9') {
                return false;
            }
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
                ++pos;
            }
        }
        if (pos == start) {
            return false;
        }
        if (!is_float) {
            (void)negative;
            static const uint64_t kMaxSafeInt = UINT64_C(9007199254740991);
            uint64_t mag = 0;
            for (size_t i = int_start; i < int_end; ++i) {
                const uint64_t d = static_cast<uint64_t>(s[i] - '0');
                if (mag > (kMaxSafeInt - d) / UINT64_C(10)) {
                    return false;
                }
                mag = mag * UINT64_C(10) + d;
            }
        }
        return true;
    }

    bool parse_value() {
        skip_ws();
        if (pos >= s.size()) {
            return false;
        }
        const char c = s[pos];
        if (c == '{') {
            if (++depth > 64) {
                return false;
            }
            ++pos;
            skip_ws();
            if (pos < s.size() && s[pos] == '}') {
                ++pos;
                --depth;
                return true;
            }
            for (;;) {
                skip_ws();
                if (!parse_string()) {
                    return false;
                }
                skip_ws();
                if (pos >= s.size() || s[pos] != ':') {
                    return false;
                }
                ++pos;
                if (!parse_value()) {
                    return false;
                }
                skip_ws();
                if (pos < s.size() && s[pos] == ',') {
                    ++pos;
                    continue;
                }
                if (pos < s.size() && s[pos] == '}') {
                    ++pos;
                    --depth;
                    return true;
                }
                return false;
            }
        }
        if (c == '[') {
            if (++depth > 64) {
                return false;
            }
            ++pos;
            skip_ws();
            if (pos < s.size() && s[pos] == ']') {
                ++pos;
                --depth;
                return true;
            }
            for (;;) {
                if (!parse_value()) {
                    return false;
                }
                skip_ws();
                if (pos < s.size() && s[pos] == ',') {
                    ++pos;
                    continue;
                }
                if (pos < s.size() && s[pos] == ']') {
                    ++pos;
                    --depth;
                    return true;
                }
                return false;
            }
        }
        if (c == '"') {
            return parse_string();
        }
        if (c == 't') {
            if (s.size() - pos >= 4 && s.compare(pos, 4, "true") == 0) {
                pos += 4;
                return true;
            }
            return false;
        }
        if (c == 'f') {
            if (s.size() - pos >= 5 && s.compare(pos, 5, "false") == 0) {
                pos += 5;
                return true;
            }
            return false;
        }
        if (c == 'n') {
            if (s.size() - pos >= 4 && s.compare(pos, 4, "null") == 0) {
                pos += 4;
                return true;
            }
            return false;
        }
        return parse_number();
    }
};

bool valid_binding_config_json(const std::string& json) {
    if (json.empty() || !is_valid_utf8_bytes(json)) {
        return false;
    }
    BindingConfigJsonParser parser(json);
    if (!parser.parse_value()) {
        return false;
    }
    parser.skip_ws();
    return parser.pos == json.size();
}

bool parse_binding_blob(const std::vector<uint8_t>& blob, capsid::WorkerBindingDescriptor* out,
                        std::string* error) {
    if (blob.size() < sizeof(uint32_t)) {
        return reject(error, "invalid binding descriptor length");
    }
    const uint8_t* cursor = &blob[0];
    const uint8_t* end = cursor + blob.size();
    uint32_t descriptor_size = 0;
    if (!capsid::protocol::read_u32(&cursor, end, &descriptor_size) || descriptor_size == 0 ||
        descriptor_size > kMaxBindingDescriptorBytes ||
        static_cast<uint64_t>(descriptor_size) > static_cast<uint64_t>(end - cursor)) {
        return reject(error, "invalid binding descriptor length");
    }
    const uint8_t* descriptor_end = cursor + descriptor_size;
    const uint8_t* source_begin = descriptor_end;

    capsid::WorkerBindingDescriptor decoded;
    if (!read_string16(&cursor, descriptor_end, &decoded.name) || contains_nul(decoded.name) ||
        !valid_binding_name(decoded.name)) {
        return reject(error, "invalid binding name");
    }
    if (!read_string32(&cursor, descriptor_end, &decoded.config_json) ||
        contains_nul(decoded.config_json) || decoded.config_json.size() > kMaxBindingConfigBytes ||
        !valid_binding_config_json(decoded.config_json)) {
        return reject(error, "invalid binding config");
    }

    uint32_t secret_count = 0;
    if (!capsid::protocol::read_u32(&cursor, descriptor_end, &secret_count) ||
        secret_count > kMaxBindingSecrets) {
        return reject(error, "invalid binding secret count");
    }
    for (uint32_t i = 0; i < secret_count; ++i) {
        capsid::WorkerBindingSecret secret;
        if (!read_string16(&cursor, descriptor_end, &secret.key) || contains_nul(secret.key) ||
            !valid_binding_secret_key(secret.key) ||
            std::any_of(decoded.secrets.begin(), decoded.secrets.end(),
                        [&secret](const capsid::WorkerBindingSecret& existing) {
                            return existing.key == secret.key;
                        })) {
            return reject(error, "invalid binding secret key");
        }
        uint32_t value_size = 0;
        if (!capsid::protocol::read_u32(&cursor, descriptor_end, &value_size) ||
            value_size > kMaxBindingSecretValueBytes ||
            static_cast<uint64_t>(value_size) > static_cast<uint64_t>(descriptor_end - cursor)) {
            return reject(error, "invalid binding secret value");
        }
        secret.value.assign(cursor, cursor + value_size);
        cursor += value_size;
        decoded.secrets.push_back(secret);
    }

    if (!read_binding_list(&cursor, descriptor_end, &decoded.profiles, 6,
                           capsid::binding_profile_known, "sandbox profile", error) ||
        !read_binding_list(&cursor, descriptor_end, &decoded.modules, kMaxBindingModules,
                           capsid::binding_module_known, "module", error) ||
        !read_binding_list(&cursor, descriptor_end, &decoded.net_rules, capsid::kMaxBindingRules,
                           valid_binding_net_target, "net target", error) ||
        !read_binding_list(&cursor, descriptor_end, &decoded.fs_read, capsid::kMaxBindingRules,
                           valid_binding_path, "fs read path", error) ||
        !read_binding_list(&cursor, descriptor_end, &decoded.fs_write, capsid::kMaxBindingRules,
                           valid_binding_path, "fs write path", error) ||
        !read_binding_list(&cursor, descriptor_end, &decoded.env, kMaxBindingEnvEntries,
                           valid_binding_env_name, "environment name", error) ||
        !read_binding_list(&cursor, descriptor_end, &decoded.stdio, kMaxBindingStdioEntries,
                           valid_binding_stdio, "stdio stream", error)) {
        return false;
    }
    if (cursor != descriptor_end) {
        return reject(error, "binding descriptor has trailing bytes");
    }

    const size_t source_size = static_cast<size_t>(end - source_begin);
    if (source_size > kMaxBindingSourceBytes) {
        return reject(error, "binding source exceeds the size limit");
    }
    decoded.source.assign(source_begin, end);
    *out = decoded;
    return true;
}

bool read_string16(const uint8_t** cursor, const uint8_t* end, std::string* value) {
    uint16_t size = 0;
    if (!capsid::protocol::read_u16(cursor, end, &size) ||
        static_cast<size_t>(end - *cursor) < size) {
        return false;
    }
    value->assign(reinterpret_cast<const char*>(*cursor), size);
    *cursor += size;
    return true;
}

bool read_string32(const uint8_t** cursor, const uint8_t* end, std::string* value) {
    uint32_t size = 0;
    if (!capsid::protocol::read_u32(cursor, end, &size) ||
        static_cast<uint64_t>(end - *cursor) < size) {
        return false;
    }
    value->assign(reinterpret_cast<const char*>(*cursor), size);
    *cursor += size;
    return true;
}

struct WireEgressPolicy {
    WireEgressPolicy() : default_action(CAPSID_EGRESS_DENY) {}

    capsid_egress_action default_action;
    std::vector<std::string> targets;
    std::vector<capsid_egress_rule> rules;
};

bool decode_wire_egress(const uint8_t** cursor, const uint8_t* end, WireEgressPolicy* decoded,
                        std::string* error) {
    uint32_t default_action = 0;
    uint32_t rule_count = 0;
    if (!decoded || !capsid::protocol::read_u32(cursor, end, &default_action) ||
        !capsid::protocol::read_u32(cursor, end, &rule_count) ||
        default_action > CAPSID_EGRESS_ALLOW || rule_count > 256) {
        return reject(error, "invalid HELLO egress header");
    }

    WireEgressPolicy candidate;
    candidate.default_action = static_cast<capsid_egress_action>(default_action);
    candidate.targets.resize(rule_count);
    candidate.rules.resize(rule_count);
    for (uint32_t index = 0; index < rule_count; ++index) {
        uint32_t action = 0;
        capsid_egress_rule_init(&candidate.rules[index]);
        if (!capsid::protocol::read_u32(cursor, end, &action) ||
            !capsid::protocol::read_u16(cursor, end, &candidate.rules[index].port_start) ||
            !capsid::protocol::read_u16(cursor, end, &candidate.rules[index].port_end) ||
            !capsid::protocol::read_u32(cursor, end, &candidate.rules[index].rule_id) ||
            !read_string16(cursor, end, &candidate.targets[index]) ||
            action > CAPSID_EGRESS_ALLOW || contains_nul(candidate.targets[index])) {
            return reject(error, "invalid HELLO egress rule");
        }
        candidate.rules[index].action = static_cast<capsid_egress_action>(action);
        candidate.rules[index].target = candidate.targets[index].c_str();
    }
    *decoded = candidate;
    /*
     * std::vector copy above preserves strings but not their c_str pointers.
     * Rebind every public descriptor to the committed storage.
     */
    for (uint32_t index = 0; index < rule_count; ++index) {
        decoded->rules[index].target = decoded->targets[index].c_str();
    }
    return true;
}

bool configure_wire_egress(const WireEgressPolicy& wire, capsid::EgressPolicy* policy,
                           std::string* error) {
    capsid_egress_policy descriptor;
    capsid_egress_policy_init(&descriptor);
    descriptor.default_action = wire.default_action;
    descriptor.rules = wire.rules.empty() ? NULL : &wire.rules[0];
    descriptor.rule_count = static_cast<uint32_t>(wire.rules.size());
    if (!policy->configure(&descriptor, error)) {
        return reject(error, "invalid HELLO egress policy");
    }
    return true;
}

bool decode_wire_capability(const uint8_t** cursor, const uint8_t* end,
                            capsid::CapabilityPolicy* output, std::string* error) {
    uint32_t version = 0;
    std::string application_identity;
    uint16_t module_count = 0;
    if (!output || !capsid::protocol::read_u32(cursor, end, &version) ||
        !read_string16(cursor, end, &application_identity) || contains_nul(application_identity) ||
        !capsid::protocol::read_u16(cursor, end, &module_count) || module_count > 64) {
        return reject(error, "invalid HELLO capability header");
    }

    std::vector<std::string> modules(module_count);
    std::vector<const char*> module_pointers(module_count);
    for (uint16_t index = 0; index < module_count; ++index) {
        if (!read_string16(cursor, end, &modules[index]) || contains_nul(modules[index])) {
            return reject(error, "invalid HELLO allowed module");
        }
        module_pointers[index] = modules[index].c_str();
    }

    uint16_t rule_count = 0;
    if (!capsid::protocol::read_u16(cursor, end, &rule_count) || rule_count > 256) {
        return reject(error, "invalid HELLO permission header");
    }
    std::vector<std::string> resources(rule_count);
    std::vector<capsid_permission_rule> rules(rule_count);
    for (uint16_t index = 0; index < rule_count; ++index) {
        uint32_t action = 0;
        uint32_t permission = 0;
        capsid_permission_rule_init(&rules[index]);
        if (!capsid::protocol::read_u32(cursor, end, &action) ||
            !capsid::protocol::read_u32(cursor, end, &permission) ||
            !capsid::protocol::read_u32(cursor, end, &rules[index].rule_id) ||
            !read_string16(cursor, end, &resources[index]) || action > CAPSID_PERMISSION_ALLOW ||
            permission < CAPSID_PERMISSION_READ || permission > CAPSID_PERMISSION_ENGINE ||
            permission == CAPSID_PERMISSION_NET || contains_nul(resources[index])) {
            return reject(error, "invalid HELLO permission rule");
        }
        rules[index].action = static_cast<capsid_permission_action>(action);
        rules[index].permission = static_cast<capsid_permission_name>(permission);
        rules[index].resource = resources[index].empty() ? NULL : resources[index].c_str();
    }

    WireEgressPolicy net;
    if (!decode_wire_egress(cursor, end, &net, error)) {
        return false;
    }
    uint16_t env_entry_count = 0;
    if (!capsid::protocol::read_u16(cursor, end, &env_entry_count) || env_entry_count > 256) {
        return reject(error, "invalid HELLO environment snapshot header");
    }
    std::vector<std::string> env_names(env_entry_count);
    std::vector<std::string> env_values(env_entry_count);
    std::vector<capsid_env_entry> env_entries(env_entry_count);
    for (uint16_t index = 0; index < env_entry_count; ++index) {
        capsid_env_entry_init(&env_entries[index]);
        if (!read_string16(cursor, end, &env_names[index]) ||
            !read_string16(cursor, end, &env_values[index]) || contains_nul(env_names[index]) ||
            contains_nul(env_values[index])) {
            return reject(error, "invalid HELLO environment snapshot entry");
        }
        env_entries[index].name = env_names[index].c_str();
        env_entries[index].value = env_values[index].c_str();
    }
    if (version == 0) {
        if (!application_identity.empty() || !modules.empty() || !rules.empty() ||
            net.default_action != CAPSID_EGRESS_DENY || !net.rules.empty() ||
            !env_entries.empty()) {
            return reject(error, "disabled HELLO capability policy has data");
        }
        if (!output->configure(NULL, error)) {
            return reject(error, "invalid disabled HELLO capability policy");
        }
        return true;
    }
    if (version != CAPSID_CAPABILITY_POLICY_VERSION) {
        return reject(error, "unknown HELLO capability policy version");
    }

    capsid_egress_policy net_descriptor;
    capsid_egress_policy_init(&net_descriptor);
    net_descriptor.default_action = net.default_action;
    net_descriptor.rules = net.rules.empty() ? NULL : &net.rules[0];
    net_descriptor.rule_count = static_cast<uint32_t>(net.rules.size());

    capsid_capability_policy descriptor;
    capsid_capability_policy_init(&descriptor);
    descriptor.version = version;
    descriptor.application_identity =
        application_identity.empty() ? NULL : application_identity.c_str();
    descriptor.allowed_modules = module_pointers.empty() ? NULL : &module_pointers[0];
    descriptor.allowed_module_count = module_count;
    descriptor.rules = rules.empty() ? NULL : &rules[0];
    descriptor.rule_count = rule_count;
    descriptor.net_policy = &net_descriptor;
    descriptor.env_entries = env_entries.empty() ? NULL : &env_entries[0];
    descriptor.env_entry_count = env_entry_count;
    if (!output->configure(&descriptor, error)) {
        return reject(error, "invalid HELLO capability policy");
    }
    return true;
}

} // namespace

namespace capsid {

bool decode_hello(const capsid::protocol::Frame& frame, capsid::WorkerStartupConfig* output,
                  std::string* error) {
    if (!output || frame.type != capsid::protocol::kHello ||
        frame.version != capsid::protocol::kVersion || frame.flags != 0 || frame.request_id != 0 ||
        frame.payload.size() < capsid::protocol::kHelloFixedPayloadSize ||
        frame.payload.size() > capsid::protocol::kMaxPayloadSize) {
        return reject(error, "invalid HELLO frame");
    }

    capsid::WorkerStartupConfig decoded;
    const uint8_t* cursor = &frame.payload[0];
    const uint8_t* end = cursor + frame.payload.size();
    uint32_t abi = 0;
    uint8_t strict = 0;
    if (!capsid::protocol::read_u32(&cursor, end, &abi) || abi != CAPSID_ABI_VERSION ||
        !capsid::protocol::read_u64(&cursor, end, &decoded.js_heap_limit) ||
        !capsid::protocol::read_u64(&cursor, end, &decoded.process_memory_limit) ||
        !capsid::protocol::read_u32(&cursor, end, &decoded.file_descriptor_limit) ||
        !capsid::protocol::read_u64(&cursor, end, &decoded.timeout_ms) ||
        !capsid::protocol::read_u32(&cursor, end, &decoded.js_stack_size) ||
        !capsid::protocol::read_u32(&cursor, end, &decoded.max_inflight) ||
        !capsid::protocol::read_u32(&cursor, end, &decoded.initial_window) ||
        !capsid::protocol::read_u32(&cursor, end, &decoded.max_header_bytes) ||
        !capsid::protocol::read_u32(&cursor, end, &decoded.max_queued_bytes) || cursor == end) {
        return reject(error, "truncated HELLO fixed fields");
    }
    strict = *cursor++;
    decoded.strict_sandbox = strict != 0;
    if (!capsid::protocol::read_u32(&cursor, end, &decoded.sandbox_required_features) ||
        !capsid::protocol::read_u32(&cursor, end, &decoded.preinstalled_sandbox_features)) {
        return reject(error, "truncated HELLO sandbox fields");
    }

    uint16_t ca_size = 0;
    if (!capsid::protocol::read_u16(&cursor, end, &ca_size) || ca_size > 4096 ||
        static_cast<size_t>(end - cursor) < static_cast<size_t>(ca_size) + 43) {
        return reject(error, "invalid HELLO CA path");
    }
    decoded.tls_ca_bundle_path.assign(reinterpret_cast<const char*>(cursor), ca_size);
    cursor += ca_size;
    if (contains_nul(decoded.tls_ca_bundle_path) ||
        !capsid::protocol::read_u64(&cursor, end, &decoded.max_fetch_request_body_bytes) ||
        !capsid::protocol::read_u64(&cursor, end, &decoded.max_fetch_response_body_bytes)) {
        return reject(error, "invalid HELLO Fetch controls");
    }

    WireEgressPolicy legacy_egress;
    if (!decode_wire_egress(&cursor, end, &legacy_egress, error) || cursor == end) {
        return reject(error, "invalid HELLO legacy egress state");
    }
    const uint8_t legacy_egress_configured = *cursor++;
    if (legacy_egress_configured > 1 ||
        (!legacy_egress_configured &&
         (legacy_egress.default_action != CAPSID_EGRESS_DENY || !legacy_egress.rules.empty())) ||
        !configure_wire_egress(legacy_egress, &decoded.egress_policy, error) ||
        !decode_wire_capability(&cursor, end, &decoded.capability_policy, error)) {
        return false;
    }
    decoded.legacy_egress_configured = legacy_egress_configured != 0;
    // Amended §1.2 (v4.1): the v4 Hello payload is [startup fields]
    // [A2 limits extension, only when A2 announced] [8-byte tail].
    // The tail is read from the END so the extension (which sits
    // before it) cannot be mistaken for the capability bits.
    if (static_cast<size_t>(end - cursor) < capsid::protocol::kHelloV4TailSize) {
        return reject(error, "invalid hello capabilities");
    }
    const uint8_t* tail = end - capsid::protocol::kHelloV4TailSize;
    uint32_t capabilities = 0;
    uint32_t reserved = 0;
    if (!capsid::protocol::read_u32(&tail, end, &capabilities) ||
        !capsid::protocol::read_u32(&tail, end, &reserved) || reserved != 0 || tail != end ||
        (capabilities & ~capsid::protocol::kCapabilitiesKnownMask) != 0 ||
        !capsid::protocol::capabilities_satisfy_closure(capabilities)) {
        return reject(error, "invalid hello capabilities");
    }
    if ((capabilities & capsid::protocol::kCapChannelLayoutA2) != 0) {
        const uint8_t* extension_end = end - capsid::protocol::kHelloV4TailSize;
        if (extension_end < cursor || !capsid::protocol::decode_hello_a2_extension(
                                          cursor, static_cast<size_t>(extension_end - cursor),
                                          &decoded.channel_limits, error)) {
            return reject(error, "invalid hello extension");
        }
        // decode_hello_a2_extension consumed exactly the 48-byte
        // extension; the tail (already validated) follows.
        cursor = extension_end;
        decoded.channel_limits_present = true;
    }
    decoded.host_capabilities = capabilities;
    // consume the validated tail only when the cursor has reached
    // it; otherwise stray bytes fall to the trailing-bytes check
    if (cursor == end - capsid::protocol::kHelloV4TailSize) {
        cursor = end;
    }
    if (cursor != end) {
        return reject(error, "trailing HELLO payload");
    }

    const uint64_t max_safe_js_integer = UINT64_C(9007199254740991);
    const bool valid =
        strict <= 1 &&
        decoded.js_heap_limit <= static_cast<uint64_t>(std::numeric_limits<int>::max()) &&
        decoded.timeout_ms != 0 && decoded.js_stack_size != 0 && decoded.max_inflight != 0 &&
        decoded.initial_window != 0 && decoded.max_header_bytes >= 8 &&
        decoded.max_header_bytes <= capsid::protocol::kMaxPayloadSize &&
        decoded.max_queued_bytes >=
            capsid::protocol::kHeaderSize + capsid::protocol::kHelloFixedPayloadSize &&
        decoded.max_queued_bytes <= capsid::protocol::kMaxBufferedBytes &&
        frame.payload.size() <= decoded.max_queued_bytes - capsid::protocol::kHeaderSize &&
        decoded.file_descriptor_limit >= 4 &&
        decoded.max_fetch_request_body_bytes <= max_safe_js_integer &&
        decoded.max_fetch_response_body_bytes <= max_safe_js_integer &&
        (decoded.sandbox_required_features & ~static_cast<uint32_t>(CAPSID_SANDBOX_FEATURE_ALL)) ==
            0 &&
        (decoded.preinstalled_sandbox_features &
         ~static_cast<uint32_t>(CAPSID_SANDBOX_FEATURE_CGROUP_V2)) == 0 &&
        (decoded.preinstalled_sandbox_features & ~decoded.sandbox_required_features) == 0 &&
        (decoded.strict_sandbox ||
         (decoded.sandbox_required_features == 0 && decoded.preinstalled_sandbox_features == 0)) &&
        (!decoded.strict_sandbox ||
         (decoded.sandbox_required_features & CAPSID_SANDBOX_FEATURE_STRICT_BASE) ==
             CAPSID_SANDBOX_FEATURE_STRICT_BASE) &&
        (!decoded.process_memory_limit || decoded.process_memory_limit >= decoded.js_heap_limit);
    if (!valid) {
        return reject(error, "invalid HELLO configuration");
    }
    *output = decoded;
    return true;
}

} // namespace capsid

namespace capsid {

WorkerStartupConfig::WorkerStartupConfig()
    : js_stack_size(1024u * 1024u), max_inflight(128), initial_window(256u * 1024u),
      max_header_bytes(64u * 1024u), max_queued_bytes(4u * 1024u * 1024u),
      file_descriptor_limit(64), timeout_ms(30000), strict_sandbox(false),
      sandbox_required_features(0), preinstalled_sandbox_features(0),
      js_heap_limit(64u * 1024u * 1024u), process_memory_limit(256u * 1024u * 1024u),
      max_fetch_request_body_bytes(0), max_fetch_response_body_bytes(0),
      legacy_egress_configured(false) {}

WorkerStartupState::WorkerStartupState()
    : hello_received_(false), bundle_started_(false), bundle_complete_(false),
      bundle_is_trusted_bytecode_(false), bundle_name_("capsid:app/main") {}

bool WorkerStartupState::consume(const protocol::Frame& frame, std::string* error) {
    if (error) {
        error->clear();
    }
    if (!hello_received_) {
        WorkerStartupConfig decoded;
        if (!decode_hello(frame, &decoded, error)) {
            return false;
        }
        config_ = decoded;
        hello_received_ = true;
        return true;
    }
    if (frame.type == protocol::kLoadBinding) {
        return consume_load_binding(frame, error);
    }
    if (frame.type != protocol::kLoadBundle || frame.request_id != 0 ||
        (frame.flags & ~(protocol::kFlagStart | protocol::kFlagEnd | protocol::kFlagBundleName |
                         protocol::kFlagTrustedBytecode)) != 0 ||
        ((frame.flags & protocol::kFlagBundleName) != 0 &&
         (frame.flags & protocol::kFlagStart) == 0) ||
        ((frame.flags & protocol::kFlagTrustedBytecode) != 0 &&
         (frame.flags & protocol::kFlagStart) == 0) ||
        bundle_complete_ ||
        // The bundle seals the binding list; it also cannot interleave a
        // binding sequence in flight.
        binding_inflight_) {
        return reject(error, "invalid startup frame sequence");
    }

    const bool starts = (frame.flags & protocol::kFlagStart) != 0;
    if ((starts && bundle_started_) || (!starts && !bundle_started_)) {
        return reject(error, "invalid bundle start sequence");
    }
    size_t payload_offset = 0;
    std::string new_name = bundle_name_;
    if ((frame.flags & protocol::kFlagBundleName) != 0) {
        if (frame.payload.size() < sizeof(uint16_t)) {
            return reject(error, "truncated bundle name");
        }
        const uint8_t* cursor = &frame.payload[0];
        const uint8_t* end = cursor + frame.payload.size();
        uint16_t name_size = 0;
        if (!protocol::read_u16(&cursor, end, &name_size) || name_size == 0 || name_size > 4096 ||
            static_cast<size_t>(end - cursor) < name_size) {
            return reject(error, "invalid bundle name");
        }
        new_name.assign(reinterpret_cast<const char*>(cursor), name_size);
        if (contains_nul(new_name)) {
            return reject(error, "NUL in bundle name");
        }
        cursor += name_size;
        payload_offset = static_cast<size_t>(cursor - &frame.payload[0]);
    }
    const size_t payload_size = frame.payload.size() - payload_offset;
    if (bundle_.size() > config_.max_queued_bytes ||
        payload_size > config_.max_queued_bytes - bundle_.size()) {
        return reject(error, "bundle buffer limit exceeded");
    }

    if (starts) {
        bundle_started_ = true;
        bundle_name_ = new_name;
        bundle_is_trusted_bytecode_ = (frame.flags & protocol::kFlagTrustedBytecode) != 0;
        if ((frame.flags & protocol::kFlagEnd) != 0 && !bundle_is_trusted_bytecode_) {
            bundle_.reserve(payload_size + 1);
        }
    }
    bundle_.insert(bundle_.end(), frame.payload.begin() + static_cast<ptrdiff_t>(payload_offset),
                   frame.payload.end());
    if ((frame.flags & protocol::kFlagEnd) != 0) {
        bundle_complete_ = true;
    }
    return true;
}

bool WorkerStartupState::consume_load_binding(const protocol::Frame& frame, std::string* error) {
    if (frame.request_id != 0 ||
        (frame.flags & ~(protocol::kFlagStart | protocol::kFlagEnd)) != 0) {
        return reject(error, "invalid binding frame flags");
    }
    const bool starts = (frame.flags & protocol::kFlagStart) != 0;
    if (starts) {
        if (bundle_started_ || binding_inflight_) {
            return reject(error, "invalid binding start sequence");
        }
    } else if (!binding_inflight_) {
        return reject(error, "invalid binding continuation sequence");
    }
    if (binding_blob_.size() > config_.max_queued_bytes ||
        frame.payload.size() > config_.max_queued_bytes - binding_blob_.size()) {
        return reject(error, "binding buffer limit exceeded");
    }
    if (starts) {
        binding_inflight_ = true;
    }
    binding_blob_.insert(binding_blob_.end(), frame.payload.begin(), frame.payload.end());
    if ((frame.flags & protocol::kFlagEnd) != 0) {
        WorkerBindingDescriptor descriptor;
        if (!parse_binding_blob(binding_blob_, &descriptor, error)) {
            binding_blob_.clear();
            binding_inflight_ = false;
            return false;
        }
        bindings_.push_back(descriptor);
        binding_blob_.clear();
        binding_inflight_ = false;
    }
    return true;
}

static bool read_view16(const uint8_t** cursor, const uint8_t* end, WorkerSlice* value) {
    uint16_t size = 0;
    if (!capsid::protocol::read_u16(cursor, end, &size) ||
        static_cast<size_t>(end - *cursor) < size) {
        return false;
    }
    *value = WorkerSlice(reinterpret_cast<const char*>(*cursor), size);
    *cursor += size;
    return true;
}

static bool read_view32(const uint8_t** cursor, const uint8_t* end, WorkerSlice* value) {
    uint32_t size = 0;
    if (!capsid::protocol::read_u32(cursor, end, &size) ||
        static_cast<uint64_t>(end - *cursor) < size) {
        return false;
    }
    *value = WorkerSlice(reinterpret_cast<const char*>(*cursor), size);
    *cursor += size;
    return true;
}

bool decode_worker_request_head_views(const protocol::Frame& frame, uint32_t max_header_bytes,
                                      WorkerRequestHeadView* output,
                                      std::vector<WorkerRequestHeaderView>* headers_out,
                                      std::string* error) {
    if (error) {
        error->clear();
    }
    if (!output || !headers_out || frame.type != protocol::kRequestHead ||
        (frame.flags & ~(protocol::kFlagRequestEnd | protocol::kFlagRequestUpgrade)) != 0 ||
        ((frame.flags & protocol::kFlagRequestUpgrade) && frame.version != protocol::kVersion) ||
        frame.request_id == 0 || frame.payload.size() < 8 ||
        frame.payload.size() > max_header_bytes ||
        frame.payload.size() > protocol::kMaxPayloadSize) {
        return reject(error, "invalid request head frame");
    }
    const uint8_t* cursor = &frame.payload[0];
    const uint8_t* end = cursor + frame.payload.size();
    WorkerRequestHeadView decoded;
    decoded.bodyless = (frame.flags & capsid::protocol::kFlagRequestEnd) != 0;
    uint16_t count = 0;
    if (!read_view16(&cursor, end, &decoded.method) || !read_view32(&cursor, end, &decoded.url) ||
        decoded.method.empty() || decoded.url.empty() ||
        !protocol::read_u16(&cursor, end, &count)) {
        return reject(error, "invalid request head fields");
    }
    headers_out->clear();
    headers_out->reserve(count);
    for (uint16_t index = 0; index < count; ++index) {
        WorkerRequestHeaderView header;
        if (!read_view16(&cursor, end, &header.name) || !read_view32(&cursor, end, &header.value) ||
            header.name.empty()) {
            headers_out->clear();
            return reject(error, "invalid request header");
        }
        headers_out->push_back(header);
    }
    if (frame.flags & protocol::kFlagRequestUpgrade) {
        protocol::UpgradeTail tail;
        if (!decoded.bodyless || !protocol::decode_upgrade_head(frame, &tail, error)) {
            headers_out->clear();
            return false;
        }
        decoded.upgrade_channel_id = tail.channel_id;
        decoded.upgrade_max_message_bytes = tail.max_message_bytes;
        cursor += 12;
    }
    if (cursor != end) {
        headers_out->clear();
        return reject(error, "trailing request head payload");
    }
    *output = decoded;
    return true;
}

bool decode_worker_request_head(const protocol::Frame& frame, uint32_t max_header_bytes,
                                WorkerRequestHead* output, std::string* error) {
    if (!output) {
        if (error) {
            error->clear();
        }
        return reject(error, "invalid request head frame");
    }
    WorkerRequestHeadView view;
    std::vector<WorkerRequestHeaderView> header_views;
    if (!decode_worker_request_head_views(frame, max_header_bytes, &view, &header_views, error)) {
        return false;
    }
    WorkerRequestHead decoded;
    decoded.method.assign(view.method.data(), view.method.size());
    decoded.url.assign(view.url.data(), view.url.size());
    decoded.bodyless = view.bodyless;
    decoded.upgrade_channel_id = view.upgrade_channel_id;
    decoded.upgrade_max_message_bytes = view.upgrade_max_message_bytes;
    decoded.headers.reserve(header_views.size());
    for (const auto& header_view : header_views) {
        WorkerRequestHeader header;
        header.name.assign(header_view.name.data(), header_view.name.size());
        header.value.assign(header_view.value.data(), header_view.value.size());
        decoded.headers.push_back(std::move(header));
    }
    *output = std::move(decoded);
    return true;
}

bool append_ready_proof(std::vector<uint8_t>* payload, uint32_t applied_feature_bits,
                        uint32_t seccomp_mode, uint32_t landlock_abi,
                        const std::string& network_namespace_identity,
                        const std::string& sandbox_profile_digest) {
    if (!payload ||
        (!sandbox_profile_digest.empty() && (sandbox_profile_digest.size() != 71 ||
                                             sandbox_profile_digest.rfind("sha256:", 0) != 0))) {
        return false;
    }
    if (sandbox_profile_digest.empty()) {
        return true; // zero-binding baseline stays byte-identical
    }
    if (network_namespace_identity.size() > 65535) {
        return false;
    }
    capsid::protocol::append_u32(payload, applied_feature_bits);
    capsid::protocol::append_u32(payload, seccomp_mode);
    capsid::protocol::append_u32(payload, landlock_abi);
    capsid::protocol::append_u16(payload, static_cast<uint16_t>(network_namespace_identity.size()));
    payload->insert(payload->end(), network_namespace_identity.begin(),
                    network_namespace_identity.end());
    payload->insert(payload->end(), sandbox_profile_digest.begin(), sandbox_profile_digest.end());
    return true;
}

bool parse_ready_proof(const std::vector<uint8_t>& payload, std::string* compatibility_id,
                       WorkerReadyProof* proof, std::string* error) {
    if (error) {
        error->clear();
    }
    if (!compatibility_id || !proof) {
        return false;
    }
    if (payload.size() < 71) {
        if (error) {
            *error = "truncated READY payload";
        }
        return false;
    }
    compatibility_id->assign(reinterpret_cast<const char*>(&payload[0]), 71);
    *proof = WorkerReadyProof();
    if (payload.size() == 71) {
        return true;
    }
    const uint8_t* cursor = &payload[71];
    const uint8_t* end = &payload[0] + payload.size();
    if (!capsid::protocol::read_u32(&cursor, end, &proof->applied_feature_bits) ||
        !capsid::protocol::read_u32(&cursor, end, &proof->seccomp_mode) ||
        !capsid::protocol::read_u32(&cursor, end, &proof->landlock_abi)) {
        if (error) {
            *error = "truncated READY proof";
        }
        return false;
    }
    uint16_t namespace_size = 0;
    if (!capsid::protocol::read_u16(&cursor, end, &namespace_size) ||
        static_cast<size_t>(end - cursor) < static_cast<size_t>(namespace_size) + 71) {
        if (error) {
            *error = "truncated READY namespace identity";
        }
        return false;
    }
    proof->network_namespace_identity.assign(reinterpret_cast<const char*>(cursor), namespace_size);
    cursor += namespace_size;
    if (static_cast<size_t>(end - cursor) != 71) {
        if (error) {
            *error = "invalid READY profile digest length";
        }
        return false;
    }
    proof->sandbox_profile_digest.assign(reinterpret_cast<const char*>(cursor), 71);
    if (proof->sandbox_profile_digest.rfind("sha256:", 0) != 0) {
        if (error) {
            *error = "invalid READY profile digest";
        }
        return false;
    }
    proof->extended = true;
    return true;
}

void WorkerStartupState::clear_sensitive_and_release() {
    for (size_t i = 0; i < bindings_.size(); ++i) {
        for (size_t s = 0; s < bindings_[i].secrets.size(); ++s) {
            std::vector<uint8_t>& val = bindings_[i].secrets[s].value;
            for (size_t k = 0; k < val.size(); ++k) {
                volatile uint8_t* p = &val[k];
                *p = 0;
            }
            std::vector<uint8_t>().swap(val);
        }
        std::vector<WorkerBindingSecret>().swap(bindings_[i].secrets);
        std::vector<uint8_t>().swap(bindings_[i].source);
        std::string().swap(bindings_[i].config_json);
    }
    std::vector<WorkerBindingDescriptor>().swap(bindings_);
    for (size_t k = 0; k < binding_blob_.size(); ++k) {
        volatile uint8_t* p = &binding_blob_[k];
        *p = 0;
    }
    std::vector<uint8_t>().swap(binding_blob_);
    std::vector<uint8_t>().swap(bundle_);
    std::string().swap(bundle_name_);
    config_.tls_ca_bundle_path.clear();
}

} // namespace capsid
