# Capsid Binding RPC v2 Technical Design (`v0.4.0`)

> Target branch: `dev/v0.4.0` (C ABI v8, FetchRPC v4, `capsid/binding-v1`
> manifest compatible). Builds on [Binding v1](bindings-v1.md) and the
> FetchRPC v4 stream/lifetime state machines ([FetchRPC v4](../reference/fetchrpc-v4.md)).
> For hands-on usage and a runnable 14-route example covering every `M4A`–`M4D`
> capability, see [Use Binding RPC v2](../guides/use-binding-rpc-v2.md) and
> [examples/binding-rpc-v2](../../examples/binding-rpc-v2/README.md).

## 1. Goals and Compatibility Boundary

Binding v1 established isolated per-Binding QuickJS runtimes on a shared
`uv_loop_t` and bounded structured-value RPC (`NeutralValue`). Binding RPC v2
extends that intra-worker bridge without breaking ABI v8, Host `LOAD_BINDING`,
existing `capsid/binding-v1` packages, or the zero-Binding fast path:

1. **Fair multi-runtime scheduling & immediate dispatch**: replace the v1
   10 ms timer 1-call-per-tick dispatch bottleneck with an immediate,
   reentrancy-guarded fair turn-budget scheduler across the User Runtime and
   every Binding Runtime.
2. **Three-tier quota ledger**: enforce explicit **per-request**,
   **per-Binding**, and **per-worker** limits across inflight calls, active
   streams, active remote object handles, queued frames, and queued bytes.
3. **Structured remote errors, call options & late-result disposal**:
   preserve typed errors (`DOMException`, `QuotaExceededError`, `TypeError`,
   `RangeError`, `SyntaxError`, `URIError`, `Error`) across runtimes; accept
   optional User call options (`{ signal, timeoutMs }` that can only tighten
   the request deadline); deterministically dispose any stream or handle
   returned after cancellation/deadline expiry.
4. **Bidirectional credit-based Stream proxies**: transfer `ReadableStream`
   and `WritableStream` across User ↔ Binding in both directions with explicit
   byte credits, backpressure, cancellation/abort propagation, locked/disturbed
   guards, and strict per-request ownership fencing.
5. **Controlled `Request` & `Response` proxies**: transfer Web `Request` and
   `Response` objects across User ↔ Binding using immutable metadata,
   tamper-proof header extraction, inline small-body fast paths, credit-backed
   streaming bodies, and `AbortSignal` propagation.
6. **Remote object & method stubs (`RpcTarget`)**: allow Binding code to
   explicitly export controlled method stubs via a native-branded `RpcTarget`
   base class while keeping arbitrary non-`RpcTarget` class instances rejected.
   Stubs enforce request-bounded leases, explicit `.dispose()` /
   `[Symbol.dispose]()` / `[Symbol.asyncDispose]()`, cross-request isolation,
   and asynchronous QuickJS GC-finalizer fallback.

Real sockets, file descriptors, SQLite handles, WASI instances, and future
Cache backends remain strictly inside the trusted Binding Runtime. The User
Runtime receives proxies only. When `bindings_` is empty, zero Binding RPC v2
state, timers, or scheduler hooks are activated.

## 2. Modular Component Split

To keep `src/worker_runtime.cc` maintainable, Binding RPC v2 is decomposed
into focused, unit-testable C++ modules:

| Module | Responsibility |
| --- | --- |
| `src/binding_rpc_ledger.{h,cc}` | Pure C++ three-tier quota accounting (per-request / per-Binding / per-worker) for calls, streams, handles, queued frames, and queued bytes, with atomic reserve/release and rollback on partial failure. |
| `src/binding_scheduler.{h,cc}` | Fair turn-budget scheduler and immediate wake-up coordinator across User and Binding runtimes with strict reentrancy guard and per-slice frame/job quanta. |
| `src/binding_rpc.{h,cc}` | Structured clone codec (`NeutralValue` / `RpcValue`), tamper-proof `RemoteErrorInfo` capture/reconstruction, and Binding RPC ID helpers. |
| `src/binding_stream_table.{h,cc}` | Pure C++ bidirectional credit-based stream state machine (`Open` / `HalfClosed` / `Closed` / `Errored` / `Canceled`), byte credit accounting, frame queues, owner-request fencing, and request/binding/worker teardown enumeration. |
| `src/binding_handle_table.{h,cc}` | Pure C++ remote object handle table (`RpcTarget` leases, method allowlists, owner-request fencing, explicit/lease/GC-fallback disposal queue, bounded disposal callbacks). |

## 3. Quotas and Fixed Default Limits (M4A)

| Resource | Per-Request | Per-Binding | Per-Worker |
| --- | ---: | ---: | ---: |
| Inflight RPC calls | 64 | 1,024 | 1,024 |
| Active stream proxies | 16 | 64 | 256 |
| Active remote handles (`RpcTarget`) | 32 | 128 | 512 |
| Queued RPC/stream frames | 512 | 2,048 | 4,096 |
| Queued stream/RPC bytes | 4 MiB | 8 MiB | 16 MiB |

Per-stream defaults:

- Initial byte credit window: `64 KiB` (`65,536` bytes)
- Maximum chunk payload per frame: `64 KiB` (`65,536` bytes)
- Per-stream buffered byte ceiling: `2 MiB` (`2,097,152` bytes)
- Per-stream queued frame ceiling: `256` frames

All quota checks fail closed before cloning payloads or creating handles.
Releasing a call, stream, or handle returns its exact reserved counts across
all three tiers atomically.

## 4. Fair Scheduling and Reentrancy Contract (M4A)

- **Immediate dispatch**: enqueueing onto `user_to_binding`,
  `binding_to_user`, stream queues, or handle disposal queues arms an
  immediate loop wake (`uv_idle_t` / `uv_check_t` on the shared `uv_loop_t`)
  instead of waiting for the 10 ms `deadline_timer_`.
- **Bounded slices**: each scheduler tick executes at most `K_CALLS_PER_TURN`
  (default `16`) RPC call dispatches, `K_FRAMES_PER_TURN` (default `32`)
  stream/handle control frames, and `K_JOBS_PER_RUNTIME_TURN` (default `64`)
  QuickJS pending jobs per runtime, rotating round-robin across
  `[User, Binding_0, ..., Binding_{N-1}]`.
- **Single-runtime entry**: only one `JSContext*` executes at any instant; a
  reentrancy guard prevents recursive scheduler entry from JS callbacks,
  abort listeners, or QuickJS GC finalizers.
- **Zero-Binding invariance**: when `binding_tables_.empty()`, the scheduler
  handle is never initialized or started.

## 5. Call Options, Cancellation, Errors & Late-Result Disposal (M4A)

- **User call signature**: `binding.method(input?, options?)`
  - 0 or 1 argument behaves identically to Binding v1.
  - Optional `options` must be `undefined`, `null`, or a plain object
    supporting `{ signal?: AbortSignal, timeoutMs?: number }` (unknown fields
    or hostile accessors/proxies fail closed).
  - Effective deadline: `min(request.deadline_ns, now + timeoutMs * 1e6)` —
    a caller can tighten a call deadline but can never extend past its
    request deadline.
  - If `options.signal` is already aborted, the call rejects immediately
    without entering the Binding Runtime; if it aborts while queued or
    dispatched, the call cancels and propagates a cloned structured abort
    reason to Binding `call.signal`.
- **Structured remote errors**: `capture_remote_error(ctx, val)` inspects
  genuine `DOMException` / `QuotaExceededError` / native `Error` subclasses
  via brand-checked native getters (never invoking hostile getters or
  user-mutated prototypes) and materializes an authentic typed error in the
  receiving runtime.
- **Late-result disposal**: if a dispatched call is canceled or expired before
  the Binding Promise settles, and the late fulfillment value contains any
  `ReadableStream`, `WritableStream`, `Response`, or `RpcTarget`, the worker
  immediately cancels/closes/disposes those Binding-side resources inside the
  Binding Runtime so nothing leaks.

## 6. Bidirectional Credit-Based Stream Proxies (M4B)

- Both `ReadableStream` and `WritableStream` may appear directly as call
  inputs, return values, or `Request`/`Response` bodies in either direction
  (`User → Binding` or `Binding → User`).
- Source stream validation happens synchronously before transfer:
  - Genuine brand check (`IsReadableStream` / `IsWritableStream`).
  - Rejects locked (`_reader`/`_writer !== undefined`) or disturbed
    (`_disturbed === true`) streams fail-closed.
  - Atomically locks/attaches an internal native reader/writer on the source
    stream using intrinsic operations immune to User prototype tampering.
- Credit flow:
  - Receiver starts with `initial_credit_bytes = 64 KiB`.
  - Producer pulls only while `available_credit > 0` (chunked to `<= 64 KiB`).
  - Receiver grants credit back via `GRANT_CREDIT(bytes)` only when its
    downstream JS consumer actually drains chunks (`pull()` or `write()`).
  - `END`, `ERROR(RemoteErrorInfo)`, and `CANCEL(RemoteErrorInfo)` / `ABORT`
    drive deterministic terminal cleanup on both sides.
- Ownership fencing: User-side stream proxies carry `owner_request_id`. Any
  read/write/cancel from a different request context fails closed with a
  `DOMException("InvalidStateError")`.

## 7. Controlled `Request` and `Response` Proxies (M4C)

- `Request` crossing User ↔ Binding:
  - Captures immutable `url`, `method`, canonical `headers` pairs, `redirect`,
    `credentials`, `mode` (Capsid's Fetch `Request` fixes `referrer` to `null`,
    so reconstructed proxy `Request` objects expose default `referrer === null`).
  - Rejects `bodyUsed` or locked body stream fail-closed.
  - Fast inline body (`<= 64 KiB` unexposed `_bodyFastData`) transfers in one
    copy without consuming a stream slot; streaming bodies attach an M4B
    `ReadableStream` proxy.
  - Propagates `request.signal` abort state/reason.
- `Response` crossing User ↔ Binding:
  - Captures immutable `status`, `statusText`, `headers`, `url`, `redirected`,
    `type`.
  - Rejects `bodyUsed`, locked body stream, or WebSocket-attached `Response`
    fail-closed.
  - Fast inline body or M4B `ReadableStream` proxy; reconstructed `Response`
    exposes immutable headers.

## 8. Remote Object & Method Stubs (`RpcTarget`, M4D)

- **Native-branded `RpcTarget` in Binding Runtime**:
  - Binding Runtime exposes a frozen intrinsic `RpcTarget` constructor whose
    instances carry an unforgeable QuickJS native brand.
  - Ordinary non-`RpcTarget` class instances continue to be rejected by
    structured clone.
  - When a Binding returns an `RpcTarget` instance (at top level or nested in
    a returned value, up to per-request/per-Binding/per-worker handle quotas),
    Capsid validates its public methods (1..128 own or prototype enumerable
    methods on the `RpcTarget` subclass, excluding `constructor`, `prototype`,
    `__proto__`, `then`, `catch`, `finally`, `dispose`, and symbol keys; no
    Proxy, no accessors).
- **User-side Method Stub**:
  - Frozen null-prototype stub object with an internal QuickJS native opaque
    class (`handle_id`, `owner_request_id`, `table_index`).
  - Exposes validated async RPC methods plus `.dispose()`,
    `[Symbol.dispose]()`, and `[Symbol.asyncDispose]()`.
  - Calling a method dispatches an RPC v2 `CALL_HANDLE` invocation carrying
    full M4A–M4C support (options, streams, `Request`/`Response`, nested
    `RpcTarget` returns, structured errors).
- **Leases, Cross-Request Fencing & GC Fallback**:
  - Every handle carries `lease_deadline_ns <= request.deadline_ns`.
  - Calling or passing a handle from a different request context fails closed.
  - Explicit disposal, lease expiry, request completion, worker shutdown, or
    poisoning marks the handle disposed, frees the Binding target reference,
    runs optional Binding-side `[Symbol.dispose]()` / `dispose()` bounded and
    fault-isolated, and releases quota slots.
  - QuickJS finalizer on the User stub enqueues an asynchronous
    `DISPOSE_HANDLE` entry when collected without explicit disposal (never
    re-entering JS from the finalizer).

## 9. Acceptance Matrix

| Milestone | Required Positive & Negative Tests |
| --- | --- |
| **M4A** | Quota ledger unit tests (all 3 tiers, rollback, exhaustion, refill); scheduler fairness (User microtask flood vs Binding, Binding flood vs User, reentrancy guard, sub-ms dispatch latency vs v1 10 ms); structured remote error round-trip (`DOMException`/`QuotaExceededError`/`TypeError`/hostile getters); User `AbortSignal` + tightened `timeoutMs` + rejected deadline extension + late-result disposal. |
| **M4B** | Bidirectional `ReadableStream` & `WritableStream` (User→Binding, Binding→User); slow-consumer credit exhaustion & backpressure resume; cancel/abort propagation with structured reason; locked/disturbed rejection; cross-request stream misuse; request end / Binding error / poison cleanup with zero leaks. |
| **M4C** | Bidirectional `Request` & `Response` (inline fast body, streaming body, immutable headers, `bodyUsed` rejection, WebSocket `Response` rejection, `Request.signal` propagation). |
| **M4D** | `RpcTarget` method calls, nested/chained handles, non-`RpcTarget` class rejection, lease expiry, explicit `.dispose()` / `[Symbol.dispose]()`, GC finalizer fallback, cross-request handle misuse, malicious provider (getters/proxies/CPU loop/post-dispose), ASan/UBSan/TSan + zero-Binding fast-path regression. |
