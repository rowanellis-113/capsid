#ifndef CAPSID_HOST_JSON_CODEC_H
#define CAPSID_HOST_JSON_CODEC_H

#include <jansson.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace capsid::host {

struct JsonDeleter {
    void operator()(json_t* value) const noexcept {
        if (value != nullptr) {
            json_decref(value);
        }
    }
};

using UniqueJson = std::unique_ptr<json_t, JsonDeleter>;

enum class StrictJsonError {
    kNone,
    kTooLarge,
    kTooDeep,
    kInvalidUtf8,
    kDuplicateKey,
    kInvalidSyntax,
};

struct StrictJsonResult {
    bool ok = false;
    StrictJsonError error = StrictJsonError::kNone;
    std::string message;
    UniqueJson root;
};

// Strictly parses UTF-8 JSON with upfront byte and nesting ceilings (checked
// before DOM allocation), rejecting duplicate object keys, comments, and
// trailing tokens. `allow_nul` forwards JSON_ALLOW_NUL for configs that permit
// NUL escapes before downstream field validation.
StrictJsonResult parse_strict_json(std::string_view json, std::size_t max_bytes,
                                   std::size_t max_nesting, bool allow_nul = true);

// Validates raw UTF-8 byte sequence (disallowing overlong encodings, UTF-16
// surrogates U+D800..U+DFFF, and codepoints above U+10FFFF).
bool is_valid_utf8(std::string_view text);

// Lowercase 64-character hex SHA-256 without "sha256:" prefix.
std::string sha256_raw_hex(std::span<const std::uint8_t> bytes);
inline std::string sha256_raw_hex(std::string_view text) {
    return sha256_raw_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

// Lowercase "sha256:<64-hex>" digest over `bytes` / `text`.
std::string sha256_hex(std::span<const std::uint8_t> bytes);
inline std::string sha256_hex(std::string_view text) {
    return sha256_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

} // namespace capsid::host

#endif
