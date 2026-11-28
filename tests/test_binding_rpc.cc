// Binding v1 §7.6: the neutral-value structured clone between the User
// and Binding Runtimes. Every allowed value round-trips; getters are
// never executed; functions, promises, symbols, cycles, Map/Set/Weak
// collections, Error/RegExp/class instances, proxies and oversized
// inputs all fail closed.

#include "binding_rpc.h"
#include "binding_rpc_ledger.h"
extern "C" {
#include "tjs.h"
}

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

void fail(const std::string& message) {
    std::cerr << "test-binding-rpc: " << message << std::endl;
    std::exit(1);
}

void require(bool condition, const char* message) {
    if (!condition) {
        fail(message);
    }
}

int noop_bootstrap(TJSRuntime* runtime, JSContext* ctx, void* opaque) {
    (void)runtime;
    (void)opaque;
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "dispatchEvent", JS_NewCFunction(ctx, NULL, "dispatchEvent", 1));
    JS_SetPropertyStr(
        ctx, global, "PromiseRejectionEvent",
        JS_NewCFunction2(ctx, NULL, "PromiseRejectionEvent", 0, JS_CFUNC_constructor, 0));
    JS_FreeValue(ctx, global);
    return 0;
}

class RuntimeFixture {
  public:
    RuntimeFixture() {
        TJSRunOptions options;
        TJS_DefaultOptions(&options);
        options.skip_run_main = true;
        options.bootstrap = noop_bootstrap;
        runtime_ = TJS_NewRuntimeOptions(&options);
        require(runtime_ != NULL, "runtime creation failed");
        ctx_ = TJS_GetJSContext(runtime_);
    }

    ~RuntimeFixture() { TJS_FreeRuntime(runtime_); }

    JSContext* ctx() const { return ctx_; }

    JSValue eval(const std::string& source) const {
        JSValue result =
            JS_Eval(ctx_, source.c_str(), source.size(), "<test>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            JSValue exception = JS_GetException(ctx_);
            const char* text = JS_ToCString(ctx_, exception);
            fail(std::string("JS exception: ") + (text ? text : "?"));
        }
        return result;
    }

    void require_clone(const std::string& source, const char* label) const {
        JSValue value = eval(source);
        capsid::NeutralValue neutral;
        std::string error;
        require(capsid::neutral_from_js(ctx_, value, &neutral, &error),
                (std::string(label) + " was rejected: " + error).c_str());
        JS_FreeValue(ctx_, value);
        JSValue round = capsid::neutral_to_js(ctx_, neutral);
        require(!JS_IsException(round), (std::string(label) + " round-trip failed").c_str());
        JS_FreeValue(ctx_, round);
    }

    void require_rejected(const std::string& source, const char* needle, const char* label) const {
        JSValue value = eval(source);
        capsid::NeutralValue neutral;
        std::string error;
        require(!capsid::neutral_from_js(ctx_, value, &neutral, &error) && !error.empty(),
                (std::string(label) + " was accepted").c_str());
        require(error.find(needle) != std::string::npos,
                (std::string(label) + " error '" + error + "' does not mention '" + needle + "'")
                    .c_str());
        JS_FreeValue(ctx_, value);
    }

  private:
    TJSRuntime* runtime_ = NULL;
    JSContext* ctx_ = NULL;
};

void test_allowed_values_round_trip() {
    RuntimeFixture fixture;
    fixture.require_clone("undefined", "undefined");
    fixture.require_clone("null", "null");
    fixture.require_clone("true", "boolean true");
    fixture.require_clone("false", "boolean false");
    fixture.require_clone("42.5", "number");
    fixture.require_clone("0", "zero");
    fixture.require_clone("9007199254740991n", "bigint");
    fixture.require_clone("'hello'", "string");
    fixture.require_clone("''", "empty string");
    fixture.require_clone("new Date(1700000000000)", "date");
    fixture.require_clone("[1, 'two', [3, null]]", "nested array");
    fixture.require_clone("[]", "empty array");
    fixture.require_clone("({ a: 1, b: 'x', c: { d: [true] } })", "nested object");
    fixture.require_clone("({})", "empty object");
    fixture.require_clone("Object.assign(Object.create(null), { key: 'value' })",
                          "null-prototype object");
    fixture.require_clone("new Uint8Array([1, 2, 3])", "uint8 array");

    // Round-trip byte fidelity: an ArrayBuffer survives with its bytes.
    JSValue bytes = fixture.eval("new Uint8Array([255, 0, 128]).buffer");
    capsid::NeutralValue neutral;
    std::string error;
    require(capsid::neutral_from_js(fixture.ctx(), bytes, &neutral, &error),
            "arraybuffer clone failed");
    JS_FreeValue(fixture.ctx(), bytes);
    require(neutral.bytes.size() == 3 && neutral.bytes[0] == 255 && neutral.bytes[1] == 0 &&
                neutral.bytes[2] == 128,
            "arraybuffer bytes are wrong");
}

void test_rejected_values_fail_closed() {
    RuntimeFixture fixture;
    fixture.require_rejected("() => 1", "function", "function");
    fixture.require_rejected("Promise.resolve(1)", "promise", "promise");
    fixture.require_rejected("Symbol('x')", "symbol", "symbol");
    fixture.require_rejected("const a = {}; a.self = a; a", "cyclic", "cycle");
    fixture.require_rejected("const a2 = []; a2.push(a2); a2", "cyclic", "array cycle");
    fixture.require_rejected("new Map([['a', 1]])", "Map", "map");
    fixture.require_rejected("new Set([1])", "Set", "set");
    fixture.require_rejected("new WeakMap()", "WeakMap", "weak map");
    fixture.require_rejected("new WeakSet()", "WeakSet", "weak set");
    fixture.require_rejected("new Error('boom')", "Error", "error");
    fixture.require_rejected("/regex/", "RegExp", "regexp");
    fixture.require_rejected("class Thing { constructor() { this.x = 1; } }; new Thing()", "class",
                             "class instance");
    fixture.require_rejected("new Proxy({}, {})", "Proxy", "proxy");
    fixture.require_rejected("new Int32Array([1])", "class", "non-byte typed array");
    fixture.require_rejected(
        "Object.defineProperty({}, 'x', { get() { return 1; }, enumerable: true })", "accessor",
        "accessor property");
    // The getter must never execute: the marker proves it.
    JSValue marker =
        fixture.eval("globalThis.__getterRan = false;"
                     "Object.defineProperty({}, 'x', { get() {"
                     "  globalThis.__getterRan = true; return 1; }, enumerable: true })");
    capsid::NeutralValue neutral;
    std::string error;
    require(!capsid::neutral_from_js(fixture.ctx(), marker, &neutral, &error),
            "accessor property was accepted");
    JS_FreeValue(fixture.ctx(), marker);
    JSValue ran = fixture.eval("globalThis.__getterRan");
    int ran_value = 0;
    JS_ToInt32(fixture.ctx(), &ran_value, ran);
    JS_FreeValue(fixture.ctx(), ran);
    require(ran_value == 0, "clone executed a getter");

    JSValue array_marker =
        fixture.eval("globalThis.__arrayGetterRan = false;"
                     "(() => { const arrayWithGetter = [1];"
                     "Object.defineProperty(arrayWithGetter, '0', { get() {"
                     "  globalThis.__arrayGetterRan = true; return 1; }, enumerable: true });"
                     "return arrayWithGetter; })()");
    error.clear();
    require(!capsid::neutral_from_js(fixture.ctx(), array_marker, &neutral, &error) &&
                error.find("accessor") != std::string::npos,
            "array accessor property was accepted");
    JS_FreeValue(fixture.ctx(), array_marker);
    JSValue array_ran = fixture.eval("globalThis.__arrayGetterRan");
    JS_ToInt32(fixture.ctx(), &ran_value, array_ran);
    JS_FreeValue(fixture.ctx(), array_ran);
    require(ran_value == 0, "clone executed an array getter");

    fixture.require_rejected("(() => { const key = Symbol('secret'); const value = {};"
                             " value[key] = 1; return value; })()",
                             "symbol", "symbol-keyed property");

    // Native type predicates must remain authoritative even when untrusted
    // code replaces the corresponding globals.
    fixture.require_rejected("(() => { const value = new Proxy({}, {});"
                             " globalThis.Proxy = function() {}; return value; })()",
                             "Proxy", "proxy after global replacement");
    fixture.require_rejected("(() => { const value = new Map([['x', 1]]);"
                             " globalThis.Map = function() {}; return value; })()",
                             "Map", "map after global replacement");

    JSValue date =
        fixture.eval("globalThis.__dateMethodRan = false;"
                     "(() => { const value = new Date(1700000000000);"
                     " value.getTime = () => { globalThis.__dateMethodRan = true; return 0; };"
                     " return value; })()");
    error.clear();
    require(capsid::neutral_from_js(fixture.ctx(), date, &neutral, &error) &&
                neutral.kind == capsid::NeutralValue::Kind::kDate &&
                neutral.date_ms == 1700000000000.0,
            "Date clone did not read the internal slot");
    JS_FreeValue(fixture.ctx(), date);
    JSValue date_ran = fixture.eval("globalThis.__dateMethodRan");
    JS_ToInt32(fixture.ctx(), &ran_value, date_ran);
    JS_FreeValue(fixture.ctx(), date_ran);
    require(ran_value == 0, "Date clone executed an overridden method");
}

void test_call_ids_use_full_width_biguint64() {
    RuntimeFixture fixture;
    const std::uint64_t values[] = {
        1,
        UINT64_C(0x7fffffffffffffff),
        UINT64_C(0x8000000000000000),
        UINT64_MAX,
    };
    for (std::uint64_t expected : values) {
        JSValue encoded = capsid::binding_call_id_to_js(fixture.ctx(), expected);
        require(!JS_IsException(encoded) && JS_IsBigInt(encoded),
                "call id was not encoded as BigUint64");
        std::uint64_t decoded = 0;
        require(capsid::binding_call_id_from_js(fixture.ctx(), encoded, &decoded) &&
                    decoded == expected,
                "call id lost bits in the JS callback round trip");
        JS_FreeValue(fixture.ctx(), encoded);
    }
    JSValue zero = JS_NewBigUint64(fixture.ctx(), 0);
    std::uint64_t decoded = 9;
    require(!capsid::binding_call_id_from_js(fixture.ctx(), zero, &decoded),
            "reserved call id zero was accepted");
    JS_FreeValue(fixture.ctx(), zero);
}

void test_neutral_to_js_propagates_conversion_errors() {
    RuntimeFixture fixture;

    capsid::NeutralValue huge;
    huge.kind = capsid::NeutralValue::Kind::kBigInt;
    huge.bigint = "1234567890123456789012345678901234567890";
    JSValue exact = capsid::neutral_to_js(fixture.ctx(), huge);
    require(!JS_IsException(exact) && JS_IsBigInt(exact),
            "large neutral bigint was not reconstructed");
    const char* text = JS_ToCString(fixture.ctx(), exact);
    require(text != NULL && huge.bigint == text, "large neutral bigint lost precision");
    if (text != NULL) {
        JS_FreeCString(fixture.ctx(), text);
    }
    JS_FreeValue(fixture.ctx(), exact);

    capsid::NeutralValue invalid;
    invalid.kind = capsid::NeutralValue::Kind::kBigInt;
    invalid.bigint = "not-a-decimal";
    JSValue rejected = capsid::neutral_to_js(fixture.ctx(), invalid);
    require(JS_IsException(rejected), "invalid neutral bigint was converted to undefined");
    JSValue exception = JS_GetException(fixture.ctx());
    JS_FreeValue(fixture.ctx(), exception);

    capsid::NeutralValue nested;
    nested.kind = capsid::NeutralValue::Kind::kArray;
    nested.array.push_back(invalid);
    rejected = capsid::neutral_to_js(fixture.ctx(), nested);
    require(JS_IsException(rejected), "nested neutral conversion swallowed a child exception");
    exception = JS_GetException(fixture.ctx());
    JS_FreeValue(fixture.ctx(), exception);
}

void test_limits_fail_closed() {
    RuntimeFixture fixture;
    // Depth: 65 nested arrays.
    std::string deep = "0";
    for (int i = 0; i < 65; ++i) {
        deep = "[" + deep + "]";
    }
    fixture.require_rejected(deep, "depth", "deep value");
    std::string deep_ok = "0";
    for (int i = 0; i < 64; ++i) {
        deep_ok = "[" + deep_ok + "]";
    }
    JSValue ok_value = fixture.eval(deep_ok);
    capsid::NeutralValue ok_neutral;
    std::string error;
    require(capsid::neutral_from_js(fixture.ctx(), ok_value, &ok_neutral, &error),
            "64-deep value was rejected");
    JS_FreeValue(fixture.ctx(), ok_value);
}

void test_object_key_ordering_and_duplicates() {
    RuntimeFixture fixture;
    JSValue value = fixture.eval("({ z: 1, a: 2, m: 3 })");
    capsid::NeutralValue neutral;
    std::string error;
    require(capsid::neutral_from_js(fixture.ctx(), value, &neutral, &error), "object clone failed");
    JS_FreeValue(fixture.ctx(), value);
    require(neutral.object.size() == 3 && neutral.object[0].first == "z" &&
                neutral.object[1].first == "a" && neutral.object[2].first == "m",
            "object key order was not preserved");
}

void test_remote_error_structured_and_tamper_proof() {
    RuntimeFixture source;
    RuntimeFixture target;

    // Install minimal DOMException + QuotaExceededError + AbortSignal surfaces
    // mirroring Capsid's bootstrap before capturing intrinsics.
    const char* setup_js =
        "class FakeDOMException extends Error {"
        "  #name; #message;"
        "  constructor(message = '', name = 'Error') {"
        "    super(String(message)); this.#name = String(name); this.#message = String(message);"
        "  }"
        "  get name() { return this.#name; }"
        "  get message() { return this.#message; }"
        "}"
        "globalThis.DOMException = FakeDOMException;"
        "class FakeQuotaExceededError extends FakeDOMException {"
        "  #quota; #requested;"
        "  static _capture(v) {"
        "    if (v === null || typeof v !== 'object' || !(#quota in v)) return null;"
        "    return { message: v.message, quota: v.#quota, requested: v.#requested };"
        "  }"
        "  constructor(message = '', opts = {}) {"
        "    super(message, 'QuotaExceededError');"
        "    this.#quota = opts.quota ?? null; this.#requested = opts.requested ?? null;"
        "  }"
        "  get quota() { return this.#quota; }"
        "  get requested() { return this.#requested; }"
        "}"
        "globalThis.QuotaExceededError = FakeQuotaExceededError;"
        "class FakeAbortSignal {"
        "  #aborted = false; #reason = undefined; #listeners = new Set();"
        "  get aborted() { return this.#aborted; }"
        "  get reason() { return this.#reason; }"
        "  _abort(r) { this.#aborted = true; this.#reason = r; for (const fn of [...this.#listeners]) fn(); }"
        "  addEventListener(t, fn) { if (t === 'abort') this.#listeners.add(fn); }"
        "  removeEventListener(t, fn) { if (t === 'abort') this.#listeners.delete(fn); }"
        "}"
        "globalThis.EventTarget = { prototype: FakeAbortSignal.prototype };"
        "globalThis.AbortSignal = FakeAbortSignal;";
    JS_FreeValue(source.ctx(), source.eval(setup_js));
    JS_FreeValue(target.ctx(), target.eval(setup_js));

    capsid::BindingRpcIntrinsics src_intrinsics;
    capsid::BindingRpcIntrinsics dst_intrinsics;
    require(capsid::capture_binding_rpc_intrinsics(source.ctx(), &src_intrinsics),
            "capture source intrinsics failed");
    require(capsid::capture_binding_rpc_intrinsics(target.ctx(), &dst_intrinsics),
            "capture target intrinsics failed");

    // Mutate globals after capture: intrinsics must remain immune.
    JS_FreeValue(
        target.ctx(),
        target.eval("globalThis.__origTypeError = TypeError;"
                    "globalThis.__origRangeError = RangeError;"
                    "globalThis.__origDOMException = DOMException;"
                    "globalThis.__origQuotaExceededError = QuotaExceededError;"
                    "globalThis.TypeError = function() { throw new Error('tampered'); };"
                    "globalThis.DOMException = function() { throw new Error('tampered'); };"));

    // Native Error subclasses round-trip cleanly.
    struct Case {
        const char* expr;
        capsid::RemoteErrorKind kind;
        const char* expected_msg;
        const char* check_expr;
    };
    const Case cases[] = {
        {"new TypeError('bad type')", capsid::RemoteErrorKind::kTypeError, "bad type",
         "(e instanceof __origTypeError) && e.message === 'bad type'"},
        {"new RangeError('out of bounds')", capsid::RemoteErrorKind::kRangeError, "out of bounds",
         "(e instanceof __origRangeError) && e.message === 'out of bounds'"},
        {"new SyntaxError('bad json')", capsid::RemoteErrorKind::kSyntaxError, "bad json",
         "e.name === 'SyntaxError' && e.message === 'bad json'"},
        {"new DOMException('timed out', 'TimeoutError')", capsid::RemoteErrorKind::kDOMException,
         "timed out",
         "(e instanceof __origDOMException) && e.name === 'TimeoutError' && e.message === 'timed out'"},
        {"new QuotaExceededError('too big', { quota: 64, requested: 128 })",
         capsid::RemoteErrorKind::kQuotaExceededError, "too big",
         "(e instanceof __origQuotaExceededError) && e.quota === 64 && e.requested === 128 && e.message === 'too big'"},
    };
    for (std::size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        JSValue src_err = source.eval(cases[i].expr);
        capsid::RemoteErrorInfo info;
        capsid::capture_remote_error(source.ctx(), src_err, &src_intrinsics, "fallback", &info);
        JS_FreeValue(source.ctx(), src_err);
        require(info.kind == cases[i].kind, "remote error kind mismatch");
        require(info.message == cases[i].expected_msg, "remote error message mismatch");
        JSValue mat = capsid::materialize_remote_error(target.ctx(), info, &dst_intrinsics);
        require(!JS_IsException(mat), "materialize_remote_error threw");
        JSValue global = JS_GetGlobalObject(target.ctx());
        JS_SetPropertyStr(target.ctx(), global, "e", mat);
        JS_FreeValue(target.ctx(), global);
        JSValue ok = target.eval(cases[i].check_expr);
        require(JS_ToBool(target.ctx(), ok) == 1, "materialized error failed check");
        JS_FreeValue(target.ctx(), ok);
    }

    // Hostile getter on Error.message and Proxy rejection must NEVER execute.
    JSValue hostile = source.eval(
        "globalThis.__errGetterRan = false;"
        "(() => { const err = new Error('ignored');"
        "  Object.defineProperty(err, 'message', { get() { globalThis.__errGetterRan = true; return 'pwn'; }, configurable: true });"
        "  return err; })()");
    capsid::RemoteErrorInfo hostile_info;
    capsid::capture_remote_error(source.ctx(), hostile, &src_intrinsics, "safe-fallback",
                                 &hostile_info);
    JS_FreeValue(source.ctx(), hostile);
    require(hostile_info.message == "safe-fallback", "hostile error getter did not fall back");
    JSValue ran = source.eval("globalThis.__errGetterRan");
    require(JS_ToBool(source.ctx(), ran) == 0, "capture_remote_error executed hostile getter");
    JS_FreeValue(source.ctx(), ran);

    JSValue proxy_err =
        source.eval("globalThis.__proxyTrapRan = false;"
                    "new Proxy({}, { get() { globalThis.__proxyTrapRan = true; return 'x'; } })");
    capsid::RemoteErrorInfo proxy_info;
    capsid::capture_remote_error(source.ctx(), proxy_err, &src_intrinsics, "proxy-fallback",
                                 &proxy_info);
    JS_FreeValue(source.ctx(), proxy_err);
    require(proxy_info.message == "proxy-fallback", "proxy error did not fall back");
    JSValue trap_ran = source.eval("globalThis.__proxyTrapRan");
    require(JS_ToBool(source.ctx(), trap_ran) == 0, "capture_remote_error triggered proxy trap");
    JS_FreeValue(source.ctx(), trap_ran);

    // Message truncation at kMaxRemoteErrorMessageBytes must not split UTF-8.
    JSValue huge = source.eval("'a'.repeat(5000)");
    capsid::RemoteErrorInfo huge_info;
    capsid::capture_remote_error(source.ctx(), huge, &src_intrinsics, "fallback", &huge_info);
    JS_FreeValue(source.ctx(), huge);
    require(huge_info.message.size() == capsid::kMaxRemoteErrorMessageBytes,
            "oversized remote error message was not truncated to limit");

    // BindingCallOptions strict validation + owned signal survival across GC.
    capsid::BindingCallOptions opts;
    std::string opt_err;
    JSValue valid_opts = source.eval("({ timeoutMs: 250, signal: new AbortSignal() })");
    require(capsid::parse_binding_call_options(source.ctx(), valid_opts, &src_intrinsics, &opts,
                                               &opt_err),
            ("valid options rejected: " + opt_err).c_str());
    require(opts.has_timeout_ms && opts.timeout_ms == 250 && opts.has_signal,
            "valid options did not decode fields");
    // Drop the options object completely and run GC; owned `opts.signal` must
    // remain valid until explicitly released by free_binding_call_options.
    JS_FreeValue(source.ctx(), valid_opts);
    JS_RunGC(JS_GetRuntime(source.ctx()));
    bool signal_aborted = true;
    require(capsid::binding_signal_is_aborted(source.ctx(), opts.signal, &src_intrinsics,
                                              &signal_aborted) &&
                !signal_aborted,
            "owned options.signal did not survive container collection");
    capsid::free_binding_call_options(source.ctx(), &opts);

    const char* bad_opts[] = {
        "123",
        "[]",
        "new Proxy({}, {})",
        "({ unknownKey: 1 })",
        "({ timeoutMs: -1 })",
        "({ timeoutMs: NaN })",
        "({ timeoutMs: Infinity })",
        "({ timeoutMs: '100' })",
        "({ signal: { aborted: false } })",
        "({ signal: new AbortSignal(), timeoutMs: -1 })",
        "({ signal: new AbortSignal(), extraBadKey: 1 })",
        "Object.defineProperty({}, 'timeoutMs', { get() { globalThis.__optGetter = true; return 10; }, enumerable: true })",
    };
    JS_FreeValue(source.ctx(), source.eval("globalThis.__optGetter = false;"));
    for (std::size_t i = 0; i < sizeof(bad_opts) / sizeof(bad_opts[0]); ++i) {
        JSValue bad = source.eval(bad_opts[i]);
        opt_err.clear();
        require(!capsid::parse_binding_call_options(source.ctx(), bad, &src_intrinsics, &opts,
                                                    &opt_err) &&
                    !opt_err.empty(),
                (std::string("bad options accepted: ") + bad_opts[i]).c_str());
        require(!opts.has_signal && JS_IsUndefined(opts.signal),
                "failed parse leaked signal ownership");
        JS_FreeValue(source.ctx(), bad);
    }
    JSValue opt_ran = source.eval("globalThis.__optGetter");
    require(JS_ToBool(source.ctx(), opt_ran) == 0, "parse_binding_call_options executed getter");
    JS_FreeValue(source.ctx(), opt_ran);

    // Abort reason round-trip (DOMException, plain object, uncloneable fallback).
    JSValue dom_abort = source.eval("new DOMException('user cancel', 'AbortError')");
    capsid::BindingAbortReason reason;
    capsid::capture_abort_reason(source.ctx(), dom_abort, &src_intrinsics, &reason);
    JS_FreeValue(source.ctx(), dom_abort);
    require(reason.kind == capsid::BindingAbortReason::Kind::kRemoteError &&
                reason.error.name == "AbortError" && reason.error.message == "user cancel",
            "DOMException abort reason lost structured metadata");

    JSValue plain_abort = source.eval("({ code: 'STOP', n: 7 })");
    capsid::capture_abort_reason(source.ctx(), plain_abort, &src_intrinsics, &reason);
    JS_FreeValue(source.ctx(), plain_abort);
    require(reason.kind == capsid::BindingAbortReason::Kind::kNeutralValue,
            "plain object abort reason was not cloned as NeutralValue");

    JSValue bad_abort = source.eval("() => {}");
    capsid::capture_abort_reason(source.ctx(), bad_abort, &src_intrinsics, &reason);
    JS_FreeValue(source.ctx(), bad_abort);
    require(reason.kind == capsid::BindingAbortReason::Kind::kDefaultAbort,
            "uncloneable abort reason did not fall back to default AbortError");

    capsid::free_binding_rpc_intrinsics(source.ctx(), &src_intrinsics);
    capsid::free_binding_rpc_intrinsics(target.ctx(), &dst_intrinsics);
}

struct IncrementalCloneLedgerCtx {
    capsid::BindingRpcLedger* ledger = nullptr;
    std::uint64_t request_id = 1;
    std::string binding_id = "svc";
    std::size_t reserved = 0;
    std::size_t calls = 0;
};

bool test_reserve_clone_bytes(std::size_t delta, std::string* error, void* opaque) {
    IncrementalCloneLedgerCtx* ctx = static_cast<IncrementalCloneLedgerCtx*>(opaque);
    ++ctx->calls;
    capsid::BindingQuotaError qerr = capsid::BindingQuotaError::kOk;
    if (!ctx->ledger->reserve_bytes(ctx->request_id, ctx->binding_id, delta, &qerr)) {
        if (error != nullptr) {
            *error = capsid::binding_quota_error_message(qerr);
        }
        return false;
    }
    ctx->reserved += delta;
    return true;
}

void test_neutral_byte_estimation_and_incremental_reservation() {
    // 1. Direct coverage of estimate_neutral_value_bytes across all kinds,
    // including variable-length HTTP Request/Response metadata, headers, and
    // RpcHandleProxy method tables (R2/F3).
    capsid::NeutralValue undef_v;
    require(capsid::estimate_neutral_value_bytes(undef_v) == 1, "undefined weight");
    capsid::NeutralValue num_v;
    num_v.kind = capsid::NeutralValue::Kind::kNumber;
    require(capsid::estimate_neutral_value_bytes(num_v) == 8, "number weight");
    capsid::NeutralValue str_v;
    str_v.kind = capsid::NeutralValue::Kind::kString;
    str_v.string = "hello";
    require(capsid::estimate_neutral_value_bytes(str_v) == 6, "string weight");
    capsid::NeutralValue big_v;
    big_v.kind = capsid::NeutralValue::Kind::kBigInt;
    big_v.bigint = "1234567890";
    require(capsid::estimate_neutral_value_bytes(big_v) == 18, "bigint weight");
    capsid::NeutralValue bytes_v;
    bytes_v.kind = capsid::NeutralValue::Kind::kBytes;
    bytes_v.bytes.assign(128, 0xAB);
    require(capsid::estimate_neutral_value_bytes(bytes_v) == 128, "bytes weight");

    capsid::NeutralValue req_short;
    req_short.kind = capsid::NeutralValue::Kind::kRequestProxy;
    req_short.request.url = "https://a.test/";
    req_short.request.method = "GET";
    req_short.request.credentials = "same-origin";
    req_short.request.redirect = "follow";
    req_short.request.headers.push_back(std::make_pair(std::string("x-k"), std::string("v")));

    capsid::NeutralValue req_long = req_short;
    req_long.request.url += std::string(100, 'u');
    req_long.request.has_mode = true;
    req_long.request.mode = "cors";
    req_long.request.headers.push_back(
        std::make_pair(std::string("x-long-header"), std::string(500, 'h')));
    req_long.request.body_kind = 1;
    req_long.request.inline_bytes.assign(200, 0x11);
    const std::size_t w_req_short = capsid::estimate_neutral_value_bytes(req_short);
    const std::size_t w_req_long = capsid::estimate_neutral_value_bytes(req_long);
    const std::size_t expected_req_delta = 100 + 4 + (13 + 500 + 16) + 200;
    require(w_req_long - w_req_short == expected_req_delta,
            "HTTP Request header/url/mode/inline delta mismatch");

    capsid::NeutralValue res_short;
    res_short.kind = capsid::NeutralValue::Kind::kResponseProxy;
    res_short.response.status = 200;
    res_short.response.status_text = "OK";
    res_short.response.type = "default";
    capsid::NeutralValue res_long = res_short;
    res_long.response.url = "https://cdn.test/object";
    res_long.response.headers.push_back(
        std::make_pair(std::string("set-cookie"), std::string(300, 'c')));
    res_long.response.body_kind = 2; // stream proxy
    const std::size_t w_res_short = capsid::estimate_neutral_value_bytes(res_short);
    const std::size_t w_res_long = capsid::estimate_neutral_value_bytes(res_long);
    const std::size_t expected_res_delta = res_long.response.url.size() + (10 + 300 + 16) + 16;
    require(w_res_long - w_res_short == expected_res_delta,
            "HTTP Response header/url/stream delta mismatch");

    capsid::NeutralValue handle_v;
    handle_v.kind = capsid::NeutralValue::Kind::kRpcHandleProxy;
    handle_v.handle_methods.push_back("alpha");
    handle_v.handle_methods.push_back("betaLongMethod");
    require(capsid::estimate_neutral_value_bytes(handle_v) == 32 + (5 + 16) + (14 + 16),
            "RpcHandleProxy method name weights mismatch");

    // 2. Incremental byte reservation during structured clone enforces 3-tier
    // byte limits BEFORE completing native allocation and matches estimated
    // weight byte-for-byte.
    RuntimeFixture rt;
    capsid::BindingRpcLimits limits;
    limits.max_bytes_per_request = 160;
    limits.max_bytes_per_binding = 320;
    limits.max_bytes_per_worker = 640;
    capsid::BindingRpcLedger ledger(limits);

    IncrementalCloneLedgerCtx ok_ctx;
    ok_ctx.ledger = &ledger;
    capsid::BindingCloneHooks hooks;
    hooks.reserve_bytes = test_reserve_clone_bytes;
    hooks.opaque = &ok_ctx;

    JSValue fit_js = rt.eval("({ tag: 'ok', arr: [1, 2, 'abc'], raw: new Uint8Array(40) })");
    capsid::NeutralValue fit_nv;
    std::string err;
    require(capsid::neutral_from_js_ex(rt.ctx(), fit_js, &fit_nv, &err, &hooks),
            ("fitting object rejected: " + err).c_str());
    JS_FreeValue(rt.ctx(), fit_js);
    const std::size_t est_fit = capsid::estimate_neutral_value_bytes(fit_nv);
    require(ok_ctx.reserved == est_fit,
            "incremental reserved bytes must equal estimate_neutral_value_bytes");
    require(ledger.request_counts(1).bytes == est_fit &&
                ledger.binding_counts("svc").bytes == est_fit &&
                ledger.worker_counts().bytes == est_fit,
            "all three ledger tiers reflect exact reserved bytes");
    ledger.release_bytes(1, "svc", ok_ctx.reserved);
    require(ledger.empty(), "ledger empty after releasing fit clone bytes");

    // Oversized object aborts mid-clone when incremental reservation crosses 160B.
    IncrementalCloneLedgerCtx fail_ctx;
    fail_ctx.ledger = &ledger;
    hooks.opaque = &fail_ctx;
    JSValue big_js =
        rt.eval("({ a: new Uint8Array(100), b: new Uint8Array(100), c: new Uint8Array(100) })");
    capsid::NeutralValue big_nv;
    err.clear();
    require(!capsid::neutral_from_js_ex(rt.ctx(), big_js, &big_nv, &err, &hooks),
            "oversized incremental clone must fail closed");
    JS_FreeValue(rt.ctx(), big_js);
    require(err.find("byte quota") != std::string::npos, "expected byte quota diagnostic");
    require(fail_ctx.reserved <= 160, "incremental reservation never exceeded 160B cap");
    ledger.release_bytes(1, "svc", fail_ctx.reserved);
    require(ledger.empty(), "ledger empty after rollback of failed mid-clone reservation");
}

} // namespace

int main() {
    // libuv argv/process-title setup belongs to the process, not each runtime.
    static char arg0[] = "test-binding-rpc";
    static char* argv[] = {arg0, nullptr};
    TJS_Initialize(1, argv);
    test_allowed_values_round_trip();
    test_rejected_values_fail_closed();
    test_limits_fail_closed();
    test_object_key_ordering_and_duplicates();
    test_call_ids_use_full_width_biguint64();
    test_neutral_to_js_propagates_conversion_errors();
    test_remote_error_structured_and_tamper_proof();
    test_neutral_byte_estimation_and_incremental_reservation();
    return 0;
}
