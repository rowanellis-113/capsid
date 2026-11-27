<div align="center">

<img src="logo.png" alt="Capsid" width="360" />

### Process-isolated JavaScript at isolate density.

Run untrusted customer extensions, webhook rules, and AI-generated JavaScript in dedicated **OS-sandboxed elastic process pools** — scaling cleanly from zero to hundreds of warm workers per host with standard Web `fetch(Request)` handlers, sub-10 ms cold starts, and a zero-JIT footprint engineered for high-density multi-tenancy.

<p>
  <a href="https://github.com/rowanellis-113/capsid/releases/tag/v0.2.2"><img src="https://img.shields.io/badge/stable-v0.2.2-2563EB?style=flat-square" alt="Stable v0.2.2" /></a>
  <a href="https://github.com/rowanellis-113/capsid/releases/tag/v0.3.1-beta.1"><img src="https://img.shields.io/badge/prerelease-v0.3.1--beta.1-7C3AED?style=flat-square" alt="Prerelease v0.3.1-beta.1" /></a>
  <a href="docs/reference/embedding-api.md"><img src="https://img.shields.io/badge/C_ABI-v8-0F766E?style=flat-square" alt="Public C ABI v8" /></a>
  <a href="docs/reference/benchmarks.md"><img src="https://img.shields.io/badge/engine-quickjs--turbo_Zero--JIT-D97706?style=flat-square" alt="quickjs-turbo Zero-JIT" /></a>
  <a href="https://rowanellis-113.github.io/capsid/"><img src="https://img.shields.io/badge/benchmarks-interactive_charts-059669?style=flat-square" alt="Interactive Benchmarks" /></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-475569?style=flat-square" alt="Apache-2.0 License" /></a>
</p>

<p>
  <a href="#install-and-60-second-quick-start"><strong>Install &amp; Quick Start</strong></a> &nbsp;&middot;&nbsp;
  <a href="#why-capsid"><strong>Why Capsid</strong></a> &nbsp;&middot;&nbsp;
  <a href="#benchmarks-and-replayable-evidence"><strong>Benchmarks</strong></a> &nbsp;&middot;&nbsp;
  <a href="#architecture"><strong>Architecture</strong></a> &nbsp;&middot;&nbsp;
  <a href="#embed-the-cc-sdk-abi-v8"><strong>Embed SDK</strong></a> &nbsp;&middot;&nbsp;
  <a href="#security-defaults-and-module-grants"><strong>Security</strong></a> &nbsp;&middot;&nbsp;
  <a href="docs/README.md"><strong>Docs</strong></a>
</p>

<br />

<table>
  <tr>
    <td align="center" width="25%">
      <br />
      <strong><code>705</code></strong><br />
      <sub><strong>Warm OS Workers in 2 GiB</strong><br />10 KiB app &middot; Linux strict sandbox</sub>
      <br /><br />
    </td>
    <td align="center" width="25%">
      <br />
      <strong><code>8.89 ms</code></strong><br />
      <sub><strong>C ABI Cold Start to First Response</strong><br />8.2 MiB PSS &middot; 10 KiB trusted bytecode</sub>
      <br /><br />
    </td>
    <td align="center" width="25%">
      <br />
      <strong><code>19.3 MiB</code></strong><br />
      <sub><strong>Loaded Service PSS</strong><br />Host + worker @ 64 conn (28.8&times; vs workerd)</sub>
      <br /><br />
    </td>
    <td align="center" width="25%">
      <br />
      <strong><code>Zero-JIT</code></strong><br />
      <sub><strong><code>quickjs-turbo</code> Native VM</strong><br />135 warm 1 MiB workers &middot; no RWX pages</sub>
      <br /><br />
    </td>
  </tr>
</table>

</div>

---

## Install and 60-Second Quick Start

### Install `dev/v0.3.2` (Config v2, Elastic OS Process Pools &amp; C ABI v8)

Build and install `capsid-host`, `capsid-worker`, `capsid-bytecode-compile`, and the C/C++ ABI v8 SDK from the current `dev/v0.3.2` branch ([Linux / macOS build guide](docs/guides/build-from-source.md) &middot; [Windows MSVC build guide](docs/guides/build-on-windows.md)):

```sh
git clone --branch dev/v0.3.2 --recurse-submodules https://github.com/rowanellis-113/capsid.git
cd capsid
npm ci --ignore-scripts --prefix vendor/txiki.js
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCAPSID_BUILD_HOST=ON -DBUILD_TESTING=OFF
cmake --build build-release --parallel
cmake --install build-release --prefix "$HOME/opt/capsid-sdk"
export PATH="$HOME/opt/capsid-sdk/bin:$PATH"
```

### Prebuilt Release Binaries

> **Stable**: [`v0.2.2`](https://github.com/rowanellis-113/capsid/releases/tag/v0.2.2)
>
> **Prerelease**: [`v0.3.1-beta.1`](https://github.com/rowanellis-113/capsid/releases/tag/v0.3.1-beta.1) *(latest published binary release; ships the earlier `single-worker` local CLI &amp; C ABI v7 — [Run the published beta](docs/guides/run-application.md#run-the-published-beta) &middot; [Windows packages and checksums](docs/guides/install-packages.md#install-on-windows))*

```sh
curl -fsSL https://github.com/rowanellis-113/capsid/releases/download/v0.3.1-beta.1/install.sh \
  | CAPSID_REPO=rowanellis-113/capsid bash -s -- v0.3.1-beta.1
```

### Run a `dev/v0.3.2` Handler in 60 Seconds

Write standard Web `fetch(Request)` JavaScript — no proprietary handler wrapper required. With `capsid-host` and `capsid-worker` from `dev/v0.3.2` on your `PATH`, save this webhook rule as `bundle.mjs` in an empty directory:

```js
export default {
  async fetch(request) {
    const event = await request.json();
    const queue = event.type === "incident.opened" ? "on-call" : "inbox";
    return Response.json({ queue });
  },
};
```

Create a minimal Config v2 manifest (`capsid/app-v2`) and start the local Host on loopback:

```sh
printf '%s\n' '{"apiVersion":"capsid/app-v2"}' > capsid.json
capsid-host --app-dir . --listen 127.0.0.1:8080 --ready-fd 1
```

Once the Host emits its `capsid-host-ready-v1` readiness line, send an event from a second terminal:

```sh
curl -sS http://127.0.0.1:8080/ \
  -H 'Content-Type: application/json' \
  -d '{"type":"incident.opened"}'
# {"queue":"on-call"}
```

> [!TIP]
> **Elastic pools &amp; Web frameworks.** In Linux managed mode, add `"pool":{"minReady":0,"maxWorkers":8,"idleTimeout":"60s"}` to `capsid.json` for scale-to-zero Elastic OS Process Pools. **Hono**, **itty-router**, **H3**, and **Elysia** all run against pinned compatibility suites on Capsid's `CAPSID-MIN-2025-subset-v0` Web API profile.
> [Config v2 &amp; elastic pools](docs/design/config-v2.md) &middot; [Framework compatibility matrix](docs/reference/frameworks/README.md) &middot; [Web API conformance](docs/reference/web-conformance.md) &middot; [Native Windows quick start](docs/guides/run-application.md#run-the-development-branch)

---

## Why Capsid

Running untrusted tenant or AI-generated JavaScript usually forces an uncomfortable trade-off: **V8 isolates** share a process address space and balloon in memory under concurrent load, while **containers and microVMs** deliver OS isolation at tens to hundreds of megabytes per tenant.

**Capsid eliminates that compromise with Elastic OS Process Pools.** Every worker is a real Linux-sandboxed OS process (`namespaces` + `Landlock` + `seccomp` + `cgroup v2`), yet hundreds of warm workers fit inside a single 2 GiB host and idle apps scale all the way down to zero processes (`0 B` worker RAM). Ship Capsid standalone via `capsid-host`, or embed `libcapsid_runtime` directly into your C/C++ service so your control plane owns the listener, holds every credential, and enforces hard deadlines.

| Dimension | V8 Isolates (e.g. `workerd`) | Containers / MicroVMs | **Capsid** |
| :--- | :--- | :--- | :--- |
| **Isolation boundary** | In-process VM heap | OS / VM kernel boundary | **Dedicated OS process** (`NS` + `Landlock` + `seccomp` + `cgroup v2`) |
| **Elasticity &amp; reclamation** | Shared in-process heap across isolates; peak growth stays in the host process | Slow cold starts (`100 ms – 1 s+`) penalize scale-to-zero | **Elastic OS Process Pools** — scale to zero (`0 B` idle), `8.89 ms` C ABI cold spawn (10 KiB bytecode), OS reclaims 100% of private pages when evicted worker processes exit |
| **Memory under load** | High (`555.7 MiB` loaded service PSS @ 64 conn) | Tens to hundreds of MiB per tenant | **`19.3 MiB` loaded PSS** (`Host + worker`, 28.8&times; lower) |
| **2 GiB warm density** | `572` cells (10 KiB app) | Dozens | **`705` warm OS workers** (10 KiB app; `501` @ 100 KiB, `135` @ 1 MiB) |
| **Cold start (10 KiB, C ABI)** | `17.21 ms` / `57.9 MiB` | `100 ms – 1 s+` | **`8.89 ms` / `8.2 MiB`** (trusted bytecode) &middot; `9.29 ms` (source) |
| **JIT / RWX attack surface** | Active JIT compiler | Runtime-dependent | **Zero-JIT** (`quickjs-turbo` native register-frame VM) |
| **Deployment model** | Standalone server | Orchestrated fleet | **Standalone Linux Host** or **Embeddable C/C++ SDK (ABI v8)** |

### Elastic OS Process Pools

Capsid's managed Host (`dev/v0.3.2`, [Config v2 &amp; elastic pools](docs/design/config-v2.md)) pairs hard OS-process isolation with serverless-grade elasticity:

- **True Scale-to-Zero &amp; Cold-Start Queueing (`minReady: 0`)** — Idle applications drop to zero resident OS processes (`0 B` worker RAM) and wake fresh sandboxed workers on demand (the C ABI strict-sandbox microbenchmark measures **`8.89 ms`** to first response for 10 KiB trusted bytecode; Managed Host cold-queue waiting and optional delegated tenant `cgroup v2` attachment are outside that C ABI timing), while bounded cold queues (`queueRequests` / `queueTimeout`) absorb concurrent bursts without `503` storms.
- **OS-Level Memory Reclamation, Zero Shared Heap Bloat** — Evicting an idle worker terminates a dedicated OS process; once the process exits, Linux reclaims 100% of its private physical pages with zero shared cross-tenant heap residue or GC pause spillover.
- **Two-Stage Tenant-Fair Eviction (`GlobalEvictionCoordinator`)** — Under a host memory ceiling (`capacity.memory`, `85% / 75%` watermark hysteresis), Capsid reclaims excess idle replicas from over-share bursty tenants *before* evicting a quiet tenant's last warm worker, while keeping active HTTP requests, SSE streams, and WebSocket channels exempt from idle eviction.
- **Tenant-Wide `cgroup v2` `memory.max` Governance** — With a delegated Linux `cgroup v2` root, sibling apps, elastic scale-out bursts, and overlapping blue-green generations for the same tenant share one kernel-enforced `cgroup v2` parent ceiling — keeping OOMs and memory spikes sealed inside the tenant boundary.
- **Non-Blocking Slot Handoff &amp; Bounded-Drain Rollouts** — Evicted idle workers transfer steady-state capacity slots immediately while process exit, reaping (`waitpid`), and kernel page reclamation finish asynchronously in a bounded `reaping` set off the spawn critical path; rolling deployments drain live HTTP/SSE/WebSocket traffic on the old generation up to the configured `drain_deadline` before forcing retirement.

### Built for

- **AI Agents & Generated Code Execution** — Execute LLM-generated handlers and tool hooks behind hard kernel syscall filters, deny-by-default egress, and millisecond-scale teardown.
- **Customer Webhooks & SaaS Extensibility** — Let tenants deploy standard Web `fetch(Request)` rules without risking cross-tenant memory leaks or credential exposure.
- **High-Density Multi-Tenant Functions** — Pack hundreds of warm apps per GiB with scale-to-zero and tenant-wide Linux `cgroup v2` `memory.max` enforcement.
- **Embeddable C/C++ Gateways & Control Planes** — Drop `libcapsid_runtime` straight into an existing POSIX event loop (`epoll`/`kqueue`) with a frozen C11 / C++11 RAII ABI.

---

## Benchmarks and Replayable Evidence

Every metric below comes from a single archived `capsid-perf-lane-v3` session containing **4,533 replayable evidence files** with byte-for-byte response verification (`exact-bytes-v2`, zero errors/timeouts/mismatches). **[Explore interactive charts &rarr;](https://rowanellis-113.github.io/capsid/)**

### Serving Throughput & Memory Efficiency (64 Keep-Alive Connections)

Medians of five 15-second measured rounds with 64 in-flight requests. `Dynamic API` parses a URL, reads headers, crosses an `async` boundary, builds and FNV-1a checksums an 8-item order, and serializes JSON per request.

| Stack | Tier &amp; Isolation | JSON 1 KiB | JSON 16 KiB | Stream 1 KiB | Dynamic API | Trickle PSS | Loaded PSS | 1K QPS / MiB |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| **Capsid** | **T1 &middot; Strict Linux OS sandbox** | **41,125** | **23,202** | **14,675** | **19,512** | **16.1 MiB** | **19.3 MiB** | **2,130.8** |
| workerd | T1 &middot; V8 Isolate | 26,121 | 19,308 | 18,823 | 19,075 | 56.4 MiB | 555.7 MiB | 47.0 |
| Deno | T1 &middot; Permission sandbox (`--allow-net`) | 143,244 | 46,036 | 86,937 | 57,849 | 41.0 MiB | 63.4 MiB | 2,259.4 |
| Node `--jitless` | T2 &middot; No sandbox (V8 no-JIT) | 12,281 | 12,710 | 10,616 | 9,050 | 56.4 MiB | 65.0 MiB | 188.9 |
| Bun `--jitless` | T2 &middot; No sandbox (JSC no-JIT) | 142,484 | 44,622 | 127,275 | 44,037 | 34.3 MiB | 37.2 MiB | 3,830.2 |
| Node.js | T3 &middot; No sandbox (V8 JIT) | 110,011 | 44,059 | 85,716 | 54,640 | 60.6 MiB | 135.0 MiB | 814.9 |
| Bun | T3 &middot; No sandbox (JSC JIT) | 145,428 | 48,254 | 141,231 | 80,535 | 36.5 MiB | 47.5 MiB | 3,061.6 |

- **28.8&times; lower loaded PSS** (`19.3 MiB` vs `555.7 MiB`) and **45.3&times; higher 1K QPS/MiB** (`2,130.8` vs `47.0`) than `workerd` under the same 64-connection load, while **outperforming `workerd` on 3 of 4 workloads** (**+57.4%** on JSON 1 KiB `41,125` vs `26,121`, **+20.2%** on JSON 16 KiB `23,202` vs `19,308`, and **+2.3%** on `Dynamic API` `19,512` vs `19,075 req/s`).
- **3.35&times; faster than `Node --jitless`** on 1 KiB JSON (`41,125` vs `12,281 req/s`) and **2.16&times; faster on `Dynamic API`** (`19,512` vs `9,050 req/s`) in **3.37&times; less memory**, while running inside a strict Linux OS sandbox.
- **1.99&times; higher `Dynamic API` throughput** (`19,512` vs `9,808 req/s`) and **+89.1% higher JSON 1 KiB throughput** (`41,125` vs `21,747 req/s`) than the earlier `c7a80b9` QuickJS-NG session, alongside **135 warm 1 MiB workers** (`14.7 MiB` cold bytecode PSS) on `dev/v0.3.2`.

### Cold Start Latency (Spawn to First Validated Response)

| Runtime | 10 KiB (ms / PSS) | 100 KiB (ms / PSS) | 1 MiB (ms / PSS) | Warm Request |
| :--- | ---: | ---: | ---: | ---: |
| **Capsid &middot; bytecode** | **8.89 ms / 8.2 MiB** | **10.22 ms / 8.9 MiB** | **21.43 ms / 14.7 MiB** | **0.071–0.076 ms** |
| **Capsid &middot; source** | **9.29 ms / 8.2 MiB** | **14.23 ms / 9.3 MiB** | **70.55 ms / 19.4 MiB** | **0.071–0.073 ms** |
| workerd | 17.21 ms / 57.9 MiB | 20.68 ms / 59.9 MiB | 38.45 ms / 73.0 MiB | 0.214–0.228 ms |
| Bun | 14.15 ms / 35.3 MiB | 13.98 ms / 36.9 MiB | 66.37 ms / 45.9 MiB | 0.110–0.114 ms |
| Deno | 30.04 ms / 38.0 MiB | 35.99 ms / 39.0 MiB | 114.36 ms / 46.0 MiB | 0.126–0.129 ms |
| Node.js | 48.76 ms / 60.7 MiB | 48.93 ms / 61.4 MiB | 69.66 ms / 76.4 MiB | 0.187–0.207 ms |

### 2 GiB Multi-Tenant Density *(Budget-Limited, Host + Entire Worker Pool)*

| Stack | Phase | 10 KiB App | 100 KiB App | 1 MiB App |
| :--- | :--- | ---: | ---: | ---: |
| **Capsid workers** *(Linux strict OS sandbox)* | Cold (1 req) | **709** | **502** | **135** |
| **Capsid workers** *(Linux strict OS sandbox)* | Warm (100 req) | **705** | **501** | **135** |
| workerd cells *(V8 Isolates)* | Cold (1 req) | 948 | 808 | 254 |
| workerd cells *(V8 Isolates)* | Warm (100 req) | 572 | 524 | 264 |

Capsid retains **99.4%–100%** of its cold count after 100 warm requests per worker — fitting **+23.3% more warm 10 KiB workers** inside 2 GiB than `workerd`, each in its own strict Linux OS sandbox.

> [!NOTE]
> **Measurement provenance &amp; honest trade-offs:** These are measurements of a **local Release/LTO rebuild** of the untagged `dev/v0.3.2` commit `24671d9` (`quickjs-turbo v0.15.1-turbo.8`, 2026-11-27 UTC), on an i5-12400F under WSL2 with logical CPUs 0–3 assigned — **not** measurements of any distributed release package. Capsid's cold-start lane measures spawn-to-first-response over the C ABI (excluding the probe), whereas reference stacks use HTTP transport, so do not read cold-start times as an engine-only comparison. On raw single-process peak throughput without memory/sandbox constraints, JIT runtimes (`Bun`, `Deno`, `Node.js`) remain faster; Capsid optimizes for **process isolation per tenant, predictable PSS, and zero-JIT density**.
> [Full seven-stack results &amp; methodology](docs/reference/benchmarks.md) &middot; [Density protocol](bench/SPEC.md) &middot; [Replay the archived measurements](perf-site/README.md#restore-and-independently-check-an-existing-point)

---

## Architecture

```mermaid
flowchart LR
    Client["HTTP / SSE / WebSocket Clients"]
    subgraph Host["Host Service (capsid-host or C/C++ Embedder)"]
        Router["HTTP Listener, Router & Pool Scheduler"]
        Policy["Credentials, Capability Policy & Tenant cgroup v2"]
    end
    subgraph Worker["OS-Sandboxed Worker Process (Namespaces + Landlock + Seccomp)"]
        AppHeap["App JS Heap (quickjs-turbo Zero-JIT)"]
        Queue["Bounded Async Queue"]
        BindingHeap["Optional Trusted Binding Heap"]
        AppHeap <--> Queue <--> BindingHeap
    end
    Client <-->|"HTTP / SSE / WS"| Router
    Policy -. "enforces limits & grants" .-> Worker
    Router <-->|"FetchRPC v4 (Byte-Credit Backpressure)"| AppHeap
```

| Subsystem | How it works | Reference |
| :--- | :--- | :--- |
| **Lazy property inline caches** | `quickjs-turbo` runs a zero-JIT bytecode interpreter. Capsid purges property lookup caches built during module initialization before the worker reports `READY`; hot request paths re-warm naturally without bloating idle worker memory. | [`src/worker_main.cc`](src/worker_main.cc) &middot; [`src/worker_runtime.cc`](src/worker_runtime.cc) |
| **Credit-based IPC backpressure** | FetchRPC places explicit byte credits on the Host&ndash;worker boundary so slow consumers never exhaust IPC buffers. Hosts can request duplex channels and WebSocket upgrades through v4 capability negotiation. | [Embedding API](docs/reference/embedding-api.md) &middot; [FetchRPC v4](docs/reference/fetchrpc-v4.md) |
| **Zero-cost Trusted Bindings** | Credentials and native handles stay outside untrusted JS. A Trusted Binding runs in its own isolated JS heap inside the worker behind an explicitly granted async method and bounded queue. Workers with zero Bindings allocate no Binding heap. | [Binding v1 architecture](docs/design/bindings-v1.md) |
| **Scale-to-zero &amp; tenant `memory.max`** | On **`dev/v0.3.2`**, idle apps scale to zero. Under Linux delegated `cgroup v2`, one tenant's `memory.max` covers sibling apps and overlapping rolling generations (e.g., an open SSE stream pinning the old generation while the new generation starts, both sharing the same 384 MiB ceiling in the rollout test). | [Config v2 design](docs/design/config-v2.md) &middot; [v0.3.2 review &amp; release gates](docs/history/v0.3.2/README.md) |

---

## Embed the C/C++ SDK (ABI v8)

The v0.3.2 development SDK uses C ABI v8. Embed `libcapsid_runtime` through `<capsid/runtime.h>` (C11) or `<capsid/runtime.hpp>` (C++11 RAII). On POSIX, register `capsid_worker_fd()` with your event loop and drain `capsid_worker_next_event()` as events arrive.

```cpp
#include <capsid/runtime.hpp>

capsid::CapabilityPolicyBuilder policy;
policy.application_identity("tenant-a/webhook")
      .allow_module("capsid:uuid");

capsid_worker_config config;
capsid_worker_config_init(&config);
config.worker_path = "/usr/local/bin/capsid-worker";
config.strict_sandbox = 1; // Linux strict baseline: rlimits + no_new_privs + Landlock + seccomp
config.capability_policy = &policy.descriptor(); // egress_policy == NULL -> deny-all

capsid::Worker worker(config);
// Load a bundle, poll worker.fd() in your reactor, drain capsid_worker_next_event(),
// and verify CAPSID_EVENT_READY.flags before dispatching requests.
```

Install the SDK headers, CMake package (`Capsid::capsid_runtime`), library, and binaries from a prepared Release build:

```sh
cmake --install build-release --prefix "$HOME/opt/capsid-sdk"
```

---

## Security Defaults and Module Grants

Capsid enforces **deny-by-default egress**, **explicit per-module grants**, and **fail-closed Linux strict sandboxing** — never trusting application JavaScript with ambient filesystem, network, or host credentials.

<details open>
<summary><strong>Sandbox contract, egress policy, and complete module inventory (12 built-in modules)</strong></summary>

<br />

- **Linux strict mode (opt-in in C API):** `strict_sandbox` is off by default in the C API. For untrusted applications, enable Linux strict mode and verify that `CAPSID_EVENT_READY.flags` contains the sandbox features required by the deployment. The strict baseline applies resource limits, `no_new_privs`, Landlock filesystem rules and a seccomp syscall allowlist.
- **Egress deny-by-default:** When `egress_policy == NULL`, all egress Fetch requests are denied.
- **No legacy `tjs:*` exposure:** `tjs:*` modules cannot be enabled through configuration.
- **Platform &amp; bytecode boundaries:** The published beta accepts `pidsMax` in application configuration but provides no per-application PID ceiling. Load only trusted bytecode compiled for the matching worker build. macOS and Windows support trusted development.

| Category | Built-in modules (explicitly granted by Host) |
| :--- | :--- |
| **Resource &amp; Policy** | `capsid:env`, `capsid:fs`, `capsid:stdio`, `capsid:storage`, `capsid:system`, `capsid:permissions` |
| **Utilities** | `capsid:assert`, `capsid:getopts`, `capsid:hashing`, `capsid:ipaddr`, `capsid:utils`, `capsid:uuid` |

[Linux sandbox contract](docs/reference/linux-sandbox.md) &middot; [Host capability policy](docs/reference/capability-policy.md) &middot; [Module permissions](docs/reference/module-permissions.md) &middot; [Security policy](SECURITY.md)

</details>

---

## Documentation

| Area | Guides &amp; Specifications |
| :--- | :--- |
| **Get Started** | [Documentation index](docs/README.md) &middot; [Run an application](docs/guides/run-application.md) &middot; [Install packages](docs/guides/install-packages.md) &middot; [Build from source](docs/guides/build-from-source.md) |
| **Embed &amp; Protocol** | [C &amp; C++ embedding API](docs/reference/embedding-api.md) &middot; [FetchRPC v4](docs/reference/fetchrpc-v4.md) &middot; [Trusted Bindings v1](docs/design/bindings-v1.md) &middot; [Host configuration](docs/reference/host-configuration.md) |
| **Architecture &amp; Scaling** | [Runtime architecture](docs/design/runtime-architecture.md) &middot; [Host architecture](docs/design/host-architecture.md) &middot; [Config v2 &amp; elastic pools](docs/design/config-v2.md) &middot; [Platform support](docs/reference/platform-support.md) |
| **Compatibility &amp; Benchmarks** | [Benchmarks](docs/reference/benchmarks.md) &middot; [Framework matrix](docs/reference/frameworks/README.md) &middot; [Web API conformance](docs/reference/web-conformance.md) &middot; [Release status](docs/reference/release-status.md) |

---

<div align="center">
  <a href="CONTRIBUTING.md">Contributing</a> &nbsp;&middot;&nbsp;
  <a href="SECURITY.md">Security Policy</a> &nbsp;&middot;&nbsp;
  <a href="docs/history/v0.3.2/README.md">v0.3.2 Review Status</a> &nbsp;&middot;&nbsp;
  <a href="LICENSE">Apache-2.0 License</a>
</div>
