# Binding RPC v2 Showcase (`v0.4.0`)

A zero-dependency, directly runnable Config v2 (`capsid/app-v2`) application
that exercises every **Binding RPC v2** capability (`M4A`–`M4D`) across two
isolated QuickJS Binding Runtimes (`vault` and `telemetry`) and one untrusted
User Runtime.

Read the practical developer walkthrough in
[Use Binding RPC v2](../../docs/guides/use-binding-rpc-v2.md) and the normative
contract in [Binding RPC v2 Technical Design](../../docs/design/binding-rpc-v2.md).

## Directory layout

```text
examples/binding-rpc-v2/
├── app/
│   ├── capsid.json          # capsid/app-v2 manifest binding vault + telemetry
│   └── bundle.mjs           # User Runtime fetch(request) handler (14 routes)
├── bindings/
│   ├── vault/               # Trusted Binding #1 (secrets, streams, Request/Response, RpcTarget)
│   │   ├── manifest.json    # capsid/binding-v1 (capsid:hashing, internal/core, utils, uuid)
│   │   └── index.js
│   └── telemetry/           # Trusted Binding #2 (fair multi-Binding scheduling peer)
│       ├── manifest.json    # capsid/binding-v1 (capsid:utils)
│       └── index.js
├── secrets/
│   └── signing-key          # Secret file loaded via valueFrom (never exposed to User JS)
└── verify.py                # Zero-dependency Python 3 end-to-end route verifier
```

## Run locally

Build or install Capsid from `dev/v0.4.0` so `capsid-host` and `capsid-worker`
are on your `PATH`.

> **Ownership note (Linux / WSL):** `capsid-host` verifies that `app/`,
> `bindings/`, and `secrets/` are owned by your user and not group/world
> writable. In WSL, copy `examples/binding-rpc-v2` to a native Linux directory
> (for example `~/capsid-binding-v2`) with `chmod -R go-w` before running.

```sh
capsid-host \
  --app-dir ./examples/binding-rpc-v2/app \
  --bindings-dir ./examples/binding-rpc-v2/bindings \
  --secrets-dir ./examples/binding-rpc-v2/secrets \
  --listen 127.0.0.1:8080 \
  --ready-fd 1
```

In a second terminal, run the end-to-end verifier against every route:

```sh
python3 ./examples/binding-rpc-v2/verify.py --url http://127.0.0.1:8080
```

Or probe individual routes with `curl`:

```sh
curl -sS http://127.0.0.1:8080/m4a/fair-scheduling
curl -sS http://127.0.0.1:8080/m4b/stream-passthrough
curl -sS http://127.0.0.1:8080/m4c/http-proxies
curl -sS http://127.0.0.1:8080/m4d/rpc-target
```

## Capability matrix covered by routes

| Route | Milestone | Capabilities verified |
| --- | --- | --- |
| `GET /` | Overview | Lists all showcase routes |
| `GET /m4a/fair-scheduling` | M4A | Immediate round-robin dispatch across `vault` and `telemetry`; microtask-storm preemption immunity |
| `GET /m4a/errors` | M4A | Structured remote errors (`TypeError`, `RangeError`, `SyntaxError`, `URIError`, `DOMException`, `QuotaExceededError`) plus tamper-proof rejection of hostile getters and `Proxy` throws |
| `GET /m4a/options` | M4A | `{ signal, timeoutMs }` call options, pre-aborted signals, `timeoutMs: 0`, strict option validation, in-flight abort reason propagation, per-call timeout preserving parent request, secret-backed `capsid:hashing` + redacted logs |
| `GET /m4a/quotas` | M4A | Per-request queued RPC byte quota (`4 MiB`) and active `RpcTarget` handle quota (`32`) with atomic refill after settlement/disposal |
| `GET /m4b/readable-stream` | M4B | Binding→User 320 KiB credit backpressure, User→Binding 150 KiB credit-sliced upload hashing, structured cancel reason, `RangeError` stream error propagation, locked/disturbed/rollback guards, late-result stream cancellation |
| `GET /m4b/writable-stream` | M4B | Binding→User slow sink 200 KiB sliced writes, `2 MiB` staged cap fail-closed, User→Binding sink writes, `TypeError` sink error, structured `abort()` reason, locked writable rejection |
| `GET /m4b/stream-passthrough` | M4B | Direct `new Response(await vault.openEventStream(6))` NDJSON streaming to HTTP client |
| `GET /m4c/http-proxies` | M4C | Inline (`<= 64 KiB`) and streaming (`90 KiB`) `Request` proxies, `Request` subclass, pre/queued/live `Request.signal` aborts, inline `Response` + `getSetCookie()` + immutable headers, `Response` subclass, `204`, bidirectional immutable `Response.error()`, atomic rollback on composite input failure, locked/used/spoofed/WebSocket-101 rejections |
| `GET /m4c/proxy-response` | M4C | Direct `return await vault.streamReport()` (`80 KiB` streaming `Response` proxy returned from `fetch()`) |
| `GET /m4d/rpc-target` | M4D | Native-branded `RpcTarget` (absent in User Runtime), frozen null-prototype stubs, stateful methods, nested `QueryCursor` handle, stream & `Response` returns, method snapshot immunity, `leaseMs` expiry, `.dispose()` / `[Symbol.dispose]()` / `[Symbol.asyncDispose]()`, fault-isolated throwing dispose, GC finalizer fallback, cycle-safe deduped late-result disposal, non-`RpcTarget` / getter / `Proxy` / pass-back rejections |
| `GET /m4d/fencing-hold` & `/m4d/fencing-steal` | M4B + M4D | Concurrent cross-request ownership fencing for `ReadableStream`, `WritableStream`, and `RpcTarget` handles (`InvalidStateError` without mutating owner state) |
| `GET /m4d/request-end-leak` & `/m4d/request-end-verify` | M4D | Deterministic request-end disposal of undisposed `RpcTarget` handles |
