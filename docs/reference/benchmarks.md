# Performance: Current Measurements and Methodology

Reviewed 2026-11-24. The latest accepted measurement uses a **local WSL2
Release/LTO rebuild** of the untagged `dev/v0.3.2` snapshot (`d0adc21`,
powered by `quickjs-turbo v0.15.1-turbo.8` native register-frame VM, HTTP/stream
fast paths with IPC event batching, explicit density-preserving PIC defaults,
and the `v0.3.2` elastic-pool / eviction / Config v2 Host). It measures that
build, **not any distributed release package**. The
[current status](release-status.md) and [generated data](../data/current-status.json)
retain the published `v0.3.1-beta.1` release status separately from this
development-branch measurement.

## Latest Accepted Record (`capsid-perf-lane-v3`)

| Field | Recorded value |
| --- | --- |
| Measured source | `d0adc21fcfb5bf75770977c8f1b0ba3c3c0b07b6` (clean untagged `dev/v0.3.2` commit; `sourceTag: ""`) |
| Measured tree | `180f6ad1d08f6bc55ab1b8e54d2915472f36c672` |
| Session | `local-wsl-v032-dev-v3-20261124T141726Z-d0adc21` |
| Contract / matrix | `capsid-perf-lane-v3` (pure-JS seven-stack across three reference tiers + `dynamic-api`) |
| Completed measurement date | 2026-11-24 UTC |
| Machine/lane | i5-12400F; 12 logical CPUs; measured set 0–3; WSL2 Linux |
| Runtime posture | Clean local Release/LTO (`CAPSID_USE_MIMALLOC=OFF`, `quickjs-turbo v0.15.1-turbo.8` `LINUX_X86_64` native register-frame VM, `QJS_SetTurboDefaults` PIC-only `own_bytecode_tail=OFF` + `property_ic_warmup=12` + pre-`READY` `JS_PurgePropertyIC`); strict C-ABI cold workers; managed/strict serving and density |
| Reference binaries | workerd `v1.20261121.1`, Node.js `v26.13.1`, Bun `1.4.5`, Deno `2.9.8` |
| Record status | All three lanes and whole-session replay passed (`4533` evidence files) |
| Distributed packages | Not measured; release packaging checks are separate |

The earlier `v0.3.1-beta.1` local rebuild (`6d4cc61`, `quickjs-turbo v0.15.1-turbo.7`),
`v0.3.0-beta.5` (`c7a80b9`, QuickJS-NG 0.15.1), and untagged `v0.3.0-beta.4`
development snapshot (`82eaab9`) `v3` records remain preserved as separate
selectable points in `perf-site/data.js`.

## Serving and Memory (Three-Tier Pure-JS Matrix)

These are medians of five 15-second measured rounds, after five seconds of
warm-up, using 64 keep-alive connections and 64 in-flight requests. Each round
verifies exact frozen bytes (`exact-bytes-v2`, zero errors/timeouts/mismatches).
`dynamic-api` executes per-request `new URL` query parsing, header reads, an
`async` microtask boundary, dynamic 8-item JSON order construction, FNV-1a
checksum calculation (`x-bench-checksum`), and `JSON.stringify`.

| Stack | Reference tier / isolation | JSON 1 KiB | JSON 16 KiB | Stream 1 KiB | Dynamic API | Trickle PSS MiB | Loaded PSS MiB | 1K QPS/MiB |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Capsid | T1 · Strict Linux OS sandbox (NS+Landlock+Seccomp) | 39802.4 | 23153.5 | 14263.6 | 20380.6 | 15.8 | 19.8 | 2010.22 |
| workerd | T1 · V8 Isolate | 26006.4 | 19771.7 | 19673.8 | 19281.9 | 56.4 | 544.0 | 47.81 |
| Deno | T1 · Permission sandbox (`--allow-net=127.0.0.1`) | 144741.4 | 47080.8 | 85487.8 | 56479.0 | 40.8 | 63.3 | 2286.59 |
| Node `--jitless` | T2 · Single process (no sandbox, V8 no-JIT) | 12132.3 | 12341.7 | 10636.5 | 8857.7 | 77.6 | 86.4 | 140.42 |
| Bun `--jitless` | T2 · Single process (no sandbox, `BUN_JSC_useJIT=0`) | 139679.8 | 44448.1 | 124094.2 | 41799.7 | 25.3 | 28.2 | 4953.18 |
| Node.js | T3 · Single process (no sandbox, V8 JIT) | 111183.8 | 42788.7 | 89321.6 | 52798.5 | 80.9 | 153.6 | 723.85 |
| Bun | T3 · Single process (no sandbox, JSC JIT) | 142293.8 | 44784.5 | 134532.2 | 77000.2 | 27.6 | 38.3 | 3715.24 |

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
| Capsid · bytecode | 8.88 ms / 7.4 MiB | 10.15 ms / 8.0 MiB | 20.22 ms / 13.8 MiB | 0.071–0.074 |
| Capsid · source | 9.87 ms / 7.4 MiB | 14.84 ms / 8.4 MiB | 75.20 ms / 18.5 MiB | 0.076–0.077 |
| workerd | 17.44 ms / 58.8 MiB | 21.05 ms / 60.5 MiB | 43.41 ms / 73.9 MiB | 0.204–0.234 |
| Deno | 71.62 ms / 39.5 MiB | 76.10 ms / 40.4 MiB | 153.56 ms / 47.9 MiB | 0.114–0.123 |
| Bun `--jitless` | 14.44 ms / 24.1 MiB | 13.96 ms / 25.4 MiB | 60.59 ms / 34.9 MiB | 0.105–0.115 |
| Bun | 13.79 ms / 26.2 MiB | 14.78 ms / 27.8 MiB | 60.47 ms / 36.8 MiB | 0.107–0.112 |
| Node.js | 43.15 ms / 81.0 MiB | 48.25 ms / 81.5 MiB | 69.57 ms / 96.7 MiB | 0.177–0.187 |
| Node `--jitless` | 77.68 ms / 85.8 MiB | 73.86 ms / 86.4 MiB | 94.78 ms / 93.1 MiB | 0.171–0.191 |

The C-ABI probe itself is excluded from cold worker PSS. Other stacks use the
recorded HTTP client/transport rather than the IPC entry, so transport overhead
must not be presented as an engine-only comparison. All sizes/stacks and the
matching binary/compiler identities are in the site data and archives.

Density uses a 2048 MiB budget and original coverage/startup/request deadlines.
Every worker/cell must complete one cold or 100 warm requests; counts below are
budget-limited, not arbitrary cap-limited observations.

| Stack | Phase | 10 KiB | 100 KiB | 1 MiB |
| --- | --- | ---: | ---: | ---: |
| Capsid workers | Cold | 709 | 502 | 135 |
| Capsid workers | Warm | 705 | 500 | 135 |
| workerd cells | Cold | 947 | 809 | 251 |
| workerd cells | Warm | 571 | 525 | 264 |

These sampled windows do not prove long-duration memory retention or universal
maximum density. Heap diagnostics, service-tree PSS, individual worker cost and
pool counts are different metrics and must not be substituted for each other.

## Recent Session Progression (`v0.3.0-beta.5` → `v0.3.1-beta.1` → `dev/v0.3.2`)

All three records below ran on the same WSL2 host with the same pinned
reference binaries (`workerd v1.20261121.1`, `Node v26.13.1`, `Bun 1.4.5`,
`Deno 2.9.8`) under `capsid-perf-lane-v3`, as separate sessions (not paired
ABBA trials; physical Windows-host exclusivity is not claimed):

| Dimension | `v0.3.0-beta.5` (`c7a80b9`, QuickJS-NG) | `v0.3.1-beta.1` (`6d4cc61`, `turbo.7`) | `dev/v0.3.2` (`d0adc21`, `turbo.8`) |
| --- | ---: | ---: | ---: |
| `dynamic-api` QPS (64 conn) | 9,808.1 | 14,421.9 | **20,380.6** |
| `matrix-json-1k` QPS (64 conn) | 21,746.6 | 24,186.0 | **39,802.4** |
| `matrix-json-16k` QPS (64 conn) | 14,194.6 | 14,441.4 | **23,153.5** |
| `stream` QPS (64 conn) | 10,649.7 | 11,153.2 | **14,263.6** |
| Trickle PSS (10 req/s) | 15.0 MiB | 15.7 MiB (`8.3 / 7.5`) | **15.8 MiB** (`8.3 / 7.5`) |
| Loaded PSS (64 conn) | 18.5 MiB | 19.6 MiB (`9.8 / 9.7`) | **19.8 MiB** (`10.1 / 9.7`) |
| 1K QPS / MiB | 1,175.5 | 1,234.0 | **2,010.2** |
| 1 MiB Bytecode Cold Start / PSS | 20.43 ms / 14.3 MiB | 20.24 ms / 16.0 MiB | **20.22 ms / 13.8 MiB** |
| 2 GiB Density `10k` (cold / warm) | 735 / 729 | 721 / 715 (99.2% warm) | **709 / 705** (99.4% warm) |
| 2 GiB Density `100k` (cold / warm) | 503 / 503 | 497 / 496 (99.8% warm) | **502 / 500** (99.6% warm) |
| 2 GiB Density `1m` (cold / warm) | 128 / 128 | 127 / 127 (100.0% warm) | **135 / 135** (100.0% warm) |

Note: across the `d0adc21` `dev/v0.3.2` session (`perf(runtime,host): optimize HTTP/stream fast paths and IPC event batching`), Capsid outperformed `workerd` on three of four QPS workloads (`39,802.4` vs `26,006.4` on 1 KiB JSON, `23,153.5` vs `19,771.7` on 16 KiB JSON, and `20,380.6` vs `19,281.9` on `dynamic-api`) while preserving `15.8 / 19.8 MiB` PSS and `705 / 500 / 135` warm workers inside 2 GiB (`+6.3%` higher 1 MiB density than `v0.3.1-beta.1`).

## Evidence and Replay

The [session review](../data/performance-review-v032.json) binds the three successful
lanes, four quiet windows, twelve density boundaries, nine executables and
clean untagged Git tree ([source manifest](../data/performance-source-manifest-v032.json)).
Physical Windows-host exclusivity is **not** claimed.

Five complete records are retained: `dev/v0.3.2` local rebuild (`4533`
evidence files), `v0.3.1-beta.1` local rebuild (`4481` files;
[review](../data/performance-review-v031-beta1.json),
[source manifest](../data/performance-source-manifest-v031-beta1.json)),
`v0.3.0-beta.5` local rebuild (`4546` files;
[review](../data/performance-review-v030-beta5.json)), the earlier untagged beta.4
`v3` point (`4533` files; [review](../data/performance-review.json)), and the
archived `v1` cross-language point (`5123` files;
[source manifest](../data/performance-source-manifest.json)). All `23216` packed
files (`23201` non-checksum evidence files plus 15 checksum manifests) are
replayed from 15 verified packed transports.
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
