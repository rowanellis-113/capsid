# Use Binding RPC v2 (`v0.4.0`)

Binding RPC v2 runs Host-trusted capability packages in isolated QuickJS
**Binding Runtimes** inside the worker process while untrusted application code
runs in the **User Runtime**. The two sides share no JavaScript heap, global
object, module cache, or native handle. Applications with zero Bindings keep the
original single-runtime path with zero extra memory or scheduler overhead.

This guide walks through configuring, authoring, and calling Binding RPC v2
packages. For the complete runnable example covering every feature below, see
[examples/binding-rpc-v2](../../examples/binding-rpc-v2/README.md). For
normative protocol and policy details, see
[Binding RPC v2 Technical Design](../design/binding-rpc-v2.md),
[Binding v1](../design/bindings-v1.md), and
[Binding Modules and Permissions](../reference/binding-permissions.md).

## 1. Package layout and configuration

Each Binding package lives in its own directory under the Host Binding registry
(`--bindings-dir` locally, or `paths.bindings` in managed `capsid/host-v2`). The
directory name is the Binding ID across registry, `capsid.json`, and
`import ... from "capsid:binding/<id>"`. Each directory must contain **only**
`manifest.json` and one self-contained ESM `index.js`:

```text
bindings/
└── vault/
    ├── manifest.json     # capsid/binding-v1 maximum authority
    └── index.js          # trusted Binding factory
app/
├── capsid.json           # capsid/app-v2 declaring bindings.vault
└── bundle.mjs            # untrusted application bundle
secrets/
└── signing-key           # regular file referenced by valueFrom
```

### Binding manifest (`bindings/vault/manifest.json`)

```json
{
  "apiVersion": "capsid/binding-v1",
  "permissions": {
    "modules": ["capsid:hashing", "capsid:internal/core", "capsid:utils", "capsid:uuid"],
    "net": { "allow": [] },
    "fs": { "read": [], "write": [] },
    "env": [],
    "stdio": []
  }
}
```

Every direct and transitive `capsid:*` module must be listed in
`permissions.modules`. Add `sandbox.requires` (such as `network-client`,
`filesystem-read`, `filesystem-write`, `sqlite`, or `wasi`) only when the
package uses matching resources; see
[Binding Modules and Permissions](../reference/binding-permissions.md).

### Application manifest (`app/capsid.json`)

```json
{
  "apiVersion": "capsid/app-v2",
  "entry": "bundle.mjs",
  "bindings": {
    "vault": {
      "config": { "namespace": "orders" },
      "secrets": {
        "signingKey": { "valueFrom": "signing-key" }
      }
    }
  }
}
```

The application can only narrow `permissions.net`, `permissions.fs`, and
`permissions.stdio` below the Binding manifest maximum. Secrets must use
`valueFrom` (inline secret values are rejected); secret plaintext stays inside
the Binding Runtime and is redacted from authenticated Binding logs.

### Run locally with `capsid-host`

```sh
capsid-host \
  --app-dir ./app \
  --bindings-dir ./bindings \
  --secrets-dir ./secrets \
  --listen 127.0.0.1:8080 \
  --ready-fd 1
```

## 2. Authoring a Binding (`index.js`)

The default export is a **synchronous** factory receiving deeply frozen
`{ config, secrets, log }` and returning an object of 1–128 public methods. Each
method receives `(input, call)` where `call` is frozen and exposes
`call.requestId`, `call.deadline`, and a Binding-side `call.signal`:

```js
import { createHash } from 'capsid:hashing';

export default function createVault({ config, secrets, log }) {
  return {
    sign(payload, call) {
      const h = createHash('sha256');
      h.update(new TextEncoder().encode(`${config.namespace}:${secrets.signingKey}:${payload}`));
      log.info('signed payload', { requestId: String(call.requestId) });
      return h.digest();
    },
  };
}
```

User code imports the generated frozen facade and awaits calls inside an active
request context:

```js
import vault from 'capsid:binding/vault';

export default {
  async fetch(request) {
    const digest = await vault.sign(await request.text(), { timeoutMs: 2000 });
    return Response.json({ digest });
  },
};
```

## 3. Scheduling, call options, errors, and quotas (`M4A`)

### Fair shared-loop scheduling

Binding RPC v2 dispatches calls immediately across a reentrancy-guarded fair
scheduler rotating round-robin across the User Runtime and every Binding
Runtime with bounded per-turn call, control-frame, and QuickJS job slices. A
microtask storm in one Binding cannot head-of-line starve another Binding or the
User Runtime.

### Call options (`{ signal, timeoutMs }`)

Every Binding method (and every `RpcTarget` stub method) accepts an optional
second argument `options`:

```js
const ac = new AbortController();
const result = await vault.fetchRecord(id, {
  signal: ac.signal,
  timeoutMs: 500,
});
```

- `timeoutMs` can only **tighten** the current HTTP request deadline; it cannot
  extend past it. `timeoutMs: 0` rejects immediately with
  `DOMException(..., "TimeoutError")` without entering the Binding.
- A pre-aborted `signal` rejects immediately without entering the Binding.
  Aborting in-flight propagates a cloned structured reason to Binding
  `call.signal`.
- Unknown keys, negative `timeoutMs`, non-`AbortSignal` values, or getter
  properties on `options` fail closed with `TypeError`.
- **Late-result disposal:** if a call times out or aborts before the Binding
  Promise settles, any `ReadableStream`, `WritableStream`, `Request`,
  `Response`, or `RpcTarget` inside the late resolution graph is canceled or
  disposed automatically inside the Binding Runtime (cycle-safe and
  deduplicated).

### Structured remote errors

Genuine `Error`, `TypeError`, `RangeError`, `SyntaxError`, `URIError`,
`DOMException` (preserving `.name`), and `QuotaExceededError` (preserving
`.quota` and `.requested`) round-trip cleanly into authentic error instances in
the caller's runtime. Hostile getters and `Proxy` throws fail closed to a generic
`Error("binding call failed")` without executing untrusted accessors.

### Three-tier quota ledger

Quotas are reserved atomically before cloning payloads or creating handles and
released deterministically when calls, streams, or handles finish:

| Resource | Per-Request | Per-Binding | Per-Worker |
| --- | ---: | ---: | ---: |
| Inflight RPC calls | 64 | 1,024 | 1,024 |
| Active stream proxies | 16 | 64 | 256 |
| Active remote handles (`RpcTarget`) | 32 | 128 | 512 |
| Queued RPC/stream frames | 512 | 2,048 | 4,096 |
| Queued stream/RPC bytes | 4 MiB | 8 MiB | 16 MiB |

## 4. Bidirectional credit-based Stream proxies (`M4B`)

Both `ReadableStream` and `WritableStream` transfer in either direction
(`User → Binding` and `Binding → User`) as call inputs, return values, or
`Request`/`Response` bodies:

- **Credit backpressure:** each stream starts with a `64 KiB` credit window and
  chunks frames to at most `64 KiB` (automatically slicing larger writes such as
  a `200 KiB` `Uint8Array`). Per-stream staging is capped at `2 MiB` and `256`
  frames. Credit refills only when the downstream consumer drains data.
- **Cancel, abort, and error propagation:** `reader.cancel(reason)` and
  `writer.abort(reason)` propagate structured reasons across the boundary; source
  or sink errors preserve typed error subclasses.
- **Fail-closed guards:** locked or disturbed streams are rejected before
  transfer. If a composite argument fails validation, earlier valid streams or
  HTTP bodies in the same batch roll back intact.
- **Cross-request ownership fencing:** User-side stream proxies carry their
  owning `request_id`. Any read, write, `getReader()`, `getWriter()`, `tee()`,
  `pipeTo()`, `cancel()`, `abort()`, or re-transfer from a different request
  rejects with `DOMException("InvalidStateError")` without mutating the owner's
  stream.

```js
// Direct Binding -> User -> HTTP client streaming passthrough
export default {
  async fetch() {
    const stream = await vault.openEventStream(100);
    return new Response(stream, {
      headers: { 'content-type': 'application/x-ndjson' },
    });
  },
};
```

## 5. Controlled `Request` and `Response` proxies (`M4C`)

Web `Request` and `Response` objects (including genuine subclasses) cross
`User ↔ Binding` safely:

- **Inline vs streaming bodies:** unconsumed fixed bodies `<= 64 KiB` use an
  inline fast path without consuming a stream slot; streaming bodies (or bodies
  `> 64 KiB`) automatically attach an `M4B` credit-backed `ReadableStream` proxy.
- **Immutable headers & `Set-Cookie`:** multi-value `Set-Cookie` headers
  round-trip intact (`headers.getSetCookie()`). Returned proxy `Response`
  headers and `Response.error()` headers (in both directions) are immutable.
- **`Request.signal` propagation:** pre-aborted, queued-aborted, and live
  aborts on `request.signal` propagate to the receiving runtime's `Request`.
- **Fail-closed rejections:** already-consumed (`bodyUsed`), locked-body,
  prototype-spoofed, and WebSocket-attached `101` `Response` objects are
  rejected. Rejected composite calls roll back without marking valid bodies as
  used.

```js
// Return a Binding-constructed streaming Response directly from fetch()
export default {
  async fetch(request) {
    return await vault.handleProxyRequest(request);
  },
};
```

## 6. Remote object handles (`RpcTarget`, `M4D`)

Binding code can export stateful remote objects by subclassing the native-branded
`RpcTarget` intrinsic (exposed **only** in Binding Runtimes, never in the User
Runtime):

```js
// Inside Binding Runtime (bindings/vault/index.js)
class DocumentSession extends RpcTarget {
  constructor(label, leaseMs) {
    super(leaseMs !== undefined ? { leaseMs } : undefined);
    this.label = label;
    this.items = [];
  }
  append(item) {
    this.items.push(String(item));
    return this.items.length;
  }
  [Symbol.dispose]() {
    // Bounded, fault-isolated cleanup hook
    this.items.length = 0;
  }
}
```

On the User side, Capsid returns a **frozen, null-prototype method stub**:

```js
const session = await vault.openSession('draft', { timeoutMs: 1000 });
try {
  await session.append('line-1');
  await session.append('line-2');
} finally {
  session.dispose(); // or session[Symbol.dispose]() / await session[Symbol.asyncDispose]()
}
```

Key invariants:

- **Explicit opt-in only:** ordinary non-`RpcTarget` class instances, `Proxy`
  wrappers, and targets with getter/setter properties are rejected fail-closed.
  Exported methods are snapshotted at export time so later mutation cannot
  hijack calls.
- **Composable:** handle methods support `{ signal, timeoutMs }`, streams,
  `Request`/`Response` proxies, structured errors, and returning nested/chained
  `RpcTarget` handles.
- **Leases and disposal:** every handle is bounded by
  `min(leaseMs, request deadline)` and automatically disposed on explicit
  `.dispose()` / `[Symbol.dispose]()` / `[Symbol.asyncDispose]()`, lease expiry,
  request completion, or asynchronous QuickJS GC-finalizer fallback.
  Binding-side `[Symbol.dispose]()` hooks are fault-isolated and CPU-bounded.
- **Ownership fencing:** handles belong strictly to their creating request and
  cannot be invoked, disposed, or passed back to a Binding from another request
  (`DOMException("InvalidStateError")`).

## Next steps

- Run the full 14-route showcase and verifier in
  [examples/binding-rpc-v2](../../examples/binding-rpc-v2/README.md).
- Review security boundaries in
  [Binding Modules and Permissions](../reference/binding-permissions.md) and
  [Linux Sandbox](../reference/linux-sandbox.md).
- Read the wire/internal design in
  [Binding RPC v2 Technical Design](../design/binding-rpc-v2.md).
