#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "quickjs-turbo.h"
}

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int32_t eval_int32(JSContext* ctx, const char* code) {
    JSValue val = JS_Eval(ctx, code, std::strlen(code), "<turbo-test>", JS_EVAL_TYPE_GLOBAL);
    require(!JS_IsException(val), "JS_Eval threw an unexpected exception");
    int32_t out = 0;
    require(JS_ToInt32(ctx, &out, val) == 0, "JS_ToInt32 failed");
    JS_FreeValue(ctx, val);
    return out;
}

} // namespace

int main() {
    // Apply Capsid's production density+performance defaults via the explicit
    // C API (no constructor priority race, no setenv mutation).
    QJSTurboOptions defaults;
    QJS_TurboOptionsInit(&defaults);
    defaults.own_bytecode_tail = QJS_TURBO_OFF;
    defaults.property_ic = QJS_TURBO_ON;
    defaults.property_ic_warmup = 12;
    require(QJS_SetTurboDefaults(&defaults) == 0, "QJS_SetTurboDefaults failed");

    JSRuntime* rt = JS_NewRuntime();
    require(rt != NULL, "JS_NewRuntime failed");
    JSContext* ctx = JS_NewContext(rt);
    require(ctx != NULL, "JS_NewContext failed");

    // 1. Verify structured diagnostics reflect Capsid's configuration.
    QJSTurboInfo info;
    std::memset(&info, 0, sizeof(info));
    info.struct_size = static_cast<uint32_t>(sizeof(info));
    require(QJS_GetTurboInfo(rt, &info) == 0, "QJS_GetTurboInfo failed");
    require(info.api_version == QJS_TURBO_API_VERSION, "unexpected QJS_TURBO_API_VERSION");
    require(info.backend_name != NULL && info.backend_name[0] != '\0', "missing backend_name");
    require(info.own_bytecode_tail_on == 0, "own_bytecode_tail must be OFF (PIC-only mode)");
    require(info.property_ic_compiled == 1, "QJS_PROPERTY_IC must be compiled in");
    require(info.property_ic_enabled == 1, "property_ic must be enabled");
    require(info.property_ic_warmup == 12u, "property_ic_warmup must be 12");
    require(info.native_vm_active == info.native_vm_compiled,
            "compiled native ASM VM must be active");

#if !defined(CAPSID_SANITIZER_BUILD)
#if defined(__linux__) && defined(__x86_64__) && defined(__GNUC__) && !defined(__clang__) &&       \
    (__GNUC__ >= 13)
    require(info.backend == QJS_TURBO_BACKEND_LINUX_X86_64 && info.native_vm_active == 1,
            "expected active LINUX_X86_64 native ASM VM");
#elif defined(_WIN32) && defined(_M_X64) && defined(_MSC_VER)
    require(info.backend == QJS_TURBO_BACKEND_WIN_X86_64 && info.native_vm_active == 1,
            "expected active WIN_X86_64 native ASM VM");
#elif defined(__APPLE__) && defined(__x86_64__) && defined(__clang__)
    require(info.backend == QJS_TURBO_BACKEND_MACOS_X86_64 && info.native_vm_active == 1,
            "expected active MACOS_X86_64 native ASM VM");
#endif
#endif

    // Purge any IC slabs created during TJS runtime bootstrap before testing
    // cold-vs-hot warmup behavior.
    (void)JS_PurgePropertyIC(rt);
    require(QJS_GetTurboInfo(rt, &info) == 0, "QJS_GetTurboInfo after bootstrap purge failed");
    require(info.property_ic_slab_count == 0 && info.property_ic_bytes == 0,
            "bootstrap purge must leave zero IC slabs");

    // 2. Cold functions (< 12 property misses) must allocate zero IC slabs.
    const char* cold_script =
        "(() => {"
        "  let sum = 0;"
        "  const obj = { a: 1, b: 2, c: 3 };"
        "  for (let i = 0; i < 40; i++) {"
        "    const fn = new Function('o', 'return o.a + o.b + o.c + ' + i + ';');"
        "    sum += fn(obj) + fn(obj);"
        "  }"
        "  return sum;"
        "})()";
    require(eval_int32(ctx, cold_script) > 0, "cold script evaluation failed");
    require(QJS_GetTurboInfo(rt, &info) == 0, "QJS_GetTurboInfo after cold script failed");
    require(info.property_ic_slab_count == 0 && info.property_ic_bytes == 0,
            "cold functions below warmup threshold must allocate 0 IC bytes");

    // 3. Hot function (>= 12 misses) must warm up lazily and populate native PIC.
    const char* hot_script = "globalThis.__hotSum = function(o, n) {"
                             "  let s = 0;"
                             "  for (let i = 0; i < n; i++) s += o.x + o.y;"
                             "  return s;"
                             "};"
                             "__hotSum({ x: 3, y: 4 }, 200);";
    require(eval_int32(ctx, hot_script) == 1400, "hot script returned wrong sum");
    require(QJS_GetTurboInfo(rt, &info) == 0, "QJS_GetTurboInfo after hot script failed");
    require(info.property_ic_slab_count >= 1 && info.property_ic_bytes > 0,
            "hot function must allocate Property IC slab after warmup");

    // 4. JS_PurgePropertyIC(rt) must reclaim live IC slabs and allow clean re-warming.
    const size_t reclaimed = JS_PurgePropertyIC(rt);
    require(reclaimed > 0, "JS_PurgePropertyIC must reclaim hot IC bytes");
    require(QJS_GetTurboInfo(rt, &info) == 0, "QJS_GetTurboInfo after purge failed");
    require(info.property_ic_slab_count == 0 && info.property_ic_bytes == 0,
            "JS_PurgePropertyIC must leave 0 slabs and 0 bytes");

    require(eval_int32(ctx, "__hotSum({ x: 5, y: 6 }, 200);") == 2200,
            "hot function after purge returned wrong sum");
    require(QJS_GetTurboInfo(rt, &info) == 0, "QJS_GetTurboInfo after re-warm failed");
    require(info.property_ic_slab_count >= 1 && info.property_ic_bytes > 0,
            "hot function must re-warm IC after purge");

    // 5. Per-runtime isolation via JS_SetRuntimeTurboOptions.
    JSRuntime* rt2 = JS_NewRuntime();
    require(rt2 != NULL, "JS_NewRuntime failed");
    QJSTurboOptions rt2_opts;
    QJS_TurboOptionsInit(&rt2_opts);
    rt2_opts.property_ic = QJS_TURBO_OFF;
    require(JS_SetRuntimeTurboOptions(rt2, &rt2_opts) == 0, "JS_SetRuntimeTurboOptions failed");
    QJSTurboInfo info2;
    std::memset(&info2, 0, sizeof(info2));
    info2.struct_size = static_cast<uint32_t>(sizeof(info2));
    require(QJS_GetTurboInfo(rt2, &info2) == 0, "QJS_GetTurboInfo(rt2) failed");
    require(info2.property_ic_enabled == 0, "rt2 must have property_ic disabled");
    require(QJS_GetTurboInfo(rt, &info) == 0 && info.property_ic_enabled == 1,
            "rt1 must remain unaffected by rt2 options");
    JS_FreeRuntime(rt2);

    std::printf(
        "QUICKJS_TURBO_INTEGRATION_OK backend=%s native=%u tail=%u warmup=%u reclaimed=%zu\n",
        info.backend_name, static_cast<unsigned>(info.native_vm_active),
        static_cast<unsigned>(info.own_bytecode_tail_on),
        static_cast<unsigned>(info.property_ic_warmup), reclaimed);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
