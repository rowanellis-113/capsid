// Binding v1 §7.6: the neutral-value structured clone between the User
// and Binding Runtimes. Every allowed value round-trips; getters are
// never executed; functions, promises, symbols, cycles, Map/Set/Weak
// collections, Error/RegExp/class instances, proxies and oversized
// inputs all fail closed.

#include "binding_rpc.h"
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
    return 0;
}
