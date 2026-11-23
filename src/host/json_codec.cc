#include "host/json_codec.h"

#include <openssl/evp.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace capsid::host {

bool is_valid_utf8(std::string_view text) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    const std::size_t n = text.size();
    std::size_t i = 0;
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

namespace {

bool check_nesting_before_dom(std::string_view json, std::size_t max_nesting) {
    std::size_t depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (const char ch : json) {
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
            continue;
        }
        if (ch == '{' || ch == '[') {
            ++depth;
            if (depth > max_nesting) {
                return false;
            }
        } else if (ch == '}' || ch == ']') {
            if (depth > 0) {
                --depth;
            }
        }
    }
    return true;
}

} // namespace

StrictJsonResult parse_strict_json(std::string_view json, std::size_t max_bytes,
                                   std::size_t max_nesting, bool allow_nul) {
    StrictJsonResult result;
    if (json.size() > max_bytes) {
        result.error = StrictJsonError::kTooLarge;
        result.message = "JSON document exceeds maximum byte limit";
        return result;
    }
    if (!is_valid_utf8(json)) {
        result.error = StrictJsonError::kInvalidUtf8;
        result.message = "JSON document is not valid UTF-8";
        return result;
    }
    if (!check_nesting_before_dom(json, max_nesting)) {
        result.error = StrictJsonError::kTooDeep;
        result.message = "JSON document exceeds maximum nesting depth";
        return result;
    }

    const std::size_t flags = static_cast<std::size_t>(JSON_REJECT_DUPLICATES) |
                              (allow_nul ? static_cast<std::size_t>(JSON_ALLOW_NUL) : 0U);
    json_error_t parse_error;
    UniqueJson root(json_loadb(json.data(), json.size(), flags, &parse_error));
    if (!root) {
        const enum json_error_code code = json_error_code(&parse_error);
        result.error = code == json_error_duplicate_key ? StrictJsonError::kDuplicateKey
                                                        : StrictJsonError::kInvalidSyntax;
        result.message = parse_error.text[0] != '\0' ? parse_error.text : "invalid JSON document";
        return result;
    }
    result.ok = true;
    result.root = std::move(root);
    return result;
}

std::string sha256_raw_hex(std::span<const std::uint8_t> bytes) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size = 0;
    if (EVP_Digest(bytes.data(), bytes.size(), digest, &digest_size, EVP_sha256(), nullptr) != 1 ||
        digest_size != 32) {
        return {};
    }
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (unsigned int i = 0; i < digest_size; ++i) {
        result.push_back(kHex[digest[i] >> 4]);
        result.push_back(kHex[digest[i] & 0x0f]);
    }
    return result;
}

std::string sha256_hex(std::span<const std::uint8_t> bytes) {
    const std::string hex = sha256_raw_hex(bytes);
    if (hex.empty()) {
        return {};
    }
    return "sha256:" + hex;
}

} // namespace capsid::host
