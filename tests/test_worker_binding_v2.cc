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
        require_result(capsid_worker_begin_bodyless_request(worker_, request_id, "GET", url.c_str(),
                                                            nullptr, 0),
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
                response.status = static_cast<std::uint16_t>(event.status);
                return false;
            }
            if (event.type == CAPSID_EVENT_RESPONSE_BODY) {
                response.body.append(reinterpret_cast<const char*>(event.payload.data),
                                     event.payload.size);
                require_result(
                    capsid_worker_grant_response_credit(
                        worker_, request_id, static_cast<std::uint32_t>(event.payload.size)),
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
                    require(false,
                            "worker error (req=" + std::to_string(event.request_id) + "): " + msg);
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
    const std::string alpha_src = "let seq = 0;"
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
    const std::string beta_src = "let seq = 0;"
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
    require(first_b >= 0 && first_b < last_a, "beta was head-of-line starved by alpha: " + rr.body);

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
    require(res.body == "{\"preAborted\":true,\"zeroTimeout\":true,\"badRejected\":4,"
                        "\"optGetterRan\":false,\"countBeforeLive\":0,\"inflightAbort\":true,"
                        "\"bindingSawAbort\":true,\"callTimeout\":true,\"bindingSawTimeout\":true,"
                        "\"reusedSignalOk\":80}",
            "options/signal/timeout assertions failed: " + res.body);
}

// M4B-1: Bidirectional ReadableStream proxies, credit-based backpressure,
// cancel/error propagation, locked/disturbed rejection, partial clone rollback,
// late-result stream disposal, and cross-request ownership fencing.
void test_m4b_readable_streams(const char* worker_path) {
    const std::string binding_src = R"JS(
let produced = 0;
let cancelReason = null;
let lateCanceled = false;
export default () => ({
  fastProducer() {
    produced = 0;
    return new ReadableStream({
      pull(controller) {
        if (produced >= 20) {
          controller.close();
          return;
        }
        const chunk = new Uint8Array(16384);
        chunk.fill((produced + 1) & 0xff);
        produced++;
        controller.enqueue(chunk);
      }
    });
  },
  producedCount() { return produced; },
  cancelableStream() {
    cancelReason = null;
    return new ReadableStream({
      pull(controller) {
        controller.enqueue(new Uint8Array([1, 2, 3, 4]));
      },
      cancel(reason) {
        cancelReason = reason ? `${reason.name}:${reason.message}` : 'none';
      }
    });
  },
  lastCancelReason() { return cancelReason; },
  erroringStream() {
    let n = 0;
    return new ReadableStream({
      pull(controller) {
        if (n++ === 0) controller.enqueue(new Uint8Array([9, 8]));
        else controller.error(new RangeError('stream-boom'));
      }
    });
  },
  delayedStream() {
    lateCanceled = false;
    return new Promise((resolve) => {
      setTimeout(() => {
        resolve(new ReadableStream({
          pull(controller) { controller.enqueue(new Uint8Array([1])); },
          cancel() { lateCanceled = true; }
        }));
      }, 40);
    });
  },
  wasLateCanceled() { return lateCanceled; },
  async consumeUpload(stream) {
    const reader = stream.getReader();
    let total = 0;
    let checksum = 0;
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      total += value.byteLength;
      for (let i = 0; i < value.byteLength; ++i) checksum = (checksum + value[i]) % 65521;
    }
    return { total, checksum };
  },
  holdStream() {
    return new ReadableStream({
      pull(controller) { controller.enqueue(new Uint8Array([42])); }
    });
  }
});
)JS";

    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';
let sharedRelease = null;
export default {
  async fetch(req) {
    const path = new URL(req.url).pathname;
    if (path === '/readable') {
      const out = {};
      // 1. Binding -> User backpressure + full 320 KiB read integrity.
      const stream = await svc.fastProducer();
      await new Promise((r) => setTimeout(r, 25));
      const pausedProduced = await svc.producedCount();
      out.backpressured = pausedProduced > 0 && pausedProduced < 12;
      const reader = stream.getReader();
      let total = 0;
      let badByte = false;
      let chunkIndex = 0;
      let buf = new Uint8Array(0);
      for (;;) {
        const { done, value } = await reader.read();
        if (done) break;
        const next = new Uint8Array(buf.length + value.length);
        next.set(buf);
        next.set(value, buf.length);
        buf = next;
        total += value.length;
      }
      for (let i = 0; i < 20; ++i) {
        const expected = (i + 1) & 0xff;
        if (buf[i * 16384] !== expected || buf[i * 16384 + 16383] !== expected) badByte = true;
      }
      out.total320k = total === 20 * 16384 && !badByte;
      out.finalProduced = await svc.producedCount();

      // 2. User -> Binding ReadableStream (including >64 KiB chunk slicing).
      const big = new Uint8Array(150000);
      for (let i = 0; i < big.length; ++i) big[i] = (i * 7) & 0xff;
      let expectedChecksum = 0;
      for (let i = 0; i < big.length; ++i) expectedChecksum = (expectedChecksum + big[i]) % 65521;
      const small = new Uint8Array([10, 20, 30]);
      for (const b of small) expectedChecksum = (expectedChecksum + b) % 65521;
      const up = new ReadableStream({
        start(c) {
          c.enqueue(big);
          c.enqueue(small);
          c.close();
        }
      });
      const upRes = await svc.consumeUpload(up);
      out.uploadOk = upRes.total === 150003 && upRes.checksum === expectedChecksum;

      // 3. Cancel propagation with structured DOMException reason.
      const cStream = await svc.cancelableStream();
      const cReader = cStream.getReader();
      const first = await cReader.read();
      await cReader.cancel(new DOMException('done-early', 'AbortError'));
      await new Promise((r) => setTimeout(r, 20));
      out.cancelOk = !first.done && first.value.length === 4 &&
                     (await svc.lastCancelReason()) === 'AbortError:done-early';

      // 4. Error propagation preserves RangeError subclass.
      const eStream = await svc.erroringStream();
      const eReader = eStream.getReader();
      const eFirst = await eReader.read();
      let errOk = false;
      try { await eReader.read(); } catch (e) {
        errOk = e instanceof RangeError && e.message === 'stream-boom';
      }
      out.errorOk = !eFirst.done && eFirst.value[0] === 9 && errOk;

      // 5. Locked & disturbed rejection + partial clone rollback.
      const locked = new ReadableStream();
      locked.getReader();
      let lockedRejected = false;
      try { await svc.consumeUpload(locked); } catch (e) {
        lockedRejected = e.message.includes('locked');
      }
      const disturbed = new ReadableStream({ start(c) { c.enqueue(new Uint8Array([1])); c.close(); } });
      await disturbed.getReader().read();
      let disturbedRejected = false;
      try { await svc.consumeUpload(disturbed); } catch (e) {
        disturbedRejected = e.message.includes('locked') || e.message.includes('disturbed');
      }
      const rollbackStream = new ReadableStream({ start(c) { c.enqueue(new Uint8Array([1])); c.close(); } });
      let partialRejected = false;
      try { await svc.consumeUpload([rollbackStream, () => {}]); } catch {
        partialRejected = true;
      }
      out.rejectsOk = lockedRejected && disturbedRejected && partialRejected;

      // 6. Late-result stream disposal when call times out before Binding resolves.
      try { await svc.delayedStream(null, { timeoutMs: 15 }); } catch {}
      await new Promise((r) => setTimeout(r, 55));
      out.lateDisposed = await svc.wasLateCanceled();

      return new Response(JSON.stringify(out));
    }
    if (path === '/hold') {
      globalThis.__leakedStream = await svc.holdStream();
      await new Promise((r) => { sharedRelease = r; });
      return new Response('released');
    }
    if (path === '/steal') {
      const s = globalThis.__leakedStream;
      let fenced = false;
      try {
        await s.getReader().read();
      } catch (e) {
        fenced = e instanceof DOMException && e.name === 'InvalidStateError';
      }
      if (sharedRelease) sharedRelease();
      return new Response(JSON.stringify({ fenced }));
    }
    return new Response('404', { status: 404 });
  }
};
)JS";

    WorkerHarness harness(worker_path, {{"svc", binding_src}}, app_src);
    const HttpResponse res = harness.get(1, "/readable");
    require(res.status == 200, "m4b readable status != 200: " + res.body);
    require(res.body == "{\"backpressured\":true,\"total320k\":true,\"finalProduced\":20,"
                        "\"uploadOk\":true,\"cancelOk\":true,\"errorOk\":true,"
                        "\"rejectsOk\":true,\"lateDisposed\":true}",
            "m4b readable assertions failed: " + res.body);
}

// M4B-2: Bidirectional WritableStream proxies, slow-sink backpressure,
// close/error/abort propagation, and locked writable rejection.
void test_m4b_writable_streams(const char* worker_path) {
    const std::string binding_src = R"JS(
let sinkBytes = 0;
let sinkClosed = false;
let sinkAbortReason = null;
export default () => ({
  slowSink() {
    sinkBytes = 0;
    sinkClosed = false;
    return new WritableStream({
      async write(chunk) {
        await new Promise((r) => setTimeout(r, 5));
        sinkBytes += chunk.byteLength;
      },
      close() { sinkClosed = true; }
    });
  },
  sinkStats() { return { sinkBytes, sinkClosed, sinkAbortReason }; },
  erroringSink() {
    let n = 0;
    return new WritableStream({
      write(chunk) {
        if (++n === 2) throw new TypeError('disk-full');
      }
    });
  },
  abortableSink() {
    sinkAbortReason = null;
    return new WritableStream({
      write() {},
      abort(reason) {
        sinkAbortReason = reason ? `${reason.name}:${reason.message}` : 'none';
      }
    });
  },
  async writeToUserSink(writable) {
    const w = writable.getWriter();
    await w.write(new Uint8Array([1, 2, 3]));
    await w.write(new Uint8Array([4, 5]));
    await w.close();
    return 'written';
  }
});
)JS";

    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';
export default {
  async fetch() {
    const out = {};
    // 1. Binding -> User WritableStream with slow sink backpressure (>64 KiB).
    const ws = await svc.slowSink();
    const writer = ws.getWriter();
    for (let i = 0; i < 8; ++i) {
      await writer.write(new Uint8Array(16384));
    }
    await writer.close();
    const stats = await svc.sinkStats();
    out.slowSinkOk = stats.sinkBytes === 8 * 16384 && stats.sinkClosed === true;

    // 2. User -> Binding WritableStream.
    let userBytes = 0;
    let userClosed = false;
    const userSink = new WritableStream({
      write(chunk) { userBytes += chunk.byteLength; },
      close() { userClosed = true; }
    });
    const wRes = await svc.writeToUserSink(userSink);
    out.userSinkOk = wRes === 'written' && userBytes === 5 && userClosed === true;

    // 3. Sink error propagates TypeError to writer.
    const errWs = await svc.erroringSink();
    const errW = errWs.getWriter();
    await errW.write(new Uint8Array([1]));
    let errPropagated = false;
    try {
      await errW.write(new Uint8Array([2]));
      await errW.close();
    } catch (e) {
      errPropagated = e instanceof TypeError && e.message === 'disk-full';
    }
    out.sinkErrOk = errPropagated;

    // 4. Writer abort propagates structured reason to sink.
    const abWs = await svc.abortableSink();
    const abW = abWs.getWriter();
    await abW.abort(new DOMException('abort-write', 'AbortError'));
    await new Promise((r) => setTimeout(r, 20));
    const abStats = await svc.sinkStats();
    out.abortOk = abStats.sinkAbortReason === 'AbortError:abort-write';

    // 5. Locked WritableStream rejected fail-closed.
    const lockedWs = new WritableStream();
    lockedWs.getWriter();
    let lockedRejected = false;
    try { await svc.writeToUserSink(lockedWs); } catch (e) {
      lockedRejected = e.message.includes('locked');
    }
    out.lockedOk = lockedRejected;

    return new Response(JSON.stringify(out));
  }
};
)JS";

    WorkerHarness harness(worker_path, {{"svc", binding_src}}, app_src);
    const HttpResponse res = harness.get(1, "/");
    require(res.status == 200, "m4b writable status != 200: " + res.body);
    require(res.body == "{\"slowSinkOk\":true,\"userSinkOk\":true,\"sinkErrOk\":true,"
                        "\"abortOk\":true,\"lockedOk\":true}",
            "m4b writable assertions failed: " + res.body);
}

// M4B-3 / F4: Pre-mutation cross-request stream proxy ownership fencing.
// Foreign-request cancel/abort/close/getReader/getWriter/constructor/tee/pipeTo/
// re-transfer attempts MUST reject/throw InvalidStateError BEFORE js/streams.js
// mutates stream state, so the owning request can continue reading and writing
// complete payloads to EOF afterwards.
void test_m4b_cross_request_fencing(const char* worker_path) {
    const std::string binding_src = R"JS(
let sinkChunks = [];
let sinkClosed = false;
export default () => ({
  openReadable() {
    const chunks = [
      new Uint8Array([0x11, 0x22]),
      new Uint8Array([0x33, 0x44]),
      new Uint8Array([0x55, 0x66]),
    ];
    let i = 0;
    return new ReadableStream({
      pull(c) {
        if (i >= chunks.length) { c.close(); return; }
        c.enqueue(chunks[i++]);
      }
    });
  },
  openReadable2() {
    return new ReadableStream({
      start(c) {
        c.enqueue(new Uint8Array([0xaa, 0xbb]));
        c.close();
      }
    });
  },
  openWritable() {
    sinkChunks = [];
    sinkClosed = false;
    return new WritableStream({
      write(chunk) { sinkChunks.push(Array.from(chunk)); },
      close() { sinkClosed = true; }
    });
  },
  sinkStats() {
    return { flat: sinkChunks.flat(), closed: sinkClosed };
  },
  async upload(stream) {
    const r = stream.getReader();
    await r.read();
    return 'unexpected';
  }
});
)JS";
    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';
const savedProtoCancel = ReadableStream.prototype.cancel;
const savedProtoAbort = WritableStream.prototype.abort;
let releaseHold = null;
export default {
  async fetch(req) {
    const path = new URL(req.url).pathname;
    if (path === '/hold') {
      const r1 = await svc.openReadable();
      const r2 = await svc.openReadable2();
      const reader2 = r2.getReader();
      const w1 = await svc.openWritable();
      globalThis.__ownedReadable = r1;
      globalThis.__ownedReader = reader2;
      globalThis.__ownedWritable = w1;
      await new Promise((r) => {
        releaseHold = r;
        console.log('hold-ready');
      });
      globalThis.__ownedReadable = null;
      globalThis.__ownedReader = null;
      globalThis.__ownedWritable = null;
      releaseHold = null;

      // After Request 2's rejected foreign attacks, Request 1 MUST still read
      // every byte of both ReadableStreams and write/close its WritableStream!
      const bytes1 = [];
      const rd1 = r1.getReader();
      for (;;) {
        const { done, value } = await rd1.read();
        if (done) break;
        for (const b of value) bytes1.push(b);
      }
      const bytes2 = [];
      for (;;) {
        const { done, value } = await reader2.read();
        if (done) break;
        for (const b of value) bytes2.push(b);
      }
      const wr1 = w1.getWriter();
      await wr1.write(new Uint8Array([9, 8, 7, 6]));
      await wr1.close();
      const st = await svc.sinkStats();
      const ok = bytes1.join(',') === '17,34,51,68,85,102' &&
                 bytes2.join(',') === '170,187' &&
                 st.flat.join(',') === '9,8,7,6' &&
                 st.closed === true;
      return new Response(ok ? 'owner-intact' : 'owner-corrupted:' + JSON.stringify({ bytes1, bytes2, st }));
    }
    if (path === '/steal') {
      const isInv = (e) => e instanceof DOMException && e.name === 'InvalidStateError';
      const checkAsync = async (fn) => {
        try { await fn(); return false; } catch (e) { return isInv(e); }
      };
      const checkSync = (fn) => {
        try { fn(); return false; } catch (e) { return isInv(e); }
      };
      const r = globalThis.__ownedReadable;
      const rd = globalThis.__ownedReader;
      const w = globalThis.__ownedWritable;
      const checks = {
        rCancel: await checkAsync(() => r.cancel('stolen')),
        rProtoCancel: await checkAsync(() => savedProtoCancel.call(r, 'proto')),
        rGetReader: checkSync(() => r.getReader()) && r.locked === false,
        rCtorIdentity: ReadableStreamDefaultReader.prototype.constructor === ReadableStreamDefaultReader,
        wCtorIdentity: WritableStreamDefaultWriter.prototype.constructor === WritableStreamDefaultWriter,
        rCtorReader: checkSync(() => new ReadableStreamDefaultReader(r)) && r.locked === false,
        rProtoCtorReader:
          checkSync(() => new ReadableStreamDefaultReader.prototype.constructor(r)) &&
          r.locked === false,
        rByobProtoCtor:
          typeof ReadableStreamBYOBReader !== 'function' ||
          (ReadableStreamBYOBReader.prototype.constructor === ReadableStreamBYOBReader &&
            checkSync(() => new ReadableStreamBYOBReader.prototype.constructor(r)) &&
            r.locked === false),
        rTee: checkSync(() => r.tee()) && r.locked === false,
        rPipeTo: (await checkAsync(() => r.pipeTo(new WritableStream()))) && r.locked === false,
        rPipeThroughSelf:
          checkSync(() => r.pipeThrough({ readable: new ReadableStream(), writable: new WritableStream() })) &&
          r.locked === false,
        wPipeThroughGetter:
          checkSync(() =>
            new ReadableStream().pipeThrough({
              readable: new ReadableStream(),
              get writable() {
                return w;
              },
            }),
          ) && w.locked === false,
        wPipeThroughFlipFlop: (() => {
          let gets = 0;
          const decoy = new WritableStream();
          const outStream = new ReadableStream({ start(c) { c.close(); } }).pipeThrough({
            readable: new ReadableStream(),
            get writable() {
              gets++;
              return gets === 1 ? decoy : w;
            },
          });
          return Boolean(outStream) && gets === 1 && w.locked === false;
        })(),
        rRetransfer: (await checkAsync(() => svc.upload(r))) && r.locked === false,
        rdRead: await checkAsync(() => rd.read()),
        rdCancel: await checkAsync(() => rd.cancel('stolen-rd')),
        rdRelease: checkSync(() => rd.releaseLock()),
        wAbort: await checkAsync(() => w.abort('stolen-w')),
        wProtoAbort: await checkAsync(() => savedProtoAbort.call(w, 'proto-w')),
        wClose: await checkAsync(() => w.close()),
        wGetWriter: checkSync(() => w.getWriter()) && w.locked === false,
        wCtorWriter: checkSync(() => new WritableStreamDefaultWriter(w)) && w.locked === false,
        wProtoCtorWriter:
          checkSync(() => new WritableStreamDefaultWriter.prototype.constructor(w)) &&
          w.locked === false,
      };
      const allOk = Object.values(checks).every(Boolean);
      const wake = releaseHold;
      releaseHold = null;
      if (wake) wake();
      return new Response(allOk ? 'fenced-ok' : 'fenced-fail:' + JSON.stringify(checks));
    }
    return new Response('404', { status: 404 });
  }
};
)JS";

    capsid_worker_config config;
    capsid_worker_config_init(&config);
    config.worker_path = worker_path;
    config.strict_sandbox = 0;
    config.process_memory_limit = 0;
    config.request_timeout_ms = 5000;
    capsid_worker* worker = nullptr;
    require_result(capsid_worker_spawn_ex(&config, NULL, &worker), "spawn fence worker");
    capsid::host::EffectiveBinding b;
    b.id = "svc";
    b.package.source = binding_src;
    b.request.config_json = "{}";
    b.profiles = {"network-client"};
    std::string err;
    require(capsid::host::load_effective_bindings_into_worker(worker, {b}, &err), err);
    require_result(capsid_worker_load_bundle_named(
                       worker, reinterpret_cast<const std::uint8_t*>(app_src.data()),
                       app_src.size(), "file:///fence.js"),
                   "load fence bundle");

    auto wait_events = [&](auto predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            const capsid_result flush = capsid_worker_flush(worker);
            require(flush == CAPSID_OK || flush == CAPSID_WOULD_BLOCK, "flush");
            capsid_event ev = {};
            ev.struct_size = sizeof(ev);
            const capsid_result r = capsid_worker_next_event(worker, &ev);
            if (r == CAPSID_OK) {
                require(ev.type != CAPSID_EVENT_ERROR && ev.type != CAPSID_EVENT_EXIT,
                        "unexpected worker error/exit in fence test");
                if (predicate(ev))
                    return;
                continue;
            }
            require(r == CAPSID_WOULD_BLOCK, "next_event");
            capsid_pollfd pfd = {};
            pfd.fd = capsid_worker_fd(worker);
            pfd.events = POLLIN | (flush == CAPSID_WOULD_BLOCK ? POLLOUT : 0);
            require(capsid::win32::capsid_poll(&pfd, 1, 20) >= 0, "poll");
        }
        require(false, "fence test timed out");
    };

    wait_events([](const capsid_event& ev) { return ev.type == CAPSID_EVENT_READY; });
    require_result(
        capsid_worker_begin_bodyless_request(worker, 1, "GET", "https://v2.test/hold", nullptr, 0),
        "begin hold");
    // Wait deterministically until req 1 has opened the stream and armed releaseHold.
    wait_events([](const capsid_event& ev) {
        if (ev.type != CAPSID_EVENT_LOG)
            return false;
        const std::string text(reinterpret_cast<const char*>(ev.payload.data), ev.payload.size);
        return text.find("hold-ready") != std::string::npos;
    });
    require_result(
        capsid_worker_begin_bodyless_request(worker, 2, "GET", "https://v2.test/steal", nullptr, 0),
        "begin steal");

    std::string body1, body2;
    bool end1 = false, end2 = false;
    wait_events([&](const capsid_event& ev) {
        if (ev.type == CAPSID_EVENT_RESPONSE_BODY) {
            if (ev.request_id == 1)
                body1.append(reinterpret_cast<const char*>(ev.payload.data), ev.payload.size);
            if (ev.request_id == 2)
                body2.append(reinterpret_cast<const char*>(ev.payload.data), ev.payload.size);
            capsid_worker_grant_response_credit(worker, ev.request_id,
                                                static_cast<std::uint32_t>(ev.payload.size));
        } else if (ev.type == CAPSID_EVENT_RESPONSE_END) {
            if (ev.request_id == 1)
                end1 = true;
            if (ev.request_id == 2)
                end2 = true;
        }
        return end1 && end2;
    });
    require(body1 == "owner-intact" && body2 == "fenced-ok",
            "cross-request fencing failed: body1=" + body1 + " body2=" + body2);

    const auto exit = capsid_test::shutdown_and_wait(worker, 3000);
    capsid_worker_destroy(worker);
    require(exit.clean(), "fence worker did not exit cleanly");
}

// M4C: Controlled Request and Response proxies (inline & streaming bodies,
// canonical multi-value headers / Set-Cookie, pre-aborted & live Request.signal,
// direct streaming Response return from fetch(), and fail-closed rejections).
void test_m4c_request_response_proxies(const char* worker_path) {
    const std::string binding_src = R"JS(
export default () => ({
  async echoRequest(req) {
    const isReq = req instanceof Request;
    const text = await req.text();
    return {
      isReq,
      url: req.url,
      method: req.method,
      credentials: req.credentials,
      redirect: req.redirect,
      mode: req.mode,
      referrer: req.referrer,
      hdr: req.headers.get('x-custom'),
      text,
    };
  },
  async sumStreamRequest(req) {
    const buf = new Uint8Array(await req.arrayBuffer());
    let sum = 0;
    for (let i = 0; i < buf.length; ++i) sum = (sum + buf[i]) % 65521;
    return { len: buf.length, sum };
  },
  checkPreAbortedRequest(req) {
    return {
      aborted: req.signal ? req.signal.aborted : false,
      msg: req.signal?.reason?.message ?? '',
    };
  },
  waitRequestSignalAbort(req) {
    return new Promise((resolve) => {
      if (!req.signal) { resolve('no-signal'); return; }
      if (req.signal.aborted) { resolve('already:' + req.signal.reason?.message); return; }
      req.signal.addEventListener('abort', () => {
        resolve(`${req.signal.reason?.name}:${req.signal.reason?.message}`);
      });
    });
  },
  inlineResponse() {
    const h = new Headers();
    h.append('x-a', '1');
    h.append('set-cookie', 'a=1');
    h.append('set-cookie', 'b=2');
    return new Response('hi-user', { status: 201, statusText: 'Created', headers: h });
  },
  noContentResponse() {
    return new Response(null, { status: 204 });
  },
  errorResponse() {
    return Response.error();
  },
  streamResponse() {
    let i = 0;
    const body = new ReadableStream({
      pull(c) {
        if (i >= 5) { c.close(); return; }
        const chunk = new Uint8Array(16384);
        chunk.fill(65 + i); // 'A', 'B', 'C', 'D', 'E'
        i++;
        c.enqueue(chunk);
      }
    });
    return new Response(body, { status: 200, headers: { 'x-stream': 'yes' } });
  },
  async usedResponse() {
    const r = new Response('consumed');
    await r.text();
    return r;
  },
  async echoResponse(res) {
    return { status: res.status, text: await res.text() };
  }
});
)JS";

    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';
export default {
  async fetch(req) {
    const path = new URL(req.url).pathname;
    if (path === '/direct-stream') {
      return await svc.streamResponse();
    }
    if (path === '/m4c') {
      const out = {};
      // 1. User -> Binding inline Request + metadata contract (R7) + bodyUsed protection.
      const r1 = new Request('https://api.test/items?k=1', {
        method: 'POST',
        credentials: 'include',
        redirect: 'manual',
        mode: 'cors',
        headers: { 'x-custom': 'abc' },
        body: 'hello-inline',
      });
      const e1 = await svc.echoRequest(r1);
      out.inlineReq = e1.isReq && e1.url === 'https://api.test/items?k=1' &&
                      e1.method === 'POST' && e1.credentials === 'include' &&
                      e1.redirect === 'manual' && e1.mode === 'cors' &&
                      e1.referrer === null && e1.hdr === 'abc' &&
                      e1.text === 'hello-inline' && r1.bodyUsed === true;
      let reuseRejected = false;
      try { await svc.echoRequest(r1); } catch (e) {
        reuseRejected = e.message.includes('already used');
      }
      out.reuseReqRejected = reuseRejected;

      // 2. User -> Binding streaming Request (>64 KiB).
      const big = new Uint8Array(90000);
      let expectSum = 0;
      for (let i = 0; i < big.length; ++i) {
        big[i] = (i * 3 + 5) & 0xff;
        expectSum = (expectSum + big[i]) % 65521;
      }
      const sBody = new ReadableStream({
        start(c) { c.enqueue(big); c.close(); }
      });
      const r2 = new Request('https://api.test/upload', {
        method: 'PUT',
        body: sBody,
        duplex: 'half',
      });
      const e2 = await svc.sumStreamRequest(r2);
      out.streamReq = e2.len === 90000 && e2.sum === expectSum;

      // 3. Pre-aborted and live-aborted Request.signal propagation.
      const preAc = new AbortController();
      preAc.abort(new DOMException('req-pre-abort', 'AbortError'));
      const preRes = await svc.checkPreAbortedRequest(
        new Request('https://api.test/pre', { signal: preAc.signal }));
      out.preSignal = preRes.aborted === true && preRes.msg === 'req-pre-abort';

      const liveAc = new AbortController();
      const livePromise = svc.waitRequestSignalAbort(
        new Request('https://api.test/live', { signal: liveAc.signal }));
      setTimeout(() => liveAc.abort(new DOMException('req-live-abort', 'AbortError')), 15);
      out.liveSignal = (await livePromise) === 'AbortError:req-live-abort';

      // 4. Binding -> User inline Response + Set-Cookie + immutable headers + 204 + Response.error().
      const res1 = await svc.inlineResponse();
      const cookies = res1.headers.getSetCookie().join(',');
      let headersImmutable = false;
      try { res1.headers.set('x-mut', '1'); } catch (e) {
        headersImmutable = e instanceof TypeError;
      }
      out.inlineRes = (res1 instanceof Response) && res1.status === 201 &&
                      res1.statusText === 'Created' && res1.headers.get('x-a') === '1' &&
                      cookies === 'a=1,b=2' && headersImmutable &&
                      (await res1.text()) === 'hi-user';
      const res204 = await svc.noContentResponse();
      out.res204 = res204.status === 204 && res204.body === null;
      const resErr = await svc.errorResponse();
      out.resErr = resErr.type === 'error' && resErr.status === 0;

      // 5. Negative controls: locked Request body, bodyUsed Response, WebSocket Response.
      const lockedStream = new ReadableStream();
      lockedStream.getReader();
      const lockedReq = new Request('https://api.test/locked', {
        method: 'POST', body: lockedStream, duplex: 'half'
      });
      let lockedReqErr = 'none';
      try { await svc.echoRequest(lockedReq); } catch (e) {
        lockedReqErr = e && e.message ? e.message : String(e);
      }
      let usedResErr = 'none';
      try { await svc.usedResponse(); } catch (e) {
        usedResErr = e && e.message ? e.message : String(e);
      }
      let wsResErr = 'none';
      try {
        const pair = new WebSocketPair();
        const wsRes = new Response(null, { status: 101, webSocket: pair[0] });
        await svc.echoResponse(wsRes);
      } catch (e) {
        wsResErr = e && e.message ? e.message : String(e);
      }
      const u2bRes = await svc.echoResponse(new Response('user-to-binding', { status: 202 }));
      out.negatives = lockedReqErr.includes('locked') &&
                      usedResErr.includes('already used') &&
                      wsResErr.includes('WebSocket') &&
                      u2bRes.status === 202 && u2bRes.text === 'user-to-binding';

      return new Response(JSON.stringify(out));
    }
    return new Response('404', { status: 404 });
  }
};
)JS";

    WorkerHarness harness(worker_path, {{"svc", binding_src}}, app_src);

    const HttpResponse res = harness.get(1, "/m4c");
    require(res.status == 200, "m4c status != 200: " + res.body);
    require(res.body == "{\"inlineReq\":true,\"reuseReqRejected\":true,\"streamReq\":true,"
                        "\"preSignal\":true,\"liveSignal\":true,\"inlineRes\":true,"
                        "\"res204\":true,\"resErr\":true,\"negatives\":true}",
            "m4c assertions failed: " + res.body);

    // Direct streaming Response passthrough from Binding -> User fetch() -> Host!
    const HttpResponse streamed = harness.get(2, "/direct-stream");
    require(streamed.status == 200, "direct stream status != 200");
    require(streamed.body.size() == 5 * 16384,
            "direct stream wrong length: " + std::to_string(streamed.body.size()));
    for (int i = 0; i < 5; ++i) {
        const char expected = static_cast<char>('A' + i);
        require(streamed.body[i * 16384] == expected &&
                    streamed.body[i * 16384 + 16383] == expected,
                "direct stream body byte mismatch");
    }
}

// M4D: Native-branded RpcTarget handles, nested handles, leases, explicit &
// Symbol.dispose, GC finalizer fallback, request-end disposal, quota refill,
// and malicious provider hardening.
void test_m4d_rpc_target_handles(const char* worker_path) {
    const std::string binding_src = R"JS(
const disposedLog = [];
let getterTripped = false;
let proxyTripped = false;

class ChildTarget extends RpcTarget {
  constructor(tag) {
    super();
    this.tag = tag;
  }
  read() { return 'child:' + this.tag; }
  [Symbol.dispose]() { disposedLog.push('child:' + this.tag); }
}

class SessionTarget extends RpcTarget {
  constructor(name, leaseMs) {
    super(leaseMs !== undefined ? { leaseMs } : undefined);
    this.name = name;
    this.n = 0;
  }
  inc(delta) {
    this.n += delta;
    return this.n;
  }
  failType() {
    throw new TypeError('session-bad-arg');
  }
  spawnNested(tag) {
    return { tag, child: new ChildTarget(tag) };
  }
  streamChunk() {
    return new ReadableStream({
      start(c) {
        c.enqueue(new TextEncoder().encode('from-handle:' + 'ok'));
        c.close();
      }
    });
  }
  responseOf() {
    return new Response('handle-res:' + this.name, { status: 202 });
  }
  [Symbol.dispose]() {
    disposedLog.push(this.name);
  }
}

class ThrowingDisposeTarget extends RpcTarget {
  ping() { return 'alive'; }
  [Symbol.dispose]() {
    disposedLog.push('throw-dispose');
    throw new Error('dispose-boom');
  }
}

class GetterTarget extends RpcTarget {
  ok() { return 1; }
  get hostile() {
    getterTripped = true;
    throw new Error('getter-ran');
  }
}

class OrdinaryClass {
  ping() { return 'nope'; }
}

export default () => ({
  openSession(name) { return new SessionTarget(name); },
  openShortLease(name) { return new SessionTarget(name, 40); },
  openThrowingDispose() { return new ThrowingDisposeTarget(); },
  openOrdinary() { return new OrdinaryClass(); },
  openGetterTarget() { return new GetterTarget(); },
  openProxyTarget() {
    const t = new SessionTarget('proxied');
    return new Proxy(t, {
      ownKeys() { proxyTripped = true; return ['inc']; }
    });
  },
  openLateTarget() {
    return new Promise((resolve) => {
      setTimeout(() => resolve(new SessionTarget('late-res')), 50);
    });
  },
  accept(v) { return v; },
  stats() {
    return {
      disposed: disposedLog.slice(),
      getterTripped,
      proxyTripped,
    };
  },
});
)JS";

    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';
export default {
  async fetch(req) {
    const path = new URL(req.url).pathname;
    if (path === '/m4d-main') {
      const out = {};
      out.noUserRpcTarget = (typeof globalThis.RpcTarget === 'undefined');

      // 1. Basic handle calls, frozen null-proto stub, streams, Response, errors, nested handle.
      const s = await svc.openSession('s1');
      out.stubShape = Object.getPrototypeOf(s) === null && Object.isFrozen(s);
      const v1 = await s.inc(3);
      const v2 = await s.inc(4);
      let errTypeOk = false;
      try { await s.failType(); } catch (e) {
        errTypeOk = (e instanceof TypeError) && e.message === 'session-bad-arg';
      }
      const stream = await s.streamChunk();
      const streamText = await new Response(stream).text();
      const hRes = await s.responseOf();
      const hResText = await hRes.text();
      const nested = await s.spawnNested('sub1');
      const childVal = await nested.child.read();
      nested.child[Symbol.dispose]();
      s.dispose();
      s.dispose(); // idempotent
      let postDisposeErr = false;
      try { await s.inc(1); } catch (e) {
        postDisposeErr = (e instanceof DOMException) && e.name === 'InvalidStateError';
      }
      out.basic = v1 === 3 && v2 === 7 && errTypeOk && streamText === 'from-handle:ok' &&
                  hRes.status === 202 && hResText === 'handle-res:s1' &&
                  nested.tag === 'sub1' && childVal === 'child:sub1' && postDisposeErr;

      // 2. Lease expiry triggers automatic disposal and fails subsequent calls.
      const shortH = await svc.openShortLease('short1');
      const beforeExp = await shortH.inc(1);
      await new Promise((r) => setTimeout(r, 80));
      let expiredErr = false;
      try { await shortH.inc(1); } catch (e) {
        expiredErr = (e instanceof DOMException) && e.name === 'InvalidStateError';
      }
      out.lease = beforeExp === 1 && expiredErr;

      // 3. GC finalizer fallback (drop without calling dispose).
      {
        let ephemeral = await svc.openSession('gc1');
        await ephemeral.inc(1);
        ephemeral = null;
      }
      // Give the shared scheduler a turn to pump the finalizer-enqueued disposal.
      await new Promise((r) => setTimeout(r, 30));
      const stAfterGc = await svc.stats();
      out.gcFallback = stAfterGc.disposed.includes('gc1') &&
                       stAfterGc.disposed.includes('s1') &&
                       stAfterGc.disposed.includes('child:sub1') &&
                       stAfterGc.disposed.includes('short1');

      // 4. Malicious provider & negative controls.
      let ordinaryRejected = false;
      try { await svc.openOrdinary(); } catch (e) { ordinaryRejected = true; }
      let getterRejected = false;
      try { await svc.openGetterTarget(); } catch (e) {
        getterRejected = e.message.includes('accessor');
      }
      let proxyRejected = false;
      try { await svc.openProxyTarget(); } catch (e) { proxyRejected = true; }

      // Throwing [Symbol.dispose]() is fault-isolated.
      const td = await svc.openThrowingDispose();
      const tdAlive = await td.ping();
      await td[Symbol.asyncDispose]();
      await new Promise((r) => setTimeout(r, 20));

      // Late-result RpcTarget after timeoutMs is deterministically disposed.
      let lateTimedOut = false;
      try { await svc.openLateTarget(undefined, { timeoutMs: 15 }); } catch (e) {
        lateTimedOut = (e instanceof DOMException) && e.name === 'TimeoutError';
      }
      await new Promise((r) => setTimeout(r, 80));

      // Passing handle back to Binding fails closed.
      const passH = await svc.openSession('pass1');
      let passBackRejected = false;
      try { await svc.accept(passH); } catch (e) { passBackRejected = true; }
      passH.dispose();

      const stNeg = await svc.stats();
      out.malicious = ordinaryRejected && getterRejected && proxyRejected &&
                      !stNeg.getterTripped && !stNeg.proxyTripped &&
                      tdAlive === 'alive' && stNeg.disposed.includes('throw-dispose') &&
                      lateTimedOut && stNeg.disposed.includes('late-res') &&
                      passBackRejected;

      // 5. Handle quota (32/request) + refill after disposal.
      const batch = [];
      for (let i = 0; i < 32; ++i) {
        batch.push(await svc.openSession('q' + i));
      }
      let quotaHit = false;
      try { await svc.openSession('q-overflow'); } catch (e) {
        quotaHit = e.message.includes('handle quota');
      }
      for (const h of batch) h.dispose();
      await new Promise((r) => setTimeout(r, 30));
      const refilled = await svc.openSession('refill-ok');
      const refillVal = await refilled.inc(9);
      refilled.dispose();
      out.quota = quotaHit && refillVal === 9;

      // 6. Leave one handle undisposed to verify request-end cleanup in req 2.
      globalThis.__leakedByReq1 = await svc.openSession('req-end-cleanup');
      globalThis.__leakedByReq1 = null; // keep test free of global leak? No, hold until req2?
      // Hold one inside closure/global so GC does NOT collect it before response ends:
      const held = await svc.openSession('held-at-req-end');
      globalThis.__heldAtReqEnd = held;
      return new Response(JSON.stringify(out));
    }
    if (path === '/m4d-after') {
      const held = globalThis.__heldAtReqEnd;
      globalThis.__heldAtReqEnd = null;
      let deadAfterReqEnd = false;
      try { await held.inc(1); } catch (e) {
        deadAfterReqEnd = (e instanceof DOMException) && e.name === 'InvalidStateError';
      }
      const st = await svc.stats();
      return new Response(JSON.stringify({
        deadAfterReqEnd,
        disposedAtEnd: st.disposed.includes('held-at-req-end'),
      }));
    }
    return new Response('404', { status: 404 });
  }
};
)JS";

    WorkerHarness harness(worker_path, {{"svc", binding_src}}, app_src);
    const HttpResponse r1 = harness.get(1, "/m4d-main");
    require(r1.status == 200, "m4d-main status != 200: " + r1.body);
    require(r1.body == "{\"noUserRpcTarget\":true,\"stubShape\":true,\"basic\":true,"
                       "\"lease\":true,\"gcFallback\":true,\"malicious\":true,\"quota\":true}",
            "m4d-main assertions failed: " + r1.body);

    const HttpResponse r2 = harness.get(2, "/m4d-after");
    require(r2.status == 200, "m4d-after status != 200: " + r2.body);
    require(r2.body == "{\"deadAfterReqEnd\":true,\"disposedAtEnd\":true}",
            "m4d-after assertions failed: " + r2.body);
}

// M4D: Cross-request handle ownership fencing (call, dispose, and pass-through
// from another concurrent request all fail closed with InvalidStateError).
void test_m4d_cross_request_handle_fencing(const char* worker_path) {
    const std::string binding_src = R"JS(
class Guarded extends RpcTarget {
  secret() { return 'req1-only'; }
}
export default () => ({
  open() { return new Guarded(); },
  accept(v) { return v; },
});
)JS";

    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';
let releaseHold = null;
export default {
  async fetch(req) {
    const path = new URL(req.url).pathname;
    if (path === '/hold') {
      const h = await svc.open();
      globalThis.__heldHandle = h;
      const p = new Promise((resolve) => { releaseHold = resolve; });
      console.log('handle-ready');
      await p;
      const ownVal = await h.secret();
      h.dispose();
      globalThis.__heldHandle = null;
      releaseHold = null;
      return new Response('hold:' + ownVal);
    }
    if (path === '/steal') {
      const foreign = globalThis.__heldHandle;
      let callFenced = false;
      try { await foreign.secret(); } catch (e) {
        callFenced = (e instanceof DOMException) && e.name === 'InvalidStateError';
      }
      let disposeFenced = false;
      try { foreign.dispose(); } catch (e) {
        disposeFenced = (e instanceof DOMException) && e.name === 'InvalidStateError';
      }
      let passFenced = false;
      try { await svc.accept(foreign); } catch (e) {
        passFenced = (e instanceof DOMException) && e.name === 'InvalidStateError';
      }
      const wake = releaseHold;
      releaseHold = null;
      if (wake) wake();
      return new Response(
        (callFenced && disposeFenced && passFenced) ? 'handle-fenced-ok' : 'handle-fenced-fail'
      );
    }
    return new Response('404', { status: 404 });
  }
};
)JS";

    capsid_worker_config config;
    capsid_worker_config_init(&config);
    config.worker_path = worker_path;
    config.strict_sandbox = 0;
    config.process_memory_limit = 0;
    config.request_timeout_ms = 5000;
    capsid_worker* worker = nullptr;
    require_result(capsid_worker_spawn_ex(&config, NULL, &worker), "spawn handle fence worker");
    capsid::host::EffectiveBinding b;
    b.id = "svc";
    b.package.source = binding_src;
    b.request.config_json = "{}";
    b.profiles = {"network-client"};
    std::string err;
    require(capsid::host::load_effective_bindings_into_worker(worker, {b}, &err), err);
    require_result(capsid_worker_load_bundle_named(
                       worker, reinterpret_cast<const std::uint8_t*>(app_src.data()),
                       app_src.size(), "file:///handle-fence.js"),
                   "load handle fence bundle");

    auto wait_events = [&](auto predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            const capsid_result flush = capsid_worker_flush(worker);
            require(flush == CAPSID_OK || flush == CAPSID_WOULD_BLOCK, "flush");
            capsid_event ev = {};
            ev.struct_size = sizeof(ev);
            const capsid_result r = capsid_worker_next_event(worker, &ev);
            if (r == CAPSID_OK) {
                require(ev.type != CAPSID_EVENT_ERROR && ev.type != CAPSID_EVENT_EXIT,
                        "unexpected worker error/exit in handle fence test");
                if (predicate(ev))
                    return;
                continue;
            }
            require(r == CAPSID_WOULD_BLOCK, "next_event");
            capsid_pollfd pfd = {};
            pfd.fd = capsid_worker_fd(worker);
            pfd.events = POLLIN | (flush == CAPSID_WOULD_BLOCK ? POLLOUT : 0);
            require(capsid::win32::capsid_poll(&pfd, 1, 20) >= 0, "poll");
        }
        require(false, "handle fence test timed out");
    };

    wait_events([](const capsid_event& ev) { return ev.type == CAPSID_EVENT_READY; });
    require_result(
        capsid_worker_begin_bodyless_request(worker, 1, "GET", "https://v2.test/hold", nullptr, 0),
        "begin hold");
    wait_events([](const capsid_event& ev) {
        if (ev.type != CAPSID_EVENT_LOG)
            return false;
        const std::string text(reinterpret_cast<const char*>(ev.payload.data), ev.payload.size);
        return text.find("handle-ready") != std::string::npos;
    });
    require_result(
        capsid_worker_begin_bodyless_request(worker, 2, "GET", "https://v2.test/steal", nullptr, 0),
        "begin steal");

    std::string body1, body2;
    bool end1 = false, end2 = false;
    wait_events([&](const capsid_event& ev) {
        if (ev.type == CAPSID_EVENT_RESPONSE_BODY) {
            if (ev.request_id == 1)
                body1.append(reinterpret_cast<const char*>(ev.payload.data), ev.payload.size);
            if (ev.request_id == 2)
                body2.append(reinterpret_cast<const char*>(ev.payload.data), ev.payload.size);
            capsid_worker_grant_response_credit(worker, ev.request_id,
                                                static_cast<std::uint32_t>(ev.payload.size));
        } else if (ev.type == CAPSID_EVENT_RESPONSE_END) {
            if (ev.request_id == 1)
                end1 = true;
            if (ev.request_id == 2)
                end2 = true;
        }
        return end1 && end2;
    });
    require(body1 == "hold:req1-only" && body2 == "handle-fenced-ok",
            "cross-request handle fencing failed: body1=" + body1 + " body2=" + body2);

    const auto exit = capsid_test::shutdown_and_wait(worker, 3000);
    capsid_worker_destroy(worker);
    require(exit.clean(), "handle fence worker did not exit cleanly");
}

// M4D: Malicious Binding whose [Symbol.dispose]() enters an infinite CPU loop
// is interrupted within the bounded disposal window and cleanly poisons/exits
// the worker instead of hanging.
void test_m4d_malicious_dispose_cpu_loop(const char* worker_path) {
    const std::string binding_src = R"JS(
class InfiniteDispose extends RpcTarget {
  ping() { return 'ok'; }
  [Symbol.dispose]() {
    while (true) {}
  }
}
export default () => ({
  open() { return new InfiniteDispose(); }
});
)JS";

    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';
export default {
  async fetch() {
    const h = await svc.open();
    h.dispose();
    await new Promise((r) => setTimeout(r, 300));
    return new Response('done');
  }
};
)JS";

    capsid_worker_config config;
    capsid_worker_config_init(&config);
    config.worker_path = worker_path;
    config.strict_sandbox = 0;
    config.process_memory_limit = 0;
    config.request_timeout_ms = 5000;
    capsid_worker* worker = nullptr;
    require_result(capsid_worker_spawn_ex(&config, NULL, &worker), "spawn cpu-dispose worker");
    capsid::host::EffectiveBinding b;
    b.id = "svc";
    b.package.source = binding_src;
    b.request.config_json = "{}";
    b.profiles = {"network-client"};
    std::string err;
    require(capsid::host::load_effective_bindings_into_worker(worker, {b}, &err), err);
    require_result(capsid_worker_load_bundle_named(
                       worker, reinterpret_cast<const std::uint8_t*>(app_src.data()),
                       app_src.size(), "file:///cpu-dispose.js"),
                   "load cpu-dispose bundle");

    bool ready = false;
    bool exited = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline && !exited) {
        const capsid_result flush = capsid_worker_flush(worker);
        require(flush == CAPSID_OK || flush == CAPSID_WOULD_BLOCK, "flush");
        capsid_event ev = {};
        ev.struct_size = sizeof(ev);
        const capsid_result r = capsid_worker_next_event(worker, &ev);
        if (r == CAPSID_OK) {
            if (ev.type == CAPSID_EVENT_READY && !ready) {
                ready = true;
                require_result(capsid_worker_begin_bodyless_request(
                                   worker, 1, "GET", "https://v2.test/loop", nullptr, 0),
                               "begin loop");
            } else if (ev.type == CAPSID_EVENT_EXIT) {
                exited = true;
            }
            continue;
        }
        require(r == CAPSID_WOULD_BLOCK, "next_event");
        capsid_pollfd pfd = {};
        pfd.fd = capsid_worker_fd(worker);
        pfd.events = POLLIN | (flush == CAPSID_WOULD_BLOCK ? POLLOUT : 0);
        require(capsid::win32::capsid_poll(&pfd, 1, 20) >= 0, "poll");
    }
    require(ready && exited, "malicious CPU-loop [Symbol.dispose]() did not interrupt and exit");
    capsid_worker_destroy(worker);
}

// Audit remediation regressions covering F1, F2 (nested recursive deduped
// late-result cleanup), F3 (queued RPC & staged WritableStream quotas), F5
// (strict 64 KiB credit slicing on 200 KiB chunks), F6 (queued-phase
// Request.signal abort delivery), F7 (RpcTarget method snapshot immunity),
// F8 (genuine Request/Response subclasses, spoof rejection, pre-closed empty
// ReadableStream EOF), F9 (bidirectional Response.error() immutable headers),
// and F10 (burst handle expiry fairness with concurrent ordinary RPC).
void test_audit_f1_to_f10_regressions(const char* worker_path) {
    const std::string binding_src = R"JS(
const lateDisposeCounts = Object.create(null);
const lateStreamCanceled = { s1: false, s2: false, s3: false };
let lastMutableTarget = null;
let mutatedGetterRan = false;
let slicedSinkChecksum = 0;
let slicedSinkLen = 0;
let slicedSinkClosed = false;
let burstDisposed = 0;

class CountedTarget extends RpcTarget {
  constructor(id) {
    super();
    this.id = id;
  }
  ping() { return this.id; }
  [Symbol.dispose]() {
    lateDisposeCounts[this.id] = (lateDisposeCounts[this.id] || 0) + 1;
  }
}

class MutableTarget extends RpcTarget {
  constructor() {
    super();
    this.v = 10;
  }
  inc(d) {
    this.v += d;
    return this.v;
  }
}

class BurstTarget extends RpcTarget {
  constructor(leaseMs) {
    super({ leaseMs });
  }
  ping() { return 'ok'; }
  [Symbol.dispose]() { burstDisposed++; }
}

class SubResponse extends Response {}

export default () => ({
  echoBytesLen(buf) { return buf ? buf.byteLength : 0; },
  ping(tag) { return 'pong:' + tag; },
  lateNested() {
    return new Promise((resolve) => {
      setTimeout(() => {
        const t1 = new CountedTarget('t1');
        const shared = new CountedTarget('shared');
        const s1 = new ReadableStream({ cancel() { lateStreamCanceled.s1 = true; } });
        const s2 = new ReadableStream({ cancel() { lateStreamCanceled.s2 = true; } });
        const s3 = new ReadableStream({ cancel() { lateStreamCanceled.s3 = true; } });
        const container = {
          targets: [t1, shared],
          dup: shared,
          stream: s1,
          req: new Request('https://late.test/req', { method: 'POST', body: s2, duplex: 'half' }),
          res: new Response(s3, { status: 200 }),
        };
        container.self = container; // cycle guard
        resolve(container);
      }, 35);
    });
  },
  lateStats() {
    return { counts: lateDisposeCounts, streams: lateStreamCanceled };
  },
  openSlowSlicedSink() {
    slicedSinkChecksum = 0;
    slicedSinkLen = 0;
    slicedSinkClosed = false;
    return new WritableStream({
      async write(chunk) {
        await new Promise((r) => setTimeout(r, 2));
        for (let i = 0; i < chunk.byteLength; ++i) {
          slicedSinkChecksum = (slicedSinkChecksum + chunk[i]) % 65521;
        }
        slicedSinkLen += chunk.byteLength;
      },
      close() { slicedSinkClosed = true; }
    });
  },
  slicedSinkStats() {
    return { len: slicedSinkLen, checksum: slicedSinkChecksum, closed: slicedSinkClosed };
  },
  checkQueuedSignal(req) {
    return {
      hasSignal: Boolean(req.signal),
      aborted: req.signal ? req.signal.aborted : false,
      name: req.signal?.reason?.name ?? '',
      message: req.signal?.reason?.message ?? '',
    };
  },
  openMutableTarget() {
    lastMutableTarget = new MutableTarget();
    return lastMutableTarget;
  },
  mutateTarget() {
    if (!lastMutableTarget) return false;
    Object.defineProperty(lastMutableTarget, 'inc', {
      configurable: true,
      get() {
        mutatedGetterRan = true;
        throw new Error('mutated-getter-must-not-run');
      },
    });
    return true;
  },
  mutatedStats() { return { mutatedGetterRan }; },
  async echoSubRequest(req) {
    return {
      isReq: req instanceof Request,
      method: req.method,
      url: req.url,
      hdr: req.headers.get('x-sub'),
      text: await req.text(),
    };
  },
  makeSubResponse() {
    return new SubResponse('sub-res-body', { status: 202, headers: { 'x-sub-res': 'yes' } });
  },
  async drainEmptyStream(stream) {
    const reader = stream.getReader();
    const first = await reader.read();
    return { done: first.done };
  },
  makeEmptyClosedStream() {
    return new ReadableStream({ start(c) { c.close(); } });
  },
  makeErrorResponse() {
    return Response.error();
  },
  checkErrorResponseImmutable(res) {
    const checkThrow = (fn) => {
      try { fn(); return false; } catch (e) { return e instanceof TypeError; }
    };
    const setThrew = checkThrow(() => res.headers.set('x-pwn', '1'));
    const appendThrew = checkThrow(() => res.headers.append('x-pwn', '1'));
    const deleteThrew = checkThrow(() => res.headers.delete('x-pwn'));
    return {
      type: res.type,
      status: res.status,
      setThrew,
      appendThrew,
      deleteThrew,
      clean: res.headers.get('x-pwn') === null,
    };
  },
  openBurstTarget(leaseMs) { return new BurstTarget(leaseMs); },
  burstStats() { return { burstDisposed }; },
});
)JS";

    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';

class CustomRequest extends Request {
  constructor(url, init) {
    super(url, init);
    this.extraTag = 'custom';
  }
}

export default {
  async fetch() {
    const out = {};

    // F1: Signal ownership across input cloning + immediate option mutation.
    const f1Ac = new AbortController();
    const f1Opts = { signal: f1Ac.signal };
    const f1Req = new Request('https://f1.test/clone', {
      method: 'POST',
      body: new ReadableStream({ start(c) { c.enqueue(new Uint8Array([1, 2])); c.close(); } }),
      duplex: 'half',
    });
    const f1Promise = svc.echoSubRequest(f1Req, f1Opts);
    f1Opts.signal = null;
    f1Ac.abort(new DOMException('f1-mutated-opts-abort', 'AbortError'));
    let f1AbortedOk = false;
    try { await f1Promise; } catch (e) {
      f1AbortedOk = e instanceof DOMException && e.name === 'AbortError' &&
                    e.message === 'f1-mutated-opts-abort';
    }
    out.f1Ok = f1AbortedOk;

    // F2: Bounded, cycle-safe, deduped nested late-result disposal.
    let f2TimedOut = false;
    try { await svc.lateNested(null, { timeoutMs: 10 }); } catch (e) {
      f2TimedOut = e instanceof DOMException && e.name === 'TimeoutError';
    }
    await new Promise((r) => setTimeout(r, 80));
    const f2Stats = await svc.lateStats();
    out.f2NestedOk = f2TimedOut &&
                     f2Stats.counts.t1 === 1 &&
                     f2Stats.counts.shared === 1 &&
                     f2Stats.streams.s1 === true &&
                     f2Stats.streams.s2 === true &&
                     f2Stats.streams.s3 === true;

    // F3: Queued RPC input payload quota (4 MiB per-request ledger limit) + refill.
    const payload900k = new Uint8Array(900 * 1024);
    const queuedCalls = [];
    for (let i = 0; i < 6; ++i) {
      queuedCalls.push(
        svc.echoBytesLen(payload900k).then(
          (len) => ({ ok: true, len }),
          (e) => ({ ok: false, msg: e && e.message ? e.message : String(e) }),
        ),
      );
    }
    const settledCalls = await Promise.all(queuedCalls);
    const quotaRejected = settledCalls.some((r) => !r.ok && r.msg.includes('byte quota'));
    const refillLen = await svc.echoBytesLen(payload900k);
    out.f3RpcQuotaOk = quotaRejected && refillLen === 900 * 1024;

    // F3 + F5: Staged WritableStream 2 MiB ceiling + 200 KiB credit-sliced write.
    const bigWs = await svc.openSlowSlicedSink();
    const bigWriter = bigWs.getWriter();
    let stagedCapRejected = false;
    try {
      await bigWriter.write(new Uint8Array(2 * 1024 * 1024 + 1024));
    } catch (e) {
      stagedCapRejected = e && e.message && e.message.includes('buffered byte limit');
    }
    const ws200k = await svc.openSlowSlicedSink();
    const w200k = ws200k.getWriter();
    const buf200k = new Uint8Array(200000);
    let expect200kSum = 0;
    for (let i = 0; i < buf200k.length; ++i) {
      buf200k[i] = (i * 11 + 3) & 0xff;
      expect200kSum = (expect200kSum + buf200k[i]) % 65521;
    }
    await w200k.write(buf200k);
    await w200k.close();
    const s200k = await svc.slicedSinkStats();
    out.f3f5StreamOk = stagedCapRejected &&
                       s200k.len === 200000 &&
                       s200k.checksum === expect200kSum &&
                       s200k.closed === true;

    // F6: Request.signal aborted synchronously while RPC call is still kQueued.
    const qAc = new AbortController();
    const qReq = new Request('https://f6.test/queued', { signal: qAc.signal });
    const qPromise = svc.checkQueuedSignal(qReq);
    qAc.abort(new DOMException('sync-queued-abort', 'AbortError'));
    const qRes = await qPromise;
    out.f6QueuedSignalOk = qRes.hasSignal && qRes.aborted === true &&
                           qRes.name === 'AbortError' &&
                           qRes.message === 'sync-queued-abort';

    // F7: RpcTarget exported method snapshot immune to post-export getter mutation.
    const mutH = await svc.openMutableTarget();
    await svc.mutateTarget();
    const incVal = await mutH.inc(5);
    mutH.dispose();
    const mutStats = await svc.mutatedStats();
    out.f7SnapshotOk = incVal === 15 && mutStats.mutatedGetterRan === false;

    // F8: Genuine Request/Response subclasses, spoofed prototype rejection, and
    // pre-closed empty ReadableStream EOF round-trip.
    const subReq = new CustomRequest('https://f8.test/sub', {
      method: 'POST',
      headers: { 'x-sub': 'ok' },
      body: 'sub-req-body',
    });
    const subReqRes = await svc.echoSubRequest(subReq);
    const subRes = await svc.makeSubResponse();
    const subResText = await subRes.text();

    const spoofReq = Object.create(Request.prototype);
    spoofReq._bodyConsumed = false;
    spoofReq._noBody = true;
    spoofReq.method = 'GET';
    spoofReq.url = 'https://spoof.test/';
    let spoofRejected = false;
    try { await svc.echoSubRequest(spoofReq); } catch { spoofRejected = true; }

    const preClosedUser = new ReadableStream({ start(c) { c.close(); } });
    const emptyU2B = await svc.drainEmptyStream(preClosedUser);
    const preClosedBinding = await svc.makeEmptyClosedStream();
    const emptyB2U = await preClosedBinding.getReader().read();

    out.f8Ok = subReqRes.isReq && subReqRes.method === 'POST' && subReqRes.hdr === 'ok' &&
               subReqRes.text === 'sub-req-body' &&
               (subRes instanceof Response) && subRes.status === 202 &&
               subRes.headers.get('x-sub-res') === 'yes' && subResText === 'sub-res-body' &&
               spoofRejected && emptyU2B.done === true && emptyB2U.done === true;

    // F9: Response.error() headers are immutable in BOTH directions.
    const errFromBinding = await svc.makeErrorResponse();
    const checkThrow = (fn) => {
      try { fn(); return false; } catch (e) { return e instanceof TypeError; }
    };
    const b2uErrImm = errFromBinding.type === 'error' && errFromBinding.status === 0 &&
                      checkThrow(() => errFromBinding.headers.set('x-pwn', '1')) &&
                      checkThrow(() => errFromBinding.headers.append('x-pwn', '1')) &&
                      checkThrow(() => errFromBinding.headers.delete('x-pwn')) &&
                      errFromBinding.headers.get('x-pwn') === null;
    const u2bErrImm = await svc.checkErrorResponseImmutable(Response.error());
    out.f9ErrorHeadersOk = b2uErrImm && u2bErrImm.type === 'error' && u2bErrImm.status === 0 &&
                           u2bErrImm.setThrew && u2bErrImm.appendThrew && u2bErrImm.deleteThrew &&
                           u2bErrImm.clean;

    // F10: Expiring 32 short-lease handles in a burst shares the bounded
    // per-turn control budget and does not starve concurrent ordinary RPC.
    const burst = [];
    for (let i = 0; i < 32; ++i) {
      burst.push(await svc.openBurstTarget(30));
    }
    await new Promise((r) => setTimeout(r, 55));
    const pings = await Promise.all([svc.ping('a'), svc.ping('b'), svc.ping('c')]);
    await new Promise((r) => setTimeout(r, 30));
    const bStats = await svc.burstStats();
    out.f10FairnessOk = pings.join(',') === 'pong:a,pong:b,pong:c' && bStats.burstDisposed === 32;

    return new Response(JSON.stringify(out));
  }
};
)JS";

    WorkerHarness harness(worker_path, {{"svc", binding_src}}, app_src);
    const HttpResponse res = harness.get(1, "/");
    require(res.status == 200, "audit regressions status != 200: " + res.body);
    require(res.body == "{\"f1Ok\":true,\"f2NestedOk\":true,\"f3RpcQuotaOk\":true,"
                        "\"f3f5StreamOk\":true,\"f6QueuedSignalOk\":true,\"f7SnapshotOk\":true,"
                        "\"f8Ok\":true,\"f9ErrorHeadersOk\":true,\"f10FairnessOk\":true}",
            "audit regressions failed: " + res.body);
}

// F2: Malicious late-result nested [Symbol.dispose]() CPU loop is bounded by
// the same 100 ms disposal interrupt window and cleanly poisons/exits the
// worker instead of hanging the shared event loop.
void test_audit_f2_late_dispose_cpu_loop(const char* worker_path) {
    const std::string binding_src = R"JS(
class InfiniteLateTarget extends RpcTarget {
  ping() { return 'ok'; }
  [Symbol.dispose]() {
    while (true) {}
  }
}
export default () => ({
  lateLoop() {
    return new Promise((resolve) => {
      setTimeout(() => resolve({ nested: new InfiniteLateTarget() }), 25);
    });
  }
});
)JS";

    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';
export default {
  async fetch() {
    try { await svc.lateLoop(null, { timeoutMs: 10 }); } catch {}
    await new Promise((r) => setTimeout(r, 400));
    return new Response('done');
  }
};
)JS";

    capsid_worker_config config;
    capsid_worker_config_init(&config);
    config.worker_path = worker_path;
    config.strict_sandbox = 0;
    config.process_memory_limit = 0;
    config.request_timeout_ms = 5000;
    capsid_worker* worker = nullptr;
    require_result(capsid_worker_spawn_ex(&config, NULL, &worker), "spawn late-loop worker");
    capsid::host::EffectiveBinding b;
    b.id = "svc";
    b.package.source = binding_src;
    b.request.config_json = "{}";
    b.profiles = {"network-client"};
    std::string err;
    require(capsid::host::load_effective_bindings_into_worker(worker, {b}, &err), err);
    require_result(capsid_worker_load_bundle_named(
                       worker, reinterpret_cast<const std::uint8_t*>(app_src.data()),
                       app_src.size(), "file:///late-cpu-dispose.js"),
                   "load late-loop bundle");

    bool ready = false;
    bool exited = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline && !exited) {
        const capsid_result flush = capsid_worker_flush(worker);
        require(flush == CAPSID_OK || flush == CAPSID_WOULD_BLOCK, "flush");
        capsid_event ev = {};
        ev.struct_size = sizeof(ev);
        const capsid_result r = capsid_worker_next_event(worker, &ev);
        if (r == CAPSID_OK) {
            if (ev.type == CAPSID_EVENT_READY && !ready) {
                ready = true;
                require_result(capsid_worker_begin_bodyless_request(
                                   worker, 1, "GET", "https://v2.test/late-loop", nullptr, 0),
                               "begin late-loop");
            } else if (ev.type == CAPSID_EVENT_EXIT) {
                exited = true;
            }
            continue;
        }
        require(r == CAPSID_WOULD_BLOCK, "next_event");
        capsid_pollfd pfd = {};
        pfd.fd = capsid_worker_fd(worker);
        pfd.events = POLLIN | (flush == CAPSID_WOULD_BLOCK ? POLLOUT : 0);
        require(capsid::win32::capsid_poll(&pfd, 1, 20) >= 0, "poll");
    }
    require(ready && exited,
            "malicious late-result CPU-loop [Symbol.dispose]() did not interrupt and exit");
    capsid_worker_destroy(worker);
}

// R1/F2: Malicious async [Symbol.asyncDispose]() CPU loop AFTER `await Promise.resolve()`
// must inherit the original 100 ms absolute disposal deadline across the Promise
// continuation boundary, interrupt promptly (<< 5 s request timeout), and poison/exit.
void test_audit_r1_async_dispose_cpu_loop(const char* worker_path) {
    const std::string binding_src = R"JS(
class AsyncLoopTarget extends RpcTarget {
  ping() { return 'ok'; }
  async [Symbol.asyncDispose]() {
    await Promise.resolve();
    while (true) {}
  }
}
export default () => ({
  open() { return new AsyncLoopTarget(); }
});
)JS";

    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';
export default {
  async fetch() {
    const h = await svc.open();
    h.dispose();
    await new Promise((r) => setTimeout(r, 3000));
    return new Response('should-have-poisoned');
  }
};
)JS";

    capsid_worker_config config;
    capsid_worker_config_init(&config);
    config.worker_path = worker_path;
    config.strict_sandbox = 0;
    config.process_memory_limit = 0;
    config.request_timeout_ms = 5000;
    capsid_worker* worker = nullptr;
    require_result(capsid_worker_spawn_ex(&config, NULL, &worker), "spawn async-dispose worker");
    capsid::host::EffectiveBinding b;
    b.id = "svc";
    b.package.source = binding_src;
    b.request.config_json = "{}";
    b.profiles = {"network-client"};
    std::string err;
    require(capsid::host::load_effective_bindings_into_worker(worker, {b}, &err), err);
    require_result(capsid_worker_load_bundle_named(
                       worker, reinterpret_cast<const std::uint8_t*>(app_src.data()),
                       app_src.size(), "file:///async-dispose-loop.js"),
                   "load async-dispose bundle");

    bool ready = false;
    bool exited = false;
    std::chrono::steady_clock::time_point req_started;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline && !exited) {
        const capsid_result flush = capsid_worker_flush(worker);
        require(flush == CAPSID_OK || flush == CAPSID_WOULD_BLOCK, "flush");
        capsid_event ev = {};
        ev.struct_size = sizeof(ev);
        const capsid_result r = capsid_worker_next_event(worker, &ev);
        if (r == CAPSID_OK) {
            if (ev.type == CAPSID_EVENT_READY && !ready) {
                ready = true;
                req_started = std::chrono::steady_clock::now();
                require_result(capsid_worker_begin_bodyless_request(
                                   worker, 1, "GET", "https://v2.test/async-dispose", nullptr, 0),
                               "begin async-dispose");
            } else if (ev.type == CAPSID_EVENT_EXIT) {
                exited = true;
            }
            continue;
        }
        require(r == CAPSID_WOULD_BLOCK, "next_event");
        capsid_pollfd pfd = {};
        pfd.fd = capsid_worker_fd(worker);
        pfd.events = POLLIN | (flush == CAPSID_WOULD_BLOCK ? POLLOUT : 0);
        require(capsid::win32::capsid_poll(&pfd, 1, 20) >= 0, "poll");
    }
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - req_started)
                                .count();
    require(ready && exited, "async [Symbol.asyncDispose]() CPU loop did not interrupt and exit");
    require(elapsed_ms < 2000,
            "async disposal took too long (ms=" + std::to_string(elapsed_ms) +
                "), indicating 100 ms disposal deadline did not propagate across await");
    capsid_worker_destroy(worker);
}

// R2 (HTTP header byte quota) + R4 (zero-callback private Fetch snapshot on
// lazy incoming Request, empty Response, and rejected used Response) + R5
// (real-worker multi-turn proof that ordinary RPC runs BEFORE a >32 handle
// disposal backlog finishes clearing).
void test_audit_r2_r4_r5_regressions(const char* worker_path) {
    const std::string binding_src = R"JS(
let disposedCount = 0;
let barrierWaiters = [];
let expiredProviderHits = 0;
class TrackedHandle extends RpcTarget {
  ping() { return 'ok'; }
  [Symbol.dispose]() { disposedCount++; }
}
export default () => ({
  countHeaderBytes(pair) {
    let n = 0;
    for (const [k, v] of pair.a.headers) n += k.length + v.length;
    for (const [k, v] of pair.b.headers) n += k.length + v.length;
    return n;
  },
  inspectReqSnapshot(req) {
    return {
      isReq: req instanceof Request,
      url: req.url,
      method: req.method,
      hdr: req.headers.get('x-lazy-in'),
      updatedHdr: req.headers.get('x-updated'),
      hasSignal: Boolean(req.signal),
      referrer: req.referrer,
    };
  },
  inspectResSnapshot(res) {
    return {
      isRes: res instanceof Response,
      status: res.status,
      hasBody: res.body !== null,
      hdr: res.headers.get('x-empty'),
      addedHdr: res.headers.get('x-added'),
      mapHdr0: res.headers.get('x-m-00'),
      mapHdr19: res.headers.get('x-m-19'),
    };
  },
  async inspectBodyLen(v) {
    const t = await v.text();
    return { len: t.length, first: t.length > 0 ? t[0] : '', last: t.length > 0 ? t[t.length - 1] : '' };
  },
  echoObj(x) {
    return x;
  },
  echoFidelity(v) {
    return {
      echoed: v,
      fromBinding: {
        str: String.fromCharCode(65, 0, 66),
        nul: '\u0000',
        rec: { '': 7, name: 'demo', ['a\u0000b']: 99 },
      },
    };
  },
  openTracked() { return new TrackedHandle(); },
  barrier() {
    return new Promise((resolve) => {
      barrierWaiters.push(resolve);
      if (barrierWaiters.length === 2) {
        const w = barrierWaiters;
        barrierWaiters = [];
        w[0](true);
        w[1](true);
      }
    });
  },
  observeDuringBurst() { return { disposedAtCall: disposedCount }; },
  resetDisposed() { disposedCount = 0; barrierWaiters = []; return true; },
  countedSlow() {
    expiredProviderHits++;
    return new Promise(() => {});
  },
  getCountedHits() { return expiredProviderHits; },
  ping(tag) { return 'pong:' + tag; },
});
)JS";

    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';

function installHostileGetters(obj, keys, bump) {
  for (const key of keys) {
    Object.defineProperty(obj, key, {
      configurable: true,
      get() {
        bump();
        throw new Error('hostile-getter-' + key);
      },
      set() {
        bump();
        throw new Error('hostile-setter-' + key);
      },
    });
  }
}

export default {
  async fetch(incomingReq) {
    const path = new URL(incomingReq.url).pathname;

    if (path === '/r2-r4') {
      const out = {};

      // R2: Variable-length HTTP headers on empty-body Requests count toward
      // the 4 MiB per-request byte cap incrementally during clone!
      // Each pair carries ~120 KiB of headers (0 inline body); 40 concurrent
      // calls (~4.8 MiB) MUST hit the 4 MiB per-request byte quota, then refill.
      const bigVal = 'h'.repeat(7500);
      const makeHeavyHeaderReq = () => {
        const h = new Headers();
        for (let i = 0; i < 8; ++i) {
          h.set('x-pad-' + String(i).padStart(2, '0'), bigVal);
        }
        return new Request('https://r2.test/headers', { headers: h });
      };
      const calls = [];
      for (let i = 0; i < 40; ++i) {
        calls.push(
          svc.countHeaderBytes({ a: makeHeavyHeaderReq(), b: makeHeavyHeaderReq() }).then(
            (n) => ({ ok: true, n }),
            (e) => ({ ok: false, msg: e && e.message ? e.message : String(e) }),
          ),
        );
      }
      const results = await Promise.all(calls);
      const acceptedCount = results.filter((r) => r.ok).length;
      const rejectedByteQuota = results.filter((r) => !r.ok && r.msg.includes('byte quota')).length;
      const refilled = await svc.countHeaderBytes({
        a: makeHeavyHeaderReq(),
        b: makeHeavyHeaderReq(),
      });
      out.r2HeaderQuotaOk =
        acceptedCount > 0 && acceptedCount < 40 && rejectedByteQuota > 0 && refilled > 120000;

      // R4/T3: Side-effect-free private Fetch snapshot on:
      //  (a) incoming Request with private lazy rawHeaders + private signalState
      //      (never aliased onto req._rawHeaders / req._signalState);
      //  (b) empty-body Response(null, { status: 204 }) + map-promoted Response
      //      (>16 headers, never aliased onto res._headersState);
      //  (c) rejected already-used Response;
      //  (d) tampered builtins (Number.isInteger, Object.create, Map.prototype.keys,
      //      Array.prototype.push) invoke ZERO callbacks;
      //  (e) normal Headers.set/append updates still propagate to the peer.
      let callbackCount = 0;
      const bump = () => { callbackCount++; };

      const emptyRes = new Response(null, { status: 204, headers: { 'x-empty': 'yes' } });
      const mapInit = {};
      for (let i = 0; i < 20; ++i) {
        mapInit['x-m-' + String(i).padStart(2, '0')] = 'v' + i;
      }
      const mapRes = new Response(null, { status: 204, headers: mapInit });
      const nestedNotAliased =
        incomingReq._rawHeaders === null &&
        incomingReq._signalState === null &&
        emptyRes._headersState === null &&
        mapRes._headersState === null;

      installHostileGetters(
        incomingReq,
        ['url', 'method', 'headers', '_headers', '_rawHeaders', 'signal', '_signal', '_signalState',
         'body', '_bodyStream', '_bodyFastKind', '_bodyFastData', '_bodyConsumed'],
        bump,
      );
      installHostileGetters(
        emptyRes,
        ['status', 'statusText', 'url', 'type', 'redirected', 'headers', '_headers',
         '_headersState', 'body', '_bodyStream', '_bodyFastKind', '_bodyFastData', '_bodyConsumed'],
        bump,
      );
      installHostileGetters(
        mapRes,
        ['status', 'statusText', 'url', 'type', 'redirected', 'headers', '_headers',
         '_headersState', 'body', '_bodyStream', '_bodyFastKind', '_bodyFastData', '_bodyConsumed'],
        bump,
      );

      const usedRes = new Response('already-read');
      await usedRes.text();
      installHostileGetters(
        usedRes,
        ['status', 'headers', '_headers', '_headersState', 'body', '_bodyStream',
         '_bodyFastKind', '_bodyFastData', '_bodyConsumed'],
        bump,
      );

      // U4: >64 KiB fixed bodies (promoted to stream via createFastBodyStream)
      // MUST also run with zero business/builtin callbacks even when
      // Object.create and own/prototype getters are hostile.
      const bigFixedReq = new Request('https://u4.test/big-req', {
        method: 'POST',
        body: 'Q'.repeat(65537),
      });
      const bigFixedRes = new Response('P'.repeat(65537));
      installHostileGetters(
        bigFixedReq,
        ['url', 'method', 'headers', '_headers', 'signal', '_signal',
         'body', '_bodyStream', '_bodyFastKind', '_bodyFastData', '_bodyConsumed'],
        bump,
      );
      installHostileGetters(
        bigFixedRes,
        ['status', 'statusText', 'url', 'type', 'redirected', 'headers', '_headers',
         '_headersState', 'body', '_bodyStream', '_bodyFastKind', '_bodyFastData', '_bodyConsumed'],
        bump,
      );

      const origIsInt = Number.isInteger;
      const origObjCreate = Object.create;
      const origMapKeys = Map.prototype.keys;
      const origArrPush = Array.prototype.push;
      let reqSnap, resSnap, mapSnap, usedRejected = false, bigReqLen, bigResLen;
      try {
        Number.isInteger = () => { bump(); throw new Error('hostile-isInteger'); };
        Object.create = () => { bump(); throw new Error('hostile-create'); };
        Map.prototype.keys = () => { bump(); throw new Error('hostile-map-keys'); };
        Array.prototype.push = () => { bump(); throw new Error('hostile-arr-push'); };

        const pReq = svc.inspectReqSnapshot(incomingReq);
        const pRes = svc.inspectResSnapshot(emptyRes);
        const pMap = svc.inspectResSnapshot(mapRes);
        const pUsed = svc.inspectResSnapshot(usedRes).then(
          () => false,
          (e) => Boolean(e && e.message && e.message.includes('already used')),
        );
        const pBigReq = svc.inspectBodyLen(bigFixedReq);
        const pBigRes = svc.inspectBodyLen(bigFixedRes);
        Number.isInteger = origIsInt;
        Object.create = origObjCreate;
        Map.prototype.keys = origMapKeys;
        Array.prototype.push = origArrPush;
        [reqSnap, resSnap, mapSnap, usedRejected, bigReqLen, bigResLen] = await Promise.all([
          pReq,
          pRes,
          pMap,
          pUsed,
          pBigReq,
          pBigRes,
        ]);
      } finally {
        Number.isInteger = origIsInt;
        Object.create = origObjCreate;
        Map.prototype.keys = origMapKeys;
        Array.prototype.push = origArrPush;
      }

      // Normal Headers mutation on Request and Response must still propagate.
      const mutReq = new Request('https://r4.test/mut', { headers: { 'x-lazy-in': 'orig' } });
      mutReq.headers.set('x-updated', 'v2');
      const mutReqSnap = await svc.inspectReqSnapshot(mutReq);
      const mutRes = new Response(null, { status: 204, headers: { 'x-empty': 'base' } });
      mutRes.headers.append('x-added', 'ok');
      const mutResSnap = await svc.inspectResSnapshot(mutRes);

      out.r4ZeroCallbackOk =
        nestedNotAliased === true &&
        callbackCount === 0 &&
        reqSnap.isReq === true &&
        reqSnap.method === 'GET' &&
        reqSnap.hdr === 'lazy-ok' &&
        reqSnap.hasSignal === true &&
        reqSnap.referrer === null &&
        resSnap.isRes === true &&
        resSnap.status === 204 &&
        resSnap.hasBody === false &&
        resSnap.hdr === 'yes' &&
        mapSnap.isRes === true &&
        mapSnap.mapHdr0 === 'v0' &&
        mapSnap.mapHdr19 === 'v19' &&
        usedRejected === true &&
        mutReqSnap.hdr === 'orig' &&
        mutReqSnap.updatedHdr === 'v2' &&
        mutResSnap.hdr === 'base' &&
        mutResSnap.addedHdr === 'ok';

      out.u4LargeFixedBodyZeroCallbackOk =
        callbackCount === 0 &&
        bigReqLen.len === 65537 &&
        bigReqLen.first === 'Q' &&
        bigReqLen.last === 'Q' &&
        bigResLen.len === 65537 &&
        bigResLen.first === 'P' &&
        bigResLen.last === 'P';

      // U2: A rejected composite RPC input MUST NOT consume caller HTTP bodies
      // or cancel caller streams; consumption commits ONLY after full clone
      // validation and enqueue succeed.
      const draftRes = new Response('draft');
      let draftResRejected = false;
      try {
        await svc.echoObj({ res: draftRes, bad: Symbol('x') });
      } catch {
        draftResRejected = true;
      }
      const draftResStillUnused = draftRes.bodyUsed === false;
      const draftResText = await draftRes.text();

      const draftReq = new Request('https://u2.test/req', { method: 'POST', body: 'req-draft' });
      let draftReqRejected = false;
      try {
        await svc.echoObj({ req: draftReq, bad: Symbol('x') });
      } catch {
        draftReqRejected = true;
      }
      const draftReqStillUnused = draftReq.bodyUsed === false;
      const draftReqText = await draftReq.text();

      const bigDraftRes = new Response('L'.repeat(65537));
      let bigDraftRejected = false;
      try {
        await svc.echoObj({ res: bigDraftRes, bad: Symbol('x') });
      } catch {
        bigDraftRejected = true;
      }
      const bigDraftStillUnused = bigDraftRes.bodyUsed === false;
      const bigDraftText = await bigDraftRes.text();

      const dupRes = new Response('dup');
      let dupRejectedUsed = false;
      try {
        await svc.echoObj([dupRes, dupRes]);
      } catch (e) {
        dupRejectedUsed = Boolean(e && e.message && e.message.includes('already used'));
      }
      const dupStillUnused = dupRes.bodyUsed === false;
      const dupText = await dupRes.text();

      const draftStream = new ReadableStream({
        start(c) {
          c.enqueue(new Uint8Array([11, 22, 33]));
          c.close();
        },
      });
      let draftStreamRejected = false;
      try {
        await svc.echoObj({ s: draftStream, bad: Symbol('x') });
      } catch {
        draftStreamRejected = true;
      }
      const draftStreamUnlocked = draftStream.locked === false;
      const draftStreamBytes = new Uint8Array(await new Response(draftStream).arrayBuffer());

      const onceRes = new Response('once');
      const onceLen = await svc.inspectBodyLen(onceRes);
      const onceMarkedUsed = onceRes.bodyUsed === true;
      let onceSecondRejected = false;
      try {
        await svc.inspectBodyLen(onceRes);
      } catch (e) {
        onceSecondRejected = Boolean(e && e.message && e.message.includes('already used'));
      }

      out.u2RejectedPreserveOk =
        draftResRejected &&
        draftResStillUnused &&
        draftResText === 'draft' &&
        draftReqRejected &&
        draftReqStillUnused &&
        draftReqText === 'req-draft' &&
        bigDraftRejected &&
        bigDraftStillUnused &&
        bigDraftText.length === 65537 &&
        dupRejectedUsed &&
        dupStillUnused &&
        dupText === 'dup' &&
        draftStreamRejected &&
        draftStreamUnlocked &&
        draftStreamBytes.length === 3 &&
        draftStreamBytes[0] === 11 &&
        draftStreamBytes[2] === 33 &&
        onceLen.len === 4 &&
        onceMarkedUsed &&
        onceSecondRejected;

      // T4: Bidirectional scalar string (embedded NUL) & record (empty + NUL key) fidelity.
      const sentPayload = {
        str: String.fromCharCode(65, 0, 66),
        nul: '\u0000',
        rec: { '': 7, name: 'demo', ['a\u0000b']: 99 },
      };
      const fid = await svc.echoFidelity(sentPayload);
      const checkFid = (obj) => {
        if (!obj || typeof obj.str !== 'string' || typeof obj.nul !== 'string' || !obj.rec) {
          return false;
        }
        const keys = Object.keys(obj.rec);
        return (
          obj.str.length === 3 &&
          obj.str.charCodeAt(0) === 65 &&
          obj.str.charCodeAt(1) === 0 &&
          obj.str.charCodeAt(2) === 66 &&
          obj.nul.length === 1 &&
          obj.nul.charCodeAt(0) === 0 &&
          keys.length === 3 &&
          keys[0] === '' &&
          obj.rec[''] === 7 &&
          keys[1] === 'name' &&
          obj.rec.name === 'demo' &&
          keys[2] === 'a\u0000b' &&
          keys[2].length === 3 &&
          obj.rec['a\u0000b'] === 99
        );
      };
      out.t4FidelityOk = checkFid(fid.echoed) && checkFid(fid.fromBinding);

      // R5/T2: Queue 40 calls with timeoutMs: 25, then synchronously spin past
      // their deadline BEFORE yielding so all 40 stay kQueued when Turn 1 runs:
      // 32 expire in control stage (exhausting control budget) and the remaining
      // 8 hit pump_binding_calls() where dequeue-time expiry MUST reject them
      // with ZERO provider entries and strictly 1 settlement per caller while
      // the valid ping behind them succeeds.
      const settleCounts = new Array(40).fill(0);
      const expCalls = [];
      for (let i = 0; i < 40; ++i) {
        expCalls.push(
          svc.countedSlow(i, { timeoutMs: 25 }).then(
            () => {
              settleCounts[i]++;
              return 'resolved';
            },
            (e) => {
              settleCounts[i]++;
              return e && e.name ? e.name : String(e);
            },
          ),
        );
      }
      const spinEnd = Date.now() + 45;
      while (Date.now() < spinEnd) {
        // Keep all 40 calls queued past their 25 ms deadline before Turn 1.
      }
      const validPing = svc.ping('after-timeout-burst');
      const [alive, expNames] = await Promise.all([validPing, Promise.all(expCalls)]);
      const providerHits = await svc.getCountedHits();
      out.r5TimeoutBurstOk =
        alive === 'pong:after-timeout-burst' &&
        providerHits === 0 &&
        expNames.length === 40 &&
        expNames.every((n) => n === 'TimeoutError') &&
        settleCounts.every((c) => c === 1);

      return new Response(JSON.stringify(out));
    }

    // R5 part A: Two concurrent requests open 24 + 24 = 48 handles (>32 single-turn
    // control budget), synchronize through a Binding barrier so each request
    // keeps its own RequestToken and resumes in the exact same libuv tick,
    // queue all 48 disposals BEFORE the next scheduler turn, and immediately
    // invoke svc.observeDuringBurst() to prove ordinary RPC runs BEFORE all 48
    // disposals drain (backlog spans >= 2 turns).
    if (path === '/r5-a') {
      const hA = [];
      for (let i = 0; i < 24; ++i) hA.push(await svc.openTracked());
      await svc.barrier();
      for (let i = 0; i < hA.length; ++i) hA[i].dispose();
      const during = await svc.observeDuringBurst();
      await new Promise((r) => setTimeout(r, 40));
      const after = await svc.observeDuringBurst();
      return new Response(JSON.stringify({
        disposedAtCall: during.disposedAtCall,
        finalDisposed: after.disposedAtCall,
      }));
    }

    if (path === '/r5-b') {
      const hB = [];
      for (let i = 0; i < 24; ++i) hB.push(await svc.openTracked());
      await svc.barrier();
      for (let i = 0; i < hB.length; ++i) hB[i].dispose();
      // Keep request B open while Turn 1 and Turn 2 drain the 48-handle disposal
      // backlog through the scheduler's 32-unit per-turn control budget (instead
      // of triggering synchronous request-end teardown before Turn 1 runs).
      await new Promise((r) => setTimeout(r, 60));
      return new Response('b-disposed');
    }

    return new Response('404', { status: 404 });
  }
};
)JS";

    capsid_worker_config config;
    capsid_worker_config_init(&config);
    config.worker_path = worker_path;
    config.strict_sandbox = 0;
    config.process_memory_limit = 0;
    config.request_timeout_ms = 15000;
    capsid_worker* worker = nullptr;
    require_result(capsid_worker_spawn_ex(&config, NULL, &worker), "spawn r2/r4/r5 worker");
    capsid::host::EffectiveBinding b;
    b.id = "svc";
    b.package.source = binding_src;
    b.request.config_json = "{}";
    b.profiles = {"network-client"};
    std::string err;
    require(capsid::host::load_effective_bindings_into_worker(worker, {b}, &err), err);
    require_result(capsid_worker_load_bundle_named(
                       worker, reinterpret_cast<const std::uint8_t*>(app_src.data()),
                       app_src.size(), "file:///r2-r4-r5.js"),
                   "load r2/r4/r5 bundle");

    auto wait_until = [&](const char* stage, auto predicate) {
        const auto started = std::chrono::steady_clock::now();
        const auto deadline = started + std::chrono::seconds(20);
        int events_seen = 0;
        int last_ev_type = -1;
        std::uint64_t last_req_id = 0;
        while (std::chrono::steady_clock::now() < deadline) {
            const capsid_result flush = capsid_worker_flush(worker);
            require(flush == CAPSID_OK || flush == CAPSID_WOULD_BLOCK, "flush");
            capsid_event ev = {};
            ev.struct_size = sizeof(ev);
            const capsid_result r = capsid_worker_next_event(worker, &ev);
            if (r == CAPSID_OK) {
                ++events_seen;
                last_ev_type = static_cast<int>(ev.type);
                last_req_id = ev.request_id;
                if (ev.type == CAPSID_EVENT_ERROR) {
                    const std::string emsg(reinterpret_cast<const char*>(ev.payload.data),
                                           ev.payload.size);
                    require(false, std::string("unexpected worker error in stage=") + stage +
                                       " req=" + std::to_string(ev.request_id) + ": " + emsg);
                }
                if (ev.type == CAPSID_EVENT_REQUEST_TIMEOUT) {
                    require(false, std::string("unexpected request timeout in stage=") + stage +
                                       " req=" + std::to_string(ev.request_id));
                }
                require(ev.type != CAPSID_EVENT_EXIT,
                        std::string("unexpected worker exit in stage=") + stage);
                if (predicate(ev))
                    return;
                continue;
            }
            require(r == CAPSID_WOULD_BLOCK, "next_event");
            capsid_pollfd pfd = {};
            pfd.fd = capsid_worker_fd(worker);
            pfd.events = POLLIN | (flush == CAPSID_WOULD_BLOCK ? POLLOUT : 0);
            require(capsid::win32::capsid_poll(&pfd, 1, 20) >= 0, "poll");
        }
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - started)
                                    .count();
        require(false, std::string("r2/r4/r5 test timed out in stage=") + stage + " elapsed_ms=" +
                           std::to_string(elapsed_ms) + " events=" + std::to_string(events_seen) +
                           " last_type=" + std::to_string(last_ev_type) +
                           " last_req=" + std::to_string(last_req_id));
    };

    wait_until("ready", [](const capsid_event& ev) { return ev.type == CAPSID_EVENT_READY; });

    // Request 1: R2 + R4/T3/U4 + U2 + T4 + R5/T2 timeout burst (with lazy incoming header x-lazy-in).
    const capsid_header hdrs[1] = {{
        {reinterpret_cast<const std::uint8_t*>("x-lazy-in"), 9},
        {reinterpret_cast<const std::uint8_t*>("lazy-ok"), 7},
    }};
    require_result(
        capsid_worker_begin_bodyless_request(worker, 1, "GET", "https://v2.test/r2-r4", hdrs, 1),
        "begin r2-r4");
    std::string body1;
    wait_until("req1:r2-r4-t2-t3-t4-u2-u4", [&](const capsid_event& ev) {
        if (ev.request_id == 1 && ev.type == CAPSID_EVENT_RESPONSE_BODY) {
            body1.append(reinterpret_cast<const char*>(ev.payload.data), ev.payload.size);
        }
        return ev.request_id == 1 && ev.type == CAPSID_EVENT_RESPONSE_END;
    });
    require(body1 == "{\"r2HeaderQuotaOk\":true,\"r4ZeroCallbackOk\":true,"
                     "\"u4LargeFixedBodyZeroCallbackOk\":true,\"u2RejectedPreserveOk\":true,"
                     "\"t4FidelityOk\":true,\"r5TimeoutBurstOk\":true}",
            "R2/R4/T2/T3/T4/R5/U2/U4 regressions failed: " + body1);

    // Concurrent Requests 2 & 3: R5 48-handle burst multi-turn fairness proof.
    require_result(
        capsid_worker_begin_bodyless_request(worker, 2, "GET", "https://v2.test/r5-a", nullptr, 0),
        "begin r5-a");
    require_result(
        capsid_worker_begin_bodyless_request(worker, 3, "GET", "https://v2.test/r5-b", nullptr, 0),
        "begin r5-b");
    std::string bodyA, bodyB;
    bool endA = false, endB = false;
    wait_until("req2+3:r5-48-handles", [&](const capsid_event& ev) {
        if (ev.request_id == 2 && ev.type == CAPSID_EVENT_RESPONSE_BODY) {
            bodyA.append(reinterpret_cast<const char*>(ev.payload.data), ev.payload.size);
        } else if (ev.request_id == 2 && ev.type == CAPSID_EVENT_RESPONSE_END) {
            endA = true;
        } else if (ev.request_id == 3 && ev.type == CAPSID_EVENT_RESPONSE_BODY) {
            bodyB.append(reinterpret_cast<const char*>(ev.payload.data), ev.payload.size);
        } else if (ev.request_id == 3 && ev.type == CAPSID_EVENT_RESPONSE_END) {
            endB = true;
        }
        return endA && endB;
    });
    require(bodyB == "b-disposed", "r5-b unexpected body: " + bodyB);
    // Parse disposedAtCall and finalDisposed from JSON response.
    auto extract_int = [&](const std::string& json, const std::string& key) -> int {
        const std::size_t pos = json.find("\"" + key + "\":");
        require(pos != std::string::npos, "missing key " + key + " in " + json);
        return std::atoi(json.c_str() + pos + key.size() + 3);
    };
    const int disposed_at_call = extract_int(bodyA, "disposedAtCall");
    const int final_disposed = extract_int(bodyA, "finalDisposed");
    require(disposed_at_call <= 32,
            "ordinary RPC must dispatch BEFORE all 48 queued disposals drain (saw " +
                std::to_string(disposed_at_call) + ", full=" + bodyA + ")");
    require(final_disposed == 48, "all 48 disposals must complete on subsequent turns (saw " +
                                      std::to_string(final_disposed) + ", full=" + bodyA + ")");

    const auto exit = capsid_test::shutdown_and_wait(worker, 3000);
    capsid_worker_destroy(worker);
    require(exit.clean(), "r2/r4/r5 worker did not exit cleanly");
}

// U1: When a request hits its request deadline while awaiting a Binding call,
// a catch block that immediately attempts a follow-up Binding call during
// timeout job drain MUST fail closed immediately (without enqueuing an orphan
// call or poisoning the worker), and subsequent requests MUST continue cleanly.
void test_audit_u1_post_timeout_binding_call_no_poison(const char* worker_path) {
    const std::string binding_src = R"JS(
let afterTimeoutHits = 0;
export default () => ({
  slowNever() {
    return new Promise(() => {});
  },
  afterTimeoutPing(tag) {
    afterTimeoutHits++;
    return 'hit:' + tag;
  },
  getAfterTimeoutHits() {
    return afterTimeoutHits;
  },
});
)JS";

    const std::string app_src = R"JS(
import svc from 'capsid:binding/svc';

export default {
  async fetch(req) {
    const path = new URL(req.url).pathname;
    if (path === '/timeout-catch') {
      try {
        await svc.slowNever();
      } catch {
        try {
          await svc.afterTimeoutPing('late');
        } catch {
          // Expected: post-timeout Binding call rejected immediately.
        }
      }
      return new Response('late-after-timeout');
    }
    if (path === '/healthy') {
      const hits = await svc.getAfterTimeoutHits();
      return new Response('hits=' + hits);
    }
    return new Response('404', { status: 404 });
  }
};
)JS";

    capsid_worker_config config;
    capsid_worker_config_init(&config);
    config.worker_path = worker_path;
    config.strict_sandbox = 0;
    config.process_memory_limit = 0;
    config.request_timeout_ms = 200;
    capsid_worker* worker = nullptr;
    require_result(capsid_worker_spawn_ex(&config, NULL, &worker), "spawn u1 worker");
    capsid::host::EffectiveBinding b;
    b.id = "svc";
    b.package.source = binding_src;
    b.request.config_json = "{}";
    b.profiles = {"network-client"};
    std::string err;
    require(capsid::host::load_effective_bindings_into_worker(worker, {b}, &err), err);
    require_result(capsid_worker_load_bundle_named(
                       worker, reinterpret_cast<const std::uint8_t*>(app_src.data()),
                       app_src.size(), "file:///u1-timeout.js"),
                   "load u1 bundle");

    auto poll_step = [&](int timeout_ms, capsid_event* out_ev) -> bool {
        const capsid_result flush = capsid_worker_flush(worker);
        require(flush == CAPSID_OK || flush == CAPSID_WOULD_BLOCK, "u1 flush");
        *out_ev = {};
        out_ev->struct_size = sizeof(*out_ev);
        const capsid_result r = capsid_worker_next_event(worker, out_ev);
        if (r == CAPSID_OK) {
            return true;
        }
        require(r == CAPSID_WOULD_BLOCK, "u1 next_event");
        capsid_pollfd pfd = {};
        pfd.fd = capsid_worker_fd(worker);
        pfd.events = POLLIN | (flush == CAPSID_WOULD_BLOCK ? POLLOUT : 0);
        require(capsid::win32::capsid_poll(&pfd, 1, timeout_ms) >= 0, "u1 poll");
        return false;
    };

    // Wait for READY.
    bool ready = false;
    const auto ready_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!ready && std::chrono::steady_clock::now() < ready_deadline) {
        capsid_event ev;
        if (poll_step(20, &ev)) {
            require(ev.type != CAPSID_EVENT_EXIT && ev.type != CAPSID_EVENT_ERROR,
                    "u1 unexpected failure before ready");
            if (ev.type == CAPSID_EVENT_READY) {
                ready = true;
            }
        }
    }
    require(ready, "u1 worker never became ready");

    // Request 1: times out at 200 ms, catches cancellation, and tries a second Binding call.
    require_result(capsid_worker_begin_bodyless_request(
                       worker, 1, "GET", "https://u1.test/timeout-catch", nullptr, 0),
                   "u1 begin req1");
    bool req1_timed_out = false;
    const auto req1_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!req1_timed_out && std::chrono::steady_clock::now() < req1_deadline) {
        capsid_event ev;
        if (poll_step(20, &ev)) {
            require(ev.type != CAPSID_EVENT_EXIT,
                    "u1 worker unexpectedly exited (poisoned) on post-timeout Binding call");
            if (ev.request_id == 1 && ev.type == CAPSID_EVENT_REQUEST_TIMEOUT) {
                req1_timed_out = true;
            }
        }
    }
    require(req1_timed_out, "u1 req1 did not emit CAPSID_EVENT_REQUEST_TIMEOUT");

    // Wait >150 ms (past 100 ms poison grace) to verify reclaim succeeded with zero poison/exit.
    const auto settle_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(160);
    while (std::chrono::steady_clock::now() < settle_until) {
        capsid_event ev;
        if (poll_step(20, &ev)) {
            require(ev.type != CAPSID_EVENT_EXIT,
                    "u1 worker poisoned and exited after req1 timeout reclaim");
            require(ev.type != CAPSID_EVENT_ERROR,
                    "u1 unexpected error during post-timeout settle");
        }
    }

    // Request 2: worker must still be healthy and the post-timeout call must never have run.
    require_result(capsid_worker_begin_bodyless_request(worker, 2, "GET", "https://u1.test/healthy",
                                                        nullptr, 0),
                   "u1 begin req2");
    std::string body2;
    bool req2_done = false;
    const auto req2_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!req2_done && std::chrono::steady_clock::now() < req2_deadline) {
        capsid_event ev;
        if (poll_step(20, &ev)) {
            require(ev.type != CAPSID_EVENT_EXIT && ev.type != CAPSID_EVENT_ERROR &&
                        ev.type != CAPSID_EVENT_REQUEST_TIMEOUT,
                    "u1 req2 failed after req1 timeout");
            if (ev.request_id == 2 && ev.type == CAPSID_EVENT_RESPONSE_BODY) {
                body2.append(reinterpret_cast<const char*>(ev.payload.data), ev.payload.size);
            } else if (ev.request_id == 2 && ev.type == CAPSID_EVENT_RESPONSE_END) {
                req2_done = true;
            }
        }
    }
    require(req2_done && body2 == "hits=0", "u1 req2 unexpected response: " + body2);

    const auto exit = capsid_test::shutdown_and_wait(worker, 3000);
    capsid_worker_destroy(worker);
    require(exit.clean(), "u1 worker did not exit cleanly");
}

} // namespace

int main(int argc, char** argv) {
    require(argc == 2, "usage: test-worker-binding-v2 <capsid-worker>");
    test_m4a_fair_scheduling(argv[1]);
    test_m4a_structured_errors(argv[1]);
    test_m4a_options_signal_timeout(argv[1]);
    test_m4b_readable_streams(argv[1]);
    test_m4b_writable_streams(argv[1]);
    test_m4b_cross_request_fencing(argv[1]);
    test_m4c_request_response_proxies(argv[1]);
    test_m4d_rpc_target_handles(argv[1]);
    test_m4d_cross_request_handle_fencing(argv[1]);
    test_m4d_malicious_dispose_cpu_loop(argv[1]);
    test_audit_f1_to_f10_regressions(argv[1]);
    test_audit_f2_late_dispose_cpu_loop(argv[1]);
    test_audit_r1_async_dispose_cpu_loop(argv[1]);
    test_audit_r2_r4_r5_regressions(argv[1]);
    test_audit_u1_post_timeout_binding_call_no_poison(argv[1]);
    return 0;
}
