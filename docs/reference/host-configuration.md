# Host and application configuration

The v0.3.2 Host accepts `capsid/host-v2` in `host.json` and `capsid/app-v2` in `capsid.json`. A Binding package has its own `capsid/binding-v1` manifest. For published v0.3.1 binaries, use the [tagged Host reference](https://github.com/rowanellis-113/capsid/blob/v0.3.1-beta.1/docs/host-config.md).

## Host modes

| | Local | Linux managed |
| --- | --- | --- |
| Start | `capsid-host --app-dir ./dist` | `capsid-host --mode managed --host-config /etc/capsid/host.json --worker /opt/capsid/bin/capsid-worker` |
| Application | One package with a required `capsid.json` and bundle | Versioned packages under `paths.applications/<app>/<version>/` |
| Host policy | Optional `--host-config`, with a single configured listener | Required Host configuration, enforced during deployment and recovery |
| Operations | The listener and pool run for the lifetime of the local Host | Admin socket, generation activation, retirement and recovery |

Local mode finds the worker beside the Host by default. Use `--listen` to select a local address, `--app` to set the application ID, and `--secrets-dir` or `--bindings-dir` to supply local resources. `--workers` overrides the local pool size within its ceiling. See [getting started](../guides/run-application.md#run-the-development-branch).

## host.json

```json
{
  "apiVersion": "capsid/host-v2",
  "paths": {
    "applications": "/srv/capsid/applications",
    "state": "/srv/capsid/state",
    "secrets": "/srv/capsid/secrets/{application}"
  },
  "admin": {
    "unix": "/run/capsid/admin.sock"
  },
  "listeners": [
    {
      "name": "public",
      "tcp": "0.0.0.0:8080",
      "routing": { "mode": "path" }
    }
  ]
}
```

`paths` names the package, state and secret roots. `admin.unix` selects the management socket. Each listener has its own address, routing policy and limits. `permissions` sets the maximum authority an application may request. The Host applies `defaults` and `maximums` to worker, request and pool settings. `capacity` limits shared resources, while `tenancy` configures tenant budgets. See the [complete examples and field tables](../design/config-v2.md#section-3).

## capsid.json

The application declares its entry point, permissions and per-worker and pool settings. A missing manifest fails at startup or deployment. A minimal manifest explicitly opts into the defaults. [Application configuration and example](../guides/configure-application.md#complete-example)

## Secret files

After substituting `{application}` in `paths.secrets`, the result is the secret
directory for that app; the key id referenced by `valueFrom` is the file name inside
that directory:

```sh
# The source is an operator-provided private file; never paste live secrets into shell history.
install -d -m 0700 /srv/capsid/secrets/orders
install -m 0600 /path/to/private/db-url /srv/capsid/secrets/orders/db-url
```

- Key id grammar `[A-Za-z0-9._-]`, no `..`, has a length limit;
- Secret files must be regular files (symlinks are rejected); size/ctime are verified
  before and after reading, and modification in between fails; values enter an
  immutable `capsid:env` snapshot, not the worker process environment.

## Startup and operations

```sh
./build-release/capsid-host --mode managed \
  --host-config /etc/capsid/host.json \
  --worker ./build-release/capsid-worker
```

`--host-config` must be a regular file owned by the Host user (O_NOFOLLOW, euid check,
≤1 MiB, mtime verified before and after reading).

The Admin API only uses a Unix socket (same euid as the Host). A deploy request
requires `app` and `version` and optionally accepts `tenant`. The tenant defaults
to the application ID. The response identifies the app as `application`. Unknown request keys, including `application`, are rejected.

HTTP 202 acknowledges submission, not routing readiness. Query that operation's
`state` and `version` within a finite client deadline; require the matching
operation to reach `active`. A `failed` state or deadline expiry is a failure,
not a reason to repeat the deploy POST. A Host READY record or on-disk
active App status alone does not prove that this version is serving requests.
When validating rollout, check the matching terminal operation and then the
expected data-plane response without concealing failures through retries.

```sh
# Deploy (blue-green: staging/warm-up/health check first, atomic switch, old version kept on failure)
curl --unix-socket /run/capsid/admin.sock \
  -X POST http://localhost/v1/deploy \
  -H 'Content-Type: application/json' \
  -d '{"app":"orders","version":"v2"}'
# → 202 {"operationId":"...","application":"orders","version":"v2","status":"..."}

# Query the exact operationId returned above (replace this value)
operation_id='replace-with-the-returned-operationId'
curl --unix-socket /run/capsid/admin.sock \
  "http://localhost/v1/operations/${operation_id}"

# App status
curl --unix-socket /run/capsid/admin.sock http://localhost/v1/apps/orders

# Explicitly retire (management action; expressed as a tombstone, not by deleting the directory)
curl --unix-socket /run/capsid/admin.sock \
  -X POST http://localhost/v1/apps/orders/retire

# Metrics
curl --unix-socket /run/capsid/admin.sock http://localhost/metrics
```

All endpoint responses are bounded JSON/text; unknown paths return 404, non-deploy
requests with a body return 400, wrong methods return 405, and unauthorized peers
return 403. The HTTP/1 transport accepts fragmented headers and fixed-length or
chunked request bodies, and sends one response with `Connection: close`.
Header and decoded-body ceilings default to 64 KiB each; header and body phase
deadlines do not restart on each fragment. Pending wire metadata is bounded by
`max(max_header_bytes, 4096)` bytes, including unfinished chunk extensions.
Invalid framing returns 400 without dispatch; header/body limit violations
return 431/413 respectively.

## Request routing

The `routing` object currently defines and reads only `mode` and `suffix`; `mode`
supports three values:

| mode | App source | Constraint |
| --- | --- | --- |
| `path` | The `/@capsid/<app>/` prefix in the URL | Strip the routing prefix, then hand the path to the worker |
| `subdomain` | The single DNS label before `suffix` in the Host | `suffix` must be a valid, portless domain starting with a dot, such as `.apps.example.com` |
| `header` | The sole `Capsid-App` request header | The listener must explicitly set `trusted: true`; duplicate or invalid App IDs are rejected |

Each mode requires exactly one valid Host header. `path` and `header` use the
listener's `publicOrigin` to construct the worker-visible URL. For subdomain routing
the Host uses the selected scheme, extracted application and configured suffix. The
`Capsid-App` header name is fixed.

`trusted: true` is an explicit declaration of a security boundary in front of the
listener, not TLS, authentication, or a firewall automatically established by Capsid.
Before enabling header routing, deployers must ensure that only a controlled reverse
proxy or trusted source can reach the listener, and that the proxy removes or
overwrites `Capsid-App` carried by external requests. The typical approach is binding
to loopback/controlled internal network addresses with network ACLs; simply setting
`trusted: true` after exposing the listener to the public Internet is not safe. A
header listener with `trusted: false` fails closed before bind; path and subdomain
routing do not require that declaration.

External reverse proxies (nginx/Caddy/Envoy) handle TLS/H2 termination and external
path, Host, or header mapping. To exercise a managed listener's routing policy
locally, provide it through `--host-config`.

## Listener CORS

The optional `listeners[].cors` object makes the listener itself answer
browser CORS preflights — **before routing** — and stamp the matched
`Access-Control-Allow-Origin` on every response the listener serves. It is an
edge/trust-boundary concern, exactly like `trusted`, and exists because a
preflight can never carry the header-routing control field: the custom header
is precisely what the preflight asks about, so without listener-level CORS
header routing is unreachable from any CORS-enforcing browser client.

| Field | Required | Description |
| --- | --- | --- |
| `allowedOrigins` | ✓ | `"*"` (any origin) or exact `http(s)://host[:port]` origins; `"*"` cannot be mixed with exact entries; scheme/host matching is ASCII case-insensitive |
| `allowedMethods` | ✓ | HTTP method tokens; matched case-insensitively |
| `allowedHeaders` | ✓ | Header names; matched case-insensitively |
| `maxAge` | | Preflight cache lifetime (`Access-Control-Max-Age`); omitted = no caching hint |

Semantics:

- `OPTIONS` with `Origin` and `Access-Control-Request-Method` is a preflight:
  the listener answers it itself. Origin, requested method and every requested
  header must match the config; a match is `204` with
  `Access-Control-Allow-Origin` (the echoed origin), `Allow-Methods`,
  `Allow-Headers`, `Vary: Origin` and the optional `Max-Age`. A mismatch is
  `403` with **no** `Access-Control-Allow-*` field — the browser reports the
  CORS failure and no CORS decision leaks.
- Any other request records whether its `Origin` is allowed; both response
  paths (Host-synthesized and worker responses) are then normalized by the
  listener. When `cors` is configured the listener owns the
  `Access-Control-Allow-Origin` field: an App-supplied value is removed and
  replaced with the matched Origin (allowed requests) or removed entirely
  (disallowed requests), so the Host allow-list cannot be bypassed by App
  headers. `Access-Control-Allow-Credentials` survives only for an exact
  allowed origin; wildcard `"*"` and disallowed/absent origins strip it, so
  wildcard can never become any-origin credentialed CORS. `Vary` is merged
  token-wise with `Origin`; an App-supplied `Vary: Accept-Encoding` cannot
  suppress the required `Vary: Origin`.
- A duplicate `Origin` header is malformed control input and is rejected
  with `400` before routing.
- An `OPTIONS` without the preflight fields is not a preflight and routes
  normally (the App may handle it).
- Absent `cors`, nothing changes: the App owns CORS entirely.
