#include "net_target.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <vector>

namespace capsid {
namespace {

bool parse_port_digits(const char* data, std::size_t len, bool allow_zero, std::uint16_t* out) {
    if (len == 0 || len > 5 || out == nullptr) {
        return false;
    }
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < len; ++i) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10u + static_cast<std::uint32_t>(c - '0');
    }
    if (value > 65535u || (!allow_zero && value == 0u)) {
        return false;
    }
    *out = static_cast<std::uint16_t>(value);
    return true;
}

bool parse_ipv4_bytes(const char* data, std::size_t len, std::array<std::uint8_t, 4>* out) {
    if (len == 0 || len > 15 || out == nullptr) {
        return false;
    }
    std::size_t idx = 0;
    for (int octet = 0; octet < 4; ++octet) {
        if (idx >= len) {
            return false;
        }
        std::size_t start = idx;
        std::uint32_t val = 0;
        while (idx < len && data[idx] != '.') {
            const unsigned char c = static_cast<unsigned char>(data[idx]);
            if (c < '0' || c > '9') {
                return false;
            }
            val = val * 10u + static_cast<std::uint32_t>(c - '0');
            if (val > 255u) {
                return false;
            }
            ++idx;
        }
        const std::size_t digits = idx - start;
        if (digits == 0 || digits > 3) {
            return false;
        }
        if (digits > 1 && data[start] == '0') {
            return false;
        }
        (*out)[static_cast<std::size_t>(octet)] = static_cast<std::uint8_t>(val);
        if (octet < 3) {
            if (idx >= len || data[idx] != '.') {
                return false;
            }
            ++idx;
        }
    }
    return idx == len;
}

int hex_val(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    return -1;
}

// Strict RFC 4291 / 5952 IPv6 address parser (without brackets or zone IDs).
bool parse_ipv6_bytes(const char* data, std::size_t len, std::array<std::uint8_t, 16>* out) {
    if (len < 2 || len > 45 || out == nullptr) {
        return false;
    }
    for (std::size_t i = 0; i < len; ++i) {
        if (data[i] == '%') {
            return false;
        }
    }

    std::uint16_t head[8] = {0};
    std::uint16_t tail[8] = {0};
    int head_count = 0;
    int tail_count = 0;
    bool seen_double_colon = false;
    std::size_t i = 0;

    if (data[0] == ':') {
        if (len < 2 || data[1] != ':') {
            return false;
        }
        seen_double_colon = true;
        i = 2;
    }

    while (i < len) {
        const std::size_t group_start = i;
        bool has_dot = false;
        std::size_t hex_digits = 0;
        std::uint32_t group_val = 0;
        while (i < len && data[i] != ':') {
            if (data[i] == '.') {
                has_dot = true;
                break;
            }
            const int hv = hex_val(data[i]);
            if (hv < 0) {
                return false;
            }
            group_val = (group_val << 4) | static_cast<std::uint32_t>(hv);
            ++hex_digits;
            if (hex_digits > 4) {
                return false;
            }
            ++i;
        }
        if (has_dot) {
            // Embedded IPv4 must occupy the final 4 bytes (2 hextets).
            std::array<std::uint8_t, 4> v4{};
            if (!parse_ipv4_bytes(data + group_start, len - group_start, &v4)) {
                return false;
            }
            const std::uint16_t hi =
                static_cast<std::uint16_t>((static_cast<std::uint16_t>(v4[0]) << 8) | v4[1]);
            const std::uint16_t lo =
                static_cast<std::uint16_t>((static_cast<std::uint16_t>(v4[2]) << 8) | v4[3]);
            int* target_count = seen_double_colon ? &tail_count : &head_count;
            std::uint16_t* target = seen_double_colon ? tail : head;
            if (*target_count + 2 > 8) {
                return false;
            }
            target[(*target_count)++] = hi;
            target[(*target_count)++] = lo;
            i = len;
            break;
        }
        if (hex_digits == 0) {
            return false;
        }
        int* target_count = seen_double_colon ? &tail_count : &head_count;
        std::uint16_t* target = seen_double_colon ? tail : head;
        if (*target_count >= 8) {
            return false;
        }
        target[(*target_count)++] = static_cast<std::uint16_t>(group_val);

        if (i == len) {
            break;
        }
        // data[i] == ':'
        if (i + 1 >= len) {
            return false; // trailing single ':'
        }
        if (data[i + 1] == ':') {
            if (seen_double_colon) {
                return false;
            }
            seen_double_colon = true;
            i += 2;
            if (i == len) {
                break;
            }
        } else {
            ++i;
        }
    }

    if (seen_double_colon) {
        if (head_count + tail_count >= 8) {
            return false;
        }
    } else if (head_count != 8 || tail_count != 0) {
        return false;
    }

    std::uint16_t words[8] = {0};
    for (int k = 0; k < head_count; ++k) {
        words[k] = head[k];
    }
    const int tail_start = 8 - tail_count;
    for (int k = 0; k < tail_count; ++k) {
        words[tail_start + k] = tail[k];
    }
    for (int k = 0; k < 8; ++k) {
        (*out)[static_cast<std::size_t>(k * 2)] = static_cast<std::uint8_t>(words[k] >> 8);
        (*out)[static_cast<std::size_t>(k * 2 + 1)] = static_cast<std::uint8_t>(words[k] & 0xffu);
    }
    return true;
}

std::string format_ipv6_canonical(const std::array<std::uint8_t, 16>& bytes) {
    std::uint16_t words[8];
    for (int i = 0; i < 8; ++i) {
        words[i] = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes[static_cast<std::size_t>(i * 2)]) << 8) |
            bytes[static_cast<std::size_t>(i * 2 + 1)]);
    }
    int best_start = -1;
    int best_len = 0;
    int cur_start = -1;
    int cur_len = 0;
    for (int i = 0; i < 8; ++i) {
        if (words[i] == 0) {
            if (cur_start < 0) {
                cur_start = i;
                cur_len = 1;
            } else {
                ++cur_len;
            }
        } else {
            if (cur_len > best_len && cur_len > 1) {
                best_start = cur_start;
                best_len = cur_len;
            }
            cur_start = -1;
            cur_len = 0;
        }
    }
    if (cur_len > best_len && cur_len > 1) {
        best_start = cur_start;
        best_len = cur_len;
    }

    static const char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(40);
    for (int i = 0; i < 8;) {
        if (i == best_start) {
            out += "::";
            i += best_len;
            continue;
        }
        if (i > 0 &&
            !(out.size() >= 2 && out[out.size() - 1] == ':' && out[out.size() - 2] == ':')) {
            out.push_back(':');
        }
        const std::uint16_t w = words[i];
        bool started = false;
        for (int shift = 12; shift >= 0; shift -= 4) {
            const int nib = (w >> shift) & 0xf;
            if (nib != 0 || started || shift == 0) {
                out.push_back(kHex[nib]);
                started = true;
            }
        }
        ++i;
    }
    return out;
}

bool ascii_dns_label_ok(const char* data, std::size_t len) {
    if (len == 0 || len > 63) {
        return false;
    }
    for (std::size_t i = 0; i < len; ++i) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        const bool alnum =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        const bool hyphen = c == '-';
        const bool underscore = c == '_';
        if (!alnum && !hyphen && !underscore) {
            return false;
        }
        if ((i == 0 || i + 1 == len) && hyphen) {
            return false;
        }
    }
    return true;
}

bool canonicalize_dns_name(const std::string& raw, std::string* out) {
    if (raw.empty() || raw.size() > 253) {
        return false;
    }
    std::string normalized;
    normalized.reserve(raw.size());
    std::size_t start = 0;
    while (start < raw.size()) {
        const std::size_t dot = raw.find('.', start);
        const std::size_t end = dot == std::string::npos ? raw.size() : dot;
        if (!ascii_dns_label_ok(raw.data() + start, end - start)) {
            return false;
        }
        if (!normalized.empty()) {
            normalized.push_back('.');
        }
        for (std::size_t i = start; i < end; ++i) {
            normalized.push_back(
                static_cast<char>(std::tolower(static_cast<unsigned char>(raw[i]))));
        }
        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1;
        if (start == raw.size()) {
            return false; // trailing dot
        }
    }
    *out = normalized;
    return true;
}

bool prefix_match(const std::uint8_t* a, const std::uint8_t* b, std::uint8_t bits) {
    const std::size_t full_bytes = static_cast<std::size_t>(bits / 8u);
    if (full_bytes > 0 && std::memcmp(a, b, full_bytes) != 0) {
        return false;
    }
    const std::uint8_t rem = static_cast<std::uint8_t>(bits % 8u);
    if (rem == 0) {
        return true;
    }
    const std::uint8_t mask = static_cast<std::uint8_t>(0xffu << (8u - rem));
    return (a[full_bytes] & mask) == (b[full_bytes] & mask);
}

bool parse_prefix_bits(const std::string& digits, std::uint8_t max_bits, std::uint8_t* out) {
    if (digits.empty() || digits.size() > 3 || out == nullptr) {
        return false;
    }
    if (digits.size() > 1 && digits[0] == '0') {
        return false;
    }
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(digits[i]);
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10u + static_cast<std::uint32_t>(c - '0');
    }
    if (value > max_bits) {
        return false;
    }
    *out = static_cast<std::uint8_t>(value);
    return true;
}

struct PortSpec {
    bool any = true;
    std::uint16_t start = 1;
    std::uint16_t end = 65535;
};

bool parse_port_spec(const std::string& text, PortSpec* out) {
    if (text == "*") {
        out->any = true;
        out->start = 1;
        out->end = 65535;
        return true;
    }
    const std::size_t dash = text.find('-');
    if (dash == std::string::npos) {
        std::uint16_t port = 0;
        if (!parse_port_digits(text.data(), text.size(), false, &port)) {
            return false;
        }
        out->any = false;
        out->start = port;
        out->end = port;
        return true;
    }
    std::uint16_t first = 0;
    std::uint16_t last = 0;
    if (!parse_port_digits(text.data(), dash, false, &first) ||
        !parse_port_digits(text.data() + dash + 1, text.size() - dash - 1, false, &last) ||
        first > last) {
        return false;
    }
    out->any = false;
    out->start = first;
    out->end = last;
    return true;
}

bool split_host_and_port_spec(const std::string& target, std::string* host_out,
                              PortSpec* port_out) {
    if (target.empty() || host_out == nullptr || port_out == nullptr) {
        return false;
    }
    *port_out = PortSpec{};
    if (target[0] == '[') {
        const std::size_t close = target.find(']');
        if (close == std::string::npos) {
            return false;
        }
        if (close + 1 == target.size()) {
            *host_out = target;
            return true;
        }
        if (target[close + 1] == ':') {
            *host_out = target.substr(0, close + 1);
            return parse_port_spec(target.substr(close + 2), port_out);
        }
        if (target[close + 1] == '/') {
            const std::size_t colon = target.find(':', close + 1);
            if (colon == std::string::npos) {
                *host_out = target;
                return true;
            }
            *host_out = target.substr(0, colon);
            return parse_port_spec(target.substr(colon + 1), port_out);
        }
        return false;
    }

    const std::size_t first_colon = target.find(':');
    if (first_colon == std::string::npos) {
        *host_out = target;
        return true;
    }
    const std::size_t last_colon = target.rfind(':');
    if (first_colon != last_colon) {
        // Bare IPv6 address (with optional /prefix), no port suffix.
        *host_out = target;
        return true;
    }
    *host_out = target.substr(0, first_colon);
    return parse_port_spec(target.substr(first_colon + 1), port_out);
}

} // namespace

bool parse_tcp_endpoint(const std::string& text, std::string* host_out, std::uint16_t* port_out,
                        bool allow_zero_port) {
    if (text.empty() || host_out == nullptr || port_out == nullptr) {
        return false;
    }
    if (text[0] == '[') {
        const std::size_t close = text.find(']');
        if (close == std::string::npos || close + 1 >= text.size() || text[close + 1] != ':') {
            return false;
        }
        const std::string literal = text.substr(1, close - 1);
        std::array<std::uint8_t, 16> v6{};
        if (!parse_ipv6_bytes(literal.data(), literal.size(), &v6)) {
            return false;
        }
        std::uint16_t port = 0;
        if (!parse_port_digits(text.data() + close + 2, text.size() - close - 2, allow_zero_port,
                               &port)) {
            return false;
        }
        *host_out = format_ipv6_canonical(v6);
        *port_out = port;
        return true;
    }

    const std::size_t colon = text.find(':');
    if (colon == std::string::npos || colon == 0 ||
        text.find(':', colon + 1) != std::string::npos) {
        return false;
    }
    const std::string host = text.substr(0, colon);
    for (std::size_t i = 0; i < host.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(host[i]);
        if (c <= 0x20u || c == 0x7fu || c == ']' || c == '/' || c == '\\') {
            return false;
        }
    }
    std::uint16_t port = 0;
    if (!parse_port_digits(text.data() + colon + 1, text.size() - colon - 1, allow_zero_port,
                           &port)) {
        return false;
    }
    *host_out = host;
    *port_out = port;
    return true;
}

bool parse_net_host(const std::string& text, CanonicalNetHost* out) {
    if (text.empty() || out == nullptr) {
        return false;
    }
    *out = CanonicalNetHost{};
    if (text == "*") {
        out->kind = CanonicalNetHost::kAny;
        out->canonical = "*";
        return true;
    }
    if (text.size() > 2 && text[0] == '*' && text[1] == '.') {
        std::string domain;
        if (!canonicalize_dns_name(text.substr(2), &domain)) {
            return false;
        }
        out->kind = CanonicalNetHost::kWildcard;
        out->canonical = domain;
        return true;
    }

    const std::size_t slash = text.find('/');
    const std::string base = slash == std::string::npos ? text : text.substr(0, slash);
    std::string addr = base;
    if (!addr.empty() && addr[0] == '[') {
        if (addr.size() < 4 || addr[addr.size() - 1] != ']') {
            return false;
        }
        addr = addr.substr(1, addr.size() - 2);
    }

    std::array<std::uint8_t, 4> v4{};
    if (parse_ipv4_bytes(addr.data(), addr.size(), &v4)) {
        if (base.size() != addr.size()) {
            return false; // IPv4 must not be bracketed
        }
        std::uint8_t prefix = 32;
        if (slash != std::string::npos && !parse_prefix_bits(text.substr(slash + 1), 32, &prefix)) {
            return false;
        }
        out->kind = CanonicalNetHost::kIpv4Cidr;
        out->v4 = v4;
        out->prefix_len = prefix;
        out->canonical = text;
        return true;
    }

    std::array<std::uint8_t, 16> v6{};
    if (parse_ipv6_bytes(addr.data(), addr.size(), &v6)) {
        std::uint8_t prefix = 128;
        if (slash != std::string::npos &&
            !parse_prefix_bits(text.substr(slash + 1), 128, &prefix)) {
            return false;
        }
        out->kind = CanonicalNetHost::kIpv6Cidr;
        out->v6 = v6;
        out->prefix_len = prefix;
        out->canonical = format_ipv6_canonical(v6);
        return true;
    }

    if (slash != std::string::npos || base.size() != addr.size()) {
        return false;
    }
    std::string dns;
    if (!canonicalize_dns_name(base, &dns)) {
        return false;
    }
    out->kind = CanonicalNetHost::kDns;
    out->canonical = dns;
    return true;
}

bool net_host_contained(const std::string& app_host_text, const std::string& manifest_host_text) {
    CanonicalNetHost app;
    CanonicalNetHost manifest;
    if (!parse_net_host(app_host_text, &app) || !parse_net_host(manifest_host_text, &manifest)) {
        return false;
    }
    if (manifest.kind == CanonicalNetHost::kAny) {
        return true;
    }
    if (app.kind == CanonicalNetHost::kAny) {
        return false;
    }
    if (manifest.kind == CanonicalNetHost::kWildcard) {
        if (app.kind != CanonicalNetHost::kDns && app.kind != CanonicalNetHost::kWildcard) {
            return false;
        }
        const std::string& a = app.canonical;
        const std::string& m = manifest.canonical;
        if (app.kind == CanonicalNetHost::kWildcard && a == m) {
            return true;
        }
        if (a.size() <= m.size() + 1) {
            return false;
        }
        const std::size_t prefix_len = a.size() - m.size();
        return a[prefix_len - 1] == '.' && a.compare(prefix_len, m.size(), m) == 0;
    }
    if (manifest.kind == CanonicalNetHost::kDns) {
        return app.kind == CanonicalNetHost::kDns && app.canonical == manifest.canonical;
    }
    if (manifest.kind == CanonicalNetHost::kIpv4Cidr) {
        return app.kind == CanonicalNetHost::kIpv4Cidr && app.prefix_len >= manifest.prefix_len &&
               prefix_match(app.v4.data(), manifest.v4.data(), manifest.prefix_len);
    }
    if (manifest.kind == CanonicalNetHost::kIpv6Cidr) {
        return app.kind == CanonicalNetHost::kIpv6Cidr && app.prefix_len >= manifest.prefix_len &&
               prefix_match(app.v6.data(), manifest.v6.data(), manifest.prefix_len);
    }
    return false;
}

bool net_target_contained(const std::string& app, const std::string& manifest) {
    std::string app_host;
    std::string manifest_host;
    PortSpec app_ports;
    PortSpec manifest_ports;
    if (!split_host_and_port_spec(app, &app_host, &app_ports) ||
        !split_host_and_port_spec(manifest, &manifest_host, &manifest_ports)) {
        return false;
    }
    if (!net_host_contained(app_host, manifest_host)) {
        return false;
    }
    if (manifest_ports.any) {
        return true;
    }
    if (app_ports.any) {
        return false;
    }
    return app_ports.start >= manifest_ports.start && app_ports.end <= manifest_ports.end;
}

} // namespace capsid
