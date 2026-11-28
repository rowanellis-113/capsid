// Binding RPC v2 (v0.4.0): the C++ neutral boundary between User and Binding
// Runtimes.
//
// Plain values clone across the boundary without executing user getters,
// proxies, or mutated globals. Binding RPC v2 adds:
//   - captured per-runtime intrinsics (immune to global/prototype tampering);
//   - structured RemoteErrorInfo capture & materialization (preserving Error
//     subclasses, DOMException, and QuotaExceededError);
//   - strict BindingCallOptions parsing ({ signal?, timeoutMs? });
//   - cross-runtime AbortSignal reason capture & listener helpers.

#ifndef CAPSID_BINDING_RPC_H
#define CAPSID_BINDING_RPC_H

#include "quickjs.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace capsid {

constexpr std::size_t kMaxBindingCloneDepth = 64;
constexpr std::size_t kMaxBindingCloneNodes = 10000;
constexpr std::size_t kMaxBindingCloneBytes = 1024U * 1024U;
constexpr std::size_t kMaxRemoteErrorNameBytes = 128;
constexpr std::size_t kMaxRemoteErrorMessageBytes = 4096;
constexpr std::size_t kMaxBindingProxyHeaders = 256;
constexpr std::size_t kMaxBindingProxyHeaderBytes = 64U * 1024U;

struct NeutralHttpRequest {
    std::string url;
    std::string method;
    std::string credentials;
    std::string redirect;
    bool has_mode = false;
    std::string mode;
    std::vector<std::pair<std::string, std::string>> headers;
    int body_kind = 0; // 0 = none, 1 = inline bytes, 2 = stream proxy
    std::vector<std::uint8_t> inline_bytes;
    std::uint64_t body_stream_id = 0;
    std::uint64_t signal_bridge_id = 0; // 0 = no signal
    bool signal_pre_aborted = false;
};

struct NeutralHttpResponse {
    std::uint16_t status = 200;
    std::string status_text;
    std::string url;
    bool redirected = false;
    std::string type;
    std::vector<std::pair<std::string, std::string>> headers;
    int body_kind = 0; // 0 = none, 1 = inline bytes, 2 = stream proxy
    std::vector<std::uint8_t> inline_bytes;
    std::uint64_t body_stream_id = 0;
};

struct NeutralValue {
    enum class Kind {
        kUndefined,
        kNull,
        kBool,
        kNumber,
        kBigInt,
        kString,
        kDate,
        kBytes,
        kArray,
        kObject,
        kReadableStreamProxy,
        kWritableStreamProxy,
        kRequestProxy,
        kResponseProxy,
        kRpcHandleProxy,
    };

    Kind kind = Kind::kUndefined;
    bool bool_value = false;
    double number = 0;
    std::string bigint;              // decimal digits
    std::string string;              // string payload
    double date_ms = 0;              // kDate
    std::vector<std::uint8_t> bytes; // kBytes
    std::vector<NeutralValue> array; // kArray
    // kObject: insertion-ordered, duplicate keys rejected at clone time.
    std::vector<std::pair<std::string, NeutralValue>> object;
    std::uint64_t stream_id = 0;             // kReadableStreamProxy / kWritableStreamProxy
    NeutralHttpRequest request;              // kRequestProxy
    NeutralHttpResponse response;            // kResponseProxy
    std::uint64_t handle_id = 0;             // kRpcHandleProxy
    std::vector<std::string> handle_methods; // kRpcHandleProxy
};

enum class BindingCloneHookResult {
    kUnhandled = 0,
    kCaptured = 1,
    kFailed = 2,
};

struct BindingCloneHooks {
    BindingCloneHookResult (*try_capture)(JSContext* ctx, JSValueConst value, NeutralValue* out,
                                          std::string* error, void* opaque) = nullptr;
    JSValue (*materialize)(JSContext* ctx, const NeutralValue& value, void* opaque) = nullptr;
    void* opaque = nullptr;
};

// Intrinsic constructors, prototypes, and brand-checked native getters
// captured once per JSContext immediately after bootstrap, before any
// untrusted application or Binding package code executes.
struct BindingRpcIntrinsics {
    JSValue object_proto = JS_UNDEFINED;
    JSValue error_ctor = JS_UNDEFINED;
    JSValue error_proto = JS_UNDEFINED;
    JSValue type_error_ctor = JS_UNDEFINED;
    JSValue type_error_proto = JS_UNDEFINED;
    JSValue range_error_ctor = JS_UNDEFINED;
    JSValue range_error_proto = JS_UNDEFINED;
    JSValue syntax_error_ctor = JS_UNDEFINED;
    JSValue syntax_error_proto = JS_UNDEFINED;
    JSValue uri_error_ctor = JS_UNDEFINED;
    JSValue uri_error_proto = JS_UNDEFINED;
    JSValue reference_error_ctor = JS_UNDEFINED;
    JSValue reference_error_proto = JS_UNDEFINED;
    JSValue eval_error_ctor = JS_UNDEFINED;
    JSValue eval_error_proto = JS_UNDEFINED;
    JSValue dom_exception_ctor = JS_UNDEFINED;
    JSValue dom_exception_name_getter = JS_UNDEFINED;
    JSValue dom_exception_message_getter = JS_UNDEFINED;
    JSValue quota_exceeded_ctor = JS_UNDEFINED;
    JSValue quota_exceeded_capture = JS_UNDEFINED;
    JSValue abort_signal_aborted_getter = JS_UNDEFINED;
    JSValue abort_signal_reason_getter = JS_UNDEFINED;
    JSValue event_target_add_listener = JS_UNDEFINED;
    JSValue event_target_remove_listener = JS_UNDEFINED;
    JSValue symbol_dispose = JS_UNDEFINED;
    JSValue symbol_async_dispose = JS_UNDEFINED;
};

bool capture_binding_rpc_intrinsics(JSContext* ctx, BindingRpcIntrinsics* out);
void free_binding_rpc_intrinsics(JSContext* ctx, BindingRpcIntrinsics* intrinsics);

enum class RemoteErrorKind {
    kError = 0,
    kTypeError,
    kRangeError,
    kSyntaxError,
    kURIError,
    kReferenceError,
    kEvalError,
    kDOMException,
    kQuotaExceededError,
};

struct RemoteErrorInfo {
    RemoteErrorKind kind = RemoteErrorKind::kError;
    std::string name;
    std::string message;
    bool has_quota = false;
    double quota = 0;
    bool has_requested = false;
    double requested = 0;

    static RemoteErrorInfo make_error(const std::string& message);
    static RemoteErrorInfo make_type_error(const std::string& message);
    static RemoteErrorInfo make_dom_exception(const std::string& name, const std::string& message);
};

// Inspects a JS error/rejection value without running user getters, Proxy
// traps, or overridden prototype methods, and records a runtime-neutral
// structured error snapshot.
void capture_remote_error(JSContext* ctx, JSValueConst value,
                          const BindingRpcIntrinsics* intrinsics, const char* fallback,
                          RemoteErrorInfo* out);

// Materializes an authentic Error / subclass / DOMException / QuotaExceededError
// in `ctx` using captured intrinsics when provided. Caller owns the result.
JSValue materialize_remote_error(JSContext* ctx, const RemoteErrorInfo& info,
                                 const BindingRpcIntrinsics* intrinsics);

// Cross-runtime abort reason carried from User AbortSignal to Binding
// AbortController.abort(reason). Preserves structured errors or plain cloneable
// values and falls back to default AbortError for uncloneable objects.
struct BindingAbortReason {
    enum class Kind {
        kDefaultAbort = 0,
        kRemoteError,
        kNeutralValue,
    };

    Kind kind = Kind::kDefaultAbort;
    RemoteErrorInfo error;
    NeutralValue value;
};

void capture_abort_reason(JSContext* ctx, JSValueConst value,
                          const BindingRpcIntrinsics* intrinsics, BindingAbortReason* out);
JSValue materialize_abort_reason(JSContext* ctx, const BindingAbortReason& reason,
                                 const BindingRpcIntrinsics* intrinsics);

struct BindingCallOptions {
    bool has_timeout_ms = false;
    std::uint64_t timeout_ms = 0;
    bool has_signal = false;
    JSValue signal = JS_UNDEFINED; // owned in caller ctx; release via free_binding_call_options
};

// Releases owned JSValues held by `BindingCallOptions` (idempotent).
void free_binding_call_options(JSContext* ctx, BindingCallOptions* options);

// Strict parser for optional `binding.method(input, options)` second argument.
// Rejects non-plain objects, proxies, accessors, unknown keys, non-finite or
// negative timeoutMs, and non-AbortSignal `signal` values without invoking any
// attacker-controlled getters or traps. On success, `out->signal` holds an
// owned reference that remains valid across subsequent JS execution.
bool parse_binding_call_options(JSContext* ctx, JSValueConst value,
                                const BindingRpcIntrinsics* intrinsics, BindingCallOptions* out,
                                std::string* error);

// Brand-checked native AbortSignal helpers immune to prototype tampering.
bool binding_signal_is_aborted(JSContext* ctx, JSValueConst signal,
                               const BindingRpcIntrinsics* intrinsics, bool* out_aborted);
bool binding_signal_read_reason(JSContext* ctx, JSValueConst signal,
                                const BindingRpcIntrinsics* intrinsics, JSValue* out_reason);
bool binding_signal_add_abort_listener(JSContext* ctx, JSValueConst signal, JSValueConst listener,
                                       const BindingRpcIntrinsics* intrinsics);
void binding_signal_remove_abort_listener(JSContext* ctx, JSValueConst signal,
                                          JSValueConst listener,
                                          const BindingRpcIntrinsics* intrinsics);

// JS -> neutral. Fails closed with a static diagnostic; `out` is not
// touched on failure.
bool neutral_from_js(JSContext* ctx, JSValueConst value, NeutralValue* out, std::string* error);
bool neutral_from_js_ex(JSContext* ctx, JSValueConst value, NeutralValue* out, std::string* error,
                        const BindingCloneHooks* hooks);

// neutral -> JS in the caller's context. The caller owns the result.
JSValue neutral_to_js(JSContext* ctx, const NeutralValue& value);
JSValue neutral_to_js_ex(JSContext* ctx, const NeutralValue& value, const BindingCloneHooks* hooks);

// Estimates the byte weight of a neutral value for stream credit & ledger
// accounting.
std::size_t estimate_neutral_value_bytes(const NeutralValue& value);

// Internal Binding RPC identifiers are never Number/Int64 values: IDs above
// 2^63 must survive the JS callback-data round trip exactly and zero is
// reserved as invalid.
JSValue binding_call_id_to_js(JSContext* ctx, std::uint64_t call_id);
bool binding_call_id_from_js(JSContext* ctx, JSValueConst value, std::uint64_t* call_id);

} // namespace capsid

#endif
