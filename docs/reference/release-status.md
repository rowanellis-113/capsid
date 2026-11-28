# Current Status and Data Scope

The [machine-readable release record](../data/current-status.json) binds the 2026-11-28 `v0.3.2-beta.1` publication, source locks and local benchmark to their recorded inputs.

## Published Prerelease (`v0.3.2-beta.1`)

The published `v0.3.2-beta.1` prerelease (`288955124e23eb88060b8267ea93e3d0e6a4fbbe`, also set as GitHub `Latest`) ships **C ABI v8** and **FetchRPC v4-only** with Build Info v2 and Capability Policy v2. Rebuild embedders against this SDK. It delivers Config v2 (`capsid/host-v2` / `capsid/app-v2`, retiring `host-v1` / `app-v1` and legacy `single-worker` / `static-pool` CLI modes), Elastic OS Process Pools (scale-to-zero, two-stage tenant-fair eviction, and tenant-wide `cgroup v2` `memory.max`), and retains `vendor/quickjs-turbo` (`v0.15.1-turbo.8`, commit `8fcb4751a66986bd42b11de3012e967d77790923`) on top of pinned `vendor/txiki.js` (`v26.6.0`, 79 SDK patches).

The current `dev/v0.4.0` checkout is a `runtime-feature-successor` (`releaseQualificationInherited:false`) built on top of `28895512` (`v0.3.2-beta.1`) that delivers **Binding RPC v2 (`v0.4.0`, `M4A`–`M4D`)** — fair multi-runtime scheduling, three-tier quota ledger, structured remote errors, `{ signal, timeoutMs }` call options and late-result disposal, bidirectional credit-based `ReadableStream`/`WritableStream` proxies, controlled `Request`/`Response` proxies, and native-branded `RpcTarget` remote object handles — while keeping **C ABI v8**, **FetchRPC v4**, `capsid/binding-v1` manifests, and the zero-Binding fast path intact. No `v0.4.0` binary release or new formal benchmark session has been published yet; `v0.3.2-beta.1` remains the latest published prebuilt binary release. See [Use Binding RPC v2](../guides/use-binding-rpc-v2.md), [Binding RPC v2 design](../design/binding-rpc-v2.md), [Config v2 design](../design/config-v2.md), and the [v0.3.2 verification record](../history/v0.3.2/README.md).

Via the upstream `quickjs-turbo::qjs` CMake target (with local LTO isolation),
non-sanitizer builds auto-enable the zero-JIT native register-frame interpreter
across all four source-level native backends supported by `quickjs-turbo`:
Linux x86-64 (`LINUX_X86_64`, GCC 13+, glibc/musl), Linux ARM64
(`LINUX_AARCH64`, GCC 13+, glibc/musl without default BTI/PAC), macOS Intel
x86-64 (`MACOS_X86_64`, AppleClang/Clang), and Windows x86-64 (`WIN_X86_64`,
MSVC `cl.exe` + `ml64.exe` or `clang-cl`, zero GNU binutils), with automatic
fallback to the portable C backend (`PORTABLE`, retaining Hybrid GC and C-level
backports) on Apple Silicon and sanitizers. Worker startup configures
`quickjs-turbo` explicitly via `quickjs-turbo.h` (`QJS_SetTurboDefaults` in
`src/worker_main.cc`) with `own_bytecode_tail = QJS_TURBO_OFF` and
`property_ic = QJS_TURBO_ON` (`property_ic_warmup = 12`), plus a pre-`READY`
`JS_PurgePropertyIC(rt)` pass in `src/worker_runtime.cc`.
See [source formatting](../guides/format-source.md) for exact-byte
exclusions and [package architecture](../guides/install-packages.md#development-package-matrix)
for the five-platform archive matrix.

## Release and Development Channels

| Item | Current recorded value | Boundary |
| --- | --- | --- |
| Recorded development branch | `dev/v0.4.0` | Runtime-feature successor (`0.4.0`, Binding RPC v2) on top of `v0.3.2-beta.1`; `releaseQualificationInherited:false` |
| Experimental prerelease | [v0.3.2-beta.1](https://github.com/rowanellis-113/capsid/releases/tag/v0.3.2-beta.1) | Current `0.3.2-beta` (Config v2, Elastic Pools, C ABI v8, FetchRPC v4-only); also GitHub `Latest` |
| GitHub Latest (recorded default) | [v0.3.2-beta.1](https://github.com/rowanellis-113/capsid/releases/tag/v0.3.2-beta.1) | Experimental prerelease default download, not the `0.2.x` stable line |
| Stable `0.2.x` release | [v0.2.2](https://github.com/rowanellis-113/capsid/releases/tag/v0.2.2) | Install by tag |
| Beta runtime version | `0.3.2` | An archive/runtime string, not a stable-channel claim |
| Qualified beta source | `288955124e23eb88060b8267ea93e3d0e6a4fbbe` | Tree `8e5bafde59408ffeb2344e0bdae15b699c939d3b`; 5515 frozen git entries |
| Published beta interface | C ABI v8; Build Info v2; capability policy v2 | Rebuild embedders against ABI v8 |
| Published beta protocol | FetchRPC v4-only | Legacy FetchRPC v3 retired |
| Pinned vendor (`v0.3.2-beta.1`) | txiki.js `v26.6.0` + quickjs-turbo `v0.15.1-turbo.8` (`8fcb4751`), 79 SDK patches | Same engine baseline as `v0.3.1-beta.1` |

The [read-only release capture](../data/release-status.json) records its actual UTC
observation, 11 asset IDs/sizes/digests and stable/beta flags. The
[release guide](../guides/install-packages.md) explains installation and package identity.

## Delivered Scope and Security Boundary

The `0.3.2-beta` release includes Config v2 (`capsid/host-v2` / `capsid/app-v2`),
Elastic OS Process Pools (scale-to-zero, bounded cold queueing, two-stage
tenant-fair eviction, and delegated tenant `cgroup v2` `memory.max`), C ABI v8,
and FetchRPC v4-only WebSocket support (inbound `WebSocketPair`/101, outbound
`ws`/`wss`, channel/result/terminal confirmation, Upgrade-fetch metadata/body
fallback and retained incoming connection ownership).
On `dev/v0.4.0`, Bindings advance to **Binding RPC v2** (preserving `capsid/binding-v1` manifests and the zero-Binding single-runtime path) with fair multi-runtime scheduling, three-tier quotas, bidirectional credit-based stream proxies, controlled `Request`/`Response` proxies, and `RpcTarget` remote object handles; published `v0.3.2-beta.1` binaries retain Binding v1.
The ordinary handler is still `fetch(request)`, not a complete Wrangler,
Cloudflare platform or `fetch(request, env, ctx)` compatibility layer.

The unified local dev Host (`capsid-host --app-dir`) supports trusted development and
integration on Linux, macOS and native Windows (replacing the `single-worker`/`static-pool` modes in `v0.3.1-beta.1`). Linux managed mode is the
first-party production path, but its deployment interface remains experimental.
Untrusted workloads require a verified Linux strict sandbox and surrounding
policy/resource/network boundaries. Native macOS/Windows do not provide that
production isolation guarantee. Successful startup or CI is not a substitute.

Use [architecture](../design/runtime-architecture.md), [embedding](embedding-api.md),
[Host configuration](host-configuration.md), [Linux sandboxing](linux-sandbox.md) and
[security policy](../../SECURITY.md) for the exact boundaries.

## Latest Published Binary Qualification

All of these bind to the `v0.3.2-beta.1` source (`288955124e23eb88060b8267ea93e3d0e6a4fbbe`), not to a later docs/data commit:

| Required scope | Actual GitHub Actions run |
| --- | --- |
| Branch Testing validity (all 8 hosted jobs passed, Attempt 2) | [45978674620](https://github.com/rowanellis-113/capsid/actions/runs/45978674620) |
| Pinned formatter and protected-input controls | [46005292962](https://github.com/rowanellis-113/capsid/actions/runs/46005292962) |
| Five-platform tag Release validation and native packaging (all 13 jobs passed, Attempt 6) | [46006877723](https://github.com/rowanellis-113/capsid/actions/runs/46006877723) |

All 11 published `v0.3.2-beta.1` assets match the five original archives and
sidecars from run `46006877723` plus `install.sh` at `28895512`, as recorded
in [the final publication review](../evidence/v0.3.2/2026-11-28-beta1-publication/beta1-publication-final-r1.json).

Earlier beta records are preserved unchanged:
[`v0.3.1-beta.1` (`845ae5d`)](../evidence/v0.3.1/2026-11-22-beta1-publication/beta1-publication-final-r1.json),
[`v0.3.0-beta.5` (`c7a80b9`)](../evidence/v0.3.0/2026-11-21-beta5-publication/beta5-publication-final-r1.json),
`v0.3.0-beta.4` (`07db439`, no published Release),
[`v0.3.0-beta.3`](../evidence/v0.3.0/2026-11-18-beta3-publication/beta3-publication-final-r1.json),
`v0.3.0-beta.2` (`87ed4630fa57e360ac0a3a8158218398250c8035`, marked Pre-release, with
[beta2-publication-final-r1.json](../evidence/v0.3.0/2026-11-13-ci-resumption/beta2-publication-final-r1.json))
and the failed `v0.3.0-beta.1` tag (`16f4354d353e6ab044394efe85438f040741c033`,
whose checkout failed before product builds/tests).

## Standards and Compatibility Data

- Profile: `CAPSID-MIN-2025-subset-v0`.
- WPT commit: `1985b47aa8972a970f005957f2bfa036da1787c6`; 84 selected files.
- Current compatibility classification:
  `CAPSID-WORKERD-v1.20261010.1-default-v2`.
- Five raw file failures remain: rejection support, PromiseRejectionEvent
  constructor, MessagePort initial-disabled, Headers basic and Headers errors.
- `ordinarySuitePassed:false`; `standardsConformanceClaimed:false`.

The v2 mapping preserves the four declared nested-task timeouts and other
exact reference conflicts. It does not accept an arbitrary timeout, cleanup
failure, changed source or extra failure. See [conformance](web-conformance.md)
and [testing](../guides/run-tests.md). Required compatibility acceptance must not be
reported as complete WPT, browser or workerd conformance.

## Latest Accepted Local Performance Record (`dev/v0.3.2` · `24671d9`)

The latest accepted local performance session was measured on **2026-11-27** using a clean local
Release/LTO rebuild of `24671d9957eee59d93564095c8b3af830301104f` (`quickjs-turbo`
`v0.15.1-turbo.8`, ABI v8). It does **not** measure the distributed release binaries
directly. It uses `capsid-perf-lane-v3` (pure-JS seven-stack matrix across
three reference tiers plus `dynamic-api`).

| Metric | Recorded Capsid value |
| --- | ---: |
| JSON 1 KiB QPS | 41,124.8 |
| JSON 16 KiB QPS | 23,202.4 |
| Stream 1 KiB QPS | 14,675.0 |
| Dynamic API QPS | 19,511.5 |
| Low-load service-tree PSS | 16.1 MiB |
| Loaded service-tree PSS | 19.3 MiB |
| Warm workers at 10 KiB / 100 KiB / 1 MiB, 2048 MiB budget | 705 / 501 / 135 |

Session: `local-wsl-v032-dev-v3-20261127T055316Z-24671d9`. Machine: i5-12400F, 12 logical CPUs, measured set 0–3
under WSL2. Cold, QPS, density and whole-session replay all passed (`4533`
evidence files). Physical Windows-host exclusivity is **not** claimed.

The earlier `v0.3.1-beta.1` (`6d4cc61`), `v0.3.0-beta.5` (`c7a80b9`, QuickJS-NG 0.15.1), untagged beta.4 `v3`
snapshot (`82eaab9`) and archived `v1` baseline (`485b29b`) are preserved
unchanged. The historical `v0.2.2` observation remains unverified under the
current archive contract. Different reference versions, different matrix
contracts and unverified historical points do not support a formal paired trend.

See the [performance reference](benchmarks.md),
[session review](../data/performance-review-v032.json), [measured source manifest](../data/performance-source-manifest-v032.json)
and [site/transport instructions](../../perf-site/README.md).

## Documentation Coverage and Historical Records

The [coverage inventory](../data/documentation-coverage.json) classifies every page as a
current guide/reference, dated plan/journal or immutable evidence snapshot. A
link audit is not a claim that every page was rewritten or every example run.
Current references are checked against the source/profile above; dated evidence
is checked for preservation, not updated to different numbers.

Large execution journals formerly prepended to current reference pages are
preserved as exact text snapshots with a hash inventory and legacy-anchor
links. The [history index](../history/README.md) explains their scope. No old
handoff is an instruction to restart a completed release or benchmark campaign.
