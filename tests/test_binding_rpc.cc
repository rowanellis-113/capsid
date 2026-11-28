// Binding v1 §7.6: the neutral-value structured clone between the User
// and Binding Runtimes. Every allowed value round-trips; getters are
// never executed; functions, promises, symbols, cycles, Map/Set/Weak
// collections, Error/RegExp/class instances, proxies and oversized
// inputs all fail closed.

#include "binding_rpc.h"
#include "binding_rpc_ledger.h"
extern "C" {
#include "tjs.h"
extern const uint8_t capsid__bootstrap[];
extern const uint32_t capsid__bootstrap_size;
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

    // Materialize all three abort-reason variants in the target runtime and
    // verify add/remove listener + read_reason helpers directly.
    {
        capsid::BindingAbortReason def_reason;
        JSValue mat_def =
            capsid::materialize_abort_reason(target.ctx(), def_reason, &dst_intrinsics);
        require(!JS_IsException(mat_def) && JS_IsObject(mat_def), "materialize default abort");
        JSValue g = JS_GetGlobalObject(target.ctx());
        JS_SetPropertyStr(target.ctx(), g, "__matDef", mat_def);
        JS_FreeValue(target.ctx(), g);
        JSValue chk = target.eval(
            "(__matDef instanceof __origDOMException) && __matDef.name === 'AbortError'");
        require(JS_ToBool(target.ctx(), chk) == 1, "default abort reason is AbortError");
        JS_FreeValue(target.ctx(), chk);

        capsid::BindingAbortReason err_reason;
        err_reason.kind = capsid::BindingAbortReason::Kind::kRemoteError;
        err_reason.error =
            capsid::RemoteErrorInfo::make_dom_exception("TimeoutError", "custom-timeout");
        JSValue mat_err =
            capsid::materialize_abort_reason(target.ctx(), err_reason, &dst_intrinsics);
        require(!JS_IsException(mat_err), "materialize remote error abort");
        g = JS_GetGlobalObject(target.ctx());
        JS_SetPropertyStr(target.ctx(), g, "__matErr", mat_err);
        JS_FreeValue(target.ctx(), g);
        chk = target.eval(
            "(__matErr instanceof __origDOMException) && __matErr.name === 'TimeoutError' && "
            "__matErr.message === 'custom-timeout'");
        require(JS_ToBool(target.ctx(), chk) == 1, "remote error abort reason mismatch");
        JS_FreeValue(target.ctx(), chk);

        capsid::BindingAbortReason val_reason;
        val_reason.kind = capsid::BindingAbortReason::Kind::kNeutralValue;
        val_reason.value.kind = capsid::NeutralValue::Kind::kString;
        val_reason.value.string.assign("A\0B", 3);
        JSValue mat_val =
            capsid::materialize_abort_reason(target.ctx(), val_reason, &dst_intrinsics);
        require(!JS_IsException(mat_val) && JS_IsString(mat_val), "materialize value abort");
        g = JS_GetGlobalObject(target.ctx());
        JS_SetPropertyStr(target.ctx(), g, "__matVal", mat_val);
        JS_FreeValue(target.ctx(), g);
        chk = target.eval("__matVal.length === 3 && __matVal.charCodeAt(1) === 0");
        require(JS_ToBool(target.ctx(), chk) == 1, "value abort reason preserved embedded NUL");
        JS_FreeValue(target.ctx(), chk);

        JSValue sig_pair = source.eval("globalThis.__abortCalls = 0;"
                                       "(() => { const s = new AbortSignal();"
                                       "  const fn = () => { globalThis.__abortCalls++; };"
                                       "  return [s, fn]; })()");
        JSValue sig = JS_GetPropertyUint32(source.ctx(), sig_pair, 0);
        JSValue fn = JS_GetPropertyUint32(source.ctx(), sig_pair, 1);
        JS_FreeValue(source.ctx(), sig_pair);
        require(capsid::binding_signal_add_abort_listener(source.ctx(), sig, fn, &src_intrinsics),
                "add abort listener failed");
        capsid::binding_signal_remove_abort_listener(source.ctx(), sig, fn, &src_intrinsics);
        require(capsid::binding_signal_add_abort_listener(source.ctx(), sig, fn, &src_intrinsics),
                "re-add abort listener failed");
        JSValue sg = JS_GetGlobalObject(source.ctx());
        JS_SetPropertyStr(source.ctx(), sg, "__testSig", JS_DupValue(source.ctx(), sig));
        JS_FreeValue(source.ctx(), sg);
        JS_FreeValue(source.ctx(), source.eval("__testSig._abort('stop-now');"));
        JSValue calls_val = source.eval("globalThis.__abortCalls");
        int calls_i = 0;
        JS_ToInt32(source.ctx(), &calls_i, calls_val);
        JS_FreeValue(source.ctx(), calls_val);
        require(calls_i == 1, "abort listener did not fire exactly once");
        JSValue read_reason = JS_UNDEFINED;
        require(
            capsid::binding_signal_read_reason(source.ctx(), sig, &src_intrinsics, &read_reason) &&
                JS_IsString(read_reason),
            "binding_signal_read_reason failed");
        JS_FreeValue(source.ctx(), read_reason);
        JS_FreeValue(source.ctx(), sig);
        JS_FreeValue(source.ctx(), fn);
    }

    // T4: NUL-embedded option key must fail closed as unknown option.
    {
        JSValue nul_opt = source.eval("({ ['timeoutMs\\u0000evil']: 10 })");
        opt_err.clear();
        require(!capsid::parse_binding_call_options(source.ctx(), nul_opt, &src_intrinsics, &opts,
                                                    &opt_err),
                "NUL-embedded option key must be rejected");
        JS_FreeValue(source.ctx(), nul_opt);
    }

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

// T4: Strings with embedded NUL code units and plain objects with empty or
// NUL-containing keys must round-trip losslessly code-unit for code-unit.
void test_string_and_object_key_fidelity() {
    RuntimeFixture fixture;
    JSContext* ctx = fixture.ctx();

    // 1. Embedded NUL strings: [65, 0, 66], [0], [0, 65, 0], and multi-byte UTF-8.
    JSValue str_in =
        fixture.eval("[String.fromCharCode(65, 0, 66), '\\u0000', '\\u0000A\\u0000', '你好🚀']");
    capsid::NeutralValue str_nv;
    std::string err;
    require(capsid::neutral_from_js(ctx, str_in, &str_nv, &err),
            ("embedded-NUL string clone failed: " + err).c_str());
    JS_FreeValue(ctx, str_in);
    require(str_nv.kind == capsid::NeutralValue::Kind::kArray && str_nv.array.size() == 4,
            "string array shape mismatch");
    require(str_nv.array[0].string.size() == 3 && str_nv.array[0].string[0] == 'A' &&
                str_nv.array[0].string[1] == '\0' && str_nv.array[0].string[2] == 'B',
            "neutral string truncated at embedded NUL");
    require(str_nv.array[1].string.size() == 1 && str_nv.array[1].string[0] == '\0',
            "single NUL string lost");
    require(str_nv.array[2].string.size() == 3 && str_nv.array[2].string[0] == '\0' &&
                str_nv.array[2].string[1] == 'A' && str_nv.array[2].string[2] == '\0',
            "leading/trailing NUL string lost");

    JSValue str_out = capsid::neutral_to_js(ctx, str_nv);
    require(!JS_IsException(str_out), "embedded-NUL string materialization failed");
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__strOut", str_out);
    JS_FreeValue(ctx, global);
    JSValue str_ok =
        fixture.eval("(() => {"
                     "  const units = (s) => Array.from(s, (c) => c.charCodeAt(0));"
                     "  const u0 = units(__strOut[0]);"
                     "  const u1 = units(__strOut[1]);"
                     "  const u2 = units(__strOut[2]);"
                     "  return u0.length === 3 && u0[0] === 65 && u0[1] === 0 && u0[2] === 66 &&"
                     "         u1.length === 1 && u1[0] === 0 &&"
                     "         u2.length === 3 && u2[0] === 0 && u2[1] === 65 && u2[2] === 0 &&"
                     "         __strOut[3] === '你好🚀';"
                     "})()");
    require(JS_ToBool(ctx, str_ok) == 1, "round-tripped strings lost embedded NUL or UTF-8");
    JS_FreeValue(ctx, str_ok);

    // 2. Plain objects with empty key "" and NUL-embedded keys.
    JSValue obj_in =
        fixture.eval("({ '': 7, name: 'demo', ['a\\u0000b']: 99, ['\\u0000']: 'nul-val' })");
    capsid::NeutralValue obj_nv;
    err.clear();
    require(capsid::neutral_from_js(ctx, obj_in, &obj_nv, &err),
            ("special-key object clone failed: " + err).c_str());
    JS_FreeValue(ctx, obj_in);
    require(obj_nv.kind == capsid::NeutralValue::Kind::kObject && obj_nv.object.size() == 4,
            "special-key object dropped keys");
    require(obj_nv.object[0].first.empty() && obj_nv.object[0].second.number == 7.0,
            "empty key lost in NeutralValue");
    require(obj_nv.object[1].first == "name" && obj_nv.object[1].second.string == "demo",
            "name key mismatch in NeutralValue");
    require(obj_nv.object[2].first.size() == 3 && obj_nv.object[2].first[0] == 'a' &&
                obj_nv.object[2].first[1] == '\0' && obj_nv.object[2].first[2] == 'b' &&
                obj_nv.object[2].second.number == 99.0,
            "NUL-embedded object key truncated in NeutralValue");
    require(obj_nv.object[3].first.size() == 1 && obj_nv.object[3].first[0] == '\0' &&
                obj_nv.object[3].second.string == "nul-val",
            "single-NUL object key lost in NeutralValue");

    JSValue obj_out = capsid::neutral_to_js(ctx, obj_nv);
    require(!JS_IsException(obj_out), "special-key object materialization failed");
    global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__objOut", obj_out);
    JS_FreeValue(ctx, global);
    JSValue obj_ok = fixture.eval(
        "(() => {"
        "  const keys = Object.keys(__objOut);"
        "  return keys.length === 4 &&"
        "         keys[0] === '' && __objOut[''] === 7 &&"
        "         keys[1] === 'name' && __objOut.name === 'demo' &&"
        "         keys[2] === 'a\\u0000b' && keys[2].length === 3 && __objOut['a\\u0000b'] === 99 &&"
        "         keys[3] === '\\u0000' && keys[3].length === 1 && __objOut['\\u0000'] === 'nul-val';"
        "})()");
    require(JS_ToBool(ctx, obj_ok) == 1, "round-tripped object lost empty or NUL-embedded keys");
    JS_FreeValue(ctx, obj_ok);
}

// T1/R2: Exact-fit vs 1-byte-short quota boundaries for HTTP Request metadata,
// HTTP Response metadata, header pairs, and RpcTarget method tables — proving
// ZERO native copies occur on rejection and all three ledger tiers stay at
// their exact initial baseline.
void test_precopy_quota_boundaries_and_zero_copy_on_reject() {
    RuntimeFixture rt;
    JSContext* ctx = rt.ctx();
    const std::size_t kBaseline = 17;

    auto make_ledger_at_cap = [&](std::size_t cap) -> capsid::BindingRpcLedger {
        capsid::BindingRpcLimits limits;
        limits.max_bytes_per_request = cap;
        limits.max_bytes_per_binding = cap;
        limits.max_bytes_per_worker = cap;
        capsid::BindingRpcLedger ledger(limits);
        capsid::BindingQuotaError qerr = capsid::BindingQuotaError::kOk;
        require(ledger.reserve_bytes(1, "svc", kBaseline, &qerr), "baseline reserve failed");
        return ledger;
    };
    auto check_tiers = [&](const capsid::BindingRpcLedger& ledger, std::size_t expected,
                           const char* msg) {
        require(ledger.request_counts(1).bytes == expected &&
                    ledger.binding_counts("svc").bytes == expected &&
                    ledger.worker_counts().bytes == expected,
                msg);
    };

    // 1. HTTP Request metadata (64 + url + method + credentials + redirect + mode).
    JSValue req_info =
        rt.eval("({ url: 'https://t1.test/path/item', method: 'POST', credentials: 'same-origin', "
                "   redirect: 'follow', mode: 'cors' })");
    const std::size_t req_expected = 64 + 25 + 4 + 11 + 6 + 4; // 114
    {
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + req_expected - 1);
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        capsid::NeutralHttpRequest req;
        req.url = "sentinel-url";
        req.method = "PATCH";
        req.credentials = "omit";
        req.redirect = "manual";
        req.has_mode = true;
        req.mode = "no-cors";
        std::string err;
        std::size_t copies = 99;
        require(!capsid::extract_neutral_http_request_meta(
                    ctx, req_info, &req, test_reserve_clone_bytes, &rctx, &err, &copies),
                "Request meta 1-byte-short must reject");
        require(err.find("byte quota") != std::string::npos, "expected byte quota error");
        require(copies == 0 && req.url == "sentinel-url" && req.method == "PATCH" &&
                    req.credentials == "omit" && req.redirect == "manual" && req.has_mode &&
                    req.mode == "no-cors",
                "Request meta must perform ZERO native copies and leave output untouched on "
                "quota reject");
        require(rctx.reserved == 0, "failed Request meta must not retain reservation");
        check_tiers(ledger, kBaseline, "Request meta reject must leave all 3 tiers at baseline");
    }
    // U5: Missing or invalid required/optional Request metadata fails closed with
    // zero copies, untouched output, and zero ledger mutation.
    {
        const char* bad_reqs[] = {
            "({ method: 'GET', credentials: 'same-origin', redirect: 'follow' })",
            "({ url: 'https://u5.test', credentials: 'same-origin', redirect: 'follow' })",
            "({ url: 'https://u5.test', method: 'GET', credentials: 42, redirect: 'follow' })",
            "({ url: 'https://u5.test', method: 'GET', credentials: 'same-origin', redirect: null "
            "})",
            "({ url: 'https://u5.test', method: 'GET', credentials: 'same-origin', "
            "   redirect: 'follow', mode: 123 })",
        };
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + 4096);
        for (std::size_t i = 0; i < sizeof(bad_reqs) / sizeof(bad_reqs[0]); ++i) {
            JSValue bad = rt.eval(bad_reqs[i]);
            IncrementalCloneLedgerCtx rctx;
            rctx.ledger = &ledger;
            capsid::NeutralHttpRequest req;
            req.url = "keep-url";
            req.method = "KEEP";
            req.has_mode = true;
            req.mode = "keep-mode";
            std::string err;
            std::size_t copies = 99;
            require(!capsid::extract_neutral_http_request_meta(
                        ctx, bad, &req, test_reserve_clone_bytes, &rctx, &err, &copies),
                    "invalid Request metadata must fail closed");
            require(!err.empty() && copies == 0 && rctx.reserved == 0 && req.url == "keep-url" &&
                        req.method == "KEEP" && req.has_mode && req.mode == "keep-mode",
                    "invalid Request metadata must leave output and ledger untouched");
            check_tiers(ledger, kBaseline, "invalid Request metadata must not touch ledger");
            JS_FreeValue(ctx, bad);
        }
        // Valid omitted/null optional `mode` clears stale mode fields cleanly.
        JSValue no_mode = rt.eval("({ url: 'https://u5.test/ok', method: 'GET', "
                                  "   credentials: 'same-origin', redirect: 'follow', mode: null "
                                  "})");
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        capsid::NeutralHttpRequest req;
        req.has_mode = true;
        req.mode = "stale";
        std::string err;
        std::size_t copies = 0;
        require(capsid::extract_neutral_http_request_meta(
                    ctx, no_mode, &req, test_reserve_clone_bytes, &rctx, &err, &copies),
                ("Request meta with null mode rejected: " + err).c_str());
        require(copies == 4 && !req.has_mode && req.mode.empty() &&
                    req.url == "https://u5.test/ok" &&
                    rctx.reserved == capsid::estimate_neutral_request_bytes(req),
                "Request meta null mode mismatch");
        ledger.release_bytes(1, "svc", rctx.reserved);
        check_tiers(ledger, kBaseline, "Request meta null mode release restores baseline");
        JS_FreeValue(ctx, no_mode);
    }
    {
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + req_expected);
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        capsid::NeutralHttpRequest req;
        std::string err;
        std::size_t copies = 0;
        require(capsid::extract_neutral_http_request_meta(
                    ctx, req_info, &req, test_reserve_clone_bytes, &rctx, &err, &copies),
                ("Request meta exact-fit rejected: " + err).c_str());
        require(copies == 5 && req.url == "https://t1.test/path/item" && req.method == "POST" &&
                    req.has_mode && req.mode == "cors",
                "Request meta exact-fit values mismatch");
        require(rctx.reserved == req_expected &&
                    capsid::estimate_neutral_request_bytes(req) == req_expected,
                "Request meta reserved bytes mismatch");
        check_tiers(ledger, kBaseline + req_expected, "Request meta exact-fit tier accounting");
        ledger.release_bytes(1, "svc", rctx.reserved);
        check_tiers(ledger, kBaseline, "Request meta release restores baseline");
    }
    JS_FreeValue(ctx, req_info);

    // 2. HTTP Response metadata (64 + statusText + url + type).
    JSValue res_info = rt.eval("({ status: 201, redirected: true, statusText: 'Created', "
                               "   url: 'https://t1.test/res', type: 'default' })");
    const std::size_t res_expected = 64 + 7 + 19 + 7; // 97
    {
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + res_expected - 1);
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        capsid::NeutralHttpResponse res;
        res.status = 418;
        res.redirected = true;
        res.status_text = "sentinel-text";
        res.url = "sentinel-url";
        res.type = "sentinel-type";
        std::string err;
        std::size_t copies = 99;
        require(!capsid::extract_neutral_http_response_meta(
                    ctx, res_info, &res, test_reserve_clone_bytes, &rctx, &err, &copies),
                "Response meta 1-byte-short must reject");
        require(err.find("byte quota") != std::string::npos, "expected byte quota error");
        require(copies == 0 && res.status_text == "sentinel-text" && res.url == "sentinel-url" &&
                    res.type == "sentinel-type" && res.status == 418 && res.redirected,
                "Response meta must perform ZERO native copies and leave output untouched on "
                "quota reject");
        check_tiers(ledger, kBaseline, "Response meta reject must leave all 3 tiers at baseline");
    }
    // U5: Missing or invalid required Response metadata fails closed cleanly.
    {
        const char* bad_res[] = {
            "({ redirected: false, statusText: 'OK', url: '', type: 'default' })",
            "({ status: 99, redirected: false, statusText: 'OK', url: '', type: 'default' })",
            "({ status: 600, redirected: false, statusText: 'OK', url: '', type: 'default' })",
            "({ status: '200', redirected: false, statusText: 'OK', url: '', type: 'default' })",
            "({ status: 200, redirected: 1, statusText: 'OK', url: '', type: 'default' })",
            "({ status: 200, redirected: false, url: '', type: 'default' })",
            "({ status: 200, redirected: false, statusText: 'OK', type: 'default' })",
            "({ status: 200, redirected: false, statusText: 'OK', url: '' })",
        };
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + 4096);
        for (std::size_t i = 0; i < sizeof(bad_res) / sizeof(bad_res[0]); ++i) {
            JSValue bad = rt.eval(bad_res[i]);
            IncrementalCloneLedgerCtx rctx;
            rctx.ledger = &ledger;
            capsid::NeutralHttpResponse res;
            res.status = 418;
            res.url = "keep-res-url";
            std::string err;
            std::size_t copies = 99;
            require(!capsid::extract_neutral_http_response_meta(
                        ctx, bad, &res, test_reserve_clone_bytes, &rctx, &err, &copies),
                    "invalid Response metadata must fail closed");
            require(!err.empty() && copies == 0 && rctx.reserved == 0 && res.status == 418 &&
                        res.url == "keep-res-url",
                    "invalid Response metadata must leave output and ledger untouched");
            check_tiers(ledger, kBaseline, "invalid Response metadata must not touch ledger");
            JS_FreeValue(ctx, bad);
        }
        // Valid status 0 (Response.error() network error response) succeeds.
        JSValue err_res = rt.eval("({ status: 0, redirected: false, statusText: '', "
                                  "   url: '', type: 'error' })");
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        capsid::NeutralHttpResponse res;
        std::string err;
        std::size_t copies = 0;
        require(capsid::extract_neutral_http_response_meta(
                    ctx, err_res, &res, test_reserve_clone_bytes, &rctx, &err, &copies),
                ("Response.error() status=0 rejected: " + err).c_str());
        require(copies == 3 && res.status == 0 && !res.redirected && res.type == "error",
                "Response.error() metadata mismatch");
        ledger.release_bytes(1, "svc", rctx.reserved);
        check_tiers(ledger, kBaseline, "Response.error() release restores baseline");
        JS_FreeValue(ctx, err_res);
    }
    {
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + res_expected);
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        capsid::NeutralHttpResponse res;
        std::string err;
        std::size_t copies = 0;
        require(capsid::extract_neutral_http_response_meta(
                    ctx, res_info, &res, test_reserve_clone_bytes, &rctx, &err, &copies),
                ("Response meta exact-fit rejected: " + err).c_str());
        require(copies == 3 && res.status == 201 && res.redirected &&
                    res.status_text == "Created" && res.url == "https://t1.test/res" &&
                    res.type == "default",
                "Response meta exact-fit values mismatch");
        require(rctx.reserved == res_expected &&
                    capsid::estimate_neutral_response_bytes(res) == res_expected,
                "Response meta reserved bytes mismatch");
        check_tiers(ledger, kBaseline + res_expected, "Response meta exact-fit tier accounting");
        ledger.release_bytes(1, "svc", rctx.reserved);
        check_tiers(ledger, kBaseline, "Response meta release restores baseline");
    }
    JS_FreeValue(ctx, res_info);

    // 3. Header pairs (U3: transactional 2-pass reserve-before-copy; zero copies
    // and untouched output whenever any pair fails validation or quota).
    JSValue hdrs_js = rt.eval("[['x-one', 'alpha'], ['x-two', 'beta-long']]");
    const std::size_t h0_bytes = 5 + 5 + 16; // 26
    const std::size_t h1_bytes = 5 + 9 + 16; // 30
    {
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + h0_bytes - 1);
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        std::vector<std::pair<std::string, std::string>> pairs;
        pairs.push_back(std::make_pair("sentinel-k", "sentinel-v"));
        std::string err;
        std::size_t copies = 99;
        require(!capsid::extract_neutral_header_pairs(
                    ctx, hdrs_js, &pairs, test_reserve_clone_bytes, &rctx, &err, &copies),
                "Header 0 1-byte-short must reject");
        require(copies == 0 && pairs.size() == 1 && pairs[0].first == "sentinel-k" &&
                    pairs[0].second == "sentinel-v" && rctx.reserved == 0,
                "Header 0 reject must perform ZERO copies and leave output untouched");
        check_tiers(ledger, kBaseline, "Header 0 reject leaves all 3 tiers at baseline");
    }
    {
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + h0_bytes + h1_bytes - 1);
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        std::vector<std::pair<std::string, std::string>> pairs;
        pairs.push_back(std::make_pair("sentinel-k", "sentinel-v"));
        std::string err;
        std::size_t copies = 99;
        require(!capsid::extract_neutral_header_pairs(
                    ctx, hdrs_js, &pairs, test_reserve_clone_bytes, &rctx, &err, &copies),
                "Header block 1-byte-short must reject transactionally");
        require(copies == 0 && pairs.size() == 1 && pairs[0].first == "sentinel-k" &&
                    pairs[0].second == "sentinel-v" && rctx.reserved == 0,
                "Header block reject must perform ZERO copies, zero reservation, and leave "
                "output untouched");
        check_tiers(ledger, kBaseline, "Header block reject leaves all 3 tiers at baseline");
    }
    {
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + h0_bytes + h1_bytes);
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        std::vector<std::pair<std::string, std::string>> pairs;
        pairs.push_back(std::make_pair("stale-k", "stale-v"));
        std::string err;
        std::size_t copies = 0;
        require(capsid::extract_neutral_header_pairs(ctx, hdrs_js, &pairs, test_reserve_clone_bytes,
                                                     &rctx, &err, &copies),
                ("Headers exact-fit rejected: " + err).c_str());
        require(copies == 4 && pairs.size() == 2 && pairs[0].first == "x-one" &&
                    pairs[1].first == "x-two" && rctx.reserved == h0_bytes + h1_bytes,
                "Headers exact-fit mismatch");
        check_tiers(ledger, kBaseline + h0_bytes + h1_bytes, "Headers exact-fit tier accounting");
        ledger.release_bytes(1, "svc", rctx.reserved);
        check_tiers(ledger, kBaseline, "Headers release restores baseline");
    }
    JS_FreeValue(ctx, hdrs_js);

    // 4. RpcTarget method table inspection (32 + sum(name.size() + 16)).
    JSValue target_setup = rt.eval("class BaseTarget {};"
                                   "class SampleTarget extends BaseTarget {"
                                   "  alpha() { return 1; }"
                                   "  betaMethod() { return 2; }"
                                   "  dispose() {}"
                                   "}"
                                   "[new SampleTarget(), BaseTarget.prototype, Object.prototype]");
    JSValue target_obj = JS_GetPropertyUint32(ctx, target_setup, 0);
    JSValue base_proto = JS_GetPropertyUint32(ctx, target_setup, 1);
    JSValue obj_proto = JS_GetPropertyUint32(ctx, target_setup, 2);
    JS_FreeValue(ctx, target_setup);
    const std::size_t handle_expected = 32 + (5 + 16) + (10 + 16); // 79
    {
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + handle_expected - 1);
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        std::vector<std::string> methods;
        methods.push_back("keepMethod");
        std::map<std::string, JSValue> method_fns;
        method_fns["keepMethod"] = JS_NewInt32(ctx, 7);
        std::string err;
        std::size_t copies = 99;
        require(!capsid::inspect_rpc_target_methods(ctx, target_obj, base_proto, obj_proto, 128,
                                                    &methods, &method_fns, test_reserve_clone_bytes,
                                                    &rctx, &err, &copies),
                "RpcTarget methods 1-byte-short must reject");
        require(err.find("byte quota") != std::string::npos, "expected byte quota error");
        require(copies == 0 && methods.size() == 1 && methods[0] == "keepMethod" &&
                    method_fns.size() == 1 && rctx.reserved == 0,
                "RpcTarget reject must perform ZERO native method-name copies and leave outputs "
                "untouched");
        JS_FreeValue(ctx, method_fns["keepMethod"]);
        method_fns.clear();
        check_tiers(ledger, kBaseline, "RpcTarget reject leaves all 3 tiers at baseline");
    }
    {
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + handle_expected);
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        std::vector<std::string> methods;
        std::map<std::string, JSValue> method_fns;
        std::string err;
        std::size_t copies = 0;
        require(capsid::inspect_rpc_target_methods(ctx, target_obj, base_proto, obj_proto, 128,
                                                   &methods, &method_fns, test_reserve_clone_bytes,
                                                   &rctx, &err, &copies),
                ("RpcTarget methods exact-fit rejected: " + err).c_str());
        require(copies == 2 && methods.size() == 2 && method_fns.size() == 2 &&
                    rctx.reserved == handle_expected,
                "RpcTarget exact-fit mismatch");
        check_tiers(ledger, kBaseline + handle_expected, "RpcTarget exact-fit tier accounting");
        for (std::map<std::string, JSValue>::iterator mit = method_fns.begin();
             mit != method_fns.end(); ++mit) {
            JS_FreeValue(ctx, mit->second);
        }
        method_fns.clear();
        ledger.release_bytes(1, "svc", rctx.reserved);
        check_tiers(ledger, kBaseline, "RpcTarget release restores baseline");
    }
    // Embedded NUL method name, >1024 property inspection cap, and long accessor
    // name clamping must all fail closed with zero copies/reservations.
    {
        capsid::BindingRpcLedger ledger = make_ledger_at_cap(kBaseline + 1024);
        IncrementalCloneLedgerCtx rctx;
        rctx.ledger = &ledger;
        JSValue nul_method_target = rt.eval("({ ['ping\\u0000x']() { return 1; } })");
        std::vector<std::string> methods;
        std::map<std::string, JSValue> method_fns;
        std::string err;
        std::size_t copies = 99;
        require(!capsid::inspect_rpc_target_methods(ctx, nul_method_target, base_proto, obj_proto,
                                                    128, &methods, &method_fns,
                                                    test_reserve_clone_bytes, &rctx, &err, &copies),
                "RpcTarget NUL-embedded method name must reject");
        require(copies == 0 && rctx.reserved == 0, "invalid method name must reserve/copy zero");
        JS_FreeValue(ctx, nul_method_target);
        check_tiers(ledger, kBaseline, "invalid RpcTarget leaves baseline untouched");

        JSValue bloated_target = rt.eval("(() => {"
                                         "  const o = {};"
                                         "  for (let i = 0; i < 1030; ++i) o['p' + i] = i;"
                                         "  return o;"
                                         "})()");
        copies = 99;
        err.clear();
        require(!capsid::inspect_rpc_target_methods(ctx, bloated_target, base_proto, obj_proto, 128,
                                                    &methods, &method_fns, test_reserve_clone_bytes,
                                                    &rctx, &err, &copies),
                "RpcTarget exceeding property inspection cap must reject");
        require(err.find("inspection limit") != std::string::npos && copies == 0 &&
                    rctx.reserved == 0,
                "bloated RpcTarget must reject at bounded inspection cap with zero copies");
        JS_FreeValue(ctx, bloated_target);
        check_tiers(ledger, kBaseline, "bloated RpcTarget leaves baseline untouched");

        JSValue long_accessor_target =
            rt.eval("(() => {"
                    "  const o = {};"
                    "  Object.defineProperty(o, 'a'.repeat(200), { get() { return 1; } });"
                    "  return o;"
                    "})()");
        copies = 99;
        err.clear();
        require(!capsid::inspect_rpc_target_methods(ctx, long_accessor_target, base_proto,
                                                    obj_proto, 128, &methods, &method_fns,
                                                    test_reserve_clone_bytes, &rctx, &err, &copies),
                "RpcTarget accessor with huge name must reject");
        require(copies == 0 && rctx.reserved == 0 &&
                    err == ("RpcTarget cannot expose accessor property: " + std::string(64, 'a')),
                "huge accessor name error must be clamped and reserve/copy zero");
        JS_FreeValue(ctx, long_accessor_target);
        check_tiers(ledger, kBaseline, "accessor RpcTarget leaves baseline untouched");

        require(capsid::valid_binding_method_name(std::string("okMethod")) &&
                    !capsid::valid_binding_method_name(std::string("__proto__")),
                "valid_binding_method_name string overload mismatch");
    }
    JS_FreeValue(ctx, target_obj);
    JS_FreeValue(ctx, base_proto);
    JS_FreeValue(ctx, obj_proto);
}

struct InterruptProbe {
    bool armed = false;
    int polls = 0;
    int triggers = 0;
};

InterruptProbe* g_interrupt_probe = NULL;

int test_interrupt_handler(JSRuntime*, void* opaque) {
    InterruptProbe* probe = static_cast<InterruptProbe*>(opaque);
    if (probe != NULL) {
        probe->polls++;
        if (probe->armed) {
            probe->triggers++;
            return 1;
        }
    }
    return 0;
}

JSValue js_arm_interrupt(JSContext*, JSValueConst, int, JSValueConst*) {
    if (g_interrupt_probe != NULL) {
        g_interrupt_probe->armed = true;
    }
    return JS_UNDEFINED;
}

JSValue js_poll_count(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    return JS_NewInt32(ctx, g_interrupt_probe != NULL ? g_interrupt_probe->polls : 0);
}

// U2 (Round-6/7/8 P2): Single-store atomic HTTP inline-body consumption
// transaction (`BindingHttpConsumeBatch`). Verifies in BOTH input
// (User->Binding) and result (Binding->User) directions:
//   1. Preparation failure on item 2 of 2 leaves 0/2 consumed (returns false).
//   2. Post-prepare rollback (`reset()`) mutates nothing (0/2 consumed).
//   3. Invalid/unwritable commit tokens reject at prepare(); freezing the token
//      before commit() fails closed at publish with 0/2 consumed and 0 apply calls.
//   4. Real QuickJS interrupt firing DURING prepare() after linking item 0 and
//      before linking item 1 leaves tx.committed === false (0/2 consumed).
//   5. Real QuickJS interrupt firing MID-LOOP DURING apply_fn after item 0
//      flushed and before item 1 flushed still transitions ALL 2/2 objects
//      atomically (single-store tx.committed === true already published).
//   6. Happy-path 2-object (normal + frozen) commit transitions 2/2 atomically.
void test_http_consume_batch_two_phase_atomicity() {
    RuntimeFixture user_rt;
    RuntimeFixture binding_rt;

    auto run_direction_checks = [](RuntimeFixture& rt, const char* dir_label) {
        JSContext* ctx = rt.ctx();
        JSRuntime* js_rt = JS_GetRuntime(ctx);
        InterruptProbe probe;
        g_interrupt_probe = &probe;

        JSValue arm_fn = JS_NewCFunction(ctx, js_arm_interrupt, "__testArmInterrupt", 0);
        JSValue global = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global, "__testArmInterrupt", arm_fn);
        JS_FreeValue(ctx, global);

        JSValue harness = rt.eval(
            "(() => {\n"
            "  const slots = new WeakMap();\n"
            "  const canWrite = (obj, key) => {\n"
            "    const desc = Object.getOwnPropertyDescriptor(obj, key);\n"
            "    return desc !== undefined && !Object.hasOwn(desc, 'get') &&\n"
            "           !Object.hasOwn(desc, 'set') && desc.writable === true;\n"
            "  };\n"
            "  const safeSet = (obj, key, val) => {\n"
            "    if (canWrite(obj, key)) obj[key] = val;\n"
            "  };\n"
            "  const syncSlotIfCommitted = (s, target) => {\n"
            "    const tx = s.pendingCommit;\n"
            "    if (tx === null || tx.committed !== true) return;\n"
            "    const stream = s.pendingConsumedStream;\n"
            "    if (stream !== null) {\n"
            "      s.bodyFastKind = 0;\n"
            "      s.bodyFastData = null;\n"
            "      s.bodyStream = stream;\n"
            "    }\n"
            "    s.bodyConsumed = true;\n"
            "    if (target !== undefined) {\n"
            "      if (stream !== null) {\n"
            "        safeSet(target, '_bodyFastKind', 0);\n"
            "        safeSet(target, '_bodyFastData', null);\n"
            "        safeSet(target, '_bodyStream', stream);\n"
            "      }\n"
            "      safeSet(target, '_bodyConsumed', true);\n"
            "    }\n"
            "    s.pendingCommit = null;\n"
            "    s.pendingConsumedStream = null;\n"
            "  };\n"
            "  const makeSource = (payload, freezeObj) => {\n"
            "    const obj = { _bodyFastKind: 1, _bodyFastData: payload, _bodyStream: null,\n"
            "                  _bodyConsumed: false };\n"
            "    slots.set(obj, {\n"
            "      bodyFastKind: 1, bodyFastData: payload, bodyStream: null,\n"
            "      bodyConsumed: false, pendingCommit: null, pendingConsumedStream: null\n"
            "    });\n"
            "    if (freezeObj) Object.freeze(obj);\n"
            "    return obj;\n"
            "  };\n"
            "  const readState = (obj) => {\n"
            "    const s = slots.get(obj);\n"
            "    syncSlotIfCommitted(s, obj);\n"
            "    return { used: s.bodyConsumed, data: s.bodyFastData,\n"
            "             locked: s.bodyStream !== null && s.bodyStream.locked === true,\n"
            "             pubUsed: obj._bodyConsumed };\n"
            "  };\n"
            "  let failOnIndex = -1;\n"
            "  let armPrepareLinkIndex = -1;\n"
            "  let armApplyIndex = -1;\n"
            "  let badTxMode = 0;\n"
            "  let lastTx = null;\n"
            "  let applyCalls = 0;\n"
            "  let applyFlushedCount = 0;\n"
            "  const spinForInterrupt = () => {\n"
            "    __testArmInterrupt();\n"
            "    let acc = 0;\n"
            "    for (let k = 0; k < 50000; ++k) { acc += k; }\n"
            "    return acc;\n"
            "  };\n"
            "  const preparePlan = (targets) => {\n"
            "    const tx = Object.create(null);\n"
            "    if (badTxMode === 1) {\n"
            "      tx.committed = true;\n"
            "    } else if (badTxMode === 2) {\n"
            "      Object.defineProperty(tx, 'committed', { value: false, writable: false });\n"
            "    } else if (badTxMode === 3) {\n"
            "      Object.defineProperty(tx, 'committed', { get() { return false; } });\n"
            "    } else {\n"
            "      tx.committed = false;\n"
            "    }\n"
            "    lastTx = tx;\n"
            "    const count = targets.length;\n"
            "    const planSlots = Object.create(null);\n"
            "    const streams = Object.create(null);\n"
            "    for (let i = 0; i < count; ++i) {\n"
            "      if (i === failOnIndex) {\n"
            "        throw new RangeError('simulated OOM during terminal stream allocation');\n"
            "      }\n"
            "      const s = slots.get(targets[i]);\n"
            "      if (!s) throw new TypeError('missing slot');\n"
            "      syncSlotIfCommitted(s, targets[i]);\n"
            "      planSlots[i] = s;\n"
            "      streams[i] = { closed: true, disturbed: true, locked: true };\n"
            "    }\n"
            "    for (let i = 0; i < count; ++i) {\n"
            "      const s = planSlots[i];\n"
            "      s.pendingConsumedStream = streams[i];\n"
            "      s.pendingCommit = tx;\n"
            "      if (i === armPrepareLinkIndex) {\n"
            "        spinForInterrupt();\n"
            "      }\n"
            "    }\n"
            "    const plan = Object.create(null);\n"
            "    plan.tx = badTxMode === 4 ? null : tx;\n"
            "    plan.count = count;\n"
            "    plan.targets = targets;\n"
            "    plan.slots = planSlots;\n"
            "    return plan;\n"
            "  };\n"
            "  const applyPlan = (plan) => {\n"
            "    applyCalls++;\n"
            "    applyFlushedCount = 0;\n"
            "    for (let i = 0; i < plan.count; ++i) {\n"
            "      syncSlotIfCommitted(plan.slots[i], plan.targets[i]);\n"
            "      applyFlushedCount++;\n"
            "      if (i === armApplyIndex) {\n"
            "        spinForInterrupt();\n"
            "      }\n"
            "    }\n"
            "  };\n"
            "  const configure = (failIdx, prepArm, applyArm, txMode) => {\n"
            "    failOnIndex = failIdx;\n"
            "    armPrepareLinkIndex = prepArm;\n"
            "    armApplyIndex = applyArm;\n"
            "    badTxMode = txMode;\n"
            "  };\n"
            "  const freezeLastTx = () => { if (lastTx) Object.freeze(lastTx); };\n"
            "  const stats = () => ({ applyCalls, applyFlushedCount });\n"
            "  return { makeSource, readState, preparePlan, applyPlan, configure,\n"
            "           freezeLastTx, stats };\n"
            "})()");
        require(!JS_IsException(harness) && JS_IsObject(harness), "atomicity harness init");
        JSValue make_source = JS_GetPropertyStr(ctx, harness, "makeSource");
        JSValue read_state = JS_GetPropertyStr(ctx, harness, "readState");
        JSValue prepare_fn = JS_GetPropertyStr(ctx, harness, "preparePlan");
        JSValue apply_fn = JS_GetPropertyStr(ctx, harness, "applyPlan");
        JSValue configure_fn = JS_GetPropertyStr(ctx, harness, "configure");
        JSValue freeze_last_tx_fn = JS_GetPropertyStr(ctx, harness, "freezeLastTx");
        JSValue stats_fn = JS_GetPropertyStr(ctx, harness, "stats");

        auto make_src = [&](const char* text, bool freeze_obj) -> JSValue {
            JSValue args[2] = {JS_NewString(ctx, text), freeze_obj ? JS_TRUE : JS_FALSE};
            JSValue out = JS_Call(ctx, make_source, JS_UNDEFINED, 2, args);
            JS_FreeValue(ctx, args[0]);
            JS_FreeValue(ctx, args[1]);
            require(!JS_IsException(out) && JS_IsObject(out), "makeSource failed");
            return out;
        };
        auto check_unread = [&](JSValueConst src, const char* expected_text, const char* msg) {
            JSValue arg = JS_DupValue(ctx, src);
            JSValue st = JS_Call(ctx, read_state, JS_UNDEFINED, 1, &arg);
            JS_FreeValue(ctx, arg);
            JSValue used_v = JS_GetPropertyStr(ctx, st, "used");
            JSValue data_v = JS_GetPropertyStr(ctx, st, "data");
            JSValue locked_v = JS_GetPropertyStr(ctx, st, "locked");
            const bool used = JS_ToBool(ctx, used_v) != 0;
            const bool locked = JS_ToBool(ctx, locked_v) != 0;
            const char* ds = JS_ToCString(ctx, data_v);
            const bool ok = !used && !locked && ds != NULL && std::strcmp(ds, expected_text) == 0;
            if (ds != NULL)
                JS_FreeCString(ctx, ds);
            JS_FreeValue(ctx, used_v);
            JS_FreeValue(ctx, data_v);
            JS_FreeValue(ctx, locked_v);
            JS_FreeValue(ctx, st);
            require(ok, (std::string(dir_label) + ": " + msg).c_str());
        };
        auto check_consumed = [&](JSValueConst src, bool expect_pub_used, const char* msg) {
            JSValue arg = JS_DupValue(ctx, src);
            JSValue st = JS_Call(ctx, read_state, JS_UNDEFINED, 1, &arg);
            JS_FreeValue(ctx, arg);
            JSValue used_v = JS_GetPropertyStr(ctx, st, "used");
            JSValue data_v = JS_GetPropertyStr(ctx, st, "data");
            JSValue locked_v = JS_GetPropertyStr(ctx, st, "locked");
            JSValue pub_v = JS_GetPropertyStr(ctx, st, "pubUsed");
            const bool ok = (JS_ToBool(ctx, used_v) != 0) && JS_IsNull(data_v) &&
                            (JS_ToBool(ctx, locked_v) != 0) &&
                            ((JS_ToBool(ctx, pub_v) != 0) == expect_pub_used);
            JS_FreeValue(ctx, used_v);
            JS_FreeValue(ctx, data_v);
            JS_FreeValue(ctx, locked_v);
            JS_FreeValue(ctx, pub_v);
            JS_FreeValue(ctx, st);
            require(ok, (std::string(dir_label) + ": " + msg).c_str());
        };
        auto configure = [&](int fail_idx, int prep_arm, int apply_arm, int tx_mode) {
            JSValue args[4] = {JS_NewInt32(ctx, fail_idx), JS_NewInt32(ctx, prep_arm),
                               JS_NewInt32(ctx, apply_arm), JS_NewInt32(ctx, tx_mode)};
            JSValue r = JS_Call(ctx, configure_fn, JS_UNDEFINED, 4, args);
            for (int i = 0; i < 4; ++i)
                JS_FreeValue(ctx, args[i]);
            JS_FreeValue(ctx, r);
        };
        auto get_stat = [&](const char* key) -> int32_t {
            JSValue st = JS_Call(ctx, stats_fn, JS_UNDEFINED, 0, NULL);
            JSValue v = JS_GetPropertyStr(ctx, st, key);
            int32_t out = 0;
            JS_ToInt32(ctx, &out, v);
            JS_FreeValue(ctx, v);
            JS_FreeValue(ctx, st);
            return out;
        };

        // 1. Controllable preparation failure on object #2 of 2: ZERO partial
        // consumption, caller receives false + error (no false success).
        {
            JSValue s1 = make_src("body-one", false);
            JSValue s2 = make_src("body-two", true);
            capsid::BindingHttpConsumeBatch batch;
            require(batch.add_target(ctx, s1) && batch.add_target(ctx, s2), "add_target failed");
            require(batch.contains(s1) && batch.contains(s2), "contains mismatch");
            configure(1, -1, -1, 0);
            std::string err;
            require(
                !batch.prepare(prepare_fn, &err),
                (std::string(dir_label) + ": prepare failure on item 2 must fail closed").c_str());
            require(!err.empty(),
                    (std::string(dir_label) + ": prepare error message missing").c_str());
            std::string commit_err;
            require(!batch.commit(apply_fn, &commit_err) && !commit_err.empty(),
                    (std::string(dir_label) + ": commit after failed prepare must reject").c_str());
            check_unread(s1, "body-one", "item 1 must stay unread when item 2 prepare fails");
            check_unread(s2, "body-two", "item 2 must stay unread when item 2 prepare fails");
            JS_FreeValue(ctx, s1);
            JS_FreeValue(ctx, s2);
        }

        // 2. Prepare succeeds for 2 objects, then simulated downstream failure
        // resets batch BEFORE commit: proves Phase 1 leaves tx.committed=false.
        {
            JSValue s1 = make_src("mat-one", false);
            JSValue s2 = make_src("mat-two", true);
            capsid::BindingHttpConsumeBatch batch;
            require(batch.add_target(ctx, s1) && batch.add_target(ctx, s2), "add_target failed");
            configure(-1, -1, -1, 0);
            std::string err;
            require(batch.prepare(prepare_fn, &err),
                    (std::string(dir_label) + ": prepare unexpectedly failed: " + err).c_str());
            check_unread(s1, "mat-one", "prepare must not mutate item 1 before commit");
            check_unread(s2, "mat-two", "prepare must not mutate item 2 before commit");
            batch.reset();
            require(batch.empty() && !batch.contains(s1), "reset must clear targets");
            check_unread(s1, "mat-one", "reset after prepare must keep item 1 unread");
            check_unread(s2, "mat-two", "reset after prepare must keep item 2 unread");
            JS_FreeValue(ctx, s1);
            JS_FreeValue(ctx, s2);
        }

        // 3. Invalid commit tokens (already true, non-writable, getter, missing)
        // reject at prepare(); freezing the token after prepare() fails closed
        // at single-store publish with 0/2 consumed and zero apply calls.
        for (int bad_mode = 1; bad_mode <= 4; ++bad_mode) {
            JSValue s1 = make_src("badtx-one", false);
            JSValue s2 = make_src("badtx-two", false);
            capsid::BindingHttpConsumeBatch batch;
            require(batch.add_target(ctx, s1) && batch.add_target(ctx, s2), "add_target failed");
            configure(-1, -1, -1, bad_mode);
            std::string err;
            require(!batch.prepare(prepare_fn, &err) && !err.empty(),
                    (std::string(dir_label) + ": bad tx mode must reject at prepare").c_str());
            if (bad_mode != 1) {
                check_unread(s1, "badtx-one", "rejected bad tx left item 1 unread");
                check_unread(s2, "badtx-two", "rejected bad tx left item 2 unread");
            }
            JS_FreeValue(ctx, s1);
            JS_FreeValue(ctx, s2);
        }
        {
            JSValue s1 = make_src("pubfail-one", false);
            JSValue s2 = make_src("pubfail-two", false);
            capsid::BindingHttpConsumeBatch batch;
            require(batch.add_target(ctx, s1) && batch.add_target(ctx, s2), "add_target failed");
            configure(-1, -1, -1, 0);
            std::string err;
            require(batch.prepare(prepare_fn, &err), "prepare before frozen tx failed");
            JSValue fr = JS_Call(ctx, freeze_last_tx_fn, JS_UNDEFINED, 0, NULL);
            JS_FreeValue(ctx, fr);
            const int32_t before_apply = get_stat("applyCalls");
            require(
                !batch.commit(apply_fn, &err) && !err.empty(),
                (std::string(dir_label) + ": frozen tx publish failure must return false").c_str());
            require(get_stat("applyCalls") == before_apply,
                    (std::string(dir_label) + ": publish failure must not run apply_fn").c_str());
            check_unread(s1, "pubfail-one", "publish failure must leave item 1 unread");
            check_unread(s2, "pubfail-two", "publish failure must leave item 2 unread");
            JS_FreeValue(ctx, s1);
            JS_FreeValue(ctx, s2);
        }

        // 4. Real QuickJS interrupt firing DURING prepare() after linking
        // item 0 and BEFORE linking item 1: returns false, 0/2 consumed.
        {
            JSValue s1 = make_src("prepirq-one", false);
            JSValue s2 = make_src("prepirq-two", false);
            capsid::BindingHttpConsumeBatch batch;
            require(batch.add_target(ctx, s1) && batch.add_target(ctx, s2), "add_target failed");
            probe.armed = false;
            probe.triggers = 0;
            JS_SetInterruptHandler(js_rt, test_interrupt_handler, &probe);
            configure(-1, 0, -1, 0);
            std::string err;
            const bool prepared = batch.prepare(prepare_fn, &err);
            probe.armed = false;
            JS_SetInterruptHandler(js_rt, NULL, NULL);
            require(!prepared && !err.empty(),
                    (std::string(dir_label) + ": interrupted prepare must return false").c_str());
            require(probe.triggers > 0,
                    (std::string(dir_label) + ": real QuickJS interrupt did not fire in prepare")
                        .c_str());
            check_unread(s1, "prepirq-one", "interrupted prepare must leave item 1 unread");
            check_unread(s2, "prepirq-two", "interrupted prepare must leave item 2 unread");
            std::string commit_err;
            require(!batch.commit(apply_fn, &commit_err),
                    (std::string(dir_label) + ": commit after interrupted prepare must reject")
                        .c_str());
            JS_FreeValue(ctx, s1);
            JS_FreeValue(ctx, s2);
        }

        // 5. Real QuickJS interrupt firing MID-LOOP DURING apply_fn AFTER
        // item 0 flushed and BEFORE item 1 flushed: because C++ already
        // published tx.committed = true in one uninterruptible store, commit()
        // succeeds and BOTH items observe full committed state (ZERO torn state).
        {
            JSValue s1 = make_src("applyirq-one", false);
            JSValue s2 = make_src("applyirq-two", false);
            capsid::BindingHttpConsumeBatch batch;
            require(batch.add_target(ctx, s1) && batch.add_target(ctx, s2), "add_target failed");
            configure(-1, -1, 0, 0);
            std::string err;
            require(batch.prepare(prepare_fn, &err), "prepare before apply interrupt failed");
            probe.armed = false;
            probe.triggers = 0;
            JS_SetInterruptHandler(js_rt, test_interrupt_handler, &probe);
            const bool committed = batch.commit(apply_fn, &err);
            probe.armed = false;
            JS_SetInterruptHandler(js_rt, NULL, NULL);
            require(committed, (std::string(dir_label) +
                                ": commit must succeed after single-store publish even if eager "
                                "apply flush is interrupted: " +
                                err)
                                   .c_str());
            require(probe.triggers > 0,
                    (std::string(dir_label) + ": real QuickJS interrupt did not fire in apply")
                        .c_str());
            require(
                get_stat("applyFlushedCount") == 1,
                (std::string(dir_label) + ": apply loop should have stopped after item 0").c_str());
            check_consumed(s1, true, "item 1 must be consumed after mid-apply interrupt");
            check_consumed(s2, true,
                           "item 2 must ALSO observe committed state after mid-apply interrupt");
            JS_FreeValue(ctx, s1);
            JS_FreeValue(ctx, s2);
        }

        // 6. Happy path: 2 objects (one normal, one frozen) prepare + commit
        // atomically together.
        {
            JSValue s1 = make_src("ok-one", false);
            JSValue s2 = make_src("ok-two", true);
            capsid::BindingHttpConsumeBatch batch;
            require(batch.add_target(ctx, s1) && batch.add_target(ctx, s2), "add_target failed");
            configure(-1, -1, -1, 0);
            std::string err;
            require(batch.prepare(prepare_fn, &err), ("happy-path prepare failed: " + err).c_str());
            require(batch.commit(apply_fn, &err), ("happy-path commit failed: " + err).c_str());
            require(batch.empty(), "commit must reset batch");
            check_consumed(s1, true, "normal item 1 must be consumed after commit");
            check_consumed(s2, false, "frozen item 2 private slot must be consumed after commit");
            JS_FreeValue(ctx, s1);
            JS_FreeValue(ctx, s2);
        }

        JS_FreeValue(ctx, make_source);
        JS_FreeValue(ctx, read_state);
        JS_FreeValue(ctx, prepare_fn);
        JS_FreeValue(ctx, apply_fn);
        JS_FreeValue(ctx, configure_fn);
        JS_FreeValue(ctx, freeze_last_tx_fn);
        JS_FreeValue(ctx, stats_fn);
        JS_FreeValue(ctx, harness);
        g_interrupt_probe = NULL;
    };

    run_direction_checks(user_rt, "input(User->Binding)");
    run_direction_checks(binding_rt, "result(Binding->User)");
}

// Round-8 P2 #1 & #2: Direct production JS (`capsid__bootstrap` = unmodified
// `js/fetch.js` + `js/streams.js` + `js/binding-rpc-bridge.js`) + production
// C++ `capsid::BindingHttpConsumeBatch` verification in BOTH input
// (User->Binding) and result (Binding->User) directions.
class ProductionFetchFixture {
  public:
    ProductionFetchFixture() {
        TJSRunOptions options;
        TJS_DefaultOptions(&options);
        options.skip_run_main = true;
        options.bootstrap = production_bootstrap;
        options.bootstrap_opaque = this;
        runtime_ = TJS_NewRuntimeOptions(&options);
        require(runtime_ != NULL, "production fetch runtime creation failed");
        ctx_ = TJS_GetJSContext(runtime_);
        require(JS_IsObject(bridge_), "production binding bridge was not installed");
    }

    ~ProductionFetchFixture() {
        if (ctx_ != NULL) {
            JS_FreeValue(ctx_, bridge_);
        }
        if (runtime_ != NULL) {
            TJS_FreeRuntime(runtime_);
        }
    }

    JSContext* ctx() const { return ctx_; }
    JSValueConst bridge() const { return bridge_; }

    void drain_jobs() const {
        JSRuntime* rt = JS_GetRuntime(ctx_);
        JSContext* job_ctx = NULL;
        for (int guard = 0; guard < 1024; ++guard) {
            const int rc = JS_ExecutePendingJob(rt, &job_ctx);
            if (rc == 0) {
                break;
            }
            if (rc < 0) {
                JSValue exc = JS_GetException(job_ctx != NULL ? job_ctx : ctx_);
                const char* text = JS_ToCString(ctx_, exc);
                std::string msg = text ? text : "?";
                if (text != NULL)
                    JS_FreeCString(ctx_, text);
                JS_FreeValue(ctx_, exc);
                fail("production fetch microtask threw: " + msg);
            }
        }
    }

    JSValue eval(const std::string& source) const {
        JSValue result =
            JS_Eval(ctx_, source.c_str(), source.size(), "<prod-test>", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(ctx_);
            const char* text = JS_ToCString(ctx_, exc);
            std::string msg = text ? text : "?";
            if (text != NULL)
                JS_FreeCString(ctx_, text);
            JS_FreeValue(ctx_, exc);
            fail("production fetch JS exception: " + msg);
        }
        return result;
    }

  private:
    static JSValue js_noop(JSContext*, JSValueConst, int, JSValueConst*) { return JS_UNDEFINED; }
    static JSValue js_req_limit(JSContext* ctx, JSValueConst, int, JSValueConst*) {
        return JS_NewInt64(ctx, 1024 * 1024);
    }
    static JSValue js_res_limit(JSContext* ctx, JSValueConst, int, JSValueConst*) {
        return JS_NewInt64(ctx, 4 * 1024 * 1024);
    }
    static JSValue js_channel_limits(JSContext* ctx, JSValueConst, int, JSValueConst*) {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "message", JS_NewInt64(ctx, 1024 * 1024));
        JS_SetPropertyStr(ctx, obj, "queue", JS_NewInt64(ctx, 4 * 1024 * 1024));
        JS_SetPropertyStr(ctx, obj, "send", JS_NewInt64(ctx, 1024 * 1024));
        return obj;
    }
    static JSValue js_observe_promise(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
        return argc > 0 ? JS_DupValue(ctx, argv[0]) : JS_UNDEFINED;
    }
    static JSValue js_buffer_alias(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
        return JS_NewArrayBufferAlias(ctx, argc > 0 ? argv[0] : JS_UNDEFINED);
    }
    static JSValue js_install_binding_bridge(JSContext* ctx, JSValueConst, int argc,
                                             JSValueConst* argv) {
        if (g_installing_fixture_ != NULL && argc >= 1 && JS_IsObject(argv[0])) {
            JS_FreeValue(ctx, g_installing_fixture_->bridge_);
            g_installing_fixture_->bridge_ = JS_DupValue(ctx, argv[0]);
        }
        return JS_UNDEFINED;
    }
    static bool set_fn(JSContext* ctx, JSValueConst obj, const char* name, JSCFunction* fn,
                       int length) {
        return JS_SetPropertyStr(ctx, obj, name, JS_NewCFunction(ctx, fn, name, length)) >= 0;
    }

    static int production_bootstrap(TJSRuntime* runtime, JSContext* ctx, void* opaque) {
        ProductionFetchFixture* self = static_cast<ProductionFetchFixture*>(opaque);
        g_installing_fixture_ = self;
        JSValue core = TJS_GetInternalCore(runtime);
        if (!set_fn(ctx, core, "capsidLog", js_noop, 2) ||
            !set_fn(ctx, core, "capsidFetchRequestBodyLimit", js_req_limit, 0) ||
            !set_fn(ctx, core, "capsidFetchResponseBodyLimit", js_res_limit, 0) ||
            !set_fn(ctx, core, "capsidFixedResponseBodyLimit", js_res_limit, 0) ||
            !set_fn(ctx, core, "capsidChannelLimits", js_channel_limits, 0) ||
            !set_fn(ctx, core, "capsidStreamObservePromise", js_observe_promise, 1) ||
            !set_fn(ctx, core, "capsidStreamBufferAlias", js_buffer_alias, 1) ||
            !set_fn(ctx, core, "capsidInstallMicrotaskReporter", js_noop, 1) ||
            !set_fn(ctx, core, "capsidInstallBridge", js_noop, 4) ||
            !set_fn(ctx, core, "capsidInstallBindingBridge", js_install_binding_bridge, 1) ||
            JS_DefinePropertyValueStr(ctx, core, "capsidEnableStreamFencing", JS_TRUE,
                                      JS_PROP_CONFIGURABLE) < 0) {
            g_installing_fixture_ = NULL;
            return -1;
        }
        const int rc = TJS_EvalBytecode(ctx, capsid__bootstrap, capsid__bootstrap_size, true);
        JSAtom hook_atom = JS_NewAtom(ctx, "capsidInstallBindingBridge");
        JS_DeleteProperty(ctx, core, hook_atom, 0);
        JS_FreeAtom(ctx, hook_atom);
        JSAtom fence_atom = JS_NewAtom(ctx, "capsidEnableStreamFencing");
        JS_DeleteProperty(ctx, core, fence_atom, 0);
        JS_FreeAtom(ctx, fence_atom);
        g_installing_fixture_ = NULL;
        return rc;
    }

    static ProductionFetchFixture* g_installing_fixture_;
    TJSRuntime* runtime_ = NULL;
    JSContext* ctx_ = NULL;
    JSValue bridge_ = JS_UNDEFINED;
};

ProductionFetchFixture* ProductionFetchFixture::g_installing_fixture_ = NULL;

void test_production_fetch_atomic_commit_sync_and_freeze() {
    ProductionFetchFixture user_rt;
    ProductionFetchFixture binding_rt;

    auto run_prod_direction = [](ProductionFetchFixture& rt, const char* dir_label) {
        JSContext* ctx = rt.ctx();
        JSRuntime* js_rt = JS_GetRuntime(ctx);
        InterruptProbe probe;
        g_interrupt_probe = &probe;

        JSValue global = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, global, "__testPollCount",
                          JS_NewCFunction(ctx, js_poll_count, "__testPollCount", 0));
        JS_SetPropertyStr(ctx, global, "__testBridge", JS_DupValue(ctx, rt.bridge()));
        JS_FreeValue(ctx, global);

        JSValue prepare_fn = JS_GetPropertyStr(ctx, rt.bridge(), "prepareHttpBodyConsumePlan");
        JSValue apply_fn = JS_GetPropertyStr(ctx, rt.bridge(), "applyHttpBodyConsumePlan");
        require(JS_IsFunction(ctx, prepare_fn) && JS_IsFunction(ctx, apply_fn),
                "production bridge missing prepare/apply functions");

        const std::string helper_src =
            std::string(
                "(() => {\n"
                "  const bridge = __testBridge;\n"
                "  let lastPlan = null;\n"
                "  const capturePrepare = (targets) => {\n"
                "    lastPlan = bridge.prepareHttpBodyConsumePlan(targets);\n"
                "    return lastPlan;\n"
                "  };\n"
                "  const noopApply = () => {};\n"
                "  const retryLastPlan = () => { bridge.applyHttpBodyConsumePlan(lastPlan); };\n"
                "  const makeReq = (body) =>\n"
                "    new Request('https://u2-prod.test/req', { method: 'POST', body });\n"
                "  const makeRes = (body) => new Response(body);\n"
                "  const readTextSync = (obj) => {\n"
                "    const out = { done: false, value: null, errName: null, errMsg: null };\n"
                "    obj.text().then(\n"
                "      (v) => { out.done = true; out.value = v; },\n"
                "      (e) => { out.done = true; out.errName = e && e.name;\n"
                "               out.errMsg = e && e.message; }\n"
                "    );\n"
                "    return out;\n"
                "  };\n"
                "  const checkConsumedSync = (obj, kind, expectPubUsed) => {\n"
                "    const firstUsed = obj.bodyUsed;\n"
                "    const secondUsed = obj.bodyUsed;\n"
                "    const stream = obj.body;\n"
                "    const locked = stream !== null && stream.locked === true;\n"
                "    let readerThrew = false;\n"
                "    try { stream.getReader(); } catch (e) { readerThrew = e instanceof TypeError; }\n"
                "    let cloneThrew = false;\n"
                "    try { obj.clone(); } catch (e) { cloneThrew = e instanceof TypeError; }\n"
                "    const snap = kind === 'req'\n"
                "      ? bridge.inspectRequest(obj)\n"
                "      : bridge.inspectResponse(obj);\n"
                "    const snapRejected = Boolean(snap && typeof snap.error === 'string' &&\n"
                "      snap.error.indexOf('already used') >= 0);\n"
                "    const textState = readTextSync(obj);\n"
                "    const pubOk = expectPubUsed === null || obj._bodyConsumed === expectPubUsed;\n"
                "    return {\n"
                "      syncOk: firstUsed === true && secondUsed === true && locked &&\n"
                "              readerThrew && cloneThrew && snapRejected && pubOk,\n"
                "      textState\n"
                "    };\n"
                "  };\n") +
            "  const alignCounter = (burn) => {\n"
            "    while (__testPollCount() === 0) {}\n"
            "    let acc = 0;\n"
            "    for (let k = 0; k < burn; ++k) { acc += k; }\n"
            "    return acc;\n"
            "  };\n"
            "  const freezeObj = (o) => Object.freeze(o);\n"
            "  return { capturePrepare, noopApply, retryLastPlan, makeReq, makeRes,\n"
            "           readTextSync, checkConsumedSync, alignCounter, freezeObj };\n"
            "})()";

        JSValue helpers = rt.eval(helper_src);
        JSValue capture_prepare = JS_GetPropertyStr(ctx, helpers, "capturePrepare");
        JSValue noop_apply = JS_GetPropertyStr(ctx, helpers, "noopApply");
        JSValue retry_last_plan = JS_GetPropertyStr(ctx, helpers, "retryLastPlan");
        JSValue make_req = JS_GetPropertyStr(ctx, helpers, "makeReq");
        JSValue make_res = JS_GetPropertyStr(ctx, helpers, "makeRes");
        JSValue read_text_sync = JS_GetPropertyStr(ctx, helpers, "readTextSync");
        JSValue check_consumed_sync = JS_GetPropertyStr(ctx, helpers, "checkConsumedSync");
        JSValue align_counter = JS_GetPropertyStr(ctx, helpers, "alignCounter");
        JSValue freeze_obj = JS_GetPropertyStr(ctx, helpers, "freezeObj");

        auto new_req = [&](const char* body) -> JSValue {
            JSValue a = JS_NewString(ctx, body);
            JSValue r = JS_Call(ctx, make_req, JS_UNDEFINED, 1, &a);
            JS_FreeValue(ctx, a);
            require(!JS_IsException(r) && JS_IsObject(r), "makeReq failed");
            return r;
        };
        auto new_res = [&](const char* body) -> JSValue {
            JSValue a = JS_NewString(ctx, body);
            JSValue r = JS_Call(ctx, make_res, JS_UNDEFINED, 1, &a);
            JS_FreeValue(ctx, a);
            require(!JS_IsException(r) && JS_IsObject(r), "makeRes failed");
            return r;
        };
        auto do_freeze = [&](JSValueConst obj) {
            JSValue arg = JS_DupValue(ctx, obj);
            JSValue r = JS_Call(ctx, freeze_obj, JS_UNDEFINED, 1, &arg);
            JS_FreeValue(ctx, arg);
            require(!JS_IsException(r), "Object.freeze failed");
            JS_FreeValue(ctx, r);
        };
        auto verify_unread_text = [&](JSValueConst obj, const char* expected, const char* msg) {
            JSValue used_v = JS_GetPropertyStr(ctx, obj, "bodyUsed");
            require(!JS_IsException(used_v) && !JS_ToBool(ctx, used_v),
                    (std::string(dir_label) + ": expected bodyUsed===false (" + msg + ")").c_str());
            JS_FreeValue(ctx, used_v);
            JSValue arg = JS_DupValue(ctx, obj);
            JSValue out = JS_Call(ctx, read_text_sync, JS_UNDEFINED, 1, &arg);
            JS_FreeValue(ctx, arg);
            require(!JS_IsException(out), "readTextSync threw");
            rt.drain_jobs();
            JSValue done_v = JS_GetPropertyStr(ctx, out, "done");
            JSValue val_v = JS_GetPropertyStr(ctx, out, "value");
            const char* s = JS_ToCString(ctx, val_v);
            const bool ok = JS_ToBool(ctx, done_v) && s != NULL && std::strcmp(s, expected) == 0;
            if (s != NULL)
                JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, done_v);
            JS_FreeValue(ctx, val_v);
            JS_FreeValue(ctx, out);
            require(ok, (std::string(dir_label) + ": " + msg).c_str());
        };
        auto verify_consumed = [&](JSValueConst obj, const char* kind, int expect_pub,
                                   const char* msg) {
            JSValue args[3] = {
                JS_DupValue(ctx, obj),
                JS_NewString(ctx, kind),
                expect_pub < 0 ? JS_NULL : (expect_pub > 0 ? JS_TRUE : JS_FALSE),
            };
            JSValue res = JS_Call(ctx, check_consumed_sync, JS_UNDEFINED, 3, args);
            for (int i = 0; i < 3; ++i)
                JS_FreeValue(ctx, args[i]);
            require(!JS_IsException(res) && JS_IsObject(res),
                    (std::string(dir_label) + ": checkConsumedSync threw on " + msg).c_str());
            rt.drain_jobs();
            JSValue sync_ok_v = JS_GetPropertyStr(ctx, res, "syncOk");
            JSValue text_st = JS_GetPropertyStr(ctx, res, "textState");
            JSValue done_v = JS_GetPropertyStr(ctx, text_st, "done");
            JSValue ename_v = JS_GetPropertyStr(ctx, text_st, "errName");
            const char* ename = JS_ToCString(ctx, ename_v);
            const bool ok = JS_ToBool(ctx, sync_ok_v) && JS_ToBool(ctx, done_v) && ename != NULL &&
                            std::strcmp(ename, "TypeError") == 0;
            if (ename != NULL)
                JS_FreeCString(ctx, ename);
            JS_FreeValue(ctx, sync_ok_v);
            JS_FreeValue(ctx, done_v);
            JS_FreeValue(ctx, ename_v);
            JS_FreeValue(ctx, text_st);
            JS_FreeValue(ctx, res);
            require(ok, (std::string(dir_label) + ": " + msg).c_str());
        };

        // 1. Production prepare failure on item 2 leaves real Response #1 unread.
        {
            JSValue r1 = new_res("one");
            JSValue bad = JS_NewObject(ctx);
            capsid::BindingHttpConsumeBatch batch;
            require(batch.add_target(ctx, r1) && batch.add_target(ctx, bad), "add_target failed");
            std::string err;
            require(!batch.prepare(prepare_fn, &err) && !err.empty(),
                    "production prepare must reject missing-slot item 2");
            verify_unread_text(r1, "one", "real Response #1 stays unread on prepare failure");
            JS_FreeValue(ctx, r1);
            JS_FreeValue(ctx, bad);
        }

        // 2. Production prepare + reset() rollback leaves real Request & Response unread.
        {
            JSValue req = new_req("left");
            JSValue res = new_res("right");
            capsid::BindingHttpConsumeBatch batch;
            require(batch.add_target(ctx, req) && batch.add_target(ctx, res), "add_target failed");
            std::string err;
            require(batch.prepare(prepare_fn, &err), ("prod prepare failed: " + err).c_str());
            batch.reset();
            verify_unread_text(req, "left", "real Request stays unread after reset");
            verify_unread_text(res, "right", "real Response stays unread after reset");
            JS_FreeValue(ctx, req);
            JS_FreeValue(ctx, res);
        }

        // 3. Round-8 P2 #2: ALL 3 freeze timings on REAL Request & Response
        // with lazy post-publish sync (eager apply omitted via noopApply):
        //   mode 0 = frozen BEFORE prepare()
        //   mode 1 = frozen AFTER publish (tx.committed=true) & BEFORE first read
        //   mode 2 = always writable
        for (int mode = 0; mode < 3; ++mode) {
            JSValue req = new_req("req-freeze-case");
            JSValue res = new_res("res-freeze-case");
            if (mode == 0) {
                do_freeze(req);
                do_freeze(res);
            }
            capsid::BindingHttpConsumeBatch batch;
            require(batch.add_target(ctx, req) && batch.add_target(ctx, res), "add_target failed");
            std::string err;
            require(batch.prepare(capture_prepare, &err),
                    ("freeze-mode prod prepare failed: " + err).c_str());
            require(batch.commit(noop_apply, &err),
                    ("freeze-mode prod publish failed: " + err).c_str());
            if (mode == 1) {
                do_freeze(req);
                do_freeze(res);
            }
            const int expect_pub = (mode == 2) ? 1 : 0;
            const std::string tag = "freeze mode " + std::to_string(mode);
            verify_consumed(req, "req", expect_pub, ("real Request " + tag).c_str());
            verify_consumed(res, "res", expect_pub, ("real Response " + tag).c_str());
            // Retry production applyHttpBodyConsumePlanForBinding on already-synced
            // / post-frozen objects must remain idempotent and throw-free.
            JSValue retry_ret = JS_Call(ctx, retry_last_plan, JS_UNDEFINED, 0, NULL);
            require(!JS_IsException(retry_ret),
                    (std::string(dir_label) + ": retryLastPlan threw in " + tag).c_str());
            JS_FreeValue(ctx, retry_ret);
            verify_consumed(req, "req", expect_pub, ("real Request post-retry " + tag).c_str());
            verify_consumed(res, "res", expect_pub, ("real Response post-retry " + tag).c_str());
            JS_FreeValue(ctx, req);
            JS_FreeValue(ctx, res);
        }

        // 4. Round-8 P2 #1: Calibrated REAL QuickJS JS_SetInterruptHandler sweep
        // firing directly inside PRODUCTION applyHttpBodyConsumePlanForBinding /
        // syncSlotIfCommitted on real [Request, Response] batches. Verifies
        // real interrupt fires, commit succeeds atomically, and retrying reads
        // (including after freezing post-interrupt) produces 100% consistent
        // consumed/locked state with ZERO torn state.
        {
            static const int kBurnSweep[] = {9996, 9992, 9988, 9982, 9974, 9960};
            int total_triggers = 0;
            for (size_t si = 0; si < sizeof(kBurnSweep) / sizeof(kBurnSweep[0]); ++si) {
                std::vector<JSValue> objs;
                capsid::BindingHttpConsumeBatch batch;
                for (int pair = 0; pair < 6; ++pair) {
                    JSValue q = new_req("secret-req");
                    JSValue s = new_res("secret-res");
                    require(batch.add_target(ctx, q) && batch.add_target(ctx, s),
                            "add_target sweep failed");
                    objs.push_back(q);
                    objs.push_back(s);
                }
                std::string err;
                require(batch.prepare(prepare_fn, &err),
                        ("sweep prod prepare failed: " + err).c_str());

                probe.armed = false;
                probe.polls = 0;
                probe.triggers = 0;
                JS_SetInterruptHandler(js_rt, test_interrupt_handler, &probe);
                JSValue burn_arg = JS_NewInt32(ctx, kBurnSweep[si]);
                JSValue aligned = JS_Call(ctx, align_counter, JS_UNDEFINED, 1, &burn_arg);
                JS_FreeValue(ctx, burn_arg);
                require(!JS_IsException(aligned), "alignCounter failed");
                JS_FreeValue(ctx, aligned);

                probe.armed = true;
                const bool committed = batch.commit(apply_fn, &err);
                probe.armed = false;
                JS_SetInterruptHandler(js_rt, NULL, NULL);

                require(committed, (std::string(dir_label) +
                                    ": production commit failed under interrupt sweep: " + err)
                                       .c_str());
                total_triggers += probe.triggers;

                // Freeze half the objects after the interrupted eager flush to
                // exercise BOTH retryable sync and post-publish freeze together.
                for (size_t oi = 0; oi < objs.size(); ++oi) {
                    if ((oi % 2) == (si % 2)) {
                        do_freeze(objs[oi]);
                    }
                    verify_consumed(objs[oi], (oi % 2 == 0) ? "req" : "res", -1,
                                    "real object after mid-production-sync interrupt");
                    JS_FreeValue(ctx, objs[oi]);
                }
            }
            require(total_triggers > 0,
                    (std::string(dir_label) +
                     ": real QuickJS interrupt never fired inside production apply")
                        .c_str());
        }

        JS_FreeValue(ctx, capture_prepare);
        JS_FreeValue(ctx, noop_apply);
        JS_FreeValue(ctx, retry_last_plan);
        JS_FreeValue(ctx, make_req);
        JS_FreeValue(ctx, make_res);
        JS_FreeValue(ctx, read_text_sync);
        JS_FreeValue(ctx, check_consumed_sync);
        JS_FreeValue(ctx, align_counter);
        JS_FreeValue(ctx, freeze_obj);
        JS_FreeValue(ctx, helpers);
        JS_FreeValue(ctx, prepare_fn);
        JS_FreeValue(ctx, apply_fn);
        g_interrupt_probe = NULL;
    };

    run_prod_direction(user_rt, "prod-input(User->Binding)");
    run_prod_direction(binding_rt, "prod-result(Binding->User)");
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
    test_string_and_object_key_fidelity();
    test_precopy_quota_boundaries_and_zero_copy_on_reject();
    test_http_consume_batch_two_phase_atomicity();
    test_production_fetch_atomic_commit_sync_and_freeze();
    return 0;
}
