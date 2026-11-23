#include "host/units.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace capsid::host::units {
namespace {

bool parse_scaled_u64(std::string_view digits, std::uint64_t multiplier, std::uint64_t* out) {
    if (digits.empty() || multiplier == 0 || out == nullptr) {
        return false;
    }
    const std::uint64_t max_u64 = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t value = 0;
    for (const char c : digits) {
        if (c < '0' || c > '9') {
            return false;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
        if (value > (max_u64 - digit) / 10ULL) {
            return false;
        }
        value = value * 10ULL + digit;
    }
    if (value > max_u64 / multiplier) {
        return false;
    }
    *out = value * multiplier;
    return true;
}

} // namespace

bool parse_size_bytes(std::string_view text, std::uint64_t* out, bool allow_bare_bytes) {
    if (out == nullptr || text.size() < 2) {
        return false;
    }
    struct Suffix {
        std::string_view suffix;
        std::uint64_t multiplier;
    };
    static constexpr Suffix kSuffixes[] = {
        {"KiB", 1024ULL}, {"MiB", 1024ULL * 1024ULL}, {"GiB", 1024ULL * 1024ULL * 1024ULL},
        {"KB", 1000ULL},  {"MB", 1000ULL * 1000ULL},  {"GB", 1000ULL * 1000ULL * 1000ULL},
        {"B", 1ULL},
    };
    for (const auto& entry : kSuffixes) {
        if (entry.suffix == "B" && !allow_bare_bytes) {
            continue;
        }
        if (text.size() > entry.suffix.size() &&
            text.compare(text.size() - entry.suffix.size(), entry.suffix.size(), entry.suffix) ==
                0) {
            return parse_scaled_u64(text.substr(0, text.size() - entry.suffix.size()),
                                    entry.multiplier, out);
        }
    }
    return false;
}

bool parse_duration_ms(std::string_view text, std::uint64_t* out, bool allow_hours) {
    if (out == nullptr || text.size() < 2) {
        return false;
    }
    struct Suffix {
        std::string_view suffix;
        std::uint64_t multiplier;
    };
    static constexpr Suffix kSuffixes[] = {
        {"ms", 1ULL},
        {"s", 1000ULL},
        {"m", 60ULL * 1000ULL},
        {"h", 60ULL * 60ULL * 1000ULL},
    };
    for (const auto& entry : kSuffixes) {
        if (entry.suffix == "h" && !allow_hours) {
            continue;
        }
        if (text.size() > entry.suffix.size() &&
            text.compare(text.size() - entry.suffix.size(), entry.suffix.size(), entry.suffix) ==
                0) {
            return parse_scaled_u64(text.substr(0, text.size() - entry.suffix.size()),
                                    entry.multiplier, out);
        }
    }
    return false;
}

bool normalize_posix_path(std::string_view path, std::string* out) {
    if (out == nullptr || path.empty() || path[0] != '/' ||
        path.find('\0') != std::string_view::npos) {
        return false;
    }
    std::vector<std::string_view> components;
    std::size_t i = 0;
    while (i < path.size()) {
        while (i < path.size() && path[i] == '/') {
            ++i;
        }
        if (i >= path.size()) {
            break;
        }
        const std::size_t start = i;
        while (i < path.size() && path[i] != '/') {
            ++i;
        }
        const std::string_view part = path.substr(start, i - start);
        if (part == ".") {
            continue;
        }
        if (part == "..") {
            return false;
        }
        components.push_back(part);
    }
    if (components.empty()) {
        *out = "/";
        return true;
    }
    std::string normalized;
    normalized.reserve(path.size());
    for (const std::string_view part : components) {
        normalized.push_back('/');
        normalized.append(part.data(), part.size());
    }
    *out = std::move(normalized);
    return true;
}

bool fs_path_contained(std::string_view app, std::string_view manifest) {
    std::string norm_app;
    std::string norm_manifest;
    if (!normalize_posix_path(app, &norm_app) || !normalize_posix_path(manifest, &norm_manifest)) {
        return false;
    }
    if (norm_app == norm_manifest) {
        return true;
    }
    if (norm_manifest == "/") {
        return true;
    }
    return norm_app.size() > norm_manifest.size() &&
           norm_app.compare(0, norm_manifest.size(), norm_manifest) == 0 &&
           norm_app[norm_manifest.size()] == '/';
}

} // namespace capsid::host::units
