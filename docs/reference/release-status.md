# Current Status and Data Scope

The [machine-readable release record](../data/current-status.json) binds the 2026-11-22 publication, source locks and local benchmark to their recorded inputs.

## Development Checkout

The v0.3.2 development SDK requires ABI v8 and FetchRPC v4 with Build Info v2 and Capability Policy v2. Rebuild embedders against this SDK. The working branch `dev/v0.3.2` delivers Config v2 (`capsid/host-v2` / `capsid/app-v2`, retiring `host-v1` / `app-v1` and legacy `single-worker` / `static-pool` CLI modes), Elastic OS Process Pools (scale-to-zero, two-stage tenant-fair eviction, and tenant-wide `cgroup v2` `memory.max`), and its latest accepted local `capsid-perf-lane-v3` measurement at `24671d9` (see [benchmarks](benchmarks.md)). Its [design](../design/config-v2.md) and [verification record](../history/v0.3.2/README.md) track implementation and verification. The sections below retain the captured `v0.3.1-beta.1` published-release baseline separately.

### Source of the published beta

The published prerelease `v0.3.1-beta.1` (`845ae5d`, also set as GitHub `Latest`)
integrates `vendor/quickjs-turbo` (`v0.15.1-turbo.8`, commit
`8fcb4751a66986bd42b11de3012e967d77790923`) on top of pinned `vendor/txiki.js`
(`v26.6.0`) through `patches/txiki/0079-quickjs-turbo-integration.patch`
(79 SDK patches total) while keeping the public C ABI v7 and FetchRPC v3/v4 wire
protocol unchanged. The recorded `dev/v0.3.1` checkout is a `test-harness-successor`
(`releaseQualificationInherited:false`) that updates release publication
metadata, documentation, `perf-site` installation commands and the
`benchmark_loadgen_correctness` CTest serialization/timeout guard
(`cmake/build_tests.cmake`, `tests/test_benchmark_loadgen.py`) on top of
`845ae5d` with identical runtime and vendor inputs.
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
`src/worker_main.cc`, replacing constructor/setenv hooks) with
`own_bytecode_tail = QJS_TURBO_OFF` (PIC-only mode without ~634 KiB/worker
eager inline-hint tails) and `property_ic = QJS_TURBO_ON` with
`property_ic_warmup = 12` (lazy warmup allocation, overridable via environment),
plus a pre-`READY` `JS_PurgePropertyIC(rt)` pass in `src/worker_runtime.cc` so
high-density worker packing and native Property IC performance coexist. The
derived release qualification record below describes the published
`v0.3.1-beta.1` (`845ae5d`) release, while the latest accepted local performance
record measures the clean local WSL2 Release/LTO rebuild of `6d4cc61` (tagged
prior to the Alpine musl build-script fix in `v0.15.1-turbo.8` and docs/archive
packaging in `845ae5d`).
See [source formatting](../guides/format-source.md) for exact-byte
exclusions and [package architecture](../guides/install-packages.md#development-package-matrix)
for the five-platform archive matrix.

## Release and Development Channels

| Item | Current recorded value | Boundary |
| --- | --- | --- |
| Recorded release-data branch | `dev/v0.3.1` | Post-release test-harness successor; does not inherit `v0.3.1-beta.1` binary qualification |
| Experimental prerelease | [v0.3.1-beta.1](https://github.com/rowanellis-113/capsid/releases/tag/v0.3.1-beta.1) | Current `0.3.1-beta` (`quickjs-turbo` `v0.15.1-turbo.8`); also GitHub `Latest` |
| GitHub Latest (recorded default) | [v0.3.1-beta.1](https://github.com/rowanellis-113/capsid/releases/tag/v0.3.1-beta.1) | Experimental prerelease default download, not the `0.2.x` stable line |
| Stable `0.2.x` release | [v0.2.2](https://github.com/rowanellis-113/capsid/releases/tag/v0.2.2) | Install by tag |
| Beta runtime version | `0.3.1` | An archive/runtime string, not a stable-channel claim |
| Qualified beta source | `845ae5dee5833c1bed1c2630cfb65e6ed76af7d7` | Tree `7894f8a984c7344a904ac0e08c6099e282036b1a`; 5492 frozen git entries |
| Published beta interface | C ABI v7; capability policy v2 | Existing layouts remain frozen; new interfaces are additive |
| Published beta protocol | FetchRPC v3 by default; explicit negotiated v4 | Never mix versions on one connection |
| Pinned vendor (`v0.3.1-beta.1`) | txiki.js `v26.6.0` + quickjs-turbo `v0.15.1-turbo.8` (`8fcb4751`), 79 SDK patches | Earlier `v0.3.0-beta.5` retained txiki.js `v26.6.0` (QuickJS-NG 0.15.1), 78 SDK patches |

The [read-only release capture](../data/release-status.json) records its actual UTC
observation, 11 asset IDs/sizes/digests and stable/beta flags. The
[release guide](../guides/install-packages.md) explains installation and package identity.

## Delivered Scope and Security Boundary

The `0.3.x` beta line (`0.3.1-beta`, continuing from `0.3.0-beta`) includes the bounded FetchRPC v4/WebSocket foundation: inbound
WebSocketPair/101, outbound ws/wss, channel/result/terminal confirmation,
Upgrade-fetch metadata/body fallback and retained incoming connection ownership.
Bindings remain v1 with independent User/Binding runtimes and operation gates.
The ordinary handler is still `fetch(request)`, not a complete Wrangler,
Cloudflare platform or `fetch(request, env, ctx)` compatibility layer.

Local single-worker/static-pool modes support trusted development and
integration on Linux, macOS and native Windows. Linux managed mode is the
first-party production path, but its deployment interface remains experimental.
Untrusted workloads require a verified Linux strict sandbox and surrounding
policy/resource/network boundaries. Native macOS/Windows do not provide that
production isolation guarantee. Successful startup or CI is not a substitute.

Use [architecture](../design/runtime-architecture.md), [embedding](embedding-api.md),
[Host configuration](host-configuration.md), [Linux sandboxing](linux-sandbox.md) and
[security policy](../../SECURITY.md) for the exact boundaries.

## Latest Published Binary Qualification

All of these bind to the beta source above, not to a later docs/data commit:

| Required scope | Actual GitHub Actions run |
| --- | --- |
| Full Release/LTO, sanitizer/provider, native platform, fuzz/index qualification (via Release `validation`) | [45008292926](https://github.com/rowanellis-113/capsid/actions/runs/45008292926) |
| Pinned formatter and protected-input controls | [45008288654](https://github.com/rowanellis-113/capsid/actions/runs/45008288654) |
| Five-platform tag Release validation and native packaging (all 13 jobs passed, attempt 1) | [45008292926](https://github.com/rowanellis-113/capsid/actions/runs/45008292926) |

All 11 published `v0.3.1-beta.1` assets match the five original archives and
sidecars from run `45008292926` plus `install.sh` at `845ae5d`, as recorded
in [the final publication review](../evidence/v0.3.1/2026-11-22-beta1-publication/beta1-publication-final-r1.json).
Branch [Testing validity](https://github.com/rowanellis-113/capsid/actions/runs/45008288717) attempt 1 is retained.

Earlier `0.3.0-beta` records are preserved unchanged:
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

## Latest Accepted Local Performance Record

The latest accepted session was measured on **2026-11-22** using a clean local
Release/LTO rebuild of `6d4cc616619dfd1ada0e484c2c47b79046d680dc` (`quickjs-turbo`
native register-frame VM, recorded prior to the `v0.15.1-turbo.8` Alpine musl
build-script fix in published `v0.3.1-beta.1` `845ae5d`). It does **not** measure
the distributed release binaries directly. It uses `capsid-perf-lane-v3` (pure-JS seven-stack matrix across
three reference tiers plus `dynamic-api`).

| Metric | Recorded Capsid value |
| --- | ---: |
| JSON 1 KiB QPS | 24,186.0 |
| JSON 16 KiB QPS | 14,441.4 |
| Stream 1 KiB QPS | 11,153.2 |
| Dynamic API QPS | 14,421.9 |
| Low-load service-tree PSS | 15.7 MiB |
| Loaded service-tree PSS | 19.6 MiB |
| Warm workers at 10 KiB / 100 KiB / 1 MiB, 2048 MiB budget | 715 / 496 / 127 |

Session: `local-wsl-v031-beta1-v3-20261122T064911Z-6d4cc61`. Machine: i5-12400F, 12 logical CPUs, measured set 0–3
under WSL2. Cold, QPS, density and whole-session replay all passed (`4481`
evidence files). Physical Windows-host exclusivity is **not** claimed.

The earlier `v0.3.0-beta.5` (`c7a80b9`, QuickJS-NG 0.15.1), untagged beta.4 `v3`
snapshot (`82eaab9`) and archived `v1` baseline (`485b29b`) are preserved
unchanged. The historical `v0.2.2` observation remains unverified under the
current archive contract. Different reference versions, different matrix
contracts and unverified historical points do not support a formal paired trend.

See the [performance reference](benchmarks.md),
[session review](../data/performance-review-v031-beta1.json), [measured source manifest](../data/performance-source-manifest-v031-beta1.json)
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
