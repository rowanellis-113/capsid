# Performance: Current Measurements and Methodology

Reviewed 2026-11-22. The latest accepted measurement uses a **local WSL2
Release/LTO rebuild** of the pre-publication `v0.3.1-beta.1` snapshot (`6d4cc61`,
recorded prior to the `v0.15.1-turbo.8` Alpine musl build-script fix and packaging
commit `845ae5d` that the published `v0.3.1-beta.1` tag points to; powered by
`quickjs-turbo` native register-frame VM + explicit density-preserving PIC
defaults). It measures that build, **not the distributed release packages**. The
[current status](release-status.md) and [generated data](../data/current-status.json)
distinguish the measured build from release qualification and older records.

## Latest Accepted Record (`capsid-perf-lane-v3`)

| Field | Recorded value |
| --- | --- |
| Measured source | `6d4cc616619dfd1ada0e484c2c47b79046d680dc` (pre-publication `v0.3.1-beta.1` snapshot; published tag is `845ae5d`) |
| Measured tree | `126888a657e26547c689bca942aeabfdb6f5a4f6` |
| Session | `local-wsl-v031-beta1-v3-20261122T064911Z-6d4cc61` |
| Contract / matrix | `capsid-perf-lane-v3` (pure-JS seven-stack across three reference tiers + `dynamic-api`) |
| Completed measurement date | 2026-11-22 UTC |
| Machine/lane | i5-12400F; 12 logical CPUs; measured set 0–3; WSL2 Linux |
| Runtime posture | Clean local Release/LTO (`CAPSID_USE_MIMALLOC=OFF`, `quickjs-turbo` `LINUX_X86_64` native register-frame VM, `QJS_SetTurboDefaults` PIC-only `own_bytecode_tail=OFF` + `property_ic_warmup=12` + pre-`READY` `JS_PurgePropertyIC`); strict C-ABI cold workers; managed/strict serving and density |
| Reference binaries | workerd `v1.20261121.1`, Node.js `v26.13.1`, Bun `1.4.5`, Deno `2.9.8` |
| Record status | All three lanes and whole-session replay passed (`4481` evidence files) |
| Distributed packages | Not measured; their release checks are separate |

The earlier `v0.3.0-beta.5` (`c7a80b9`, QuickJS-NG 0.15.1) and untagged
`v0.3.0-beta.4` development snapshot (`82eaab9`) `v3` records remain preserved
as separate selectable points in `perf-site/data.js`.

## Serving and Memory (Three-Tier Pure-JS Matrix)

These are medians of five 15-second measured rounds, after five seconds of
warm-up, using 64 keep-alive connections and 64 in-flight requests. Each round
verifies exact frozen bytes (`exact-bytes-v2`, zero errors/timeouts/mismatches).
`dynamic-api` executes per-request `new URL` query parsing, header reads, an
`async` microtask boundary, dynamic 8-item JSON order construction, FNV-1a
checksum calculation (`x-bench-checksum`), and `JSON.stringify`.

| Stack | Reference tier / isolation | JSON 1 KiB | JSON 16 KiB | Stream 1 KiB | Dynamic API | Trickle PSS MiB | Loaded PSS MiB | 1K QPS/MiB |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Capsid | T1 · Strict Linux OS sandbox (NS+Landlock+Seccomp) | 24186.0 | 14441.4 | 11153.2 | 14421.9 | 15.7 | 19.6 | 1233.98 |
| workerd | T1 · V8 Isolate | 26134.4 | 21091.6 | 19567.7 | 19572.0 | 56.0 | 521.6 | 50.10 |
| Deno | T1 · Permission sandbox (`--allow-net=127.0.0.1`) | 148062.2 | 45184.8 | 87887.6 | 56575.2 | 40.9 | 64.1 | 2309.86 |
| Node `--jitless` | T2 · Single process (no sandbox, V8 no-JIT) | 12101.0 | 12459.4 | 10617.7 | 8897.6 | 56.4 | 66.0 | 183.35 |
| Bun `--jitless` | T2 · Single process (no sandbox, `BUN_JSC_useJIT=0`) | 142069.8 | 45405.9 | 125348.1 | 44439.6 | 33.3 | 36.3 | 3913.77 |
| Node.js | T3 · Single process (no sandbox, V8 JIT) | 112422.9 | 43998.4 | 89316.4 | 54687.3 | 60.6 | 135.7 | 828.47 |
| Bun | T3 · Single process (no sandbox, JSC JIT) | 145384.3 | 45879.8 | 137498.2 | 79682.4 | 35.5 | 45.7 | 3181.28 |

Trickle is 15 seconds at 10 req/s on a fresh forest, **before** loaded traffic.
PSS includes the complete owned service forest; rounded roles need not sum to
the independently rounded total. Latency percentiles in the raw summaries are
from the last measured round, not medians across five rounds.

## Cold Start and Density

Cold start ends at the first validated response. Each cell has one discarded
run plus five measured rounds; 50 warm requests and stable worker-tree PSS are
recorded separately (under `v3`, cold and QPS PSS settle checks allow an
explicit `32 KiB` allocator page-flap tolerance while density remains exact
`0 KiB`).

| Runtime | 10 KiB total ms / PSS | 100 KiB total ms / PSS | 1 MiB total ms / PSS | Warm request ms |
| --- | ---: | ---: | ---: | ---: |
| Capsid · bytecode | 8.66 ms / 7.2 MiB | 9.61 ms / 8.2 MiB | 20.24 ms / 16.0 MiB | 0.072–0.075 |
| Capsid · source | 9.97 ms / 7.4 MiB | 13.79 ms / 8.5 MiB | 70.63 ms / 19.5 MiB | 0.072–0.078 |
| workerd | 17.72 ms / 58.3 MiB | 19.48 ms / 60.0 MiB | 37.66 ms / 73.2 MiB | 0.203–0.209 |
| Deno | 34.84 ms / 38.1 MiB | 35.43 ms / 39.0 MiB | 114.10 ms / 45.8 MiB | 0.116–0.123 |
| Bun `--jitless` | 9.04 ms / 32.4 MiB | 14.23 ms / 33.7 MiB | 60.67 ms / 43.2 MiB | 0.106–0.111 |
| Bun | 8.83 ms / 34.3 MiB | 13.92 ms / 36.0 MiB | 60.42 ms / 44.9 MiB | 0.105–0.108 |
| Node.js | 49.22 ms / 60.7 MiB | 48.41 ms / 61.3 MiB | 70.10 ms / 76.3 MiB | 0.168–0.193 |
| Node `--jitless` | 74.06 ms / 90.0 MiB | 74.11 ms / 90.6 MiB | 101.21 ms / 104.9 MiB | 0.179–0.187 |

The C-ABI probe itself is excluded from cold worker PSS. Other stacks use the
recorded HTTP client/transport rather than the IPC entry, so transport overhead
must not be presented as an engine-only comparison. All sizes/stacks and the
matching binary/compiler identities are in the site data and archives.

Density uses a 2048 MiB budget and original coverage/startup/request deadlines.
Every worker/cell must complete one cold or 100 warm requests; counts below are
budget-limited, not arbitrary cap-limited observations.

| Stack | Phase | 10 KiB | 100 KiB | 1 MiB |
| --- | --- | ---: | ---: | ---: |
| Capsid workers | Cold | 721 | 497 | 127 |
| Capsid workers | Warm | 715 | 496 | 127 |
| workerd cells | Cold | 947 | 808 | 250 |
| workerd cells | Warm | 572 | 524 | 265 |

These sampled windows do not prove long-duration memory retention or universal
maximum density. Heap diagnostics, service-tree PSS, individual worker cost and
pool counts are different metrics and must not be substituted for each other.

## Engine Transition Snapshot (`v0.3.1-beta.1` vs `v0.3.0-beta.5`)

Both `v0.3.1-beta.1` (`6d4cc61`, `quickjs-turbo`) and `v0.3.0-beta.5`
(`c7a80b9`, QuickJS-NG 0.15.1) were measured on the same WSL2 host with the
same pinned reference binaries (`workerd v1.20261121.1`, `Node v26.13.1`,
`Bun 1.4.5`, `Deno 2.9.8`) under `capsid-perf-lane-v3`, as separate sessions:

| Dimension | `v0.3.0-beta.5` (QuickJS-NG) | `v0.3.1-beta.1` (`quickjs-turbo`) | Relative shift |
| --- | ---: | ---: | ---: |
| `dynamic-api` QPS (64 conn) | 9,808.1 | **14,421.9** | **+47.0%** |
| `matrix-json-1k` QPS (64 conn) | 21,746.6 | **24,186.0** | **+11.2%** |
| `matrix-json-16k` QPS (64 conn) | 14,194.6 | **14,441.4** | **+1.7%** |
| `stream` QPS (64 conn) | 10,649.7 | **11,153.2** | **+4.7%** |
| Trickle PSS (10 req/s) | 15.0 MiB | **15.7 MiB** (`8.3 / 7.5`) | +0.7 MiB |
| Loaded PSS (64 conn) | 18.5 MiB | **19.6 MiB** (`9.8 / 9.7`) | +1.1 MiB |
| 1K QPS / MiB | 1,175.5 | **1,234.0** | **+5.0%** |
| 2 GiB Density `10k` (cold / warm) | 735 / 729 | **721 / 715** (99.2% warm) | -1.9% |
| 2 GiB Density `100k` (cold / warm) | 503 / 503 | **497 / 496** (99.8% warm) | -1.4% |
| 2 GiB Density `1m` (cold / warm) | 128 / 128 | **127 / 127** (100.0% warm) | -0.8% |

## Evidence and Replay

The [session review](../data/performance-review-v031-beta1.json) binds the three successful
lanes, four quiet windows, twelve density boundaries, nine executables and
clean tagged Git tree ([source manifest](../data/performance-source-manifest-v031-beta1.json)).
Physical Windows-host exclusivity is **not** claimed.

Four complete records are retained: `v0.3.1-beta.1` local rebuild (`4481`
evidence files), `v0.3.0-beta.5` local rebuild (`4546` files;
[review](../data/performance-review-v030-beta5.json)), the earlier untagged beta.4
`v3` point (`4533` files; [review](../data/performance-review.json)), and the
archived `v1` cross-language point (`5123` files;
[source manifest](../data/performance-source-manifest.json)). All `18683` packed
files (`18671` non-checksum evidence files plus 12 checksum manifests) are
replayed from 12 verified packed transports.
The [transport index](../../perf-site/archive-index.json) records every part digest,
file count and uncompressed length. Site preparation:

1. verifies part and complete transport digests;
2. restores only bounded regular portable members into a new scratch directory;
3. runs the unchanged complete lane/point validation and replays raw metrics for both `v3` and `v1`;
4. verifies every original checksum and the exact bytes published for download.

Compression changes transport, not the accepted source, samples, counts, budgets
or verdict. It is not permission to repair a failed run. The
[site instructions](../../perf-site/README.md) describe replay/downloads.

## Historical Comparisons and Limits

The archived `v1` point (`485b29b`, 2026-11-16, cross-language Capsid / workerd /
FastAPI / Flask / PHP Slim: Capsid `9341.6 / 8090.2 / 6491.7 QPS`, `14.0 / 18.0 MiB`
PSS, `738 / 509 / 128` warm workers) and the historical unverified `v0.2.2` point
(`9489.7 / 7109.1 / 6523.2 QPS`, `882 / 568 / 132` warm workers) are preserved
unchanged. Do not infer a formal paired gain or single-variable causal attribution
across different matrix contracts (`v1` vs `v3`) or unverified historical points.

Beta.4 and beta.5 are separate sessions; the reference versions changed, so the
charts do not establish a paired optimization gain between them. Physical-host
equivalence and engine-only attribution are not claimed. Diagnostic ABBA
percentages cannot be added to this result.

The normative protocol is [bench/SPEC.md](../../bench/SPEC.md); the
[archive contract](../../bench/perf-evidence.md) defines failure and provenance
rules. Source, fixture, controller, configuration and environment changes need
their own matching evidence. Required CI acceptance alone is not a benchmark.

## Historical source and preserved anchors

Read the dated checkpoints and their recorded measurements in the
[English journal](../history/archives/2026-11-17/performance-benchmarks.md.txt). They are not current release instructions.

<details>
<summary>Legacy section links</summary>

<a id="performance-evidence-rules-and-current-state"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L1) — performance-evidence-rules-and-current-state

<a id="capsid-review-phase-93"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L11) — capsid-review-phase-93

<a id="archived-section-173"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L13) — Round 93 (2026-10-20): Five-stack QPS controller and archive connected

<a id="capsid-review-phase-92"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L24) — capsid-review-phase-92

<a id="archived-section-179"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L26) — Round 92 Status (2026-10-20): QPS load correctness and original-evidence fixes; full producer continues

<a id="capsid-review-phase-91"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L35) — capsid-review-phase-91

<a id="archived-section-185"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L36) — Round 91 Status (2026-10-20): Bytecode compiler mount-compatibility fixes; full lane targets continue

<a id="capsid-review-phase-90"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L45) — capsid-review-phase-90

<a id="archived-section-191"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L46) — Round 90 Status (2026-10-20): Cold-start controller and archiving enabled; full targets continue

<a id="capsid-review-phase-89"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L59) — capsid-review-phase-89

<a id="archived-section-197"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L60) — Round 89 Status (2026-10-20): Cold probe fixed; full lane with strict targets not yet completed

<a id="1-evidence-rules"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L96) — 1-evidence-rules

<a id="conclusion-threshold"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L98) — conclusion-threshold

<a id="measurements-that-must-not-be-mixed"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L112) — measurements-that-must-not-be-mixed

<a id="result-storage-format"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L116) — result-storage-format

<a id="current-optimization-principles"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L120) — current-optimization-principles

<a id="2-test-environment-2026-10-10"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L130) — 2-test-environment-2026-10-10

<a id="3-five-stack-serving-matrix-2026-10-10-c64-5-rounds"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L156) — 3-five-stack-serving-matrix-2026-10-10-c64-5-rounds

<a id="reading-the-matrix"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L186) — reading-the-matrix

<a id="measured-density-fill-a-pool-to-2-gb-2026-10-10"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L194) — measured-density-fill-a-pool-to-2-gb-2026-10-10

<a id="4-cold-start-comparison-2026-10-10-median-ms-5-rounds"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L206) — 4-cold-start-comparison-2026-10-10-median-ms-5-rounds

<a id="5-retired-checkpoints"></a>
[Historical section](../history/archives/2026-11-17/performance-benchmarks.md.txt#L222) — 5-retired-checkpoints

</details>
