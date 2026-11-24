# Capsid v0.3.2 independent review brief for blocker fixes and release qualification

> This document records the inputs and scope of this review round. Subsequent fixes, retests, and remaining release checks are tracked in [v0.3.2 Qualification Records](README.md).
> **Baseline and Scope Definition**: The delivered stable release is `v0.2.2`, and the published experimental prerelease is `v0.3.1-beta.1` (release identity and benchmark provenance are in [Current Status](../../reference/release-status.md)). This brief applies to development branch `dev/v0.3.2` (`HEAD` at `85b9e1e`, preceding commits `a7fd47e..85b9e1e`, plus the uncommitted working-tree fixes on top of `85b9e1e`). The focus is verifying the fixes and formal regressions for the **4 P1 blockers** and **P2 #1–#5, #7 / P3 #1, #3** from the [Independent Follow-up Review](runtime-followup.md).

---

## 0. Review goals, entry points and release constraints

Conduct an independent adversarial code review to determine whether the fixes close every blocker from the previous review and introduce any concurrency, accounting, lifetime, memory-safety or Web conformance regressions. For each finding, give the exact source path, line range, and a reproducible thread interleaving or input.

Reference documents:
1. [AGENTS.md](../../../AGENTS.md) (Project development principles)
2. [v0.3.2 Initial technical review](runtime-review.md) (First-round 13 findings)
3. [v0.3.2 Independent follow-up review](runtime-followup.md) (4 P1, 7 P2, and 3 P3 findings plus the independent probe baseline)
4. [Config v2 and Elastic Pool Design](../../design/config-v2.md)

Run the following commands from the repository root to inspect working-tree scope and verify the frozen-path red lines:

```sh
git status -s
git log --oneline a7fd47e..HEAD
git diff --stat

# Changes to the public ABI, wire protocol, frozen baseline or pinned WPT manifest are P0 blockers.
git diff -- \
  include/capsid/runtime.h \
  include/capsid/runtime.hpp \
  src/protocol.h \
  src/memory_metrics.cc \
  tests/fixtures/websocket-interop-lock.json \
  tests/wpt/manifest.json \
  tests/wpt/workerd-default-profile-v2.json \
  docs/data/current-status.json \
  docs/data \
  README.md
```

---

## 1. Fixes corresponding to `runtime-followup.md`

### 1.1 Four P1 blockers with registered CTest regressions

| Review Item | Root cause and fix mechanism | Production code | Formal regression and qualification points |
| --- | --- | --- | --- |
| **P1 #1**: After a scale-out failure, `minReady=1` can remain at `0` READY workers with no recovery scheduled. | 1) The `run_replacement` failure path sets `observation.target_ready_workers = min_ready_` (preserving backoff and crash-quarantine budgets) and prioritizes recovering guaranteed floor slots marked `needs_replacement`; <br> 2) `abandon()` (startup permit denied) reschedules recovery with backoff whenever an active pool has `ready + scheduled < min_ready_`; <br> 3) `install_replacement()` fills a `min_ready_` deficit first and, once the floor is met, retires extra `needs_replacement` slots and calls `settle_evicted_slot_elastic_locked` to return excess elastic slots; <br> 4) `record_instability()` explicitly clears `scale_out_spawn` and `elastic_held` on scheduled replacement slots. | [generation_pool.cc](../../../src/host/generation_pool.cc#L877-L885), [generation_pool.cc](../../../src/host/generation_pool.cc#L1432-L1498), [generation_pool.cc](../../../src/host/generation_pool.cc#L1605-L1685) | `host_elastic_pool_floor_recovery_after_scale_out_failure` ([test_host_elastic_pool.cc](../../../tests/test_host_elastic_pool.cc)): with no new requests sent, observes automatic recovery trigger a 3rd startup attempt and restore `ready=1`. |
| **P1 #2**: After health-driven recycling removes a guaranteed worker, the surviving worker continues occupying an elastic slot. | `GenerationPool::recycle_worker` now performs elastic settlement via `settle_evicted_slot_elastic_locked(s)` under the pool lock—transferring the guaranteed-floor identity to a surviving slot—and returns the freed elastic quota outside the lock. | [generation_pool.cc](../../../src/host/generation_pool.cc#L485-L545), [generation_pool.cc](../../../src/host/generation_pool.cc#L1346-L1393) | `host_elastic_pool_health_recycle_returns_elastic_slot` ([test_host_elastic_pool.cc](../../../tests/test_host_elastic_pool.cc#L1384-L1416)): asserts `ready=1`, `elastic=0`, and that a peer application can actually acquire the freed slot. |
| **P1 #3**: While handing `DispatchLease` over to `kBeginRequest`, `is_idle()` can transiently observe the worker as idle. | Introduced continuous atomic occupancy tracking `request_claims_` (covering `pending_dispatches_ + inflight_`) and `DispatchLease::submit_begin` -> `WorkerExecutor::submit_begin_from_dispatch`. Under `WorkerExecutor::mutex_`, `inflight_` increments while `pending_dispatches_` decrements so `request_claims_` stays continuously `>= 1` across the handoff (never dropping to 0 or double-counting). `is_idle()` reads `request_claims_` and `connection_owners_`. On abnormal exit, clearing `inflight_ids_` deducts the exact `cleared_inflight` count from `request_claims_`. | [worker_executor.h](../../../src/host/worker_executor.h#L278-L315), [worker_executor.cc](../../../src/host/worker_executor.cc#L240-L321), [worker_executor.cc](../../../src/host/worker_executor.cc#L602-L621), [generation_pool.h](../../../src/host/generation_pool.h#L274-L283), [managed_listener.cc](../../../src/host/managed_listener.cc#L1634-L1637) | `host_elastic_pool_dispatch_handoff_blocks_eviction` ([test_host_elastic_pool.cc](../../../tests/test_host_elastic_pool.cc#L1487-L1585)): thread barriers hold the lease handoff inside the `is_idle()` window, asserting eviction is rejected and the real HTTP request receives a complete `200 "elastic-ok"` response. |
| **P1 #4**: HTTP dispatch picked the lowest combined-load worker first and only checked that single worker's HTTP cap. | Added `enforce_max_inflight` and `saw_ready_worker_out` to `select_best_worker_locked`: it filters READY workers with `active_http_requests() < max_inflight_per_worker` before ranking by load. `acquire_worker` reports `at_max_inflight = true` only when READY workers exist and all of them have reached the HTTP cap. | [generation_pool.h](../../../src/host/generation_pool.h#L459-L462), [generation_pool.cc](../../../src/host/generation_pool.cc#L387-L468) | `host_elastic_pool_http_selection_skips_full_worker` ([test_host_elastic_pool.cc](../../../tests/test_host_elastic_pool.cc#L1418-L1485)): gives the second worker higher `client_bytes` and asserts the second HTTP request is dispatched to it and returns a complete `200 "elastic-ok"`. |

### 1.2 P2 and P3 fixes

| Review Item | Production/test location and fix mechanism | Corresponding regression test |
| --- | --- | --- |
| **P2 #1**: Concurrent scale-down replacements can over-commit `surgeWorkers`. | [worker_capacity_ledger.h](../../../src/host/worker_capacity_ledger.h#L101-L111), [worker_capacity_ledger.cc](../../../src/host/worker_capacity_ledger.cc#L19-L160): added `replace_surge_growth` and `pending_surge_growth_` to reserve the surge delta on scale-down `reserve_replace` and settle it on `commit_replace` or `abort_reserve`. | `host_worker_capacity_ledger_ledger_concurrent_shrink_surge` ([test_worker_capacity_ledger.cc](../../../tests/test_worker_capacity_ledger.cc#L99-L120)) |
| **P2 #2**: Alive `retired_` workers dropped out of soft-watermark memory accounting. | [eviction_coordinator.h](../../../src/host/eviction_coordinator.h#L98-L116), [eviction_coordinator.cc](../../../src/host/eviction_coordinator.cc#L356-L361), [eviction_coordinator.cc](../../../src/host/eviction_coordinator.cc#L566-L665), [generation_pool.h](../../../src/host/generation_pool.h#L344-L347), [generation_pool.cc](../../../src/host/generation_pool.cc#L744-L760): removed permanent `async_reaping_retired` skipping; tracked same-cycle/cached evictions in `recently_evicted_pids_` cleared on fresh sampling cycles (`run_eviction_cycle(true)`), while keeping `evictable=false` so retiring workers are counted in memory without being selected twice. | `host_eviction_active_retiring_worker_counted_in_soft_watermark` ([test_host_eviction_coordinator.cc](../../../tests/test_host_eviction_coordinator.cc#L622-L710)) and `host_eviction_phase1_overshare_protects_small_tenant` |
| **P2 #3**: Wakeup descriptors accumulated across cold-start/stop cycles when `on_worker_started` was enabled. | [worker_event_source.h](../../../src/host/worker_event_source.h#L34-L36), [worker_event_source.cc](../../../src/host/worker_event_source.cc#L52-L68), [worker_executor.cc](../../../src/host/worker_executor.cc#L602-L621): `worker_thread_main` calls `source_.close_wake_pipe()` under `mutex_` immediately when setting `exited_ = true` (mutually exclusive with `submit()` / `try_submit_channel()`), releasing the OS pipe fds immediately even if external observers still hold references. | `host_elastic_pool_cold_cycles_release_descriptors` ([test_host_elastic_pool.cc](../../../tests/test_host_elastic_pool.cc#L978-L1022)): covers both default mode and enabled `on_worker_started`. |
| **P2 #4**: `QuotaExceededError` could not be structured-cloned. | [dom-exception.js](../../../js/dom-exception.js#L15-L284), [bootstrap.js](../../../js/bootstrap.js#L5-L10): added `installStructuredCloneDomExceptionSupport()` covering top-level values, nested objects/arrays, `Map`, `Set`, `Object.create(null)`, cyclic graphs, and shared/aliased references for `QuotaExceededError` and `DOMException` (preserving `constructor`, `name`, `message`, `code`, `quota`, `requested`, including `null` and `-0`, and standard `transfer` validation). | [p1-platform-contract.js](../../../tests/fixtures/p1-platform-contract.js#L550-L625) (`worker_p1_platform_contract`) + [web_contract.mjs](probes/web_contract.mjs) |
| **P2 #5**: `-0` lost in Web IDL double conversion; `getRandomValues` threw `TypeMismatchError` on non-views and read user-shadowed `byteLength`. | [dom-exception.js](../../../js/dom-exception.js#L15-L21): `toWebIdlDouble` preserves `-0`; [bootstrap.js](../../../js/bootstrap.js#L47-L108): `Crypto.prototype.getRandomValues` validates `ArrayBuffer.isView` first (`TypeError`), checks integer TypedArray tag via intrinsic `%TypedArray%.prototype[@@toStringTag]` (`TypeMismatchError`), and reads intrinsic `byteLength`/`buffer`/`byteOffset` getters before invoking `core.random`. | [p1-platform-contract.js](../../../tests/fixtures/p1-platform-contract.js#L584-L625) + locked WPT `WebCryptoAPI/getRandomValues.any.js` (39/39) |
| **P2 #7 & P3 #1/#3**: Generation digest contract in tests, deterministic drain synchronization, and category count verification in docs audit. | [test_host_eviction_coordinator.cc](../../../tests/test_host_eviction_coordinator.cc#L364), [test_host_eviction_coordinator.cc](../../../tests/test_host_eviction_coordinator.cc#L1702): prefixed generation digests with `sha256:`; [generation_pool.h](../../../src/host/generation_pool.h#L363-L366), [generation_pool.cc](../../../src/host/generation_pool.cc#L1065), [test_host_elastic_pool.cc](../../../tests/test_host_elastic_pool.cc#L918): replaced 50 ms sleep with deterministic `drain_check_cycles() >= 2` wait; [audit-current-docs.mjs](../../../tests/audit-current-docs.mjs#L485-L489): enforced strict match between `coverage.counts` and per-`kind` row counts in `coverage.documents`. | `host_elastic_pool_drain_during_slot_admission`, `node tests/audit-current-docs.mjs` |

### 1.3 Work deferred to separate changes

- **P2 #6 (Retiring `capsid/host-v1` / `capsid/app-v1` production dispatch and migrating 16 existing v1 tests plus `tests/soak/run_managed_soak.py` to v2):** Deferred to a separate cleanup change so existing test inputs and assertions remain untouched during this blocker-fix round.
- **P3 #2 (Extending Windows directory-race test to two distinct directory identities):** Retained the existing 14/15 scenarios for this round.
- **Clean working-tree gates:** `worker_build_identity_matrix` requires an empty `git status --porcelain`; `tools/format.py --check` requires the pinned formatter environment (`.venv-format` with `clang-format`).

---

## 2. Five areas for independent review

In addition to verifying the fixes and new regressions above, probe these five boundaries for remaining edge cases:

1. **Guaranteed floor, elastic capacity ledger, and automatic recovery (P1 #1 + P1 #2):**
   - Inspect [generation_pool.cc](../../../src/host/generation_pool.cc#L1432-L1685): when `minReady >= 1` and consecutive crashes or startup-permit rejections hit `policy_.max_events`, do the `run_replacement` failure path, `abandon()` path, and `install_replacement()` path strictly respect `quarantine_waiting_` and `state_ != kActive` without leaking threads or deadlocking?
   - When `slot 0` (guaranteed floor) is reclaimed while `slot 1` (elastic, `elastic_held = true`) is starting up (`needs_replacement = true`), and `slot 1` **succeeds**, `install_replacement()` ([generation_pool.cc](../../../src/host/generation_pool.cc#L1647-L1685)) retires `slot 0` and calls `settle_evicted_slot_elastic_locked(other)`. Can `on_worker_exited`, `retired_`, or `release_cb()` double-release or leak slots?
2. **Continuous occupancy count `request_claims_` and immediate descriptor close (P1 #3 + P2 #3):**
   - Inspect [worker_executor.cc](../../../src/host/worker_executor.cc#L240-L321) and [worker_executor.cc](../../../src/host/worker_executor.cc#L602-L621): across `acquire_dispatch` -> `submit_begin_from_dispatch` -> `mark_canceled` / `retire_terminal_request` / abnormal-exit `cleared_inflight` / direct `submit(kBeginRequest)`, does `request_claims_` always equal `inflight_ + pending_dispatches_` without underflow?
   - Check the WebSocket 101 upgrade handoff in `CAPSID_EVENT_RESPONSE_HEAD` ([worker_executor.cc](../../../src/host/worker_executor.cc#L1131-L1158)): is there any brief idle gap between `request_claims_` and `connection_owners_` around `mark_canceled` and `refresh_channels()`?
   - Check `WorkerEventSource::close_wake_pipe()` ([worker_event_source.cc](../../../src/host/worker_event_source.cc#L54-L64)) for mutual exclusion and exit ordering against `submit()`, `try_submit_channel()`, and `worker_thread_main()` to rule out `EBADF` or fd reuse races.
3. **HTTP worker filtering and queuing/scale-out consistency (P1 #4):**
   - Inspect [generation_pool.cc](../../../src/host/generation_pool.cc#L387-L468) and [managed_listener.cc](../../../src/host/managed_listener.cc#L736-L772): when `max_inflight_per_worker == 0`, `round_robin_tie_break == true`, no READY workers exist (`saw_ready == false`), or all READY workers are saturated (`saw_ready == true`), are `acquire_worker`, `at_max_inflight`, and listener cold-start/queue/scale-out branches completely aligned?
4. **Scale-down surge commitment and `recently_evicted_pids_` lifecycle (P2 #1 + P2 #2):**
   - Inspect [worker_capacity_ledger.cc](../../../src/host/worker_capacity_ledger.cc#L19-L160): under overlapping scale-up and scale-down replacements, draining old generations after `commit_replace`, and `abort_reserve`, is `pending_surge_growth_ + surge_used_` strictly conserved against `available_steady_or_elastic_locked()`?
   - Inspect [eviction_coordinator.cc](../../../src/host/eviction_coordinator.cc#L566-L665): `run_eviction_cycle(true)` clears `recently_evicted_pids_`, while `run_eviction_cycle(false)` and `acquire_worker_slot()` retain it (without skipping hard-budget checks `total_bytes >= limit_bytes`). Can stale skipping or premature eviction still happen under rapid scaling?
5. **Web IDL `QuotaExceededError` structured cloning and `crypto.getRandomValues` boundaries (P2 #4 + P2 #5):**
   - Inspect [dom-exception.js](../../../js/dom-exception.js#L127-L284):
     - Top-level `QuotaExceededError` / `DOMException` validation and `ArrayBuffer` detachment when `options.transfer` contains valid or invalid buffers;
     - Clone fidelity for plain objects, `Object.create(null)`, sparse arrays, `Map` keys/values, `Set`, cyclic references, and shared/aliased references;
     - Rejection of forged-prototype objects (`Object.create(QuotaExceededError.prototype)`) via private brand checks;
     - Correct `DataCloneError` when mixed with unclonable values (e.g., `{ e: new QuotaExceededError('x'), fn: () => {} }`);
     - Zero impact of the `structuredClone` wrapper on `profileGlobalNames` or locked WPT cases.
   - Inspect [bootstrap.js](../../../js/bootstrap.js#L47-L108): verify strict Web IDL + Web Crypto error types and behavior for `undefined`, `null`, `{}`, `ArrayBuffer`, `DataView`, `Float32Array`, `Float64Array`, shadowed `byteLength`/`buffer`/`byteOffset` on `Uint8Array`, 0-byte views, detached `ArrayBuffer` views, and integer views `> 65536` bytes.

---

## 3. Recorded results and commands to reproduce them

All fixes in this round were rebuilt and verified on WSL2 (`RelWithDebInfo`, Clang 18 `ASan+LSan+UBSan`, Clang 18 `TSan`) and Windows (`MinSizeRel`):

1. **Independent probes (all exit `0`):**
   - Linux C++ probes (`run_pool_probes.py`): **`4/4` passed** (`health: ready=1 elastic=0 peer=1`, `floor: ready=1 calls=3 scale_in_flight=0`, `selection: ready=2 second_lease=1 at_max=0`, `surge: second_reserved=0 surge=2 configured_surge=3`).
   - Windows real-worker Web contract probe (`web_contract.mjs`): **`7/7` assertions passed (exit `0`)**.
2. **Dual-platform builds and CTest registration:**
   - Verified all 6 new CTest regressions are registered on Linux and Windows via `ctest -N` ([build_tests.cmake](../../../cmake/build_tests.cmake#L1123-L1127), [build_tests.cmake](../../../cmake/build_tests.cmake#L6998-L7062)):
     - `host_elastic_pool_floor_recovery_after_scale_out_failure`
     - `host_elastic_pool_health_recycle_returns_elastic_slot`
     - `host_elastic_pool_http_selection_skips_full_worker`
     - `host_elastic_pool_dispatch_handoff_blocks_eviction`
     - `host_worker_capacity_ledger_ledger_concurrent_shrink_surge`
     - `host_eviction_active_retiring_worker_counted_in_soft_watermark`
   - WSL2 `RelWithDebInfo`: `host_eviction_*` **`11/11` passed**; all affected Host/pool/ledger/observer/soak/platform-contract suites (including `worker_p1_platform_contract`) **passed**.
   - Windows `MinSizeRel`: `host_eviction_*` **`10/10` passed**; all affected Host/pool/ledger/observer/soak/platform-contract suites **passed**.
3. **Clang 18 sanitizer suites:**
   - **ASan + LSan + UBSan**: **`64/64` passed** (0 failures, 0 leaks, 0 memory/UB errors).
   - **TSan** (`-j 1`): **`63/63` passed** (0 data races, 0 thread leaks).
4. **Locked WPT (`1985b47aa8972a970f005957f2bfa036da1787c6`) and documentation audit:**
   - `worker_wpt_WebCryptoAPI_getRandomValues_any_js`: **Passed (`39/39` subtests, `failures: []`)**.
   - Full locked WPT `^(wpt_source_integrity|worker_wpt_.*)$`: **`80/85` passed, 0 skipped, 5 failed** (matching `WORKERD_STANDARD_FAILURES` exactly).
   - `node tests/audit-current-docs.mjs` (including strict `coverage.counts` validation): **Passed (`62 documents, 31 archived snapshots hash-checked, 12 modules verified`)**.

Reproduction commands (set `$BUILD_DIR` to the Linux Ninja build directory containing `compile_commands.json` and `$WIN_BUILD` to the Windows build directory):

```sh
# 1. Run the Linux C++ probes (expect exit 0).
python3 docs/history/v0.3.2/probes/run_pool_probes.py "$BUILD_DIR"

# 2. Run the selected regression suite on Linux (similarly for ASan / TSan build dirs).
ctest --test-dir "$BUILD_DIR" --output-on-failure -j 4 \
  -R '^(host_elastic_pool_.*|host_worker_capacity_ledger_.*|host_eviction_.*|host_managed_listener.*|host_generation_pool.*|host_process_observation.*|host_generation_adoption.*|host_worker_supervisor.*|worker_p1_platform_contract)$' \
  -E '^host_eviction_real_cgroup_memory_current$'

# 3. Run the pinned WPT target, full locked WPT suite, and documentation audit.
ctest --test-dir "$BUILD_DIR" -V -R '^worker_wpt_WebCryptoAPI_getRandomValues_any_js$'
ctest --test-dir "$BUILD_DIR" --output-on-failure -j 2 -R '^(wpt_source_integrity|worker_wpt_.*)$'
node tests/audit-current-docs.mjs .
```

On Windows PowerShell:

```powershell
& "$WIN_BUILD\test-worker-integration.exe" "$WIN_BUILD\capsid-worker.exe" .\docs\history\v0.3.2\probes\web_contract.mjs platform
```

---

## 4. Required review output

Provide a re-review report containing:

1. **Overall verdict:** Lift the block (`Pass` / `Conditional pass`) or maintain `Block`, with concrete rationale.
2. **Verification table for the 4 P1 and completed P2/P3 items:** Actual verification result, verified source line ranges, and established invariants for each item.
3. **New findings list** (classified as `P0 blocker` / `P1 serious` / `P2 moderate` / `P3 suggestion`; state "none" for empty levels): exact `path:line`, trigger sequence or counterexample input, real impact, and minimal fix consistent with [AGENTS.md](../../../AGENTS.md).
4. **Per-area verification conclusions and residual blind-spot assessment across the five deep-dive review areas above.**
