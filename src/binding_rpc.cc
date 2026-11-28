// Binding RPC v2 (v0.4.0): structured clone, tamper-proof remote error
// propagation, call options parsing, and AbortSignal bridging between the User
// and Binding Runtimes.
//
// Only plain data crosses the boundary. The clone walks own enumerable
// properties via JS_GetOwnProperty (which returns accessor functions
// without invoking them — getters are detected and rejected, never
// executed). Proxies are rejected by class identity before any property
// read can trigger a trap; Map/Set/Weak collections, Error, RegExp and
// custom class instances are rejected by prototype/class checks.

#include "binding_rpc.h"

#include "quickjs.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>

namespace capsid {
namespace {

constexpr double kMaxBindingTimeoutMs = 31536000000.0; // 1 year; avoids ns overflow

std::size_t saturating_add(std::size_t a, std::size_t b) {
    const std::size_t max_val = (std::numeric_limits<std::size_t>::max)();
    return (b > max_val - a) ? max_val : (a + b);
}

struct CloneContext {
    JSContext* ctx;
    std::size_t depth;
    std::size_t nodes;
    std::size_t bytes;
    std::unordered_set<const void*> ancestors; // cycle detection
    JSValue object_proto;                      // Object.prototype, referenced
    const BindingCloneHooks* hooks;
};

bool charge_clone_bytes(CloneContext* context, std::size_t delta, std::string* error) {
    if (delta == 0 || context == NULL || context->hooks == NULL ||
        context->hooks->reserve_bytes == NULL) {
        return true;
    }
    return context->hooks->reserve_bytes(delta, error, context->hooks->opaque);
}

bool clone_value(JSContext* ctx, JSValueConst value, NeutralValue* out, std::string* error,
                 CloneContext* context);

bool fail_closed(std::string* error, const char* message) {
    if (error != NULL) {
        *error = message;
    }
    return false;
}

void clear_pending_exception(JSContext* ctx) {
    JSValue exc = JS_GetException(ctx);
    JS_FreeValue(ctx, exc);
}

std::string clamp_utf8_text(const char* text, std::size_t len, std::size_t max_bytes) {
    if (text == NULL || len == 0 || max_bytes == 0) {
        return std::string();
    }
    if (len <= max_bytes) {
        return std::string(text, len);
    }
    std::size_t cut = max_bytes;
    // Do not split a multi-byte UTF-8 sequence across the truncation point.
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0U) == 0x80U) {
        --cut;
    }
    return std::string(text, cut);
}

bool read_js_string_bounded(JSContext* ctx, JSValueConst value, std::size_t max_bytes,
                            std::string* out) {
    if (!JS_IsString(value)) {
        return false;
    }
    size_t len = 0;
    const char* text = JS_ToCStringLen(ctx, &len, value);
    if (text == NULL) {
        clear_pending_exception(ctx);
        return false;
    }
    *out = clamp_utf8_text(text, len, max_bytes);
    JS_FreeCString(ctx, text);
    return true;
}

bool read_own_data_property(JSContext* ctx, JSValueConst object, const char* name,
                            JSValue* out_value, bool* out_was_accessor) {
    if (out_was_accessor != NULL) {
        *out_was_accessor = false;
    }
    *out_value = JS_UNDEFINED;
    JSAtom atom = JS_NewAtom(ctx, name);
    if (atom == JS_ATOM_NULL) {
        clear_pending_exception(ctx);
        return false;
    }
    JSPropertyDescriptor desc;
    const int present = JS_GetOwnProperty(ctx, &desc, object, atom);
    JS_FreeAtom(ctx, atom);
    if (present <= 0) {
        if (present < 0) {
            clear_pending_exception(ctx);
        }
        return false;
    }
    const bool is_accessor = !JS_IsUndefined(desc.getter) || !JS_IsUndefined(desc.setter);
    JS_FreeValue(ctx, desc.getter);
    JS_FreeValue(ctx, desc.setter);
    if (is_accessor) {
        JS_FreeValue(ctx, desc.value);
        if (out_was_accessor != NULL) {
            *out_was_accessor = true;
        }
        return false;
    }
    *out_value = desc.value;
    return true;
}

bool read_own_getter(JSContext* ctx, JSValueConst object, const char* name, JSValue* out_getter) {
    *out_getter = JS_UNDEFINED;
    JSAtom atom = JS_NewAtom(ctx, name);
    if (atom == JS_ATOM_NULL) {
        clear_pending_exception(ctx);
        return false;
    }
    JSPropertyDescriptor desc;
    const int present = JS_GetOwnProperty(ctx, &desc, object, atom);
    JS_FreeAtom(ctx, atom);
    if (present <= 0) {
        if (present < 0) {
            clear_pending_exception(ctx);
        }
        return false;
    }
    JS_FreeValue(ctx, desc.value);
    JS_FreeValue(ctx, desc.setter);
    if (JS_IsFunction(ctx, desc.getter)) {
        *out_getter = desc.getter;
        return true;
    }
    JS_FreeValue(ctx, desc.getter);
    return false;
}

bool capture_error_pair(JSContext* ctx, JSValueConst global, const char* name, JSValue* out_ctor,
                        JSValue* out_proto) {
    JSValue ctor = JS_GetPropertyStr(ctx, global, name);
    if (JS_IsException(ctor)) {
        clear_pending_exception(ctx);
        return false;
    }
    if (!JS_IsConstructor(ctx, ctor)) {
        JS_FreeValue(ctx, ctor);
        return false;
    }
    JSValue proto = JS_GetPropertyStr(ctx, ctor, "prototype");
    if (JS_IsException(proto) || !JS_IsObject(proto)) {
        if (JS_IsException(proto)) {
            clear_pending_exception(ctx);
        }
        JS_FreeValue(ctx, proto);
        JS_FreeValue(ctx, ctor);
        return false;
    }
    *out_ctor = ctor;
    *out_proto = proto;
    return true;
}

// A fresh object literal reaches the runtime's intrinsic Object.prototype
// without consulting a mutable global constructor. Every exotic kind uses
// QuickJS's native predicate below; no attacker-controlled Symbol.hasInstance
// or replaced global constructor participates in type classification.
bool learn_class_ids(JSContext* ctx, CloneContext* context) {
    JSValue plain = JS_Eval(ctx, "({})", 4, "<clone-probe>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(plain)) {
        JS_FreeValue(ctx, plain);
        return false;
    }
    context->object_proto = JS_GetPrototype(ctx, plain);
    JS_FreeValue(ctx, plain);
    if (JS_IsException(context->object_proto)) {
        JS_FreeValue(ctx, context->object_proto);
        return false;
    }
    return true;
}

bool clone_string(JSContext* ctx, JSValueConst value, std::size_t weight_overhead,
                  std::string* target, std::string* error, CloneContext* context) {
    size_t size = 0;
    const char* text = JS_ToCStringLen(ctx, &size, value);
    if (text == NULL) {
        clear_pending_exception(ctx);
        return fail_closed(error, "unprintable string");
    }
    if (size > kMaxBindingCloneBytes - context->bytes) {
        JS_FreeCString(ctx, text);
        return fail_closed(error, "clone byte limit exceeded");
    }
    if (!charge_clone_bytes(context, saturating_add(size, weight_overhead), error)) {
        JS_FreeCString(ctx, text);
        return false;
    }
    target->assign(text, size);
    JS_FreeCString(ctx, text);
    context->bytes += size;
    return true;
}

bool reject_enumerable_symbol_keys(JSContext* ctx, JSValueConst value, std::string* error) {
    JSPropertyEnum* properties = NULL;
    uint32_t count = 0;
    if (JS_GetOwnPropertyNames(ctx, &properties, &count, value,
                               JS_GPN_ENUM_ONLY | JS_GPN_SYMBOL_MASK) < 0) {
        return fail_closed(error, "object properties are unreadable");
    }
    JS_FreePropertyEnum(ctx, properties, count);
    return count == 0 || fail_closed(error, "symbol property keys are not cloneable");
}

bool clone_value(JSContext* ctx, JSValueConst value, NeutralValue* out, std::string* error,
                 CloneContext* context) {
    if (context->depth > kMaxBindingCloneDepth) {
        return fail_closed(error, "clone depth limit exceeded");
    }
    if (++context->nodes > kMaxBindingCloneNodes) {
        return fail_closed(error, "clone node limit exceeded");
    }

    if (JS_IsUndefined(value)) {
        if (!charge_clone_bytes(context, 1, error)) {
            return false;
        }
        out->kind = NeutralValue::Kind::kUndefined;
        return true;
    }
    if (JS_IsNull(value)) {
        if (!charge_clone_bytes(context, 1, error)) {
            return false;
        }
        out->kind = NeutralValue::Kind::kNull;
        return true;
    }
    if (JS_IsBool(value)) {
        if (!charge_clone_bytes(context, 1, error)) {
            return false;
        }
        out->kind = NeutralValue::Kind::kBool;
        out->bool_value = JS_ToBool(ctx, value) != 0;
        return true;
    }
    if (JS_IsNumber(value)) {
        out->kind = NeutralValue::Kind::kNumber;
        if (JS_ToFloat64(ctx, &out->number, value) != 0) {
            return fail_closed(error, "unrepresentable number");
        }
        return charge_clone_bytes(context, 8, error);
    }
    if (JS_IsBigInt(value)) {
        out->kind = NeutralValue::Kind::kBigInt;
        if (!clone_string(ctx, value, 8, &out->bigint, error, context)) {
            return false;
        }
        return true;
    }
    if (JS_IsString(value)) {
        out->kind = NeutralValue::Kind::kString;
        return clone_string(ctx, value, 1, &out->string, error, context);
    }
    if (JS_IsSymbol(value)) {
        return fail_closed(error, "symbol values are not cloneable");
    }
    if (JS_IsDate(value)) {
        out->kind = NeutralValue::Kind::kDate;
        if (JS_GetDateValue(ctx, value, &out->date_ms) != 0) {
            return fail_closed(error, "date value is unreadable");
        }
        return charge_clone_bytes(context, 8, error);
    }
    if (JS_IsError(value)) {
        return fail_closed(error, "Error values are not cloneable");
    }
    if (JS_IsRegExp(value)) {
        return fail_closed(error, "RegExp values are not cloneable");
    }
    if (JS_IsPromise(value)) {
        return fail_closed(error, "promise values are not cloneable");
    }
    if (JS_IsFunction(ctx, value)) {
        return fail_closed(error, "function values are not cloneable");
    }
    if (!JS_IsObject(value)) {
        return fail_closed(error, "value kind is not cloneable");
    }

    if (JS_IsArrayBuffer(value)) {
        out->kind = NeutralValue::Kind::kBytes;
        size_t size = 0;
        uint8_t* data = JS_GetArrayBuffer(ctx, &size, value);
        if (data == NULL) {
            return fail_closed(error, "array buffer is unreadable");
        }
        if (size > kMaxBindingCloneBytes - context->bytes) {
            return fail_closed(error, "clone byte limit exceeded");
        }
        if (!charge_clone_bytes(context, size, error)) {
            return false;
        }
        out->bytes.assign(data, data + size);
        context->bytes += size;
        return true;
    }
    if (JS_GetTypedArrayType(value) == JS_TYPED_ARRAY_UINT8) {
        out->kind = NeutralValue::Kind::kBytes;
        size_t typed_size = 0;
        uint8_t* typed = JS_GetUint8Array(ctx, &typed_size, value);
        if (typed == NULL) {
            return fail_closed(error, "byte array is unreadable");
        }
        if (typed_size > kMaxBindingCloneBytes - context->bytes) {
            return fail_closed(error, "clone byte limit exceeded");
        }
        if (!charge_clone_bytes(context, typed_size, error)) {
            return false;
        }
        out->bytes.assign(typed, typed + typed_size);
        context->bytes += typed_size;
        return true;
    }

    if (JS_IsProxy(value)) {
        return fail_closed(error, "Proxy values are not cloneable");
    }
    if (JS_IsMap(value)) {
        return fail_closed(error, "Map values are not cloneable");
    }
    if (JS_IsSet(value)) {
        return fail_closed(error, "Set values are not cloneable");
    }
    if (JS_IsWeakMap(value)) {
        return fail_closed(error, "WeakMap values are not cloneable");
    }
    if (JS_IsWeakSet(value)) {
        return fail_closed(error, "WeakSet values are not cloneable");
    }
    if (context->hooks != NULL && context->hooks->try_capture != NULL) {
        const BindingCloneHookResult hooked =
            context->hooks->try_capture(ctx, value, out, error, context->hooks->opaque);
        if (hooked == BindingCloneHookResult::kCaptured) {
            return true;
        }
        if (hooked == BindingCloneHookResult::kFailed) {
            return false;
        }
    }
    if (!reject_enumerable_symbol_keys(ctx, value, error)) {
        return false;
    }
    if (!JS_IsArray(value)) {
        JSValue proto = JS_GetPrototype(ctx, value);
        const bool plain = !JS_IsException(proto) &&
                           (JS_IsNull(proto) ||
                            JS_VALUE_GET_PTR(proto) == JS_VALUE_GET_PTR(context->object_proto));
        JS_FreeValue(ctx, proto);
        if (!plain) {
            return fail_closed(error, "class instance values are not cloneable");
        }
    }

    const void* identity = static_cast<const void*>(JS_VALUE_GET_PTR(value));
    if (!context->ancestors.insert(identity).second) {
        return fail_closed(error, "cyclic values are not cloneable");
    }

    if (JS_IsArray(value)) {
        out->kind = NeutralValue::Kind::kArray;
        uint32_t length = 0;
        JSValue length_value = JS_GetPropertyStr(ctx, value, "length");
        if (JS_IsException(length_value) || JS_ToUint32(ctx, &length, length_value) != 0) {
            JS_FreeValue(ctx, length_value);
            context->ancestors.erase(identity);
            return fail_closed(error, "array length is unreadable");
        }
        JS_FreeValue(ctx, length_value);
        if (!charge_clone_bytes(context, 8, error)) {
            context->ancestors.erase(identity);
            return false;
        }
        ++context->depth;
        for (uint32_t index = 0; index < length; ++index) {
            JSAtom atom = JS_NewAtomUInt32(ctx, index);
            JSPropertyDescriptor descriptor;
            const int present = JS_GetOwnProperty(ctx, &descriptor, value, atom);
            JS_FreeAtom(ctx, atom);
            if (present < 0) {
                --context->depth;
                context->ancestors.erase(identity);
                return fail_closed(error, "array element is unreadable");
            }
            JSValue element = JS_UNDEFINED;
            if (present > 0) {
                const bool is_accessor =
                    !JS_IsUndefined(descriptor.getter) || !JS_IsUndefined(descriptor.setter);
                element = descriptor.value;
                JS_FreeValue(ctx, descriptor.getter);
                JS_FreeValue(ctx, descriptor.setter);
                if (is_accessor) {
                    JS_FreeValue(ctx, element);
                    --context->depth;
                    context->ancestors.erase(identity);
                    return fail_closed(error, "accessor properties are not cloneable");
                }
            }
            NeutralValue child;
            const bool ok = clone_value(ctx, element, &child, error, context);
            JS_FreeValue(ctx, element);
            if (!ok) {
                --context->depth;
                context->ancestors.erase(identity);
                return false;
            }
            out->array.push_back(std::move(child));
        }
        --context->depth;
        context->ancestors.erase(identity);
        return true;
    }

    out->kind = NeutralValue::Kind::kObject;
    JSPropertyEnum* tab = NULL;
    uint32_t count = 0;
    if (JS_GetOwnPropertyNames(ctx, &tab, &count, value, JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK) <
        0) {
        context->ancestors.erase(identity);
        return fail_closed(error, "object properties are unreadable");
    }
    if (!charge_clone_bytes(context, 8, error)) {
        JS_FreePropertyEnum(ctx, tab, count);
        context->ancestors.erase(identity);
        return false;
    }
    ++context->depth;
    for (uint32_t index = 0; index < count; ++index) {
        size_t key_len = 0;
        const char* key = JS_AtomToCStringLen(ctx, &key_len, tab[index].atom);
        if (key == NULL) {
            clear_pending_exception(ctx);
            JS_FreePropertyEnum(ctx, tab, count);
            --context->depth;
            context->ancestors.erase(identity);
            return fail_closed(error, "object property key is unreadable");
        }
        if (key_len > kMaxBindingCloneBytes - context->bytes) {
            JS_FreeCString(ctx, key);
            JS_FreePropertyEnum(ctx, tab, count);
            --context->depth;
            context->ancestors.erase(identity);
            return fail_closed(error, "clone byte limit exceeded");
        }
        if (!charge_clone_bytes(context, key_len, error)) {
            JS_FreeCString(ctx, key);
            JS_FreePropertyEnum(ctx, tab, count);
            --context->depth;
            context->ancestors.erase(identity);
            return false;
        }
        std::string key_string(key, key_len);
        JS_FreeCString(ctx, key);
        context->bytes += key_len;
        JSPropertyDescriptor descriptor;
        if (JS_GetOwnProperty(ctx, &descriptor, value, tab[index].atom) < 0) {
            JS_FreePropertyEnum(ctx, tab, count);
            --context->depth;
            context->ancestors.erase(identity);
            return fail_closed(error, "object property is unreadable");
        }
        const bool is_accessor =
            !JS_IsUndefined(descriptor.getter) || !JS_IsUndefined(descriptor.setter);
        const JSValue member = descriptor.value;
        if (is_accessor) {
            JS_FreeValue(ctx, descriptor.getter);
            JS_FreeValue(ctx, descriptor.setter);
            JS_FreeValue(ctx, member);
            JS_FreePropertyEnum(ctx, tab, count);
            --context->depth;
            context->ancestors.erase(identity);
            return fail_closed(error, "accessor properties are not cloneable");
        }
        JS_FreeValue(ctx, descriptor.getter);
        JS_FreeValue(ctx, descriptor.setter);
        NeutralValue child;
        const bool ok = clone_value(ctx, member, &child, error, context);
        JS_FreeValue(ctx, member);
        if (!ok) {
            JS_FreePropertyEnum(ctx, tab, count);
            --context->depth;
            context->ancestors.erase(identity);
            return false;
        }
        out->object.push_back(std::make_pair(std::move(key_string), std::move(child)));
    }
    JS_FreePropertyEnum(ctx, tab, count);
    --context->depth;
    context->ancestors.erase(identity);
    return true;
}

RemoteErrorKind classify_error_prototype(JSContext* ctx, JSValueConst value,
                                         const BindingRpcIntrinsics* intrinsics) {
    if (intrinsics == NULL) {
        return RemoteErrorKind::kError;
    }
    JSValue cur = JS_GetPrototype(ctx, value);
    for (int depth = 0; depth < 8 && JS_IsObject(cur); ++depth) {
        const void* ptr = JS_VALUE_GET_PTR(cur);
        if (JS_IsObject(intrinsics->type_error_proto) &&
            ptr == JS_VALUE_GET_PTR(intrinsics->type_error_proto)) {
            JS_FreeValue(ctx, cur);
            return RemoteErrorKind::kTypeError;
        }
        if (JS_IsObject(intrinsics->range_error_proto) &&
            ptr == JS_VALUE_GET_PTR(intrinsics->range_error_proto)) {
            JS_FreeValue(ctx, cur);
            return RemoteErrorKind::kRangeError;
        }
        if (JS_IsObject(intrinsics->syntax_error_proto) &&
            ptr == JS_VALUE_GET_PTR(intrinsics->syntax_error_proto)) {
            JS_FreeValue(ctx, cur);
            return RemoteErrorKind::kSyntaxError;
        }
        if (JS_IsObject(intrinsics->uri_error_proto) &&
            ptr == JS_VALUE_GET_PTR(intrinsics->uri_error_proto)) {
            JS_FreeValue(ctx, cur);
            return RemoteErrorKind::kURIError;
        }
        if (JS_IsObject(intrinsics->reference_error_proto) &&
            ptr == JS_VALUE_GET_PTR(intrinsics->reference_error_proto)) {
            JS_FreeValue(ctx, cur);
            return RemoteErrorKind::kReferenceError;
        }
        if (JS_IsObject(intrinsics->eval_error_proto) &&
            ptr == JS_VALUE_GET_PTR(intrinsics->eval_error_proto)) {
            JS_FreeValue(ctx, cur);
            return RemoteErrorKind::kEvalError;
        }
        if (JS_IsObject(intrinsics->error_proto) &&
            ptr == JS_VALUE_GET_PTR(intrinsics->error_proto)) {
            JS_FreeValue(ctx, cur);
            return RemoteErrorKind::kError;
        }
        JSValue next = JS_GetPrototype(ctx, cur);
        JS_FreeValue(ctx, cur);
        if (JS_IsException(next)) {
            clear_pending_exception(ctx);
            return RemoteErrorKind::kError;
        }
        cur = next;
    }
    if (JS_IsException(cur)) {
        clear_pending_exception(ctx);
    } else {
        JS_FreeValue(ctx, cur);
    }
    return RemoteErrorKind::kError;
}

const char* default_error_name(RemoteErrorKind kind) {
    switch (kind) {
    case RemoteErrorKind::kError:
        return "Error";
    case RemoteErrorKind::kTypeError:
        return "TypeError";
    case RemoteErrorKind::kRangeError:
        return "RangeError";
    case RemoteErrorKind::kSyntaxError:
        return "SyntaxError";
    case RemoteErrorKind::kURIError:
        return "URIError";
    case RemoteErrorKind::kReferenceError:
        return "ReferenceError";
    case RemoteErrorKind::kEvalError:
        return "EvalError";
    case RemoteErrorKind::kDOMException:
        return "Error";
    case RemoteErrorKind::kQuotaExceededError:
        return "QuotaExceededError";
    }
    return "Error";
}

} // namespace

RemoteErrorInfo RemoteErrorInfo::make_error(const std::string& message) {
    RemoteErrorInfo info;
    info.kind = RemoteErrorKind::kError;
    info.name = "Error";
    info.message = clamp_utf8_text(message.data(), message.size(), kMaxRemoteErrorMessageBytes);
    return info;
}

RemoteErrorInfo RemoteErrorInfo::make_type_error(const std::string& message) {
    RemoteErrorInfo info;
    info.kind = RemoteErrorKind::kTypeError;
    info.name = "TypeError";
    info.message = clamp_utf8_text(message.data(), message.size(), kMaxRemoteErrorMessageBytes);
    return info;
}

RemoteErrorInfo RemoteErrorInfo::make_dom_exception(const std::string& name,
                                                    const std::string& message) {
    RemoteErrorInfo info;
    info.kind = RemoteErrorKind::kDOMException;
    info.name = clamp_utf8_text(name.data(), name.size(), kMaxRemoteErrorNameBytes);
    info.message = clamp_utf8_text(message.data(), message.size(), kMaxRemoteErrorMessageBytes);
    return info;
}

bool capture_binding_rpc_intrinsics(JSContext* ctx, BindingRpcIntrinsics* out) {
    if (ctx == NULL || out == NULL) {
        return false;
    }
    free_binding_rpc_intrinsics(ctx, out);

    JSValue plain = JS_Eval(ctx, "({})", 4, "<intrinsics-probe>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(plain)) {
        clear_pending_exception(ctx);
        return false;
    }
    out->object_proto = JS_GetPrototype(ctx, plain);
    JS_FreeValue(ctx, plain);
    if (JS_IsException(out->object_proto) || !JS_IsObject(out->object_proto)) {
        if (JS_IsException(out->object_proto)) {
            clear_pending_exception(ctx);
        }
        free_binding_rpc_intrinsics(ctx, out);
        return false;
    }

    JSValue global = JS_GetGlobalObject(ctx);
    const bool core_ok =
        capture_error_pair(ctx, global, "Error", &out->error_ctor, &out->error_proto) &&
        capture_error_pair(ctx, global, "TypeError", &out->type_error_ctor,
                           &out->type_error_proto) &&
        capture_error_pair(ctx, global, "RangeError", &out->range_error_ctor,
                           &out->range_error_proto) &&
        capture_error_pair(ctx, global, "SyntaxError", &out->syntax_error_ctor,
                           &out->syntax_error_proto) &&
        capture_error_pair(ctx, global, "URIError", &out->uri_error_ctor, &out->uri_error_proto) &&
        capture_error_pair(ctx, global, "ReferenceError", &out->reference_error_ctor,
                           &out->reference_error_proto) &&
        capture_error_pair(ctx, global, "EvalError", &out->eval_error_ctor, &out->eval_error_proto);
    if (!core_ok) {
        JS_FreeValue(ctx, global);
        free_binding_rpc_intrinsics(ctx, out);
        return false;
    }

    // Optional Web Platform intrinsics (always present in full Capsid worker
    // bootstrap; may be absent in minimal standalone QuickJS unit tests).
    JSValue dom_ctor = JS_GetPropertyStr(ctx, global, "DOMException");
    if (JS_IsException(dom_ctor)) {
        clear_pending_exception(ctx);
    } else if (JS_IsConstructor(ctx, dom_ctor)) {
        JSValue dom_proto = JS_GetPropertyStr(ctx, dom_ctor, "prototype");
        if (!JS_IsException(dom_proto) && JS_IsObject(dom_proto)) {
            JSValue name_get = JS_UNDEFINED;
            JSValue msg_get = JS_UNDEFINED;
            if (read_own_getter(ctx, dom_proto, "name", &name_get) &&
                read_own_getter(ctx, dom_proto, "message", &msg_get)) {
                out->dom_exception_ctor = JS_DupValue(ctx, dom_ctor);
                out->dom_exception_name_getter = name_get;
                out->dom_exception_message_getter = msg_get;
            } else {
                JS_FreeValue(ctx, name_get);
                JS_FreeValue(ctx, msg_get);
            }
        } else if (JS_IsException(dom_proto)) {
            clear_pending_exception(ctx);
        }
        JS_FreeValue(ctx, dom_proto);
    }
    JS_FreeValue(ctx, dom_ctor);

    JSValue quota_ctor = JS_GetPropertyStr(ctx, global, "QuotaExceededError");
    if (JS_IsException(quota_ctor)) {
        clear_pending_exception(ctx);
    } else if (JS_IsConstructor(ctx, quota_ctor)) {
        JSValue capture_fn = JS_UNDEFINED;
        bool was_accessor = false;
        if (read_own_data_property(ctx, quota_ctor, "_capture", &capture_fn, &was_accessor) &&
            JS_IsFunction(ctx, capture_fn)) {
            out->quota_exceeded_ctor = JS_DupValue(ctx, quota_ctor);
            out->quota_exceeded_capture = capture_fn;
        } else {
            JS_FreeValue(ctx, capture_fn);
        }
    }
    JS_FreeValue(ctx, quota_ctor);

    JSValue signal_ctor = JS_GetPropertyStr(ctx, global, "AbortSignal");
    if (JS_IsException(signal_ctor)) {
        clear_pending_exception(ctx);
    } else if (JS_IsObject(signal_ctor)) {
        JSValue signal_proto = JS_GetPropertyStr(ctx, signal_ctor, "prototype");
        if (!JS_IsException(signal_proto) && JS_IsObject(signal_proto)) {
            JSValue aborted_get = JS_UNDEFINED;
            JSValue reason_get = JS_UNDEFINED;
            if (read_own_getter(ctx, signal_proto, "aborted", &aborted_get) &&
                read_own_getter(ctx, signal_proto, "reason", &reason_get)) {
                out->abort_signal_aborted_getter = aborted_get;
                out->abort_signal_reason_getter = reason_get;
            } else {
                JS_FreeValue(ctx, aborted_get);
                JS_FreeValue(ctx, reason_get);
            }
        } else if (JS_IsException(signal_proto)) {
            clear_pending_exception(ctx);
        }
        JS_FreeValue(ctx, signal_proto);
    }
    JS_FreeValue(ctx, signal_ctor);

    JSValue et_ctor = JS_GetPropertyStr(ctx, global, "EventTarget");
    if (JS_IsException(et_ctor)) {
        clear_pending_exception(ctx);
    } else if (JS_IsObject(et_ctor)) {
        JSValue et_proto = JS_GetPropertyStr(ctx, et_ctor, "prototype");
        if (!JS_IsException(et_proto) && JS_IsObject(et_proto)) {
            JSValue add_fn = JS_UNDEFINED;
            JSValue remove_fn = JS_UNDEFINED;
            bool acc1 = false;
            bool acc2 = false;
            if (read_own_data_property(ctx, et_proto, "addEventListener", &add_fn, &acc1) &&
                read_own_data_property(ctx, et_proto, "removeEventListener", &remove_fn, &acc2) &&
                JS_IsFunction(ctx, add_fn) && JS_IsFunction(ctx, remove_fn)) {
                out->event_target_add_listener = add_fn;
                out->event_target_remove_listener = remove_fn;
            } else {
                JS_FreeValue(ctx, add_fn);
                JS_FreeValue(ctx, remove_fn);
            }
        } else if (JS_IsException(et_proto)) {
            clear_pending_exception(ctx);
        }
        JS_FreeValue(ctx, et_proto);
    }
    JS_FreeValue(ctx, et_ctor);

    JSValue symbol_ctor = JS_GetPropertyStr(ctx, global, "Symbol");
    if (JS_IsException(symbol_ctor)) {
        clear_pending_exception(ctx);
    } else if (JS_IsObject(symbol_ctor)) {
        JSValue s_disp = JS_GetPropertyStr(ctx, symbol_ctor, "dispose");
        if (JS_IsException(s_disp)) {
            clear_pending_exception(ctx);
        } else if (JS_IsSymbol(s_disp)) {
            out->symbol_dispose = s_disp;
        } else {
            JS_FreeValue(ctx, s_disp);
        }
        JSValue s_adisp = JS_GetPropertyStr(ctx, symbol_ctor, "asyncDispose");
        if (JS_IsException(s_adisp)) {
            clear_pending_exception(ctx);
        } else if (JS_IsSymbol(s_adisp)) {
            out->symbol_async_dispose = s_adisp;
        } else {
            JS_FreeValue(ctx, s_adisp);
        }
    }
    JS_FreeValue(ctx, symbol_ctor);

    JS_FreeValue(ctx, global);
    return true;
}

void free_binding_rpc_intrinsics(JSContext* ctx, BindingRpcIntrinsics* intrinsics) {
    if (ctx == NULL || intrinsics == NULL) {
        return;
    }
    JSValue* fields[] = {
        &intrinsics->object_proto,
        &intrinsics->error_ctor,
        &intrinsics->error_proto,
        &intrinsics->type_error_ctor,
        &intrinsics->type_error_proto,
        &intrinsics->range_error_ctor,
        &intrinsics->range_error_proto,
        &intrinsics->syntax_error_ctor,
        &intrinsics->syntax_error_proto,
        &intrinsics->uri_error_ctor,
        &intrinsics->uri_error_proto,
        &intrinsics->reference_error_ctor,
        &intrinsics->reference_error_proto,
        &intrinsics->eval_error_ctor,
        &intrinsics->eval_error_proto,
        &intrinsics->dom_exception_ctor,
        &intrinsics->dom_exception_name_getter,
        &intrinsics->dom_exception_message_getter,
        &intrinsics->quota_exceeded_ctor,
        &intrinsics->quota_exceeded_capture,
        &intrinsics->abort_signal_aborted_getter,
        &intrinsics->abort_signal_reason_getter,
        &intrinsics->event_target_add_listener,
        &intrinsics->event_target_remove_listener,
        &intrinsics->symbol_dispose,
        &intrinsics->symbol_async_dispose,
    };
    for (std::size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        JS_FreeValue(ctx, *fields[i]);
        *fields[i] = JS_UNDEFINED;
    }
}

void capture_remote_error(JSContext* ctx, JSValueConst value,
                          const BindingRpcIntrinsics* intrinsics, const char* fallback,
                          RemoteErrorInfo* out) {
    if (out == NULL) {
        return;
    }
    const std::string fallback_msg =
        (fallback != NULL && fallback[0] != '\0') ? fallback : "binding call failed";
    *out = RemoteErrorInfo::make_error(fallback_msg);
    if (ctx == NULL || JS_IsException(value) || JS_IsUndefined(value) || JS_IsNull(value)) {
        return;
    }
    if (JS_IsString(value)) {
        std::string text;
        if (read_js_string_bounded(ctx, value, kMaxRemoteErrorMessageBytes, &text)) {
            out->message = text;
        }
        return;
    }
    if (JS_IsBool(value) || JS_IsNumber(value) || JS_IsBigInt(value)) {
        size_t len = 0;
        const char* text = JS_ToCStringLen(ctx, &len, value);
        if (text != NULL) {
            out->message = clamp_utf8_text(text, len, kMaxRemoteErrorMessageBytes);
            JS_FreeCString(ctx, text);
        } else {
            clear_pending_exception(ctx);
        }
        return;
    }
    // Never call toString/getters/traps on non-objects or Proxies.
    if (!JS_IsObject(value) || JS_IsProxy(value)) {
        return;
    }

    // 1. Brand-checked QuotaExceededError (uses private #quota field).
    if (intrinsics != NULL && JS_IsFunction(ctx, intrinsics->quota_exceeded_capture)) {
        JSValue arg = JS_DupValue(ctx, value);
        JSValue record = JS_Call(ctx, intrinsics->quota_exceeded_capture,
                                 intrinsics->quota_exceeded_ctor, 1, &arg);
        JS_FreeValue(ctx, arg);
        if (JS_IsException(record)) {
            clear_pending_exception(ctx);
        } else {
            if (JS_IsObject(record)) {
                out->kind = RemoteErrorKind::kQuotaExceededError;
                out->name = "QuotaExceededError";
                out->message.clear();
                JSValue msg_val = JS_UNDEFINED;
                bool acc = false;
                if (read_own_data_property(ctx, record, "message", &msg_val, &acc)) {
                    read_js_string_bounded(ctx, msg_val, kMaxRemoteErrorMessageBytes,
                                           &out->message);
                    JS_FreeValue(ctx, msg_val);
                }
                JSValue quota_val = JS_UNDEFINED;
                if (read_own_data_property(ctx, record, "quota", &quota_val, &acc)) {
                    double d = 0;
                    if (JS_IsNumber(quota_val) && JS_ToFloat64(ctx, &d, quota_val) == 0 &&
                        std::isfinite(d) && d >= 0.0) {
                        out->has_quota = true;
                        out->quota = d;
                    }
                    JS_FreeValue(ctx, quota_val);
                }
                JSValue req_val = JS_UNDEFINED;
                if (read_own_data_property(ctx, record, "requested", &req_val, &acc)) {
                    double d = 0;
                    if (JS_IsNumber(req_val) && JS_ToFloat64(ctx, &d, req_val) == 0 &&
                        std::isfinite(d) && d >= 0.0) {
                        out->has_requested = true;
                        out->requested = d;
                    }
                    JS_FreeValue(ctx, req_val);
                }
                JS_FreeValue(ctx, record);
                return;
            }
            JS_FreeValue(ctx, record);
        }
    }

    // 2. Brand-checked DOMException (native QuickJS class brand check).
    if (intrinsics != NULL && JS_IsFunction(ctx, intrinsics->dom_exception_name_getter) &&
        JS_IsFunction(ctx, intrinsics->dom_exception_message_getter)) {
        JSValue name_val = JS_Call(ctx, intrinsics->dom_exception_name_getter, value, 0, NULL);
        if (JS_IsException(name_val)) {
            clear_pending_exception(ctx);
        } else {
            std::string dom_name;
            const bool got_name =
                read_js_string_bounded(ctx, name_val, kMaxRemoteErrorNameBytes, &dom_name);
            JS_FreeValue(ctx, name_val);
            if (got_name) {
                std::string dom_message;
                JSValue msg_val =
                    JS_Call(ctx, intrinsics->dom_exception_message_getter, value, 0, NULL);
                if (JS_IsException(msg_val)) {
                    clear_pending_exception(ctx);
                } else {
                    read_js_string_bounded(ctx, msg_val, kMaxRemoteErrorMessageBytes, &dom_message);
                    JS_FreeValue(ctx, msg_val);
                }
                out->kind = RemoteErrorKind::kDOMException;
                out->name = dom_name.empty() ? "Error" : dom_name;
                out->message = dom_message;
                return;
            }
        }
    }

    // 3. Genuine native Error / subclass.
    if (JS_IsError(value)) {
        out->kind = classify_error_prototype(ctx, value, intrinsics);
        out->name = default_error_name(out->kind);
        JSValue msg_val = JS_UNDEFINED;
        bool was_accessor = false;
        if (read_own_data_property(ctx, value, "message", &msg_val, &was_accessor)) {
            std::string msg_text;
            if (read_js_string_bounded(ctx, msg_val, kMaxRemoteErrorMessageBytes, &msg_text)) {
                out->message = msg_text;
            } else {
                out->message = fallback_msg;
            }
            JS_FreeValue(ctx, msg_val);
        } else if (!was_accessor) {
            // Standard `new Error()` / `new TypeError()` has no own message
            // property and inherits empty string `""` from Error.prototype.
            out->message.clear();
        } else {
            // Hostile getter on `message` was not executed; use safe fallback.
            out->message = fallback_msg;
        }
        return;
    }

    // 4. Plain object or non-Error class instance: keep safe fallback without
    // running attacker-controlled toString() or getters.
}

JSValue materialize_remote_error(JSContext* ctx, const RemoteErrorInfo& info,
                                 const BindingRpcIntrinsics* intrinsics) {
    if (ctx == NULL) {
        return JS_EXCEPTION;
    }
    const std::string clamped_msg =
        clamp_utf8_text(info.message.data(), info.message.size(), kMaxRemoteErrorMessageBytes);
    JSValue msg_val = JS_NewStringLen(ctx, clamped_msg.data(), clamped_msg.size());
    if (JS_IsException(msg_val)) {
        return msg_val;
    }

    if (info.kind == RemoteErrorKind::kQuotaExceededError && intrinsics != NULL &&
        JS_IsConstructor(ctx, intrinsics->quota_exceeded_ctor)) {
        JSValue opts = JS_NewObjectProto(ctx, JS_NULL);
        if (JS_IsException(opts)) {
            JS_FreeValue(ctx, msg_val);
            return opts;
        }
        bool ok = true;
        if (info.has_quota) {
            ok = ok && JS_SetPropertyStr(ctx, opts, "quota", JS_NewFloat64(ctx, info.quota)) >= 0;
        }
        if (info.has_requested) {
            ok = ok &&
                 JS_SetPropertyStr(ctx, opts, "requested", JS_NewFloat64(ctx, info.requested)) >= 0;
        }
        JSValue err = JS_EXCEPTION;
        if (ok) {
            JSValue args[2] = {msg_val, opts};
            err = JS_CallConstructor(ctx, intrinsics->quota_exceeded_ctor, 2, args);
        }
        JS_FreeValue(ctx, opts);
        JS_FreeValue(ctx, msg_val);
        if (!JS_IsException(err)) {
            return err;
        }
        clear_pending_exception(ctx);
        // Fall through to DOMException("QuotaExceededError") if options failed.
        msg_val = JS_NewStringLen(ctx, clamped_msg.data(), clamped_msg.size());
        if (JS_IsException(msg_val)) {
            return msg_val;
        }
    }

    if ((info.kind == RemoteErrorKind::kDOMException ||
         info.kind == RemoteErrorKind::kQuotaExceededError) &&
        intrinsics != NULL && JS_IsConstructor(ctx, intrinsics->dom_exception_ctor)) {
        const std::string dom_name =
            info.name.empty()
                ? std::string("Error")
                : clamp_utf8_text(info.name.data(), info.name.size(), kMaxRemoteErrorNameBytes);
        JSValue name_val = JS_NewStringLen(ctx, dom_name.data(), dom_name.size());
        if (JS_IsException(name_val)) {
            JS_FreeValue(ctx, msg_val);
            return name_val;
        }
        JSValue args[2] = {msg_val, name_val};
        JSValue err = JS_CallConstructor(ctx, intrinsics->dom_exception_ctor, 2, args);
        JS_FreeValue(ctx, name_val);
        JS_FreeValue(ctx, msg_val);
        if (!JS_IsException(err)) {
            return err;
        }
        clear_pending_exception(ctx);
        msg_val = JS_NewStringLen(ctx, clamped_msg.data(), clamped_msg.size());
        if (JS_IsException(msg_val)) {
            return msg_val;
        }
    }

    JSValueConst ctor = JS_UNDEFINED;
    if (intrinsics != NULL) {
        switch (info.kind) {
        case RemoteErrorKind::kTypeError:
            ctor = intrinsics->type_error_ctor;
            break;
        case RemoteErrorKind::kRangeError:
            ctor = intrinsics->range_error_ctor;
            break;
        case RemoteErrorKind::kSyntaxError:
            ctor = intrinsics->syntax_error_ctor;
            break;
        case RemoteErrorKind::kURIError:
            ctor = intrinsics->uri_error_ctor;
            break;
        case RemoteErrorKind::kReferenceError:
            ctor = intrinsics->reference_error_ctor;
            break;
        case RemoteErrorKind::kEvalError:
            ctor = intrinsics->eval_error_ctor;
            break;
        case RemoteErrorKind::kError:
        case RemoteErrorKind::kDOMException:
        case RemoteErrorKind::kQuotaExceededError:
            ctor = intrinsics->error_ctor;
            break;
        }
    }
    if (JS_IsConstructor(ctx, ctor)) {
        JSValue err = JS_CallConstructor(ctx, ctor, 1, &msg_val);
        JS_FreeValue(ctx, msg_val);
        if (!JS_IsException(err)) {
            return err;
        }
        clear_pending_exception(ctx);
        msg_val = JS_NewStringLen(ctx, clamped_msg.data(), clamped_msg.size());
        if (JS_IsException(msg_val)) {
            return msg_val;
        }
    }

    JSValue err = JS_NewError(ctx);
    if (JS_IsException(err)) {
        JS_FreeValue(ctx, msg_val);
        return err;
    }
    JS_DefinePropertyValueStr(ctx, err, "message", msg_val, JS_PROP_C_W_E);
    return err;
}

void capture_abort_reason(JSContext* ctx, JSValueConst value,
                          const BindingRpcIntrinsics* intrinsics, BindingAbortReason* out) {
    if (out == NULL) {
        return;
    }
    out->kind = BindingAbortReason::Kind::kDefaultAbort;
    out->error = RemoteErrorInfo();
    out->value = NeutralValue();
    if (ctx == NULL || JS_IsUndefined(value) || JS_IsException(value)) {
        return;
    }
    // Check structured errors first (DOMException / QuotaExceededError / Error).
    if (JS_IsObject(value) && !JS_IsProxy(value)) {
        if (intrinsics != NULL && JS_IsFunction(ctx, intrinsics->quota_exceeded_capture)) {
            JSValue arg = JS_DupValue(ctx, value);
            JSValue rec = JS_Call(ctx, intrinsics->quota_exceeded_capture,
                                  intrinsics->quota_exceeded_ctor, 1, &arg);
            JS_FreeValue(ctx, arg);
            if (JS_IsException(rec)) {
                clear_pending_exception(ctx);
            } else {
                const bool is_quota = JS_IsObject(rec);
                JS_FreeValue(ctx, rec);
                if (is_quota) {
                    capture_remote_error(ctx, value, intrinsics, "The operation was aborted",
                                         &out->error);
                    out->kind = BindingAbortReason::Kind::kRemoteError;
                    return;
                }
            }
        }
        if (intrinsics != NULL && JS_IsFunction(ctx, intrinsics->dom_exception_name_getter)) {
            JSValue dom_name = JS_Call(ctx, intrinsics->dom_exception_name_getter, value, 0, NULL);
            if (JS_IsException(dom_name)) {
                clear_pending_exception(ctx);
            } else {
                const bool is_dom = JS_IsString(dom_name);
                JS_FreeValue(ctx, dom_name);
                if (is_dom) {
                    capture_remote_error(ctx, value, intrinsics, "The operation was aborted",
                                         &out->error);
                    out->kind = BindingAbortReason::Kind::kRemoteError;
                    return;
                }
            }
        }
        if (JS_IsError(value)) {
            capture_remote_error(ctx, value, intrinsics, "The operation was aborted", &out->error);
            out->kind = BindingAbortReason::Kind::kRemoteError;
            return;
        }
    }
    // Otherwise attempt plain neutral structured clone (e.g. string/object reason).
    NeutralValue cloned;
    std::string clone_err;
    if (neutral_from_js(ctx, value, &cloned, &clone_err)) {
        out->kind = BindingAbortReason::Kind::kNeutralValue;
        out->value = std::move(cloned);
        return;
    }
    // Uncloneable objects (Proxy, Function, Symbol, etc.) fall back safely.
    out->kind = BindingAbortReason::Kind::kDefaultAbort;
}

JSValue materialize_abort_reason(JSContext* ctx, const BindingAbortReason& reason,
                                 const BindingRpcIntrinsics* intrinsics) {
    switch (reason.kind) {
    case BindingAbortReason::Kind::kDefaultAbort:
        return materialize_remote_error(
            ctx, RemoteErrorInfo::make_dom_exception("AbortError", "The operation was aborted"),
            intrinsics);
    case BindingAbortReason::Kind::kRemoteError:
        return materialize_remote_error(ctx, reason.error, intrinsics);
    case BindingAbortReason::Kind::kNeutralValue:
        return neutral_to_js(ctx, reason.value);
    }
    return materialize_remote_error(
        ctx, RemoteErrorInfo::make_dom_exception("AbortError", "The operation was aborted"),
        intrinsics);
}

bool binding_signal_is_aborted(JSContext* ctx, JSValueConst signal,
                               const BindingRpcIntrinsics* intrinsics, bool* out_aborted) {
    if (out_aborted != NULL) {
        *out_aborted = false;
    }
    if (ctx == NULL || intrinsics == NULL || !JS_IsObject(signal) || JS_IsProxy(signal) ||
        !JS_IsFunction(ctx, intrinsics->abort_signal_aborted_getter)) {
        return false;
    }
    JSValue res = JS_Call(ctx, intrinsics->abort_signal_aborted_getter, signal, 0, NULL);
    if (JS_IsException(res)) {
        clear_pending_exception(ctx);
        return false;
    }
    const bool aborted = JS_ToBool(ctx, res) != 0;
    JS_FreeValue(ctx, res);
    if (out_aborted != NULL) {
        *out_aborted = aborted;
    }
    return true;
}

bool binding_signal_read_reason(JSContext* ctx, JSValueConst signal,
                                const BindingRpcIntrinsics* intrinsics, JSValue* out_reason) {
    if (out_reason == NULL) {
        return false;
    }
    *out_reason = JS_UNDEFINED;
    if (ctx == NULL || intrinsics == NULL || !JS_IsObject(signal) || JS_IsProxy(signal) ||
        !JS_IsFunction(ctx, intrinsics->abort_signal_reason_getter)) {
        return false;
    }
    JSValue res = JS_Call(ctx, intrinsics->abort_signal_reason_getter, signal, 0, NULL);
    if (JS_IsException(res)) {
        clear_pending_exception(ctx);
        return false;
    }
    *out_reason = res;
    return true;
}

bool binding_signal_add_abort_listener(JSContext* ctx, JSValueConst signal, JSValueConst listener,
                                       const BindingRpcIntrinsics* intrinsics) {
    if (ctx == NULL || intrinsics == NULL ||
        !JS_IsFunction(ctx, intrinsics->event_target_add_listener)) {
        return false;
    }
    JSValue type = JS_NewString(ctx, "abort");
    JSValue opts = JS_NewObjectProto(ctx, JS_NULL);
    if (JS_IsException(type) || JS_IsException(opts) ||
        JS_SetPropertyStr(ctx, opts, "once", JS_TRUE) < 0) {
        clear_pending_exception(ctx);
        JS_FreeValue(ctx, type);
        JS_FreeValue(ctx, opts);
        return false;
    }
    JSValue args[3] = {type, JS_DupValue(ctx, listener), opts};
    JSValue res = JS_Call(ctx, intrinsics->event_target_add_listener, signal, 3, args);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, args[2]);
    if (JS_IsException(res)) {
        clear_pending_exception(ctx);
        return false;
    }
    JS_FreeValue(ctx, res);
    return true;
}

void binding_signal_remove_abort_listener(JSContext* ctx, JSValueConst signal,
                                          JSValueConst listener,
                                          const BindingRpcIntrinsics* intrinsics) {
    if (ctx == NULL || intrinsics == NULL || !JS_IsObject(signal) ||
        !JS_IsFunction(ctx, intrinsics->event_target_remove_listener)) {
        return;
    }
    JSValue type = JS_NewString(ctx, "abort");
    if (JS_IsException(type)) {
        clear_pending_exception(ctx);
        return;
    }
    JSValue args[2] = {type, JS_DupValue(ctx, listener)};
    JSValue res = JS_Call(ctx, intrinsics->event_target_remove_listener, signal, 2, args);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    if (JS_IsException(res)) {
        clear_pending_exception(ctx);
    } else {
        JS_FreeValue(ctx, res);
    }
}

void free_binding_call_options(JSContext* ctx, BindingCallOptions* options) {
    if (options == NULL) {
        return;
    }
    if (ctx != NULL && !JS_IsUndefined(options->signal)) {
        JS_FreeValue(ctx, options->signal);
    }
    options->signal = JS_UNDEFINED;
    options->has_signal = false;
    options->has_timeout_ms = false;
    options->timeout_ms = 0;
}

bool parse_binding_call_options(JSContext* ctx, JSValueConst value,
                                const BindingRpcIntrinsics* intrinsics, BindingCallOptions* out,
                                std::string* error) {
    if (ctx == NULL || out == NULL) {
        return fail_closed(error, "binding call options arguments are invalid");
    }
    free_binding_call_options(ctx, out);
    if (error != NULL) {
        error->clear();
    }
    if (JS_IsUndefined(value) || JS_IsNull(value)) {
        return true;
    }
    if (!JS_IsObject(value) || JS_IsProxy(value) || JS_IsArray(value) ||
        JS_IsFunction(ctx, value)) {
        return fail_closed(error, "binding call options must be a plain object");
    }
    JSValue object_proto = JS_UNDEFINED;
    bool owned_proto = false;
    if (intrinsics != NULL && JS_IsObject(intrinsics->object_proto)) {
        object_proto = intrinsics->object_proto;
    } else {
        JSValue plain = JS_Eval(ctx, "({})", 4, "<options-probe>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(plain)) {
            clear_pending_exception(ctx);
            return fail_closed(error, "binding call options prototype probe failed");
        }
        object_proto = JS_GetPrototype(ctx, plain);
        JS_FreeValue(ctx, plain);
        if (JS_IsException(object_proto)) {
            clear_pending_exception(ctx);
            return fail_closed(error, "binding call options prototype probe failed");
        }
        owned_proto = true;
    }
    JSValue proto = JS_GetPrototype(ctx, value);
    const bool is_plain =
        !JS_IsException(proto) &&
        (JS_IsNull(proto) || JS_VALUE_GET_PTR(proto) == JS_VALUE_GET_PTR(object_proto));
    if (JS_IsException(proto)) {
        clear_pending_exception(ctx);
    } else {
        JS_FreeValue(ctx, proto);
    }
    if (owned_proto) {
        JS_FreeValue(ctx, object_proto);
    }
    if (!is_plain) {
        return fail_closed(error, "binding call options must be a plain object");
    }
    if (!reject_enumerable_symbol_keys(ctx, value, error)) {
        return false;
    }

    JSPropertyEnum* tab = NULL;
    uint32_t count = 0;
    if (JS_GetOwnPropertyNames(ctx, &tab, &count, value, JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK) <
        0) {
        clear_pending_exception(ctx);
        return fail_closed(error, "binding call options are unreadable");
    }
    BindingCallOptions parsed;
    for (uint32_t index = 0; index < count; ++index) {
        size_t key_len = 0;
        const char* key = JS_AtomToCStringLen(ctx, &key_len, tab[index].atom);
        if (key == NULL) {
            clear_pending_exception(ctx);
            free_binding_call_options(ctx, &parsed);
            JS_FreePropertyEnum(ctx, tab, count);
            return fail_closed(error, "binding call option key is unreadable");
        }
        const std::string key_str(key, key_len);
        JS_FreeCString(ctx, key);

        JSPropertyDescriptor desc;
        if (JS_GetOwnProperty(ctx, &desc, value, tab[index].atom) < 0) {
            clear_pending_exception(ctx);
            free_binding_call_options(ctx, &parsed);
            JS_FreePropertyEnum(ctx, tab, count);
            return fail_closed(error, "binding call option property is unreadable");
        }
        const bool is_accessor = !JS_IsUndefined(desc.getter) || !JS_IsUndefined(desc.setter);
        JSValue prop = desc.value;
        JS_FreeValue(ctx, desc.getter);
        JS_FreeValue(ctx, desc.setter);
        if (is_accessor) {
            JS_FreeValue(ctx, prop);
            free_binding_call_options(ctx, &parsed);
            JS_FreePropertyEnum(ctx, tab, count);
            return fail_closed(error, "binding call options do not allow accessor properties");
        }

        if (key_str == "timeoutMs") {
            if (!JS_IsUndefined(prop)) {
                double ms = 0;
                if (!JS_IsNumber(prop) || JS_ToFloat64(ctx, &ms, prop) != 0 || !std::isfinite(ms) ||
                    ms < 0.0 || ms > kMaxBindingTimeoutMs) {
                    JS_FreeValue(ctx, prop);
                    free_binding_call_options(ctx, &parsed);
                    JS_FreePropertyEnum(ctx, tab, count);
                    return fail_closed(
                        error,
                        "binding call option 'timeoutMs' must be a non-negative finite number");
                }
                parsed.has_timeout_ms = true;
                parsed.timeout_ms = static_cast<std::uint64_t>(ms);
            }
            JS_FreeValue(ctx, prop);
        } else if (key_str == "signal") {
            if (!JS_IsUndefined(prop)) {
                bool aborted = false;
                if (!binding_signal_is_aborted(ctx, prop, intrinsics, &aborted)) {
                    JS_FreeValue(ctx, prop);
                    free_binding_call_options(ctx, &parsed);
                    JS_FreePropertyEnum(ctx, tab, count);
                    return fail_closed(error,
                                       "binding call option 'signal' must be an AbortSignal");
                }
                if (!JS_IsUndefined(parsed.signal)) {
                    JS_FreeValue(ctx, parsed.signal);
                }
                parsed.has_signal = true;
                // Transfer the descriptor's strong reference directly into
                // `parsed.signal` so subsequent JS execution cannot collect it.
                parsed.signal = prop;
            } else {
                JS_FreeValue(ctx, prop);
            }
        } else {
            JS_FreeValue(ctx, prop);
            free_binding_call_options(ctx, &parsed);
            JS_FreePropertyEnum(ctx, tab, count);
            if (error != NULL) {
                *error = "unknown binding call option: " + key_str;
            }
            return false;
        }
    }
    JS_FreePropertyEnum(ctx, tab, count);
    *out = parsed;
    return true;
}

JSValue neutral_to_js_ex(JSContext* ctx, const NeutralValue& value,
                         const BindingCloneHooks* hooks) {
    switch (value.kind) {
    case NeutralValue::Kind::kUndefined:
        return JS_UNDEFINED;
    case NeutralValue::Kind::kNull:
        return JS_NULL;
    case NeutralValue::Kind::kBool:
        return value.bool_value ? JS_TRUE : JS_FALSE;
    case NeutralValue::Kind::kNumber: {
        return JS_NewFloat64(ctx, value.number);
    }
    case NeutralValue::Kind::kBigInt: {
        std::size_t offset = 0;
        if (!value.bigint.empty() && (value.bigint[0] == '-' || value.bigint[0] == '+')) {
            offset = 1;
        }
        if (offset == value.bigint.size()) {
            return JS_ThrowSyntaxError(ctx, "invalid neutral bigint");
        }
        for (; offset < value.bigint.size(); ++offset) {
            if (!std::isdigit(static_cast<unsigned char>(value.bigint[offset]))) {
                return JS_ThrowSyntaxError(ctx, "invalid neutral bigint");
            }
        }
        const std::string literal = "(" + value.bigint + "n)";
        return JS_Eval(ctx, literal.data(), literal.size(), "<binding-bigint>",
                       JS_EVAL_TYPE_GLOBAL);
    }
    case NeutralValue::Kind::kString:
        return JS_NewStringLen(ctx, value.string.data(), value.string.size());
    case NeutralValue::Kind::kDate:
        return JS_NewDate(ctx, value.date_ms);
    case NeutralValue::Kind::kBytes: {
        JSValue array = JS_NewUint8ArrayCopy(ctx, value.bytes.data(), value.bytes.size());
        return array;
    }
    case NeutralValue::Kind::kArray: {
        JSValue array = JS_NewArray(ctx);
        if (JS_IsException(array)) {
            return array;
        }
        for (std::size_t index = 0; index < value.array.size(); ++index) {
            JSValue child = neutral_to_js_ex(ctx, value.array[index], hooks);
            if (JS_IsException(child)) {
                JS_FreeValue(ctx, array);
                return child;
            }
            if (JS_SetPropertyUint32(ctx, array, static_cast<uint32_t>(index), child) < 0) {
                JS_FreeValue(ctx, array);
                return JS_EXCEPTION;
            }
        }
        return array;
    }
    case NeutralValue::Kind::kObject: {
        JSValue object = JS_NewObjectProto(ctx, JS_NULL);
        if (JS_IsException(object)) {
            return object;
        }
        for (const auto& entry : value.object) {
            JSValue child = neutral_to_js_ex(ctx, entry.second, hooks);
            if (JS_IsException(child)) {
                JS_FreeValue(ctx, object);
                return child;
            }
            JSAtom key_atom = JS_NewAtomLen(ctx, entry.first.data(), entry.first.size());
            if (key_atom == JS_ATOM_NULL) {
                JS_FreeValue(ctx, child);
                JS_FreeValue(ctx, object);
                return JS_EXCEPTION;
            }
            const int def_rc = JS_DefinePropertyValue(ctx, object, key_atom, child, JS_PROP_C_W_E);
            JS_FreeAtom(ctx, key_atom);
            if (def_rc < 0) {
                JS_FreeValue(ctx, object);
                return JS_EXCEPTION;
            }
        }
        return object;
    }
    case NeutralValue::Kind::kReadableStreamProxy:
    case NeutralValue::Kind::kWritableStreamProxy:
    case NeutralValue::Kind::kRequestProxy:
    case NeutralValue::Kind::kResponseProxy:
    case NeutralValue::Kind::kRpcHandleProxy: {
        if (hooks != NULL && hooks->materialize != NULL) {
            return hooks->materialize(ctx, value, hooks->opaque);
        }
        return JS_ThrowInternalError(ctx, "binding proxy materializer unavailable");
    }
    }
    return JS_ThrowInternalError(ctx, "invalid neutral value kind");
}

JSValue neutral_to_js(JSContext* ctx, const NeutralValue& value) {
    return neutral_to_js_ex(ctx, value, NULL);
}

bool neutral_from_js_ex(JSContext* ctx, JSValueConst value, NeutralValue* out, std::string* error,
                        const BindingCloneHooks* hooks) {
    if (ctx == NULL || out == NULL) {
        return fail_closed(error, "clone arguments are invalid");
    }
    if (error != NULL) {
        error->clear();
    }
    CloneContext context;
    context.ctx = ctx;
    context.depth = 0;
    context.nodes = 0;
    context.bytes = 0;
    context.hooks = hooks;
    if (!learn_class_ids(ctx, &context)) {
        return fail_closed(error, "clone class probe failed");
    }
    NeutralValue result;
    const bool ok = clone_value(ctx, value, &result, error, &context);
    JS_FreeValue(ctx, context.object_proto);
    if (ok) {
        *out = std::move(result);
    }
    return ok;
}

bool neutral_from_js(JSContext* ctx, JSValueConst value, NeutralValue* out, std::string* error) {
    return neutral_from_js_ex(ctx, value, out, error, NULL);
}

std::size_t estimate_neutral_request_bytes(const NeutralHttpRequest& request) {
    std::size_t total = 64;
    total = saturating_add(total, request.url.size());
    total = saturating_add(total, request.method.size());
    total = saturating_add(total, request.credentials.size());
    total = saturating_add(total, request.redirect.size());
    if (request.has_mode) {
        total = saturating_add(total, request.mode.size());
    }
    for (std::size_t i = 0; i < request.headers.size(); ++i) {
        total = saturating_add(total, request.headers[i].first.size());
        total = saturating_add(total, request.headers[i].second.size());
        total = saturating_add(total, 16);
    }
    total = saturating_add(total, request.inline_bytes.size());
    if (request.body_kind == 2) {
        total = saturating_add(total, 16);
    }
    return total;
}

std::size_t estimate_neutral_response_bytes(const NeutralHttpResponse& response) {
    std::size_t total = 64;
    total = saturating_add(total, response.status_text.size());
    total = saturating_add(total, response.url.size());
    total = saturating_add(total, response.type.size());
    for (std::size_t i = 0; i < response.headers.size(); ++i) {
        total = saturating_add(total, response.headers[i].first.size());
        total = saturating_add(total, response.headers[i].second.size());
        total = saturating_add(total, 16);
    }
    total = saturating_add(total, response.inline_bytes.size());
    if (response.body_kind == 2) {
        total = saturating_add(total, 16);
    }
    return total;
}

std::size_t estimate_neutral_value_bytes(const NeutralValue& value) {
    switch (value.kind) {
    case NeutralValue::Kind::kUndefined:
    case NeutralValue::Kind::kNull:
    case NeutralValue::Kind::kBool:
        return 1;
    case NeutralValue::Kind::kNumber:
    case NeutralValue::Kind::kDate:
        return 8;
    case NeutralValue::Kind::kBigInt:
        return saturating_add(value.bigint.size(), 8);
    case NeutralValue::Kind::kString:
        return saturating_add(value.string.size(), 1);
    case NeutralValue::Kind::kBytes:
        return value.bytes.size();
    case NeutralValue::Kind::kArray: {
        std::size_t total = 8;
        for (std::size_t i = 0; i < value.array.size(); ++i) {
            total = saturating_add(total, estimate_neutral_value_bytes(value.array[i]));
        }
        return total;
    }
    case NeutralValue::Kind::kObject: {
        std::size_t total = 8;
        for (std::size_t i = 0; i < value.object.size(); ++i) {
            total = saturating_add(total, value.object[i].first.size());
            total = saturating_add(total, estimate_neutral_value_bytes(value.object[i].second));
        }
        return total;
    }
    case NeutralValue::Kind::kReadableStreamProxy:
    case NeutralValue::Kind::kWritableStreamProxy:
        return 16;
    case NeutralValue::Kind::kRequestProxy:
        return estimate_neutral_request_bytes(value.request);
    case NeutralValue::Kind::kResponseProxy:
        return estimate_neutral_response_bytes(value.response);
    case NeutralValue::Kind::kRpcHandleProxy: {
        std::size_t total = 32;
        for (std::size_t i = 0; i < value.handle_methods.size(); ++i) {
            total = saturating_add(total, value.handle_methods[i].size());
            total = saturating_add(total, 16);
        }
        return total;
    }
    }
    return 1;
}

bool valid_binding_method_name(const char* name, std::size_t len) {
    static const char* const reserved[] = {"constructor", "prototype", "__proto__",
                                           "then",        "catch",     "finally"};
    if (name == NULL || len == 0 || len > 64) {
        return false;
    }
    for (std::size_t index = 0; index < sizeof(reserved) / sizeof(reserved[0]); ++index) {
        const std::size_t rlen = std::strlen(reserved[index]);
        if (len == rlen && std::memcmp(name, reserved[index], rlen) == 0) {
            return false;
        }
    }
    const auto ascii_alpha = [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
    };
    const unsigned char first = static_cast<unsigned char>(name[0]);
    if (!(ascii_alpha(first) || first == '_' || first == '$')) {
        return false;
    }
    for (std::size_t index = 1; index < len; ++index) {
        const unsigned char ch = static_cast<unsigned char>(name[index]);
        if (!(ascii_alpha(ch) || (ch >= '0' && ch <= '9') || ch == '_' || ch == '$')) {
            return false;
        }
    }
    return true;
}

bool valid_binding_method_name(const std::string& name) {
    return valid_binding_method_name(name.data(), name.size());
}

namespace {

struct BorrowedJsString {
    JSContext* ctx = NULL;
    JSValue val = JS_UNDEFINED;
    const char* ptr = NULL;
    std::size_t len = 0;
    bool present = false;

    explicit BorrowedJsString(JSContext* c) : ctx(c) {}
    ~BorrowedJsString() { reset(); }

    BorrowedJsString(const BorrowedJsString&) = delete;
    BorrowedJsString& operator=(const BorrowedJsString&) = delete;

    void reset() {
        if (ctx != NULL) {
            if (ptr != NULL) {
                JS_FreeCString(ctx, ptr);
                ptr = NULL;
            }
            if (!JS_IsUndefined(val)) {
                JS_FreeValue(ctx, val);
                val = JS_UNDEFINED;
            }
        }
        len = 0;
        present = false;
    }

    bool borrow_prop(JSValueConst obj, const char* name) {
        reset();
        JSValue prop = JS_GetPropertyStr(ctx, obj, name);
        if (JS_IsException(prop)) {
            clear_pending_exception(ctx);
            return false;
        }
        if (!JS_IsString(prop)) {
            JS_FreeValue(ctx, prop);
            return false;
        }
        size_t slen = 0;
        const char* s = JS_ToCStringLen(ctx, &slen, prop);
        if (s == NULL) {
            clear_pending_exception(ctx);
            JS_FreeValue(ctx, prop);
            return false;
        }
        val = prop;
        ptr = s;
        len = slen;
        present = true;
        return true;
    }

    // Distinguishes valid null/undefined absence from invalid types or conversion
    // errors for optional string metadata (such as Request.mode).
    bool borrow_optional_prop(JSValueConst obj, const char* name) {
        reset();
        JSValue prop = JS_GetPropertyStr(ctx, obj, name);
        if (JS_IsException(prop)) {
            clear_pending_exception(ctx);
            return false;
        }
        if (JS_IsNull(prop) || JS_IsUndefined(prop)) {
            JS_FreeValue(ctx, prop);
            return true;
        }
        if (!JS_IsString(prop)) {
            JS_FreeValue(ctx, prop);
            return false;
        }
        size_t slen = 0;
        const char* s = JS_ToCStringLen(ctx, &slen, prop);
        if (s == NULL) {
            clear_pending_exception(ctx);
            JS_FreeValue(ctx, prop);
            return false;
        }
        val = prop;
        ptr = s;
        len = slen;
        present = true;
        return true;
    }

    void copy_to(std::string* out, std::size_t* copy_count) const {
        if (present && out != NULL) {
            out->assign(ptr, len);
            if (copy_count != NULL) {
                ++(*copy_count);
            }
        }
    }
};

} // namespace

bool extract_neutral_http_request_meta(JSContext* ctx, JSValueConst info, NeutralHttpRequest* out,
                                       BindingReserveBytesFn reserve_bytes, void* reserve_opaque,
                                       std::string* error, std::size_t* out_copies) {
    if (out_copies != NULL) {
        *out_copies = 0;
    }
    if (ctx == NULL || out == NULL || !JS_IsObject(info)) {
        return fail_closed(error, "invalid Request metadata object");
    }
    BorrowedJsString url(ctx);
    BorrowedJsString method(ctx);
    BorrowedJsString credentials(ctx);
    BorrowedJsString redirect(ctx);
    BorrowedJsString mode(ctx);
    if (!url.borrow_prop(info, "url")) {
        return fail_closed(error, "Request metadata missing or invalid url");
    }
    if (!method.borrow_prop(info, "method")) {
        return fail_closed(error, "Request metadata missing or invalid method");
    }
    if (!credentials.borrow_prop(info, "credentials")) {
        return fail_closed(error, "Request metadata missing or invalid credentials");
    }
    if (!redirect.borrow_prop(info, "redirect")) {
        return fail_closed(error, "Request metadata missing or invalid redirect");
    }
    if (!mode.borrow_optional_prop(info, "mode")) {
        return fail_closed(error, "Request metadata has invalid mode");
    }

    std::size_t meta_bytes = 64;
    meta_bytes = saturating_add(meta_bytes, url.len);
    meta_bytes = saturating_add(meta_bytes, method.len);
    meta_bytes = saturating_add(meta_bytes, credentials.len);
    meta_bytes = saturating_add(meta_bytes, redirect.len);
    if (mode.present) {
        meta_bytes = saturating_add(meta_bytes, mode.len);
    }
    if (reserve_bytes != NULL && !reserve_bytes(meta_bytes, error, reserve_opaque)) {
        return false;
    }
    url.copy_to(&out->url, out_copies);
    method.copy_to(&out->method, out_copies);
    credentials.copy_to(&out->credentials, out_copies);
    redirect.copy_to(&out->redirect, out_copies);
    if (mode.present) {
        out->has_mode = true;
        mode.copy_to(&out->mode, out_copies);
    } else {
        out->has_mode = false;
        out->mode.clear();
    }
    return true;
}

bool extract_neutral_http_response_meta(JSContext* ctx, JSValueConst info, NeutralHttpResponse* out,
                                        BindingReserveBytesFn reserve_bytes, void* reserve_opaque,
                                        std::string* error, std::size_t* out_copies) {
    if (out_copies != NULL) {
        *out_copies = 0;
    }
    if (ctx == NULL || out == NULL || !JS_IsObject(info)) {
        return fail_closed(error, "invalid Response metadata object");
    }
    JSValue st_val = JS_GetPropertyStr(ctx, info, "status");
    if (JS_IsException(st_val)) {
        clear_pending_exception(ctx);
        return fail_closed(error, "Response metadata status is unreadable");
    }
    double st_f64 = 0.0;
    if (!JS_IsNumber(st_val) || JS_ToFloat64(ctx, &st_f64, st_val) != 0) {
        clear_pending_exception(ctx);
        JS_FreeValue(ctx, st_val);
        return fail_closed(error, "Response metadata status is invalid");
    }
    JS_FreeValue(ctx, st_val);
    if (!std::isfinite(st_f64) || std::floor(st_f64) != st_f64 ||
        (st_f64 != 0.0 && (st_f64 < 100.0 || st_f64 > 599.0))) {
        return fail_closed(error, "Response metadata status is out of range");
    }
    const std::uint16_t status_u16 = static_cast<std::uint16_t>(st_f64);

    JSValue redir_val = JS_GetPropertyStr(ctx, info, "redirected");
    if (JS_IsException(redir_val)) {
        clear_pending_exception(ctx);
        return fail_closed(error, "Response metadata redirected is unreadable");
    }
    if (!JS_IsBool(redir_val)) {
        JS_FreeValue(ctx, redir_val);
        return fail_closed(error, "Response metadata redirected is invalid");
    }
    const bool redirected = JS_ToBool(ctx, redir_val) != 0;
    JS_FreeValue(ctx, redir_val);

    BorrowedJsString status_text(ctx);
    BorrowedJsString url(ctx);
    BorrowedJsString type(ctx);
    if (!status_text.borrow_prop(info, "statusText")) {
        return fail_closed(error, "Response metadata missing or invalid statusText");
    }
    if (!url.borrow_prop(info, "url")) {
        return fail_closed(error, "Response metadata missing or invalid url");
    }
    if (!type.borrow_prop(info, "type")) {
        return fail_closed(error, "Response metadata missing or invalid type");
    }

    std::size_t meta_bytes = 64;
    meta_bytes = saturating_add(meta_bytes, status_text.len);
    meta_bytes = saturating_add(meta_bytes, url.len);
    meta_bytes = saturating_add(meta_bytes, type.len);
    if (reserve_bytes != NULL && !reserve_bytes(meta_bytes, error, reserve_opaque)) {
        return false;
    }
    out->status = status_u16;
    out->redirected = redirected;
    status_text.copy_to(&out->status_text, out_copies);
    url.copy_to(&out->url, out_copies);
    type.copy_to(&out->type, out_copies);
    return true;
}

bool extract_neutral_header_pairs(JSContext* ctx, JSValueConst arr,
                                  std::vector<std::pair<std::string, std::string>>* out,
                                  BindingReserveBytesFn reserve_bytes, void* reserve_opaque,
                                  std::string* error, std::size_t* out_copies) {
    if (out_copies != NULL) {
        *out_copies = 0;
    }
    if (ctx == NULL || out == NULL) {
        return fail_closed(error, "invalid proxy headers destination");
    }
    if (JS_IsNull(arr) || JS_IsUndefined(arr)) {
        out->clear();
        return true;
    }
    if (!JS_IsArray(arr)) {
        return fail_closed(error, "invalid proxy headers array");
    }
    JSValue len_val = JS_GetPropertyStr(ctx, arr, "length");
    uint32_t count = 0;
    if (JS_IsException(len_val) || JS_ToUint32(ctx, &count, len_val) != 0) {
        if (JS_IsException(len_val)) {
            clear_pending_exception(ctx);
        } else {
            clear_pending_exception(ctx);
            JS_FreeValue(ctx, len_val);
        }
        return fail_closed(error, "unreadable proxy headers length");
    }
    JS_FreeValue(ctx, len_val);
    if (count > kMaxBindingProxyHeaders) {
        return fail_closed(error, "proxy header count limit exceeded");
    }

    // U3/T1: Two-pass transactional extraction using bounded stack storage
    // (<= 256 pairs) so ANY validation error or whole-block quota rejection
    // performs zero target container string copies, leaves `*out` untouched,
    // and leaves ledger reservations unchanged.
    struct PendingHeader {
        JSValue k_val = JS_UNDEFINED;
        JSValue v_val = JS_UNDEFINED;
        const char* ks = NULL;
        const char* vs = NULL;
        std::size_t klen = 0;
        std::size_t vlen = 0;
    };
    PendingHeader pending[kMaxBindingProxyHeaders];
    std::size_t pending_count = 0;
    auto cleanup_pending = [&]() {
        for (std::size_t i = 0; i < pending_count; ++i) {
            if (pending[i].ks != NULL) {
                JS_FreeCString(ctx, pending[i].ks);
                pending[i].ks = NULL;
            }
            if (pending[i].vs != NULL) {
                JS_FreeCString(ctx, pending[i].vs);
                pending[i].vs = NULL;
            }
            if (!JS_IsUndefined(pending[i].k_val)) {
                JS_FreeValue(ctx, pending[i].k_val);
                pending[i].k_val = JS_UNDEFINED;
            }
            if (!JS_IsUndefined(pending[i].v_val)) {
                JS_FreeValue(ctx, pending[i].v_val);
                pending[i].v_val = JS_UNDEFINED;
            }
        }
        pending_count = 0;
    };

    std::size_t total_raw_bytes = 0;
    std::size_t total_reserve_bytes = 0;
    for (uint32_t i = 0; i < count; ++i) {
        JSValue entry = JS_GetPropertyUint32(ctx, arr, i);
        if (!JS_IsArray(entry)) {
            if (JS_IsException(entry)) {
                clear_pending_exception(ctx);
            } else {
                JS_FreeValue(ctx, entry);
            }
            cleanup_pending();
            return fail_closed(error, "invalid proxy header pair");
        }
        JSValue k = JS_GetPropertyUint32(ctx, entry, 0);
        JSValue v = JS_GetPropertyUint32(ctx, entry, 1);
        JS_FreeValue(ctx, entry);
        if (JS_IsException(k) || JS_IsException(v) || !JS_IsString(k) || !JS_IsString(v)) {
            clear_pending_exception(ctx);
            if (!JS_IsException(k)) {
                JS_FreeValue(ctx, k);
            }
            if (!JS_IsException(v)) {
                JS_FreeValue(ctx, v);
            }
            cleanup_pending();
            return fail_closed(error, "unreadable proxy header entry");
        }
        size_t klen = 0;
        size_t vlen = 0;
        const char* ks = JS_ToCStringLen(ctx, &klen, k);
        const char* vs = JS_ToCStringLen(ctx, &vlen, v);
        if (ks == NULL || vs == NULL) {
            clear_pending_exception(ctx);
            if (ks != NULL) {
                JS_FreeCString(ctx, ks);
            }
            if (vs != NULL) {
                JS_FreeCString(ctx, vs);
            }
            JS_FreeValue(ctx, k);
            JS_FreeValue(ctx, v);
            cleanup_pending();
            return fail_closed(error, "unreadable proxy header entry");
        }
        if (klen > kMaxBindingProxyHeaderBytes || vlen > kMaxBindingProxyHeaderBytes - klen ||
            total_raw_bytes > kMaxBindingProxyHeaderBytes - (klen + vlen)) {
            JS_FreeCString(ctx, ks);
            JS_FreeCString(ctx, vs);
            JS_FreeValue(ctx, k);
            JS_FreeValue(ctx, v);
            cleanup_pending();
            return fail_closed(error, "proxy header byte limit exceeded");
        }
        total_raw_bytes += klen + vlen;
        total_reserve_bytes =
            saturating_add(total_reserve_bytes, saturating_add(saturating_add(klen, vlen), 16));
        pending[pending_count].k_val = k;
        pending[pending_count].v_val = v;
        pending[pending_count].ks = ks;
        pending[pending_count].vs = vs;
        pending[pending_count].klen = klen;
        pending[pending_count].vlen = vlen;
        ++pending_count;
    }

    if (total_reserve_bytes > 0 && reserve_bytes != NULL &&
        !reserve_bytes(total_reserve_bytes, error, reserve_opaque)) {
        cleanup_pending();
        return false;
    }

    out->clear();
    out->reserve(pending_count);
    for (std::size_t i = 0; i < pending_count; ++i) {
        out->push_back(std::make_pair(std::string(pending[i].ks, pending[i].klen),
                                      std::string(pending[i].vs, pending[i].vlen)));
        if (out_copies != NULL) {
            *out_copies += 2;
        }
    }
    cleanup_pending();
    return true;
}

bool inspect_rpc_target_methods(JSContext* ctx, JSValueConst target, JSValueConst rpc_target_proto,
                                JSValueConst object_proto, std::size_t max_methods,
                                std::vector<std::string>* out_methods,
                                std::map<std::string, JSValue>* out_method_fns,
                                BindingReserveBytesFn reserve_bytes, void* reserve_opaque,
                                std::string* error, std::size_t* out_copies) {
    if (out_copies != NULL) {
        *out_copies = 0;
    }
    if (ctx == NULL || out_methods == NULL || out_method_fns == NULL) {
        return fail_closed(error, "invalid RpcTarget inspection arguments");
    }

    constexpr std::size_t kMaxStackMethods = 128;
    constexpr std::size_t kMaxInspectProperties = 1024;
    constexpr std::size_t kSeenTableMask = 2047; // 2048 open-addressed stack slots
    const std::size_t method_limit =
        (max_methods > 0 && max_methods < kMaxStackMethods) ? max_methods : kMaxStackMethods;
    struct PendingMethod {
        JSAtom atom = JS_ATOM_NULL;
        std::size_t name_len = 0;
        JSValue fn = JS_UNDEFINED;
    };
    PendingMethod pending[kMaxStackMethods];
    std::size_t pending_count = 0;
    auto cleanup_pending = [&]() {
        for (std::size_t i = 0; i < pending_count; ++i) {
            if (pending[i].atom != JS_ATOM_NULL) {
                JS_FreeAtom(ctx, pending[i].atom);
                pending[i].atom = JS_ATOM_NULL;
            }
            if (!JS_IsUndefined(pending[i].fn)) {
                JS_FreeValue(ctx, pending[i].fn);
                pending[i].fn = JS_UNDEFINED;
            }
        }
        pending_count = 0;
    };

    // U3: Bounded fixed-size stack atom set (zero heap allocation, capped at
    // 1024 unique properties scanned across the prototype chain).
    JSAtom seen_slots[kSeenTableMask + 1] = {};
    std::size_t seen_count = 0;
    auto insert_seen_atom = [&](JSAtom atom, bool* out_limit_exceeded) -> bool {
        *out_limit_exceeded = false;
        std::size_t slot = static_cast<std::size_t>(atom) & kSeenTableMask;
        for (std::size_t probe = 0; probe <= kSeenTableMask; ++probe) {
            const JSAtom existing = seen_slots[slot];
            if (existing == atom) {
                return false;
            }
            if (existing == JS_ATOM_NULL) {
                if (seen_count >= kMaxInspectProperties) {
                    *out_limit_exceeded = true;
                    return false;
                }
                seen_slots[slot] = atom;
                ++seen_count;
                return true;
            }
            slot = (slot + 1) & kSeenTableMask;
        }
        *out_limit_exceeded = true;
        return false;
    };

    JSValue cur = JS_DupValue(ctx, target);
    for (int depth = 0; depth < 16 && JS_IsObject(cur); ++depth) {
        if (JS_IsProxy(cur)) {
            JS_FreeValue(ctx, cur);
            cleanup_pending();
            return fail_closed(error, "RpcTarget prototype chain contains a Proxy");
        }
        if ((JS_IsObject(rpc_target_proto) &&
             JS_VALUE_GET_PTR(cur) == JS_VALUE_GET_PTR(rpc_target_proto)) ||
            (JS_IsObject(object_proto) &&
             JS_VALUE_GET_PTR(cur) == JS_VALUE_GET_PTR(object_proto))) {
            break;
        }
        JSPropertyEnum* tab = NULL;
        uint32_t count = 0;
        if (JS_GetOwnPropertyNames(ctx, &tab, &count, cur, JS_GPN_STRING_MASK | JS_GPN_SET_ENUM) <
            0) {
            clear_pending_exception(ctx);
            JS_FreeValue(ctx, cur);
            cleanup_pending();
            return fail_closed(error, "RpcTarget method enumeration failed");
        }
        if (count > kMaxInspectProperties) {
            JS_FreePropertyEnum(ctx, tab, count);
            JS_FreeValue(ctx, cur);
            cleanup_pending();
            return fail_closed(error, "RpcTarget exceeds property inspection limit");
        }
        for (uint32_t i = 0; i < count; ++i) {
            size_t name_len = 0;
            const char* raw_name = JS_AtomToCStringLen(ctx, &name_len, tab[i].atom);
            if (raw_name == NULL) {
                clear_pending_exception(ctx);
                JS_FreePropertyEnum(ctx, tab, count);
                JS_FreeValue(ctx, cur);
                cleanup_pending();
                return fail_closed(error, "RpcTarget property name is unreadable");
            }

            JSPropertyDescriptor desc;
            const int present = JS_GetOwnProperty(ctx, &desc, cur, tab[i].atom);
            if (present <= 0) {
                if (present < 0) {
                    clear_pending_exception(ctx);
                }
                JS_FreeCString(ctx, raw_name);
                JS_FreePropertyEnum(ctx, tab, count);
                JS_FreeValue(ctx, cur);
                cleanup_pending();
                return fail_closed(error, "RpcTarget property descriptor is unreadable");
            }
            const bool is_accessor = !JS_IsUndefined(desc.getter) || !JS_IsUndefined(desc.setter);
            const bool is_func = JS_IsFunction(ctx, desc.value);
            JS_FreeValue(ctx, desc.getter);
            JS_FreeValue(ctx, desc.setter);

            const bool is_constructor =
                (name_len == 11 && std::memcmp(raw_name, "constructor", 11) == 0);
            if (is_constructor) {
                JS_FreeValue(ctx, desc.value);
                JS_FreeCString(ctx, raw_name);
                continue;
            }
            if (is_accessor) {
                if (error != NULL) {
                    *error = "RpcTarget cannot expose accessor property: " +
                             clamp_utf8_text(raw_name, name_len, 64);
                }
                JS_FreeValue(ctx, desc.value);
                JS_FreeCString(ctx, raw_name);
                JS_FreePropertyEnum(ctx, tab, count);
                JS_FreeValue(ctx, cur);
                cleanup_pending();
                return false;
            }
            bool seen_limit = false;
            if (!insert_seen_atom(tab[i].atom, &seen_limit)) {
                JS_FreeValue(ctx, desc.value);
                JS_FreeCString(ctx, raw_name);
                if (seen_limit) {
                    JS_FreePropertyEnum(ctx, tab, count);
                    JS_FreeValue(ctx, cur);
                    cleanup_pending();
                    return fail_closed(error, "RpcTarget exceeds property inspection limit");
                }
                continue;
            }
            const bool is_dispose = (name_len == 7 && std::memcmp(raw_name, "dispose", 7) == 0);
            if (!is_func || is_dispose) {
                JS_FreeValue(ctx, desc.value);
                JS_FreeCString(ctx, raw_name);
                continue;
            }
            if (!valid_binding_method_name(raw_name, name_len)) {
                if (error != NULL) {
                    *error = "RpcTarget exposes invalid method name: " +
                             clamp_utf8_text(raw_name, name_len, 64);
                }
                JS_FreeValue(ctx, desc.value);
                JS_FreeCString(ctx, raw_name);
                JS_FreePropertyEnum(ctx, tab, count);
                JS_FreeValue(ctx, cur);
                cleanup_pending();
                return false;
            }
            JS_FreeCString(ctx, raw_name);
            if (pending_count >= method_limit) {
                JS_FreeValue(ctx, desc.value);
                JS_FreePropertyEnum(ctx, tab, count);
                JS_FreeValue(ctx, cur);
                cleanup_pending();
                return fail_closed(error, "RpcTarget exceeds 128 method limit");
            }
            // T1/F7/U3: Retain only the QuickJS atom and validated callable on
            // the stack during pass 1 — zero native string/map copies until
            // quota reservation succeeds below.
            pending[pending_count].atom = JS_DupAtom(ctx, tab[i].atom);
            pending[pending_count].name_len = name_len;
            pending[pending_count].fn = desc.value;
            ++pending_count;
        }
        JS_FreePropertyEnum(ctx, tab, count);
        JSValue next = JS_GetPrototype(ctx, cur);
        JS_FreeValue(ctx, cur);
        if (JS_IsException(next)) {
            clear_pending_exception(ctx);
            cleanup_pending();
            return fail_closed(error, "RpcTarget prototype walk failed");
        }
        cur = next;
    }
    JS_FreeValue(ctx, cur);
    if (pending_count == 0) {
        cleanup_pending();
        return fail_closed(error, "RpcTarget must expose at least 1 method");
    }

    std::size_t handle_bytes = 32;
    for (std::size_t i = 0; i < pending_count; ++i) {
        handle_bytes = saturating_add(handle_bytes, pending[i].name_len);
        handle_bytes = saturating_add(handle_bytes, 16);
    }
    if (reserve_bytes != NULL && !reserve_bytes(handle_bytes, error, reserve_opaque)) {
        cleanup_pending();
        return false;
    }

    out_methods->clear();
    for (std::map<std::string, JSValue>::iterator mit = out_method_fns->begin();
         mit != out_method_fns->end(); ++mit) {
        JS_FreeValue(ctx, mit->second);
    }
    out_method_fns->clear();

    for (std::size_t i = 0; i < pending_count; ++i) {
        size_t slen = 0;
        const char* s = JS_AtomToCStringLen(ctx, &slen, pending[i].atom);
        if (s == NULL) {
            clear_pending_exception(ctx);
            cleanup_pending();
            for (std::map<std::string, JSValue>::iterator mit = out_method_fns->begin();
                 mit != out_method_fns->end(); ++mit) {
                JS_FreeValue(ctx, mit->second);
            }
            out_method_fns->clear();
            out_methods->clear();
            return fail_closed(error, "RpcTarget method name copy failed");
        }
        std::string method_name(s, slen);
        JS_FreeCString(ctx, s);
        JS_FreeAtom(ctx, pending[i].atom);
        pending[i].atom = JS_ATOM_NULL;
        out_methods->push_back(method_name);
        out_method_fns->emplace(std::move(method_name), pending[i].fn);
        pending[i].fn = JS_UNDEFINED;
        if (out_copies != NULL) {
            ++(*out_copies);
        }
    }
    pending_count = 0;
    return true;
}

bool BindingHttpConsumeBatch::contains(JSValueConst value) const {
    if (!JS_IsObject(value)) {
        return false;
    }
    void* target_ptr = JS_VALUE_GET_PTR(value);
    for (std::size_t i = 0; i < targets.size(); ++i) {
        if (JS_VALUE_GET_PTR(targets[i]) == target_ptr) {
            return true;
        }
    }
    return false;
}

bool BindingHttpConsumeBatch::add_target(JSContext* target_ctx, JSValueConst value) {
    if (target_ctx == NULL || !JS_IsObject(value)) {
        return false;
    }
    if (ctx != NULL && ctx != target_ctx) {
        return false;
    }
    ctx = target_ctx;
    targets.push_back(JS_DupValue(target_ctx, value));
    return true;
}

bool BindingHttpConsumeBatch::prepare(JSValueConst prepare_fn, std::string* error) {
    if (targets.empty()) {
        return true;
    }
    if (ctx == NULL || !JS_IsFunction(ctx, prepare_fn)) {
        return fail_closed(error, "HTTP body consumption prepare helper unavailable");
    }
    if (!JS_IsUndefined(prepared_plan)) {
        JS_FreeValue(ctx, prepared_plan);
        prepared_plan = JS_UNDEFINED;
    }
    JSValue arr = JS_NewArray(ctx);
    if (JS_IsException(arr)) {
        clear_pending_exception(ctx);
        return fail_closed(error, "failed to allocate HTTP body commit batch");
    }
    for (std::size_t i = 0; i < targets.size(); ++i) {
        if (JS_DefinePropertyValueUint32(ctx, arr, static_cast<uint32_t>(i),
                                         JS_DupValue(ctx, targets[i]), JS_PROP_C_W_E) < 0) {
            clear_pending_exception(ctx);
            JS_FreeValue(ctx, arr);
            return fail_closed(error, "failed to populate HTTP body commit batch");
        }
    }
    JSValue plan = JS_Call(ctx, prepare_fn, JS_UNDEFINED, 1, &arr);
    JS_FreeValue(ctx, arr);
    if (JS_IsException(plan) || !JS_IsObject(plan)) {
        if (JS_IsException(plan)) {
            clear_pending_exception(ctx);
        } else {
            JS_FreeValue(ctx, plan);
        }
        return fail_closed(error, "failed to prepare HTTP body consumption");
    }
    prepared_plan = plan;
    return true;
}

bool BindingHttpConsumeBatch::commit(JSValueConst apply_fn, std::string* error) {
    if (targets.empty()) {
        reset();
        return true;
    }
    if (ctx == NULL || !JS_IsFunction(ctx, apply_fn) || !JS_IsObject(prepared_plan)) {
        reset();
        return fail_closed(error, "HTTP body consumption commit state is missing");
    }
    JSValue arg = prepared_plan;
    JSValue ret = JS_Call(ctx, apply_fn, JS_UNDEFINED, 1, &arg);
    if (JS_IsException(ret)) {
        clear_pending_exception(ctx);
        reset();
        return fail_closed(error, "failed to commit HTTP body consumption");
    }
    JS_FreeValue(ctx, ret);
    reset();
    return true;
}

void BindingHttpConsumeBatch::reset() {
    if (ctx != NULL) {
        if (!JS_IsUndefined(prepared_plan)) {
            JS_FreeValue(ctx, prepared_plan);
            prepared_plan = JS_UNDEFINED;
        }
        for (std::size_t i = 0; i < targets.size(); ++i) {
            JS_FreeValue(ctx, targets[i]);
        }
    }
    targets.clear();
    prepared_plan = JS_UNDEFINED;
    ctx = NULL;
}

JSValue binding_call_id_to_js(JSContext* ctx, std::uint64_t call_id) {
    if (ctx == NULL || call_id == 0) {
        return ctx != NULL ? JS_ThrowRangeError(ctx, "binding call id zero is invalid")
                           : JS_EXCEPTION;
    }
    return JS_NewBigUint64(ctx, call_id);
}

bool binding_call_id_from_js(JSContext* ctx, JSValueConst value, std::uint64_t* call_id) {
    if (ctx == NULL || call_id == NULL || !JS_IsBigInt(value)) {
        return false;
    }
    std::uint64_t decoded = 0;
    if (JS_ToBigUint64(ctx, &decoded, value) != 0) {
        clear_pending_exception(ctx);
        return false;
    }
    if (decoded == 0) {
        return false;
    }
    *call_id = decoded;
    return true;
}

} // namespace capsid
