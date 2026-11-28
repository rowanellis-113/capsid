// Binding RPC v2 (v0.4.0) end-to-end real-worker verification suite.
// Uses the public C ABI (capsid/runtime.h) + Host binding loader so the same
// test executable runs on Linux, macOS, and Windows.

#include "capsid/runtime.h"
#include "graceful_worker_exit.h"
#include "host/binding_compile.h"
#include "win32_compat.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}

void require_result(capsid_result result, const char* operation) {
    require(result == CAPSID_OK, std::string(operation) + ": " + capsid_result_string(result));
}

struct BindingSpec {
    std::string id;
    std::string source;
};

struct HttpResponse {
    std::uint16_t status = 0;
    std::string body;
};

class WorkerHarness {
  public:
    WorkerHarness(const char* worker_path, const std::vector<BindingSpec>& binding_specs,
                  const std::string& user_source) {
        capsid_worker_config config;
        capsid_worker_config_init(&config);
        config.worker_path = worker_path;
        config.strict_sandbox = 0;
        config.process_memory_limit = 0;
        config.request_timeout_ms = 5000;

        require_result(capsid_worker_spawn_ex(&config, NULL, &worker_), "spawn");

        if (!binding_specs.empty()) {
            std::vector<capsid::host::EffectiveBinding> bindings;
            bindings.reserve(binding_specs.size());
            for (const auto& spec : binding_specs) {
                capsid::host::EffectiveBinding binding;
                binding.id = spec.id;
                binding.package.source = spec.source;
                binding.request.config_json = "{}";
                binding.profiles = {"network-client"};
                bindings.push_back(std::move(binding));
            }
            std::string error;
            require(capsid::host::load_effective_bindings_into_worker(worker_, bindings, &error),
                    "load bindings: " + error);
        }

        require_result(capsid_worker_load_bundle_named(
                           worker_, reinterpret_cast<const std::uint8_t*>(user_source.data()),
                           user_source.size(), "file:///binding-v2-app.js"),
                       "load bundle");

        receive_until([](const capsid_event& event) { return event.type == CAPSID_EVENT_READY; });
    }

    ~WorkerHarness() {
        if (worker_ != nullptr) {
            const auto exit = capsid_test::shutdown_and_wait(worker_, 3000);
            capsid_worker_destroy(worker_);
            worker_ = nullptr;
            if (!exit.clean()) {
                std::cerr << "WARN: worker exit not clean: reaped=" << exit.reaped
                          << " status=" << exit.status << " closed=" << exit.channel_closed
                          << std::endl;
                std::exit(1);
            }
        }
    }

    HttpResponse get(std::uint64_t request_id, const std::string& path) {
        const std::string url = "https://v2.test" + path;
        require_result(capsid_worker_begin_bodyless_request(worker_, request_id, "GET",
                                                            url.c_str(), nullptr, 0),
                       "begin GET");
        HttpResponse response;
        receive_until([&](const capsid_event& event) {
            if (event.type == CAPSID_EVENT_LOG) {
                return false;
            }
            if (event.request_id != request_id) {
                return false;
            }
            if (event.type == CAPSID_EVENT_RESPONSE_HEAD) {
                response.status = event.status;
                return false;
            }
            if (event.type == CAPSID_EVENT_RESPONSE_BODY) {
                response.body.append(reinterpret_cast<const char*>(event.payload.data),
                                     event.payload.size);
                require_result(capsid_worker_grant_response_credit(
                                   worker_, request_id,
                                   static_cast<std::uint32_t>(event.payload.size)),
                               "grant credit");
                return false;
            }
            if (event.type == CAPSID_EVENT_RESPONSE_END) {
                return true;
            }
            require(false, "unexpected event type " + std::to_string(event.type));
            return false;
        });
        return response;
    }

  private:
    template <class Consume> void receive_until(Consume consume) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (std::chrono::steady_clock::now() < deadline) {
            const capsid_result flush = capsid_worker_flush(worker_);
            require(flush == CAPSID_OK || flush == CAPSID_WOULD_BLOCK, "worker flush failed");
            capsid_event event = {};
            event.struct_size = sizeof(event);
            const capsid_result result = capsid_worker_next_event(worker_, &event);
            if (result == CAPSID_OK) {
                if (event.type == CAPSID_EVENT_EXIT) {
                    require(false, "worker exited early");
                }
                if (event.type == CAPSID_EVENT_ERROR) {
                    const std::string msg(reinterpret_cast<const char*>(event.payload.data),
                                          event.payload.size);
                    require(false, "worker error (req=" + std::to_string(event.request_id) +
                                       "): " + msg);
                }
                if (consume(event)) {
                    return;
                }
                continue;
            }
            require(result == CAPSID_WOULD_BLOCK, "worker next_event failed");
            capsid_pollfd fd = {};
            fd.fd = capsid_worker_fd(worker_);
            fd.events = POLLIN | (flush == CAPSID_WOULD_BLOCK ? POLLOUT : 0);
            require(capsid::win32::capsid_poll(&fd, 1, 20) >= 0, "worker poll failed");
        }
        require(false, "timed out waiting for worker event");
    }

    capsid_worker* worker_ = nullptr;
};

// M4A-1: Immediate shared-loop wake, round-robin fairness across multiple
// Bindings, and bounded per-turn microtask preemption.
void test_m4a_fair_scheduling(const char* worker_path) {
    const std::string alpha_src =
        "let seq = 0;"
        "export default () => ({"
        "  ping(n) { return { b: 'a', n, seq: ++seq }; },"
        "  storm(count) {"
        "    return new Promise((resolve) => {"
        "      let i = 0;"
        "      function step() {"
        "        if (++i >= count) resolve(i);"
        "        else Promise.resolve().then(step);"
        "      }"
        "      Promise.resolve().then(step);"
        "    });"
        "  }"
        "});";
    const std::string beta_src =
        "let seq = 0;"
        "export default () => ({"
        "  ping(n) { return { b: 'b', n, seq: ++seq }; }"
        "});";
    const std::string app_src =
        "import alpha from 'capsid:binding/alpha';"
        "import beta from 'capsid:binding/beta';"
        "export default {"
        "  async fetch(req) {"
        "    const path = new URL(req.url).pathname;"
        "    if (path === '/rr') {"
        "      const order = [];"
        "      const t0 = performance.now();"
        "      const ps = [];"
        "      for (let i = 0; i < 20; ++i) ps.push(alpha.ping(i).then(() => order.push('a' + i)));"
        "      for (let i = 0; i < 20; ++i) ps.push(beta.ping(i).then(() => order.push('b' + i)));"
        "      await Promise.all(ps);"
        "      const elapsed = performance.now() - t0;"
        "      const firstB = order.indexOf('b0');"
        "      const lastA = order.indexOf('a19');"
        "      return new Response(JSON.stringify({ count: order.length, firstB, lastA, elapsed }));"
        "    }"
        "    if (path === '/storm') {"
        "      const events = [];"
        "      const pStorm = alpha.storm(320).then((n) => events.push('storm:' + n));"
        "      const pBeta = beta.ping(1).then(() => events.push('beta'));"
        "      await Promise.all([pStorm, pBeta]);"
        "      return new Response(JSON.stringify(events));"
        "    }"
        "    return new Response('missing', { status: 404 });"
        "  }"
        "};";

    WorkerHarness harness(worker_path, {{"alpha", alpha_src}, {"beta", beta_src}}, app_src);

    const HttpResponse rr = harness.get(1, "/rr");
    require(rr.status == 200, "rr status != 200: " + rr.body);
    // In Binding v1 (1 call / 10 ms FIFO), 40 calls took >= 400 ms and b0 came
    // strictly after a19 (firstB == 20, lastA == 19). With M4A fair round-robin
    // immediate scheduling, b0 arrives well before a19!
    require(rr.body.find("\"count\":40") != std::string::npos, "rr missing results: " + rr.body);
    const std::size_t first_b_pos = rr.body.find("\"firstB\":");
    const std::size_t last_a_pos = rr.body.find("\"lastA\":");
    require(first_b_pos != std::string::npos && last_a_pos != std::string::npos,
            "malformed rr body: " + rr.body);
    const int first_b = std::atoi(rr.body.c_str() + first_b_pos + 9);
    const int last_a = std::atoi(rr.body.c_str() + last_a_pos + 8);
    require(first_b >= 0 && first_b < last_a,
            "beta was head-of-line starved by alpha: " + rr.body);

    const HttpResponse storm = harness.get(2, "/storm");
    require(storm.status == 200, "storm status != 200: " + storm.body);
    require(storm.body == "[\"beta\",\"storm:320\"]",
            "alpha microtask storm starved beta: " + storm.body);
}

// M4A-2: Structured remote errors preserve native subclasses, DOMException,
// QuotaExceededError, and remain tamper-proof against hostile getters, proxies,
// and user-mutated global constructors.
void test_m4a_structured_errors(const char* worker_path) {
    const std::string binding_src =
        "let hostileRan = false;"
        "export default () => ({"
        "  failType() { throw new TypeError('bad arg'); },"
        "  failRange() { return Promise.reject(new RangeError('out of range')); },"
        "  failDom() { throw new DOMException('missing item', 'NotFoundError'); },"
        "  failQuota() { throw new QuotaExceededError('cap hit', { quota: 16, requested: 32 }); },"
        "  failHostileGetter() {"
        "    const e = new Error('orig');"
        "    Object.defineProperty(e, 'message', {"
        "      get() { hostileRan = true; return 'pwned'; },"
        "      configurable: true"
        "    });"
        "    throw e;"
        "  },"
        "  failProxy() {"
        "    return Promise.reject(new Proxy({}, { get() { hostileRan = true; return 'pwned'; } }));"
        "  },"
        "  hostileRan() { return hostileRan; }"
        "});";
    const std::string app_src =
        "import svc from 'capsid:binding/svc';"
        "const OrigTypeError = TypeError;"
        "const OrigRangeError = RangeError;"
        "const OrigDOMException = DOMException;"
        "const OrigQuotaExceededError = QuotaExceededError;"
        "export default {"
        "  async fetch() {"
        "    globalThis.TypeError = function() { throw new Error('tampered'); };"
        "    globalThis.DOMException = function() { throw new Error('tampered'); };"
        "    const out = {};"
        "    try { await svc.failType(); } catch (e) {"
        "      out.type = (e instanceof OrigTypeError) && e.name === 'TypeError' && e.message === 'bad arg';"
        "    }"
        "    try { await svc.failRange(); } catch (e) {"
        "      out.range = (e instanceof OrigRangeError) && e.name === 'RangeError' && e.message === 'out of range';"
        "    }"
        "    try { await svc.failDom(); } catch (e) {"
        "      out.dom = (e instanceof OrigDOMException) && e.name === 'NotFoundError' && e.message === 'missing item';"
        "    }"
        "    try { await svc.failQuota(); } catch (e) {"
        "      out.quota = (e instanceof OrigQuotaExceededError) && e.quota === 16 && e.requested === 32 && e.message === 'cap hit';"
        "    }"
        "    try { await svc.failHostileGetter(); } catch (e) {"
        "      out.hostile = e.message === 'binding call failed';"
        "    }"
        "    try { await svc.failProxy(); } catch (e) {"
        "      out.proxy = e.message === 'binding call failed';"
        "    }"
        "    out.hostileRan = await svc.hostileRan();"
        "    return new Response(JSON.stringify(out));"
        "  }"
        "};";

    WorkerHarness harness(worker_path, {{"svc", binding_src}}, app_src);
    const HttpResponse res = harness.get(1, "/");
    require(res.status == 200, "errors status != 200: " + res.body);
    require(res.body ==
                "{\"type\":true,\"range\":true,\"dom\":true,\"quota\":true,\"hostile\":true,"
                "\"proxy\":true,\"hostileRan\":false}",
            "structured error assertions failed: " + res.body);
}

// M4A-3: Call options ({ signal, timeoutMs }), pre-aborted signals, in-flight
// signal cancellation with cloned reason, per-call TimeoutError without killing
// parent request, strict option validation, and full quota recovery.
void test_m4a_options_signal_timeout(const char* worker_path) {
    const std::string binding_src =
        "let invocations = 0;"
        "let lastAbortName = '';"
        "let lastAbortMessage = '';"
        "let lastAbortPlain = null;"
        "export default () => ({"
        "  count() { return invocations; },"
        "  abortInfo() { return { name: lastAbortName, msg: lastAbortMessage, plain: lastAbortPlain }; },"
        "  ping(x) { invocations++; return 'ok:' + x; },"
        "  waitAbort(_input, call) {"
        "    invocations++;"
        "    return new Promise((resolve) => {"
        "      call.signal.addEventListener('abort', () => {"
        "        const r = call.signal.reason;"
        "        if (r && typeof r === 'object' && 'name' in r) {"
        "          lastAbortName = String(r.name);"
        "          lastAbortMessage = String(r.message);"
        "          lastAbortPlain = null;"
        "        } else {"
        "          lastAbortName = '';"
        "          lastAbortMessage = '';"
        "          lastAbortPlain = r;"
        "        }"
        "        setTimeout(() => resolve('late-value'), 15);"
        "      });"
        "    });"
        "  }"
        "});";
    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';
export default {
  async fetch() {
    const out = {};
    // 1. Pre-aborted signal must not invoke Binding.
    const pre = new AbortController();
    pre.abort(new DOMException('already-dead', 'AbortError'));
    try { await svc.ping('x', { signal: pre.signal }); } catch (e) {
      out.preAborted = e instanceof DOMException && e.name === 'AbortError' && e.message === 'already-dead';
    }
    // 2. timeoutMs: 0 rejects immediately without invoking Binding.
    try { await svc.ping('x', { timeoutMs: 0 }); } catch (e) {
      out.zeroTimeout = e instanceof DOMException && e.name === 'TimeoutError';
    }
    // 3. Invalid options fail closed with TypeError and never run getters.
    let optGetterRan = false;
    const badOpt = Object.defineProperty({}, 'timeoutMs', {
      get() { optGetterRan = true; return 10; }, enumerable: true
    });
    let badRejected = 0;
    for (const o of [{ extra: 1 }, { timeoutMs: -1 }, { signal: { aborted: false } }, badOpt]) {
      try { await svc.ping('x', o); } catch (e) { if (e instanceof TypeError) badRejected++; }
    }
    out.badRejected = badRejected;
    out.optGetterRan = optGetterRan;
    out.countBeforeLive = await svc.count();
    // 4. In-flight AbortSignal cancellation propagates structured reason.
    const ac = new AbortController();
    const pAbort = svc.waitAbort(null, { signal: ac.signal });
    await new Promise((r) => setTimeout(r, 15));
    ac.abort(new DOMException('stop-now', 'AbortError'));
    try { await pAbort; } catch (e) {
      out.inflightAbort = e instanceof DOMException && e.message === 'stop-now';
    }
    await new Promise((r) => setTimeout(r, 30));
    const ai1 = await svc.abortInfo();
    out.bindingSawAbort = ai1.name === 'AbortError' && ai1.msg === 'stop-now';
    // 5. Per-call timeoutMs cancels call with TimeoutError while parent request survives.
    try { await svc.waitAbort(null, { timeoutMs: 25 }); } catch (e) {
      out.callTimeout = e instanceof DOMException && e.name === 'TimeoutError';
    }
    await new Promise((r) => setTimeout(r, 30));
    const ai2 = await svc.abortInfo();
    out.bindingSawTimeout = ai2.name === 'TimeoutError';
    // 6. Reusing one long-lived signal across 80 sequential calls recovers quota cleanly.
    const shared = new AbortController();
    let okCount = 0;
    for (let i = 0; i < 80; ++i) {
      if (await svc.ping(i, { signal: shared.signal }) === 'ok:' + i) okCount++;
    }
    out.reusedSignalOk = okCount;
    return new Response(JSON.stringify(out));
  }
};
)JS";

    WorkerHarness harness(worker_path, {{"svc", binding_src}}, app_src);
    const HttpResponse res = harness.get(1, "/");
    require(res.status == 200, "options status != 200: " + res.body);
    require(res.body ==
                "{\"preAborted\":true,\"zeroTimeout\":true,\"badRejected\":4,"
                "\"optGetterRan\":false,\"countBeforeLive\":0,\"inflightAbort\":true,"
                "\"bindingSawAbort\":true,\"callTimeout\":true,\"bindingSawTimeout\":true,"
                "\"reusedSignalOk\":80}",
            "options/signal/timeout assertions failed: " + res.body);
}

} // namespace

int main(int argc, char** argv) {
    require(argc == 2, "usage: test-worker-binding-v2 <capsid-worker>");
    test_m4a_fair_scheduling(argv[1]);
    test_m4a_structured_errors(argv[1]);
    test_m4a_options_signal_timeout(argv[1]);
    return 0;
}
