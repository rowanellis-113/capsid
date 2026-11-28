# Use WebSockets (`FetchRPC v4`)

Capsid supports both **inbound WebSocket servers** (`WebSocketPair` + `101`
Upgrade `Response`) and **outbound WebSocket clients** (`new WebSocket(url, protocols)`)
over Host-mediated **FetchRPC v4** duplex channels with explicit byte-credit
backpressure.

For a complete, runnable 8-route showcase and zero-dependency RFC 6455 verifier,
see [examples/websocket-showcase](../../examples/websocket-showcase/README.md).
Minimal single-file snippets are also available in
[examples/websocket.js](../../examples/websocket.js) and
[examples/websocket-outbound.js](../../examples/websocket-outbound.js). For the
full workerd-differential compatibility contract and transport rules, see
[WebSocket Semantics](../reference/websocket-semantics.md) and
[FetchRPC v4](../reference/fetchrpc-v4.md).

## 1. Inbound WebSockets (`WebSocketPair`)

Create a connected `[client, server]` pair, call `server.accept()` on the
worker-held endpoint, and return the paired `client` endpoint in a `101`
`Response`:

```js
export default {
  fetch(request) {
    if (request.headers.get('upgrade')?.toLowerCase() !== 'websocket') {
      return new Response('Upgrade required', { status: 426 });
    }
    const [client, server] = Object.values(new WebSocketPair());
    server.accept();
    server.binaryType = 'arraybuffer';

    server.addEventListener('message', (event) => {
      server.send(event.data);
    });

    return new Response(null, { status: 101, webSocket: client });
  },
};
```

### Important inbound behaviors

- **Default `binaryType`:** under the pinned workerd `2026-08-20` profile,
  `binaryType` defaults to `"blob"`. Set `server.binaryType = 'arraybuffer'`
  when you want incoming binary frames as `ArrayBuffer` without allocating a
  `Blob`. Invalid assignments are ignored and preserve the current mode.
- **Subprotocol negotiation:** inspect `request.headers.get('sec-websocket-protocol')`
  and include the chosen protocol in the `101` `Response` `headers`.
- **Graceful half-open drain (`allowHalfOpen`):** call
  `server.accept({ allowHalfOpen: true })` to send a final message from the
  `close` handler after observing the peer's clean Close frame before calling
  reciprocal `server.close(1000, 'done')`.
- **Delayed `accept()`:** returning `101` before `server.accept()` buffers early
  incoming frames in order until `accept()` runs (provisional coupling is
  bounded by the earlier of the request deadline and 10 seconds).
- **Retained connection lifetime:** once upgraded, an inbound WebSocket can live
  beyond the initial HTTP response as a bounded Host-owned duplex connection,
  and active WebSocket sessions are exempt from elastic idle eviction.
- **Fail-closed `Response` checks:** coupling a `webSocket` to a non-`101`
  status throws `RangeError`, a non-empty body on `101` is rejected, and
  cloning an upgraded `101` `Response` is rejected.

## 2. Outbound WebSockets (`ws://` / `wss://`)

Outbound WebSockets use Host-mediated DNS, egress checks, TCP/TLS/SNI, and
RFC 6455 handshakes governed by **`permissions.fetch.allow`** in `capsid.json`
(and `permissions.fetchTargets` in managed `host.json`). Outbound access is
denied by default:

```json
{
  "apiVersion": "capsid/app-v2",
  "entry": "bundle.mjs",
  "permissions": {
    "fetch": {
      "allow": ["echo.example.com:443"]
    }
  }
}
```

```js
export default {
  async fetch() {
    const reply = await new Promise((resolve, reject) => {
      const ws = new WebSocket('wss://echo.example.com/ws', ['demo.v1']);
      ws.binaryType = 'arraybuffer';
      ws.onopen = () => ws.send('hello');
      ws.onmessage = (event) => {
        ws.close(1000, 'done');
        resolve(event.data);
      };
      ws.onerror = () => reject(new Error('WebSocket failed'));
    });
    return new Response(reply);
  },
};
```

- Capsid enforces RFC 6455 §4.1 strictly: if a server returns a subprotocol not
  in the offered list, the connection is rejected.
- An unfinished outbound constructor follows its owning HTTP request's lifetime;
  once `open` fires, an established connection can outlive the response body
  within Host channel budgets, but another HTTP request can never reuse its
  native I/O origin.

## 3. HTTP Upgrade fallback (`fetch` with `Upgrade: websocket`)

Calling `fetch(url, { headers: { Upgrade: 'websocket' } })` against an endpoint
that replies with an ordinary HTTP status (for example `200 OK`, including
`Content-Encoding: gzip`) falls back cleanly to a credit-controlled Fetch
response body with transparent streaming gzip decoding. See
[Native runtime behavior](../reference/runtime-behavior.md#upgrade-fallback-gzip).

## 4. Framing, close codes, and transport budgets

- **Host-owned framing:** RFC 6455 masking, fragmentation, and Ping/Pong frames
  are handled by the Host transport; Ping frames never consume JS events.
- **Close codes:** valid JS close codes are `1000`–`4999` excluding `1004`,
  `1005`, `1006`, and `1015`. Invalid received Close codes are rejected with
  `1002`.
- **Finite budgets:** Config v2 defaults to a `1 MiB` maximum WebSocket message
  size, `4 MiB` worker-wide send budget, and `2 MiB` per-channel queue
  (raw C ABI `ChannelLimits` defaults are `1 MiB`, `16 MiB`, and `4 MiB`).
  Per-message compression, hibernation, and cross-worker broadcast are not
  enabled.

## Next steps

- Run and verify every scenario locally with
  [examples/websocket-showcase](../../examples/websocket-showcase/README.md).
- Read the complete compatibility scope in
  [WebSocket Semantics](../reference/websocket-semantics.md).
- Configure egress targets and pool settings in
  [Configure an application](configure-application.md) and
  [Host configuration](../reference/host-configuration.md).
