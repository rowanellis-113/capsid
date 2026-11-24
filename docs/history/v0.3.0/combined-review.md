# Capsid v0.3.0 combined implementation, workerd compatibility and test-suite review

<!-- capsid-historical-context-20261117 -->
> **Historical plan/review — not current execution instructions.** The original
> body, counts, failures and pending language below retain their captured source/date.
> Stable is v0.2.2 and experimental 0.3.0-beta is now published. Use
> [current status](../../reference/release-status.md) for delivered scope, exact qualification and
> measured-source data; do not restart old campaigns from this record.
<!-- /capsid-historical-context-20261117 -->

On 2026-10-14 we reviewed GLM's changes to generation cleanup, replacement tests and archived evidence,
then combined our findings with the [test integrity review](test-coverage-review.md).
The review covered HEAD `8028b88` plus uncommitted changes. It made no changes to the runtime.

Resource growth during successful generation replacement fell substantially. P1 remained open because the new callback kept
the pool alive after a failed deployment. After three consecutive failures, the Host had 4 Workers and 14 threads, up from 1 Worker and 8 threads.
The new replacement test also failed once in a suite run. All 17 observed differences from workerd remained. The A–D compatibility
regressions and strict gate were still missing. The 64 passing checks did not cover those requirements.

The [third- and fourth-round review](fix-review.md) could no longer reproduce the accumulation after failed deployments.
It verified fixes for 16 of the original 17 inputs, and identified further edge cases in numeric conversion,
Response types and exceptions thrown after close. This document records the second-round observations.
The follow-up review records the later results.

## 1. Up-to-date content and verifiable improvements

| Update | This round of confirmation results | Current Status |
| --- | --- | --- |
| The drain callback transfers the pool to `finished_pools`. | Callback avoids direct destruction on the pump thread; it is cleaned up during the next activation/retire or Host exit. | Normal path improvement, failed transactions and the final recycling boundary are still not completed. |
| glibc `mallopt(M_ARENA_MAX, 8)` | The current Host has a limit on the number of arenas; the increment in address space for independent sampling has significantly decreased. | Confirmed improvements; arena limitations cannot replace object destruction assertions |
| `websocket_managed_replacement-recursion` | Registered. 16 rounds of RSS/VmSize assertions for a bundle of approximately 512 KiB have been written. | The entire group failed once, but three separate runs were successful. It's still not stable. |
| Configuration and report archiving | `configure-formal.log`, QuickJS 2 items, overlay 4 items, context/identity 2 items - JUnit exists and passed; the number of build targets has been corrected to 39 | Some issues in Part P2 have been resolved; new regression tests and the original evidence from Red still need to be completed. |
| reference registration | The current directory has recovered `websocket_outbound_reference`, with new replacement items added, totaling 411 items. | This directory has been restored; the gaps caused by conditional registration still exist in the gate. |
| The fast/full sets of GLM | JUnit is actually 13/13 and 64/64, with zero skipping; The Managed report is bound to the current Host hash. | Both sets do not contain replacement cases. |

The current 411 items in CTest include unit tests, integration tests, scripts, etc. None of the 411 items were run entirely this round.
Previously, the 409 project log was a historical checkpoint. The latest additions are reference and replacement regression tests.

Comparison of the same independent sampling script, 16 rounds of replacement, with each round having approximately 512 KiB of bundles:

| Observation | Before Correction | Current product |
| --- | ---: | ---: |
| First Host RSS, KiB | 11,740 | 11,892 |
| Round 16 Host RSS, KiB | 23,764 | 16,660 |
| RSS incremental, KiB | 12,024 | 4,768 |
| VmSize is incremental, in KiB | 270,404 | 16,392 |
| Current round 16 / All evictions completed | — | 8 / 6 |
| After complete retirement, Worker | 0 | 0 |

Samples were taken at 60 ms intervals in each round. They show an improvement during normal replacement.
They do not cover failed transactions or establish that the last pool and factory were destroyed. Only a later control
operation or shutdown cleared `finished_pools`. A direct assertion was still needed after deployments stopped.

## 2 P1: The new pool created after write failure has a cumulative capacity equal to that of the actual Worker

Relevant Code:
[pool_box in main.cc](../../../src/host/main.cc#L762),
[abort_activation](../../../src/host/main.cc#L876),
[finish_activation failed branch](../../../src/host/managed_host.cc#L946).

`pool_box` is a `shared_ptr<shared_ptr<GenerationPool>>`. The pool stores an `on_drain_complete` callback
that captures `pool_box`. Assigning `*pool_box = pool` creates a reference cycle.

```text
GenerationPool -> options.on_drain_complete -> pool_box -> GenerationPool
```

On normal drain, the callback calls `reset()` and breaks the cycle. If persistence fails, `abort_activation`
restores the old routing view without requesting a drain on the new pool. Releasing the plan cannot destroy the pool,
so the callback never runs. `GenerationPool::create_adopted` explicitly forbids self-ownership.
The new callback bypasses the cleanup that previously followed plan release.

In the temporary application exclusive to this probe, use the `active.json` directory to prevent atomic writes and confirm the new
Worker is READY, deployment status failed, old v1 HTTP still returns 200. Capacity configuration is
`workersTotal=1`, `activationSurgeWorkers=1`. Both rounds of independent probes have yielded:

| Number of failures | Actual Number of Workers | Host thread |
| --- | ---: | ---: |
| 0 | 1 | 8 |
| 1 | 2 | 10 |
| 2 | 3 | 12 |
| 3 | 4 | 14 |

In the second round, the size of the Host RSS increased from 11,736 KiB to 17,124 KiB. The Host successfully terminated, and all related worker processes also ceased to run.
The issue occurred during the failed deployment process that continued to run on the Host.

Currently, the `host_managed_deploy_persist_failure_aborts` has been approved for this round. However,
its `TransactionHarness` does not have any callbacks for handling `pool_box` scenarios. Therefore, this issue cannot be detected.

There are still reference remnants that can be confirmed by source code on the same rollback path: prepare, which adds the old pool each time.
`draining_pools`, abort: this insertion is not canceled; drain: the callback only deletes the first matching item.
In the event of consecutive failures, the old pool's duplicate references should be retained and handled together. The new Worker should not be allowed to exit alone.

Rectification and Qualification:

1. Separate ownership of unsubmitted, activated, and draining pools, cancel the long-term ownership of new pools over themselves
   Strong reference, or lifecycle holding with clear abort and rollback exits designed.
2. After a persist/prepare failure, empty and release the unpublished pool on the joinable control thread; undo
   This reference is for the old pool parking. Avoid using `pools_mutex` to execute tasks that may wait for the callback's join.
3. Synchronize checking `abort_reserve` and `release_drained`: Drain notifications for pools that have not been submitted
   The capacity that has been rolled back cannot be released again, nor can the balance with other generations of the application be deducted.
4. Supplement the real Host's `websocket_managed_deploy-persist-failure` multiple times
   Failure and assertion that the old version is available, disappearance of the new Worker, threads returning to baseline, capacity reusability.
5. Add object-level assertions: pool/factory references release, threads complete join, no duplicate parking references.
   Maintain independent coordinator unit tests, which should also cover production wiring to avoid discrepancies between test callbacks and actual implementations.

The above new name is a suggestion and has not yet been registered.

**2026-10-14 GLM Correction Results (Section P1, Handled Regression Failure):**
`pool_box` changed to `std::weak_ptr` (restoring the non-ownership convention of create_adopted); abort path
Released pool through pure reference counting deconstruction (~GenerationPool = request_drain + wait, in coordination)
Threads are emptied and joined together. The drain hook locks first—the pool that is being destroyed neither recovers itself nor does anything else.
Repeatedly releasing capacity (abort_reserve has been settled). `abort_activation` and `abort_retire`
Cancel the parking references for the old pool (delete all matching items), and also delete all matching drain hooks. Added
`websocket_managed_deploy-persist-failure` (Real Host, 3 consecutive failures, assertion)
Legacy versions are available, new Workers disappear within ≤12s with bounded teardown, threads return to baseline, capacity can be reused):
Failed to restore the strong reference cycle with "failure 1: the never-published pool is reaped" (Red,)
`persist-failure/dpf-red.log`). Fixed Version 6/6 Passed and reapMs ≤ 2ms The destructor is called before failed
As seen in the completion). Stability: The request duration for the churn scenario has been extended to 5s (1s duration on the load generator for scheduling)
Noise becomes legitimate for replacement and deployment failure, see `persist-failure/dpf-loop-*.log` and round 5
For full-gate scenarios, set the values to final-last-test.log. For recursion, set it to 3/3 and persist to
6/6. The required configurations should be updated to fast 15/15, full 66/66, and no skipping at all (`index-abortfix.json`).

**2026-10-14 GLM Correction Results (§4 workerd is highly compatible):** 16 out of 17 issues have been fixed: C01–C14
(parameter/object construction – WebIDL ToUint16 truncation, USVString Symbol rejection, null init/protocols, binaryType
ToString, addEventListener callback verification, once registration period capture, Response 101 construction period
RangeError/endpoint exemption), C16 (outbound failure handling with message/error in ErrorEvent), C17 (synchronous handling of handlers: send/close
operations are discarded during the same round; transmission interruption – client 1006/wasClean=false; slow-consumer-related budget
rejections are handled through internal errors instead of sending 1011 network Close messages). Official
regression testing is performed using `api-compat.mjs` and `test_workerd_ws_fixtures.mjs`; dual-mode golden
assertions are used (reference verification, capsid verification and fixes, each with 30 assertions). **C15 remains unfixed**: Native
concurrent dispatching causes a frame loss under flood conditions (worker trace proves that in/sent operations are complete, but client-side
frames are missing; serial processing is stable with 6/6 success rates; concurrent dispatching results in ~40% loss of frames). Dispatches remain
serially processed. Api-compat uses explicit assertions to indicate the current gap and record golden results. Once the status changes to green, it
indicates that there is still work to be done. Synchronous aspects: The worker_websocket contract is updated along with C14; fast dispatching is maintained at 15/15.
Complete 66/66, with no skipped instances. (In the first full iteration, outbound_reference passed the test on both runs
within -j3.) Evidence: `index-c0116.json`.

### 2.1 Items to be processed subsequently (not in this round)

- C15 (Message Handler Concurrency Interlacing) - The root cause of native frame loss remains under investigation; The true protocol of C12/C14
  The timing of the transfer between the header and 101 still requires independent verification.
- Outbound reference depends on the configure environment, and the CI Node 24 vs 24.14.0 lock is inconsistent.
- Binding, owner/GC, CPU/heap/OOM, second client, platform and fuzz/soak matrix.

## 3 P1: The new generation regression has not achieved stable and complete qualification.

In this round, 17 items were independently selected. In reality, **16/17 items were selected, with zero failures**. The only failure
occurred in the `websocket_managed_replacement-recursion` task: the 10th deployment attempt failed, and no final assertion regarding
RSS/VmSize was reached. Subsequent attempts were successful. Therefore, it’s appropriate to keep records of both failures and successes. The results of the
last successful attempt should not override those of the failed attempts.

In each round, the number of workers waiting to be assigned tasks is limited to 1. This condition does not mean that the drain process for the old pool is complete, nor that the ledger balance
has been restored. There is a time window during which notifications and cleanup operations need to be performed. Failure logs do not get processed
in the worker_starting phase after validation. This is consistent with the behavior observed during the admission phase. However, the current Admin response does
not provide any details about the reasons for the failures. Therefore, the issue of “capacity exceeded” is not considered a confirmed reason for the failure this round.

It is necessary to establish observable conditions for the completion of generation recycling. Alternatively, verification can be carried out using shared
production components to ensure that drain/ledger and reference processes are completed before proceeding to the next round. The reasons for failures and the status at each stage should be retained for future evaluation
purposes. Do not rely on relaxed memory thresholds or arbitrary sleep periods to complete acceptance checks. Additionally, incorporate checks to ensure that objects
are returned only after the last administrative operation has been performed.

Neither of the 13-item nor the 64-item reports from GLM includes any new test cases. The `index-p1fix.json` file only contains
information regarding whether the test cases were passed or failed during the recycling process. It does not contain any relevant JUnit test
results, JSON descriptions, or original output from the Red system. New test cases should be included in the required test list, along with precise command details, outcome status,
input/source code/binary details, and complete reports. Old JUnit test results cannot serve as evidence for new test cases.

## 4 workerd has strong compatibility differences: 17 issues remain unresolved

Current Host, Worker, and re-locking of workerd with the same worker input: 27 API observatories
There are 14 differences, along with 3 event differences. The complete input and results can be found here.
[Section 3: Testing Completeness Review](test-coverage-review.md#section-3).

| Team | ID | The differences that still exist in this round |
| --- | --- | --- |
| Parameter Conversion | C01–C04, C07, C10–C12 | send lacks parameters/Symbol, close decimal/reason Symbol, binaryType object, conversion between URLs and protocols, and exception categories |
| Object Construction | C05/C06, C13/C14 | CloseEvent is a decimal/null value, Response 101 has no endpoint, and the construction phase is not yet accepted. |
| listener | C08/C09 | Illegal callback, removal and reregistration after modifying once options |
| Asynchronous Messages | C15 | reference allows the subsequent message handler to start before the previous await is completed; Capsid serial waiting |
| Failure Events | C16 | reference is ErrorEvent and message/error. Capsid is a regular Event |
| handler throw | C17 | reference close 1006/wasClean=false; Capsid 1011/wasClean=true |

C12/C14 still only confirms differences in the construction phase; the actual protocol header and the 101 handover need to be verified independently.
The tolerance for illegal references cannot be extended to cover all targets. Regular APIs, objects,
events, and normal lifecycles will be modified with strong compatibility in mind. Limits and deployment strategies will be declared independently.

All tasks related to wheels A–D remain pending completion: formal API/Response implementations, rigorous comparison of reports from both ends of the process, as well as
necessary reference checks for access control mechanisms. Two of the existing references have been successfully completed this round, indicating
that the current assertions are valid. However, this does not mean that all the aforementioned observations are compatible with each other.

## 5 Test Set and Gate: Recovering Registration Is Still Not Enough for Qualification

`websocket_outbound_reference` has been restored, but
[Conditional Registration Logic](../../../cmake/build_tests.cmake#L1835) No changes. Missing in the configuration.
`CAPSID_WORKERD_REF` will still remove it; the front wheels are retained when CTest returns 0 without a match
Evidence. In this round, `--no-tests=error` was used, and the selected names were checked against zero skips.

The CI still uses `node-version: 24`. The test lock is set to v24.14.0, and no steps were taken to lock down workerd. It’s
necessary to maintain fixed dependencies, consistent registrations, complete lists of dependencies, and strict comparisons. The total number
of 64/411 cannot replace the actual execution of the required sets.

Confirmed basic coverage of the front wheels: 69 v4 frame vectors, 30 state sequences, 20 core items
Directionality CTest Passed all of them, 84 A foundation WPT Documents. WPT does not have a WebSocket file yet; Foundation
The unit tests for EventTarget cannot cover the WebSocket's own wrapper layer.

The matrix that must be advanced, excluding A–D, still includes:

- E: Actual Host failure events, owner/CPU/heap/OOM issues, continuous succession, and final object/thread return.
- F/G: Binding-origin WebSocket, actors and budget attribution, GC pause/cancel/return/throw.
- H/I: Second client, promised native platform, v4/WS fuzz, and WS long-running tests.

Currently, the fuzz still uses v3 as its entry point. HTTP/memory-wave attacks cannot replace WS and generation churn in terms of
effectiveness. For detailed task and acceptance criteria, please refer to sections 4–6, which discuss testing completeness and verification.

## 6 The execution order of the merged GLM

1. Convert the real Host persist failure probe into a formal red test case, fix the issue of having a new pool by itself and an old pool
   Duplicate parking references and corresponding capacity rollbacks; independent test harnesses cannot be modified alone.
2. Verify and stabilize the completion conditions for the regression test, incorporate necessary datasets, save new test cases and the original Red report;
   Separate verification is not followed by subsequent control operations for final recycling.
3. Translate C01–C17 into formal regression tests and revise APIs/events. Complete strict reference of gate from A–D;
   Event scheduling modifies the synchronization running owner/GC verification to ensure cancellation and resource ownership under multiple Promises.
4. Complete the remaining E–I matrices, and then advance the release evidence tied to performance, collector, and submission.

## 7 Evidence and constraints for this round

The evidence directory is `build-v030-dev/evidence/review-latest-combined-20261014/`. The index
(located in the same directory: `build-v030-dev/evidence/review-latest-combined-20261014/index.json`)
contains information on source code, scripts, binary summaries, GLM report snapshots, results from 17 JUnit tests, one failure case, and three
reruns. It also includes details about normal iteration processes, samples taken during two failed deployments, as well as the latest reports from workerd.

| Product | SHA-256 |
| --- | --- |
| Host | `0e590a9da680bb21cb83ea480d422b0f0eaba00d2454be7f50c1f32c7fed9286` |
| Worker | `249649f6f77c6be7fa5ee9bdd7eba17eece0aa322af0c4a7a39a19799eee21cf` |

Reference: Keep v1.20261010.1; Compatibility Date: 2026-08-20; Flags: `[]`.
Node v24.14.0, undici 7.21.0; verify the reference hash and version before startup. This round of review
The source code and binaries of the system were unchanged during the inspection period, and the probe Host/reference exited normally.

This iteration only involves the addition of review documents and evidence for reproduction purposes. No new regression names have been added to the implementation guidelines or registration requirements. The
aforementioned evidence files have been ignored by Git; they need to be archived or re-generated when delivered across different machines.
