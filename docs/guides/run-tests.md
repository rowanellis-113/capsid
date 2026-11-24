# Testing and validation

Choose the checks that exercise the change. Keep the source revision, build, test selection, exit code and raw failures or skips with the result.

## Choose the Right Check

| Change | Start with | What it does not establish |
| --- | --- | --- |
| Documentation | Documentation/link/archive audit; execute changed examples where possible | Runtime or release qualification |
| Runtime or Host behavior | Relevant CTests, causal regression and adjacent lifetime/policy tests | Unselected configurations or platforms |
| Standards/framework behavior | Pinned references, actual runtime cases and negative controls | Full browser or ECMA-429 conformance |
| Sanitizer changes | Dedicated instrumented build with matching provider coverage | Production RSS or throughput |
| Release | Full required CI, actual platform packages, source-bound review and asset verification | Closure of every historical issue |
| Performance | The complete declared benchmark protocol and session | Acceptance from CI timing or sampled diagnostics alone |

## Documentation Checks

From the repository root, using the project's Node runtime:

```sh
node tests/audit-current-docs.mjs .
node --test tests/test_documentation_archive.mjs
node tests/audit-testing-validity-workflow.mjs .
python3 tests/test_perf_archive.py
```

The [coverage inventory](../data/documentation-coverage.json) distinguishes current
references, dated plans and exact archives. To verify the published release data, run
`python3 tools/refresh-documentation-data.py --check` in the clean recorded
`dev/v0.3.1` checkout at `a7fd47e`. The check binds that source tree to its release
and measured-build records. Site/transport changes additionally require the positive/negative
controls and full original-byte replay in [site instructions](../../perf-site/README.md).

These check local links, index reachability, security guidance, archived
qualification hashes and workflow contracts. They do not execute every command
block. Do not weaken an audit merely to accommodate stale prose; correct the
facts and keep equivalent security, provenance and archive protections.

## Local Development Tests

Prepare the [build dependencies](build-from-source.md), including all locked framework
packages. Use a separate Debug build:

```sh
cmake -S . -B build-dev -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCAPSID_ENABLE_LTO=OFF \
  -DCAPSID_BUILD_HOST=ON -DBUILD_TESTING=ON
cmake --build build-dev --parallel 2
ctest --test-dir build-dev --show-only=json-v1
```

The last command lists registrations; it does not run them. Run relevant tests
with an explicit empty-selection failure:

```sh
ctest --test-dir build-dev --no-tests=error --output-on-failure \
  -R '^(worker_memory_metrics_reclaimed|worker_memory_metrics_retained)$'
```

A broad local development run without an installed WPT checkout can explicitly
exclude only the missing-WPT sentinel:

```sh
ctest --test-dir build-dev --no-tests=error --output-on-failure \
  -E '^wpt_conformance_not_configured$'
```

**This intentionally omits WPT and is not full CI acceptance.** Other required
reference/client inputs can still fail their own checks. Do not add exclusions,
repeat failed tests until green, increase budgets or reinterpret skips as passes.
Windows commands must run natively with the prerequisites in [Windows](build-on-windows.md).

## Pinned Web Platform Tests

Obtain the repository and exact revision declared in
[`tests/wpt/manifest.json`](../../tests/wpt/manifest.json). Configure its absolute
checkout path:

```sh
cmake -S . -B build-wpt -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCAPSID_ENABLE_LTO=OFF \
  -DCAPSID_BUILD_HOST=ON -DBUILD_TESTING=ON \
  -DCAPSID_WPT_ROOT=/absolute/path/to/pinned/wpt
cmake --build build-wpt --parallel 2
ctest --test-dir build-wpt --no-tests=error --output-on-failure
```

The declared file inventory and source hashes matter, not just the number of
registered tests. The raw ordinary suite may retain known failures. Current
release CI separately checks the pinned workerd compatibility profile, retaining
raw results rather than turning ordinary failures into standards passes.

The [conformance reference](../reference/web-conformance.md) defines the profile; the
[beta qualification record](../history/v0.3.0/release-qualification.md) records the actual
accepted source and known five raw WPT file failures. Do not substitute a green
compatibility result for a claim that every upstream assertion passed.

## WebSocket and Independent Clients

The reference gate uses locked workerd, Node and Python/client identities.
For a Linux development environment with Node 24.14.0 and full Python 3.12.3:

```sh
node tools/workerd-reference.mjs install /tmp/capsid-workerd/workerd
export CAPSID_WORKERD_REF=/tmp/capsid-workerd/workerd
python3.12 tools/setup-ws-interop-python.py /tmp/capsid-ws-clients
export CAPSID_WS_INTEROP_PYTHON=/tmp/capsid-ws-clients/bin/python
export CAPSID_WS_INTEROP_NODE_PACKAGE=/tmp/capsid-ws-clients/node-ws
node tools/run-workerd-ws-gate.mjs build-dev build-dev/workerd-ws-evidence
```

Use short, private paths for native socket fixtures. The gate checks complete
registration, actual reference/product runs, semantic observations, source and
binary identities, negative controls and shutdown. Installing a reference or
running an API-only subset does not complete the gate. Managed backends require
Linux; Windows and macOS have separate native contracts, not pretend managed
passes. The platform-specific setup is in [Windows](build-on-windows.md), and behavioral
limits are in the [workerd semantics reference](../reference/websocket-semantics.md).

## Sanitizers, Fuzzing and Isolation

Use dedicated build trees for ASan, UBSan and TSan. Do not reuse their binaries
for ordinary performance evidence or combine TSan with ASan/UBSan. CMake disables
LTO for instrumentation and rejects unsupported allocator/profile combinations.

The current hosted TSan lane uses **Clang on Ubuntu**; the exact compiler,
provider flags, exclusions and budgets are in the
[checked-in workflow](../../.github/workflows/testing-validity.yml). Older journal
sections about a local GCC/musl environment are historical, not a universal
Clang prohibition.

Linux has two distinct sandbox profiles:

- `CAPSID_SANDBOX_PROFILE=production`: production isolation policy; required for
  production isolation acceptance.
- `CAPSID_SANDBOX_PROFILE=sanitizer`: diagnostic policy requiring sanitizer
  instrumentation. It permits tooling operations needed by the instruments and
  is not the production security policy.

Instrumented provider compilation must be verified; linking a sanitizer driver
against uninstrumented dependency objects is not complete coverage. Fatal
sanitizer reports and ordinary sanitizer-test failures cannot be waived as RSS
measurement differences. The narrowly scoped instrumented-memory acceptors keep
raw results and require their original conditions.

Fuzz evidence includes the actual selected harnesses, corpus, budgets, fatal
diagnostics and execution outcome. A compile or a later seed passing does not
close an earlier timeout's cause.

For sandbox checks, unavailable cgroup/namespace/filesystem prerequisites must
remain genuine skips or failures. Delegated Linux CI requires real positive
isolation execution; macOS/Windows native passes cannot replace it. Do not
change host security policy or global system settings just to make a test pass.

## Full Qualification and Evidence

The hosted validity workflow covers Release/LTO, ASan/UBSan/TSan, fuzz, native
macOS and Windows, plus a final hosted evidence index. Release packaging depends
on successful validation. Supplemental Debug and uninterrupted soak evidence is
recorded separately for the qualifying source. Follow the actual workflow and
qualification contract, not a copied historical test count.

Keep at least:

- Exact product commit/tree, dependency locks, build configuration and binary
  identities; distinguish the product checkout from a workflow overlay commit.
- Registration inventory, actual exit code, JUnit, `Testing/Temporary/LastTest.log`
  and complete relevant command logs, including failed and skipped results.
- Reference and tool identities, provider instrumentation, real protocol/OS
  observations, original selections and budgets.
- Package archives, original SHA-256 sidecars, metadata/file manifest, SBOM and
  actual native program compatibility checks.
- Separate reviews for archive integrity and semantic acceptance. An index or a
  coordinator's exit code cannot override a real failed test.

Any infrastructure-only retry needs explicit scope and authorization. Retain the
original outcome and attempt provenance; never present a retried execution as
first-attempt evidence or rerun unrelated successful jobs.

For the published beta's exact results, including the distinction between the
branch retry and first-attempt tag qualification, see
[release qualification](../history/v0.3.0/release-qualification.md). For formal
performance acceptance, use the separate [benchmark methodology](../reference/benchmarks.md).

## Historical source and preserved anchors

Read the dated checkpoints and their recorded measurements in the
[English journal](../history/archives/2026-11-17/testing.md.txt). They are not current release instructions.

<details>
<summary>Legacy section links</summary>

<a id="testing-journal-and-legacy-reference"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1) — testing-journal-and-legacy-reference

<a id="testing-and-continuous-gates"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L3) — testing-and-continuous-gates

<a id="capsid-review-phase-107"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L22) — capsid-review-phase-107

<a id="archived-section-203"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L24) — Round 107 (2026-11-07): SDK70 and branch fixes merged, continued acceptance of mainline and clean builds

<a id="capsid-review-phase-106"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L45) — capsid-review-phase-106

<a id="archived-section-209"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L47) — Round 106 (2026-11-07): Progress in verifying private contexts and unread gzip branches

<a id="capsid-review-phase-105"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L61) — capsid-review-phase-105

<a id="archived-section-215"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L63) — Update #105 (2026-11-07)

<a id="capsid-review-phase-104-in-progress"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L73) — capsid-review-phase-104-in-progress

<a id="archived-section-221"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L75) — Round 104 (2026-10-24): 480 targeted combinations in working tree, 4 C API helper response-credit fixes, and runtime timeout isolation

<a id="archived-section-224"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L81) — Working tree merged: 4 C API test helpers return response credit

<a id="archived-section-227"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L101) — New discovery: The snapshot text becomes corrupted after some transmissions due to timeouts or cancellations, leading to runtime errors.

<a id="archived-section-230"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L111) — The private JS v7 is still retained within its original scope.

<a id="archived-section-233"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L117) — Current differences and continuation order

<a id="capsid-review-phase-103"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L126) — capsid-review-phase-103

<a id="archived-section-239"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L128) — Round 103 (2026-10-21): BYOB parameter order, standard capacity rejection, and ABI self-check synchronization

<a id="capsid-review-phase-102"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L157) — capsid-review-phase-102

<a id="archived-section-245"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L159) — Round 102 (2026-10-21): Large volume reading, DefaultReader allocation, and EOF offset

<a id="capsid-review-phase-101"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L190) — capsid-review-phase-101

<a id="archived-section-251"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L192) — Round 101 (2026-10-21): Reader prototype borrowing, Float16Array, and ABI test fixes

<a id="capsid-review-phase-100"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L221) — capsid-review-phase-100

<a id="archived-section-257"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L223) — Round 100 (2026-10-21): Upgrade and rollback of the native BYOB base matrix for HTTP response bodies

<a id="capsid-review-phase-99"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L252) — capsid-review-phase-99

<a id="archived-section-263"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L254) — Round 99 (2026-10-20): Direct DefaultReader construction path for native body

<a id="capsid-review-phase-98"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L281) — capsid-review-phase-98

<a id="archived-section-269"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L283) — Round 98 (2026-10-20): Optimizing real-time allocation probes and complete spawn exception traversal in the build process

<a id="capsid-review-phase-97"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L306) — capsid-review-phase-97

<a id="archived-section-275"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L308) — Round 97 (2026-10-20): Custom configurations, v4 initialization, and actual resource recycling

<a id="capsid-review-phase-96"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L339) — capsid-review-phase-96

<a id="archived-section-281"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L341) — Round 96 (2026-10-20): Default ABI allocation failure, resource cleanup, and test coverage fixes

<a id="capsid-review-phase-95"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L353) — capsid-review-phase-95

<a id="capsid-review-phase-94"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L367) — capsid-review-phase-94

<a id="archived-section-290"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L369) — Round 94 (2026-10-20): Default coding stack fixes, complete three configuration baseline, and new differences added

<a id="capsid-review-phase-93"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L388) — capsid-review-phase-93

<a id="archived-section-296"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L390) — Round 93 (2026-10-20): Five-stack QPS controller and archive connected

<a id="capsid-review-phase-92"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L401) — capsid-review-phase-92

<a id="archived-section-302"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L403) — Round 92 Status (2026-10-20): QPS load correctness and original-evidence fixes; full producer continues

<a id="capsid-review-phase-91"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L412) — capsid-review-phase-91

<a id="archived-section-308"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L413) — Round 91 Status (2026-10-20): Bytecode compiler mount-compatibility fixes; full lane targets continue

<a id="capsid-review-phase-90"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L422) — capsid-review-phase-90

<a id="archived-section-314"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L423) — Round 90 Status (2026-10-20): Cold-start controller and archiving enabled; full targets continue

<a id="capsid-review-phase-89"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L436) — capsid-review-phase-89

<a id="archived-section-320"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L437) — Round 89 Status (2026-10-20): Cold probe fixed; full lane with strict targets not yet completed

<a id="capsid-review-phase-88"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L448) — capsid-review-phase-88

<a id="archived-section-326"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L449) — Round 88 Status (2026-10-20): Density control and archive producer are connected

<a id="capsid-review-phase-87"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L460) — capsid-review-phase-87

<a id="archived-section-332"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L461) — Round 87 Status (2026-10-20): Corrected performance collection and web evidence boundaries

<a id="capsid-review-phase-86"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L472) — capsid-review-phase-86

<a id="archived-section-338"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L473) — Round 86 Status (2026-10-20): Completed controlled build comparison for Windows stack pressure

<a id="capsid-review-phase-85"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L484) — capsid-review-phase-85

<a id="archived-section-344"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L485) — Round 85 Status (2026-10-20): 30 new transform and lifecycle scenarios completed and verified

<a id="capsid-review-phase-84"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L496) — capsid-review-phase-84

<a id="archived-section-350"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L497) — Round 84 Status (2026-10-20): Outbound fetch boundary additions completed and verified

<a id="capsid-review-phase-83"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L506) — capsid-review-phase-83

<a id="archived-section-356"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L507) — Round 83 (2026-10-20): 22 proxy cases, mandatory gate tests, and boundary coverage

<a id="capsid-review-phase-82"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L518) — capsid-review-phase-82

<a id="archived-section-362"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L519) — Round 82 Status (2026-10-20): Managed update retained local connection sources, fixed Windows incremental caching

<a id="capsid-review-phase-81"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L530) — capsid-review-phase-81

<a id="archived-section-368"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L531) — Round 81 Status (2026-10-20): Local close-delay gap resolved

<a id="capsid-review-phase-80"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L540) — capsid-review-phase-80

<a id="archived-section-374"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L541) — Round 80 Status (2026-10-20): Abnormal parent-connection pending events and local Close

<a id="capsid-review-phase-79"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L548) — capsid-review-phase-79

<a id="archived-section-380"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L549) — Round 79 Status (2026-10-20): Shared I/O lifecycle and complete mandatory gate

<a id="capsid-review-phase-78"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L558) — capsid-review-phase-78

<a id="archived-section-386"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L559) — Round 78 Status (2026-10-20): 13 public Upgrade-fetch baseline scenarios passed

<a id="capsid-review-phase-77"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L566) — capsid-review-phase-77

<a id="archived-section-392"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L567) — Round 77 (2026-10-20): Upgrade-fetch foundation verified; public path pending wiring

<a id="capsid-review-phase-76"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L576) — capsid-review-phase-76

<a id="archived-section-398"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L577) — Current progress of the 76th round (2026-10-19): Collection of built-in methods, fetch request headers, and current full regression tests

<a id="capsid-review-phase-75"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L590) — capsid-review-phase-75

<a id="archived-section-404"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L591) — Round 75 (2026-10-19): WebCrypto digest/HMAC and full regression verification

<a id="capsid-review-phase-74"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L605) — capsid-review-phase-74

<a id="archived-section-410"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L606) — Current progress of round 74 (2026-10-19): Timeout abort bridge resumes request context

<a id="capsid-review-phase-73"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L615) — capsid-review-phase-73

<a id="archived-section-416"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L616) — Current progress of round 73 (2026-10-19): fetch caller context restoration

<a id="capsid-review-phase-72"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L625) — capsid-review-phase-72

<a id="archived-section-422"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L626) — Current progress of the 72nd round (2026-10-19): fetch resource recycling and cancellation semantics

<a id="capsid-review-phase-71"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L635) — capsid-review-phase-71

<a id="archived-section-428"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L636) — Current progress of the 71st round (2026-10-19): Cross-context Promise

<a id="capsid-review-phase-70"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L645) — capsid-review-phase-70

<a id="archived-section-434"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L646) — Current progress of the 70th round (2026-10-19): Asynchronous callbacks and execution budget

<a id="capsid-review-phase-69"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L655) — capsid-review-phase-69

<a id="archived-section-440"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L656) — Current progress of the 69th round (2026-10-19): Connecting timers with Promise lifecycle

<a id="capsid-review-phase-68"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L689) — capsid-review-phase-68

<a id="archived-section-446"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L690) — Current progress of round 68 (2026-10-19): Native exception reconstruction and built-in string conversion

<a id="capsid-review-phase-67"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L723) — capsid-review-phase-67

<a id="archived-section-452"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L724) — Round 67 (2026-10-19): Cloning of plain Error objects and engine type checks

<a id="capsid-review-phase-66"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L757) — capsid-review-phase-66

<a id="archived-section-458"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L758) — Current progress of the 66th round (2026-10-19): I/O for exception getters and error listeners

<a id="capsid-review-phase-65"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L790) — capsid-review-phase-65

<a id="archived-section-464"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L791) — Current progress of the 65th round (2026-10-19): Exceptional snapshot sequence, built-in functions, and strict regression tests

<a id="capsid-review-phase-64"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L829) — capsid-review-phase-64

<a id="archived-section-470"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L830) — Current progress of the 64th round (2026-10-19): Windows native references and complete matrix, closure, and error event process

<a id="capsid-review-phase-63"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L868) — capsid-review-phase-63

<a id="archived-section-476"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L869) — Current progress of the 63rd round (2026-10-18): Formal SDK corrections and C17/C18 original frame regression

<a id="capsid-review-phase-62"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L899) — capsid-review-phase-62

<a id="archived-section-482"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L900) — Round 62 (2026-10-18): Full validation of engine fixes and C17/C18 edge-case counterexamples

<a id="capsid-review-phase-61"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L931) — capsid-review-phase-61

<a id="archived-section-488"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L932) — Current progress of the 61st round (2026-10-18): Concurrent failure diagnosis and Hono cold start time

<a id="archived-section-491"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L960) — Current progress of the 60th round (2026-10-18): Outbound ACK pressure assumptions and actual Worker fixtures

<a id="archived-section-494"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1003) — Current conclusion of the 59th round (2026-10-18)

<a id="archived-section-497"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1011) — Qualification Round 57 (2026-10-17): Quarantine sequence, generational verification, and process capacity

<a id="archived-section-500"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1047) — Qualification Round 56 (2026-10-17): Observation of Managed Processes and Review of Replacement Callback Lifecycle

<a id="archived-section-503"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1064) — Qualification Round 55 (2026-10-17): Managed Lifecycle Response and Actual OS Exit Verification

<a id="archived-section-506"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1080) — Qualification Round 54 (2026-10-17): Precise mapping of workerd WPT profile and verification of evidence

<a id="archived-section-509"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1098) — Qualification Round 53 (2026-10-17): Managed Preheating Worker Handoff Failure Reclamation

<a id="archived-section-512"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1118) — Qualification Round 52 (2026-10-17): Host drain signal waiting and actual exit status

<a id="testing-layers"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1171) — testing-layers

<a id="validity-rules"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1182) — validity-rules

<a id="common-commands"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1199) — common-commands

<a id="required-workerd-websocket-comparison"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1239) — required-workerd-websocket-comparison

<a id="egress-and-tls-targeted-regression"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1368) — egress-and-tls-targeted-regression

<a id="tsan-m1c-gate"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1397) — tsan-m1c-gate

<a id="platform-contract-gates"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1424) — platform-contract-gates

<a id="environment-based-sandbox-evidence"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1436) — environment-based-sandbox-evidence

<a id="how-current-evidence-is-obtained"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1483) — how-current-evidence-is-obtained

<a id="archived-section-542"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1522) — 2026-10-16 Local GCC TSan Lifecycle Project

<a id="archived-section-545"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1538) — 2026-10-16 External qualification and Worker exit special update

<a id="archived-section-548"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1556) — 2026-10-16 Worker Exit Connection Recycling Special Project

<a id="archived-section-551"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1572) — 2026-10-16 adoption submission and closure project

<a id="archived-section-554"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1590) — Pending-handshake fetch canceled on 2026-10-16

<a id="archived-section-557"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1599) — 2026-10-16: Full-pool pending-admission fault injection

<a id="archived-section-560"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1609) — 2026-10-17 External handover drain window project: Review section 48

<a id="archived-section-563"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1625) — 2026-10-17 Local 503 Late Callback and Pool Close Special Project: Review Section 49

<a id="archived-section-566"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1641) — 2026-10-17: Realistic timeout verification and full-pool backpressure (Review Section 50)

<a id="archived-section-569"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1654) — Local verification update for round 58 (2026-10-17)

<a id="linux-production-and-sanitizer-sandbox-profiles"></a>
[Historical section](../history/archives/2026-11-17/testing.md.txt#L1668) — linux-production-and-sanitizer-sandbox-profiles

</details>
