#ifndef CAPSID_HOST_UNITS_H
#define CAPSID_HOST_UNITS_H

#include <cstdint>
#include <string>
#include <string_view>

namespace capsid::host::units {

// Parses an unsigned integer byte size with an explicit unit suffix:
// "KiB", "MiB", "GiB", "KB", "MB", "GB" (and optionally "B").
// Rejects bare numbers, whitespace, '+'/'-' signs, non-digits, and overflow.
bool parse_size_bytes(std::string_view text, std::uint64_t* out, bool allow_bare_bytes = true);

// Parses an unsigned integer duration in milliseconds with an explicit suffix:
// "ms", "s", "m" (and optionally "h").
// Rejects bare numbers, whitespace, '+'/'-' signs, non-digits, and overflow.
bool parse_duration_ms(std::string_view text, std::uint64_t* out, bool allow_hours = true);

// Lexically normalizes an absolute POSIX path (resolving ".", collapsing "//",
// stripping trailing slashes except "/"). Returns false on relative paths,
// ".." traversal components, NUL bytes, or empty input.
bool normalize_posix_path(std::string_view path, std::string* out);

// Returns true iff normalized `app` equals `manifest` or resides strictly inside
// `manifest` at a '/' component boundary.
bool fs_path_contained(std::string_view app, std::string_view manifest);

} // namespace capsid::host::units

#endif
