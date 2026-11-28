# WebSocket Showcase (`FetchRPC v4`)

A zero-dependency, directly runnable Config v2 (`capsid/app-v2`) application
that exercises Capsid's inbound `WebSocketPair`, outbound `new WebSocket()`,
`allowHalfOpen`, delayed `accept()`, `Blob` vs `ArrayBuffer` binary modes,
subprotocol negotiation, fail-closed API guards, and non-101 gzip HTTP Upgrade
fallback.

Read the practical developer guide in
[Use WebSockets](../../docs/guides/use-websockets.md) and the full compatibility
specification in
[WebSocket Semantics](../../docs/reference/websocket-semantics.md). For minimal
single-file snippets, see [websocket.js](../websocket.js) and
[websocket-outbound.js](../websocket-outbound.js).

## Run locally

> **Ownership note (Linux / WSL):** `capsid-host` verifies that the application
> directory and `capsid.json` are owned by your user and not group/world
> writable. In WSL, copy `examples/websocket-showcase` to a native Linux
> directory before running.

```sh
capsid-host \
  --app-dir ./examples/websocket-showcase \
  --listen 127.0.0.1:8080 \
  --ready-fd 1
```

In a second terminal, run the zero-dependency Python 3 verifier (which starts a
local RFC 6455 echo & gzip fallback peer on `127.0.0.1:18099`, matching
`capsid.json`'s `permissions.fetch.allow`):

```sh
python3 ./examples/websocket-showcase/verify.py --url http://127.0.0.1:8080
```

## Scenarios covered

| Route | Capabilities verified |
| --- | --- |
| `GET /` | Lists all WebSocket showcase routes |
| `WS /ws/echo` | Inbound `WebSocketPair` + `101` Upgrade, `binaryType` switch (`blob` default → `arraybuffer`), text & 32 KiB binary round-trip, Host-handled Ping/Pong, custom close code/reason (`4001`) |
| `WS /ws/subprotocol` | `Sec-WebSocket-Protocol` offer selection and `101` response header echo |
| `WS /ws/half-open` | `server.accept({ allowHalfOpen: true })` delivering a final message after client Close before reciprocating Close |
| `WS /ws/delayed-accept` | Returning `101` before calling `server.accept()` while buffering early client frames in order |
| `WS /ws/blob-mode` | Default `binaryType === "blob"` binary message delivery with WebCrypto `SHA-256` digest |
| `GET /ws/outbound` | Outbound `new WebSocket(url, [protocol])` text + binary echo under `permissions.fetch.allow`, plus fail-closed egress denial for ungranted ports |
| `GET /ws/upgrade-fallback` | `fetch()` with `Upgrade: websocket` receiving an ordinary gzip-encoded `200 OK` fallback response |
| `GET /ws/error-cases` | Fail-closed guards: non-101 with `webSocket`, non-empty `101` body, cloning upgraded `Response`, invalid close codes (`1005`/`1006`), ignored invalid `binaryType` |
