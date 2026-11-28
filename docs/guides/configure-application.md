# Configure an application

For v0.3.2, put a UTF-8 `capsid.json` beside the application's self-contained ESM bundle. Local startup and managed deployment both require the manifest. Start with the following file to run `bundle.mjs` with no module or network grants.

```json
{ "apiVersion": "capsid/app-v2" }
```

Use `entry` to select another bundle in the package. The Host rejects duplicate keys, unknown fields and unsupported schema versions. To start the application locally, run `capsid-host --app-dir .`. For the published beta, use its [tagged application guide](https://github.com/rowanellis-113/capsid/blob/v0.3.1-beta.1/docs/capsid-json.md). [Run the development branch](run-application.md#run-the-development-branch)

## Module permissions (`permissions.modules`)

For every `capsid:` module the app `import`s, you must list it here, otherwise
the import fails directly (`module is not authorized`):

```json
{
  "permissions": {
    "modules": ["capsid:env", "capsid:fs", "capsid:storage", "capsid:stdio"]
  }
}
```

Available modules are listed in the
[Modules and Permissions Reference](../reference/module-permissions.md). Rules:

- Only public `capsid:*` names are accepted; writing `tjs:*` or `tjs:internal/*`
  is **directly rejected** — these are permanently forbidden, not open items;
- Pure utility modules (`capsid:assert`, `capsid:hashing`, etc.) must also be
  listed individually;
- Standard `fetch()` does not go through this section — outbound networking uses
  `permissions.fetch` below.

## Environment variables (`permissions.env`)

The environment variables the app reads **come only from here** (the worker
process environment is cleared). There are two sources, **exactly one of which
must be chosen**:

```json
{
  "permissions": {
    "env": {
      "APP_MODE": { "value": "production" },
      "DATABASE_URL": { "valueFrom": "db-url" }
    }
  }
}
```

- `value`: write a literal value directly;
- `valueFrom`: references a secret file. The key id `db-url` corresponds to the
  regular file under the resolved `paths.secrets` directory. The local Host reads
  application secrets from `--secrets-dir`. [Secret files](../reference/host-configuration.md#secret-files)
- Key name grammar: **starts with a letter or `_`**, and subsequent characters
  may only be letters, digits, or `_` (no `*`, and it cannot start with a
  digit) — `1APP_MODE` and `APP*MODE` are rejected;
- The same key **cannot have both `value` and `valueFrom`**; writing both or
  neither is rejected;
- Environment value sizes are subject to runtime constraints; oversized values
  fail during snapshot compilation.

## Read-only filesystem (`permissions.fs`)

`capsid:fs` can only read, not write. `allow` is the authorization root, and
`deny` takes precedence over `allow`:

```json
{
  "permissions": {
    "fs": {
      "read": {
        "allow": ["/srv/capsid/config"],
        "deny": ["/srv/capsid/config/private.json"]
      }
    }
  }
}
```

- Paths must be canonical absolute paths; under strict sandbox an authorization
  root cannot be a symlink;
- In managed mode, the root must also appear in host.json's
  `permissions.fsReadRoots`. A local `--host-config` applies the same Host ceiling.

## Outbound network (`permissions.fetch`)

Controls application Fetch egress; the Host-mediated outbound WebSocket/v4
path also uses effective application egress authority. It does not expose a raw
socket or listener. The syntax is `host` or
`host:p1,p2` (port list):

```json
{
  "permissions": {
    "fetch": {
      "allow": ["api.example.com:443", "metrics.example.com:443"]
    }
  }
}
```

- Every request checks the hostname, **every address** returned by DNS
  resolution, and **every redirect**;
- In managed mode, the target must also be allowed by host.json's
  `permissions.fetchTargets`; the local Host also applies these limits when given `--host-config`;
- Omitting this section denies outbound Fetch. At the C API layer, a null
  `egress_policy` denies Fetch when `capability_policy` (or its `net_policy`)
  is absent; when `capability_policy->net_policy` has grants and
  `egress_policy` is `NULL`, `net_policy` governs; when both are non-null,
  both must allow the request.

## In-memory storage and logging (`permissions.storage` / `permissions.stdio`)

```json
{
  "permissions": {
    "storage": { "namespaces": ["session"] },
    "stdio": ["stdout", "stderr"]
  }
}
```

- `storage.namespaces`: accepts only ASCII letters, digits, `_`, `-`, `.`, up to
  128 characters; each namespace has an independent quota and lives only in a
  single worker;
- `stdio`: accepts only the three strings `stdin`/`stdout`/`stderr`;
  `capsid:stdio` only emits bounded stdout/stderr log events and never touches
  real fds. A parsed `stdin` grant does not implement a stdin API.

## Resource limits, timeouts and queues

Set `worker.jsHeap` and `worker.memoryMax` with size units. After inheritance, the memory limit must accommodate the JS heap. Set `request.timeout` with a duration and `request.maxInflight` with a count per worker. The built-in request values are `30s` and `64`. A Host default supplies an omitted value, and a Host maximum bounds the result. Deployment rejects an explicit application setting above its ceiling.

Use `pool.workers` for a fixed fleet, or set `pool.minReady` and `pool.maxWorkers` for an elastic fleet. An application that can start cold can request `minReady: 0`. `pool.queueRequests` bounds queued requests and `pool.queueTimeout` bounds their wait. The listener owns the stream-idle and write deadlines. [Config v2 field reference](../design/config-v2.md#section-4)

## Complete example

```json
{
  "apiVersion": "capsid/app-v2",
  "entry": "bundle.mjs",
  "permissions": { "fetch": { "allow": ["api.example.com:443,8443"] } },
  "worker": { "jsHeap": "64MiB", "memoryMax": "256MiB" },
  "request": { "timeout": "5s", "maxInflight": 16 },
  "pool": { "minReady": 1, "maxWorkers": 4, "queueRequests": 32, "queueTimeout": "2s" }
}
```

Each requested target must fit the Host's egress ceiling. For `bindings.<id>.permissions.net.allow`, every requested port must also be granted by the installed Binding's manifest. The application can use one comma-separated list for ports granted by separate manifest entries. For outbound WebSocket connections, `permissions.fetch.allow` governs the allowed `ws://` / `wss://` host and port targets; see [Use WebSockets](use-websockets.md).

## Trusted Bindings (`bindings.<id>`)

Declare Host-installed `capsid/binding-v1` packages under `bindings`, keyed
directly by their Binding ID (matching the directory name under `--bindings-dir`
or `paths.bindings` and the User import `capsid:binding/<id>`):

```json
{
  "apiVersion": "capsid/app-v2",
  "entry": "bundle.mjs",
  "bindings": {
    "vault": {
      "config": { "namespace": "orders" },
      "secrets": { "signingKey": { "valueFrom": "signing-key" } }
    }
  }
}
```

- `config` is an opaque JSON object (up to 256 KiB) deeply frozen inside the
  Binding Runtime;
- `secrets` accepts only `{"valueFrom":"<secret-key>"}` references resolved from
  `--secrets-dir` or `paths.secrets`; inline secret literals are rejected;
- `permissions` (`net`, `fs`, `env`, `stdio`) may only narrow the Binding
  manifest maximum; omitting a class denies it;
- on `dev/v0.4.0`, Bindings execute over **Binding RPC v2** with fair
  scheduling, 3-tier quotas, bidirectional credit-based streams, controlled
  `Request`/`Response` proxies, and `RpcTarget` remote object handles.

See [Use Binding RPC v2](use-binding-rpc-v2.md),
[examples/binding-rpc-v2](../../examples/binding-rpc-v2/README.md),
[Binding packages and permissions](../design/bindings-v1.md), and
[Binding RPC v2 design](../design/binding-rpc-v2.md).

## Deployment in three steps

```sh
# 1. Put capsid.json and the bundle into the version directory (app id: starts with
#    a lowercase letter/digit, [a-z0-9._-], ≤63 chars; version id: starts with an
#    ASCII letter/digit, [A-Za-z0-9._-], ≤128 chars, cannot contain "..")
mkdir -p /srv/capsid/applications/orders/v2
cp capsid.json bundle.mjs /srv/capsid/applications/orders/v2/

# 2. Deploy through the Admin API (Unix socket, same euid as the Host; blue-green:
#    prewarm + health check, atomic switch, failure keeps the old version)
curl --unix-socket /run/capsid/admin.sock \
  -X POST http://localhost/v1/deploy \
  -H 'Content-Type: application/json' \
  -d '{"app":"orders","version":"v2"}'

# 3. Check that exact returned operation, within a finite client deadline
operation_id='replace-with-the-returned-operationId'
curl --unix-socket /run/capsid/admin.sock \
  "http://localhost/v1/operations/${operation_id}"
# Require the matching version and state=active before the first expected request.
# A failed state/deadline is a failure, not a reason to repeat the deploy POST.
```

At deployment time, if the bundle directory also contains `bundle.qjsb`,
`bytecode.json`, and `bytecode.sig`, the trusted bytecode path is used — these
three files are **all-or-nothing**; missing any one rejects deployment.
