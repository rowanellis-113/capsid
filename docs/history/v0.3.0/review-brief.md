# Capsid v0.3.0 engineering audit brief for the independent reviewer

<!-- capsid-historical-context-20261117 -->
> **Historical plan/review — not current execution instructions.** The original
> body, counts, failures and pending language below retain their captured source/date.
> Stable is v0.2.2 and experimental 0.3.0-beta is now published. Use
> [current status](../../reference/release-status.md) for delivered scope, exact qualification and
> measured-source data; do not restart old campaigns from this record.
<!-- /capsid-historical-context-20261117 -->

> 2026-10-14 Update: This briefing retains the historical conclusions based on the original review baseline. The actual progress of subsequent submissions of GLM,
> The fixes and unimplemented projects for this round are as follows: [GLM Implementation Review and Handoff Record](takeover-review.md);
> The execution status is based on the updated task list. The original sequence and the development of fixture 77 do not prove that version 0.3.0 has been completed.
> The latest progress of the handoff can be found in section 10 of this record: Configurable total quotas for services/listeners, shared budgets, and metrics
> Connected. The Linux development gate is set to 51/51, with 0 skip. The platform and formal release gates are still not fully qualified.

> Scope: Two engineering phases after the release of v0.2.2 - (A) Benchmarking system and perf-site
> B) Implementation of TDD for v0.3.0 (FetchRPC v4 / WebSocket).
> Baseline commit `aef6020` (tag v0.2.2) → Current `51b65fe` (main, all tests passed in CI).
> This document is for independent review: each section lists facts, evidence locations, known risks, and unresolved issues.

## 0. Review Entrance

- Repository: `ErosZy/capsid`, main line; CI "Testing validity" and
  "Performance site" has two workflows, and the last one (`51b65fe`) has all passes.
- Key Document Index:
  - Measurement specifications (normative): `bench/SPEC.md`
  - v0.3.0 Plan: `docs/v0.3.0-fetchrpc-v4-plan.md`
  - FetchRPC v4 Contract (Frozen + Amendments): `docs/fetchrpc-v4.md`
  - workerd WebSocket Semantics Reference: `docs/workerd-websocket-semantics.md`
  - Performance Benchmarks: `docs/performance-benchmarks.md`, `README.md` §Performance
  - Online site: https://eroszy.github.io/capsid/ (GitHub Pages, Actions deployment)
- Submission sequence (`aef6020..51b65fe`, 29 items in total): 21 items related to performance system +
  v0.3.0 Plan and Implementation 8 items. Please refer to `git log --oneline aef6020..HEAD` for details.

## 1. Phase A: Baseline measurement system (completed and deployed)

### 1.1 Three lanes (`bench/`, see SPEC.md for protocol)

| Lane | Script | Key Agreement Points |
| --- | --- | --- |
| Service throughput | `qps-lane.sh` | Freeze loadgen, 64 connections/64 inflight, 5s warm-up + 5×15s, byte-by-byte precise verification, single serving process/stack |
| Cold Start | `cold-start.sh`+`cold-start.cc` | **spawn→First Verified Response**; total_ms previously included 50 curl warm-up cycles (old data ~400ms platform is measuring artifacts, fixed) |
| Density | `density-lane.sh` | Fill to 2048MB budget, doubling + binary + jitter-step retreat, cold (1 req/worker)/hot (100 req/worker) dual phase, fatal failure on touch, dual read stable gate |

### 1.2 Current Data Points (v0.2.2, All Production Postures: Managed Coordinator + Linux Strong Isolation)

| Metrics | capsid | workerd | FastAPI | Flask | PHP Slim |
| --- | --- | --- | --- | --- | --- |
| JSON-1k QPS | 9,490 | 26,624 | 12,948 | 3,568 | 2,095 |
| JSON-16k QPS | 7,109* | 20,189 | 12,599 | 2,878 | 2,008 |
| stream QPS | 6,523 | 14,192 | 6,671 | 3,987 | 2,032 |
| Service Memory in MB | 16.4 | 581.0 | 46.1 | 17.0 | 28.1 |
| Cold Start 10k/1m Bytecode ms | 17.7 / 31.1 | 20 / 38 | — | — | — |
| Density: 2GB, Hot: 10k/1m | 882 / 132 workers | 638 / 214 cells | — | — | — |

\* Reason for the 16k fluctuations: The old lane was stuck with `--initial-stream-window 1024`. For managed scenarios, the
default settings from production are used. Details are recorded in `five-qps-v022-managed-20261011/manifest.txt`.

**Review Points**:
- The capsid measures differently from other stacks for **different sessions** (retest with capsid=strict, other stacks follow the same method).
  `five-qps-v022-20261010`)——The document has declared two evidence directories, but strictly
  The "same-session" standard is not met, see §1.4 R1.
- The isolation cost has been quantified: zero loss in throughput, cold start time of +0.3~5.4ms, and -1.6% at 10k density.
- The workerd memory description has been refined to "No decline observed within the sampling window" + equivalent configurations instead of equivalent functions.
  Limitation (workerd with multiple windows services ~2.8× requests).

### 1.3 Evidence Chain Rules (Firmly integrated into SPEC + collector requirements)

- Each item includes: git commit/tag, machine fingerprint, binary/fixture/original sample SHA-256
- Collector fail-closed: Duplicate (stack, size) cells rejected; deno/node/workerd
  The result exists, but the fingerprint lacks identity, resulting in a rejection. (Previously encountered coexistence of "deno: absent" and deno data)
- Evidence archiving `bench/results/<lane>-v<ver>-<date>/` +.gitignore whitelist
- The density manifest explicitly records `CAPSID_STRICT=1` and dual phase

### 1.4 Known Risks/Unresolved Issues (Phase A)

| # | Problems | Status |
| --- | --- | --- |
| R1 | The capsid strictly sandbox retesting is not the same as other sessions. | Declared; next release should undergo a full-stack retest per session |
| R2 | The warm-up distribution of shared acceptors for capsid is a connection hash approximation (doubled request count + annotations). | The agreement states that there is no verification of per-worker counts. |
| R3 | The 1m density is limited by the QuickJS aggressive compilation (approximately 1.4KB per function metadata, with actual measurements showing 1MB for annotation padding = 6.0MB vs. actual code = 17.2MB) | Document has been researched and documented; lazy compilation is listed as a roadmap-level direction, not part of this version. |
| R4 | celld comparison pause (QPS/cold start with data: json-1k 48.9k, cold ~9ms; density not completed due to process tree hierarchy/ports/ready timing measurement issues) | Data retained in /tmp; lane code has been rolled back, not released |

## 2. Phase B: v0.3.0 FetchRPC v4 / WebSocket (in progress)

### 2.1 Process (Strict TDD, testing precedes implementation freezing)

Completed workflow (compared to `docs/v0.3.0-fetchrpc-v4-plan.md`):

| Workflow | Deliverables | Testing |
| --- | --- | --- |
| WS-A | Workerd semantic archive (listing files/lines 6dd2348) + 6 fixtures with absolute assertions | The runner uses a profile manifest to control the gate. Exiting 77 is skipped before WebSocketPair goes online; if the harness is not taken over after going online, a loud failure will be reported. |
| WS-B | Contract + Golden Vectors (`vectors.json`) | Boundary suite: The current v3 parser rejects every v4 vector, and the error string is precise ("never mix silently" is deadlocked before implementation) |
| C1 | protocol v4: Frames 19-26, versioning flags, Hello capability verification | v4: All vectors for round-trip/negotiation/bad capabilities are green; v3: No changes at the byte level |
| C3a | RFC 6455 Encoding/Decoding (Self-contained SHA-1/base64) | RFC vector + bidirectional roles + length morphology + six types of violations |
| C3b | Host Handshake Gate (before admission): Bad key 400 / Version 426+13 / Valid handshake release | Real Host Binary HTTP Driver |
| C-Negotiation | End-to-end v4: ABI addition tail field → client v4 Hello → worker negotiate lock → READY feedback capability → v4 stream carrying v3 semantics | `test_worker_v4_negotiation.cc`: Negotiation + Transport + Local Rejection + v3 Default |

### 2.2 Contract Amendment (Review Notes: These are the "correction" implementations of the draft)

1. Hello v4 = **Complete v3 startup configuration + 8-byte capability suffix** (Draft fixed 87 bytes)
   Ignored configuration payloads) — `fetchrpc-v4.md` §1.2 has been corrected.
2. The worker's capabilities are reported via the **READY suffix** (no response channel in the original draft).
3. The upgrade of RequestHead is **pre-allocated by the host for the channel_id** (the authority of the ID is reserved by the host).
4. worker **Bootstrap period** Accept the first frame 3/4 And lock it (steady-state matrix remains unchanged).
5. The audit found that draft frame numbers 15–18 conflicted with `kAudit`, `kMemoryMetrics*` and `kLoadBinding`.
   The revised assignment uses 19–26.

### 2.3 ABI Compatibility Design (Review Notes)

- Append `protocol_version`, `protocol_reserved` and `protocol_capabilities` to `capsid_worker_config`
  (8 bytes total). Guard reads of these fields with `struct_size`.
- Keep the ABI snapshot in `tests/compat/runtime_v7.h` in sync with the struct. CI caught a missing update in this round.
  Reviewers can compare the snapshot with `runtime.h` field by field.
- `capsid_worker_config_init` calls `memset` using the library's `sizeof(*config)`.
  A caller compiled with an older header can therefore supply a buffer smaller than the new library expects.
  This matches the repository's source-compatibility convention. Reviewers should confirm this ABI boundary.

### 2.4 Known Risks/Unresolved Issues (Phase B)

| # | Problems | Status |
| --- | --- | --- |
| B1 | Failure of assertions in the multi-segment python replacement script will **quietly roll back** the previous segments (patches were lost due to this, discovered through runtime diagnostics). | Lessons learned; recommended to have the reviewer verify the authenticity of all "claimed applied" patches (can be verified with `git log -p`) |
| B2 | The worker stderr is swallowed by `--close-stdio`, and debugging can only rely on the /tmp file probes. | All temporary measures have been removed (`grep -r capsid-dbg` should return empty); long-term consideration for debugging log channels |
| B3 | The v4 negotiation test's v3-READY branch only includes weak assertions (not ruling out any payloads where the last 8 bytes are exactly (0,0)). | The test documents contain comments for clarification; these can be replaced with strong assertions after the implementation is completed. |
| B4 | The density lane once had issues with orphan processes/port reuse/semi-orphan lane collisions (celld segment exposure) | Fixed the capsid/workerd path (recursive tree traversal, inner linking of parent shells, killing all descendants); rolled back the celld |
| B5 | Channel registry/duplex pump/WebSocketPair/101 bridge not implemented (negotiation is in the "transport" phase, worker capability bit=0) | Next steps in the plan; fixtures are still SKIP |

### 2.5 Suggestions for Review Path (In Dependency Order)

1. `bench/SPEC.md` ↔ Three-lane script ↔ Consistency with the lane directory README
2. `docs/fetchrpc-v4.md` (with amendment markers) ↔ `src/protocol.{h,cc}` ↔
   The `tests/test_fetchrpc_v4.cc` + `vectors.json` are consistent with third-party requirements.
3. End-of-file field in `include/capsid/runtime.h` ↔ `tests/compat/runtime_v7.h`
   The snapshot is consistent across fields; the struct_size guard in `src/client.cc` reads the logic.
4. `src/worker_runtime.cc` All go to the emission point (queue_output, two)
   Response fast path, outbound_buffer) version chapter completeness
5. Evidence chain sampling: Select a number from the README table and proceed along
   `docs/performance-benchmarks.md` → Evidence Catalog manifest → cells.txt
   → sha256sums Traceability Verification
6. CI: The most recent full green status of both workflows; `workerd_ws_fixtures` should be
   Skipped(77), `fetchrpc_v4_boundary`/`ws_codec`/`worker_v4_negotiation`
   /`host_ws_upgrade_gates` has been passed

## 3. Environmental Considerations (Reviewed by the reviewer)

- The Release build has a clean tree gate (`ComputeBuildIdentity.cmake`); the working tree has
  When there is CRLF noise, use `git stash push -u` → build → pop. Local iteration recommended.
  `build-dev` (RelWithDebInfo + `CAPSID_BUILD_HOST=ON`), initial configuration required
  `npm ci --ignore-scripts --prefix examples/{hono,itty-router,h3-v2,elysia}-reference`.
- Pushing credentials on the Windows side (PowerShell `git push`), insufficient PAT permissions are available in WSL.
  (No permissions for ‘pages/workflow-dispatch’; ‘rerun’ is also unavailable. Failed jobs can only be handled using…)
  New submission re-triggered).
- Density/QPS requires a quiet environment for lane operation; the lane will exclusively occupy the 19400+/1899x port segment
  Failure leaves an orphan—SPEC and scripts have been cleaned up, but it is recommended before reproduction.
  `pgrep -af "celld|capsid-host|workerd"` confirms that it is empty.
