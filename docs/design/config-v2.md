# Config v2 and multi-tenant elastic worker pools

> This design is shipped in `v0.3.2-beta.1` / `dev/v0.3.2`. It defines Host and application configuration, elastic pools and tenant budgets. The [v0.3.2 acceptance record](../history/v0.3.2/README.md) records verified checks and gates.
> For the earlier `v0.3.1-beta.1`, use the [tagged Host configuration reference](https://github.com/rowanellis-113/capsid/blob/v0.3.1-beta.1/docs/host-config.md) and [tagged application configuration guide](https://github.com/rowanellis-113/capsid/blob/v0.3.1-beta.1/docs/capsid-json.md). [Release and measurement provenance](../reference/release-status.md)

---

## Contents

- **Phase 1. Configuration and local development**
  - [1. Problems in the earlier configuration and CLI](#section-1)
  - [2. Field ownership, units and inheritance](#section-2)
  - [3. Minimal and full configuration examples](#section-3)
  - [4. Capacity scopes and Host field reference](#section-4)
  - [5. Use the managed package layout locally](#section-5)
- **Phase 2. Elastic pools and memory budgets (v0.3.2+)**
  - [6. Architecture comparison with workerd](#section-6)
  - [7. Tenant fairness and two-stage eviction](#section-7)
  - [8. Engineering constraints](#section-8)
  - [9. Application and Host settings for elastic pools](#section-9)
  - [10. Configuration retirement and delivery milestones](#section-10)

---

# Phase 1. Configuration and local development

> Phase 1 covers the capabilities already delivered at the baseline: fixed-size pools, Bindings v1, FetchRPC v4 long-lived channels, admission queues, slow-connection protection, crash recovery and strict sandboxing. Dynamic scaling and multi-tenant quotas belong to Phase 2.

## 1. Problems in the earlier configuration and CLI <a id="section-1"></a>

A review of `src/host/config.cc`, `src/host/host_config_model.cc`, `src/host/policy_compiler.cc` and `src/host/main.cc` identified six problems in v1 and the transitional v2 model.

| Problem | Evidence in the implementation | Operational impact |
| --- | --- | --- |
| 1. Accepted settings had no effect | The parsers accepted `worker.pidsMax`, `capacity.startupsConcurrent`, `capacity.workerMemoryCommitTotal`, `capacity.queuedRequestsTotal` and `capacity.queuedHeaderBytesTotal` without a consumer. The schema accepted `recovery.restartBackoff.stableReset`, but `host_config_model.cc` hard-coded `60000`. Only `defaults.request.timeout` reached the policy compiler out of 13 exposed defaults. | Operators could configure `defaults.pool.queueRequests` or `defaults.worker.memoryMax` and silently receive no protection. |
| 2. Internal tuning exposed as configuration | WebSocket exposed seven buffer and handshake parameters (`reassemblyBudgetBytes`, `sendBudgetBytes`, `perChannelQueueBytes`, `channelQueueBytesTotal`, `pendingDialsTotal`, `dialTimeoutMs`, `closeTimeoutMs`). Rollout exposed `activationSurgeWorkers` (default `0` blocked rollout at capacity). Recovery exposed `restartBackoff.*` and `replacementsConcurrentPerApp`. The only accepted `admin.mode` was `"0600"`. | Operators had to understand internal state machines to configure a service. |
| 3. Inconsistent units and types | `timeout`, `queueTimeout` and `listeners[].limits.streamIdleTimeout` took strings such as `"60s"`. Adjacent `request.{streamIdleTimeoutMs, writeTimeoutMs}` and `channels.*TimeoutMs` required integers such as `60000`. Most sizes used `"16MiB"`, while `channels.*Bytes` required `16777216` and `headerBytes` expected `"32KiB"`. App `fetch.allow` and Binding `net.allow` used incompatible target syntax. | Equivalent quantities needed different representations. A unit mistake could prevent loading the configuration. |
| 4. Versioned field lists diverged | `kHostMembers` (`capsid/host-v1`) included top-level `channels`. When `bindingsRoot` was added to `kHostV2Members` (`capsid/host-v2`), `channels` was omitted. | One Host configuration could not enable Bindings and set long-lived channel limits together. |
| 5. Fixed pools and platform-specific settings | The fixed pool required both `minReady` and `maxWorkers` and enforced equality. `worker.processAddressSpace` was Linux-specific and macOS rejected a manifest that supplied it. | A single pool size required two settings. Application manifests were difficult to share across platforms. |
| 6. Local and managed deployment diverged | A local Hello World required 12 arguments. The CLI split `single-worker` and `static-pool`, required `/@capsid/<app>/` for a single application, and duplicated JSON settings through 36 arguments while ignoring `capsid.json#pool`. | An application that ran with local overrides could fail in managed deployment. |

---

## 2. Field ownership, units and inheritance <a id="section-2"></a>

### 2.1 Configuration ownership

```text
┌────────────────────────────────────────────────────────────────────────────┐
│ [1] Host environment:   paths | admin | isolation | trustedBytecodeKeys    │
│                         listeners | recovery                               │
├────────────────────────────────────────────────────────────────────────────┤
│ [2] Machine capacity:   capacity.{workersTotal, channelsTotal,             │
│                                   maxChannelMessageSize, channelIdleTimeout}│
├────────────────────────────────────────────────────────────────────────────┤
│ [3] Per-app policy:     permissions (intersection of allowed grants)       │
│                         defaults.{worker, request, pool}                   │
│                         maximums.{worker, request, pool} (hard ceilings)   │
├────────────────────────────────────────────────────────────────────────────┤
│ [4] Application:        entry | permissions | bindings | healthCheck       │
│     (capsid.json)       worker | request | pool (inherit omitted fields)   │
└────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Field changes

| Old setting | Config v2 design |
| --- | --- |
| Six unused fields (`pidsMax`, `workerMemoryCommitTotal`, `queuedRequestsTotal`, `queuedHeaderBytesTotal`, `startupsConcurrent`, `stableReset`) | Remove them. Test that every field the schema accepts has a consumer. |
| `admin.mode` (only valid value `"0600"`) | Remove it. The Host enforces `0600` and checks for the same UID with `SO_PEERCRED`. |
| `applicationsRoot`, `stateRoot`, `secretRootTemplate`, `bindingsRoot` | Group the four paths under `paths.{applications, state, secrets, bindings}`. |
| Required `listeners[].{publicScheme, publicAuthority}` | Replace both with optional `publicOrigin`, for example `"https://apps.example.com"`. The default is `"http://<tcp>"`. |
| `capacity.activationSurgeWorkers` | Automatically reserve `max(2, workersTotal * 25%)` temporary overlap slots during a blue-green rollout. |
| Seven WebSocket settings (`reassemblyBudget`, `sendBudget`, `perChannelQueue`, `channelQueue`, `pendingDials`, `dialTimeout`, `closeTimeout`) | Derive internal buffer ratios and timeouts from protocol defaults. Expose `channelsTotal`, `maxChannelMessageSize` and `channelIdleTimeout`, including in `host-v2`. |
| `recovery.restartBackoff.*` and `replacementsConcurrentPerApp` | Use exponential backoff (`100ms..10s`, `10%` jitter, reset after `60s` of stability), with serial replacement per application. Expose `crashBudget` and `healthCheck`. |
| `pool.queueHeaderBytes` | Remove it. Queued headers are bounded by `listeners[].limits.headerSize × pool.queueRequests`. |
| `request.{maxStreamingInflightPerWorker, streamIdleTimeoutMs, writeTimeoutMs}` | Move slow-connection and stream protection to `listeners[].limits`. Keep `timeout` and `maxInflight` in the application request settings. |
| `worker.processAddressSpace` | Remove it from the cross-platform `capsid.json`. In Linux managed mode with `isolation.cgroupRoot`, per-worker `worker.memoryMax` and shared tenant `tenancy.*.hardMemory` enforce physical memory via cgroup v2 `memory.max`; the runtime platform default `RLIMIT_AS` ceiling remains separate from `memoryMax` so mmap and WASM virtual reservations are not broken. |
| Required equal `pool.{minReady, maxWorkers}` | Phase 1 uses the optional fixed-size `pool.workers`, falling back to `defaults.pool.workers ?? 1`. |

### 2.3 Units and inheritance

1. **Use one representation for each quantity.**
   - Durations use strings such as `"100ms"`, `"5s"` and `"1m"`. Remove bare `*Ms` integers.
   - Sizes use strings such as `"32KiB"`, `"256MiB"` and `"2GiB"`. Remove bare `*Bytes` integers.
   - Host `permissions.fetchTargets`, App `permissions.fetch.allow` and App `bindings.<id>.permissions.net.allow` share the `<host>[:<p1>[,<p2>...]]` parser. It accepts exact names, `*.` wildcards, IPv4/IPv6 literals and CIDR prefixes. Explicit ports are decimal numbers from 1 to 65535. Use a comma-separated list for multiple ports. An installed `capsid/binding-v1` manifest grants one explicit port per target. The compiler checks each requested Binding port against these grants and emits one Worker rule per authorized endpoint.
2. **Use the same fields in Host defaults, Host maximums and the application manifest.**
   All three use the same 11 leaf fields under `{ worker, request, pool }` (`worker.{memoryMax, jsHeap, fileDescriptors}`, `request.{timeout, maxInflight}`, and `pool.{workers, minReady, maxWorkers, idleTimeout, queueRequests, queueTimeout}`). Resolve each field in this order:
   $$\text{effective}(f) = \text{app}(f) \;\text{??}\; \text{host.defaults}(f) \;\text{??}\; \text{builtin}(f)$$
   At startup require $\text{host.defaults}(f) \le \text{host.maximums}(f)$. Reject deployment if an explicit `app(f)` exceeds `host.maximums(f)`. When the application omits the field, clamp the built-in default to the Host maximum.
   After resolving inheritance, the effective `worker.memoryMax` must accommodate `worker.jsHeap`. Reject a 32 MiB application limit combined with an inherited 64 MiB heap.

---

## 3. Minimal and full configuration examples <a id="section-3"></a>

### 3.1 Minimal configuration

#### Minimal `host.json`
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

#### Minimal `capsid.json`
```json
{
  "apiVersion": "capsid/app-v2"
}
```

### 3.2 Configuration using every supported field

#### Full `host.json`
```json
{
  "apiVersion": "capsid/host-v2",

  "paths": {
    "applications": "/srv/capsid/applications",
    "state": "/srv/capsid/state",
    "secrets": "/srv/capsid/secrets/{application}",
    "bindings": "/etc/capsid/bindings"
  },

  "admin": {
    "unix": "/run/capsid/admin.sock"
  },

  "isolation": {
    "mode": "strict",
    "required": ["cgroup-v2"],
    "cgroupRoot": "/sys/fs/cgroup/capsid"
  },

  "trustedBytecodeKeys": {
    "2026-11": "/etc/capsid/keys/ed25519-2026-11.pub"
  },

  "listeners": [
    {
      "name": "public",
      "tcp": "0.0.0.0:8080",
      "publicOrigin": "https://apps.example.com",
      "trusted": false,
      "routing": {
        "mode": "subdomain",
        "suffix": ".apps.example.com"
      },
      "limits": {
        "connections": 512,
        "headerSize": "32KiB",
        "headerTimeout": "5s",
        "bodyIdleTimeout": "30s",
        "streamIdleTimeout": "60s",
        "writeTimeout": "10s",
        "channels": 128
      },
      "cors": {
        "allowedOrigins": ["https://dev.example.com"],
        "allowedMethods": ["GET", "POST", "OPTIONS"],
        "allowedHeaders": ["content-type", "capsid-app", "access-token"],
        "maxAge": "86400s"
      }
    }
  ],

  "permissions": {
    "modules": ["capsid:env", "capsid:fs", "capsid:storage", "capsid:stdio"],
    "environmentNames": ["APP_MODE", "DATABASE_*"],
    "fsReadRoots": ["/srv/capsid/config"],
    "fetchTargets": ["api.example.com:443", "metrics.example.com:443"],
    "storageNamespaces": ["session"],
    "stdioStreams": ["stdout", "stderr"]
  },

  "defaults": {
    "worker": {
      "memoryMax": "256MiB",
      "jsHeap": "64MiB",
      "fileDescriptors": 64
    },
    "request": {
      "timeout": "5s",
      "maxInflight": 64
    },
    "pool": {
      "workers": 2,
      "queueRequests": 256,
      "queueTimeout": "10s"
    }
  },

  "maximums": {
    "worker": {
      "memoryMax": "512MiB",
      "jsHeap": "256MiB",
      "fileDescriptors": 256
    },
    "request": {
      "timeout": "30s",
      "maxInflight": 128
    },
    "pool": {
      "workers": 16,
      "queueRequests": 1024,
      "queueTimeout": "30s"
    }
  },

  "capacity": {
    "workersTotal": 64,
    "channelsTotal": 1024,
    "maxChannelMessageSize": "1MiB",
    "channelIdleTimeout": "60s"
  },

  "recovery": {
    "crashBudget": {
      "maxEvents": 5,
      "window": "60s"
    },
    "healthCheck": {
      "interval": "30s",
      "failures": 3
    }
  }
}
```

#### Full `capsid.json`
```json
{
  "apiVersion": "capsid/app-v2",
  "entry": "bundle.mjs",

  "permissions": {
    "modules": ["capsid:env", "capsid:fs", "capsid:storage", "capsid:stdio"],
    "env": {
      "APP_MODE": { "value": "production" },
      "DATABASE_URL": { "valueFrom": "db-url" }
    },
    "fs": {
      "read": {
        "allow": ["/srv/capsid/config"],
        "deny": ["/srv/capsid/config/private.json"]
      }
    },
    "fetch": {
      "allow": ["api.example.com:443", "metrics.example.com:443"]
    },
    "storage": {
      "namespaces": ["session"]
    },
    "stdio": ["stdout", "stderr"]
  },

  "bindings": {
    "redis": {
      "permissions": {
        "net": { "allow": ["10.0.1.15:6379"] }
      },
      "config": {
        "keyPrefix": "orders:"
      },
      "secrets": {
        "password": { "valueFrom": "redis-password" }
      }
    }
  },

  "worker": {
    "memoryMax": "256MiB",
    "jsHeap": "64MiB",
    "fileDescriptors": 64
  },

  "request": {
    "timeout": "5s",
    "maxInflight": 64
  },

  "pool": {
    "workers": 2,
    "queueRequests": 256,
    "queueTimeout": "10s"
  },

  "healthCheck": {
    "path": "/health",
    "timeout": "2s"
  }
}
```

---

## 4. Capacity scopes and Host field reference <a id="section-4"></a>

### 4.1 Per-application and machine-wide worker limits

In managed mode, one `capsid-host` hosts several applications. Size individual pools and the machine-wide process budget separately.

```text
Host capacity.workersTotal = 64 (maximum total worker processes)
 ├── orders:  capsid.json sets pool.workers = 2
 ├── payment: capsid.json omits pool and inherits host defaults.pool.workers = 2
 ├── search:  capsid.json sets pool.workers = 12, below host maximums.pool.workers = 16
 └── Slots available to other deployments: 64 - (2 + 2 + 12) = 48
```

| Setting | Scope | Effect |
| --- | --- | --- |
| `capsid.json` → `pool.workers` | One application | Number of worker processes requested by the application. |
| `host.json` → `defaults.pool.workers` | Per-application default | Allocation when the application omits `pool.workers`. |
| `host.json` → `maximums.pool.workers` | Per-application ceiling | Prevent a single pool from consuming the whole machine. |
| `host.json` → `capacity.workersTotal` | All applications on the Host | Reject a deployment that would take the sum of `pool.workers` above this limit. |

### 4.2 Host v2 field reference

| Section | Fields | Required | Default | Purpose |
| --- | --- | --- | --- | --- |
| `paths` | `applications` / `state` / `secrets` / `bindings` | First three | `bindings`: empty | Application packages, persistent state, secret path template containing `{application}`, and optional Binding plugins. |
| `admin` | `unix` | Yes | — | Local administration socket. The Host enforces mode `0600` and same-UID access. |
| `isolation` | `mode` / `required` / `cgroupRoot` | | `mode: "strict"` | Linux production sandbox mode, required kernel features and delegated cgroup root. |
| `trustedBytecodeKeys` | `<keyId>: <path>` | | `{}` | Maps signing key IDs to Ed25519 public keys for verifying precompiled bytecode. |
| `listeners[]` | `name` / `tcp` / `publicOrigin` / `trusted` / `routing` / `limits` / `cors` | `name`, `tcp`, `routing.mode` | `publicOrigin: "http://<tcp>"`, `trusted: false` | Ingress address and public origin, routing (`path`, `subdomain`, `header`, `single`; optional `routing.header`), slow-connection and concurrency limits, and gateway CORS. |
| `permissions` | `modules` / `environmentNames` / `fsReadRoots` / `fetchTargets` / `storageNamespaces` / `stdioStreams` | | All empty (deny all) | Machine-wide grant ceiling. Effective permissions are its intersection with `capsid.json#permissions`. |
| `defaults` / `maximums` | `worker.{memoryMax, jsHeap, fileDescriptors}`, `request.{timeout, maxInflight}`, `pool.{workers, minReady, maxWorkers, idleTimeout, queueRequests, queueTimeout}` | | Built-in defaults / omitted = unlimited (`0` internally; explicit `0` in JSON is rejected for positive ceilings) | Per-application defaults and hard ceilings. |
| `capacity` | `workersTotal` / `memory` / `channelsTotal` / `maxChannelMessageSize` / `channelIdleTimeout` | | `1` / disabled / `1024` / `"1MiB"` / `"60s"` | Limits total resident workers, Host memory budget/watermarks, WebSocket channels and message size, and sets the channel idle timeout. |
| `recovery` | `crashBudget.{maxEvents, window}` / `healthCheck.{interval, failures}` | | 5 events per 60s / 30s and 3 failures | Crash quarantine threshold and active health-check interval and failure threshold. |

---

## 5. Use the managed package layout locally <a id="section-5"></a>

### 5.1 Run the managed application package locally

On Linux, macOS and Windows, the local Host mounts a version directory that can also be deployed in managed mode. Reading application settings from its manifest prevents a deployment from depending on development-only CLI overrides.

1. **Package layout and validation**
   The local directory (for example, `./dist/`) and the deployed directory `<paths.applications>/<app>/<version>/` share one layout and the same validation rules.
   ```text
   ./dist/
     ├── capsid.json     (capsid/app-v2 application manifest)
     └── bundle.mjs      (self-contained ESM bundle, unless capsid.json#entry specifies another file)
   ```
   The manifest specifies the entry point, permissions, Bindings, memory limits, timeouts, concurrency, `pool.workers` and health checks. Remove the 36 CLI arguments that duplicated these settings.

2. **One local mode replaces the 12-argument invocation.**
   - `single-worker` is the `workers == 1` case of `static-pool`.
   - By default, find the matching `capsid-worker` beside `capsid-host`.
   - Mount at `/` by default. The worker sees the same `request.url` path it would after the managed gateway strips `/@capsid/<app>/`. Local development does not require `--application`.
   - The remaining switches supply the local equivalent of the Host environment.

   | Local CLI argument | Default | Managed counterpart |
   | --- | --- | --- |
   | `--app-dir <dir>` | `.` (working directory) | Version directory using the managed package layout. |
   | `--app <id>` | `"local"` | Application ID used in routing, metrics, and logs. |
   | `--listen <ip:port>` | `127.0.0.1:8080` | Local counterpart of `listeners[].tcp`. |
   | `--host-config <path>` | Empty | Optional `capsid/host-v2` file for exercising Host ceilings, listener routing, and CORS locally. |
   | `--worker <path>` | Sibling `capsid-worker` | Explicit path to the `capsid-worker` binary. |
   | `--secrets-dir <dir>` | Empty | Supplies the single-app `paths.secrets` directory for testing `valueFrom`. |
   | `--bindings-dir <dir>` | Empty | Supplies the `paths.bindings` plugin directory for testing `bindings`. |
   | `--workers <n>` | `capsid.json#pool.workers` (default `1`, max `256`) | Temporary concurrency override for local load tests. |
   | `--strict-sandbox` | Optional `on`/`off` on Linux, automatically `off` on macOS/Windows | Exercise Linux production sandboxing locally. |
   | `--ready-fd` / `--stop-fd` | Optional in interactive terminals | Readiness and shutdown signalling for test harnesses and parent supervisors. |

   **Local command before and after the change**
   ```sh
   # Earlier CLI: 12 required arguments and the /@capsid/hello/ route prefix.
   capsid-host --mode single-worker --worker "$CAPSID_BIN/capsid-worker" \
     --source-bundle ./bundle.mjs --source-name "file://$PWD/bundle.mjs" \
     --application hello --listen 127.0.0.1:8080 --routing path \
     --public-scheme http --public-authority 127.0.0.1:8080 \
     --request-timeout-ms 5000 --strict-sandbox off --ready-fd 1 --capsid-json ./capsid.json

   # Config v2: run the same version directory that will be deployed in managed mode.
   capsid-host --app-dir ./dist
   ```

---

# Phase 2. Elastic pools and tenant memory budgets

> Phase 2 extends the Phase 1 configuration with on-demand creation, idle shutdown, pool floors and ceilings, machine-wide memory limits and fair eviction across tenants.

## 6. Architecture comparison with workerd <a id="section-6"></a>

| Dimension | Cloudflare `workerd` | Capsid v0.3.2 design target |
| --- | --- | --- |
| Isolation unit | Multiple threads and V8 isolates share a process and its platform. | Each worker uses a separate sandboxed OS process (namespaces, Landlock, seccomp and cgroups) running the zero-JIT `quickjs-turbo` engine. |
| Startup and resident memory | Recreating an isolate takes a few milliseconds. The shared V8 service tree occupied hundreds of MiB in the idle and loaded measurements (`555.7 MiB` loaded PSS on `24671d9`). | On `dev/v0.3.2` (`24671d9`, 2026-11-27), 10 / 100 / 1024 KiB bytecode cold start measured **8.89 / 10.22 / 21.43 ms** at **8.2 / 8.9 / 14.7 MiB** PSS, loaded serving PSS was **19.3 MiB**, and the 2048 MiB density lane measured **705 / 501 / 135** warm workers (compared with **715 / 496 / 127** on `v0.3.1-beta.1` `6d4cc61`). [Benchmark methodology](../reference/benchmarks.md) |
| Eviction unit | Evict a cold isolate context within the engine, relying on V8 heap isolation. | Reclaim an idle worker process. The OS releases its private physical pages, and tenants do not share a fragmented JS heap or residual heap state. |
| Accounting and scheduling | Read V8 heap statistics in process. | The Host must queue cold requests, throttle concurrent forks, reap processes asynchronously and enforce tenant fairness. |

---

## 7. Tenant fairness and two-stage eviction <a id="section-7"></a>

### 7.1 Why global LRU alone can penalize a small tenant

Consider one list of idle workers shared by all tenants:
- A traffic burst starts 100 workers for tenant A. After the burst, all 100 are idle but recently used.
- Tenant B receives one request every 30 seconds. Its only warm worker is the least recently used.
- At the memory ceiling, global LRU would evict B's only worker before any of A's 100 idle workers. Repeated bursts could force every request from B through cold start.

### 7.2 Two-stage fair eviction (`GlobalEvictionCoordinator`)

A worker is eligible only when all three conditions hold:
1. **Idle.** `inflight_requests == 0` and `active_channels == 0`. Active HTTP requests, SSE streams and WebSocket connections protect the worker.
2. **Above the floor.** The pool has more live workers than its guarantee: `live > pool.minReady`.
3. **Resident long enough.** The worker has completed startup and passed `minResidency` (default `5s`), preventing immediate eviction and respawn.

When working-set memory reaches `highWatermark` (default `85%`) or the budget blocks scale-up, evict eligible workers toward `lowWatermark` (default `75%`). The ten-percentage-point gap supplies hysteresis.

- **Stage 1. Reclaim excess tenant usage.**
  Compute the share using every active tenant with resident workers:
  $$\text{FairShare} = \frac{\text{MemoryBudget}}{N_{\text{active\_tenants}}}$$
  For tenants with a configured soft minimum, use $\max(\text{FairShare}, \text{minSoftMemory})$.
  Among tenants above their soft share, start with the tenant furthest above its share by ratio. Evict its oldest eligible worker by `lastActiveAt`. This reclaims spare replicas before taking a smaller tenant's only warm worker.
- **Stage 2. Use global LRU when every tenant is within its share.**
  If all tenants are within their shares and memory is still above the target, evict the oldest eligible worker across the Host. If an over-share tenant has only guaranteed or busy workers, keep the workers of tenants within their shares. Memory can temporarily stay above the target, and a scale-up request can be rejected for lack of capacity.

---

## 8. Engineering constraints <a id="section-8"></a>

A review of `generation_pool.h`, `worker_capacity_ledger.h` and `worker_supervisor.h` yielded six implementation requirements.

1. **Keep soft watermarks in userspace.**
   - Exceeding Linux `memory.high` can throttle an active event loop in direct reclaim and increase P99 latency for requests already in flight.
   - Read `memory.current` for O(1) accounting and set `memory.max = hardMemory` as the kernel-enforced OOM boundary. The userspace coordinator enforces soft watermarks asynchronously by selecting idle workers.
   - In Linux managed mode, `isolation.cgroupRoot` identifies a cgroup v2 root with the memory controller delegated. All applications, generations and additional workers for a tenant with `hardMemory` share a parent cgroup. Per-worker leaf limits still apply. If a configured root cannot enforce the tenant ceiling, worker startup fails. When the root is omitted, sampled memory informs deployment and scale-up admission only.
2. **Measure memory with and without a delegated controller.**
   - Unprivileged Docker containers, rootless containers and development environments may lack permission to create child cgroups or delegate `+memory`.
   - Read `memory.current` when delegated. Otherwise, sample and cache process PSS/RSS (for example from `/proc/<pid>/smaps_rollup`) when a worker returns to idle and during low-frequency background scans (default `5s`). Keep system calls off the request path.
3. **Reap asynchronously during scale-up.**
   - Waiting synchronously for graceful exit via `waitpid` before spawning the replacement could add hundreds of milliseconds to a roughly 9 ms bytecode cold start.
   - Atomically remove the idle victim from READY, confirm zero in-flight requests and issue an irreversible `shutdown`. Transfer its steady-state slot to the replacement immediately so it can spawn in parallel. Put the old worker in a bounded `reaping` set for asynchronous cleanup.
4. **Keep active health checks separate from application activity.**
   - The Host sends `submit_probe` every `30s` by default. Refreshing `lastActiveAt` would prevent a worker with `idleTimeout = 60s` from scaling to `0` and distort LRU ordering.
   - Health probes leave `lastActiveAt` unchanged. When `live == 0 && minReady == 0`, `WorkerSupervisor` skips the probe without recording a failure or waking a worker.
5. **Track guaranteed and elastic capacity separately in `WorkerCapacityLedger`.**
   - Fixed-size reservations charged against `maxWorkers` would prevent elastic overcommit and compete with blue-green rollout.
   - Charge `committed = Σ minReady` at deployment and enforce `committed <= workersTotal`. The remaining `elastic = workersTotal - committed` slots are allocated on demand and reclaimed by LRU. Warm `max(1, new.minReady)` workers for rollout. With `minReady == 0`, the new pool can return to zero after its health check. Existing work drains on the old generation in `draining`.
6. **Protect WebSocket and SSE connections.**
   - A worker with an active connection is ineligible for idle eviction. Once `streamIdleTimeout` or `channelIdleTimeout` expires a stale connection, the worker may become eligible again. While alive, it counts against its own tenant's `hardMemory`.

---

## 9. Application and Host settings for elastic pools <a id="section-9"></a>

The elastic pool and tenancy settings build on the existing `defaults` inheritance chain.

### 9.1 Assign tenants in the Host control plane

- `capsid.json` has no `tenant` field. An untrusted application cannot claim another tenant's identity, and developers can use the same application package across deployments.
- The platform may supply `{"app": "orders", "version": "v2", "tenant": "acme"}` in the Admin API call `POST /v1/deploy`. Omitting the tenant defaults to `tenant = app`, enabling per-application fair shares without additional configuration.

### 9.2 Additional configuration

1. **Application pool and Host `defaults.pool` / `maximums.pool`**
   Keep `pool.workers: N` as shorthand for `minReady = maxWorkers = N`. An elastic application can declare:
   ```json
   "pool": {
     "minReady": 0,
     "maxWorkers": 8,
     "idleTimeout": "60s"
   }
   ```
   > Built-in `defaults.pool` are conditional on pool shape: `idleTimeout: "60s"` applies only to elastic pools (`minReady < maxWorkers`; fixed pools default to `0`/disabled), and `queueRequests: 64` / `queueTimeout: "5s"` apply only when `minReady == 0` (`minReady >= 1` defaults queueing to `0`/disabled unless configured). An app that specifies only `"minReady": 0, "maxWorkers": 8` can therefore queue concurrent cold requests during the roughly 9 ms startup without failing with `503`.

2. **Host memory budget under `host.json#capacity`**
   Set `memory` to `"2GiB"` for default watermarks of `85% / 75%` and a `5s` residency and inspection interval. The object form is `{ limit, highWatermarkPercent=85, lowWatermarkPercent=75, minResidency="5s", checkInterval="5s" }`.
   ```json
   "capacity": {
     "workersTotal": 512,
     "memory": "2GiB"
   }
   ```
   Multi-tenant deployments can also configure the optional top-level `tenancy` block:
   ```json
   "tenancy": {
     "defaultQuota": {
       "maxWorkers": 16,
       "hardMemory": "512MiB"
     },
     "overrides": {
       "vip-acme": {
         "maxWorkers": 64,
         "minSoftMemory": "256MiB",
         "hardMemory": "2GiB"
       }
     }
   }
   ```

### 9.3 Explain each eviction and rejection

Admin status, structured logs and `/metrics` must expose the specific reason for every eviction and rejection:
- **`capsid_pool_evictions_total{app, tenant, reason}`** — `idle_timeout`, `tenant_overshare`, `global_lru`, `tenant_quota_swap`.
- **`capsid_pool_rejections_total{app, tenant, reason}`** — `queue_disabled`, `queue_full`, `queue_timeout`, `app_max_workers`, `tenant_max_workers`, `tenant_hard_memory`, `host_capacity_exhausted`.

---

## 10. Configuration retirement and delivery milestones <a id="section-10"></a>

1. **Target configuration contract.** `dev/v0.3.2` uses `capsid/host-v2` and `capsid/app-v2` and has retired `capsid/host-v1` and `capsid/app-v1`. `capsid/binding-v1` and C ABI v8 are independent contracts.
2. **Deliver each milestone with its own tests.**
   - **A. Config v2 and local CLI.** Implement the Phase 1 Host/app model, wire all 11 `defaults`/`maximums` fields, and replace local `single-worker`/`static-pool` modes with one CLI that uses managed packages. Test that every accepted schema field is consumed.
   - **B. Single-pool elasticity.** Implement `0 <= minReady <= maxWorkers`, `idleTimeout`, the cold-request queue, bounded asynchronous `reaping`, and health probes isolated from idle accounting.
   - **C. Memory budget and tenant fairness.** Implement `capacity.memory`, cgroup v2 and sampled PSS accounting, tenant assignment by the Admin deployment API, `tenancy` quotas, two-stage LRU and operational metrics.
