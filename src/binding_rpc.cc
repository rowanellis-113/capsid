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

#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>

namespace capsid {
namespace {

constexpr double kMaxBindingTimeoutMs = 31536000000.0; // 1 year; avoids ns overflow

struct CloneContext {
    JSContext* ctx;
    std::size_t depth;
    std::size_t nodes;
    std::size_t bytes;
    std::unordered_set<const void*> ancestors; // cycle detection
    JSValue object_proto;                      // Object.prototype, referenced
    const BindingCloneHooks* hooks;
};

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

bool clone_string(JSContext* ctx, JSValueConst value, std::string* target, std::string* error,
                  CloneContext* context) {
    const char* text = JS_ToCString(ctx, value);
    if (text == NULL) {
        return fail_closed(error, "unprintable string");
    }
    const std::size_t size = std::strlen(text);
    if (size > kMaxBindingCloneBytes - context->bytes) {
        JS_FreeCString(ctx, text);
        return fail_closed(error, "clone byte limit exceeded");
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
        out->kind = NeutralValue::Kind::kUndefined;
        return true;
    }
    if (JS_IsNull(value)) {
        out->kind = NeutralValue::Kind::kNull;
        return true;
    }
    if (JS_IsBool(value)) {
        out->kind = NeutralValue::Kind::kBool;
        out->bool_value = JS_ToBool(ctx, value) != 0;
        return true;
    }
    if (JS_IsNumber(value)) {
        out->kind = NeutralValue::Kind::kNumber;
        if (JS_ToFloat64(ctx, &out->number, value) != 0) {
            return fail_closed(error, "unrepresentable number");
        }
        return true;
    }
    if (JS_IsBigInt(value)) {
        out->kind = NeutralValue::Kind::kBigInt;
        if (!clone_string(ctx, value, &out->bigint, error, context)) {
            return false;
        }
        return true;
    }
    if (JS_IsString(value)) {
        out->kind = NeutralValue::Kind::kString;
        return clone_string(ctx, value, &out->string, error, context);
    }
    if (JS_IsSymbol(value)) {
        return fail_closed(error, "symbol values are not cloneable");
    }
    if (JS_IsDate(value)) {
        out->kind = NeutralValue::Kind::kDate;
        if (JS_GetDateValue(ctx, value, &out->date_ms) != 0) {
            return fail_closed(error, "date value is unreadable");
        }
        return true;
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
        ++context->depth;
        out->array.reserve(length);
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
    ++context->depth;
    for (uint32_t index = 0; index < count; ++index) {
        const char* key = JS_AtomToCString(ctx, tab[index].atom);
        if (key == NULL) {
            continue;
        }
        const std::string key_string(key);
        JS_FreeCString(ctx, key);
        if (key_string.empty()) {
            continue;
        }
        if (key_string.size() > kMaxBindingCloneBytes - context->bytes) {
            JS_FreePropertyEnum(ctx, tab, count);
            --context->depth;
            context->ancestors.erase(identity);
            return fail_closed(error, "clone byte limit exceeded");
        }
        context->bytes += key_string.size();
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
        out->object.push_back(std::make_pair(key_string, std::move(child)));
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

bool parse_binding_call_options(JSContext* ctx, JSValueConst value,
                                const BindingRpcIntrinsics* intrinsics, BindingCallOptions* out,
                                std::string* error) {
    if (ctx == NULL || out == NULL) {
        return fail_closed(error, "binding call options arguments are invalid");
    }
    *out = BindingCallOptions();
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
        const char* key = JS_AtomToCString(ctx, tab[index].atom);
        if (key == NULL) {
            JS_FreePropertyEnum(ctx, tab, count);
            return fail_closed(error, "binding call option key is unreadable");
        }
        const std::string key_str(key);
        JS_FreeCString(ctx, key);

        JSPropertyDescriptor desc;
        if (JS_GetOwnProperty(ctx, &desc, value, tab[index].atom) < 0) {
            clear_pending_exception(ctx);
            JS_FreePropertyEnum(ctx, tab, count);
            return fail_closed(error, "binding call option property is unreadable");
        }
        const bool is_accessor = !JS_IsUndefined(desc.getter) || !JS_IsUndefined(desc.setter);
        JSValue prop = desc.value;
        JS_FreeValue(ctx, desc.getter);
        JS_FreeValue(ctx, desc.setter);
        if (is_accessor) {
            JS_FreeValue(ctx, prop);
            JS_FreePropertyEnum(ctx, tab, count);
            return fail_closed(error, "binding call options do not allow accessor properties");
        }

        if (key_str == "timeoutMs") {
            if (!JS_IsUndefined(prop)) {
                double ms = 0;
                if (!JS_IsNumber(prop) || JS_ToFloat64(ctx, &ms, prop) != 0 || !std::isfinite(ms) ||
                    ms < 0.0 || ms > kMaxBindingTimeoutMs) {
                    JS_FreeValue(ctx, prop);
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
                    JS_FreePropertyEnum(ctx, tab, count);
                    return fail_closed(error,
                                       "binding call option 'signal' must be an AbortSignal");
                }
                parsed.has_signal = true;
                // `prop` is owned by the options object (still rooted on the
                // caller stack), so drop the extra descriptor ref and borrow.
                parsed.signal = prop;
            }
            JS_FreeValue(ctx, prop);
        } else {
            JS_FreeValue(ctx, prop);
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
            if (JS_SetPropertyStr(ctx, object, entry.first.c_str(), child) < 0) {
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
        return value.bigint.size() + 8;
    case NeutralValue::Kind::kString:
        return value.string.size() + 1;
    case NeutralValue::Kind::kBytes:
        return value.bytes.size();
    case NeutralValue::Kind::kArray: {
        std::size_t total = 8;
        for (std::size_t i = 0; i < value.array.size(); ++i) {
            total += estimate_neutral_value_bytes(value.array[i]);
        }
        return total;
    }
    case NeutralValue::Kind::kObject: {
        std::size_t total = 8;
        for (std::size_t i = 0; i < value.object.size(); ++i) {
            total +=
                value.object[i].first.size() + estimate_neutral_value_bytes(value.object[i].second);
        }
        return total;
    }
    case NeutralValue::Kind::kReadableStreamProxy:
    case NeutralValue::Kind::kWritableStreamProxy:
        return 16;
    case NeutralValue::Kind::kRequestProxy:
        return 64 + value.request.inline_bytes.size();
    case NeutralValue::Kind::kResponseProxy:
        return 64 + value.response.inline_bytes.size();
    case NeutralValue::Kind::kRpcHandleProxy:
        return 32 + value.handle_methods.size() * 16;
    }
    return 1;
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
