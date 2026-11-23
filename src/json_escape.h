#ifndef CAPSID_JSON_ESCAPE_H
#define CAPSID_JSON_ESCAPE_H

#include <cstddef>
#include <cstring>
#include <string>
#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) >= 201703L
#include <string_view>
#endif

namespace capsid {

// Escapes arbitrary bytes for placement inside a JSON string literal per
// RFC 8259 §7: quotes, backslashes, named controls (\b,\f,\n,\r,\t), and all
// remaining control bytes < 0x20 as lowercase \u00xx.
inline std::string json_escape(const char* data, std::size_t size) {
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(size + 8);
    for (std::size_t i = 0; i < size; ++i) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20) {
                char buf[7] = {'\\', 'u', '0', '0', kHex[c >> 4], kHex[c & 0x0f], '\0'};
                out.append(buf, 6);
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    return out;
}

#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) >= 201703L
inline std::string json_escape(std::string_view value) {
    return json_escape(value.data(), value.size());
}
#else
inline std::string json_escape(const std::string& value) {
    return json_escape(value.data(), value.size());
}

inline std::string json_escape(const char* value) {
    return value ? json_escape(value, std::strlen(value)) : std::string();
}
#endif

// Escapes one reference token for an RFC 6901 JSON Pointer ('~' -> '~0',
// '/' -> '~1').
inline std::string json_pointer_escape(const char* data, std::size_t size) {
    std::string out;
    out.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        const char c = data[i];
        if (c == '~') {
            out += "~0";
        } else if (c == '/') {
            out += "~1";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) >= 201703L
inline std::string json_pointer_escape(std::string_view component) {
    return json_pointer_escape(component.data(), component.size());
}
#else
inline std::string json_pointer_escape(const std::string& component) {
    return json_pointer_escape(component.data(), component.size());
}

inline std::string json_pointer_escape(const char* component) {
    return component ? json_pointer_escape(component, std::strlen(component)) : std::string();
}
#endif

} // namespace capsid

#endif
