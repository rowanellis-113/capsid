#ifndef CAPSID_NET_TARGET_H
#define CAPSID_NET_TARGET_H

#include <array>
#include <cstdint>
#include <string>

namespace capsid {

// Parses a listener TCP endpoint of the form "host:port" or "[ipv6]:port".
// Rejects unbracketed IPv6 colons, IPv6 zone IDs ('%'), empty hosts, and ports
// outside [allow_zero_port ? 0 : 1, 65535]. For bracketed IPv6, writes the
// unbracketed IPv6 address (or canonical IPv6 string) to `*host_out`.
bool parse_tcp_endpoint(const std::string& text, std::string* host_out, std::uint16_t* port_out,
                        bool allow_zero_port = true);

struct CanonicalNetHost {
    enum Kind {
        kInvalid = 0,
        kAny,      // "*"
        kWildcard, // "*.example.com" -> domain = "example.com"
        kDns,      // "orders.example.com"
        kIpv4Cidr, // v4_addr / prefix_len (0..32; single IP uses 32)
        kIpv6Cidr, // v6_addr / prefix_len (0..128; single IP uses 128)
    };

    Kind kind = kInvalid;
    std::string canonical; // canonical lowercase host or domain without leading "*."
    std::array<std::uint8_t, 4> v4{};
    std::array<std::uint8_t, 16> v6{};
    std::uint8_t prefix_len = 0;
};

// Parses a host / wildcard / CIDR pattern without a port suffix.
// Accepts "*", "*.domain", DNS hostname, IPv4, IPv4/prefix, [IPv6], IPv6,
// [IPv6]/prefix, or IPv6/prefix. Rejects zone IDs and malformed prefixes.
bool parse_net_host(const std::string& text, CanonicalNetHost* out);

// Returns true iff every concrete network endpoint matched by `app_host` is
// also matched by `manifest_host`.
bool net_host_contained(const std::string& app_host, const std::string& manifest_host);

// Parses "host[:port | :start-end | :*]" and returns true iff `app` is a subset
// of `manifest` in both host/CIDR/wildcard and port dimensions.
bool net_target_contained(const std::string& app, const std::string& manifest);

} // namespace capsid

#endif
