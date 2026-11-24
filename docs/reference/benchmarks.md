# Performance: Current Measurements and Methodology

Reviewed 2026-11-24. The latest accepted measurement uses a **local WSL2
Release/LTO rebuild** of the untagged `dev/v0.3.2` snapshot (`56ed405`,
powered by `quickjs-turbo v0.15.1-turbo.8` native register-frame VM, explicit
density-preserving PIC defaults, and the `v0.3.2` elastic-pool / eviction /
Config v2 Host). It measures that build, **not any distributed release package**.
The [current status](release-status.md) and
[generated data](../data/current-status.json) retain the published `v0.3.1-beta.1`
release status separately from this development-branch measurement.

## Latest Accepted Record (`capsid-perf-lane-v3`)

| Field | Recorded value |
| --- | --- |
| Measured source | `56ed405fae71157b3767940e3d3b2ca3ca469f86` (clean untagged `dev/v0.3.2` commit; `sourceTag: ""`) |
| Measured tree | `c120d56d214017a8fa87b5c961f9f4d1113f4583` |
| Session | `local-wsl-v032-dev-v3-20261124T075629Z-56ed405` |
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
| Capsid | T1 · Strict Linux OS sandbox (NS+Landlock+Seccomp) | 23099.6 | 14462.0 | 11200.0 | 13958.0 | 15.5 | 19.4 | 1190.70 |
| workerd | T1 · V8 Isolate | 25342.0 | 19436.3 | 18898.9 | 16910.7 | 56.3 | 543.8 | 46.60 |
| Deno | T1 · Permission sandbox (`--allow-net=127.0.0.1`) | 137519.8 | 43836.4 | 83320.7 | 54121.7 | 41.4 | 63.9 | 2152.11 |
| Node `--jitless` | T2 · Single process (no sandbox, V8 no-JIT) | 11804.8 | 11913.6 | 10230.9 | 8641.8 | 85.2 | 94.8 | 124.52 |
| Bun `--jitless` | T2 · Single process (no sandbox, `BUN_JSC_useJIT=0`) | 133123.4 | 42462.1 | 118664.5 | 42462.9 | 28.6 | 31.5 | 4226.14 |
| Node.js | T3 · Single process (no sandbox, V8 JIT) | 106216.3 | 40116.1 | 87076.0 | 50041.0 | 88.5 | 167.0 | 636.03 |
| Bun | T3 · Single process (no sandbox, JSC JIT) | 139509.2 | 43998.7 | 133433.0 | 75766.7 | 30.8 | 40.4 | 3453.20 |

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
| Capsid · bytecode | 9.10 ms / 7.4 MiB | 10.44 ms / 7.9 MiB | 20.34 ms / 13.8 MiB | 0.076–0.078 |
| Capsid · source | 10.03 ms / 7.3 MiB | 14.64 ms / 8.5 MiB | 72.59 ms / 18.5 MiB | 0.075–0.078 |
| workerd | 18.46 ms / 58.9 MiB | 20.27 ms / 60.8 MiB | 39.72 ms / 73.9 MiB | 0.215–0.227 |
| Deno | 45.74 ms / 38.6 MiB | 51.16 ms / 40.7 MiB | 123.93 ms / 46.5 MiB | 0.124–0.127 |
| Bun `--jitless` | 9.12 ms / 27.7 MiB | 14.42 ms / 29.0 MiB | 66.03 ms / 38.5 MiB | 0.112–0.114 |
| Bun | 14.28 ms / 29.6 MiB | 15.26 ms / 31.2 MiB | 66.28 ms / 40.1 MiB | 0.112–0.116 |
| Node.js | 49.00 ms / 88.7 MiB | 49.20 ms / 89.2 MiB | 70.60 ms / 104.3 MiB | 0.193–0.204 |
| Node `--jitless` | 79.96 ms / 85.5 MiB | 79.72 ms / 86.2 MiB | 100.56 ms / 100.6 MiB | 0.185–0.198 |

The C-ABI probe itself is excluded from cold worker PSS. Other stacks use the
recorded HTTP client/transport rather than the IPC entry, so transport overhead
must not be presented as an engine-only comparison. All sizes/stacks and the
matching binary/compiler identities are in the site data and archives.

Density uses a 2048 MiB budget and original coverage/startup/request deadlines.
Every worker/cell must complete one cold or 100 warm requests; counts below are
budget-limited, not arbitrary cap-limited observations.

| Stack | Phase | 10 KiB | 100 KiB | 1 MiB |
| --- | --- | ---: | ---: | ---: |
| Capsid workers | Cold | 720 | 506 | 135 |
| Capsid workers | Warm | 711 | 505 | 135 |
| workerd cells | Cold | 947 | 808 | 254 |
| workerd cells | Warm | 571 | 525 | 264 |

These sampled windows do not prove long-duration memory retention or universal
maximum density. Heap diagnostics, service-tree PSS, individual worker cost and
pool counts are different metrics and must not be substituted for each other.

## Recent Session Progression (`v0.3.0-beta.5` → `v0.3.1-beta.1` → `dev/v0.3.2`)

All three records below ran on the same WSL2 host with the same pinned
reference binaries (`workerd v1.20261121.1`, `Node v26.13.1`, `Bun 1.4.5`,
`Deno 2.9.8`) under `capsid-perf-lane-v3`, as separate sessions (not paired
ABBA trials; physical Windows-host exclusivity is not claimed):

| Dimension | `v0.3.0-beta.5` (`c7a80b9`, QuickJS-NG) | `v0.3.1-beta.1` (`6d4cc61`, `turbo.7`) | `dev/v0.3.2` (`56ed405`, `turbo.8`) |
| --- | ---: | ---: | ---: |
| `dynamic-api` QPS (64 conn) | 9,808.1 | 14,421.9 | **13,958.0** |
| `matrix-json-1k` QPS (64 conn) | 21,746.6 | 24,186.0 | **23,099.6** |
| `matrix-json-16k` QPS (64 conn) | 14,194.6 | 14,441.4 | **14,462.0** |
| `stream` QPS (64 conn) | 10,649.7 | 11,153.2 | **11,200.0** |
| Trickle PSS (10 req/s) | 15.0 MiB | 15.7 MiB (`8.3 / 7.5`) | **15.5 MiB** (`8.1 / 7.4`) |
| Loaded PSS (64 conn) | 18.5 MiB | 19.6 MiB (`9.8 / 9.7`) | **19.4 MiB** (`9.8 / 9.6`) |
| 1K QPS / MiB | 1,175.5 | 1,234.0 | **1,190.7** |
| 1 MiB Bytecode Cold Start / PSS | 20.43 ms / 14.3 MiB | 20.24 ms / 16.0 MiB | **20.34 ms / 13.8 MiB** |
| 2 GiB Density `10k` (cold / warm) | 735 / 729 | 721 / 715 (99.2% warm) | **720 / 711** (98.8% warm) |
| 2 GiB Density `100k` (cold / warm) | 503 / 503 | 497 / 496 (99.8% warm) | **506 / 505** (99.8% warm) |
| 2 GiB Density `1m` (cold / warm) | 128 / 128 | 127 / 127 (100.0% warm) | **135 / 135** (100.0% warm) |

Note: across the `2026-11-24` session, all six pinned reference stacks also
recorded ~3%–14% lower raw QPS than on `2026-11-22` (e.g. `workerd` `-3.0%` to
`-13.6%`, `Node` `-2.5%` to `-8.8%`, `Bun` `-3.0%` to `-4.9%`, `Deno` `-3.0%`
to `-7.1%`), consistent with cross-session WSL2 host variance, while Capsid's
100 KiB (+1.8%) and 1 MiB (+6.3%) density pools and 1 MiB bytecode PSS
(`13.8 MiB` vs `16.0 MiB`) improved measurably.

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
