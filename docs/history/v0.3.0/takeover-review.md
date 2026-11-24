# Capsid v0.3.0 GLM implementation review and handoff (2026-10-13)

<!-- capsid-historical-context-20261117 -->
> **Historical plan/review — not current execution instructions.** The original
> body, counts, failures and pending language below retain their captured source/date.
> Stable is v0.2.2 and experimental 0.3.0-beta is now published. Use
> [current status](../../reference/release-status.md) for delivered scope, exact qualification and
> measured-source data; do not restart old campaigns from this record.
<!-- /capsid-historical-context-20261117 -->

The verification archive link on this page points to a copy saved with the source code at the time of saving; the original location, SHA-256, and collection time can be found here. [Archive Index](../../evidence/v0.3.0/2026-11-07/index.json) The original execution log remains stored at the local location indicated in the index, and the progress is based on the most recent record at the top of the page.



<!-- capsid-review-phase-107 -->
<a id="capsid-review-phase-107"></a>

## Round 107 (2026-11-07): SDK70 and branch fixes merged, continued acceptance of mainline and clean builds

**The current main working tree is the SDK70, JS v11 branch fix, and the version remains 0.2.2. v0.3.0 The overall goal has not been completed yet.** The historical boundary of "staying before final combination qualification" has been replaced by the current complete goal. The main working tree has not been submitted, pushed, upgraded, or released. The local snapshot submission in the independent clean verification copy is only used to meet the existing Release identity requirements.

The SDK70 has added the `0070-native-fetch-encoded-branches.patch` to handle the end of compressed branches, failed evictions, and the release of the last consumer from the upstream; JavaScript synchronization has begun to process the identity branches after decoding. `js/streams.js` has been regenerated with a controlled TypeScript patch, and the production generator `--check` has passed. The generated text is equivalent to the verified private candidate. The patch locks the overlay key at `b9d1f367f166f40117e64d87a4cc3f4a6aa64d9c6b4eb8c0a679adc866bd65bd`, and the manifest is `e27b6acc1a282a92dc39503ca4d059a54b55fb469af88b2fd96e549328d8efd0`.

- The merge is based on the sealed private R8 verification: a total of 1212 test cases for three configurations (888 complete public states, 216 production GC scenarios, 108 quota scenarios). 23238 consistent modification counterexamples were rejected, with 96 adjacent CTests and zero skips. These belong to the original private inputs; the main line is updated separately. Additionally, there are 390 complete public states matching the Linux native parameter matrix, 16380 product nested observations, and 4160 consistent state modification counterexamples. They are not separately closed for R3 or the old large JSON IPC diagnosis.
- The first SDK70 mainline input frozen resulted in 4774 files. The Windows Debug original set plus new tests amounted to 173 combinations, with 171 combinations having passed the check. Two complete gate control commands timed out within the original 120 seconds, and both the original results and serial reviews were retained. Windows ASan had 156 different combinations: 149 combinations ran successfully on the first attempt, three original commands passed serial review, four Hono tests passed, and zero skips occurred. Two decoding timeouts and one natural exit assertion failure that occurred during concurrent runs were still retained, with subsequent full acceptance checks to assess stability.
- The new builds for Windows Debug, Windows ASan, as well as the independent clean Windows Release and Linux Release/LTO builds have been successfully completed. All four sets of Hono products have passed the test, with a total of 16 items passed. The original application budget of 500 milliseconds remains unchanged. The remaining complete matrices for clean Release/LTO builds are yet to be executed. The ASan new builds initially lacked adjacent libraries, and the 152 original records of DLL loading failures are retained. After supplementing the three MSVC libraries specified in the build records, the tests were rerun without any change to the product binary or test budget.
- The private optimization version of gate testing only caches the immutable construction template as serialized bytes, ensuring that each control still receives a new object graph. Value-by-value, shared references, and object independence are all verified. The complete validator, actual archived I/O, 292 frozen controls, and a time limit of 120 seconds per group remain unchanged. The main working tree has 114 items in approximately 70 seconds, the archived group has 117 items in approximately 103 seconds, and the extended group has 61 items in approximately 50 seconds. All passed, with zero skips. This optimization has been merged, and the official registration command for the main tree is pending completion.
- The strict Linux build has identified a single-parameter `static_assert` that is not compatible with C++11. The diagnostic text has been corrected, and the condition, a 5/10-second event deadline, as well as the original test budget remain unchanged. This fix has been merged into the main working tree after passing the private Linux clean build. Affected Windows helpers and related commands continue to be updated. This subsequent fix only alters two test source files, leaving the production C++, SDK, and JS byte sizes unchanged.

The list of evidence for this round is located at `build-v030-dev/evidence/fetch-encoded-layer-20261107/`. The private complete basis can be found in `v11-framed-r8-progress-audit.json`. The controlled generation and SDK identity can be seen in `sdk70-main-integration-r2/integration.json`. The test source code will be revised in the future as described in `sdk70-main-integration-r3/integration.json`. Each new build, command, original deadline, JUnit, rerun due to failure, and summary of source/product are saved separately. The current requirements document is located in the same directory as `current-goal-requirements.json`. The progress of the 501 combination and below from the old SDK69, up to the 106th round, is kept as historical records and does not count as a complete qualification input for the current SDK70.

**Continuing execution:** Refreshing the main working tree affected tests and completing the current candidate's independent clean Debug/Release/Linux LTO without filtering the complete matrix; continuing with R3/R5/R6/R7/R8, and finally completing R9. The remaining combinations of native parameters and branch resource diagrams, DefaultReader precise sequences/latency, the cause of the old Linux large JSON `invalid IPC frame` issue, the historical 15 failed combinations, and the actual skipped assumptions are still being reviewed item by item. Complete dependencies on instrumentation, ASan/UBSan/TSan, fuzz/soak, Linux strict, supported Windows/macOS native, actual Hosted CI, the same session's formal three-road five-round performance, and the actual 2 GiB density and four-viewing areas still require actual evidence. WSL feature verification cannot serve as formal performance qualification; the formal performance release point remains at 0, with T15/T16/T17 kept open.

The following retains the original progress and historical boundaries for each round. The current state and the current requirements ledger shall be taken as the basis.



<!-- capsid-review-phase-106 -->
<a id="capsid-review-phase-106"></a>

## Round 106 (2026-11-07): Progress in Verifying Private Contexts and Unread gzip Branches

The current mainline maintains **JS v5, SDK69, version 0.2.2**. The C++ responses that have been merged and closed the eviction/poll issues continue to hold the qualification for 501 different "test×configuration" combinations: Windows ordinary 169, Windows ASan 152, Linux UID1000 180, skipping 0. Private JS/SDK experiments have not been merged yet; these private runs are not included in the mainline 501 count. The mainline is based on [Mainline 501 Combined Audit](../../evidence/v0.3.0/2026-11-07/response-retirement-progress.json) This round is based on [Private v9 Audit](../../evidence/v0.3.0/2026-11-07/context-v9-progress.json) and [Private v10 Audit](../../evidence/v0.3.0/2026-11-07/encoded-v10-progress.json).

Private v9 has completed 54 actions, 36 single reads, 72 branches with reading of GC, and 96 adjacent CTests have been started; 45 gzip complete states still retain piecemeal differences. The unread tee/clone high entropy matrix configuration in private v10 has 144 complete states that all match the actual reference, eliminating the original 30 delays in gzip branch differences; additionally, 252 internal CPU interface checks, 72 unread branch GCs, and 96 adjacent CTests have been conducted. The 87 adjacent protocol reports in v10 have been independently replayed. The actual negative comparison of the coding pending live points has failed in the triple configuration, while the raw control of the same variant has passed. Detailed inputs, summaries, counterexamples, and scope are provided in the aforementioned audit.

**The overall goal of v0.3.0 continues.** In v10, the unread gzip branch is only enabled when the response body limit is 0. It explicitly rejects the approach of “decoding a sub-branch after it has already been read” – this approach is not yet implemented. The decoding of the identity layer, the 45 context-related gzip chunks, as well as issues related to limits, abnormal compression streams, interruptions, Bindings, and other constructs and concurrent combinations still need to be addressed. Not all of the 684 private and public examples have been updated in v10. Next, these aspects will be implemented and verified, and then consideration will be given to integration with the main JS/SDK framework. R1/R2/R4 remain subject to their respective completion requirements; R3/R5/R6/R7/R8 are open for consideration; R9 remains unfinished. For more details, please refer to… [Current Target Account Book](../../evidence/v0.3.0/2026-11-07/goal-requirements-at-capture.json) The old boundary of "staying before final combination qualification" has been replaced by the current complete objective.

The first version of the private method has not been forwarded. There were 4 actual failures, ASan batch CPU checks timed out for 90 seconds, original compilation/line break checks failed, and audit script revisions were retained. CPU reruns divided the same 64 assertions into 8 non-overlapping groups, with each group retaining a 90-second limit. The original text is fully backed up in its original bytes. [This round of document changes serves as proof of](../../evidence/v0.3.0/2026-11-07/document-update-proof.json) Only these 9 documents are allowed to be changed, while the code and test qualifications remain traceable. No commits, pushes, or releases were performed in this round.

The following records are retained as history; the current state is based on the 106th round and the current target ledger.


<!-- capsid-review-phase-105 -->
<a id="capsid-review-phase-105"></a>

## Update Round 105 (2026-11-07)

The C++ snapshot exit and poll closure fixes have been merged. The current target set includes **501 different test×configuration combinations passed, with 0 skipped** (Windows: 169, ASan: 152, Linux: 180). The JS is still at version v5, with SDK 69 and version 0.2.2. Among the 21 formal counterexamples in the main branch, 8 failed, 19 runners failed, and all were fixed and rerun; 12 Hono instances maintained a 500ms application budget and passed serially. In this round, a total of 562 main branch tasks were executed, with 535 passed and 27 failed. 165 private candidate regressions are recorded separately.

72 scenarios of private v8's garbage collection resource policies and 36 single-read cross-request errors/ closed/release Scenarios are implemented through independent models. gzip18 still has segmentation differences, and out of 54 actions related to extension ownership, 51 still have public status differences. v8 remains private. The requirements for a complete garbage collector and branch ownership are still open. The health second request in the formal counterexample has been successfully completed. However, it failed due to a TypeError diagnosis and exited with exit code 1.

For the full scope and remaining tasks, please refer to [Current Handover Document, Round 105](next-steps-handoff.md#capsid-review-phase-105) and [Branch Follow-up Plan](native-fetch-plan.md); Mainline audit is [Mainline Audit](../../evidence/v0.3.0/2026-11-07/response-retirement-progress.json) GC audit is [Private GC Audit](../../evidence/v0.3.0/2026-11-07/lifetime-v8-progress.json), context audit is [Private Context Audit](../../evidence/v0.3.0/2026-11-07/context-v7-progress.json) The current ledger is [Current Requirements Notebook](../../evidence/v0.3.0/2026-11-07/historical-goal-requirements.json) R3/R5/R6/R7/R8 continues; R9 is not completed. The current complete version of v0.3.0 replaces the boundary that was “stuck at final integration stage”. No modifications or revisions have been made to commit/push/release. The original text remains intact, with no changes made to the version history.


<!-- capsid-review-phase-104-in-progress -->
<a id="capsid-review-phase-104-in-progress"></a>

## Round 104 in progress (2026-10-24): 480 directed combinations in the main working tree, 4 assistant quotas filled, and locating runtime timeout exceptions

The current main working tree has **480 different "test×configuration" combinations** for orientation checks: Windows standard 162, ASan 145, Linux UID1000 173. Currently, 0/0 combinations have failed/got skipped in qualification. The audit report is `upgrade58bsj-main69-helper-credit-progress-audit.json`. This round refreshed 18 old combinations affected by assistant product changes, qualified 6 existing protocol/cancel command combinations for the first time, and added 18 new official combinations. Additionally, 438 combinations were retained after verifying that they were not affected by source files, products, or registration commands. A total of 4755 test inputs were frozen, with 25 products tracked for each of the three configurations. The main working tree's Worker remains at version v5, and SDK69 with version 0.2.2 continues to maintain its current status.

0.3.0 Continues as planned. R3/R5/R6/R7/R8 are open; R9 is not completed, and the 104th round has not been sealed. **The timeout/cancellation exception for the newly discovered snapshot in the main working tree has not been resolved; the following C++ fixes are only valid for private copies.** The final combination, full/clean, real CI, platform/performance/density/viewport qualifications remain beyond the agreed boundaries; no submissions, pushes, releases, or version changes have been made.

### Main working tree merged: 4 C API test helpers returned with response quota

`tests/test_native_fetch_deadline.cc`, `tests/test_fetch_response_limits.cc`, `tests/test_worker_channel_discard.cc`, `tests/test_worker_v4_negotiation.cc` are now passed through `tests/response_credit_probe.h`, and `capsid_worker_grant_response_credit` is called for each actual consumption of `RESPONSE_BODY` bytes. This API only queues quota frames locally and does not actively flush them. The channel-discard retains the barrier order of observing CANCEL, then sending DATA and ACK temporarily. The original call parameters, deadlines, and protocol assertions are retained; the explicit `--small-response-window` mode is used to supplement cross-window observation.

| New command suffix (unified prefix helper_response_credit_) | Initial Response Window | Actual Coverage | CTest deadline |
| --- | ---: | --- | ---: |
| deadline | 32 bytes | 31/32/33/101 characters of text, each with a health second request | 30 seconds |
| limits / limits_encoded | 256 bytes | Each retains 320 original quotas, shards, cancellations, encoded-header scenarios | Each session lasts for 60 seconds. |
| discard | 16 bytes | 12 cancellations and 12 reuse requests for health checks | 30 seconds |
| negotiation | 3 bytes | Retain 7 groups of capabilities and unknown options / V3 READY check | 30 seconds |
| contract_controls | — | Complete bytes, quota, sequence, response headers, completion, and diagnostic reverse control | 30 seconds |

First, freeze the original behavior without quota corrections and run the real CTest: three configurations failed with 15 commands, 24 original observations, 18 exhausted initial window, 6 small text completed but without returning quota; complete stdout/stderr, JUnit, LastTest, and old assistant artifacts are retained in `upgrade58brv-negative-*`, `upgrade58brw-no-credit-artifacts-*`. After correction, 18 new command combinations passed all, 15 complete reports with 24 observations, byte-by-byte verification of **2037 responses, 1251210 bytes, 6108 actual fragments**, where 1983 responses exceeded the initial window. Additionally, reviewed the complete semantic state of 1920 quota scenarios and 90 deadline scenarios, which remained consistent with the previous iteration.

The new verifier initially treated all response headers as fixed-body, resulting in 3 failed real CTest reviews. The source code rules are as follows: If the body does not exceed the initial credit, it should be set to fixed-body; otherwise, it must be set to streaming header. Flags= 0, credit= 0. Amend to adopt this definitive rule, add 24 metadata counterexamples, with controls rejecting 109 error records per configuration. All 24 revised observations have passed, and all 24 unlimitated old observations remain rejected. The original failures with 3 errors of expectation were not covered as successes. For independent review, please refer to `upgrade58bry-model-replay.json`, `upgrade58bsj-full-response-review.json`.

This round of official CTest consisted of 75 attempts: 57 were successful, 15 failed due to no quota counterexamples, and 3 failed due to incorrect response header models. Only 42 successful commands were retained for qualification, and the remaining 15 were replaced by executions after the model was corrected. Over the course of SDK69, a total of 1526 attempts were made: 1466 were successful, 60 failed in history and were retained, and 986 successful attempts were replaced by subsequent runs; 1466−986=480. Private direct executions and model assertions are not counted as additional CTest combinations.

The default 256KiB quota consumption for the front-end `tests/test_native_fetch_response_limits.cc` is also retained. The actual response sizes of 262143/262144/262145/786469 bytes, with three configurations having 12 responses each, and 57 controls per configuration, are still included in the current selection according to their original evidence range, as specified in `upgrade58brh-main69-credit-progress-audit.json`. Four subsequent assistant tasks have been completed; however, it cannot be claimed that all C API consumers or runtime error paths have been perfected.

### New discovery: Snapshot text will be corrupted due to timeout/cancellation after partial transmission, leading to runtime exception states.

The native snapshot write of `PendingWrite.resolve/reject` is equivalent to `JS_UNDEFINED`, but `resolve_pending/reject_pending` still directly calls it. `TypeError: not a function` is generated in case of timeout or cancellation; the old 5 second timeout observation also resulted in an assertion of `UV_HANDLE_CLOSING` with Windows ASan. The source code shows that even after shutting down the poll, the subsequent output path may still call update_poll. The connection between the two paths must be isolated independently, and it cannot be claimed that the causal proof of a handle issue is complete just because the symptoms have disappeared.

The private C++ candidate is located in `upgrade58bsa-candidate/worker_runtime.cc`. Only snapshot callbacks that do not exist are skipped, and the process of restarting the closed poll is prevented. The request context, release, and original handling methods of the real Promise callbacks are retained. The candidate uses **main working tree JS v5**, without mixing in private JS v7, and the SDK69 has not been changed. Three sets of compilation/linking outputs are stored in `upgrade58bsb-{native,native-asan}-private/` and `upgrade58bsc-linux-private/`. The 25 main tree outputs remain unchanged.

The independent C API reproduces usage with a 32-byte window, 33/101 bytes of text, a request timeout of 750 milliseconds, covering snapshot/streamed text with timeout/cancellation/return quotas, and holding writes that are closed at the end. The original product with three configurations passed 36 items, while **12 items with snapshot timeouts/cancellations actually failed**. The candidate with 48 items all passed for complete events/byte, timeout states, reuse of the same Worker's health, and the natural exit model. All 576 consistent rewritten counterexamples were rejected. The READY status of Windows ASan was reviewed according to its actual zero-process-memory default value. The original private model judgments for the initial errors were also retained, as seen in `upgrade58bsd-windows-replay.json`.

The old ones with the same byte count 5 Repeated attempts by the No Limit Assistant within seconds 12 Subsequent: Original products now have additional instances of reproduction. 1 Closing the handle assertion; candidates 6 There were no errors of the TypeError, or any such assertions. But the assistant itself… 5 The duration of the event will be determined based on the Worker’s settings. 5 Second-level competition, **This 12 The assistant still failed. It’s still a failure.** It cannot be considered as a valid final state or evidence of healthy reuse. The complete semantic proof comes from independent sources. 750 Millisecond probes. Summarized as `upgrade58bsk-private-retirement-and-v7-audit.json` The C++ candidate has not yet been registered with the official CTest system, nor has it completed the integration process with the main tree. The patches that still need to be reviewed can be found in the relevant documentation. `upgrade58bsl-runtime-candidate.patch`, `upgrade58bsl-runtime-integration-proposal.json`.

### The private JS v7 is still retained within its original scope.

v6 There is 12 In the gzip compression scenario with low quotas, the bytes that have been accepted from the shared identity queue will be discarded. v7 Only release these bytes into a separate branch before exiting the limit, and then report any abnormalities related to that same limit. 19 The private source file and three sets of output files remain unchanged. The input values are as follows: `upgrade58brj-candidate/js/`. v7 Read first, then branch 144 Actions/Nesting 288 Unread branches 144 Total number of 576 In the observations that compare the actual results of using one set of fixed parameters with those of using two sets of fixed parameters… 546 A complete match. **30 There are still discrepancies in the gzip segments that have not been read yet.**;C API limits 108 It was verified using a local strategy model. 684 All observations have been completed, and the system has exited naturally. 4512 Revisions that conflict with the original text are rejected. Optimization of the quota mechanism is still needed. 23 Failed to meet the ASan deadline. v6 that 12 Failed to transfer data. Original fixture not available. 56 See also HTTP500 and 3 The data is retained even during intermittent collection processes.

The original 25 registered commands/settings for private Workers have been replaced with regression tests. Currently, the 63 unaffected commands remain in place, and the 12 deadline commands have been refreshed with the new quota assistant. The status of 90 refresh scenarios is consistent with the old records. Currently, there are still 75 commands and 1077 scenarios. The historical execution has accumulated 87 times, without increasing the main working tree to 480 combinations. Evidence is: `upgrade58bse-existing-*.json` And `upgrade58bsk-private-retirement-and-v7-audit.json` The original complete branch proof remains `upgrade58brp-private-v7-progress-audit.json`. The v7 version has not been merged or registered for formal testing.

### Current differences and continuation order

First, organize the C++ timeout/cancellation reproductions into formal fixtures/independent contracts/counterexamples. Complete the independent verification for closing polls and adjacent regression tests. After meeting the conditions, merge precise patches to refresh all main working tree combinations affected by runtime changes. **Do not directly transfer the current 480 old runtime combinations to the modified Worker.** Then continue with the v7 branch's garbage collection, cross-request ownership/concurrency, values/pipe, and any transmission shard quotas/exceptional exits. Finally, formalize and evaluate the JS candidate merge.

The main working tree v5 still retains 72 gzip-segmented differences: 30 unread texts and 42 texts that require reading before branching. In v7, these 42 differences have been resolved within the measured range, and the first 30 require additional coding-level prototypes to be compared with actual references. The fixed reference is the workerd v1.20261010.1, submitted with the commit 8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f, compatible date 2026-08-20, with flags = []; the complete state is directly compared. The new C API quota/request time probe does not have a corresponding public workerd configuration equivalent commitment.

For detailed plans, please refer to `docs/v0.3.0-native-fetch-branches-plan.md`. Evidence is organized in the `build-v030-dev/evidence/native-asan-qualification-20261018/` directory: the main working tree inputs/construction/ failures/final additions are sequentially `upgrade58bry-project-inputs.json`, `upgrade58bru-*-credit.json`, `upgrade58brv-negative-*`, `upgrade58brx-*`, `upgrade58brz-*`; the main working tree audit is `upgrade58bsj-main69-helper-credit-progress-audit.json`, and the private audit is `upgrade58bsk-private-retirement-and-v7-audit.json`. The source list and the list after document updates are saved separately; the content of the following phase103 and earlier is retained in its original form, and its historical tasks do not replace the conclusions of this round.

<!-- capsid-review-phase-103 -->
<a id="capsid-review-phase-103"></a>

## Round 103 (2026-10-21): BYOB parameter order, standard capacity rejection, and ABI self-check synchronization

This round of closure has confirmed **8 standard JS capacity rejection differences**, and has addressed the parameter conversion, views, release status, and recipient checks for five construction paths. Locked versions of Windows and Linux workerd **v1.20261010.1** (`8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f`) have been tested with **435/435** results. Before the fix, the actual phase102 product had **417 passes and 18 failures**, with all of the original 417 passes retained. The additional failures were due to 8 standard capacity scenarios and 10 verification groups, with the original records retained as `upgrade57e-*` and the independent review as `upgrade57f-baseline-review.json`. The compatibility date remains **2026-08-20**, with no flags specified. The complete locked submission is `8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f`.

Product modifications are focused on `js/websocket.js`: strictly verifying the instance identity of the BYOB reader; distinguishing the parameter binding order between `read(view, options)` and `readAtLeast(min, view)`; the `min` getter follows a short-circuit order, executed once on the path where it should be executed; the original object is retained after the conversion is thrown. According to the reference, first check if the value is within the signed 32-bit range, then truncate the decimal part; the same exception types, text, and order are required for combinations such as BigInt, Symbol, invalid views, separated inputs, released readers, and readers released or separated during conversion. The standard JS stream delegates the original reading after verification, maintaining the view type; the native path continues to convert bytes to minimum width according to the element width. The reader reacquired after release must still be able to fully read the original 16-byte data, and then complete EOF, cancellation, and unlocking to prevent data being swallowed up or the state being contaminated due to parameter errors.

The official list is as follows: **435 = 330 Item native view + 64 Item standards, compared with JS standards + 10 Item type borrowing + 10 Item parameters verified + 21 Item Native Body**. 10 Groups contain **420 strip** Independent observations: Each of the five tectonic pathways has its own unique characteristics. 44 These lines and those related to “readAtLeast”… 40 Check the order of events, the number of getter calls, the object causing the exception, as well as aspects related to the same VM type, input separation, backing mechanisms, subsequent reading operations, and final processing steps. This is necessary. 420 This method uses standard JS byte-source. It ensures that each operation is performed in a complete manner. 16 Bytes. The views are mainly… Float64Array Additionally, there are also empty, separated, and invalid inputs. The parameters corresponding to the actual fetch of the native body need to be determined, and retries should be attempted if errors occur. The resource matrix still needs to be filled in. Other view types also require further testing. Internal observations do not represent any new additions to CTest. 417 The relative order of the items, independent 46 And then there’s the transition phase. 21 All parts of the body with a large volume, empty bodies, and short-bodied objects are retained. The five runtime groups are organized accordingly. **100, 100, 106, 108, 21** Each group consists of… **120 seconds** - Peers for each scenario **10 seconds** Unchanged. Formal contracts and anti-example controls remain unchanged. **8507/8507**; This includes the independent control that must be exercised when reference errors of the same magnitude occur along with errors in all synthetic candidates, as well as errors related to the number of getter operations or abnormal identities. This is an essential aspect of the original access control mechanism. **482** Continue with the item control settings. **114 + 114 + 254** Executed, frozen 228 The names of items, the actual names used during execution, and the precise commands/environments/timings have not been relaxed at all. Each of the three groups follows the same rules. 120 Seconds, total budget 360 Continued using the same method as before. 102 This round doesn’t have any additional additions.

The key verifications for the first version of the product on ordinary Windows and ASan systems included **10 failed verification groups and 20 failed nested observations**, all resulting in negative unsigned64 diagnostics. In actual use, the global `BigInt.asUintN(64, negative)` function returns negative values on ordinary Windows and Linux products, while the reference implementation returns the corresponding unsigned integers. The original probes and failures are retained in the `upgrade57j-*` and `upgrade57j-first-focus-review.json` files. This round of testing only involved changing the BYOB path to use precise arithmetic `(1n << 64n) + negative`, **without fixing the built-in functions of the global BigInt**. Ultimately, the 4th and 5th groups of the three configurations passed the key tests, recorded as `upgrade57l-*`; the old `upgrade57h-*` remains failed.

The first round of the complete regression is **223/224**: For the `abi_guard_without_worker` on standard Windows, after the private control child process is confirmed to exit and the handle is closed, the job statistics still show 2 processes. The ASan and Linux regressions all passed. The original `upgrade57m-native.json`, JUnit, and complete logs remain failed. Subsequently, only the self-check in `tests/test_abi_guard.cc` was modified: requiring the real process to exit, with a exit code of 0, and the job count to return to exactly 1 within a maximum of 5 seconds. Both the old and new checks must reject real live child processes, and the general final cleanup check still executes immediately. The Windows job statistics are updated after the process exits and releases the reference. [Microsoft Job Statistics Description](https://learn.microsoft.com/windows/win32/api/winnt/ns-winnt-jobobject_basic_accounting_information) and [TerminateProcess Description](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess) The independent extraction of the original and candidate versions passed 100 runs on two different Windows configurations. Independent extraction did not reproduce the original failure, and therefore cannot replace comprehensive testing. The corrected complete ABI executable program passed **50 consecutive runs** on both ordinary Windows and ASan environments. These 100 repeated runs still only correspond to one registered test, and are not included in the following 224 items.

| Final Main Build | ALL / Package | Correction of Regression Tests Executed After ABI Self-Test | The complete gate of the actual execution and strict review in this round |
| --- | --- | --- | --- |
| Windows Ordinary Debug | Pass | 74/74 | 79/79 |
| Windows ASan Debug | Pass | 74/74 | 79/79 |
| Linux UID 1000, build type is empty | Pass | 76/76 | 100/100 |

Final Regression **224/224**, Complete Required Gate **258/258** Zero failure, zero skipping. The gate was already in the final runtime before the above ABI test file corrections were made `upgrade57k` Input to execution. Subsequent `upgrade57t` All/Packaging Confirmation **host/worker All other gate products are byte-for-byte consistent, and all are registered with CTest identically 4519 One input is for a single change in the ABI test code** According to `upgrade57t-retained-runtime-gates.json` Retain the gate. The regression of the final source input is a new execution, and the gate is clearly followed by the actual execution of the same product with the same input from this round **It is not claimed that the ABI self-check changes have been re-executed after the gate, nor has the previous phase102 gate been reused**. The regression failures of the original ordinary Windows were not covered by the gate. CTest registration remains **577 + 578 + 811 = 1966**. All items that were not executed in this round and were not filtered out 1966 Items.

The evidence root directory is `build-v030-dev/evidence/native-asan-qualification-20261018`. Runtime gate freezes the input and creates a complete snapshot `upgrade57k-project-inputs.json`, `upgrade57n-tested-project-inputs`. Final ABI corrections for input and snapshots: `upgrade57t-project-inputs.json`, `upgrade57v-tested-project-inputs`, all **4519** One copy. Final ALL/ packaged as `upgrade57t-build-*`, The new regression is `upgrade57u-*`, The complete gate is `upgrade57m-*`, Untruncated gate log = `upgrade57o-*`, strict archiving and replay of failed old products `upgrade57p-*`. Final **418** This product, library, object, and selected diagnostic files are `upgrade57v-current-build-binaries`. Shelved for `upgrade57-final-audit.json`. Eight documents have been updated after verification. Prepared by `upgrade57w-post-verification-documents.json` Single column. One product document, eight test/contract/samples, and eight documents in total **17** One modification, no new or deleted files.

**Full body compatibility and 0.3.0 qualification remained open.** Of the 44 previously unqualified standard combinations, 8 capacity cases formally passed. The remaining **36** were tracked separately: 12 unaligned EOF internal errors, 6 Float64 zero-length EOF views, 10 controlled high-minimum cancellations and 8 older wait-script issues. Private probes on both platforms matched **59/59** observations for 16-byte sources and **44/46** for 8-byte sources. Two Float64 read/readAtLeast cases used minimum=2.9 (truncated to 2 elements, requiring 16 bytes). At an aligned 8-byte tail, the reference returned the tail. The product reported `Insufficient bytes to fill elements in the given buffer`, affecting subsequent teardown. `upgrade57q-postfix-diagnostic-review.json` preserves the differences and full comparison. The 420 passing parameter checks on complete 16-byte sources did not close the findings.

The parameters/leases/invalid inputs of the actual native body are retried, and the resource matrix, global BigInt unsigned conversions, other coercion/receiver/intrinsic changes, concurrent reads, owner/abort/context end, and released.closed semantics remain open. The actual non-zero C ABI `max_fetch_response_body_bytes` Upgrade body path, known/unknown length below/equal/above, and error recycling have not been tested yet. The DefaultReader precise chunk sequence, latency/performance, and old Linux large JSON `invalid IPC frame` have not been closed; ordinary fetch metadata and tee/clone/values/pipe resource diagrams have not been fully qualified. Historical **15** combinations, final inputs without filtering 1966 items, independent clean Debug/Release/Linux LTOs, actual CI, complete dependencies sanitizer/UBSan/TSan/fuzz/soak, formal three-lane five-round performance, actual 2 GiB density, and four-viewports are still pending acceptance. The formal performance release point is **0**, T15/T16/T17, D005/D010/D012 are open. Version **0.2.2**, patch lock **59**, Root HEAD remains unchanged, with no submissions, pushes, or releases.

Next steps: See below. [Round 103 Handover](next-steps-handoff.md#capsid-review-phase-103) The following sections of the 102nd round and earlier are retained as historical inputs. The status of tasks is subject to the conclusions of this round.

<!-- capsid-review-phase-102 -->
<a id="capsid-review-phase-102"></a>

## Round 102 (2026-10-21): Large-scale reading, DefaultReader allocation, and EOF offset

This round of corrections addresses the discrepancies in actual readings beyond the initial window, and corrects two instances of real-world reference errors. Locked successfully on Windows and Linux systems: **417/417**. Two issues were resolved: **401 cases were approved, and 16 cases failed**. Among these, **6 cases involved issues with DefaultReader backing capacity**, and **10 cases had non-zero input offsets in BYOB scenarios**. The original reference data, labeled `upgrade56e-*`, was reviewed carefully in detail using `upgrade56f-baseline-case-review.json`. The initial verifier mistakenly recorded EOF offsets as 16; however, actual references returned 0 in all cases. The previous results were not revised, but the corrected samples, labeled `upgrade56g-*`, now show 417 successful cases on both platforms.

The final product only requires modifications to `js/websocket.js`. The native byte source automatically allocates a DefaultReader buffer of **4096** bytes. It copies data to the controller's BYOB view and calls `respond()`; when the view cannot accommodate the entire transfer fragment, it retains the remaining portion and returns the credit based on the actual number of copied bytes. Only by configuring automatic allocation and then `enqueue()` will the original fragment continue to be exposed, and this fix cannot be completed. The native BYOB returns an empty EOF view that is the same as the transferred backing view, with a zero-length byte offset of **0**. The backing capacity and outer padding values are retained. Limiting the wrapping layer to transfer the existing correct backing results in no changes to `js/fetch.js`.

The official list consists of **417 = 330 items of native view + 56 items of standard JS comparison + 10 items of type borrowing + 21 items of native body**, while the original 396 items and the independent 46 items of transition are retained. The new 21 item includes a DefaultReader with five construction paths, the empty/short body 10 items, five paths of BYOB minimum/readAtLeast, 10 items, as well as a large-scale DefaultReader 1 item. All large-scale responses are **1 MiB + 37 bytes**. BYOB requires a complete response for a single input offset 16, separation of two inputs, an empty EOF, complete sequential bytes and SHA, outer bytes, the same VM identity, and a normal conclusion. The actual size crosses **64 KiB** of the initial window.

Each actual reference/backend execution. **100, 100, 106, 90, 21** Five groups. Access control requires a complete list of all groups, identical input/binaries/run identities, and independent operations across groups. challenge/VM Each group remains the same. **120 seconds** The single-scenario peer remains the same. **10 seconds** The original body transmission records are compressed without loss. After decompression, the original length, SHA code, and each byte are verified. No parts of the large response are discarded. The observer uses an equivalent method for processing these data. 32 Batch reading of bits reduces the overhead associated with JS callbacks. Additionally, there are other improvements. **111150 Posterolateral segment + 53240 Current payload = 164390 times** Independent byte-by-byte comparison is performed, covering empty intervals, misaligned beginnings and ends, individual byte changes, and multiple errors. Formal byte-stream contracts/anti-examples are used for control purposes. **7733/7733** The original six-file access control system is still in use. **482/482** Press the button accordingly. **114 + 114 + 254** Divided into three necessary CTests; each group consists of… **120 seconds** The total budget for the control group is determined based on individual items. 120 Changed to seconds **3 × 120 = 360 seconds** This adjustment to the grouping does not mean that the total processing time is still below the specified threshold. 120 Seconds; each scenario during runtime 10 In seconds, and for each shard 120 Second-row constraints. The first two groups are independently checked and frozen for accuracy. **228 Items/Things** The ordered naming convention ensures that the registration process is properly executed. Both auditors are involved in enforcing constraints on the selection process, as well as in managing command precision and timeout settings. The final execution name after combining the three components matches the original one. 482 All items were processed correctly; no private decoding caching schemes were used.

| The final actual main build input | ALL / Package | ABI, Strategy, Context, Lifecycle, and Package Regression | The fully executed required gate |
| --- | --- | --- | --- |
| Windows Ordinary Debug | Pass | 74/74 | 79/79 |
| Windows ASan Debug | Pass | 74/74 | 79/79 |
| Linux UID 1000, build type is empty | Pass | 76/76 | 100/100 |

Final regression **224/224**, fully executed required gates **258/258**, zero failures, zero skips; three sets of latest outputs each passed with 21 additional focused tests. No reuse of gates from the previous round of execution. CTest registration is **577 + 578 + 811 = 1966**; no unselected 1966 items were executed this round, and internal scenarios and verifiers did not duplicate the number of CTest items.

The first version includes complete main verification functions. 224 All items have been successfully registered for regression. ASan 77 Relevant to Linux. 98 The access control has been approved, but for ordinary Windows systems… 77 There are several access control systems available. 1 Item six files are subject to control measures. 120 Seconds; initial failure. `upgrade56x-native-initial-failure-review.json` And JUnit, complete logs are retained. Attempts to use private decoding caches still take about as much time as before. 110.8 Seconds have passed, insufficient quantities available. No records were written to products or formally verified. In the end, fixed lists were used to manage groups. Two additional CTest elements were added. The final result is as follows: 258 The access control functions are based on newly inputted and executed commands. The previously archived data has not been successfully combined into a single entity. The first verification after splitting the data results in some output directories being missing. Four functions support control based on actual ENOENT failures. The issue has been resolved by rechecking the script directories after creation, and the results are recorded as follows: `upgrade56zh-focus-driver-correction.json` The source code has not been altered as a result of the testing.

Failed development periods are kept separately: The first version failed due to ineffective allocation, with 111 ASan combinations touching 120 seconds (`upgrade56i-*`); after correcting data writing, the second version passed, and ASan exited in about 98 seconds, as the large-scale DefaultReader observations at the end touched a single scenario for 10 seconds (`upgrade56p-*`). The latter only optimized the execution of byte-by-byte observations, without reducing response time, decreasing checks, or extending time limits; the actual product binary is identical to the final observation version. The corresponding inputs are fully preserved in `upgrade56k-tested-project-inputs` and `upgrade56t-tested-project-inputs`. The maximum 1000 cycles of the earliest private probes and the Linux large report `invalid IPC frame` are recorded in `upgrade56b-*`; the complete read after compact output is in `upgrade56c-*`; the IPC diagnostics triggered by the large report have not been individually identified as the cause and are not listed as fixed.

Final freeze of inputs **4519** Number of copies: `upgrade56zh-project-inputs.json`, `upgrade56zj-tested-project-inputs`. Actual ALL/ packaged as `upgrade56zh-build-*` The actual regression/complete gate is `upgrade56zi-*`, Uncut gate log = Uncut gate `upgrade56zk-*`, All actual archives are strictly replayed with the old 16 Item failed review as `upgrade56zl-gate-archive-replay.json`. Final **418** A set of actual products, libraries, objects, and selected diagnostic files `upgrade56zj-current-build-binaries`. The entry is sealed `upgrade56-final-audit.json`. These eight documents will be updated separately after the verification is completed `upgrade56zm-post-verification-documents.json` Differentiate from test freeze input.

**The body has not yet been fully closed for strong compatibility or 0.3.0.** This round aligns with DefaultReader, fixing the backing capacity, bytes, offsets, and EOF. There are still differences in the actual block length and number of times: this round focuses on samples of 1 MiB + 37 bytes in response. Both platforms refer to **257** data reads, and the current three sets of single-workers each have **2049** reads, with backing being 4096. The original shape is retained in `upgrade56yc-default-segmentation-review.json`, and the complete backends of the gate are also archived and replayed in a single line, **without consistently considering the exact chunk sequence as passing**. below/equal/above, which has non-zero response limits, and those with known/unknown lengths and erroneous endings are still not qualified. The runner has not set this C ABI limit, and cannot successfully replace the limit boundary with a larger body. BYOB completely empty bodies, other min conversions/parameter orders, invalid/separated views, similar readers released/reclaimed, concurrent reading, owner/abort/context end, built-in attribute changes, other recipients, and ordinary fetch/tee/clone/values/pipe branches still need to be completed.

Previously, **44 standard JS byte stream combinations were still not qualified**: 8 normal capacity rejections, 12 misaligned EOF internal errors, 6 Float64 EOF zero-length views, 10 controlled high minimum cancellations, and 8 old waiting script issues continue to be tracked separately, with the actual error IDs retained. Among them, 8 capacity rejections have been re-measured using current Windows and Linux products: refer to return `TypeError: Minimum bytes to read (24) exceeds size of buffer (16).`, product return `RangeError: options.min must be less than or equal to view's length`; original samples and dual-platform reviews are at `upgrade56yd-*`, and they are not yet counted towards the official 417 pass. The remaining fields, unseparated inputs, and final touches cannot replace exceptional compatibility. This round also does not close the historical **15 failed combinations** and skips the prerequisites. New independent clean Debug/Release/Linux LTOs, real CI, complete dependencies sanitizer/fuzz/soak, official three-way five-round performance, real 2 GiB density, and four viewports. The official performance release point remains **0**; observer speed-up is not considered a product performance release point, and T15/T16/T17, D005/D010/D012 remain open. Version **0.2.2**, patch lock **59** remain unchanged, with no Root submissions, pushes, or releases.

Next steps: See below. [Round 102 Handover](next-steps-handoff.md#capsid-review-phase-102) The following chapter of round 101 and earlier are retained as historical inputs. Their status is determined by the conclusions of this round.

<!-- capsid-review-phase-101 -->
<a id="capsid-review-phase-101"></a>

## Round 101 (2026-10-21): Reader prototype borrowing, fixes for Float16Array and ABI testing

This round has completed the two priority reviews from the previous round. Real-world tests on Windows and Linux have confirmed that the old product would contaminate the cancellation reason when using another type of reader's `cancel()` method, and would reject incorrect recipients. The `Float16Array` would incorrectly reject normal reads due to missing element width information. Fixes resulted in **376 passes, 20 failures / 396 items** across both platforms, which corresponds to **10 prototype borrows and 10 Float16Array cases**. Both platforms have locked the reference at **396/396**. The original failure exits, individual reasons, and binary identities are retained in `upgrade55e-*` and `upgrade55f-baseline-case-review.json`, and are not marked as passed.

The product only requires modifications to `js/websocket.js`: Record independent weak reference types. Before parameter reading, context checking, and modifying suspension states, cross-class prototype borrowing between Default/BYOB readers is prohibited. The type records remain valid after `releaseLock()`, and the active I/O context is still unassociated as usual. Errors in `read/readAtLeast/closed/releaseLock/cancel` synchronous operations throw a fixed-reference `Illegal invocation` exception, which does not trigger option getters, does not separate input, and does not change the subsequent normal cancellation reasons. The element width is derived from the `byteLength/length` getters of the TypedArray captured at startup, overriding `Float16Array`. It does not depend on constructors or width properties that can be changed by the application. This round does not claim that all error recipient boundaries, such as ordinary objects, fake instances, and Proxy, have been completed.

The official BYOB list has increased from **306 to 396 items**, retaining the old **250 native items, 56 standard JS comparison items**, and **46 independent transition items**. Eight new views of `Int8Array/Uint8ClampedArray/Int16Array/Uint32Array/Float16Array/Float32Array/BigInt64Array/BigUint64Array` have been added, covering default/min2/min3/readAtLeast2/readAtLeast3 and held/EOF. There are **80 items** in total, all using `getReader`. The five construction path matrices of the original five views are retained. Additionally, **10 new native reader type checks** have been added, covering two types of readers and five construction methods, and checking the borrowing behavior before and after release. Currently, there are **330 native views, 56 standard comparison items, and 10 prototype borrows**; the native view family consists of **13 types (including DataView)**, not all of which have been tested with five construction methods.

Literal contracts and independent control over rejecting error reports **7058/7058**; actual complete six-file gate control **477/477**, including a single file for overall gate control **223/223**. Added Float16 separation, cancellation reasons, synchronization exceptions, and counterexample checks after release, and it is required to have additional grouped reports for actual existence. The composite backend report is only used for verifier control; the product passes from the following actual CTests.

| The actual current main build after final input | ALL / Package | ABI, Strategy, Context, Lifecycle, and Package Regression | Complete and Necessary Gate |
| --- | --- | --- | --- |
| Windows Ordinary Debug | Pass | 74/74 | 74/74 |
| Windows ASan Debug | Pass | 74/74 | 74/74 |
| Linux UID 1000, build type is empty | Pass | 76/76 | 94/94 |

The final complete regression **224/224** was executed with a complete gate **242/242**. The final result showed zero failures and zero skips. The gate was first executed on the `upgrade55i` actual product. Subsequently, only the ABI test source code was corrected. The gate scripts/scenarios for `upgrade55r` remained unchanged when compared with the running product binary. Therefore, the same actual gate was used according to `upgrade55t-gate-reuse-review.json`, and it was strictly archived and replayed for confirmation. The 224 regression was re-executed after the final build was completed. The gate cannot be described as a new round of execution after correcting the ABI test. Four groups of **100, 100, 106, 90** were executed for each actual reference/backend. The requirements were a complete list, the same input and binary/run identities, as well as a unique challenge/VM across groups. Each task still had an upper limit of **120 seconds**. Currently, the CTest registration amounts to **572 + 573 + 805 = 1950**, and the number of internal scenarios and verifiers does not count as additional CTest instances.

The first complete regression is **223/224**, of which ASan has **73/74** failures. The failure `abi_guard_oom_egress_empty` caused the irrelevant update processes started three days ago to be treated as unfinished processes. The original test only compared parent PIDs, and the reuse of historical PIDs led to false positives. The results are recorded in `upgrade55p-abi-ownership-root-cause.json`, which retains the actual process identity and source inference, as well as the 1→2→1 transformation of 10 real private Jobs and four independent copy checks for normal/ASan. These diagnoses do not replace the formal regression. `tests/test_abi_guard.cc` now checks the entire sub-process tree based on the number of processes with existing private Jobs, and adds control over passing the test after rejecting and terminating real suspended sub-processes in the existing Windows ABI scenarios. The inspection intensity remains the same as the original test, without any relaxation. For the semantics of Job counts, please refer to [Microsoft Documentation](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_accounting_information) The original first attempt failed, `upgrade55j-native-asan-*` remains a failure; the actual complete regression after correction is completed in `upgrade55s-*`.

The initial input and run were performed on `upgrade55h-*`; the `upgrade55i` input with LF and sample group meta-information cleaned up, along with actual gatekeeping, were used. After ABI testing corrections, the final `upgrade55r-project-inputs.json` only changed `tests/test_abi_guard.cc`. Three sets of ALL/package and 224 regression tests were re-run. The product source code and the gatekeeping running product remained unchanged. The final **4518** inputs were stored in `upgrade55t-tested-project-inputs`; the previously accurate captured inputs were stored in `upgrade55e-baseline-project-inputs`; the `upgrade55i` inputs were saved in `upgrade55k-tested-project-inputs`. Finally, **418** actual products, libraries, objects, and selected diagnostic files were stored in `upgrade55t-current-build-binaries`. The actual regression and gatekeeping records were categorized under `upgrade55s-*` and `upgrade55j-*` respectively. The complete gatekeeping logs were stored as `upgrade55l-*`; the reuse checks and strict replays were stored as `upgrade55t-gate-reuse-review.json` and `upgrade55v-gate-archive-replay.json`; the final audit report was saved as `upgrade55-final-audit.json`. These eight documents were updated separately after verification and stored separately from the frozen test inputs.

**0.3.0 Strong compatibility with the body is still not fully completed.** Previously, **44 JavaScript byte combinations** have not yet been qualified; 8 normal capacity checks, 12 misaligned EOF internal errors, 6 Float64 EOF zero-length views, 10 controlled high minimum cancellations, and 8 old script waiting errors are still being tracked separately. The original error reference IDs are retained and not normalized as passed. Premature reading, empty EOF, min conversion and parameter order, invalid/separated views, similar readers release/reacquisition, concurrent BYOB, owner/abort/context end, built-in property changes, large bodies exceeding the credit window and actual response limit boundaries still require additional testing. The normal fetch and tee/clone/values/pipe branch diagrams remain open; the current six-byte peer cannot prove the credit for the large body. There are also differences in the actual default reading shapes: on two platforms, the private six-byte premature reading has a backing of 4096, while on older products it is 6 (`upgrade55b-*`); this round of prototype borrowing scenarios uses premature reading as a byte checkpoint, without checking the backing capacity of DefaultReader, so it cannot be claimed that this difference has been resolved. Further testing is required along with the block/allocation shape of the large body, see `upgrade55l-open-coverage-review.json` for details.

The **1950 complete matrices** that were not filtered in this round remain unexecuted. The old **15 failed combinations** and the historical skipped prerequisites have not been closed. The newly input independent clean Debug/Release/Linux LTOs, real CI, complete dependencies including sanitizer/fuzz/soak, formal performance five-rounds, real 2 GiB density, and four-viewports are still pending acceptance. The formal performance release point remains at 0, and T15/T16/T17 are still open. The version remains at **0.2.2**, with a patch lock at **59**. There are no Root commits, pushes, version upgrades, or releases.

Next steps: See below. [Round 101 Handover](next-steps-handoff.md#capsid-review-phase-101) The following sections from the 100th round and earlier are retained as historical inputs. Their status is determined by the conclusions of this round.

<!-- capsid-review-phase-100 -->
<a id="capsid-review-phase-100"></a>

## Round 100 (2026-10-21): Upgrade rollback of native BYOB base matrix for HTTP response body

This round of modifications involves `js/websocket.js` and `js/fetch.js`. The Upgrade/Regression HTTP data channel now provides a byte source; native BYOB reading converts to a minimum size based on the element width of the original view, and returns a `Uint8Array` of the same range, preserving the input offset, backing buffer, and separation behavior. Non-empty EOF tail blocks that are less than the minimum size retain `done=false`; the native `closed` signal for normal completion is delayed until an empty EOF reading or cancellation. Explicit completion is performed when the byte source is closed; the ACK length is saved before byte enqueue, limiting the ability to retain bytes associated with native streams. Standard JS byte streams retain their view type, and the positive call path for `readAtLeast()` is added.

New independent official suite added. `tests/test_websocket_fetch_byob.mjs` - Work scripts, actual TLS peers, formal contracts, and compressed Linux reference samples. Each is executed on the actual backend/reference system. **306 Items/Things**: **250 Items are native in nature.** Covering `getReader` Direct construction, `prototype.constructor` Subclasses `Reflect.construct` Five paths. `Uint8Array/Uint16Array/Int32Array/Float64Array/DataView` Five views, default settings /min2/min3/readAtLeast2/readAtLeast3 And held/EOF; There are also others. **56 Item standards for JS byte streams** The minimum value represents the number of elements. In practice, an odd-length value of five bytes can be returned. The offset is also relevant. 8, backing 32 Separate status, EOF, empty view, cancellation reason types, and response order all require independent validation. The same reader should first read the leading bytes, and then measure the remaining five bytes. This prevents misjudging the transmission as being in a “minimum” state before it is actually ready. Old… **46 This is a transition point.** All retained. Abnormal checks for the generic engine after DataView separation are performed. `TypeError` Class; This round does not have a declaration. V8/QuickJS The general error messages are exactly the same.

The original baseline has been preserved for restoration: Both Windows and Linux platforms have successfully completed the process of locking workerd **306/306**; In the 99th round of actual product processing, each item has been processed accordingly **36 cases passed, 270 cases failed** Among them, **250 attempts have been made to obtain native BYOB resources, but 20 instances still have missing read operations. At least…**. Captured in `upgrade54e-*` The product’s binary identity has been verified against the previous round of ALL results. Please review it carefully `upgrade54o-baseline-case-review.json`. `tests/test_workerd_fetch_byob_contract.mjs` Independent control **5208/5208**; Complete six-file access control system **472/472** (Among them) `tests/test_workerd_reference_gate.mjs` The single file is… **218/218**). The synthesis platform/backend report is only used for testing purposes. The actual product uses the data from the real CTest mentioned later.

| The current main build in this round | ALL / Package | ABI, Strategy, Context, Lifecycle, and Package Regression | Complete and Necessary Gate |
| --- | --- | --- | --- |
| Windows Ordinary Debug | Pass | 74/74 | 71/71 |
| Windows ASan Debug | Pass | 74/74 | 71/71 |
| Linux UID 1000, build type is empty | Pass | 76/76 | 90/90 |

This round of regression testing **224/224**, with a complete gate **232/232**. There were no failures or skips, and all tests were rerun using the modified actual main build. BYOB is divided into **100, 100, 106** groups for each reference/backend. The gate requires that all three reports be present, the list be complete, and the source code/binaries/run identities be consistent. Cross-group challenges and VMs are not allowed to be reused. The original **120 seconds** limit for individual tests remains unchanged. Currently, CTest totals are **569 + 570 + 801 = 1940**. The 306 internal scenarios per platform/backend do not count as additional CTest instances.

First ungrouped runs are also fully preserved: Windows normal build selected **8/8** Pass. ASan Selection **6/8**, two 306 Example product operation involving all involved parties 120 Maximum number of seconds, see `upgrade54j-native-asan.*`. The following only modifies the test groups and aggregations. The product source code remains unchanged. Then, the aforementioned actual ALL/package operations are carried out 224 Regression and Completeness 232 Gate. First time during development period 350 The reference item is also found in the capture section 8 AtLeast There should be a waiting condition. Correct it first and then collect the clean data 306 Item baseline. The initial script and failed data `upgrade54c/d-*` Uncovered. The independent JUnit fixed list missing in the early overall control `upgrade54g-*` Check and recheck the complete document after completion, and record it `upgrade54h/l-*`. All of the above failures have been rewritten as successes.

**There are still 44 JS byte stream combinations that have not been officially approved in this round.** Observations from each platform's development period include: 8 minimum oversized normal parameter checks, 12 misaligned EOF internal errors, 6 cases of Float64Array EOF zero-length views with a backing containing five bytes, 10 controlled high minimum suspension cancellations, and 8 cases of errors waiting for the above scripts. Capacity checks are to be covered up; the rest must be handled separately or confirmed regarding the choice of a fixed profile. These cannot be collectively referred to as reference defects. The list is found in `fetchBYOBDeferredStandardCases`, and the original error reference IDs are retained in the capture. They were not normalized or relaxed to consider as product approval. For details, see `upgrade54q-open-coverage-review.json`.

The current freeze of inputs `upgrade54m-project-inputs.json` and `upgrade54p-tested-project-inputs` amounts to **4518** items. Of these, **418** items are actual products, libraries, objects, and selected diagnostic files found in `upgrade54p-current-build-binaries`. Ungrouped inputs are stored in `upgrade54m-before-shards`. The earlier `upgrade54a-before-project-inputs.json` and the 99th round of archives remain unchanged. Execution logs are stored as `upgrade54m/n-*`, and the final audit archive is `upgrade54-final-audit.json`. These eight documents are updated separately after verification, without mixing with previously frozen inputs.

**This iteration does not involve full closure of the body for strong compatibility or 0.3.0.** Issues such as unread first read of native reading, empty EOF, order of parameters and accessors, illegal borrowing of reader prototypes, release/reacquisition, concurrent BYOB, owner/abort/context end, and large bodies across credit windows with limit boundaries still need to be addressed. The normal fetch, tee/clone/values/pipe, and branch resource diagrams remain open. The current six-byte fixture cannot prove the credit supplementation for large bodies, and cannot be corrected with the aforementioned ACK source code instead of actual testing.

The version is still **0.2.2**, with a patch lock of **59**. There are no Root commits, pushes, version upgrades, or releases. 1940 complete matrices were executed without filtering this time. The historical **1877/1819/34/24** and **1762/1733/6/23** are retained according to their respective inputs. There are still **15 old failed combinations that have not been closed**. The default Debug version of 1 MiB Hono, worker reselection, old WPT/profile, newly input independent clean Debug/Release/Linux LTOs, real CI, complete dependency sanitizer/fuzz/soak, formal three-way five-round performance, real 2 GiB density, and four-viewports are still pending acceptance. The formal performance release point is still 0; T15/T16/T17 has not been completely closed.

Next steps: See below. [Round 100 Handover](next-steps-handoff.md#capsid-review-phase-100) The following chapter of round 99 and earlier are retained as historical inputs.

<!-- capsid-review-phase-99 -->
<a id="capsid-review-phase-99"></a>

## Round 99 (2026-10-20): Direct DefaultReader construction path for native body

This round of fixes has addressed the confirmed gap in reader association in `js/websocket.js`. The original implementation only associated the native body with `body.getReader()` when reading was suspended; now, the same association is achieved directly, while maintaining the instance validation of the original constructor, `newTarget`, and prototype relationships. Global construction, `prototype.constructor`, subclasses, and `Reflect.construct` have all been subjected to actual verification. The standard JS stream continues to use its original queuing reading behavior.

The formal fetch transition has been increased from **30 items to 46 items**, with the original 30 items retained, and four new constructs added, each with three native behaviors and one standard flow control. The native items include checks for object cancellation, cancellation before the first read, and suspension of the second read. The standard flow control involves queuing two reads followed by cancellation, requiring both to complete normally. The new constructs also verify the identity of instances, prototypes, constructors, and prototype constructors. Original verifications such as byte, reason type, Promise response order, actual TCP termination, same VM, input, and binary identity continue to be effective.

**Evidence of true failures to pass recorded.** Windows and Linux lock the workerd each 46 Item and approval. The actual product on both platforms before the fix was **38 Item passes 8 Item verification failed**, original 30 All items and all standard flow controls passed, failure is due to cancellation of four constructs and a second suspension read. The original complete captures and failed exits are saved in `upgrade53b-*` Review item by item `upgrade53d-baseline-case-review.json`. The reference sample is based on real Linux 46 Item execution generation, checker and rejection errors/omissions/matrix of identical errors are independently controlled **1596/1596**. Its synthesis platform/backend replication is only used for verification purposes.

| The current main build in this round | ALL / Package | ABI, Strategy, Context, Lifecycle, and Package Regression | Complete and Necessary Gate |
| --- | --- | --- | --- |
| Windows Ordinary Debug | Pass | 74/74 | 61/61 |
| Windows ASan Debug | Pass | 74/74 | 61/61 |
| Linux UID 1000, build type is empty | Pass | 76/76 | 77/77 |

This round of regression testing resulted in **224/224** total tests, and the total number of complete gate tests was **199/199**. All tests were re-run after this round of modifications, with zero failures and zero skips. The official reports for each reference and actual backend include 46 items; both ordinary Windows backends, Linux backends, and ASan backends actually underwent the same strict verification. Currently, the CTest registration still stands at **1907** items, and additional scenarios within the internal scenario do not constitute new CTest items.

The first attempt at completing the process resulted in **196/199** errors. All three failures occurred with the `workerd_ws_gate_controls` module. Although the number of attempts in the formal scenario increased to 46, the total number of independent elements that still require updating remains 30. Internal checks show that each of the 463/464 elements needs to be updated. The results from the original `upgrade53f-*` command remain failed. Subsequently, we changed the value for this expectation in `tests/test_workerd_reference_gate.mjs` to 46. We kept the fixed list and all strict checks intact. After re-running the process three times on the same actual implementation, we obtained the latest results of **199/199**, which are recorded as `upgrade53r-*` results. This failure is not due to any product-related issues or timeouts.

ALL/All packaging and 224 regression tests use `upgrade53e-project-inputs.json`; the final version uses `upgrade53p-project-inputs.json`. There is no difference between the two versions, except for the aforementioned control tests. Product inputs and all 418 protected outputs remain unchanged. Both input snapshots are stored in `upgrade53h-tested-project-inputs` and `upgrade53q-tested-project-inputs`, with **4510** copies of each. Outputs are stored in `upgrade53h-current-build-binaries`. Execution logs are stored as `upgrade53e/f/r-*`, and the final audit report is saved as `upgrade53-final-audit.json`. This document was updated after verification, and now corresponds to the frozen test inputs. The previous version, `upgrade52-final-audit.json`, as well as any previous failed tests and complete matrices, remain intact.

**This round only closes the tested differences in the direct DefaultReader path mentioned above.** BYOB, multi-byte views, tee/clone/inner reader, ordinary fetch and limited packaging, as well as owner/credit/resource diagrams, are still pending and awaiting formal qualification. Rounds 97/98, which are independent clean Linux Debug and Windows non-LTO releases, as well as Linux Release/LTOs, belong to their respective old frozen inputs. The JS changes from this round have not been re-qualified in these independent snapshots; therefore, the conclusions from the old Release/LTOs cannot be extended as qualifications for this round's products.

Version is still **0.2.2**, patch lock **59**. This round did not perform a complete matrix filtering; the most recent completions are **1877/1819/34/24**, and earlier ones are **1762/1733/6/23**. The mappings selected currently still have **15 old failed combinations not closed**. The default Windows Debug 1 MiB Hono, unstable worker reselection, old WPT profile, real CI/complete dependency sanitizer/fuzz/soak, and official performance, real 2 GiB density, and four-viewport configurations are still not qualified. The official performance release point is 0; T15/T16/T17 has not been completely closed. There are no Root submissions, pushes, version upgrades, or releases.

Next steps: See below. [Round 99 Handover](next-steps-handoff.md#capsid-review-phase-99) The following sections from round 98 and earlier are retained as historical notes.

<!-- capsid-review-phase-98 -->
<a id="capsid-review-phase-98"></a>

## Round 98 (2026-10-20): Optimizing real-time allocation probes and complete spawn exception traversal in the build process

In this round, Root only modified `tests/test_abi_guard.cc`. After the original allocator replacement function was inlined under GCC `-O3 -Werror`, it was diagnosed as belonging to a different allocation family due to the implementation of malloc/free and array allocation delegation. As a result, four allocation entries and eight release entries were reserved for non-inlined probes. The warnings, optimizations, and normal allocation/release pairing remain unchanged. The intermediate candidates that only failed to compile at the release entry were retained. Private verifications under six actual SDKs were completed with nine modes, no parameters, and unknown mode rejections before Root was integrated.

The formal nine-mode now injects `bad_alloc` and common C++ exceptions into each allocated point that is successfully spawned. The common exceptions must actually occur, and according to the observed nothrow exception handling requirements, the exceptions must be precise in terms of `OUT_OF_MEMORY / INTERNAL_ERROR`, empty output pointers, non-empty errors, and resource conservation. The necessity of this is evident in real-world sensitivity counterexamples: private runtimes that only remove common exception cleanup during construction still pass through the original coverage, and extended coverage checks leak handles. This correction does not change any product source code, quotas, or protocols.

| Current Main Build | ALL / Package | This round of ABI, protocol, strategy, registration, and documentation review | There is evidence of gate access |
| --- | --- | --- | --- |
| Windows Ordinary Debug | Pass | 23/23 | Follow the complete 97th round with 61/61 |
| Windows ASan Debug | Pass | 23/23 | Follow the complete 97th round with 61/61 |
| Linux UID 1000, build type is empty | Pass | 23/23 | Follow the complete 97th round of 77/77 |

In this round, the total count is **69/69**, with zero failures and zero skips. A comparison was made with the non-probe products archived in the previous round. Only the files related to the ABI allocation model changed. The SUT/auxiliary binaries, commands, and configurations used for the gate were unchanged, so **199 complete gate** test evidence was retained. None of these 199 items were rerun in this round, nor were the replaced test models considered the same binaries. The **224/224** main regression results from the 97th round and the **60/61** results from the first-round ordinary Windows gate were retained, along with subsequent complete retesting.

**New and clean Linux Release/LTO Candidates have completed the actual tasks ALL, Packaging, 28/28 Selected regressions and 77/77 The system has a complete access control function.** Both the actual SDK compilation and worker linking commands have been marked with the LTO flag. The source tree remains clean; Its frozen input is: `upgrade51al-project-inputs.json` This round of verification results in `upgrade51am-*`. It only differs from the previous failed candidates in this test file. All product inputs are the same; Original `upgrade51x-*` All failures are still retained as originally entered; they have not been overwritten. Clean Windows, not LTO releases **32/32 + 61/61** Clean Linux Debug environment **28/28 + 77/77** It is the 97th round of independent measurements; Windows’ LTO mechanism still forces shutdown, maintaining the existing baseline boundaries.

Currently, the Root has frozen the following items: **, 4510,**, `upgrade52e-tested-project-inputs`. The products/libraries/objects related to the selected diagnostic files are **, 418,**, `upgrade52e-current-build-binaries`. There are still **, 1907,** items registered. The versions are **, 0.2.2,**, and the patch lock is **, 59,**. This round of operations did not include the entire matrix. The latest complete results are still **, 1877/1819/34/24,**. There are still **, 15 combinations that have not been resolved;** remains active. The earlier items **, 1762/1733/6/23,** are kept in a separate list. The execution records are `upgrade52c/d-*`. The items from the previous round are stored as `upgrade51-final-audit.json`. This round’s items are stored as `upgrade52-final-audit.json`. This document will be updated after verification, along with the frozen test inputs, which are listed separately.

The two types of C++ exceptions handled by spawn still cannot be extended to cover all memory/ABI qualifications: non-head points at entry points such as requests and transaction rollbacks, bindings, and native C/OS allocation. Resource pressure also needs to be expanded. Direct reader access to the native body, BYOB, tee/clone, ordinary fetch, and context/credit/resource diagrams still have known differences. The next round of solutions is outlined in `upgrade51aq-native-body-integration-plan.json`. Default Windows Debug with 1 MiB Hono, unstable worker reselection, old WPT profile, current complete matrix, platform/CI, complete dependency sanitizer/fuzz/soak, and official three-lane five-round cold/QPS. The actual 2 GiB density and four-viewports are still under acceptance. The official performance release point remains 0; T15/T16/T17 has not been completely shut down. There are no Root submissions, pushes, or releases.

Next steps: See below. [Round 98 of Handover](next-steps-handoff.md#capsid-review-phase-98) The following sections from round 97 and earlier are retained as historical notes.

<!-- capsid-review-phase-97 -->
<a id="capsid-review-phase-97"></a>

## Round 97 (2026-10-20): Custom configurations, v4 initialization, and real resource recycling

Five product source code corrections and four test/registration corrections have been integrated: temporary strings/ containers used in strategy compilation can throw an empty constructor; snapshot reserved capacity and in-place construction of environment entries; internal result types using throwable copies, to avoid implicit `noexcept` moves in Debug STL from terminating upon allocation failure. Worker construction and normal exceptions in v4 initialization will also reclaim IPC and child processes. Protocols, quotas, and strategy decision-making rules remain as defined.

The formal spawn allocation has covered nine valid configurations: default, explicit empty/non-empty egress, empty capability policy, permission rules, capability snapshot, v4 without channel/complete channel, snapshot with complete channel. For each path, the number of successful allocations is first tested, and then the first out-of-bounds point is injected and verified. The `nothrow` method is also actually used in the injection process. Linux has added point-by-point descriptor conservation and subprocess checks, and corrected the inspection blind spot of "claiming success after reclaiming residual zombies." Both real leakage descriptors and residual subprocesses, which are private counterexamples, were rejected. The independent registration contract restricts the name, actual mode parameters, 60-second upper limit, and normal exit conditions item by item, while retaining the existing TSan exclusion boundaries.

| Actual build | The selected real regression | The complete set of workerd requires a gate. |
| --- | --- | --- |
| Windows Normal Debug Main Build | 74/74 | 61/61, stand by for re-inspection |
| Windows ASan Debug Main Build | 74/74 | 61/61 |
| Linux UID 1000 is the main build, with the build type set to empty. | 76/76 | 77/77 |
| Independent and clean Linux explicit Debug | 28/28 | 77/77 |
| Independent clean Windows Release, LTO off | 32/32 | 61/61 |

The first three sets of the current main build have actually completed `ALL` and packaging. The selected regression tests **224/224** were passed. The complete latest execution with the gate has **199/199** passes. The last two independent clean candidates have completed new `ALL`, packaging, identity, and package consumer/recurrence checks. They cannot replace the full matrix. The four Hono tests for Windows Release have passed with a **1048576 bytes** JS stack. The four old failures in the regular Windows Debug are still open.

The first gate control in standard Windows actually results in **60/61**: `workerd_ws_gate_controls` has a timeout of **120 seconds**. Since the aggregation failed and only zero results were listed, the original JUnit record contains a total of 61 results. The difference between this failure and the successful outcome is retained in `upgrade51e-native-*` and `upgrade51y-*`; after stabilization, the same source code, binary, and upper limit are used to perform a complete gate control, resulting in `upgrade51z-native-*` as the outcome. No additional runs are concatenated into a single successful outcome, and no changes are made to the original failure.

Current Registration **1907** Items not rerun in this round will not be filtered in full. The complete matrix is still at the 94 Round **1877/1819/34/24** (Select/Pass/Fail/Skip), all three configurations exit 8. Earlier **1762/1733/6/23** Single column is for historical purposes. According to the 95/96 Round the re-verification mapping, the accumulated closed old failed combinations 19 One, **15 The Gejiu combination is still not closed**. The new configuration added in this round resulted in seven actual failures on two sets of Windows systems. It has now been included in the formal test cases for verification. Do not rewrite the old full statistics based on this.

**Tests and failures still pending:** The first `ALL` of the independent Linux Release/LTO failed at the `-O3 -Werror=mismatched-new-delete` compilation stage due to old allocator probes, with `upgrade51x-*` still failing. The next version of the probe retains an non-inline boundary for allocation/release entries and adds a standard C++ exception for each successfully spawned allocation point, with precise return values according to the observed nothrow exception handling requirements. Six actual SDK configurations, including **54/54** modes, six parameter-free runs, and six unknown mode rejections, all passed. Only the private runtime library that removes standard exception construction cleanup passed the original test, but handle leaks were detected in the extended test, proving that the new coverage is effective. Phase-by-phase candidate failures, final candidates, and counterexamples are available in `upgrade51af/ah/ai/v-*`; the application file is `upgrade52a-staged-abi-guard.cc`. These private passes have not yet closed the original failures completely in the complete `ALL`. Another independent clean candidate, `upgrade51al/am-*`, is being verified with new inputs, with results listed separately.

Windows LTO will be forced to be disabled by the existing baseline configuration. The configuration asserted after enabling this request correctly identifies the actual values; the result is retained for `upgrade51u-*`. The qualification scope for Windows Release is clearly not LTO, with Linux taking on the actual LTO verification. The request values are not considered as actual build options.

Freeze test inputs **4510** One, version still **0.2.2**, Patch Lock **59**, in `upgrade51j-tested-project-inputs`. Products, libraries, objects, and selected diagnostic files built in three main working trees **418** A copy of `upgrade51j-current-build-binaries`. Save the independently built selected output `upgrade51ag-*` The outputs that were missing due to the first LTO failure are clearly listed. Build/Regression: See `upgrade51d/e-*`, Place the gate and see `upgrade51z-*` The official statistics can be found in `upgrade51an-main-spawn-statistics.json` This round of sealing is as follows: `upgrade51-final-audit.json`. This section is a verification document and should not be mixed with frozen inputs.

Strong compatibility has not yet been fully qualified: direct readers of the native body, BYOB, tee/clone, standard fetch, context/credit/resource graph, default Debug Hono, worker reselection instability, old WPT default profile, full recheck, other platforms/real CI, complete dependencies on sanitizer/fuzz/soak, and official three-lane five-round cold/QPS. The GiB density and four-viewport of real 2 are still under consideration. The current probes mainly fully traverse the C++ allocation of spawn; entries such as requests still only have representative first points. C/OS/binding pressure is not equal to the covered area. The official performance release point is still 0, and T15/T16/T17 has not been fully closed. There are no Root submissions, pushes, or releases.

The subsequent execution order is as follows: [Round 97 Handoff](next-steps-handoff.md#capsid-review-phase-97) The following sections from round 96 and earlier are retained as historical notes.

<!-- capsid-review-phase-96 -->
<a id="capsid-review-phase-96"></a>

## Round 96 (2026-10-20): Default ABI allocation failed, resource recycling, and test coverage correction

All three current main builds have completed the actual `ALL` phase and packaging. The default spawn of MSVC Debug allocation failed, which previously terminated in the `noexcept` construction of STL empty containers. Now, a throwable empty construction is used, and field exchange is employed to avoid additional allocations during Debug construction. When an exception occurs during the Windows command-line construction, the two IPC descriptors are reclaimed by local guards. The original 172 checks are passable, but the counterexample with 12 handles remaining is retained; the corrected version shows a handle difference of 0.

| Actual Configuration | The actual regression selected in this round | The complete set of workerd requires a gate. |
| --- | --- | --- |
| Windows Ordinary Debug | 66/66 | 61/61 |
| Windows ASan Debug | 66/66 | 61/61 |
| Linux UID 1000, build type is empty | 68/68 | 77/77 |

The selected regression total is **200/200**, with the required gate total also being **199/199**. Both have zero failures and zero skips. The regressions include real ABI/Protocol/Lifecycle, package consumers, and fresh package reproductions; new independent test cases without worker parameters are added. By default, the test cases first measure the successful path, then iterate through each C++ allocation point to verify the first out-of-bounds point successfully: Windows has **58 failed points + successful paths / 346 checks**, Linux has **13 failed points + successful paths / 97 checks**; in the no-worker mode, there are **14 / 14 / 28** checks respectively. The `nothrow` single-value/array injection has real counterexamples; Windows checks the handle conservation for each point and verifies that there are no child processes at the end; immediate failure is reported in case of abnormal termination. Independent registered audit constraints ensure that the actual commands, parameters, a 60-second upper limit, and a normal exit status are met, while the exclusion boundaries of the original TSan allocator symbol conflicts are retained; TSan metadata control does not represent new TSan product qualification.

<a id="capsid-review-phase-95"></a>
In round 95, the descriptor regression, build tool selection in configuration, short nested directories, explicit empty consumption configuration, and package reproduction tool transfer were merged. The actual selection was **34/34 + 34/34 + 36/36 = 104/104**. The assertion for CTest automatic package fixtures was not calculated in the first collector, the suspension of the Linux CLI wrapper after being incorrectly passed to the native tool override variables was retained, and the corresponding recovery and correction were also preserved. The final fixture directly called the CMake-selected CLI, and checked the tool lock version along with all eight real-time groups.

Currently, **1883** items are registered, but this round does not rerun or filter the full set. The most recent complete matrix is still from the **94th round: 1877 / 1819 / 34 / 24** (selection/pass/fail/skip, exit code 8 for three configurations); earlier **1762/1733/6/23** are still other historical results. In the two rounds of selection and rechecking, 19 old failed combinations were closed, **15 old failed combinations remain unclosed**: two combinations for ordinary Windows workers, four for the default 1 MiB Hono, two old WPTs, and clean source code identities; for ASan and Linux, it's two old WPTs and identity prerequisites. This is a mapping of old failed items and should not be considered as new full failure statistics.

**Newly added configurations that are not currently disabled in the main library:** After expanding to nine valid configurations including default, explicit empty/non-empty egress, empty capability policy, permission rules, capability snapshot, v4 without channel/complete channel, and snapshot with complete channel, **7/9** configurations fail in both sets of Windows current libraries. Explicit empty policy point 2 terminates abnormally, string construction in permission rules, container movement in snapshots, and point 41 of v4 results in actual failures with two handles and child processes. `upgrade50e/j/k/q-*` indicates saved failures and intermediate candidates; `upgrade50s-*` indicates that both sets of Windows are **9/9** successful, and `upgrade50t-*` is **9/9** successful on Linux. Independent parameter-free modes are successful, while unknown modes fail. The same nine modes are also passed in the current Linux library, so it cannot be generalized from Windows Debug STL issues to existing Linux failures. The combined candidates have not been merged into the frozen main input for this round; in the next round, duplicates found in the candidates must be eliminated, and formal registrations, independent contracts, Linux descriptors, and zombie checks must be recreated. Default paths passing does not mean that all custom policies, bindings, v4, or OS/C assignments failures have been covered.

Test the input **4510** The version remains the same **0.2.2** Patch lock **59**. Currently `src/js/include` There have been changes in the C++ code. Therefore, it is no longer possible to continue using the historical description from round 94, which stated that “153 product inputs remain unchanged”. Fully frozen input is currently in… `upgrade50g-tested-project-inputs` Current product/library/object and selected diagnostic files **418** That’s one more copy `upgrade50g-current-build-binaries`; Not all files are executable programs. Execution records are as follows: `upgrade50c-*`, `upgrade50d-*` The final audit result is `upgrade50-final-audit.json`; Cycle 95 has been sealed `upgrade49-final-audit.json`. This section of the document has been updated after verification. It now includes a separate column for frozen test inputs.

Complete 0.3.0 strong compatibility remains unqualified: direct readers of native body, BYOB, tee/clone/normal fetch, and resource graph differences; default Hono, remaining old failures, explicit Linux Debug, clean Release/LTO/ALL/package and consumer, complete dependency sanitizer/fuzz/soak, other platforms/real CI, formal three-lane five-round cold/QPS, real 2 GiB density and four-viewports remain pending. The existing Linux `CMAKE_BUILD_TYPE` is empty; passing Windows ASan configuration does not imply complete dependency instrumentation. The formal performance release point is still 0, and T15/T16/T17 has not been completely shut down. There are no Root commits, pushes, or releases.

This round of handover and subsequent sequence are as follows: [Round 96 of Handover](next-steps-handoff.md#capsid-review-phase-96) The following sections from round 94 and earlier are retained as historical notes.

<!-- capsid-review-phase-94 -->
<a id="capsid-review-phase-94"></a>

## Round 94 (2026-10-20): Default coding stack fixes, complete three configuration baseline, and new differences added

The number of QuickJS patches has increased to **59**: The interpreter uses five immutable JSValue values for reuse. In standard MSVC Debug mode, the number of fixed frames used is determined by… **20504 Down to zero 17992 Bytes** Original compilation options, Debug checks, and default settings 1 The MIB budget remains unchanged. The three configurations are tailored specifically for each case. **23/23** At the same time, the implementation of SingleWorker is included in the static libraries of the Host core when Boost is enabled. The link compensation for individual round-trip tests is also removed. Three configurations are used in practice. **ALL** All tasks have been completed.

| Actual Configuration | The complete workerd requires a certain number of gates. | Do not filter complete CTest results: Choose / Pass / Fail / Skip |
| --- | --- | --- |
| Windows Ordinary Debug | 61/61 | 549 / 523 / 16 / 10 |
| Windows ASan Debug | 61/61 | 550 / 528 / 12 / 10 |
| Linux UID 1000, `CMAKE_BUILD_TYPE` is empty | 77/77 | 778 / 768 / 6 / 4 |

Total Summary **1877 / 1819 / 34 / 24** All exit codes are 8. The gate passed, with no full failure in closing. The original JUnit, logs, and individual failures/skips are retained `upgrade48l-*`. The general Debug names used in Linux should be based on the actual caching situation in this round. Do not use these names to determine the Debug qualification. Windows Ordinary Hono is still available 1 Four overflows under MiB. ASan Hono explicitly uses 2 MiB cannot be used to close the default budget compatibility.

The direct DefaultReader, BYOB, tee/clone methods in the native body still have practical reference differences compared to standard HTTP. For details on the selection set, see `upgrade48r-*`, `upgrade48s-*`, `upgrade48v-*`. The Host environment variable settings for standard HTTPS are not equivalent to the existing C-ABI explicit CA settings, and it is necessary to correct the testing prerequisites. The independent descriptors have been verified for Windows 24/24 + ASan 24/24. The new model has actually failed in the old library, while the candidate has passed 17 items. Linux with strict compilation has passed 14 items; these candidates have not been merged into the complete baseline in this round. The configured Windows esbuild allows the original eight sets of runtime counterexamples to pass with two configurations; a short directory allows the original ASan configuration/counter-examples to pass; the consumption candidate for Linux with empty configuration has also retained both positive and negative examples.

Root tested input **4509**, version still **0.2.2**; 153 inputs from `src/js/include` remain unchanged, but they have been recompiled with patch 59 during runtime. All test inputs are saved as `upgrade48ax-tested-project-inputs`, and a total of **409** binary/database/diagnostic files are saved as `upgrade48al-current-build-binaries`. Full strong compatibility, clean Release/LTO/packages/consumers, remaining failures and platform prerequisites, official three-way merge/2 GiB density/quad viewport, complete dependency sanitizer/stress/soak are all pending. T15/T16/T17 has not been completely closed. No Root submissions, pushes, or releases have been made.

The complete results, implementation boundaries, and handoff sequence for this round can be found here. [Round 94 of Handover](next-steps-handoff.md#capsid-review-phase-94) Evidence integrity audit for `upgrade48-final-audit.json`; specific difference proposal is `upgrade48an-implementation-and-test-review.json`; the verified next set of fixes is saved as `upgrade48az-next-fixes.patch`. The following round 93 and earlier are historical records, with the old **1762/1733/6/23** also retained as separate historical baselines.

<!-- capsid-review-phase-93 -->
<a id="capsid-review-phase-93"></a>

## Round 93 (2026-10-20): The Five Stack QPS Controller and Archiving are connected

The current QPS public interface has been replaced with the old shell. By default, it operates in five rounds/managed/strict mode, covering five stacks, three workloads, exclusive ports, PHP dual roots, and Go client ownership. It processes tasks in a trickle-first, then another load mode, ensuring precise readiness/response, recursive stability of PSS, and automatic cleanup and sealing of self-checking. The official mode requires clean build/session metadata; the short `--observation` flag is used without a publishing envelope. The old message "QPS producer still not connected" represents a historical state.

Final Tool Selection Set: Windows Normal/ASan **14/14, 14/14** (Python 101: 70 passed/31 skipped on platform), Linux **18/18** (120/120); No full product reengineering or sanitizer qualification for this round. True Five Stack 15 units with short observations, 2,191 sealed files, and SIGINT/SIGTERM **10/10** passed during measurement; shared cold/density actual short observations also passed. Old verifiers had 10 failures and the first Linux PSS instability failure were retained; final Linux individual review/sequential regression did not relax the protocol.

Complete 0.3.0 strong compatibility, T15/T16/T17 still not fully closed. Clean Release/LTO/ALL/package and consumer, complete gate/matrix, same quiet session formal three-lane/2 GiB density/quad viewport, native body/pressure/soak, Windows default Debug encoding stream root reasons and other platforms/dependencies sanitizer still pending completion. Historical **1762/1733/6/23** not rerun; version still 0.2.2, 4508 input, runtime/Host 153 input unchanged, no Root commit/push/release.

Implementation, real and synthetic test boundaries, retained failures, evidence entry points, and handoff tasks can be found in [This round of full handover](next-steps-handoff.md#capsid-review-phase-93) Final Audit: `upgrade47-final-audit.json`; Linux final results are stored as `upgrade47q-linux.json`. The failed `upgrade47m-linux.json` should not be reclassified as a successful result.

<!-- capsid-review-phase-92 -->
<a id="capsid-review-phase-92"></a>

## Round 92 Status (2026-10-20): QPS load correctness/original evidence fixes, full producer continues

Fixing issues in the flow text of the real Go load tool, including missed checks, errors/timings out classified as errors with successful verdicts, failure exit codes, and invalid parameters. Added complete text identities along with counts of attempted/checked/completed operations. Formal verification requirements include `exact-bytes-v2` and a 30000 ms deadline. The old actual tool's three sets of red examples and 28 failed assertions from the old verifier are retained. The final CTest results are Windows **13/13**, Windows ASan **13/13**, and Linux **16/16**. New rules are added in 8 groups, with 8 groups of actual Linux HTTP tests and 2 groups of contract supplements.

A real tool's 64/64, 5+15 seconds round only faced with exclusive correctness fixtures; additionally, credible bytecode was run twice for startup/102 responses/38 small file observations. Neither is considered a formal performance metric. `src/js/include` had 153 unchanged inputs, with the version still at 0.2.2; the old QPS shell still does not have a fully functional five-stack producer. Complete strong compatibility, clean ALL/complete matrix, body/stream pressure, formal three-lane/2 GiB/quad viewport, and complete dependency sanitizer/other platforms are still pending completion, and historical complete results are not updated.

Scope, platform boundaries, binary/raw failures, and the next steps are as follows: [Round 92 Handoff](next-steps-handoff.md#capsid-review-phase-92) Final Audit: `build-v030-dev/evidence/native-asan-qualification-20261018/upgrade46-final-audit.json`. No Root commits, pushes, or releases have been performed.

<!-- capsid-review-phase-91 -->
<a id="capsid-review-phase-91"></a>
## Round 91 Status (2026-10-20): Compiler releases compatible fixes, full targets continue

The compiler does not support completion `RENAME_NOREPLACE` The Linux file system backup path and clear system error diagnosis. Added attribution tracking to avoid deleting existing temporary files after creation fails. New Addition 8 Group Real Compiler Regression and 7 Group C++ operation model, final CTest for Windows **12/12**, Windows ASan **12/12**, Linux **14/14**. The mount disk runs separately **8/8**. The symbolic link groups in Windows and the original Linux-exclusive groups are explicitly skipped. Source/Bytecode Three-Dimensional Real-Time Running **12 Secondary startup, 612 Sub-response verification, 123 A sealed document**, For functional observation only.

The first round of C++11 compilation failed, Linux linking had dependencies missing, and the real red examples from the old compiler were retained. The 153 implementations in `src/js/include` remain unchanged; the version is still 0.2.2. The complete 0.3.0 version is highly compatible, with QPS, a clean ALL/complete matrix, and formal three-way/2 GiB and four-view review are still pending. The historical 1762 complete results have not been updated; there are no new performance points, Root submissions, pushes, or releases.

Full scope, platform skipping, original evidence and next steps are as follows: [Round 91 of Handoff](next-steps-handoff.md#capsid-review-phase-91) Audit entry: `build-v030-dev/evidence/native-asan-qualification-20261018/upgrade45-final-audit.json`.

<!-- capsid-review-phase-90 -->
<a id="capsid-review-phase-90"></a>
## Round 90 Status (2026-10-20): Cold controller/archiving enabled, full target continues

The cold start of the public entrance has unified the C-ABI source code/bytecode, the process and port ownership of Node/Deno/workerd, precise response, fixed members/stable recursive PSS, exclusive fixtures/native temporary compilation, CPU-bound recovery, discard rounds and measurement round archives, as well as real verifiers. The 15 actual red examples that were missed by the old verifier for discard/process/warm evidence have been corrected. The original failures and the source code before correction are retained. The formal mode requires a complete protocol and actual clean build/session metadata; the observation mode cannot be published.

Final tools CTest: Windows standard 8/8, Windows ASan 8/8, Linux 10/10 (`upgrade44h-*`). Added Python group 21, all Linux tests passed, Windows tests now 10 passed/11 skipped for Linux; total of 67 groups of Windows scripts, 46 passed/21 skipped. Linux has 74 Python groups and existing 10 C++ models passed. No product compilation or new native sanitizer/leak qualification tests in this round.

The final real cold smoke coverage covered five paths/three dimensions, with 30 startup attempts and 1530 responses, verifying 318 files (`upgrade44i-*`). There was only one measurement round, which could not replace the official five rounds. For the four types of real SUTs, a total of 8 cleanup passes were achieved for SIGINT/SIGTERM (`upgrade44j-*`). The density entrance regression involved 12 probes/4 pool units, with 174 files verified (`upgrade44k-*`). The synthetic complete archive was only tested in the temporary repository and was not written to data.js. The site displays warm values according to the new HTTP protocol, while the old points remain unverified historical observations. The visual four-view review is not yet completed.

**The full Linux/Windows 0.3.0 strong compatibility has not yet been completed.** QPS contracts, compiler-mounted file systems/error diagnosis, actual three-lane sessions with 2 GiB density, broader body/resource pressure, default Windows Debug stack failures, new ALL/Release/LTO/packages/full matrix, and platform and dynamic verification still need further development. 4495 inputs, version 0.2.2, 58 patches, Root HEAD and `src/js/include` with 153 implementation inputs remain unchanged; no submissions/pushes/releases have been made.

For the complete implementation, evidence, and subsequent sequence, please refer to [Round 90 Handoff Instructions](next-steps-handoff.md#capsid-review-phase-90). Audit Entrance `build-v030-dev/evidence/native-asan-qualification-20261018/upgrade44-final-audit.json`.

<!-- capsid-review-phase-89 -->
<a id="capsid-review-phase-89"></a>
## Round 89 Status (2026-10-20): The cold probe has been fixed, and the entire lane with strongly compatible targets is not yet completed.

The cold start C-ABI tool has been modified to use public worker PID, strictly verifying the status/request ID/complete text. Failures include warm failures, timeouts, process exit/recovery, PID replacement, and unreadable/zero/instable PSS. The worker's subtree is fixed after the first response before warm; the cold start timer ends before warm/PSS. Interruption will cause the worker to be destroyed and recovered through C-ABI. Old error success lines and real counterexamples left behind by interruptions have been preserved (`upgrade43d-*`), and independent zero-tick analysis of counterexamples has also been corrected and incorporated into the final model.

Final actual tool CTest: Windows standard 6/6, Windows ASan 6/6, Linux 8/8 (`upgrade43k-*`); added 10 cross-platform C++ models and 7 Linux real process regression tests. Additionally, 6 strict smoke tests and 306 response verification tests were conducted on source code/traffic bytes of 10k/100k/1m, with results archived in `upgrade43p-real-smoke.json`. These are not formal cold lane or new workerd performance comparisons. New compiled measurements/models and Linux bytecode compiler; the existing host/worker, runtime libraries, and 153 implementations in `src/js/include` remained unchanged, with no new product/leakage/unfiltered matrix qualifications.

**The complete goal is still in progress.** Cold shell: The control layer still needs to have unified ownership, ports, and file contracts. The QPS has not been connected yet. The real three-lane session and 2 GiB density, site viewport, compatibility with broader body/resource pressures, default Windows Debug stack failure, new ALL/Release/LTO /The package and the original plan for platform and dynamic verification are still not completed./. 4489 Input, Version 0.2.2, 58 Patches, Root HEAD remain unchanged. No submission/push/release was made, and no new performance points were generated. The old matrix and the number of skips maintain their historical nature.

Full fixes, evidence, and next steps are available. [Explanation of the 89th Handoff](next-steps-handoff.md#capsid-review-phase-89). Audit Entrance `build-v030-dev/evidence/native-asan-qualification-20261018/upgrade43-final-audit.json`.

<!-- capsid-review-phase-88 -->
<a id="capsid-review-phase-88"></a>
## Round 88 Status (2026-10-20): Density control and archive producer are connected

The density entry has been changed to Python controller: managed/strict, port lease, listening ownership, PID/start time, recursive positive PSS and stable readings, fixed process members after READY, worker-by-worker coverage, controlled recycling, final verification and cap lower limit. Formal operation requires actual build/session metadata, and restricted observations are clearly not publishable; fixtures/runners are first taken a snapshot and reviewed, without covering shared APPS/already existing OUTs. Start interruptions, OUT race conditions, and worker recycling have led to the omission of three groups of true red-to-green evidence, which has been saved; after correction, the tool CTest/actual entry has been re-executed.

The actual tool CTest has a score of **5/5** for Windows standard configurations, **5/5** for ASan configurations, and **6/6** for Linux configurations. The registration count is **542/543/768**. There are **31 new Python groups**: all passed on Linux, 21 passed on Windows, and 10 Linux-specific groups skipped. Additionally, the original 15 groups of evidence verification were rerun. No new product compilations, native sanitizers, or complete required gates were added. The final real entry was completed with 12 probes, 4 observation units in small pools, and 173 sealed files. After actual SIGINT/SIGTERM events, no residual processes or release units were left behind. The official full unit files were only verified using temporary synthetic probes and not as performance data.

**The QPS/cold, actual 2 GiB, and complete sampling within the same session for compatibility with the entire 0.3.0 version for the main working tree and merge have not yet been completed for T15.** The new confirmed cold tool deduces the worker PID from the parent-side socketpair's SO_PEERCRED. The real-machine mechanism verifies that the return is created by the creator, not the child process. The public `capsid_worker_pid()` function already exists, and priority should be given to correcting and verifying the worker subtree, response content, warm failures, and stable PSS. No changes have been made to the cold tool this round.

Root 4486 input, version 0.2.2, patch 58. During runtime, 153 implementations showed no changes in input values; no commits/pushes/releases were performed. Finally, the overall state remains at compat27b. **1762:1733 passed, 6 failed, 23 skipped**. The old Windows Debug encoding stream still causes failures and should be retained. For detailed information regarding the actual range, sources, and next steps, please refer to… [Round 88 Handoff Instructions](next-steps-handoff.md#capsid-review-phase-88) The entry point is `build-v030-dev/evidence/native-asan-qualification-20261018/upgrade42-final-audit.json`. The old rounds listed below are historical and do not represent the status of remaining tasks.

<!-- capsid-review-phase-87 -->
<a id="capsid-review-phase-87"></a>
## Round 87 Status (2026-10-20): Corrected performance collection and web evidence boundaries

The official collection entry for T16 has been separated from the loose parser: it is required to have a fully sealed lane manifest, original samples, correctness, and isolation records, and the SHA should be recalculated. Duplicate/missing rows, abnormal values, cross-lane sources/CPU/kernels/binary conflicts, discrepancies in summaries, late writes are all rejected. The identity and evidence of density are no longer ignored. Site temporary storage will replay and archive verified points, old points are marked as unverified, the cap is clearly defined as the lower limit, and the memory description limits the sampling window.

The final tool CTest has been tested on Windows standard **2/2**, Windows ASan configuration **2/2**, and Linux **3/3**. It runs **15 Python test suites** per setup; the registration count is **539/540/765**. This does not involve new product compilation or additional native sanitizer qualifications. The actual old configuration is expected to reject failures; 43 historical files/data.js remain unchanged, and synthetic contract fixtures are not considered as performance data. Browsers are unavailable, and the four-level visual qualification is still pending completion.

**T15: Producer integration, real performance measurement, full Linux/Windows 0.3.0 compatibility, and release qualification are ongoing.** Input from Root 4478, package 0.2.2, patch 58, no submission/push/release; no new runs of ALL, full required gates, or filtering matrix this round. The original runtime and Node gate continuity have been verified, and the final result remains compat27b **1762:1733 passed, 6 failed, 23 skipped**. The old Windows Debug encoding stream failures are retained.

For boundaries, test evidence, and subsequent execution order, see [Explanation of the 87th Handoff](next-steps-handoff.md#capsid-review-phase-87) The input file is `build-v030-dev/evidence/native-asan-qualification-20261018/upgrade41-final-audit.json`. The file contract can be found in `bench/perf-evidence.md`. The old rounds listed below are historical; do not consider the T16 input as representing the completion of the entire T15/T16 or the entire version.

<!-- capsid-review-phase-86 -->
<a id="capsid-review-phase-86"></a>
## Round 86 Status (2026-10-20): Completed controlled build comparison for Windows stack pressure

Windows currently and in the 84th round, the Debug worker has reproduced 5 instances of RangeError under a stack of 1 MiB in JS. Explicitly, after 2/4 MiB, all six items pass. Linux has passed under all test constraints. The independent clean **Release** snapshots of the same product code under default/explicit **1 MiB** have passed all six items, and have completed **17/17** of the selected streams/WPTs. In comparison with the same driver, bundle, and JS quota, both executable files have a native stack reservation of 8 MiB. This supports the construction of stack pressure judgments related to configuration; the specific reasons for the Debug stack frames are still unknown. By default, if the Debug fails, it will be retained as a failure, and the product's default quota has not been changed.

The test instances for the fixed Windows/Linux workerd are set to 6/6. The module globally disables timers, so the execution context adaptation, the original bundle, and the adapted module SHA are clearly recorded without any modifications. The clean snapshot required by the Release guard contains the 85th round of **4474** identical project inputs. The local snapshot is submitted as `a0f873026414e52ee5eb79f5bd33e948a382787d`. The Root HEAD remains unchanged, and historical frozen artifacts are retained. The build only covers the worker/integration and selected stream tests. The LTO is closed, and it cannot be considered as an ALL, complete required gate, full matrix, or release qualification.

Chapter one 85 The results of this product remain valid: Newly added 30 The true reference for these items, along with the seven sets of product combinations, have been fully verified, Native regression **21/21, 21/21, 22/22** Actual required gate value **61/61, 61/61, 77/77** Two ends each have 16 Node modules **6173/6173**. This round is related to diagnostics. The overall process of 0.3.0 is still ongoing; Package version: 0.2.2; Patch number: 58. The final complete matrix remains compat27b **1762: 1733 approved, 6 failed, 23 skipped** Maintaining historical classification.

For detailed methods, adaptation boundaries, failure retention, and subsequent matters such as T15/T16, please refer to [Round 86 Handoff Instructions](next-steps-handoff.md#capsid-review-phase-86) The evidence for this round is located at `build-v030-dev/evidence/native-asan-qualification-20261018/upgrade40-final-audit.json`. The older rounds are listed below as historical records.

<!-- capsid-review-phase-85 -->
<a id="capsid-review-phase-85"></a>
## Round 85 Status (2026-10-20): 30 new conversion and lifecycle scenarios have been completed, achieving a dedicated closed-loop.

**upgrade39** corrected issues with premature termination of HTTP Promises/Timers in standard 101, as well as pending reads and serialization issues when the native body reader is canceled. It added a fixed **30 items**, with both real workerd and seven product running combinations achieving 30/30 results. This includes TLS IPv6, cross-protocol redirection, cancellations while holding the body/response headers/ClientHello, and final connection termination. The original ClientHello/HTTP/WebSocket bytes, SNI, certificate SAN, nonce, number of connections, and checks for the same VM and termination are all subjected to literal assertions.

Windows Normal/ASan, Linux's complete required gate is **61/61, 61/61, 77/77** The native-oriented CTest is **21/21, 21/21, 22/22**. Both ends 16 Each of the Node modules **6173/6173**, Added a new contract module 968 Items. Registration **538/539/764**, Required **133/133/154**. The native exit test has been changed to waiting for and asserting a normal exit. The AG build has actually rerun the native test. The complete gate follows the same AC execution as the byte/input, proving the correlation with continuity. The entire group is not rerun.

Linux operating systems for streaming/WPT 17/17; Windows, both standard version and ASan version 16 Approved. 1 Failure (WPT Ontology) 15/16 Additionally, there is verification from other sources. The encoding stream is converted accordingly. 5 The RangeError was also encountered in the previous iteration of the worker being saved. It remains unresolved. complete/allPassed It’s false. **Overall 0.3.0 Strong compatibility and delivery processes are still in progress.** Package version 0.2.2 Patches 58 Enter the input. 4474 Not submitted/pushed/published. All items are frozen or not filtered at all. In the end, all items are included. compat27b It remains the same. **1762: 1733 Approved. 6 Failure, 23 Skip ahead** It cannot be rewritten using special results.

Evidence file: `build-v030-dev/evidence/native-asan-qualification-20261018/upgrade39-final-audit.json`. For details on corrections, testing procedures, 67 binary files, 203 input archives, and remaining tasks, please refer to the relevant documents. [Round 85 Handoff Instructions](next-steps-handoff.md#capsid-review-phase-85) The old rounds below are historical, and we no longer consider the 30 closed items as unimplemented. Broader compatibility such as BYOB/direct constructor readers/tee, old Windows stream failures, complete matrices, and delivery gate still need to continue.

<!-- capsid-review-phase-84 -->
<a id="capsid-review-phase-84"></a>
## Round 84 Status (2026-10-20): The addition of the fetch boundary has been completed, achieving a closed-loop project.

**, upgrade38,** – New additions. TLS/SNI, actual implementations. IPv6 – Redirection, response body errors. Request/Response – Conversion mechanisms. **, 22 – Specific scenarios.** – Dual real workerd instances, seven groups of current products. **, 22/22,** – Windows standard/ASan, Linux versions. Required gates for full functionality: **, 57/57, 57/57, 72/72,**. Targeted CTest versions: **, 21/21, 21/21, 22/22,**. Two ends each have 15 node verification modules. **, 5169/5169,** – Details about these modules. Registered items: **, 534/535/759,**. Required items: **, 130/130/150,**. Unused registered quantities were not used to replace products during execution.

The comparator checks the original HTTP/complete WebSocket bytes, each hop's nonce, certificates/SAN, number of requests, errors after prefix, and the source endpoint before the same VM and service stopped, according to the literal semantics. New boundaries include 640 items for single modules, 559 items for platform single modules, and 202 items for archived gate modules; the combined `workerd_ws_gate_controls` CTest amounts to 456 items. The same error must be rejected at both ends. Neither the irrelevant redirect values nor the getters of the Response should be observed. The initial draft's shared initialization regression has been corrected. The true reverse results of the old products and the initial draft, along with incomplete state collection and failed verification records, are retained.

**Overall 0.3.0 Strong compatibility and release are still incomplete.** Package Version 0.2.2, Patches 58, Input 4466 Not submitted/pushed/released, not run again for freeze ALL or without filtering the full matrix. Ultimately, it remains compatable with compat27b **1762: 1733 Pass 6 Failure, 23 Skip**. Evidence Entrance `build-v030-dev/evidence/native-asan-qualification-20261018/upgrade38-final-audit.json`. For detailed corrections, verification of boundaries, and remaining tasks, please refer to [Round 84 Handoff Instructions](next-steps-handoff.md#capsid-review-phase-84) The old rounds below are historical, and it should not be repeated that the gap in this round has not yet been achieved.

<!-- capsid-review-phase-83 -->
<a id="capsid-review-phase-83"></a>
## Round 83 Execution Status (2026-10-20)

upgrade37 has been connected to the fetched 101 Response native bidirectional proxy, correcting the original HTTP application lifespan, legitimate URL/RequestInfo conversion, and the incorrect order of cross-request accept. The formal fixation of 22 items is **22/22** across both ends of the workerd and seven product groups; the previous Y prototype was **21/22** complete on both ends, with the only failure being this incorrect order. The complete required gates are **53/53, 53/53, 67/67**, with **19/19** for each of the three underlying systems, **6/6** for basic C/C++, and **2/2** for ASan; 14 Node modules on both ends total **4494/4494**. Windows fault sweep and additional Debug iterator checks are required, with coverage reported separately.

The current Root **4458** inputs, package **0.2.2**, patch **58**, this round includes 20 implementation/test inputs and eight result documents. The exact source code, 181 runtime-related inputs, 57 binaries, and original reports can be found in `upgrade37-final-audit.json` and the corresponding directory archive. **Overall Linux/Windows compatibility at version 0.3.0 is strong, but the release is not yet completed.**

Next, we will supplement the mixed resource diagrams, reentrancy, allocation failures, and cross-pressure issues, as well as the error boundaries related to TLS/SNI/IPv6/redirect/body. Then, we will freeze three inputs and execute ALL on the full matrix without filtering. The original platform/standards/dependencies, sanitizer, actual CI, T15/T16 defects, and real performance/T17 will continue to be retained. The previous complete compat27b **1762:1733 passed, 6 failed, 23 skipped** has not been rewritten. The complete execution boundaries, evidence, and sequence are as follows: [Current Handover](next-steps-handoff.md#capsid-review-phase-83) The following text is from round 82 and earlier; there were no Root submissions, pushes, or releases.

<!-- capsid-review-phase-82 -->
<a id="capsid-review-phase-82"></a>
## Round 82 Status (2026-10-20): managed updated to retain local connection source, fixed Windows incremental cache

Current Candidate **upgrade36** Fix an issue where the managed hot update prematurely reclaims old generations when the transmission is disconnected and the local send direction is still open. New Optional Capabilities **128**, v4 Frame **28** And additional C events **19**, maintaining the original ABI structure. The old negotiation results that did not request this capability remain at their original values. The Host determines drain based on the unified survival values of physical channels and reserved sources. The original **10 seconds** The deadline remains in effect. Linux Added Realistic Managed Four Scenarios **4/4**. Complete collection of the original product from the old upgrade35 **4 Items, Only 2 Item Qualification** Accurate detection of updates/ premature retirement recycling. This special verification project covers the deployment lifecycle and does not claim that workerd provides the same deployment interface.

Windows ordinary/ASan, Linux current orientation CTest **17/17, 17/17, 18/18**; complete workerd required gates **49/49, 49/49, 62/62**, no failures/skips; existing lifecycle group products total **19/19** each. Both ends have **3932/3932** modules for 13 nodes, with the lifecycle comparator still at **686** items. Total platform registrations **525/526/749**, required registrations **122/122/141**; registration verification itself does not count as product execution. The constructor's `unrequested-subprotocol` differences are still kept separately; passing the complete gate does not mean all semantic differences have been resolved.

The first real failure of Windows was due to the zero number of header files dependencies of old objects: simply correcting the new compilation prefix was not sufficient to update the existing cache. Now, by revising the compiler/prefix, such individual objects within the build directory are removed at once, and recompilation is performed to obtain dependencies. For C/C++ regression tests, the old prefixes are retained, and cache migration, subsequent recompilation of header files, and repeated configuration are verified. Each of the two main caches migrates **368** objects; each independent new directory has **6/6** objects, and key objects have **35/34** dependencies recorded. The main engineering race conditions are recovered using normal/ASan methods. Failure JUnit, old objects/binaries, and collector CRLF parsing errors are retained, with no changes to the old original results.

**: Overall status. 0.3.0: Strong compatibility issues exist, and further development is still needed.**: Fetch response issues; native agent coupling, extensive resource management/concurrency challenges. New fetch methods required for TLS/IPv6/: Handling read errors, processing all frozen entries without filtering out any data. The remaining candidates are **, compat27b,**. For **and 1762: 1733 has been approved; 6 failed; 23 was skipped.** remains pending. The root package version remains at **, 0.2.2,**. The current evidence comes from `upgrade36-final-audit.json`. For more details, please refer to…[Round 82 Handoff Instructions](next-steps-handoff.md#capsid-review-phase-82) The following records from round 81 and earlier are historical.

<!-- capsid-review-phase-81 -->
<a id="capsid-review-phase-81"></a>
## Round 81 Status (2026-10-20): The delay in local closure of the gap has been resolved

The current **upgrade35** separates the handling of EOF in inbound transmission from the local send direction. This has resolved the issue of premature cancellation of events when there are no child connections. The formal lifecycle consists of **19 items**, with **19/19** for both ends of the actual workerd and seven groups of products; the complete required gates are **49/49, 49/49, 62/62**. Each of the 13 Node modules on both ends has **3908/3908**, with **686** items under lifecycle control. The native orientation is **15/15, 15/15, 14/14**, and the ASan audit is **2/2**. The original 30-second control set limit is retained.

The native regression now includes restrictions on retained sources, delayed closures, microtask emptying, timer cessation, reuse, and normal process exit. The first three failures of the old "EOF cancels timer" assertion are supported by independent workerd probes on both ends: the old cancel fixture is now explicitly local close, the original side-effect-free assertion is retained, and delayed closures have strict positive coverage. Failure logs and inputs have not been deleted. The old product has fully collected **19 items, with semantics 18/19**, and only the newly added sub-connection delayed closures have failed.

**Overall strong compatibility has not been completed.** Managed generation remains, broader context/resources graph, native Response proxy, TLS/IPv6, freezing ALL, and full qualification continue to be pending. The previous full result **1762: 1733 passed, 6 failed, 23 skipped** is still retained, with the Root package version at **0.2.2**. Finalize `upgrade35-final-audit.json` for details. [Explanation of the 81st Handoff](next-steps-handoff.md#capsid-review-phase-81) The following text is from an older iteration and is considered historical.

<!-- capsid-review-phase-80 -->
<a id="capsid-review-phase-80"></a>
## Round 80 Status (2026-10-20): Exceptional parent connection pending events and local Close

**upgrade34** fixed premature termination of a pending same-origin event when its parent connection disconnected abnormally. It also fixed close() leaving the local send direction unchanged after an inbound paired endpoint disconnected. Constructor/fetched endpoints retained their independently observed behavior. The formal lifetime suite had **17 cases**. Both real workerd references and all seven product configurations passed **17/17**. The complete required gates passed **49/49, 49/49, 62/62**. On each platform the 13 Node modules passed **3816/3816**, including **594** lifetime comparator cases. Targeted native results were **15/15, 15/15, 14/14**, and the ASan audit passed **2/2**. The original 30-second limit and negative controls remain. The first count assertion failure and its precise correction were archived separately.

**Overall strong compatibility has not been completed.** The delayed close() probes for new additions and normal cleanup are **4/4** on both ends, and **3/4** on single-workers on both ends. Wait events without sub-connections were canceled prematurely, see `upgrade35b-gap-audit.json`. It is not included in the official 17 items or gate pass scores; this item is prepared for the next step. HTTP/accept/contextual cross-communication/large resource graphs, native proxies, TLS/IPv6, freezing ALL, and full acceptance continue to be pending. The previous full version **1762: 1733 passed, 6 failed, 23 skipped** is still retained, with the Root package version **0.2.2**. Finalize `upgrade34-final-audit.json`, see details. [Explanation of the 80th Handoff](next-steps-handoff.md#capsid-review-phase-80) The following text is from an older iteration and is considered historical.

<!-- capsid-review-phase-79 -->
<a id="capsid-review-phase-79"></a>
## Round 79 Status (2026-10-20): Shared I/O Lifecycle and Complete Qualified Gate

The current **upgrade33** has corrected the shared I/O source lifecycle in Upgrade fetch according to the real workerd: Sub-sockets that have been accepted can maintain the same-origin fetch/timers, and uncompleted resources are only reclaimed when the last upgraded connection ends. The previous scheme of reclaiming resources immediately upon connection creation **9/10** has been replaced by implementations that are **12/12** for both ends of this round and seven product groups. New contract checks include original requests/nonces, frames, Close ACKs, EOFs, state continuation points, and static points; old failures and inputs that are not covered are also updated.

The actual complete required gates are **49/49 (Windows standard), 49/49 (Windows ASan), 62/62 (Linux standard users)**, with no failures or skips. Currently, there are **3653/3653** instances for each of the 13 Node modules. The targeted CTest results are **15/15, 15/15, 14/14**. The platform requires registrations of **121, 121, 139**. The 30-second timeout for the Windows control set was resolved by reducing fixture re-construction and temporary file overhead. All negative cases and complete verifications were retained, with no extended limits. Both the first failure and subsequent actual complete gates were preserved. Any existing standard/contract deviations are still clearly present.

**Overall strong compatibility and release are still incomplete.** Homogeneous events that are still awaited in case of exceptions, broader HTTP/accept/cross-context boundaries, native proxies for fetched Responses, fetch TLS/IPv6/backpressure, and new freezes of ALL/non-filtering full sets are still pending completion. The previous complete candidate **compat27b** has **1762: 1733 passed, 6 failed, 23 skipped**, which will continue to be retained. The package version remains **0.2.2**. The current entry is `upgrade33-final-audit.json`. For more details, see [Round 79 Handoff Instructions](next-steps-handoff.md#capsid-review-phase-79) The following text refers to previous rounds, which are historical. In particular, the assumption of 'recycling connections based on creation' from the previous round has been corrected in this round.

<!-- capsid-review-phase-78 -->
<a id="capsid-review-phase-78"></a>
## Round 78 Status (2026-10-20): 13 basic scenarios for the public Upgrade fetch have passed

The current work candidate **upgrade32** has been connected to the public fetch → Worker → Host upgrade/non-101 body link. There are two modes for Windows ordinary/ASan, and three modes for Linux. **7 groups have 13/13 instances** each. There are **32/32** resources for the ordinary fetch; the constructor has **179 executions per 36 classes**, still retaining the existing `unrequested-subprotocol` differences. The main project is directed to CTest **15/15, 15/15, 14/14**, with platform registrations at **118/118, 118/118, 135/135**; in reality, 12 Node modules have **3206/3206** instances on both ends. New C/C++ header file modifications trigger automatic recompilation in positive/negative regression tests, and MSVC dependency prefixes are fixed in the project CMake. Early failures and all original reports are retained.

**The overall strong compatibility/release is still not completed.** The parent connection of the new fetch is recycled, and the response is forwarded directly for upgrade. This link TLS/IPv6 /Expanding boundaries and three freeze phases - complete gate/complete matrix still to be completed/. First Orientation 64 It has been agreed that the term "unannounced/unconnected" from the historical context below cannot be continued. The previous complete candidate is **compat27b**, Original full set **1762: 1733 Pass 6 Failure, 23 Skip** Continue to retain. Root package version is **0.2.2**. For detailed implementation, precise evidence scope, and further steps, please refer to [Round 78 Handoff Instructions](next-steps-handoff.md#capsid-review-phase-78). Final Entry Point `upgrade32-final-audit.json`. The following old iteration is historical.

<!-- capsid-review-phase-77 -->
<a id="capsid-review-phase-77"></a>
## Current progress of round 77 (2026-10-20): The base of Upgrade fetch has been verified, but the public link is still to be connected.

The Root working candidate **upgrade29a** has been supplemented with optional capabilities 64, HTTP metadata codec/Registry, non-101 body direction and credit, and ABI v7 adds `capsid_worker_dial_accept_http()`, as well as 13 scenarios for formal drivers/comparators. The actual workerd on both ends is **13/13**; the old compat27b product has **1 passed / 12 failed**, which is not a pass for new products. The first-party Host/Worker has not yet declared 64, and network links such as JS fetch/Worker/Host, body/cancellation/proxy/TLS, etc., still need to be implemented.

For the main project, three sets of candidates have been selected: CTest, **, 6/6,**. Additionally, C11/C++, 11 have been selected. All components have undergone rigorous inspection: 6/6. At both ends, 12 groups of Node controllers are used to control **, 3, 206/3, 206,**. The actual number of registrations includes Windows **, 519,**; ASan **, 520,**; Linux **, 742,**. Platform-specific registrations must be verified: **, 118/118, 118/118, 135/135,**. The complete list of required tests has been expanded to include Windows 45 and Linux 57. Registration verification and synthetic testing cannot replace actual product operation. This round of testing does not involve any new candidates for ALL, complete product tests, or overall registration results.

The complete implementation sequence, evidence, and limitations can be found in [Explanation of the 77th Handoff](next-steps-handoff.md#capsid-review-phase-77) Evidence entry points: `upgrade31j-observations.json`, `upgrade31g-protocol-*`, `upgrade31f-units-*`, `upgrade31m-main-*`, and `upgrade31n-main-*`. Early configuration/fixture failures and reference Close race condition events are retained; precise exceptions for references are not applicable to the product. The last complete candidate is still **compat27b**, with full **1,762: 1,733 passed, 6 failed, 23 skipped**; other old tasks were not closed in this round. The Root HEAD / package version remains **`8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`** / **0.2.2**. Overall strong compatibility and release are incomplete. The following old rounds are historical.

<!-- capsid-review-phase-76 -->
<a id="capsid-review-phase-76"></a>
## Round 76 Current Progress (2026-10-19): Collection of Built-in Methods, Fetch Request Headers, and Current Full Regression Tests

Current candidate **compat27b**: 4,432 byte inputs, 58 patches, three sets of strict ALL; special cases **70/70, 71/71, 76/76** and native workerd gate **41/41, 41/41, 52/52** all failed/skipped with zero failures. The formal encryption contract **42 scenarios** has closed the differences between the two sets of prototypes from the previous round; **fetch 32 scenarios** verified non-default ports of Host, precise default/caller request headers, and actual accepted sockets, with the original bytes retaining duplicate headers. There are **2,789/2,789** validators for each of the 11 groups on both ends; selected projects TSan **63/63, 0 reports, 123 C++ input** projects, still not fully dependent on plug-ins.

All registered tests for the current iteration have been executed: Windows **513 (501 passed/2 failed/10 skipped)**, ASan **514 (502/2/10)**, Linux **735 (730/2/3)**; total **1,762: 1,733 passed, 6 failed, 23 skipped**, no tests excluded. Six standard WPT failures have been re-evaluated based on actual reference/product observations, matching the default workerd; original failures and old expectations are retained. All 13 new items added compared to the previous version have passed, with no new failures; this entire set belongs to compat27b. Overall strong compatibility with 0.3.0 and the release process is still in progress.

23 instances of skipping categorized by item (13 unique tests): 14 instances of skipping on the Windows platform for Linux-specific probes, 4 instances of gaps in Windows native testing fixtures, 2 instances of performance prerequisites, 2 instances of Linux general user privileges, and 1 instance of cgroup controller prerequisites. Additional permission runs for the same product **2 passed/1 skipped (cpu.max missing)**. Strict gate testing failed, with exit1 and the original full skip retained. Details can be found in `compat27b-current-skip-audit.json`, `compat27b-linux-qualified-root.json`, and `compat27b-linux-root-prerequisite-review.json`.

There are additional newly discovered read-only issues that are not part of this round of 32 scenarios: Under strict server conditions, both references for the Upgrade fetch return 101 and `response.webSocket`. In the current ordinary/ASan/Linux versions, all seven actual product modes return 400 without any socket issues, and all nine instances complete successfully, as seen in `upgrade28c-review.json`. For the original deadline under more relaxed server conditions and normal exit scenarios, see `upgrade28b-review.json`. This issue has not yet been resolved; first, we need to formalize the contract and implement the corresponding channel/HTTP metadata path, then proceed with the Windows test setup and the remaining tasks.

For complete fixes, evidence of negative results before the fix, and the remaining order, please refer to [Round 76 Handoff Instructions](next-steps-handoff.md#capsid-review-phase-76) The main evidence includes `compat27b-verification.json`, `compat27b-final-audit.json`, `compat27b-current-full-matrix-audited.json`, and `compat27b-current-full-profile-audit.json`. 325 actual inputs and 23 source code differences have been archived. The Root HEAD / version remains `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f` / 0.2.2. Only eight result documents have been updated after verification. Further work is required to handle 23 items, including skipped ones, other crypto/fetch/asynchronous boundaries, D004/D005, complete instrumentation/clean rebuild/actual hosting CI, as well as T15/T16/T17. The following old rounds are historical.

<!-- capsid-review-phase-75 -->
<a id="capsid-review-phase-75"></a>
## Current progress of round 75 (2026-10-19): WebCrypto digest/HMAC and full regression closed loop

**Post-Validation Only Supplement:** `brand27a-review.json` has identified two discrepancies in the dual-end actual crypto26c and dual-end reference reproduction: the replacement of prototype methods in the global WeakMap/WeakSet will affect the product's key state reading and recipient determination. These two discrepancies are not counted towards the pass in the current round of 40 scenarios. In the next round, we will first fix the related built-in methods and formalize the contract, then perform three full sets of tests on the merged current source code. The original product, reference, inputs, and final evidence have all been preserved, and no post-validation modifications have been made to the implementation or formal assertions.


Current candidate **crypto26c**: 4,430 byte inputs, 56 patches, three sets of strict ALL builds passed; special cases **70/70, 71/71, 76/76**, native platform gate controls **41/41, 41/41, 52/52**, all with zero failures/skips. The formal digest/HMAC contract has been added to **40 scenarios**, with both ends referencing the positive control and the rejection control observed in the old products retained. 11 modules, **2,704/2,704** each on both ends; selected project TSan **63/63, 0 reports, 123 C++ compilation inputs**, still not fully dependent on instrumentation; the actual Windows ASan commands for the seven key native C source codes have been verified.

Precedent http25a, latest three sets of unfiltered full data **1,749 items: 1,720 passed, 6 failed, 23 skipped**, compared to the previous round which eliminated 15 failed combinations without any new failures. Each of the six standard WPT failures was matched against the actual default behavior of the workerd on each side. The original failures and old expectations were not rewritten. This result belongs to http25a; **The 1,762 items of current crypto26c still need to be executed**, and overall strong compatibility with version 0.3.0 is not yet completed.

The results and subsequent order are as follows: [Round 75 Handoff Instructions](next-steps-handoff.md#capsid-review-phase-75) This round of source code differences includes 22 files, with 321 actual input archives. The core evidence consists of `crypto26c-verification.json`, `crypto26c-final-audit.json`, `http25a-current-full-matrix-audited.json`, and `http25a-current-full-profile-audit.json`. Records of execution by the same user with abnormal local Linux startup entries are retained. The Root HEAD / version remains `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f` / 0.2.2. Only eight result documents have been updated after this round of verification. The previous rounds are considered historical.

# v0.3.0 GLM Implementation Review and Handoff Record (2026-10-13)

<!-- capsid-review-phase-74 -->
<a id="capsid-review-phase-74"></a>
## Round 74 Current Progress (2026-10-19): Timeout abort bridge resumes request context

The following **http25a** has been accepted: three sets of strict ALL passed, special items **67, 68, 72**, mandatory gate tests **37/37, 37/37, 47/47**, selected project TSan **59/59, 0 reports** (123 projects with C++ input, not complete dependency plug-ins), and **2,003/2,003** for each of the ten contract modules.

Fixed the issue where the deadline abort bridge did not enter the corresponding request, resulting in the proprietary timer being unable to clean up. The cooperative deadline, Hono/h3/Elysia, and itty-router have all passed the special tests for their lifecycle management, as well as the original continuation/request context protection. The original results of io24d, consisting of three sets of unfiltered **1,749 items, 1,705 passes, 21 failures, and 23 skips**, are retained. The current source code has not rerun the complete matrix, so these results are not rewritten as a full pass. Two items, WPT, WebCrypto, fetch headers, additional resource graphs, CI, and T15/T16/T17 are still pending completion.

**0.3.0: Overall compatibility is still satisfactory, but the release acceptance process is not yet completed.** Package version: 0.2.2. For detailed implementation details, product index information, and next steps, please refer to the relevant documents. [Round 74 Record](next-steps-handoff.md#capsid-review-phase-74) The following content in round 73 and earlier is historical records.

<!-- capsid-review-phase-73 -->
<a id="capsid-review-phase-73"></a>
## Round 73 Current Progress (2026-10-19): Fetch Caller Context Recovery

The current qualification for **io24d** has been completed: three sets of bytecode source code have passed the strict ALL build. The special CTest items **60, 61, 65** have been executed, and the platform gate tests **37/37, 37/37, 47/47** have been run. The selected project TSan **52/52, 0 reports** (117 projects with C++ input, not a complete dependency plug-in), with **2,003/2,003** of each of the ten contract/gate modules.

Patch 0055 Fixed the issue where the ownership of the current request is cleared after synchronous fetch. Retained the priority of the Binding resource owner. Shared resource scenarios have been increased to **31 Items**: hostname/numeric Both actual paths verified the successful sending of messages before calling, after returning, and during asynchronous callbacks, as well as the rejection of cross-connection sending. Three native owners control coverage of DNS, denial paths, forward network, callback, and reference balancing. Host Parsing and Completing Retention `.invalid` The asynchronous negative result of the domain. Native control must be run to verify that there are no DNS packets and that regular domain names are genuine A/AAAA, NXDOMAIN, Cancel and Timeout. End-of-life products from both ends 29 Accepted, two new behaviors RED, io24b's unit test counting omission, io24c's ASan gate DNS differences, and negative control of compiling the old DNS behavior separately are all documented.

**0.3.0: Overall compatibility is satisfactory, but the release acceptance process is still incomplete.** The next steps prioritize WebCrypto-related tasks, as well as handling issues related to fetch Host/default headers, resource management, concurrency optimization, and final volume processing. T15/T16/T17-related tasks remain pending. The package version remains at 0.2.2. For full implementation, verification, and handover procedures, please refer to…[Round 73 Record](next-steps-handoff.md#capsid-review-phase-73) The following content in round 72 and earlier is historical records.

<!-- capsid-review-phase-72 -->
<a id="capsid-review-phase-72"></a>
## Current progress of round 72 (2026-10-19): fetch resource recycling and cancellation semantics

Current candidate **resource23g**: Three sets of strict ALL builds with the same input size have passed; Windows Release/ASan, Linux Release/LTO must undergo gate testing **37/37, 37/37, 47/47**, with special CTest items **55, 56, 60**. Project TSan **47/47, 0 reports** (116 projects with C++ input, incomplete dependency instrumentation); ten contract/gate modules with **1,923/1,923** each on both ends.

Added 29 scenarios for shared resources and native DNS/handshake/GC/reference control. Fixed issues such as native fetch recycling after connection termination, cancellation reason snapshots, synchronization abort checkpoints, empty path Cookie cache blocking, and rule conflicts in multi-port configurations on the same host. The old cross22g had 24 RED behaviors and 2 configuration startup failures on both ends, which are retained separately. The order of encryption operations, fetch Host/default headers, additional resources/concurrency diagrams, and the final full dataset are still to be completed. T15/T16/T17 cannot be selected as a whole.

**0.3.0: Overall compatibility is still limited, and the release acceptance process is not yet completed.** The package version remains at 0.2.2. For details on the complete implementation, any failure logs, product indexes, and next steps, please refer to the relevant documents. [Round 72 Handoff](next-steps-handoff.md#capsid-review-phase-72) The following content in round 71 and earlier is historical records.

<!-- capsid-review-phase-71 -->
<a id="capsid-review-phase-71"></a>
## Current progress of Round 71 (2026-10-19): Cross-Context Promise

The current candidate **cross22g** has passed the strict ALL build with three sets of input data of the same byte length. The complete platform gatefolds for Windows Release/ASan and Linux Release/LTO are **33/33, 33/33, 42/42** respectively. There are **40, 41, 43** items for the dedicated CTest. The project TSan has **39/39** (114 projects with C++ input, not all of which rely on instrumentation).

This round of corrections did not address the misuse of the 'reject' caller context, premature execution of cross-connection callbacks, continued execution after the creator's end, as well as the order of checks for module-level Promises and closed connections. The old async21c had 14/26 failures on the same final input; the intermediate failures and corrections from the prototype to the new native tests are retained.

**0.3.0: Overall compatibility is still limited, and the release acceptance process is not yet completed.** The package version remains at 0.2.2. For details on the complete implementation, original failures, resulting indexes, and subsequent tasks, please refer to…[Round 71 Handoff](next-steps-handoff.md#capsid-review-phase-71) The following content in round 70 and earlier is historical records.

<!-- capsid-review-phase-70 -->
<a id="capsid-review-phase-70"></a>
## Current progress of round 70 (2026-10-19): Asynchronous callbacks and execution budget

The current candidate **async21c** has passed the strict ALL build with three sets of input bytes. The platform gatefolds for Windows Release, Windows ASan, and Linux Release/LTO are **29/29, 29/29, 37/37** respectively. The special CTest results are **36, 37, 39 items**, with zero failures and zero skips. The project TSan has **31/31** (110 projects with C++ input, still not a complete dependency plug-in).

This round of corrections addressed issues such as misstopping timers in applications, incorrect error reports for microtasks, loss of connection context due to refusal notifications, delayed synchronization dispatch with catch statements, and reentering after timeouts. The old product actually failed on the same final 24 inputs; the 28/29 of the intermediate async20r were not qualified due to two timers competing during testing. The failed results were retained, and the final inputs were queued causally according to catch completion, with no assertions being relaxed.

**Linux/Windows 0.3.0: The overall acceptance testing is still incomplete.** The version remains at 0.2.2. The complete CTest/root configuration using the handler15b cannot be considered as a successful candidate. For details on the complete implementation, original failures, resulting files, and subsequent tasks, please refer to…[Round 70 Handover](next-steps-handoff.md#capsid-review-phase-70) The following content in round 69 and earlier is historical records.

<!-- capsid-review-phase-69 -->
<a id="capsid-review-phase-69"></a>
## Current progress of round 69 (2026-10-19): Connecting timers with Promise lifecycle

The current candidate is **owner19c**. Three frozen source code files contain **4,399 byte-related project files**, and three sets of strict ALL builds have passed. The Root HEAD remains at `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, and the package version is still **0.2.2**. This round of modifications involves 16 implementations, build locks, and test inputs compared to the previous candidate. The C++ project is `src/worker_runtime.cc`. Additionally, eight documents from the previous round of results have been included in the frozen input. The patch stack has increased from 45 to **46**, with the addition of `0046-connection-async-lifetime.patch`. **The connection lifecycle initiative has been verified; overall compatibility with Linux/Windows 0.3.0 is strong, and the release qualification is still pending.**

The main differences exposed by the front-wheel timer diagnosis have been incorporated into the implementation and must-run tests: The WebSocket callback is completed without ending the ownership of the connection resources; one-time, periodic, and Promise chain timers for open connections can continue to execute. A normal Close operation retains the timer while waiting for an ACK from the other party, and the timer is canceled after both parties close or terminate abnormally; another HTTP request's clearTimeout does not cancel the timer for that connection, and the next event for the same connection can cancel it. After closing, the Promise no longer executes its callback, even if it is resolved by another request, including callbacks that only modify the normal JS state without calling native interfaces.

Recycling occurs after the current microtask checkpoint, and the order of queued microtasks and valid error events remains intact. Error events use separate internal cleanup tasks to prevent application timers from canceling the erroneous dispatch of C18. Native callbacks retain valid context references, avoiding invalid contexts left behind after timers are cleaned up in callbacks. Contexts that have been revoked but still referenced by promises remain lazy, releasing on reference and no longer occupying active connection registrations. The recycling linked list does not allocate additional nodes on the cancellation path. Execution entry still uses a limited deadline; extreme CPU/microtask load and broader issues related to callback errors have not yet been qualified.

Initial owner19b Complete Windows Gatekeeping Guarantee **21 Pass / 2 Failure**: When the outbound WebSocket reaches EOF, the original HTTP request is still waiting for the close callback 15ms Timer. The timer will be deleted if it is recycled by connection only. The current owner19c checks the generation and survival status of the original HTTP token, retains the outbound callback context for requests that are still pending, and reclaims resources after the response is terminated. Inbound Upgrade still uses the connection lifecycle. No original outbound assertions were removed. Failure reports and the actual product are stored in `owner19b-failure-audit.json` And `owner19b-outbound-failed-products/`.

Review of additional addition to `allowHalfOpen`: When a peer Close is received, the closing event can wait for the timer, then send the final data and complete the sending direction. It cannot be mistaken for the end of the entire connection due to the first Close. This item, together with the previous 16 items, constitutes the current **17 shared lifecycle use cases**, all of which establish independent contracts based on the actual running results of two official x64 workerd assets, dated `2026-08-20`, with compatible flags `[]`.

| Current owner19c verification | Special Project CTest (including four vendor reviews) | Complete platform reference: Gate |
| --- | ---: | ---: |
| Windows Release | 30/30 | 23/23 |
| Windows ASan | 31/31 | 23/23 |
| Linux Release/LTO | 32/32 | 29/29 |

The required access control system is currently set to Windows. **23 Items/Things**, Linux **29 Items/Things** Including all supported backends. 17 The item owner is controlled by independent contracts. The test verifies the authenticity of the Upgrade challenge, the integrity of the original frames, TCP communication with the peer side, the number of times measured before and after closing, the complete sequence of events/microtasks, and the HTTP behavior within the same realm. 200 As well as actual cooperation and withdrawal procedures. The report includes information on fixtures, drivers, contracts, clients, and more. Host/Worker SHA; missing items, mutual tampering from both ends, falsification of results, partial socket disposal, forced termination of processes – all of these cannot be considered valid methods of passing the test. There are two copies of the data that need to be verified. 18c The product is in the same new state. fixture/driver/contract Actual RED: `live-one-shot` Only “queued” messages were received. Subsequently… `WORKER POISONED: terminal continuation leak` HTTP request failed; please keep the original report and diagnostic information for later reference.

The original shared exception sets consist of **80 categories/161 items**. There are **162 individual boundaries** related to products, with **3,009 API assertions** and **36 fixture variants** remaining valid. The original 120-second variant threshold has not been reduced. Seven contract/report testing modules have been completed: **479/479**. The current GCC Debug project’s TSan configuration has been expanded to **20/20**. There are no reports, no failures, and no skips. Additionally, **105 C++ compilation inputs and 21 selected build targets** have been reviewed. Three backend mechanisms for real-world owners and native Worker WebSocket functionality have been added. However, this still falls within the scope of C++ instrumentation in the project; it cannot be considered a complete implementation of TSan. The HTTP cancel, timeout, suspension reactions, and timer management mechanisms related to `worker_terminal_continuation` remain strictly negative-controlled and continue to be implemented.

The actual running output, bootstrap, compiler, identity, build files, as well as the QuickJS/TJS C source files, header files, and four C compilation unit objects affected in this round have been verified and archived. The number of archived files is recorded in the `owner19c-verification.json` file. The Root relative frozen input was updated with eight result documents after the qualification process was completed. The running implementation and assertions were not modified after verification. No commit/push/release was performed for the root repository.

The next steps prioritize verifying timer callback errors, CPU/microtask execution budget reentrance, and addressing the recycling of non-timer asynchronous resources at connection termination, as well as concurrent owners and cross-connection boundaries. The 17 current use cases cannot replace these broader scenarios. Subsequently, there is still work to be done on native error aliases, Date/Map/Set/RegExp/ArrayBuffer/typed array/Web API wrapped objects, mixed graphs and stack/accessor edge behaviors; capacity exhaustion, backpressure, ACK writing, cross-closure, cancellation/stop combinations; D004 clock and D005 HTML; general MSVC automatic dependency capture, complete dependency instrumentation/fuzz/soak, Windows/Linux hosted, and byte-by-byte construction in independent new directories. Finally, it is necessary to rerun the complete ordinary/ASan, profile, and qualified root on the stable final source code.

The most recent complete CTest and qualified root results are still from the history **handler15b**: Windows 485 (473 passed / 2 failed / 10 skipped), ASan 486 (474/2/10), Linux 701 (696/2/3), totaling 1,672 items: 1,643 passed, 6 failed, 23 skipped. The original exit codes were all 8. The standard WPT's `support_promise_rejection_events_js` and `MessagePort_initial_disabled_any_js` were retained in all three sets; the workerd profile classification cannot replace standard compliance. The historical root of 3/3 is not counted as a current pass.

Evidence directory: `build-v030-dev/evidence/native-asan-qualification-20261018/`. Current entries include `owner19c-verification.json`, `owner19c-final-audit.json`, `owner19c-gate-controller.json`, three sets of `owner19c-*-focused*` / `owner19c-*-reference-gate/`, and `tsan-owner19c-result.json`. Independent references and old product RED are represented by `owner19b-canonical-{win32,linux}-{reference,old18c-red}.json`. The `qualificationPassed=false` in the previous `handler19b-owner-lifecycle-diagnostics-v2.json` was a fix for previous historical diagnostics, and has not been changed.

> The content of the 68th round and earlier is historical snapshots; the current conclusions are based on the 69th round.

<!-- capsid-review-phase-68 -->
<a id="capsid-review-phase-68"></a>
## Current progress of round 68 (2026-10-19): Native exception reconstruction and built-in string conversion

The current candidate is **handler18c**, and the three frozen source codes contain **4,393 byte-per-item files**. All three sets of strict ALL builds have passed, with the Root HEAD remaining at `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`. The package version is still **0.2.2**. This round of changes involves 16 implementations, build locks, and test files, with no modifications to the project's C++ source files. After adding `0045-event-string-intrinsic.patch`, 45 patches were applied in sequence on clean copies. The overlay key/manifest, actual bootstrap, build identities, and run products were verified and archived separately. **This special project has been verified, and the overall compatibility with Linux/Windows 0.3.0 is strong. However, the release qualification is still incomplete.**

The front wheels still have four basic entries: `name-getter-throws`, `dom-exception`, `aggregate-error`, and `override-string`. These have been entered into the current regression test. DOMException determines its true type through a native getter, using internal names, messages, and code to reconstruct it. Overloaded properties with the same name do not replace internal fields, and their getter side effects still occur in the tested order. AggregateError retains its native type and errors/cause data. Error's non-enumerable fields are reconstructed as enumerable data properties. First, all its own values are read, then it is recursively cloned. The symbolic getter is executed, while the symbolic keys are discarded. Loops and aliases for ordinary objects/arrays continue to be retained. Inputs that cannot be cloned, such as loops, function properties, etc., that directly return to the native Error, are backed up by reference. Sparse arrays and function backends that are directly thrown away have also been verified.

When the getter continuously makes mistakes, a new 24-bit base32 diagnostic number and real-time log are generated. The numbers for error events, error messages, stack, and object graphs must be consistent, while the C17/C18 numbers must be different. The comparator first strictly verifies these fields, the number of getter calls, trusted distribution, microtask order, real line end, and HTTP recovery, and then only compares and normalizes the numbers. The original report remains unchanged. When the global String is replaced, the Event/WebSocket uses built-in conversion functions for capturing.

| Current 18c Verification | Special Project CTest (including four vendor reviews) | Complete platform reference: Gate |
| --- | ---: | ---: |
| Windows Release | 28/28 | 19/19 |
| Windows ASan | 29/29 | 19/19 |
| Linux Release/LTO | 30/30 | 24/24 |

The issues related to the anchors and import relocations of the 18a variant for the gate have been corrected. The 18b Windows gate has been approved, and the ASan variant group retains the original 120-second timeout. Currently, the 18c variant continues to use the same 120-second threshold: the complete baseline and all conventional backends maintain 161 regression anomalies. Each fault variant uses an isolated program bound to the target assertion, and the target cases and actual program SHA are recorded and verified. The regular runner does not have environment variable switches omitted for the cases. All 35 fault variants and precise diagnostics continue to meet all requirements.

The current shared exception set consists of **80 classes × C17/C18 + C18 error listener re-throwing errors = 161 items**. Two official x64 workerd assets each passed **3,009 API assertions**. The old 17a products on both ends actually failed at the specified assertions in the new `c17/dom-exception`. Product independent line tests executed 12 original line cases and **162 exception cases** per backend, including a single column of C17 error listener re-throwing resource boundaries; client cleanup is not counted as an end-of-file on the other side. Comparator/report control was **251/251**. The actual fixture failure set consisted of **36 items** (baseline + 35 variants that must hit specified diagnostics), including five new categories: DOM private names, Aggregate aliases, getter order, residual original stack, and diagnostic field disconnection. There were no failures or skips in the special cases and gate controls. Product requirements for exit 0, no signals, no forced stops; reference stops were retained according to the platform, with Windows SIGTERM/Linux exit 0.

The same-origin GCC Debug project TSan **15/15**, with zero reports, zero failures, zero skips. **104 C++ compilation inputs and 20 selected build targets were verified**. This remains within the scope of the C++ instrumentation project. The complete dependency on instrumentation has not yet been qualified. The existing `worker_terminal_continuation` features negative control for cancellation, timeout, Promise reaction, and separation of HTTP timers; the queue limit, recycling window, timeout, Hono 500ms cold request budget, and function stack configuration remain in accordance with the established contract.

The 25ms timer issue has been addressed by adding a first set of paired probes: each platform runs 8 items on the previous 18b and the official reference, totaling **32 original diagnostics**, with consistent states across both ends of the reference. After an abnormal termination of the connection, the reference no longer runs these timers, and the HTTP status remains 200; the product has timers that continue to run or cause a poison effect, resulting in an HTTP 503 status. The deadline for normal closes without a response to ACK is kept separately, not counted as a successful EOF. The original evidence is `handler19a-owner-diagnostics-v2.json` and its four reports. **This discrepancy has not been fixed yet**; next, we will address the normal closes with ACK, the legitimate timers that are still open, and the strictly HTTP-separated timers with negative controls. We will then correct for connection/event resource ownership issues, and not expand the shared recycling window to cover up the problem.

Subsequently, 9 types of lifecycle inputs were completed. These were executed once on each of the two references on the current two platform products, resulting in **36 new diagnoses**. The states of the two references were consistent. Independent verification was conducted on the number of timers and the actual complete frames: the one-time/periodic timers for open connections were executed normally. They could continue to execute until the ACK of Close was received, or they would stop after the ACK was received or in the event of an exception termination. The HTTP normal await timers could be completed. The current product also poisons the timer for legitimate open connections, so the repair scope includes the normal connection lifecycle and should not only handle exceptional cleanup. The items related to HTTP separation timers are strictly poisoned and controlled in the negative direction, and should be retained. The evidence is the `handler19b-owner-lifecycle-diagnostics-v2.json` file, where `qualificationPassed=false` clearly indicates that no correction has been made yet. It cannot be considered a pass just by exiting the collection script with a 0 result.

The following steps will first fix the lifecycle of the 'owner' object. Subsequently, it is still necessary to complete the broader mixed object graphs (Map/Set, Date, RegExp, ArrayBuffer/typed array, Web API wrapper objects), native Error aliases, and the edge behavior of stack read/write/accessors. Capacity exhaustion, backpressure, ACK writing, cross-closure, and cancellation/stop combinations are also to be addressed. D004 clock, D005 HTML; general MSVC automatic dependency capture, complete dependency instrumentation/fuzz/soak, Windows/Linux hosted execution, and independent new directories byte-by-byte reconstruction are also required. Finally, full normal/ASan, profile, and qualified roots must be re-executed on the stable final source code. The current goal is Linux/Windows; the Mac projects in the old documentation are kept separately as alternatives to the two-platform goals.

**The most recent complete CTest and qualified root are still the historical handler15b.** Windows 485 (473/2/10), ASan 486 (474/2/10), Linux 701 (696/2/3), total 1,672 items: 1,643 passed, 6 failed, 23 skipped, with all original exit codes being 8. Two standard WPT failures, `support_promise_rejection_events_js` and `MessagePort_initial_disabled_any_js`, are retained in all three sets, and cannot be replaced by workerd profile classification for standard compliance. The root 3/3 of 15b cannot be reclassified as currently passed.

The root directory of the evidence is `build-v030-dev/evidence/native-asan-qualification-20261018/`. Current entries include `handler18c-verification.json`, `handler18c-final-audit.json`, `handler18c-gate-controller.json`, `handler18c-*-focused*`, and `handler18c-*-reference-gate/`, `tsan-handler18c-result.json`. The original source code can be found at `handler18a-shared-reference-v2-{native,linux}/reference.json`. The old product RED is represented by `handler18a-old-product-red-v2-*.json`. Only eight result documents have been updated for the Root version; the root repository commit/push/release has not been updated yet.

> The content of the 67th round and earlier is historical snapshots; the current conclusions are based on the 68th round.

<!-- capsid-review-phase-67 -->
<a id="capsid-review-phase-67"></a>
## Current progress of round 67 (2026-10-19): Cloning of ordinary exception objects and judgment of engine types

The current candidate is **handler17a**. Three frozen source codes consist of **4,392 byte project files**, and all three sets of strict ALL builds have passed. This round of modifications **included 10 implementation/test files**, where the C++ changes were only to the internal type judgment helper functions in `src/worker_runtime.cc`. The CMake and vendor patch files remain unchanged. The generated bootstrap, identity header, and actual artifacts are archived separately by hash. The Root HEAD remains at `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, and the package version is still **0.2.2**. No commit/push/release has been performed. The acceptance for this round's special project, reference gate, and project TSan has been completed; however, the overall strong compatibility and release acceptance for **0.3.0** have not been completed.

First, execute **20 types of input × C17/C18 = 40 independent tests** on two locked official x64 workerd assets and the old 16a product. Retain event records, object properties, conversion calls, actual paths, HTTP recovery, and exit logs. Keep the upstream internal error IDs when the name getter fails, and do not replace them with broad categories. Test confirmation: The old implementation calls custom string conversions for common objects, misjudges pseudo-error prototypes, loses object graphs, some inputs have no error events, and they are waiting for completion. The formal regression uses the same final fixture to execute **99 shared exception cases and 2,136 API assertions** on both official assets; both old products actually fail at the specified `c17/object` assertion.

`capsidChannelValueKind` directly uses QuickJS's Error, Proxy, Array, and object types for classification, avoiding the need to identify types through the user prototype, `Symbol.hasInstance`, or proxy traps. Ordinary objects and arrays within them are cloned into new objects using enumerable string keys: symbolic keys are ignored, getters are read once during the synchronous capture phase, loops and shared references are preserved, and output properties are ordinary writable data properties. Array holes and explicitly undefined values are separated; the literal `__proto__` is defined as a data property, and non-enumerable ordinary properties are ignored. Non-clonable own functions or proxies are fallback to a new Error based on tested inputs, without invoking the user's `toString()` / `Symbol.toPrimitive` / ownKeys traps. The original synchronous error propagation sequence, error listener I/O, C17 late frames, and C18 final Close continue to be verified.

| Current 17a Verification | Special Project CTest | Complete platform reference: Gate |
| --- | ---: | ---: |
| Windows Release | 24/24 | 19/19 |
| Native Windows ASan | 25/25 | 19/19 |
| Linux Release/LTO | 26/26 | 24/24 |

The current verification has failed with zero failures and zero skips. Keep it as it is `worker_terminal_continuation` The existing cancel, timeout, parking Promise reaction, and detached HTTP timer are used for negative control. Sharing exceptional inputs from **77 Increase to 99**, total **49 Class × Two Scenarios + C18 Error Listener Still Fails**. Independent line testing is performed on each product backend **12 Original Feature Cases + 100 Item Exception Cases**. 100 The item also includes a separate C17 error listener that rethrows the resource boundary error. Reference gate coverage Windows/ASan Two backends and three Linux backends. Local Comparator Control **213/213** Pass. The actual fixture failure set is **31 Items** (First pass through the baseline + 30 Variants that must hit specified diagnostics), adding three items: transformed miscalls, lost shared references, and retained pseudo-Error prototypes, and rejecting pseudo-consistency caused by missing new test cases from both sides or simultaneous damage to references.

GCC Debug project TSan with the same Linux source code **15/15** Zero reports, zero failures, zero skips, inspection **104 Project C++ Compilation Input**, including new type judgment helper functions. 20 The selected build targets, actual run products, and key compilation inputs have been archived. This scope remains the C++ closure for project C **Not equal to complete vendor/dependency Instrumentation Qualification**.

**Among the original eight groups of abnormal input cases, this round we closed four groups of normal object cases:** `object`, `object-primitive`, `object-tostring-throws`, `null-prototype`. Additionally, verification `object-data`, `object-cycle`, `object-getter`, `object-symbol-value`, `object-proxy`, `object-tostringtag`, `object-fake-error`. The passing of ordinary objects, arrays, and those that have been listed back into the regression input does not mean that all exceptions are structurally cloned. The current ordinary object cloning does not support containing native Errors, Map/Set, Date, RegExp, ArrayBuffer/typed broader object diagrams such as array. Native Error cause/errors, Owned properties, non-string names, loop backtracking, and stack semantics are not fully implemented. The new diagram asserts that it does not verify the stack descriptor or format generated by the engine for errors, and therefore cannot consider it as passed.

The following will continue in the following order:

1. **Exceptionally Compatible.** Continuing on with the current 17a and two official assets `name-getter-throws`, `dom-exception`, `aggregate-error`, `override-string` Four groups, as well as the real Error modifying prototype, cause/errors, Own Properties/Descriptors, Stack, Loops/Alternatives, Mixed Built-in Object Graph with Non-Clonable Field Backups. Additionally, provide type judgment for arrays/functions and Web API wrapper objects, as well as backtrack probes. This round of array validation is limited to the data diagrams within ordinary objects. Existing 40 Item-wide diagnosis only corresponds to the original 16a/reference, and cannot be directly renamed as 17a. All passed. Dynamic internal error ID retains the original text and field associations, getter counts, credible events, and true termination requirements. Do not cover lost events or unfinished connections with a single error category. Supplement abnormal identities, broader getter/listener reentrance, and cross-owner operations.
2. **Lifecycle and Resources.** The terminal-continuation/poison method with a 25ms timer is used to monitor the remaining isolated and closed processes. Probes are set based on the HTTP/WS owner, creation timing, failure types, and transmission termination, with strict HTTP negative control maintained. Continued operation is ensured under conditions such as full capacity, backpressure, ACK pending writing, cross-shard/owner closures, and cancellation/stopping scenarios. The original queue limits, recycling windows, timeouts, Hono 500ms cold request budget, and the existing 2 MiB feature stack configuration are retained.
3. **Special Projects and Deliverables.** D004 clock variation, D005 HTML, universal MSVC automatic dependency capture, complete dependency instrumentation/fuzz/soak, actual hosted/native Mac, independent new directory byte-by-byte reconstruction, and final platform delivery are still pending completion.

**The most recent complete CTest and qualified root still belong to handler15b.** The original complete set includes Windows 485 (473/2/10), ASan 486 (474/2/10), and Linux 701 (696/2/3), totaling **1,672 items: 1,643 passed, 6 failed, 23 skipped**, with all three exit codes being 8. Two standard WPT failures occurred with `support_promise_rejection_events_js` and `MessagePort_initial_disabled_any_js`, which were not resolved to a pass. The root 3/3 of 15b remains a historical record. In 17a, the complete normal/ASan/root/profile was not rerun, and the old results were not renamed as passed; reruns are required after stability is achieved. The reference stop results are retained as they were on the original platform (Windows SIGTERM, Linux exit 0, neither forced). The product host requires an exit code of 0, with no signals or forced stops. The C17 error listener continues to throw errors, which is an independent resource boundary. Client cleanup is not considered an EOF from the other side.

The evidence root directory is `build-v030-dev/evidence/native-asan-qualification-20261018/`. The current inputs include `handler17a-verification.json`, `handler17a-final-audit.json`, `handler17a-gate-controller.json`, three sets of `handler17a-*-focused*`, `handler17a-*-reference-gate/`, `tsan-handler17a-result.json`, and `handler17a-tsan-inputs.json`. The final reference evidence is **`handler17a-shared-reference-v2-{native,linux}/reference.json`**. The 99 reference records that originally did not contain array/literal proto extensions are also retained separately. The current old product RED is `handler17a-old-product-red-v2-*.json`, and the first input records are also preserved. Only eight result documents have been updated for the final Root relative frozen source code.

> The content of the 66th round and earlier is historical snapshots; the current implementation, verification scope, and next steps are based on the 67th round.

<!-- capsid-review-phase-66 -->
<a id="capsid-review-phase-66"></a>
## Current progress of round 66 (2026-10-19): I/O for exception getters and error listeners

The current candidate is **handler16a** Three frozen source code files remain **4,392 One-byte project file** Three sets of strict ALL builds have passed. This round of modifications **9 One implementation/test file**. C++, CMake and vendor patches have the same inputs as the verified 15b. Root HEAD remains `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f` The package version remains the same **0.2.2**, Not commit/push/release. **This round of special projects has completed the reference gate qualification. 0.3.0 Overall strong compatibility and release qualification are not completed.**

First, execute **11 types of I/O inputs × C17/C18 = 22 test cases** on two locked official x64 workerd assets. The event traces are consistent across Windows/Linux platforms. The old 15b product has **18 events**, frames, or transmission end differences on each platform. After extending the formal fixture, both official references passed **77 shared exception cases, 1,828 API assertions**. The two old products actually failed at the specified `c17/error-send` assertions. Original counterexamples, getter Close wait deadlines, and normal/forced exit fields were retained.

Fixing three observable behaviors: When the C17 error listener sends data synchronously, the readyState is already CLOSED, yet data is still accepted and written out; after the listener returns, the microtask that sends data still converts parameters but no longer writes out data, ignoring the close operation does not change the sending direction. The close operation within the listener rethrows the saved exception if the user's close is not called, and the original C18 closed direction continues to prevent sending. The parameter conversion of send is moved before state checking, covering the Symbol, object conversion, and error-prone conversions. Exception handling uses the existing final Close capability within the getter and in failed microtasks, ending the transmission after writing out the accepted frames, without requiring a response from the client. An accurate diagnosis of the illegal code 1006 is also included in the assertions.

| Current 16a Verification | Special Project CTest | Complete platform reference: Gate |
| --- | ---: | ---: |
| Windows Release | 24/24 | 19/19 |
| Native Windows ASan | 25/25 | 19/19 |
| Linux Release/LTO | 26/26 | 24/24 |

The current verification has failed with zero failures and zero skips. The special feature `worker_terminal_continuation` has been added, retaining the existing negative controls for cancel, timeout, Promise reaction, and separating HTTP timers. The recycling window, timeout, or Hono cold request budget is not extended, and capacity is not increased. All backends covered by the gate support are referenced: Windows/ASan with single-workers and static-pool, and Linux with managed. Independent line tests executed **12 original line cases + 78 exception cases** for each backend; the 78 cases include 77 shared inputs and a single-column C17 error listener that rethrows error resource boundaries. The local comparator achieved **179/179** pass rates; the actual fixture fault set consists of **28 cases** (passing the baseline + 27 variants that must hit specified diagnostics), with three new items added: lost synchronous writes, skipped conversions, and ignoring getter Close.

Reference the stop results and continue saving according to the actual platform: `SIGTERM` and `forced:false` for Windows, 0 and `forced:false` for Linux; the product Host requires 0 for exit, with no signals or forced stops. The C17 error listener synchronizes and then continues to throw errors, maintaining independent resource boundaries. Client cleanup is not considered an EOF from the peer side. Assertions check for precise frames, the order of getter/transformation, error states, true termination, HTTP recovery, and bounded stops. These scenarios do not prove a complete exception object clone or all reentrancy/backpressure combinations.

**The complete matrix, qualified root, and the most recent round of project TSan still belong to handler15b.** The original full list for 15b was Windows 485 (473/2/10), ASan 486 (474/2/10), Linux 701 (696/2/3), totaling 1,672 items: 1,643 passed, 6 failed, 23 skipped; all three exit codes were 8. Two criteria for WPT failures were `support_promise_rejection_events_js` and `MessagePort_initial_disabled_any_js`, which were not changed to passed. The root 3/3, project TSan 15/15, and 104 C++ compilation inputs for project 15b are also for historical reference only. In round 16a, changes were made according to JS behavior, and the full platform reference gate was followed. No reruns of all CTest, root, or TSan were conducted, so it is not possible to rename the old results as currently passed.

There are still four groups of work left, so it is not possible to provide a reliable overall completion percentage at this time:

1. **Exception Object Compatibility.** Eight groups of unqualified inputs still throw `name-getter-throws`, `object`, `object-primitive`, `object-tostring-throws`, `null-prototype`, `dom-exception`, `aggregate-error`, and `override-string`. Reuse of original diagnostics across the current product and two official assets, continue to verify native brand, cause/errors, stack, proprietary properties/descriptors, circular/shared references, and non-clonable value fallback; supplement exception identities with broader getter/listener reentrancy, and cannot claim object graph compatibility based on name/message equality.
2. **Lifecycle and resource combinations.** The terminal-continuation/poison mechanism, which is used for recovery, still has issues with timers of 25ms. This can be categorized based on the HTTP/WS owner, the timing of timer creation, the type of error, and the method of termination. This information is used to determine whether callback functions are executed, HTTP recovery processes are successful, quotas are released, and exits occur. Continued operations include scenarios where capacity is full, ACKs need to be written, cross-shard/owner interactions, cancellations/stoppages. Strict negative control mechanisms should be maintained in these situations.
3. **Standard/Profile Verification.** Additional verification for clock variations in D004 and HTML execution in D005 is performed, documenting standard failures and established workerd profiles. MSVC's automatic dependency capture stability issues have also not been resolved as announced in this round of re-linkage.
4. **Final Delivery Qualification.** After stabilization, complete normal/ASan/root/profile verification of the final source code will be performed. Complete dependencies will be tested using instrumentation/fuzz/soak. Actual hosted/native Mac environments will be utilized, and independent new directories will be reconstructed byte by byte. The delivery will be on the established platform. This phase will still involve local native verification.

The evidence directory is `build-v030-dev/evidence/native-asan-qualification-20261018/`. The current sources include `handler16a-verification.json`, `handler16a-final-audit.json`, `handler16a-gate-controller.json`, three sets of `handler16a-*-focused*`, `handler16a-*-reference-gate/`, two sets of `handler16a-shared-reference-*`, `handler16a-preparation.json`, and `handler16a-runtime-inputs-*.json`. The source code and actual run/compile inputs have been archived by hash. Only the results for this round, consisting of eight documents, are updated.

> The content of the 65th round and earlier is historical snapshots; the current implementation, verification scope, and next steps are based on the 66th round.

<!-- capsid-review-phase-65 -->
<a id="capsid-review-phase-65"></a>
## Current progress of Round 65 (2026-10-19): Exceptional snapshot sequence, built-in functions, and strict regression tests

The current candidate is **handler15b** Three copies of the frozen source code are included. **4, 392 File containing items and bytes** Three sets of strict ALL configurations have been approved. The situation remains relatively stable this round. 14i The source code/testing files have been updated to **9 One file** The inputs for C++, CMake, and vendor patches remain unchanged in size. The Root HEAD remains the same. `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f` The package version remains the same. **0.2.2**; Not yet commit/push/release. **The review of evidence for this round is complete. 0.3.0 Overall compatibility is still intact, but the release acceptance process is not yet completed.**

| Complete CTest Matrix | Registration/Execution | Pass | Failure | Condition skipping | Original exit code |
| --- | ---: | ---: | ---: | ---: | ---: |
| Windows Release | 485 | 473 | 2 | 10 | 8 |
| Native Windows ASan | 486 | 474 | 2 | 10 | 8 |
| Linux Release/LTO | 701 | 696 | 2 | 3 | 8 |

None of the three complete matrices were selected or excluded for testing; the original exit codes, failures, skips, JUnit, LastTest, and registration lists were retained. The standard WPT's `support_promise_rejection_events_js` and `MessagePort_initial_disabled_any_js` still failed; the classification of the default workerd profile did not change the standard results. This round of special tests covered Windows **23/23**, native ASan **24/24**, and Linux **25/25**. The gate values were **19/19, 19/19, 24/24**, with zero failures and zero skips. The qualified root for the same Linux products, **3/3**, and the project TSan, **15/15**, passed. There were no reports of TSan failures or skips, with **104 C++ compilation inputs** checked; this does not equal a complete dependency sanitizer qualification.

In this round, we first recorded a broader input on two locked official x64 workerd assets, and then retained clear counterexamples using the old product. The old 14i products for Windows/Linux failed at the custom error name assertion; the intermediate candidate 15a passed through three builds, but its getter was executed after microtasks, at readyState 3. Both official references were captured during the synchronization capture phase, reading at C17 state 1/C18 state 2. The results, inputs, failures, and unfinished states of 15a were listed separately and not counted as a current pass.

`js/websocket.js`: Currently saves error names and messages during synchronous capture, and distributes trusted error events after microtasks are completed. Custom names are reconstructed into new Error objects, and known names use corresponding built-in types. For non-string or read failures, `message` uses empty text. The internal error timer uses the native function for capturing, and `Object.hasOwn` and formatting functions also preserve the original references. Queued for transmission, C18 has accepted Close, error events in CLOSED state, and the original transmission boundaries continue to accept precise byte validation.

The shared error inputs have been expanded from **23 to 55**, covering **27 types of inputs × two scenarios**, and the C18 error listener is used to synchronize and then throw errors. New custom names, message getters/error getters, Symbol/BigInt, built-in functions have been rewritten, error listeners that return a rejected Promise have been added, as well as snapshot cases where microtasks modify the original exception. Two actual official references execute **1,520 API assertions**; the comparator checks the number of times the getter is called, the state, and the sequence of events, as well as whether the conversion function is not called, the cloned type, and the exact frame. **164/164** items are controlled by the local comparator; the actual fixture failure set consists of **25 items**, including the originally passed original baseline and **24 items** that must meet the specified criteria for diagnosis.

Independent line order tests for each backend retain **12 original line order test cases + 56 incorrect test cases**; two backends for Windows/ASan and three backends for Linux. Among the 56 cases, 55 share inputs and 1 is dedicated to resource boundaries for C17, and client-side termination is not considered as an end-of-file for the other side. After complete qualification, only the log count at the end of the document was corrected from `24 → 56`. The summary was changed to take data from the actual observed list, and the original assertions were executed and checked against the 56 cases. **7 backends** of the same batch of products have replayed the corrected independent scripts, while the original full logs and old scripts are still retained. The final Root relative freeze of the source code only resulted in eight result documents and this one output count change, with no changes to the implementation or assertions.

The reference process stops and is recorded accordingly by platform: Windows is recorded as `SIGTERM`, `forced:false`; Linux is recorded as exiting at 0, `forced:false`. The product host requires exiting at 0, with no signals or forced stops. The C17 error listener synchronization error still hinders normal stoppage in upstream Linux, and continues to act as an independent resource boundary, not merging into the shared reference for normal exit. The original EOF/reset and client scheduling traces are retained, and it is not declared that all transmission schedules are consistent.

**There are still clear gaps in exceptional compatibility.** Passing this round does not represent a complete Error-structured cloning; `cause`, `errors`, proprietary attributes, and object diagrams are not fully supported by the current 55 items. There are eight groups of inputs that require verification/correction: `name-getter-throws`, `object`, `object-primitive`, `object-tostring-throws`, `null-prototype`, `dom-exception`, `aggregate-error`, `override-string`. The results of stringifying and cloning ordinary objects must be observed separately; DOMException's event messages and types, AggregateError's members, and backports for unknown error names all require complete assertions. The original diagnosis also includes a 25ms timer for terminal-continuation/poison observation; the current fixture restores built-in functions within the error listener, isolating only this input. **This does not close this independent lifecycle gap.**

The following process will continue in the following order. For each item, two official assets and actual counterexamples from the old product will be retained, and then the implementation will be corrected:

1. The remaining eight sets of inputs are reused from `handler15a-probe-fixture-v1.mjs` / `v2.mjs` and the two platform original reports. New independent checks are added for Error/DOMException/native brand, non-string name/message, property descriptors, cause/errors/recursive references, and non-clonable properties. Improvements are made to send/close/reentrancy within getters, as well as I/O operations within error listeners and after event completion. The dynamic upstream internal error IDs are retained as is, as they cannot be obscured by a broad error category to cover up lost events or unfinished connections. Continued expansion of Promise/global built-in replacement inputs.
2. A single combination of recovery timer and error termination/owner recycling, checking HTTP recovery, actual quota release, and bounded stopping. `worker_terminal_continuation` The Promise reaction that requires cancellation, timeout, and parking, as well as the detached HTTP timer, have triggered the specified teardown. It is necessary to retain these negative controls, clearly define the lifecycle boundaries of WebSocket events and HTTP requests. Do not globally cancel the garbage collection check or extend the window period. using channel races, owned I/O, Static pool and Supervisor fixture expansion capacity full, ACK pending writing, cross-closure, owner exit, combination of cancel and stop. Do not increase the queue limit, timeout, or Hono due to failures 500ms Cold Request Budget.
3. Supplement D004 for clock variation and D005 for HTML executable verification. Standard results and default workerd profile are recorded separately. Single-machine clock observations, project TSan, or individual special tasks are not considered as conclusive evidence.
4. After the final implementation is stable, the complete dependencies include instrumentation, fuzz/soak, actual hosted/native Mac delivery, and a separate new directory for byte-by-byte reconstruction. This round is for local native verification, and the hosted CI has not been declared as executed. The version 0.2.2 will remain in use until the release qualification is completed.

This round still retains the build failures caused by single Windows link files and the retry records for strict ALL from the same origin. The old source snapshot names remaining in the auxiliary script were rejected by consistency checks, and only after correction was the actual CTest initiated. The preparation failures were not rewritten as passes. The CLI compatible IDs of the two Windows compilers are consistent with the generated headers; all automatic dependencies currently recorded contain this header, and the general MSVC capture stability does not claim to be resolved based on this.

Evidence directory: `build-v030-dev/evidence/native-asan-qualification-20261018/`. The current indexes are `handler15b-verification.json`, `handler15b-final-audit.json`, `handler15b-resumed-workflow.json`, three sets of `handler15b-*-full*`, three `handler15b-*-reference-gate/`, `tsan-handler15b-result.json`, `profile-audit-handler15b.json`, `handler15b-code-identity.json`, and `handler15b-report-replays-*.json`. More detailed original diagnostics, 15a sequence counterexamples, and preparation failures are retained.

> The content of the 64th round and earlier is historical snapshots; the current implementation, verification scope, and unfinished items are subject to the 65th round.

> Historical progress snapshot: Fixed false passes in the write timeout scenario. Added configurations for disabling certain behaviors, as well as four items related to write timeout/drain/stop operations in the complete pool. The source code remains unchanged on both platforms. In both platforms, there were three rounds of testing each, resulting in 21/21 failures for both types of products. All errors were detected by corresponding assertions. On Windows, the results were **318:306/0/12**; on Linux, it was **607:602/2/3** (passed/failed/skipped). For workerd, the results were **22/22**; for the same binary root, it was **3/3**; for TSan, it was **51/51**. No reports were generated, and no items were skipped. For more details, please refer to…[Reviewer Section 50](takeover-review.md#section-50) Runtime exit diagnosis, local 503 pending completion of kernel writing and other matrices remain open. Overall, 0.3.0 is not completed; old records are understood according to their respective snapshots.

> Historical progress snapshot: Local data has been completed. 503 There are three real TCP issues related to delayed session writes, as well as complete dual-worker pools. stop/drain Two items are pending recovery from the read head; the source code remains in its original state. 48 Just like before, both platforms have three wheels each. 15/15 All three types of errors have been detected by the corresponding assertions. Full Windows scan completed. **314 Items: 302/0/12** Linux full version **603 Items: 597/3/3** (Passed/Failed/Skipped; Failed due to HR-time issues in this round); Worker **22/22** Same as binary root. **3/3** TSan Special Project **44/44 Zero reports, zero skips.**.hr-time is set as a fixed value. 20 The diagnosis is as follows: 19 Through /1 Failure occurred. The failure window corresponds to the clock changes. All failed devices have not been replaced yet. It is necessary to recheck the stable clock environment before proceeding further. For more details, please refer to…[Reviewer Section 49](takeover-review.md#section-49) The complete pool is still to be written up, and the matrix with other resources/platforms remains open. 0.3.0 is not fully completed yet. The following old records are understood according to their respective snapshots.

> Current progress: New submissions are rejected after the begin_drain response is received for shared external input/output connections. Queued owner callbacks also undergo checks during session creation. In five direct API scenarios across two platforms, three tests failed, while two passed. For the same test subjects, each candidate underwent three rounds of testing, with results of 15/15 successes. Three types of errors were identified through specified assertions. On Windows, there were **309 issues: 297/0/12**; on Linux, there were **598 issues: 593/2/3** (passed/failed/skipped). On the worker side, the results were **22/22**; for the same binary root, the results were **3/3**; for TSan tests, the results were **39/39**, with zero reports and zero skips. The new test setup uses two direct API workers, and the full pool matrix continues to be tested. For more details, please refer to…[Reviewer Section 48](takeover-review.md#section-48)0.3.0 Overall unfinished, the following old records are understood as snapshots.

The initial review involved the implementation plan for `f1496d3`, along with the 7 implementations of GLM that follow. The latest version is
`8028b88`. At that time, the workspace was clean. Initial conclusion: There is already a basic framework for ABI/protocol/configuration settings, but a usable WebSocket data path has not
yet been established. Originally, it was planned to include T00–T08, allowing for early acceptance of the results.
This document records the rectifications and remaining gaps; the following historical stages will be updated in the subsequent handoff sections.
The current development tree has achieved the basic functionality of real inbound for three types of hosts and outbound for User ws/wss.
For the latest complete pool failure allocation fixes and verifications, see Section 45; for the cancellation of the fetch handshake, see Section 44. Earlier records can be understood according to their respective snapshots.
Continue to read [Next AI will take over the task.](next-steps-handoff.md) Section 35 is for fixing the previous red-test history.
For details on packaging processes, reproducing input data, and completing necessary tests during Windows CI phase, please refer to Section 34. For details on testing the authenticity of native Windows environments, please refer to Section 33. Information regarding multi-worker identities and correcting errors in contracts can be found in Section 32. Details on preparing independent clients can be found in
Section 31. Information on response construction and the 101 submission phase can be found in Section 30. Details on the lifecycle of HTTP owners can be found in Section 29.
Information on handling exceptional EOF situations can be found in Section 28.
Details on WPT and clock-related evidence can be found in Section
27. Details on packaging and release processes can be found in Section 26. Historical evidence related to testing effectiveness can be found in
Section 23. Details regarding listener corrections, conflicts between workerd/WPT
implementations, and overall review processes can be found in Section 22. Details related to public events can be found in Section 21. Overall review of
all aspects can be found in Section 20. Details regarding the initial round of testing effectiveness can be found in Section 18. The historical verification information from previous sections remains intact.
[Rounds five to seven of the review process](rounds-5-7-review.md) Retain historical findings. For the verification sequence and commands, please refer to
[Continued explanation](continuation-handoff.md) The complete release qualification has not yet been completed.

## 1. GLM Handover and the First Two Rounds of Rectification (Historical Baseline) <a id="section-1"></a>

| Tasks | GLM Submission or Current Status | Review and Handoff Results |
| --- | --- | --- |
| T00 | Development Builds and Basic Use Cases | This round of rebuilding; Linux WSL, GNU 13.3, RelWithDebInfo, LTO off; cannot replace the release build |
| T01 | `cc4e92f` restores v7, adds spawn_ex/options | The original layout is retained for fixes; additional protocol info is placed at the small buffer boundary and caller-size event initialization. |
| T02 | `2e3a489` starts the recycling process | Linux has tests; Windows branch previously allowed direct PASS without assertions, now changed to explicitly requiring qualification; native recycling is pending completion. |
| T03 | As mentioned above, RFC codec | Buffer/zero mask/length fixes retained; ran independently with codec sanitizer; incremental frames and message layers belong to T10 |
| T04 | `5ab72bb` reference driven | Correct the release/source identity, lock the client, fix file conflicts. Actually ran 11 fixtures; complete boundaries and sequence of events are still lacking. |
| T05 | `ac19d52` A2 Contract | Revised sequence/complete head; continuation of implementation v4.2 result/terminal barrier, pending cancellation, double-end interleaving and deadline, now frozen |
| T06 | `e949ac1` registry | Repair ID/Recycle/Associate. Continue to add the worker boundary and owned IO/lease. JS roots/Independent connection token, control write pump still lacking |
| T07 | `ab14318` C ABI | Exceeding boundaries / UAF/WOULD_BLOCK; Continued processing with additional ACKs from event/kind/confirm. Real-time IPC interleaving and retry mechanisms are implemented. executor/C ++/Binding replacement not completed yet. |
| T08 | `8028b88` Host Configuration/ledger | Configuration and identities have been implemented; undercounting has been corrected; the ledger has not yet been connected to the listener/executor, and the service budget is pending wiring. |
| T09 | Unimplemented | This round has used the gate for shared access. The real HTTP regression and worker admission counts for single-worker/static-pool/managed have passed. |
| T10–T14 | Unimplemented | JS API, Real Network 101, ws/wss, owner/lifecycle, Client Diff, fuzz/soak/CI not completed |
| T15–T17 | Unimplemented | Measurement, collector/archiving/web source, and full platform production posture release qualification to be executed. |

Source Code Evidence: `src/worker_runtime.cc` still claims zero channel capability, `js/profile-manifest.js`
There is still no WebSocketPair available. The registry/ChannelIO has been added to the worker’s configuration, but it hasn’t been actually initialized yet.
Received JS connection event and network adapter. ChannelLedger still has no listener/executor caller. Cannot be present as a class file, passed unit test, or proven by checkboxes for T06/T08.
Connection established. The development of the Capsid WebSocket fixture cannot be considered as a qualification.

## 2. Defects identified in this round of fixes <a id="section-2"></a>

1. **Sequence Release to Green.** Both harnesses release the root using sequences, with the target being processed sequentially.
   Sequence reset, error in tag matching, skip subsequent steps after error report. Keep the JSON until the playback ends.
   Reset each target, compare each step; empty sequences, repeated JSON fields fail. Independent models
   credit/fragment: The status should be changed to per channel, per direction; use a full u64 key for pending dial.
2. **Frozen bytes are not accurate.** The original Upgrade vector omitted the complete v3 head, ResponseHead status.
   It is treated as u32, status text/header count is missing. There is also a REJECT length and text.
   Continuation, credit overflow, and pseudo-fragments exceeding 64 KiB. Now using the full version.
   head, matching HTTP id, valid frame size, expected correct quota; magic comments are synchronized to the actual value.
   `0x43545257` / LE `57 52 54 43`.
3. **Descriptor out of bounds.** Even after accepting a short caller_size, the field should still be filled in completely. Changed to local decoding.
   Replicate the restricted prefix, add caller to compile time to pass size for init_sized; cover every byte with a sentinel.
4. **Borrowed pointer invalidation.** The reason for closing originally pointed to a temporary std::string. Now it points to the original event.
   Payload. The C11 example is also changed to copy the text within the current pump, to avoid using old pointers after the next event.
5. **WOULD_BLOCK consumes status.** Previously, credit was deducted first, followed by the advancement phase, and then it was enqueued. Now, it is a candidate.
   The registry is validated, and only after successful queuing is submission made; 60000 bytes reject, and retrying still provides a verbatim response. Allocation
   Place an ABI guard, compress the consumed output prefixes; the request accounting also rolls back in case of failure in the queue insertion.
6. **ID Collision and Long-Term Growth.** The parking slot and channel id share the map, with the first id being 1.
   It may fail, and subsequently OPEN may overwrite the active channel. Separate pending dial/channel
   Table; final state deletion records, with monotonic high-water rejection for reuse; repeatedly closed without retaining unbounded tombstones.
7. **Upgrade not sent.** UpgradeBegin originally only registered a tail-only probe; the actual begin is not included.
   flag/tail. Currently, the off-wire reservation is retained. The actual coding of the complete head will begin later. 101: Verification of the request.
   The owner ends the HTTP accounting after the end, and the channel continues to send and receive messages; non-101 releases the reservation; cancels the unannounced reservation.
   Do not send frames with unknown IDs. Disordered announcements of small IDs are rejected before being enqueued.
8. **Closure and Syntax Boundaries.** Bi-directional half-close, mixed close/half-close, transport
   observation: Recycling; Budget determination based on negotiated message cap. Channel header HTTP id is non-zero.
   A2 OPEN with non-zero flags, illegal REJECT reason, and a normal Close reason without code.
9. **Service count underflow.** Empty releases are originally wrapped to the maximum u64 value. This is limited by the CAS operation for returning and checking.
   Overflow due to auto-increment; The data path that actually goes into the service still awaits T08.
10. **Handshake Gate Blocked/Bypass.** Normal HTTP duplicate key is rejected; duplicate Connection/
    Upgrade originally returned kNone; there is no gate for managed. Currently, first identify WS, then merge the list fields and
    Verify GET, HTTP/1.1+, Connection, key, version, subprotocol. Common 400/426
    And supported-version header; Non-101 application response is fully forwarded. Regression observation worker
    Counting the admission count to prove that bad requests did not enter the application.

## 3. Reference identities and true coverage <a id="section-3"></a>

The official tag `v1.20261010.1` refers to the value
`8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f`. This is not the original value `6dd2348…`.
Download the official `workerd-linux-64.gz` file, and verify both the compressed package and the extracted content:

- asset SHA-256: `a4530a03ab3da1e1cb0173887133a720bd48cd236135990d7cbc561c16091530`
- binary SHA-256: `50f31156a9a4c7d549f1b4753dd61eb527e1b6b0cd416e5ba3d02d64dfdee5dd`
- compatibility date: 2026-08-20; explicit flags: []
- Node: v24.14.0; bundled undici: 7.21.0

Newly added semantics fixtures allow for direct observation of worker behavior related to binaryType/datatype,
pair keys/iterators, accept/readyState, send-before-accept, view copying, and Blob text conversion. The default binaryType
value on the client side is no longer used as a basis for worker semantics. Each selected fixture must have both a driver and a non-zero assertion; incorrect identifications
or empty/duplicate selections are not allowed. Any discrepancies are reported along with the runner/fixture hashes.

File corrections for client/server endpoints, allowHalfOpen, send transformation, and messageerror conclusions.
The ping driver ended its observation window, but this does not prove that the watchdog terminated the process. The RFC allows for the merging of pong responses, as well as all
responses being processed. The complete T04 implementation still lacks proper handling of misuse/repeated transfers, as well as proper sequencing of operations like empty/reason/close. Additionally, the allowHalfOpen
true parameter needs to be properly implemented, along with proper conversion of outgoing parameters and proper timeout settings for all stages.

## 4. Current remaining execution order <a id="section-4"></a>

1. **T04/T10 Boundary Filling.** The inbound core has been implemented; complete the close code/reason/event sequence.
   Endpoint has been repeatedly transferred/overused, as well as the large message/remaining quota boundary of allowHalfOpen.
   After a normal Close, the CREDIT is still prohibited. If the last message exceeds the remaining window, it may reach the closing deadline.
   It is not possible to generalize the verified small-message half-open cases to all message sizes that are compatible.
2. **T11 Outbound Qualification.** User-origin constructor, host ws/wss dialer, immutable
   Egress snapshot, DNS actual address filtering, CA/SNI/hostname, and confirmation barrier are connected.
   Coverage includes three types of hosts, actual CIDR/default/protected deny, disorderly completion, TLS/handshake cancellation.
   Dial timeout and ACK writer stress. This round adds the ability to cancel DNS, DNS/TCP/TLS/HTTP phases.
   Cancel/timeout/stop cancels provisional ACKs, see Section 9; production is still missing.
   Actual evidence of strict syscall and native Windows/macOS.
   The WS native bridge of Binding Runtime still refuses, and does not borrow User authority.
3. **T08/T12 Lifecycle and Configuration.** Existing service slots/budget, independent HTTP/channel counts,
   generation pin and bounded drain; configurable services/listener aggregate
   The ceilings and metrics have been connected and verified, as described in Section 10. The live/pending channel retire, drain deadline, and worker crash have been addressed.
   Health recycle, replacement, and closure deadlines. Full matrix of CPU/heap, complete Managed
   The admin/Binding replacement and cross-runtime authority cannot be replaced with a shared pool driver.
4. **T13/T14/T17 Collect Evidence.** Currently using Node native client/raw TCP, locking workerd;
   Missing the second independent client, complete fuzz/TSan/soak/CI, qualified Linux production strict
   Windows/macOS native qualification. The development report, dirty tree, and exit code 77 do not meet the release criteria.
5. **, T15/T16 – Measurement and archiving.**, probe/port /Process tree/ Stabilization sampling, failed retesting, text verification…
   The collection of missing/duplicated/unknown sources/hash checks, as well as web-reversible archiving, are still pending.

## 5. First-round rectification verification scope <a id="section-5"></a>

The development build is named `build-v030-dev`. This round of reconstruction includes the original v7 consumer, ABI guard,
protocol/options, worker v4 carriage, registry/raw-peer ABI, codec, ledger,
Normalization and Host; operations are carried out based on the actual CTest registration. JUnit/tool reports are saved in
`build-v030-dev/evidence/`. These files do not serve as proof of a clean release submission.

The current protocol includes **56 frames, 30 sequences, and 168 steps**.
Negative controls cover empty sequences, duplicate fields, incorrect step expectations, damaged headers, incorrect binary/client identities, as well as errors in status/body fields.
All such cases should result in failures. In the v4 protocol, the status and detailed text of the data are now being checked.

The sanitizer involves the separate insertion of components such as client/protocol/registry, related drivers, and codecs. It also
supports objects that are not fully inserted during runtime, as well as restricted raw peers. However, this approach does not replace complete worker/Host TSan
mechanisms, JS fuzz measures, live-channel testing, or native platform validation. For more details, please refer to the evidence section.
Unchecked tasks continue as deliverables, and the release status is not changed locally through rewriting.

This round's results: targeted CTest 18/18, old HTTP/static pool regression 4/4, total of 22 items passed;
Workerd reference 11/11, 2290 assertions; 12 negative controls all rejected; ASan/UBSan
Group 3 passed. Static pool regression failed due to compatibility issues with the old test program, re-
Link accepted. Document audit verified 36 current documents and 12 modules. Evidence index is
`build-v030-dev/evidence/index.json`, records the dirty state of the development workspace and the original report summary.

## 6. Continuation of implementation: v4.2 and worker native IO <a id="section-6"></a>

For users who choose “Continue”, the work will primarily focus on T05/T06. The code remains in the development environment and is not considered a candidate for release.
The original findings and reports from the first round of evaluation remain intact. New evidence will be processed separately.
`build-v030-dev/evidence/continuation-v42/`.

### 6.1 Agreement Confirmation Barrier

- `CAP_CHANNEL_CONFIRM = 16`, dependent on DUPLEX + A2; payload of ACK type 27
  Fixed to `u32 channel_id, u64 dial_id, u32 kind`, where the three kinds are result, cancel,
  transport close. assigned CANCEL reserves 8 bytes, pending form 16 bytes.
- ACCEPT is only provisional before the worker result ACK, and no DATA is generated between the two parties. REJECT
  Keep bounded associations until the same result ACK. Results that are pending CANCEL are only cleared.
  Do not open the JS object, do not return the result ACK; return the record after the host terminal ACK.
- Opened channel to cancel or transport Close, waiting for terminal ACK; verification but no allocation/disribution
  Queued data. Both parties must confirm to cancel at the same time; duplicate, unknown, or incorrect identities will still be rejected.
- Close only terminates the sender's data/credit stream, retaining the peer's in-transit segments; CONFIRM the next two
  Half-close will not prematurely terminate the control flow. Unconfirmed expiration will not close the IPC and all associated resources will be returned.
  Do not retain tombstones that grow with completed IDs.

`test_channel_races.cc` utilizes two registries and two independent FIFO structures to handle different reading
sequences. This allows for reproducibility of the issue where “closing interleaved segments results in false rejections”. After 1000 rounds of actual REJECT/ACK
checks, the number of rejections returns to zero. Additionally, 2000 rounds of channel recycling tests are still carried out. The number of gold frames has been increased
to 69; the original 30 sequences and 168 steps continue to be executed.

### 6.2 Boundaries between Owned IO and workers

New entries such as `src/channel_io.{h,cc}` have been added. Synchronous replication is performed for send inputs; receive operations retain
their independent owned chunks and immutable origins. The budget lease persists with the owner after the message is dequeued, and is released only at the end,
preventing the situation where the count is reduced before the payload is allocated to the callback. The aggregation for both sending and receiving is independent per channel.
For receive operations, a payload of 256 bytes along with metadata is allocated per piece of data. The first piece of data receives 512 bytes of metadata. Pending send
operations allocate payload of 1024 bytes per piece of data. This mechanism involves bounded native storage
accounting, rather than precise measurement of the entire process’s RSS/JS heap.

Each piece of credit allows for the transmission of messages totaling 150000 bytes across a window of 64 KiB. Transmission allocation is handled accordingly.
The last submission is fully billed. Pieces are issued in rotation to prevent single-channel monopolization. Zero-byte messages still occupy space.
metadata quota. After cancellation, the delayed DATA is only validated, no payload/callback is reconstructed.
After the allocation failure, the receive/send/control parameters remain unchanged. The quota, credit, phase/queue values
remain as they were before the failure. All ordinary send operations are checked word by word. Any modifications made to the original view will not affect the copied data.

Workers actually engage in processes such as HELLO/READY, steady-state frame reception, monotonic ticking, and cleanup during
shutdown. Even though the capacity is still declared to be zero, real-world testing shows that setting host bitset to 31 does not trigger any new paths
for workers. This indicates that bypassing the channels injected by C ABI is possible, resulting in clear errors. At this stage, no
WebSocketPairs are available, there are no independent JS connection owners, and no actual network communication takes
place. T06/T07/T08/T10/T11 still need to be verified.

### 6.3 C ABI and Build Reliability

Added `CAPSID_EVENT_CHANNEL_ACK`, scalar ack-kind getter, explicit
`capsid_worker_channel_confirm`; Existing typed descriptor utilizes channel_id/dial_id.
No expansion. Added u64 pending cancel/ACK, word-by-word caller-size sentinel, and malformed
Scalar output invariant check. Real IPC verification involves crossing CANCEL/DATA, full writer ACK.
WOULD_BLOCK Retries, and the next dial after cancellation is not affected.

The first regression was caused by test-channel-abi not having a dependency on fake-worker-peer, resulting in the use of the old peer.
New scenario not executed; recompiled current peer succeeded, CMake has explicitly built dependencies. Old failed.
The report is for diagnostic purposes only and cannot be used as the final approval document. The final gate result and source code summary can be found in the report.
Continuing from the index.json in the directory; sanitizer is limited to the native core/client and driver.
Do not replace a complete worker/host, JS owner, TSan, live-channel soak, or native platform.

### 6.4 Continuing Verification Results

The final Linux/WSL development regression selection includes 20 items, covering core/ABI/real workers, as well as three types.
Host gate, standard HTTP, and static pool barrier/exit isolation; 20/20 pass, zero skips.
The three groups of ASan/UBSan cover aspects such as dual-end protocols, owned IO, and client ABI. Five source code mutations control mechanisms, including
those related to result barriers, Close interleaving, send billing, message size, and cancellation deadlines, have all been successfully
exploited.
The native OOM sweep actually caused failures at points 5, 8, and 2. We will verify each case by rolling back the system and retrying the
process. Additionally, we have added boundaries at 4999/5000 milliseconds to ensure that delayed ACCEPT/REJECT events do not trigger a restart of the
cancellation process. Document audits revealed issues with 36 current documents and 12 modules.

The report and the source SHA-256 hash values are included in the subsequent index. This directory retains its developmental nature; it does not claim to be a “clean” submission, nor does it
indicate that it contains complete WS/JS paths or that it has passed the cross-platform release gate checks.

## 7. Inbound WebSocket and Independent Event Owner (2026-10-13) <a id="section-7"></a>

New files have been added: `js/websocket.js`, `src/host/ws_stream.{h,cc}`,
and `src/host/websocket_session.{h,cc}`. The `examples/websocket.js` file can be used as an inbound echo mechanism.
Pair/Response 101 adopts a unique handoff mechanism; HTTP response/token recycling does not delete active connections.
`send()` synchronously copies and returns undefined. By default, it returns a blob, but can also return an arraybuffer. It is queued in advance.
Discard messages before Close. Event Promises use a separate deadline/token/roots, origin.
Record creation generation; cross-request I/O rejection. The canceled native-side effects stop immediately, limited
Promise to clean up the owner properly, without misjudging the detached work of HTTP requests as completed.

The Host first verifies the bodyless handshake, and then assigns IDs based on the order of declarations from the same executor.
101 and WS frames use the same serial write queue, retaining Beast leftovers. RFC decoder, each time
With a maximum payload of 16 KiB, it handles masks, lengths, UTF-8 across continuations.
Ping/Pong and Close. Credits are returned for accepted segments/completed network writes, with 180 KB messages measured.
Exceeds the initial 64 KiB limit. The network queue is also subject to per-channel and service 16 MiB metadata/byte budget constraints.
Limit: In-flight buffer lease is retained until callback release. Service 1024 channel slots are
Default upper limit; Non-101 means releasing the reserved channel, 101 releases the HTTP slot, and the connection is referenced by the connection pool.

Identified and corrected three types of real issues: Old bootstrap deleted CloseEvent; MessageEvent vendor
The construction signature is different from the standard init object; valid DATA is discarded after a normal Close/error in CREDIT.
Leading to poison. Additionally, the handshake cancels the passage through the barrier of queue 101, and the asynchronous message continues after cancellation.
Service HTTP, embedding NUL in the Close reason, and managing network queue leases.

Reference indicates that the close-boundary scenarios were mixed into the server’s planned Close process, resulting in timing
dependencies. After changing to a separate path, the workerd became unable to automatically handle invalid codes like 999. The Capsid still adheres to the RFC 1002
verification standards. The report clearly notes this discrepancy, as well as issues related to UTF-8 1007 and a budget denial of 4 MiB, with a required
synchronization size of 8 MiB. It’s not possible to ensure complete workerd parity by allowing all branches to be processed simultaneously.

The evidence directory is `build-v030-dev/evidence/continuation-inbound/`. Among the initial 37 options
36 items passed, the old wire assertion in the reference 'Close' failed; the failure report is retained as 'attempt-1'.
Finally, 37/37 passed, with zero skips. Three hosts executed all 13 fixtures, reference
Same, 13/13 passed. ASan/UBSan covers owned IO, bidirectional protocols, and incremental RFC decoder.
Both compileable runtime mutations (allowing bad UTF-8 to pass, discarding Close interleaved receipts) were captured.
The complete matrix of worker/host, TSan, and release platforms is not within the scope of proof for these three sets of sanitizers.
The source code and binary digest information is recorded in the index.json file. The
continuation-v42 report serves as evidence from the previous stage; it does not replace the verification process that has been modified in this update.

Finally, checking the failure ownership of QuickJS in creating ArrayBuffer/TypedArray and the submission in part 101
Handle the ownership of Promise handles to avoid OOM leaks/double release. Fixed version of C TypedArray calls
Please explicitly provide the offset/length parameters; the failed regression that requires correction is also retained as attempt-3.
Continue to use the green light for the first two stages of the system. Additionally, add an Upgrade bodyless/ended entry for the old begin_request.
Status: Ensures that both begin APIs can complete the actual 101, without requiring additional REQUEST_END.

In normal builds, OOM sweeps resulted in 5/8/2 instances for receive/send/control operations. In instrumented
builds, the numbers were 4/8/2. The test now requires that each type of failure actually occur. For each failure occurring at position 60, both rollback
measures and full payload analysis should be performed. The fixed number of allocations is not considered as part of the compliance
requirements. Logs from failed attempts using the old sanitizer mechanism are also retained.

Worker READY declaration 27 (DUPLEX/UPGRADE/A2/CONFIRM), no HALF_CLOSE declaration.
`new WebSocket(url, protocols)` remains explicitly unimplemented. There have been no changes to the version/tag, no builds have been released, and no progress has
been made in cross-platform implementation. Currently, it should be implemented in conjunction with the remaining items from section 4, rather than by duplicating the existing implementation for inbound operations.

## 8. Outbound Handoff: User ws/wss Data Path <a id="section-8"></a>

This section replaces the description of the “unimplemented outbound” status at the end of Section 7. The HEAD value remains at `8028b88`.
All uncommitted changes related to previous rounds and this round are retained. There are no changes in the ABI v7, v3 entries, or v4.2.1 version.

### 8.1 Implemented and Fixed Issues

`new WebSocket(url, protocols)` creates a WebSocket instance within an active User request/connection event
Standalone owner, assigned u64 dial ID. Basic verification of URL/protocol is performed synchronously; JavaScript is visible on the URL.
Keep the call value, remove userinfo from the network. CONNECTING prohibits send/accept, confirm the connection result.
Only distributed open; sending still syncs replication and returns undefined. Failures are handled as errors → not clean close.
1006 Distribution; OPEN: Canceled observations that have not yet been distributed are converted to assigned CANCEL, suppressing open status.

The constructor must first submit the entire OPEN request to the IPC writer, and then return it. If the size of the request exceeds the limits set by the writer or the budget for adding
elements to the queue, a RangeError will be raised. No undeclared owners will be left behind, regardless of protocol duration or host count.
As a result, ACK, pending cancel ACK, and assigned terminal ACK still follow the rules defined in v4.2. The HTTP
response terminal does not delete those Standalone actors that have been successfully submitted.

Newly Added [host dialer](../../../src/host/websocket_dialer.cc) Three hosts share an executor
Network owner. Spawn saves a deep copy of the User egress snapshot; enabled capabilities
The net policy takes precedence over the old egress descriptor. The dialing process first checks the hostname/port, and then performs filtering.
The actual sockaddr parsed by the host is always valid for explicit IP/CIDR deny; the caller modifies the original.
The policy string/rules cannot be expanded. The worker does not call the network WS implementation.

Host performs DNS, TCP, TLS, and HTTP 101 checks. Retains data after headers, prohibits automatic redirection.
Unrequested extension/subprotocol. TLS uses the default system/operator CA, peer verification,
hostname/IP checksum, DNS SNI, minimum TLS 1.2. nonce and each client frame mask
Generated by OpenSSL RAND_bytes. The shared incremental decoder extends the client role, rejecting masked
server frames, do not cache the entire frame before forwarding. The current credit/metadata/queue lease is based on actual
The socket/write callback function remains active; provisional ACCEPT does not initiate WS data IO until the result ACK is received.

When Boost is absent, the pure executor/core compilation paths are retained. It is explicitly specified that the REJECT network dialing mechanism is used. The WS/TLS
network source files and SSL dependencies follow the same rules as Boost’s data plane rules. This arrangement ensures that the portable-unit configuration remains intact.
Newly Added [Outbound Examples](../../../examples/websocket-outbound.js), The authorization still uses App fetch.allow
And the managed Host fetchTargets, default to rejection. The Binding native is still disabled.

### 8.2 Evidence and Differences

[Real outbound driving](../../../tests/test_websocket_outbound.mjs) Use independent raw TCP/TLS peers.
The same script drives Capsid in three modes and locks down workerd. Verify 180 KB bidirectional/incremental data, view
Copy, first frame and write together with 101, UTF-8 continuation/Ping, NUL close reason, error acceptance,
Not requested protocol, not 101/redirect, trusted CA, unknown CA/hostname/SNI, default deny,
DNS failure after authorization, disordered multiple dials, pending/accepted HTTP owner separation, failure after 140 rounds
Capacity restoration. The Capsid also has its own dial deadline. The report differentiates between scenario types and cases.
invocations record the actual number of assertions and the runner/binary hash, without claiming reference parity.

[worker driver](../../../tests/test_worker_outbound.cc) Rejection of DATA before the actual result ACK is covered.
Cross pending CANCEL delays, ACCEPT, 20 rounds of low-capacity recycling, and observations made when the write queue was saturated
Cancel the assigned of ACCEPT. New [Host Permission Driver](../../../tests/test_host_outbound_policy.cc)
Check the actual TCP backlog for no connections, immutable snapshots, capability precedence,
After protected/CIDR denial and stop, the service slot/bytes returns to its initial value.

Locked reference: measured three differences: returning unrequested protocols will succeed; HTTP has ended
Pending constructor is not completed, while the Capsid Standalone owner can continue; sudden disconnection
The reference in the Close callback still reports OPEN with a longer diagnostic, and the Capsid has been CLOSED.
Successful connection, failed establishment, and connected HTTP owner.
Separate common paths are verified by independent assertions. The differences are recorded. [Semantic Archive](../../reference/websocket-semantics.md),
Do not consider the separately passed branches as fully compatible.

This round of logs, JUnit, actual fixture reports, sanitizer/negative-control scope, and source code/binaries.
Summary is found in the outbound evidence index (Local artifact `build-v030-dev/evidence/continuation-outbound/index.json`).
The final result is based on the index. Failed attempts at construction/reference-driven tasks are retained, and the green light for previous round inbound requests is not reused.

### 8.3 This round has not yet been qualified (see Section 9 later)

T11: Not all items are selected together. The system’s getaddrinfo operation has been cancelled/shut down, but it’s still not possible to verify whether the OS resolver has actually
stopped working. Asio cancel function stops IO operations, but it’s not possible to determine whether the blocking OS resolver has completely ceased functioning.
Evidence of Linux strict syscall operations, as well as network paths related to Binding origin, are still pending in Windows/macOS environments.
The complete ErrorEvent object, URL/protocol conversion, and the sequence of close/error still need to be extended in the T04 matrix.
Services aggregate configuration/metrics, live generation retire/replacement, CPU/heap/OOM,
For fuzz/TSan/soak, the second client, performance/collector, and clean release of evidence, continue following the procedures outlined in Section 4.

### 8.4 Final Development Verification

Before this round of reconstruction, all relevant executables were updated, along with new workers and permission drivers. In the end, configuration using CTest **43/43** was selected, with 0 skipped cases. Three
different outbound modes were utilized, covering **24 types of scenarios/166 test cases** each. This included 140
attempts at recovery from failures, involving **1931 actual assertions**. The reference data consisted of 23 categories, with 165 instances,
totaling 1901 assertions. This includes the unique deadline associated with Capsid. The original ABI consumer, size negotiation,
three-mode inbound processing, HTTP, owned IO, and race conditions are all included in these 43 items.

The newly added network sanitizer only targets four main source files: instrument dialer, WS session,
incremental decoder, and executor. All other host/runtime/worker files remain unchanged from the regular build process. Real WS/WSS data, cancellation operations,
failure recovery, and exit processes are handled using ASan+UBSan/LSan. Additionally, independent ASan+UBSan processes are performed for server/client
codec-related functions. This scope does not constitute a complete implementation of worker/host instrumentation or TSan.
Both variants were successfully compiled and linked, but failed during execution. Issues were encountered regarding the assignment of “assigned cancel” in
the ACCEPT scenario, as well as the skipping of “resolved sockaddr deny”. The same test cases using the standard source code still passed.

In the first round of reference multi-dial scenarios, the Close reply is sent simultaneously with the TCP FIN, mixing it into the EOF race condition.
The driver has been changed to operate normally using the “Close” and “/abrupt” methods. However, the requirements regarding event handling and text processing remain unchanged. New features
include normal exit mechanisms, forced termination, and sanitizer for stderr output. The final report is generated after cleanup. The failures reported
in lines 42/43 are stored in JUnit as “attempt1”. The complete stdout report is extracted from LastTest.log, thereby
avoiding missing fields due to truncation of default success outputs by CTest. All summary information can be found in the index section of this report.


## 9. DNS and generation lifecycle handoff (2026-10-14) <a id="section-9"></a>

This round of corrections addresses two implementation issues. The getaddrinfo task of the original Asio resolver system might have been canceled.
Continue to block exit, has been changed to [AsyncResolver](../../../src/host/async_resolver.cc): DNS is composed of
The same network event loop is driving, canceling the destruction of c-ares query/socket/timer, and also suppressing those that are already queued.
Complete the callback. Prohibit address sorting detection for c-ares. Obtain the real DNS endpoint and complete the egress.
Connect only after making decisions. Up to 64 addresses can be returned; the native testing configuration does not involve IPC or JS authorization layers.

The second issue was discovered by a real active connection test: the pool enters with a predicate in the Active state
After waiting, the retire notification does not meet the predicate requirements. The quiet WS will keep the pump asleep, making it impossible to
Checking the drain deadline. Fixed. [generation pool](../../../src/host/generation_pool.cc)
Two waiting conditions, the Draining state transition itself wakes up the pump. The timeout logs from 44/45 are retained.
The repaired live/pending deadline recycling does not depend on the network peer sending events first.

Host networking has a new system dependency c-ares >= 1.27. Priority is given to static libraries. No-download is retained.
Construction. The header files are fixed to the selected provider to avoid mixing with other local Boost/conda include paths.
c-ares version; Installation instructions for Linux/macOS/Windows, synchronized with existing workflows. The Host package carries upstream
license/authors/source has detected that the standalone components have been successfully installed. System DNS/hosts is now ready.
We have switched to the c-ares semantics, and no OS NSS modules are loaded. However, end-to-end platform construction still cannot be completed using this approach.

### 9.1 This Round of Verification

Rebuild 30 actual executable targets. Oriented CTest **45/45 passed, 0 skipped**, including
The previous round included 43 items and the new ones [DNS/dialer driver](../../../tests/test_host_dns_dialer.cc),
[generation driver](../../../tests/test_host_channel_lifecycle.cc) The former is actually executed:
A/AAAA, NXDOMAIN, 64 round of cancellation/cancellation of late responses, cancellation of queuing results, no resolver thread/fd
Residual; DNS/TCP/TLS/HTTP, four stages each, Cancel/timeout/stop, a total of 12 combinations, plus
Results of the provisional ACCEPT for Cancel/stop, including two combinations and active connections. Also includes the duration of ACK/Close.
TCP stall uses Linux’s full listening backlog mechanism; the duration required for each network stop should be less than 1 second. After cancellation, the owner will be responsible for handling the situation.
Continuing to survive beyond the original deadline, it has been confirmed that there is no delay in ACCEPT/REJECT, and the slots/bytes have been returned.
The Linux connect wrapper additionally verifies that the resolver does not probe return addresses that are not authorized.

In the five scenarios involving real workers and TCP, the old generation continues to handle connection events after retirement.
The new generation starts serving new HTTP requests. For ordinary scenarios, retirement occurs after the connection is closed. Active/pending connections
are closed after reaching the drain deadline. SIGKILL signals and health recycling mechanisms are used to close old sockets. The
replacement mechanism restores HTTP services and establishes new WS connections for communication. Resources are checked for availability in each scenario before retirement occurs.

Independent ASan+UBSan instrumentation was performed on the files related to six hosts (resolver, dialer,
session, decoder, executor, pool), as well as two new drivers. Both drivers successfully terminated after passing the LSan test.
The remaining host/runtime/worker components remain in the standard build, in accordance with the third-party c-ares library. This is part of the instrumentation evidence.
This translation does not replace the full sanitizer/TSan. The two resolver variants are compiled and linked successfully before being run.
Assertion capture: Start address sorting detection, suppress queue results after cancellation of omissions. Also, handle Active waiting.
The condition has been restored to only considering the pool variant that has exited the fleet, resulting in a deadlock during the actual reproduction deadline.

Details of the original logs, failed attempts, actual JUnit results, instrumentation commands, variant diffs, and source code/binaries for
this iteration can be found in the Lifecycle Evidence Index (local file: `build-v030-dev/evidence/continuation-lifecycle/index.json`).
The path of workerd is configured during both the configure and test stages. The system checks each of the 45 test names listed in the registration form. If any
reference is missing, the registration will fail immediately. Therefore, any test cases that are not run will not be considered as passed.

### 9.2 Further implementation and verification are required

T11/T12 still require further evaluation before being accepted as complete: There are still gaps in terms of worker network syscall operations,
native Windows/macOS support, Binding bridge/authority/replacement mechanisms, managed administration systems, as well
as complete parameter settings, ErrorEvents, event sequences, and CPU/heap/OOM configurations. The service aggregate configuration and metrics for T08 are described in Section
10. For T13, the second independent client is still under evaluation; for T14, full sanitizer/fuzz/TSan/soak functions need further testing. For T15/T16,
performance and collector-related aspects still require evaluation. Finally, for T17, clean release procedures should follow the procedures outlined in Section 4.

## 10. Service Total Quota and Metrics Handoff (2026-10-14) <a id="section-10"></a>

Previously, the budget was determined based on the default global objects. Therefore, operators couldn’t be configured. Multiple shard/generation instances were assigned to the same global object.
The observable service count is also missing. This round will allocate the budget to service instances: static pools and their shards.
The managed active/draining/replacement executor shares one service parent budget. Each of them has its own budget.
The inbound listener is reused with a sub-budget. The standalone user's outbound does not have a wire listener.
Owner, using service budget. After each scope is rejected, roll back the local temporary reservation, asynchronous write buffer
The ancestor byte quota is returned only when the lease ends and the last owner leaves.

Added three service fields and two listener fields to the configuration. Both the CLI and `host.json` are now properly configured.
Invalid, zero value, overflow failure closure; see finite default values and their semantics
[Refer to host configuration](../../reference/host-configuration.md#websocket-aggregate-limits-and-metrics) The budget only limits
WebSocket network write buffer and metadata/in-flight bytes, IPC, reorganization, Worker/JS heap
Both the entire process and memory have their own independent limits. The Host aggregate quota does not go into the wire/ABI, nor does it change.
There is a generation identity with per-Worker protocol limits.

### 10.1 Metrics and Observed Issues Identified

Reserved connections, active channel after 101/result ACK, pending dial, HTTP inflight
Both queue bytes and managed admin are exported. Managed Admin `/metrics` can read services and listeners.
used/limit/high-water/rejections; using only a fixed scope/resource and configured listener
name, excluding base tags such as URL/ID. provider is a weak reference, allowing retirement
Listener recycling; concurrent scrape does not lock the network loop, nor does it promise atomic transaction snapshots.

In the local environment, setting `CAPSID_HOST_IPC_METRICS=1` enables diagnostic output during the first round of processing. However, after the
last HTTP write operation is completed, there may be no more events to process. As a result, the value “1” remains displayed for those events that are still being
processed. By adding diagnostic output for HTTP completion/abortion and post-inbound connection processing, we can observe that when the value “101” is encountered, the value of “active=1” remains, while “HTTP=0” is displayed.
The request timer for the first round of queue fixtures did not trigger the expected network write rejection; instead, it was handled by client messages.
Triggering the large frame send, continuing to require an increase in the number of rejects, EOF, normal HTTP status, and zero budget, with no
Relax the result assertion. The failed configuration examples, JUnit results that passed in the first round with 5/8, and the original logs are retained.

### 10.2 Practical Verification

This round of reconstruction resulted in the creation of 33 executable targets. The targeted CTest **51/51 passed, 0 skipped**.
Includes 45 items with front wheels, schema/model regression tests, new budget-based unit tests, and real TCP capacity tests for three modes.
Each of the three modes operates in five different scenarios: maximum service connection limits, listener limits, pending dialing limits, network buffer failures during
service operations, and network buffer failures related to listeners. In the static pool configuration, only 3 shards are actually used. This approach allows us to verify that the
overall limit does not increase proportionally with the number of shards. Additionally, we ensure that there is no actual outbound TCP traffic before a failure occurs. We also consider scenarios where 101/HTTP requests are processed independently, as
well as situations where capacity is reused after successful closures or failures. The buffer limits are set appropriately, and the exit code is set to 0,
with no forced termination occurring. The complete stdout report is extracted from LastTest.log, ensuring that any successful outcomes are properly recorded for JUnit testing purposes.

The Native budget also covers two scenarios: ancestor rejection/rollback, duplicate releases, byte lease during
the last writer’s survival period, weak metrics providers, and concurrent reservation/release operations by
12 threads. The Generation Driver includes a shared quota for two pools, and health replacement cannot bypass the parent budget.
The Dialer Driver has a shared pending-dial limit among two network owners, allowing for reuse across owners after cancellation.
DNS-based four-stage cancellation mechanisms, retirement/collapse mechanisms, Close deadline mechanisms, ABI consumers, and
three-mode standard HTTP/WS mechanisms are still functional. The independent compilation of WorkerExecutor without any networking-related macros has been verified.
This only demonstrates the conditional compilation boundaries for Linux compilers; it does not represent native Windows/macOS implementations.

In the local mode, five new CLI fields are used, along with 30 additional invalid input combinations. Values that are zero, negative/error values,
and u64 overflows are all rejected before the necessary worker/source parameter checks are performed.
Some sanitizer plugins cover a range of eight hosts: ledger, metrics, resolver, dialer, session,
decoder, executor, pool. In total, there are 11 translation units involved.
Three complete native drivers have passed ASan+UBSan/LSan tests for both three-mode capacity testing. Other hosts/
runtime/worker, c-ares remain as regular builds and do not replace the full sanitizer/TSan.
Both variant instances were successfully compiled and linked. However, during runtime, they were caught by assertions: the queue budget of the ancestor was
ignored; the replacement instance did not inherit any service budgets. The behavior is similar to that of the regular source code, using the same drivers.

Details of this round of evidence can be found in the “Budget Evidence Index” (file: `build-v030-dev/evidence/continuation-budget/index.json`). This file contains
information regarding the selected test names, actual JUnit results, failed attempts, source code/binary summaries, as well as complete scenario reports.

### 10.3 Not yet fully qualified

T08: Continued retention of non-comprehensive qualification: This round is for Linux development build, not for Windows/macOS native platforms
Gate; no runtime hot reconfiguration commitment. Production strict syscall, Binding for T11/T12
WS bridge/authority/replacement, Managed Admin actual replacement and CPU/heap/OOM matrix still pending
Compensation measures: T04 parameters/ErrorEvents/Closing boundaries; T13 second client; T14 full-scale sanitizer/fuzz measures.
TSan/soak, T15/T16 measurement and collector, T17 clean release evidence continues to be executed. The working tree has not been submitted.
Please keep the ABI 7 and wire v4.2.1 standards. The results of this round should not be labeled as v0.3.0 release-ready.

## 11 Managed Lifecycle and async Reference Reclamation Handoff <a id="section-11"></a>

2026-10-14: Three new mechanisms have been added: deploy, retire-deadline, and replacement for managed administrators.
These mechanisms address issues such as old connections across generations, pending handshakes, delays in completing old callbacks, repeated deployments, cessation of new requests
after retirement, 10-second quiet draining periods, worker replacement, as well as service budget management and reuse.
The test uses a standalone Node TCP/HTTP client. The source files are as follows:
[Managed lifecycle driven](../../../tests/test_websocket_managed_lifecycle.mjs).

### 11.1 Implemented Fixes

The managed retired tombstone now correctly returns `active: false`. The previous state mapping is still being attempted.
Reading the active generation field caused an exception in the Admin status query. Corresponding to the native eviction scenario.
Supplemented assertion. The final state of Admin operations still uses `active`, separate from whether the application is active.

Both single-worker and ManagedListener share non-blocking read operations. If there is no data available after the read is completed, the operation
is retried. The request and executor are rechecked, and the response written by the client after sending a FIN signal is retained.
Newly Added [Read the test cases](../../../tests/test_host_client_disconnect.cc). The semi-closed state before correction
Scenario has no complete response, corrected and passed.

Actual WS testing further revealed that resolvers captured by async closures lost their valid references after garbage collection.
Newly Added [0036 Patch](../../../patches/txiki/0036-quickjs-async-closure-gc.patch), Based on
[Bellard upstream fix](https://github.com/bellard/quickjs/commit/7414e5f67f9a404f3cf91ffa69d0c93bf46d099e)
Adapt the NG's var-ref array, establish the recycling relationship for capturing references to heap frames, and synchronize the adjustment of the generator.
Completion path. [Forced GC Regression](../../../tests/test_quickjs_async_gc.cc) Covering resolver,
Two types of generators: throw and deprecated async loops.

### 11.2 Verified Scope

The addition of new test cases resulted in failures during the operation of the old engine. However, partial debugging and patching successfully resolved the issue. For the 2,000
deprecated async loops, the heap usage remained constant at 84,417 bytes before and after the cleanup process. The local reimplementation of the single-worker WS scenario was successfully tested
using 23 assertions; the exit code was 0, and no forced stops occurred.

Of the 13 targeted regression tests conducted before round 0036, 12 were successful. The “retire-deadline” operation failed during the re-establishment of
outbound connections. The complete Managed Regression Tests have not yet been executed, so it’s not possible to achieve a score of 13/13.
This section retains the initial handover state; the formal test results will be described in Section 11.3 and Section 12.
The local Worker instrumentation only covers QuickJS and worker runtime environments. It disables leak detection and other
related functions, thereby not replacing the verification processes that are performed for managed or officially configured environments.

### 11.3 GLM Continuation Checkpoint

The number of overlays has been updated to 37. [Build baseline](../../data/txiki-upgrade-baseline.json) The key and
The manifest has been synchronized, and the CMake configuration is successful. The formal build of 39 targets has begun, and it is in accordance with user requirements.
The transfer of the document has been completed; however, a complete build and regression testing have not yet been carried out to ensure that all issues are resolved.

The original log is stored in `build-v030-dev/evidence/continuation-managed/`. GLM has
2026-10-14 Formal verification completed according to section 4 of the continuation instructions: Configuration (including `CAPSID_WORKERD_REF`),
After registering `websocket_outbound_reference` (total 410 items); the saved make graph command
Contains 39 executable targets, with 47 command parameters. `quickjs_async_closure_gc`…
`quickjs_job_context_hooks`, 13 items of orientation
Regression tests (`fast-junit.xml` 13/0/0) and a complete set of 64 items (`final-junit.xml` 64/0/0)
Passed all tests; Overlay integrity four items, `worker_async_request_context` and
`runtime_build_identity` passed; vendors remain clean. See final index and binary digest for details.
`build-v030-dev/evidence/continuation-managed/index.json`. T12's Binding,
CPU/heap/OOM, native platforms, and subsequent T13–T17 are still scheduled to remain as unfinished tasks. Currently, all of them
The changes have not yet been submitted.

## 12 GLM Formal Verification Review <a id="section-12"></a>

2026-10-14: Independent review of this GLM iteration has been completed. The HEAD value remains `8028b88`; no new
Git commits have been added. The working directory still contains implementations from previous iterations that have not been committed yet. The deliverables that can be verified currently include formal test results,
evidence indexes, and status documents. Any existing implementations will not be counted as new features of GLM.

### 12.1 Acceptable Verification Results

`fast-junit.xml` actually has 13/13 tests, and `final-junit.xml` has 64/64 tests. There were no failures or skips
in either case. The names of the 64 test cases match those in the selection list. There are indeed 410 items in the registration data, but
this is just the number of registered tests. All three complete JSON reports for the managed components are complete; the exit code is 0, and there were no
forced stops. The summaries for Hosts, Workers, Drivers, and Helpers are consistent with the information in the current files.

Independent rerun of 13 items all passed: Managed three scenarios, read detection, native retirement, QuickJS two items,
Request context, runtime identity, and overlay. The functional results in the original verification scope can be
Acceptance, T12 remaining scope still needs to be completed.

### 12.2 P1 The old generation pool is retained during the Host's survival period <a id="section-122"></a>

[main.cc](../../../src/host/main.cc) The activation/retire path will put the old pool in
`draining_pools`, this collection is only cleared in the `reclaim_workers` of Host shutdown.
[GenerationPool::finalize_drain](../../../src/host/generation_pool.cc) Clear the slot and
The retired executor releases the ledger count, but does not release the pool held by the Host collection.
The `options_.factory` of the pool still captures data such as the bundle and effective config using value-based approaches. The joining of pump threads
also relies on the pool’s wait/cleanup mechanisms. Therefore, even after the Worker exits and the budget is reset to zero, it cannot be guaranteed that all
resources associated with the generation have been properly returned.

The controlled reproduction process utilized a source bundle of approximately 512 KiB. The v1/v2 versions were deployed 16 times across one managed
host. Each time, it was ensured that the old workers were terminated. In the end, all workers were retired.

| Checkpoints | Host RSS KiB | Host VmSize KiB | live Worker |
| --- | --- | --- | --- |
| Initial v1 | 11,740 | 531,508 | 1 |
| The 8th Generation | 19,088 | 736,344 | 1 |
| The 16th Generation | 23,764 | 801,912 | 1 |
| After all being retired | 23,720 | 801,912 | 0 |

Finally, service channels, active, dials, queue bytes, and HTTP inflight are all 0.
The host has successfully exited. The source code confirms that the reference remains valid; memory sampling shows that the process continues smoothly. It’s not possible to completely stop this process at this point.
The RSS increments are precisely attributable to a specific object. This behavior affects the long-running Managed Host and should be addressed
T12 Pre-Qualification Fixes, which are remnants of the existing implementation. There is no evidence in this round that these fixes are caused by GLM's new code.

The rectification should allow the drain to complete the notification and enter the control thread, during the joinable, event, and request pin lifecycle.
Allow removal of old pools at permitted times. Do not directly release the last one in the pool's pump thread callback.
References, so that the destruction does not wait for its own thread. Additional rounds of replacement/retirement testing, check factory/pool references.
Indeed, release and thread join; one cannot simply assert that the number of child processes and the ledger are 0.

**2026-10-14 GLM Correction and Self-Report Results (P1, further review in Section 14):** The old pool is now in `on_drain_complete` (pump thread,
After finalizing, move the Host references from `draining_pools` to the `finished_pools` parking lot—
Never destruct on the pump thread (destruct requires joining this thread); `prepare_activation`, `prepare_retire`
Coordinating the cleanup of the parking lot with the `reclaim_workers` function that exits with the Host.
`mallopt(M_ARENA_MAX, 8)` prevents instant thread replacement from reserving 64 MiB of arena space per thread for glibc
Address space. Added regression `websocket_managed_replacement-recursion` (16 rounds ~512 KiB)
Replacement, RSS/VmSize assertion + full retirement return): Disabled return logic
"16 replacements increased the RSS size by 11556 KiB" failed (confirmed by Red, the magnitude is consistent with the review reproduction).
Enabled and passed; the arena cap reduced the increment of VmSize from 147,464 KiB to 16,392 KiB in 16 rounds.
After corrections, the fast 13/13 configuration, full 64/64 configuration, three-scenario Managed mode, and the newly reintroduced features all passed the testing. For more details
on the individual reports and indexes, please refer to the file `build-v030-dev/evidence/continuation-managed/index-p1fix.json`.

### 12.3 P2 Formal Verification Archive Incomplete

The `handoff-configure2.log` file referenced by the GLM index is not stored in the evidence directory. The existing
`build-gc.log` file still contains a log that was terminated during the initial transfer process, and thus cannot be considered a record of successful construction.
The reports related to QuickJS and overlay also did not get archived separately. The index only retains information related to successful passes. Since the HEAD version
remains unchanged and the source code has not been submitted, the index lacks summaries of the C++/patch-related source code from this round of development.
This independent rerun confirms that the corresponding application is currently passing. It cannot be used to compensate for the log generated by the original GLM operation.

Additionally, the original `fast-*` files from 12/13 have been overwritten by the results of this round 13/13, and no failed versions are retained.
The original documentation still interprets these files as records of failures prior to the fix. It is necessary to include official configuration/build commands, completion
status, separate test reports, source code, and summaries of results. Any missing historical failure records should be marked as “not retained”.
The 47 argv items should be adjusted to 39 executable targets.

### 12.4 P2 Handover Instructions Still Reference Old Checkpoints

GLM indicates that the specified test cases have been passed at the top of the handover instructions, but the main text and the final copyable task instructions still require
“Recovery construction is still in progress; the entire group has not yet been restored.” Priority 1 still refers to the same old verification task. This will lead to further issues in the future.
The executor repeats the work from the previous round. This time, the handover instructions and plan have been synchronized: prerequisites of section 12.2, generation, have been completed.
Return the resources and provide additional evidence, then continue with the remaining matrices for Binding, CPU/heap/OOM, etc.

For details on the reviewed evidence, please refer to the review index (located at `build-v030-dev/evidence/review-glm-20261014/index.json`). This index
includes 13 independent items related to JUnit testing, complete LastTest.log files, script logs for successive generations, as well as summaries of current source code
and binary files. Only the review and transfer processes have been updated in this round; the actual implementation details have not been modified.

## 13 Review of Completeness and Strong Compatibility with workerd <a id="section-13"></a>

2026-10-14 Inspection required by user request for test integrity of 0.3.0. The conclusion is that additional work is needed. Complete task matrix.
17 confirmed observation differences and formal regression suggestions are as follows:
[Test Integrity Review](test-coverage-review.md).

All 20 agreements/channels/budgets/networks/engine components in this round have passed the testing. The same code used in all projects have met the requirements.
Among the 27 API observatories locked to workerd and Capsid, 14 have returned values or exception categories.
Differences; additional async messages overlap; ErrorEvent categories and properties; closing result of handler throws
3 Item differences, stable reproduction over three consecutive rounds. These behaviors have not been asserted through collection, and therefore they cannot be used to claim strong compatibility.

The new regression tests should cover stages from C01 to C17. It is also necessary to consider differences in construction/transfer processes, multiple callbacks connected to the same object, CPU/heap issues,
OOM situations, as well as issues related to Binding and generation of objects. The strict differential comparator should include all necessary
cases. It is not recommended to standardize exception categories, states, event categories, or their order. Additionally, failures on both sides should not be considered as successful outcomes.

The current registered directory contains 409 items. Compared to the previous round of 410 items, the `websocket_outbound_reference` item is missing.
The item may disappear if the environment variables are missing during configuration. When this item is selected alone, ctest actually returns 0, indicating zero tests.
The selection of the main version of CI’s nodes also differs from the test lock specified in v24.14.0. For strongly compatible lanes, it is necessary to address issues related
to fixed dependencies, mandatory registrations, reporting mechanisms, and zero-skipping checks.

During the final stage of processing, new Host pools were detected. These pools underwent modifications and binary updates. After the updates, the Hosts were
rerun through the API/event probes, and the same 17 differences were still observed. Section 12.2 serves as historical evidence for the old implementation. The new recycling mechanism still needs further
testing, with verification required for objects and threads involved. The initial snapshot and the results after the updates are archived separately.

This round of modifications was implemented without any changes to the underlying system. Details regarding APIs, event detectors, duplicate reports, 20 JUnit tests, complete
registration directories, source code, and binary summaries can be found
in the testing and review index (file location: `build-v030-dev/evidence/review-v030-tests-20261014/index.json`).

## 14 Latest Recycle Implementation, Compatibility and Test Set Consolidation Review <a id="section-14"></a>

2026-10-14 Independent review of GLM second-round recycling and correction. Details of the complete source code, observation tables, and the order of rectification can be found here.
[Latest merge feedback](combined-review.md) Normal 16-generation RSS increments are
The size was reduced from 12,024 to 4,768 KiB, and the increment in VmSize decreased from 270,404 to 16,392 KiB, indicating a confirmed improvement.

P1 has not yet been resolved: The `on_drain_complete` event captures that the `pool_box` still holds its own pool. The
persist operation fails, and no request is made to drain the pool. Therefore, the pool is not released, and no destruction occurs. In actual scenarios, this failure occurs three times.
In both rounds of testing, Workers 1 to 4, and threads 8 to 14 are involved. Older versions still return a 200 status code. The failed prepare/abort
operations have not yet cleared the old pool from references. It is necessary to verify both the capacity rollback process and the return of final objects and threads.

In this round, 17 independent CTests were conducted, with 16/17 passing. The new features failed during the 10th deployment attempt, but succeeded
after three additional runs. The existing failed transactions have passed unit tests. However, when using a test harness that does not include production-related callbacks, it is
impossible to detect any issues with the actual Host system. None of the 13/64 sets in GLM contain any new features, so they cannot be
used as substitutes for the official acceptance tests.

The current directory contains 411 items. The issues related to reference restoration and new item addition have been addressed. However, problems related to conditional
registration and CI-related floating nodes still need to be resolved. After rerunning the workerd process with the same input, tasks from C01–C17 are still active.
Tasks related to A–D, owner/GC/Binding, the second client, native components, and fuzz/soak still need to be completed.

This round of operations was carried out without any modifications. For more details, refer to the combined review
index (located at `build-v030-dev/evidence/review-latest-combined-20261014/index.json`). This index includes information on
failed attempts, three additional runs, resource sampling per round, failed transactions after two rounds of attempts, as well as compatibility reports and summaries.

## 15 GLM, Third and Fourth Round of Independent Review <a id="section-15"></a>

2026-10-14 Review: weak pool wiring, failed rollback, formal API fixtures, and stage commit corrections.
For detailed results, please refer to [Fix Review](fix-review.md). After three rollbacks with the old failed deployment probe
Worker always 1, threads always 8. The issue with the original strong self-holding has no longer occurred; the cleanup of parking references has been fixed.
The inputs for C01–C14, C16, and C17 are valid. C15 remains open. The GLM 15/66 and zero-skip
reports are valid, and they include two new managed cases.

New boundary review findings: 16-bit modulo operation destroys the original matched large number code; Response construction accepted
Non-WebSocket; When the stage closes, set outgoingClosed first, then abort early when synchronously throwing and exiting early.
There is no final state in the 2-second window. The reject after immediate/wait of the async handler is treated as fatal by Capsid.
Close it. The next message can continue as normal. Tags like Symbol/BigInt still need to be processed further.

In this round, 70 independent tasks were completed, with a success rate of 69/70. There was one failure due to incorrect timeout settings
during the exit process. However, the remaining three tasks were completed successfully. The worker responsible for the failed task has been reclaimed. Nevertheless, when using older versions of the system,
the RSS metrics still indicate that the new worker has been removed from the system. It is not possible to use existing “green lights” to cover up these shortcomings.

Evidence can be found in the index file for this round of testing (`build-v030-dev/evidence/review-glm-fixes-20261014/index.json`). This file contains information
about GLM snapshots, results from 70 tests and three additional runs, as well as details regarding original/extended APIs, error-prone paths, metrics, and summaries of source code/products.
This round has no modifications to the running implementation. T12 remains strongly compatible and has not been fully qualified.

Continuing with the review process (same source code and output): In the actual protocol header, elements such as null, numbers, boolean values, ordinary
objects, and iterable types differ from reference types. In C12, these elements are only used during the construction phase. After the three modes complete their returns, there is a 80 ms delay in accepting responses,
resulting in a status of 503. However, when using reference types, the acceptance process completes successfully in 101/echo/close scenarios. Repeated
attempts at transfer fail within 8 seconds, with no response being given from the system. The behavior during normal upgrades, before returning, during the first transfer, and
when cloning also matches the expected behavior. Finally, the isolation probe exits normally. For detailed results and early warnings regarding potential issues, please refer to sections 10–11 of the repair report.

## 16 GLM Fifth to Seventh Independent Reviews (2026-10-15) <a id="section-16"></a>

For more details, please refer to the following: [This round of review](rounds-5-7-review.md). Original 38 API observation points, C15,
Synchronous throw/async reject, protocol header/negotiation, delayed accept, and failure metric identity have been
Review passed; the permanent reference registration and precise Node pin have also been corrected.

Independent 70 items are for 69/70, with zero skips. This time, only `websocket_managed_deploy-persist-failure` is involved.
Failure: There is still one surviving Worker after the third rollback. Three separate attempts are counted as pass/fail/pass. Diagnostics copy.
Also reproduced, the stability recovery remains open; no native stack was obtained, so the investigation direction for notifier references is not pursued.
Root cause has been identified. 9 out of 14 conversion controls are inconsistent with the three modes. Formal regression test for `/retransfer`
It is impossible to prove that there is a repeated handover each time the Response is changed, and there is no Upgrade for the second time. It is a continuous occurrence with the same fixture.
Two real Upgrade instances revealed differences in the pre-requestly built endpoint, with a reference of 500, and Capsid
All three modes are at 101/echo. Independent probes that actually receive the same response and then hand it over have a clear 500 status, with no rollback.
This fix. The formal C12 peer key parsing failed, and all 8 valid inputs still passed after closing 1006.
Test corrections required and restoration of open/protocol/clean close gold; independent correctness of peer implementation consistency.
CI obtaining reference, zero skipping required, and strict comparison still needs improvement.

Currently, the code still uses the HEAD state `8028b88`, and no changes have been made to the running code. Details regarding this round’s source code/products, official
failures, reruns, transitions between modes, metrics, and diagnostics can be found in
the evidence index (file: `build-v030-dev/evidence/review-glm-fixes-20261015/index.json`).
The subsequent order is as follows: [Continued explanation](continuation-handoff.md) Section 5-6 for details.
T12: Strong compatibility and complete release qualification remain unfinished.

## 17 2026-10-15 Follow-up fix after review <a id="section-17"></a>

This round of updates involves directly repairing issues in the latest GLM workspace, with authorization granted by the user. The functionality of decoding characters using
valid USVStrings and message texts is retained. Additionally, simplified identity checks, assertion counts, budgets, and comprehensive report checks are restored. Exceptions related to unbounded
retries and terminal token attribution checks, which allow bypassing bounded native submissions, are also removed.

| Fixing | Root cause analysis and implementation | Evidence of old problems that can be detected |
| --- | --- | --- |
| GenerationPool Recycling | The temporary `weak.lock()` of the executor notifier can become the last reference and will be destroyed in its own thread; the replacement strong hold also carries similar risks. The callback and the production thread use non-owning pointers whose lifetimes are guaranteed by pool join. The thread is pinned to the finished pool until the completion hook returns. | `notify-red-ctest.log` has failed in the old implementation; normal/adopted/replacement methods still involve rolling back the green color of the Hosts that were referenced in the original configuration. |
| Request Attribution and Conversion | The endpoint records the creation of the origin; it ensures that the transfer requirements are consistent with the current request. The value conversion is called once, distinguishing between the code and USVString reason. The priority of Symbol.toPrimitive is retained. | The extended probe found that the code was read twice; the formal 191 assertion/API and the independent 52-point difference passed. |
| Managed exit | `sessions_.size()+1` reuses the persistent connection number after the intermediate connection is closed, causing the registration to be overwritten. As a result, stop cannot close all connections. | For older real HTTP connections, only managed connections require forced termination for regression tests. The call stack indicates that the listener wait/IO loop has not been terminated. After the monotonic numbering fix, the three-mode system exits normally. |
| Pong Sequence | After adding eight new windows, the earliest pending Pong was merged and replaced, so that subsequent old replies are placed after the new ones. | The old binary actually executed `ping-027` after `ping-013`. It has been changed to the latest draft and the upper limit of queue bytes after verification and replacement has been set. The three-mode flood has been officially approved. |
| Test exit and UTF-8 | `wsSession` maintains connections that remain open after the script is completed, forcing the reference to exit; the message text has been changed to 'lossy', but the original unit tests still expect 1007. | The assistant is waiting for the Close handshake, and all fixtures have saved and exited their results; four types of invalid coding, subsequent binary exchanges, and clean close have passed both ends; the independent 1007 check for the Close reason is retained. |
| Complete reference: Gate | Node pin and registration alone are not sufficient to prove execution; reports may also be incomplete or use old source code/binaries. | Lock the installer, five essential CTest runs, comparison of four complete reports, and 24 validations controls; input deficiencies, skips, identity differences, behavior differences, and abnormal exits are all rejected. |

The official `/retransfer` maintains the same Response, while `/precreated` uses the items that were not transferred from the previous request.
Endpoint, `/fresh` creates new ones each time; all three proceed with a real Upgrade. There must be eight valid inputs for C12.
Open/Select protocol/1000 Clean, errors must be accepted with a failure. Only terminal observations were made in C12.
Close, the reference to ErrorEvent when the subsequent HTTP context is not cleaned up is not considered a compatible conclusion.

Final verification was performed using Linux RelWithDebInfo, GNU 13.3, LTO off, and OpenSSL 3.0.13 was used for development.
Re-download the official locked workerd and verify the compressed package, executable file summary, and version; Node
24.14.0/undici 7.21.0, reference date 2026-08-20, flags [].

| Verification | Actual results |
| --- | --- |
| Final Complete Build | Success, `build-pong-result.json` exits with 0. |
| Independent Required Sets | 74/74, zero failures/skip; currently registered 416 items; 48.447 seconds |
| Complete workerd gatekeeping | 5/5; 14 fixtures × reference/three types of Host modes, plus outbound reference; 36.375 seconds |
| fixture assertion record | reference 2,783; single-worker 530; static-pool 530; managed 496. The number of merges in Pong varies due to differences in the number of counts, and it is not required that the counts be equal; the API for each mode is 191. |
| Gate control | 24/24, no skips, includes error summaries, missing items, repetitive patterns, erroneous observations, and abnormal exits |
| Recycling and Exit Orientation | Six items passed three times each in a row; including three-mode connection registration, native pool interleaving, and real persistence rollback |
| Independent Conversion and Events | 52 points × three modes consistent with the reference; concurrent order, failure events, and final state after throw compared to passing |

The original evidence is concentrated in
continuation-codex-20261015/index.json (Local artifact `build-v030-dev/evidence/continuation-codex-20261015/index.json`)
Including the initial GLM snapshot, previous version failures, intermediate gate failures, final commands/JUnit/LastTest, four complete sets
Report and source summary. In the first round of security checks, it was discovered that the UTF-8 encoding was still in use, and the reference connections were not properly closed. This issue persisted even in the second round of checks.
Exposed Pong disorder; two rounds of failures retained, third round with strict gatekeeping and subsequent 74 passes.

Host SHA-256: `17f54a0daf0697808a4de881109127572e4cee5b9929398f85d9b4d71ab630ba`.
Worker SHA-256: `d04473a252a49d2252717fb1311814573daa9a667eca16721a143bf910d073de`.
HEAD remains `8028b88`. No commits, pushes, or releases have been performed this round.

The current compatibility exceptions have been narrowed down to two fixtures: received illegal Close code, and limited sending budget.
An API field (the accepted endpoint terminates after 500 or partial 101); outbound
There are still differences in the three items, which can be seen in the semantic file. After T12 is finally retired, the finished pool/factory is completed.
Release, Binding, CPU/heap/OOM, Request Number Table and Old Executor Retain Quantization, Second Client,
Windows/macOS, Release, and the complete sanitizer/TSan/fuzz/soak matrix have not yet been qualified.
The results of this round are not expanded to represent all 416 items as passed, nor are they considered as the conclusion for the release of v0.3.0.

## 18 2026-10-15 Review of Test Validity and Differences with workerd <a id="section-18"></a>

This round of testing was conducted to determine whether the intensity of the tests has been reduced, any discrepancies have been masked, or there are any missing controls. Before the testing, the source code was consistent
with the final summary provided in section 17. Isolated fault injections and observations were carried out only in the evidence directory that is ignored by
Git. The actual implementation, fixtures, and drivers remained unchanged. These findings cannot be interpreted as signs of subjective cheating, but
they do indicate the presence of weak assertions, oversight in the testing process, and actual compatibility issues.

Evidence directory:
test-integrity-audit-20261015/index.json (Original file: `build-v030-dev/evidence/test-integrity-audit-20261015/index.json`).
Section 17's scores of 74/74 and five successful access attempts are historical facts. The concept of “strict comparison” applies only to those fields that
are actually collected and used for comparison. It cannot be extended to cover the entire 14 scenarios. The aforementioned access issues also involve shortcomings that were
addressed in the previous version of the Codex. These shortcomings cannot be attributed solely to GLM.

### 18.1 P1: The actual type of ErrorEvent.error is not compatible, and this was overlooked during formal testing.

The same worker's source code is dialed to the newly released local port, locking the reference with three types of Hosts.
Generate error→close/1006, but the error payload is different:

| Observation Values | workerd | Capsid Three Modes |
| --- | --- | --- |
| event.constructor.name | ErrorEvent | ErrorEvent |
| typeof event.error | object | string |
| event.error instanceof Error | true | false |
| event.message | Uncaught Error: Network connection lost. | error |
| event.error.name | Error | There is no such thing. |

The `js/websocket.js` file incorrectly calls `new ErrorEvent('error', options)`. However, the constructor
in `vendor/txiki.js/src/js/polyfills/event-target-polyfill.js` only accepts a `constructor(error)`
parameter, so only the first string parameter is stored. The `api-compat` C16
only checks whether the class name and message/error fields exist; the `failure()` function only checks the event type,
readyState, and close status, ignoring the type and content of the payload.

The evidence is `error-event-shape.mjs/json`. All four backends have terminated normally. Refer to the diagnostic text for more details; for other types of
failures, random reference IDs may be involved, so it’s not advisable to strictly follow the text verbatim. The type of the Error object and its stable fields must be explicitly specified.
Prioritize the construction call and convert independent probes into formal regression tests.

### 18.2 P2: There is a provable false green light in the order of closing codes, reasons, and getters

Isolated copies only inject erroneous behavior, retaining all assertions of the formal driver:

| Fault Injection | Lock in reference normal results | The results of the injected Capsid | Formal API Testing |
| --- | --- | --- | --- |
| The actual closing code for close(1000.5) has been replaced | 1000 | 3001 | 191 assertions passed |
| The reason for close(1000, 123n) has been replaced | 123 | CORRUPTED | 191 assertions passed |
| The input dictionary is read once before constructing the CloseEvent. | Appeared once in each of code/reason/wasClean categories. | All three getters are read twice. | 191 assertions passed |

C03 and e-close only wait for close and check the "call acceptance", without asserting the code received by the client,
reason, wasClean. Both wire failures do not alter the API observation; the comparator is not available.
Difference data. The init of `E-getter-order` is a plain literal, followed by reading the construction result
`ce.code/ce.reason/ce.wasClean`; This does not test the reading order or frequency of the input dictionary getter.
The independent test with 52 points has verified the current implementation of the input getter, but this test has not yet been included in the formal CTest.

The evidence includes files such as `mutation-baseline.log`, `mutation-wire-close.log`, and
`mutation-getter-reads.log`, along with the corresponding source code snippets. This indicates a deficiency in regression detection capabilities. It cannot be claimed that the
production implementations that are not compromised are still capable of sending messages with the status 3001/CORRUPTED.

### 18.3 P1: The complete set of scenarios does not have an independent list. Deleting tests can also reduce expectations.

`run-workerd-ws-gate.mjs` generates expected.fixtures through directory enumeration, and the runner also comes from the same
List the current fixtures. The gate checks that "all remaining files have been run", there is no fixed
List of required scenarios. The list of directories derived from synchronization after the transport is simulated and deleted by negative control:
The comparator accepts 13 items and returns complete=true. The existing "fixture deficiency" control is only from results
Deleting one item while keeping the expected values unchanged does not result in any files being removed from the directory itself.

A manifest detailing the necessary scenarios for review should be created. This manifest should include information on the directories, drivers, reports, and execution results.
Any items missing from the manifest should be considered failed. The `gate-negative-controls.json` file also serves as a
mechanism to ensure that any attempts to manipulate the order of the APIs involved in the comparison are successfully prevented.

### 18.4 P2: Most reports lack actual observational data, with an exception range that is too broad. Outgoing results do not undergo strict comparison.

In the final gate's reference report, 11 out of 14 observations were null; echo returned
Object, but both internal callbacks have no return value, and they become `{}` after serialization. Only the API and
The invalid-utf8 field contains non-empty observation data. In other scenarios, there are still valid absolute assertions, so it cannot be considered as not tested.
However, comparisons of `null===null` or empty objects cannot provide independent observations across implementations.

Exceptions for `close-boundary` and `slow-consumer` are skipped across the entire fixture. Negative control
The report observation of close-boundary has been changed to an unexpected 1000, and the comparator still accepts it. Formal driver of
It is absolutely certain that the code will be checked currently. Therefore, this result indicates a flaw in the reporting mechanism. It should be noted that…
Each scenario returns actual results, with the exceptions for Close code limited to precise fields; stress cases include
Independent bounded contracts and resource final states.

The outbound runner supports the `--report` option. However, the full access control mechanism only runs the outbound CTest related to the
reference case. It does not generate reports for each individual case in the three-mode outbound scenario. The standard set of 74 items is simply executed and passed through without any further processing.
After the complete outbound reports were rearchived in this round, an additional error occurred with object ID 18.1. The three-mode Capsid
outbound process failed once; the reference driver also failed once (due to additional error events during dial-up connections). Only the observation copies that
failed during archiving were processed, and they were able to pass through twice. The original failures remain unresolved and cannot be attributed to any stable solution. This
assertion has not been reduced, nor can the failures be compensated by retrying the process.

The outbound report shows 165/166 as the number of calls, including 141 badkey instances; only 23/24 represent the number of grouped scenarios.
This kind of repetition can be used to indicate resource recycling pressure, but it cannot serve as evidence for hundreds of independent semantic scenarios. The report provides
statistics for both types of situations, and this in itself is not considered cheating.

### 18.5 Confirmed Existing Differences and Historical Reduction of Intensity

Except for the newly discovered ErrorEvent payload, the following existing differences have been explicitly documented or branch-asserted:

| Input/Stage | workerd | Capsid | Current Processing |
| --- | --- | --- | --- |
| The server has selected an unproposed subprotocol. | Accept | Rejection | Outbound Explicit Differences |
| The dial-up connection for HTTP completion has not been established yet. | No more delivery completion events | The independent owner continues to complete the work. | Outbound Explicit Differences |
| The close callback that suddenly ends with EOF after open | readyState 1, reference to the reasons for disconnection | readyState 3, native disconnection reason | Outbound record, existing assertions are not frozen state/reason |
| Received Close code 999 | Automatic Repeater 999 | 1002 | fixture explicit difference |
| Synchronous transmission of 8 MiB burst | Complete Delivery | Limited Queue Rejection, Client 1006 | Product Budget Differences |
| Accepted endpoint handover | Terminate after writing 101 | 500 | API Field Exceptions |

They are not hidden fully compatible results, and cannot be considered completely consistent with workerd just by passing both sides' use cases.

The last saved `glm-delta-tests_test_workerd_ws_fixtures.mjs.txt` confirms that GLM has
Delete Node/undici/version/date/flags check, allow skipping without driver, select all items
Change “Executed” to “At least six items”. Remove the complete summary report, and specify “assertions:0” explicitly. The above parts have been updated.
Section 17 has been restored; this is code evidence that reduces the test intensity, and it cannot be inferred from the subjective intent.

The workflow has been published, but only annotations have been provided. The corresponding main checks have been passed. No actual testing has been conducted to verify the accuracy of the
tag SHA values within the repository, and this process does not involve any enforcement measures. The remote tag rules/branch protection mechanisms have not been
checked yet, so it cannot be assumed that this issue can be circumvented in a live environment. The submission for approval remains pending as a task labeled “T17”.

### 18.6 Next Steps for Rectification

1. The ErrorEvent method actually constructs and adds the three-mode/reference object type to the regression process.
2. Close the code/reason/clean and input the getter to read the log and add it to the formal API gold, then proceed to
   Independent 52-point critical control; the aforementioned fault injection must transition from green to red.
3. Introducing a fixed scenario manifest, to compensate for deleted files, missing drivers, and zero actual observations
   Negative control; each scenario produces comparable observations, with exceptions limited to specific fields/contracts.
4. Access the complete outbound report and field-level comparison, to trace the additional errors in the reference dial-up
   Instability, retain failure without relaxing assertions.
5. Run the complete necessary set and strict gatekeeping; for now, do not expand the compatibility qualification conclusions of Section 17.

## 19 2026-10-15 Testing enhancements and TDD fixes <a id="section-19"></a>

The goal of this round is to address any testing issues and then refine the implementation using TDD. Section 18 remains under review, along with all evidence of failures. No submissions
or releases have been made yet. For details on the current scope and subsequent progress,
please refer to the index for this round (file: `build-v030-dev/evidence/test-integrity-tdd-20261015/index.json`).

### 19.1 Implementation and Test Corrections with Red-First, Green-Later Approach

| Problems | Red-light evidence | Current corrections and regression tests |
| --- | --- | --- |
| ErrorEvent.error is a string. | The same enhancement API fixture is locked for qualification, the old Capsid fails the object type assertion. | Pass the error parameter to the single-parameter constructor of txiki, retain error.message, and generate an event message; the stable fields for rejecting connections are consistent across the four backends. |
| HTTP non-101 error classes are lost | A new assertion is capturing the Error of Capsid, with a reference to TypeError | The Host retains the actual HTTP status, and the JS dispatch results in a TypeError; both 200 and 302 are verified for a complete and stable message. |
| Missing checks for wire close and getter | Section 18: Fault Injection Still Passes the Old 191 Assertions | The formal driver checks the received code/reason/wasClean; the input getter records the actual reading order and count. |
| List reduction, empty observation values, entire exceptions | Five new controls were missed in the old comparator | Fixed mandatory test manifest; each item returns actual observed values; field-level exceptions and limited budget/Pong subsequence contract |
| Outgoing did not perform four-backend comparison | The original gate lacks input from the Capsid outbound report. | Fixed scenarios and call count lists, complete event/error objects/end states, and identity verification. The four backend reports are included within the same gate. |
| Release only after the comments have passed the test | The new workflow audit failed with the old release configuration. | Release the call according to the submitted validity workflow. The package depends on validation, and the checkout is fixed to github.sha. |

`workerd_ws_mutation_controls` uses isolated fixture copies, first requiring the normal baseline to pass.
Change the closing codes to 3001, change 'reason' to 'CORRUPTED', read the getter once additionally,
The ErrorEvent payload should be changed to a string. All four items must result in a specified assertion failure; non-zero exit or startup.
Failure by itself does not constitute a successful test. This control has been entered into CTest and the necessary gate has been closed.
Previously, issues such as abort/readd, listener receiver, illegal callback objects, NaN values, reasons without a specified code, and
local CLOSING states also have been addressed and are now included in the official API specifications.

### 19.2 Narrow Variant of Reference Closure Event

The failure of the original reference test was reproduced again in this round. The eighth round of isolation concurrent dialing probes captured:
Both parties have a Close value of 1000, and the reason is the same, containing NUL text. The original peer's Close reply
Flushed, then received TCP end; the reference is dispatched again before the normal clean Close.
`Network connection lost.` ErrorEvent. Changing 'peer' to 'allowHalfOpen' will still cause the issue to occur.
Therefore, this assumption has been refuted, and the experimental changes have been withdrawn. No modifications were made to workerd, lock files, or compatible dates.

Now, all original events are retained. Only the observed variant where “there is exactly one occurrence after the
normal message sequence, before the matching clean Close operation, with a fixed Error object/message/readyState” is allowed.
Corresponding to control verification, error messages, states, sequences, second errors, and non-clean closes are all rejected.
The Capsid does not enjoy this exception. The gate summary lists the actual occurrences. It cannot be used to claim that the workerd has been determined.
The ultimate cause of internal conflicts. The source code observations come from the locked submissions
[WebSocket implementation](https://github.com/cloudflare/workerd/blob/8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f/src/workerd/api/web-socket.c%2B%2B),
For specific measurements, refer to the `outbound-race-probe.json` file.

### 19.3 Current Qualification and Scope

- Full build passed; first build failed due to lack of Node PATH in the invocation environment. Added a locked Node to fix the issue.
  Rebuild succeeded, failure logs retained.
- Complete gate **10/10, zero skips**: 14 inbound fixtures passed on all four backends and returned
  Non-empty observation values; four sets of outbound reports have passed the scenario comparison.
- The inbound and outbound comparators control **56/56**; the normal API baseline and four fault detections have passed.
- Outbound references consist of 24 groups/165 calls, and Capsid consists of 25 groups/166 calls.
  Add a clear dial-up deadline contract for Capsid, as 141 badkey instances constitute a repetitive stress call.
- The local audit for the release workflow has been marked as passed first and then as failed. When workflows are reused with relative paths in the same repository,
  GitHub uses the same definition for commits by the caller, see [Official Reuse Instructions](https://docs.github.com/en/actions/how-tos/reuse-automations/reuse-workflows).
  This round did not involve any pushes, triggering of hosted CI, or releases. Therefore, the remote complete Release qualification has not yet been executed.
- The first round with all **417** items has been completed, but the results are incomplete. Details of failures, omissions, and interruptions can be found in Section 20.
  The 10 gate items cannot be regarded as a complete pass.

The vulnerabilities listed in Section 18 have been addressed and fixed. The full testing process for users is still ongoing. In Section 5, regarding
the order of testing and the completeness of tests from A to I, the final retires, Binding, CPU/heap/OOM issues,
the second client, dynamic/native/complete release evidence still need to be addressed individually. These issues should not be
deleted or prioritized due to the green status granted this time.


## 20 2026-10-15 Full Test Review and Running Prerequisites <a id="section-20"></a>

### 20.1 First-round actual results

First round of registration: 417 items, completed 416 items: 401 passed, 11 failed, 3 skipped, 1 timed out.
`worker_package_contents` encountered a timeout of 1,800 seconds during the serial reconstruction process. Subsequent
attempts to use the old archive still failed. The remaining `worker_package_reproducibility` has been terminated to prevent further construction
based on this ineffective packaging process. This round cannot be considered fully successful, and there is no complete JUnit report available for evaluation.
Details regarding the reasons for exit/abortion, as well as individual logs, can be found in `full-result.json`.
`full-interruption.json`, `full-parsed-summary.json`, `full-ctest.log`.

These results pertain to the development workspace and WSL environment, and are processed as follows:

| Results | Review Conclusion and Evidence |
| --- | --- |
| Environment variable check failed | Confirm that the implementation violates the null environment convention: POSIX forwards all CAPSID_*, and Windows retains debugging parameters to inherit the entire environment. All strict/normal mode canaries on the old library failed; all corrected copies passed. |
| Skip the entire active-state group | Due to a chown permission check, the directory and size verification were also skipped. Now it is split into three items; the first two items are executed by ordinary users, with only the owner item having a skip semantics. |
| The subordinate use case may be passed off as a pass. | The original state content was written as invalid `{}`. After removing the ownership verification, it may still be rejected due to parsing failures. Now, we retain the valid document. First, verify the normal recovery, then only modify the ownership and request a recovery failure; it can be run independently and pass. |
| Five Binding file access failures | The same binary and content failed in the Windows mount directory, but passed all tests in the Linux temporary directory. The original results are saved in `binding-filesystem-diagnostic.json`, with no changes to the access assertion. |
| Two strict TLS failures | The same CA content is placed in the Linux temporary directory for processing; evidence: `tls-filesystem-diagnostic.json`. A new explicit test data directory is configured, and still checks for denied access without authorization and successful authorization. |
| WPT is not configured. | The manifest with the fixed value `1985b47aa8972a970f005957f2bfa036da1787c6` has been obtained and actual build and execution have been completed. The failure of placeholder cases has not been changed to a pass. |
| The identity matrix requires a clean tree. | Submit and execute locally in a temporary independent source code snapshot. All 6 variants and duplicate configurations have passed. The original workspace has not been submitted, saved, or pushed. The snapshot file summary, submission, and results are kept separately. |
| cgroup / Skip network namespace | Passed in independent test runs with appropriate permissions; Only used resources created for testing, original skip records retained, see `delegated-root-diagnostics.json` |

### 20.2 Tests and Implementation Fixes for This Merge Round

- `client.cc` uses a clear empty environment array on POSIX, and a double NUL empty block on Windows.
  The application environment still comes from the explicit HELLO snapshot. Linux modes have been tested on independent copies, with the red and green tests occurring in sequence.
  The Windows branch still requires native testing and cannot be replaced by Linux results.
- FD/Environment checks include two types of canaries. When `/proc/<pid>/environ` is not readable, it is no longer miscounted as empty.
  In normal mode, registration is performed separately; assertions are made after the wait limit takes effect, to avoid checking before the HELLO application.
- active-state The directory, Very large files, It is the responsibility of the host to verify the independent registration. Use cases are executed first to restore valid documents
  Positive control, negative control also require failure to occur during the corresponding application recovery phase.
- Set the `RUN_SERIAL` parameter for packaging tests to prevent the binary from being used by other tests during reconstruction;
  smoke uses `FIXTURES_REQUIRED` to successfully bind the package setup. A controlled failure experiment.
  Proof: The old `DEPENDS` still runs the consumer, and the new fixture configuration prevents the consumer from reading the old artifact.
- `CAPSID_SANDBOX_TEST_DATA_DIR` allows explicit selection of Linux native file systems, Binding, and
  Strict TLS uses the test files in this directory. The CA copies them byte by byte, and the original success/failure assertions are retained.
  This belongs to the assumptions for the test run, and does not imply that the Windows-mounted file system exhibits the same Landlock behavior.

The corrections have been applied to the workspace, and the full build has been successful. Subsequently, 105 tasks were executed: 104 succeeded, 1 failed, and zero tasks were skipped.
The only failure occurred in the comparison of execution times for `hr-time/basic.any.js`. There was a difference of approximately 678 milliseconds
between the two clocks, which exceeds the allowed tolerance of 30 milliseconds. No adjustments were made to this issue. Three independent reruns of the diagnostic
process were successful, but no changes were made to address the clock discrepancies that caused the failure. Therefore, it cannot be claimed that the root cause of the problem has been identified or that the issue has been resolved.
Evidence can be found in `followup-targeted-result.json` and `wpt-clock-diagnostics.json`.
The reconstructed domain, cgroup, and network namespace are now functional during authorized independent operations. For more details, refer to
`followup-required-root.json`. This configuration is stored separately from the settings set by the original ordinary user.
The aforementioned independent red-green evidence is located in `environment-tdd-staged*.json` and `package-dependency-red-green.json`.
The intermediate failure results are retained in these files, allowing for distinction between different testing scenarios and corresponding corrections.

### 20.3 Newly Discovered Event Compatibility Issues

After expanding the attributes and public constructor probes, the following real differences remain unresolved:

- `new ErrorEvent(type, init)` parameters include type, message, filename, lineno, colno,
  error: Identity and parameter/type conversion issues; the current implementation is still a single-parameter constructor from txiki.
- Dictionary, default values, data properties, and getter read order for `new MessageEvent(type, init)`.
- Create new events, re-deploy phase, `isTrusted` of runtime events.
- Post-distribution `currentTarget`: Lock the default behavior of workerd to retain the target, current Capsid
  Clear; and the basic use cases for the locked WPT EventTarget explicitly require clearing. Workerd
  See the locked submission for additional standard behavior under `pedantic_wpt`.
  [Event Definition](https://github.com/cloudflare/workerd/blob/8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f/src/workerd/api/basics.h).
  It is necessary to clarify the profile/compatibility contract. Do not secretly modify the reference flags or delete WPT assertions.

Details of the original results can be found in `extended-event-lifecycle.json`,
`error-event-constructor-probes.json`, and `event-extra-probes.json`. The latter files retain information
related to numerical boundaries, infinite values, BigInts, lone surrogates, and observations related to the MessageEvent dictionary. In section
19, we addressed runtime-related errors. However, this does not mean that the entire public event API is fully compatible with this framework.

The full test objectives are still in progress; these newly added gaps and the remaining items from A–I / T12–T17 remain as pending tasks.

### 20.4 Continued review of WPT test effectiveness

The following issues indicate potential problems related to false positives or incorrect attribution of results. These issues demonstrate that the testing intensity was insufficient, and it is not possible to infer the
author’s subjective intentions based solely on the results. Section 18.5 provides evidence of historical code changes related to GLM deletion checks.

| Problems | Original Behavior vs. Counterexample | Corrections and Evidence |
| --- | --- | --- |
| All known failures are released by name | Tests with the same name can be changed to different assertion, TypeError, timeout, or cleanup errors, and the original exceptions can still be borrowed. | 20 original exception binding classes, test phases, and precise first lines; the complete original diagnosis is retained, with no new exceptions added. The report indicates 8 failures and 2 passes for the old version; after correction, the result is 10/10. |
| Self-made harnesses are weaker than those from upstream. | NaN approximate comparison, empty slots in arrays, exception subclasses, and nested assertions can pass falsely; exactly does not conform to NaN/positive and negative zero. SameValue | The same input ran on the official harness and adapter, with two batches confirming 14 and 10 differences respectively. After correction, 37 items of control passed entirely, with 36 items matching the official results, and 1 item verified against the actual report phase. |
| Native results only search for successful strings. | Top-level duplicated passed fields, nested successful or invalid JSON can be checked through combinations of strings and non-zero counts. | The top-level structure checks for contradictions, missing fields, and duplicate fields. It also verifies empty failure arrays and JSON scalars. The old consumption logic triggers a red light, and the control passes after correction. |
| WPT only locks the HEAD | After modifying the selected files or resources, even after marking them as assume-unchanged, the HEAD remains unchanged, and the original checks still pass. | Build and CTest compare the actual bytes of each declared input with the locked Git blob. Resources are also checked for coverage; isolated repositories control to cover unstaged, staged, hidden modifications, and lost inputs. |
| The symbolic link startup leads to an audit that runs empty. | The CLI compares the path string in argv with the actual module path; when starting from the `/home/...` alias, the main function is not executed, and it still exits with 0. The original metadata/workerd tool and the newly added source audit are affected this round. | Four entrances have been changed to more realistic paths; two methods, direct/alias, are used to test four items each. The old version 4 failed, while 4 passed. After correction, 8/8 was used. The counterexample requires a clear error in parameters; failure due to arbitrary methods cannot be used to count. |
| The standard explanation for D010 is written backwards. | The document states that WPT is outdated, but in reality, the standard requires the queue to be initially disabled. | Lock down workerd and distribute tasks in advance for all three Capsid modes. Maintain the default compatibility behavior, change it to a clear common standard deviation, and add formal API observation; do not remove upstream assertions. |

The red-green record is located in `wpt-report-contract-*.log`, `wpt-adapter-contract-*.log`,
`wpt-adapter-additional-*.log`, `wpt-success-*.log`,
`wpt-source-integrity-controls.log`. The adapter logs the differences between the shell/self and
upstream AssertionError prototypes encountered during the initial run as diagnostic records. These records are not counted as red flags.
`message-port-queue-red.json` retains the original observations from the four backends. Initially, the script predicted that references
would not be distributed initially, and failures were assumed to occur on references. Therefore, this file cannot be
interpreted as a unique implementation of Capsid.

In the first 96/96 of `wpt-integrity-final-tests`, the original logs revealed that both the source and metadata
outputs were empty; these components did not execute the main function at that time. Therefore, these records were retained as evidence of defects at
the entry level, and not considered complete for acceptance. After correcting the four CLI entries, audits were conducted using actual alias paths. As a result, 111
input files and 20 instances of expected failures were reported. Ultimately, CTest and access control systems used
the new `audit-entry-final-*` records. When WPT was successful, a complete `WPT_REPORT` was also generated, enabling the
archiving of exceptions and diagnostics from LastTest.log.

The complete build after correction of errors has been completed. The score for `audit-entry-final-tests` is **97/97, with zero skips**.
This covers all 84 selected WPT files and audit controls. There are a total of 14,060 subtests in the report. Among these, 20 subtests involved
failed assertions due to precise matches with known conditions. Therefore, it cannot be said that all 14,060 subtests were passed.
111 upstream inputs have been verified for byte integrity. The actual outputs of the source/metadata functions have been confirmed. 84
complete reports have been archived as `wpt-raw-reports-final.json`.

After correction, the workerd process must be executed on **10/10**, with no skips. The actual execution directory
is `complete-gate-final/run-9xxXYi`. All four backend observations from the 14 incoming fixtures, along with the complete outbound
reports, passed the validation. Additionally, all 56 comparators, the standard API baseline, and four fault injections also passed the tests.
The new MessagePort observations have also been verified in four official reports. The results can be found in
`audit-entry-final-workerd-gate-result.json` and `complete-gate-final/summary.json`.

This is still not a complete list of all 512 items that have passed the CTest or release matrix checks. Issues related to differences in public event APIs, root
causes of first-time clock failures, incomplete packaging/duplicate builds, as well as those items that remain uncompleted in Section 20.3 and A–I, will remain unresolved.

## 21 2026-10-15 Public Event Testing Enhancements and Reference Contract Review <a id="section-21"></a>

Section 18.5 describes the historical differences that can indicate that certain checks were either removed or relaxed. Section 20.4 provides counterexamples that demonstrate how some
checks can be falsely deemed to be passed. Neither of these types of evidence can, on its own, provide sufficient evidence to support the author’s subjective motives. In this section, we continue to use the
same fixed workerd binary code, date values, and empty flags. The current implementation of Capsid is not taken into account as a potential source of errors.

### 21.1 Added Reproducible Test Flaws

There are 104 instances where the public constructors and lifecycle methods have similarities in terms of input parameters. In the old Capsid framework, there were 85 differences between
these instances in each of the three modes. After considering the order in which microtasks are dispatched synchronously, there are 105 such instances, with 86 differences between them.
When running independently, observe the properties related to the dispatch process and after dispatch for methods
`reportError`, MessagePort messages, unhandledrejection, and rejectionhandled. The formal driver uses 314 assertions to verify the
locking of references; this fails as expected on the old Capsid version. Details about the input, binary digest, and results
can be found in `public-events-105-red.json` and `runtime-events-expanded-red.json`. Formal
red-green baselines can be found in `runtime-mandatory-reference-red.log` and `runtime-mandatory-capsid-red.log`.

| Uncovered areas | Specific issues with the old implementation | New Assertions |
| --- | --- | --- |
| Public Event / ErrorEvent / MessageEvent / CloseEvent / CustomEvent | Missing parameters, dictionary default values, type conversion, property snapshots, number of getter calls/order, and object identity have not been fully checked. | A fixed list of 105 named cases, each item should be deeply compared with the independent reference expectations. |
| Task Lifecycle | New/Redeployment phase, trust flag, inconsistent currentTarget after distribution | Pre-deployment, During-Deployment, Post-deployment, Re-deployment, Rejection of Re-entry, and Descriptor Observation |
| Microtask checkpoints | Each listener actively clears microtasks after it, ensuring that microtasks appear before subsequent listeners. await may also encounter phase 2. | The immediate return order of synchronous dispatch, as well as the order of two listeners/microtasks of MessagePort and the phase after await. |
| MessagePort message source | The old event lacks a source; simply having the port serialized to JSON and getting `{}` is not enough to determine correctness. | Check whether the source is the sender, receiver, or a real MessagePort; refer to the receiver. |
| rejectionhandled reason | The original project test requirements require the reason to be retained, and the workerd returns undefined when locked. | Retain the original reason of unhandledrejection for the identity check, freezing the identity of the handled promise and the default reason respectively. |

Checking the source files or JavaScript copies that lack actual workers cannot replace the real-time testing process.
The first build will stop at the point where new patches conflict with existing issues; logs will be retained for this purpose. Only after a complete patch audit
is completed can the overlay summary be updated. Identity verification must remain intact.

### 21.2 Adjacent behaviors that still require independent correction

The file `event-adjacent-red.json` contains two additional differences. The reconstructed
`event-adjacent-current.json` was confirmed to be valid on all four backends. Currently, the 105 items listed do not include these differences.

- Listener of the public EventTarget is synchronously throwing errors: Refer to the original object thrown to the dispatchEvent caller.
  Stop the distribution and resume phase 0; the Capsid still calls the next listener after reporting through reportError.
  The existing p1 project tests have fixed the behavior of Capsid, and thus cannot be considered evidence of compatibility with workerd.
- The `once` listener deletes subsequent listeners and adds a new one during execution: Only the first one is called in this round.
  The next round of additions; Capsid still calls the deleted items this round. This is in contrast to the once removed items in the linked list.
  The traversal of states involves the necessary implementation of regression repairs for removal, re-addition, and nested dispatch.

These differences should not be included in the ignore fields of the comparator. Even if 105 items and the current access control mechanism are passed, it only proves that the frozen input
range has been verified. However, this does not mean that the EventTarget or the entire 0.3.0 version of the system is considered fully tested.

### 21.3 Current Implementation and Qualification Status

The modifications to the public event constructor, runtime-trustworthy distribution mechanism, micro-task checkpoints,
MessagePort source, and rejectionhandled reason have been implemented in the working environment. The first round of API testing for the actual workers was
successful, with 314 assertions passed in both reference and three CAPSID modes. In the first round of testing, 97 out of
98 WPT/audit tasks were passed, with 1 failing: the upstream assertion for currentTarget required a null value, and the
workerd was locked, resulting in the default retention of the target. Now, CAPSID-D012 serves as the standard for recording this development profile. The
complete failure scenario, where `expected null, got {}` is encountered, is retained as part of the failure report. This is not a standard implementation fix.

Fault injection expands the normal baseline by adding 9 types of errors. The first round of injection tools has two issues: strings
`handled = event` also matches unhandled, and the constructor wrapper unexpectedly outputs missing parameters.
Both uniqueness checks and specified failure assertions rejected these runs, so they were not considered as successful. After corrections, 10
independent runs were successful. Nine errors occurred in those runs, and all of them failed according to the specified assertions. For more details, see
`events-mutation-controls.json`. The complete failure records from the first round of testing are stored in
`events-complete-gate/run-JiFEeQ`. These records cannot be considered as final acceptance results.

During the first complete build, warnings were encountered regarding timestamps of dependency files that were 0.0037–0.55 seconds ahead of the actual time. Subsequent complete builds were successful, and no
such warnings were encountered. Details can be found in the `events-clock-diagnostic.json` file. However, this does not constitute a definitive explanation for
the failure in section 20.2. After the issue was corrected by reconfiguring the dependencies automatically, similar warnings occurred again.
These incidents are recorded in the `events-reconfigure-clock-warnings.json` file. It cannot be assumed that a round without
warnings indicates that the entire environment is stable.

### 21.4 The newly discovered incremental builds use old test packages

After registering D012, only standard incremental builds were performed. The generated WPT packages still contain no new entries. When running the 100-item set again,
the patch audit and negative controls were passed. However, the currentTarget test cases still fail due to outdated metadata. In this round, 99 tests were passed, and 1 test failed.
Therefore, it’s not yet possible to say that all tests have been successfully passed.

The reason is that the `tests/wpt/manifest.json` file is not automatically configured by CMake. This allows for continued use of
the old package in case of new additions/removals or expected failures. It was also confirmed that the
`docs/txiki-upgrade-baseline.json` file (including all its custom paths) also lacks such dependencies.
The newly added configurations rely on the original settings. These two missing items are clearly indicated in the old diagram; please refer
to the log file `events-reconfigure-inputs-red.log`. The issue has been resolved by using actual configuration properties to define inputs.
We continue to verify the generated dependency graph. The automatic invalidation rules are no longer applied during manual configuration processes.

The revised incremental build has been automatically reconfigured, generating a package containing D012, with the dependency regression passed.
Finally, the complete workerd system must be tested on **10/10**, with no skips allowed. The execution
directory is `events-complete-gate-corrected/run-Sfo9Fk`. All 14 inbound fixtures were tested
using four backends, four sets of outbound reports, 56 comparators, along with a normal API baseline and nine fault
injections. The complete outputs and input summaries from the fault injections are archived as `mutation-controls.json`.

**WPT and related regression rates remain at 98/99, with zero skips. Therefore, it cannot be considered a complete pass.** There are 84
WPT files, comprising 14,060 subtests. Among these, 21 cases of failed assertions have been identified, along with 1 case
where the test failed to pass. The relative elapsed time for the test `hr-time/basic.any.js` is approximately 1,990.595
milliseconds. The time taken for the test to complete is 3,258 milliseconds, which exceeds the allowed tolerance of 30 milliseconds. Three separate diagnostics
were performed, but no cause for the failure could be determined. Therefore, no assertions were removed, no tolerances were granted, no exceptions were added, and the rerun was not considered a fix.
The original 84 reports are stored as `events-wpt-raw-reports.json`. The aggregated results are presented as
`events-verification-summary.json`. The system clock-related data is contained in
`events-clock-native-diagnostic.json`. The auditing process for 39 patches and the four types of negative controls was successfully completed in a separate
step, as documented in `events-wpt-final-LastTest.log`. However, there were still failures due to old packages during this round of processing.

Section 20.4 describes the state of the numbers before any changes were made to public events. There are still two issues that need to be addressed: the failure of the
clock mechanism, and the need for 512 additional tests. Additionally, there are still unresolved issues related to matrices A–I/12–T17. Therefore, it cannot be claimed that the
testing is complete, or that the system is fully compatible or ready for release.

## 22 2026-10-15 Listener Regression Compilation and TDD Correction <a id="section-22"></a>

This section continues the discussion of the two observed differences from Section 21.2. First, we expand the independent reference inputs, then make modifications to the implementation.
We do not attempt to automatically fill in the Capsid output with the desired values, nor do we modify the dates or flags of the fixed workerd instances.

### 22.1 New Tests and Failure Evidence

The official API fixtures include 22 named listeners for EventTarget, WebSocketPair endpoints, and MessagePort.
In total, there are 66 such cases. These cases cover various aspects such as abnormal object identities, stopping distribution and phase cleanup processes, the role of object listener
receivers, adding or removing listeners during distribution, as well as readding listeners after certain events occur. They also address issues related
to repeated registration, stop flags, option handling, snapshots of handleEvent operations, cancellation semantics when
returning true, as well as the behavior of signals during callback execution, after reregistration, and when the listener stops propagating events due to user abort operations.

The default behavior of locking references includes clear standard differences: synchronous exceptions are propagated to the caller; capture and
The true value of passive is rejected; handleEvent captures during registration; stop flag is reissued across reallocation
Retain; If the callback returns true, the cancelable event will be canceled. Expected from the same input reference observation, and
[Fixing the workerd source code](https://github.com/cloudflare/workerd/blob/8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f/src/workerd/api/basics.c%2B%2B)
Cross-verification; cannot be described as a complete implementation of the DOM standard.

`listener-events-66-red.json` retains the complete text and summary of the fixture, as well as the outputs
from the four backends and binary identities. The old Capsid has 48 differences in each of the three modes. The formal testing passed with
384 assertions; the old Capsid failed first in `EventTarget/exception`. The real WebSocket C17 also identified
gaps in the native delivery mechanism: after the first listener fails, the second one is still called; the detailed log is located at
`native-listener-red.log`. The temporary drivers used for isolation and troubleshooting only consider those listeners that have failed to be golden
comparisons, and these are not counted as successful tests. The formal testing retains all the assertions.

### 22.2 Implementation Changes and Verification Boundaries

`0039-workerd-listener-contract.patch` changes the distribution to callback snapshots with real-time identity verification.
"Once removed before calling, the 'finally' phase is restored, and exceptions are propagated directly. Capturing callbacks during registration..."
Receiver, options are read in reference order only; the internal removal algorithm of AbortSignal occurs before the user abort
Listener execution, remove/once, and revoke the cleanup relationship of the old signal.

WebSocket has been changed to use a basic listener mechanism. Asynchronous return values are collected by an internal trusted party. Synchronous exceptions cause the subsequent callbacks to
stop. The existing processes for storing, discarding, and terminating channels will continue to be tested according to formal requirements such as C17. The assertion that the public dispatchEvent
should swallow any exceptions was originally required in p1. However, this requirement has been replaced with a check to ensure that the original exception
object matches the phase after dispatching. It is no longer possible to rely on project-specific behaviors to prove compatibility.

Fault injection added three cases to the original nine: swallowing listener exceptions, ignoring removals, and reading `once` twice.
The baseline must pass first. Each designated assertion must detect its own injected fault. A startup failure or timeout cannot count
as detection. The isolated JavaScript module passed 105 public-event checks and 66 listener-input checks.
The real Worker, native WebSocket and full gate still required separate verification.

All 40 patches passed the audit against clean copies before the build identity digest was updated. The real Worker
rebuilt successfully. The reference and all three Capsid modes each passed the 384 public API assertions.

### 22.3 The actual qualification results conflict with the newly exposed standards

The complete workerd gate passed **10/10 with zero skips**. The run directory was
`listener-complete-gate/run-Xlzu7O`. Reports from four backends for 14 inbound fixtures, four sets of
outbound reports, and 56 comparator controls passed. All 13 runs (one baseline and 12 fault injections)
met their expectations. Every native C17 result recorded `nativeThrowListeners` as `["first"]`.
The reference recorded 0 occurrences of its registered narrow clean-close variant.

WPT and related regressions still passed **98/99 with zero skips**, but a different check failed.
`MessagePort_initial_disabled.any.js` first registers a listener with `capture: true`.
After the fix to match the pinned default behavior, registration threw a TypeError before the queue assertions ran.
The report validator rejected the D010 exemption for `AssertionError / reached unreachable code`.
This left 1 failure and 1 old expectation that no longer matched.

We ran the same generated package on all four backends, retaining the upstream code and adapter and moving only execution
into the request context. The reference also threw a TypeError during listener registration. The raw report is
`listener-wpt-profile-conflict.json`. This establishes a conflict between the default workerd profile and the standard test input.
It gives no basis to dismiss the WPT as stale, extend the earlier exemption to TypeError, or claim that the queue assertions ran.
The failure remains open until the acceptance criteria distinguish the standard and default workerd profiles.

The 84 raw WPT reports still contained 14,060 subtests. Of these, 20 matched registered failure expectations,
1 was an unapproved failure, and the rest passed. The failure report was extracted from the complete JSON in native
error output even though it lacked the success prefix. See `listener-wpt-raw-reports.json` and
`listener-verification-summary.json`.

The HR-Time phase has been successfully completed. Out of the 2,363 system clock samples collected during this
phase, the deviation between realtime and monotonic timestamps is approximately 0.028 ms. No recurrence of the original failure was observed in this phase, so it is not possible
to conclude that the root cause has been identified. There are still warnings regarding timestamp advances during the complete build process. Issue D004 remains unresolved;
historical failures should be retained, with no tolerance or exceptions allowed. Issue D005 involves only the
verified differences in common listeners. The issue related to the upstream rejection process for Document-scoped events remains unresolved.
All 512 items, along with the complete processing of EventTarget properties, as well as the remaining matrices for
A–I / T12–T17, have not yet been fully verified.

The final metadata incremental build was successful. The host/worker summary is identical to the aforementioned gate binary.
Patch integrity, patch negative control, key normalization, automatic reconfiguration dependencies, WPT metadata and its negative control,
The document audit consists of **7/7, zero skips**. The results can be found in `listener-final-audits.json`; source code and
The binary summary can be found in `listener-source-review.json`. All changes remain in the working area and have not been committed or published.


## 23 2026-10-15 Current Test Validity Review and Missed Events Attributes <a id="section-23"></a>

The current `8028b88` HEAD has unsubmitted changes to the workspace. The workspace contains modifications related to GLM and
subsequent updates. The findings are categorized based on actual evidence; not all defects are attributed to the same author. Historical diff files indicate that some checks may
have been deleted or relaxed. This is separate from the claim that the author intentionally fabricated results. Currently, there is no evidence supporting this claim.

### 23.1 P1: Interaction between existing API gatekeeper events and common listeners

Newly added events that cover WebSocket include open/message/error/close, along with
MessagePort.message. These events support eight operations: replace, reassign, duplicate,
remove, clear, reset, once, and nonfunction. In total, there are 40 named inputs. The reference and the
three Capsid modes load exactly the same code. There are **38/40 differences between the three modes**.
This is a statistics of 40 inputs, which does not equate to 38 independent root causes. All four backends exited normally, with no forced termination.

The same set of binary files for the current host/worker will subsequently be executed using the complete workerd mechanism **10/10**, with no skipping
required. The execution directory will be `coverage-complete-gate/run-*` (specific directory details can be found in its `summary.json` file).
This result coincides with the 38 identified differences, thereby directly proving that the coverage of existing access control systems remains valid. It cannot be concluded that existing
access control systems can be successfully applied to these new inputs as well.

| Operations | Lock down workerd | Current Capsid |
| --- | --- | --- |
| First set the event properties, then addEventListener, and finally replace the property callback. | Attribute callback is still executed first. | The property callback has been moved after the regular listener. |
| The same function is registered through both properties and addEventListener. | The call is made twice each time. | Duplicate elimination should be done at once. |
| After setting the properties, call removeEventListener on the same function | Attribute callback is still executed. | The property callback has also been removed. |
| After registering the same function in two ways, set the property to null. | Ordinary listeners retained | The ordinary listener was accidentally deleted. |
| The ordinary once listener shares a function with the property callback, which is called twice. | First time, two times; second time, one time. | First time, once; second time, zero times |
| Set WebSocket properties to null first, then restore them. | Retain the original property listening positions | Move to the end |
| Reading back after assigning a non-function object to WebSocket properties | Keep this object; do not call it when distributing. | Read back null |

The reset input of MessagePort is consistent with the nonfunction input, while the other six inputs are different; WebSocket
The eight inputs of the four types of events are all different. There are 384 API assertions, and 66 ordinary listener scenarios that have not been addressed yet.
Cover these combinations. The source code is located at `js/websocket.js` for the on* setter and
The onmessage setter in `js/message-channel.js`: They directly remove/add user functions.
Sharing the registration identity with the regular listener, the positions were also rearranged when replacing attributes.

Minimum reproduction (executed in request context):

```js
const socket = new WebSocketPair()[0];
let calls = 0;
const callback = () => { calls++; };
socket.addEventListener('message', callback);
socket.onmessage = callback;
socket.dispatchEvent(new Event('message'));
// Fixed workerd: calls === 2; current Capsid: calls === 1.
```

The evidence is located in the local directory `build-v030-dev/evidence/test-integrity-tdd-20261015/`.
`coverage-handler-fixture.mjs`, `coverage-handler-probe.mjs`,
`coverage-handler-observations.json` and `coverage-handler-differences.json`.
The report includes all input parameters, reference locks, source/host/worker SHA values, and the original results from the four endpoints. This
probe examines the actual behavior of public synchronization mechanisms during operation. It has not been officially registered as a CTest, and therefore it cannot
claim to have tested all the event properties related to native network messaging mechanisms. Next, we will include these 40 inputs along with the native
message/close scenarios in the official test cases. After that, we will update the event properties accordingly. Do not attempt to change the current state of
Capsid, nor add any new compatibility exceptions.

### 23.2 P1: Registered compatibility exceptions remain actual differences

The current comparator checks the exact results at both ends of the exception fields. Therefore, no longer do entire items get skipped. However, a successful
access does not mean that all of these behaviors are the same.

- Transfer of Response with accepted endpoint: reference is 101, Capsid is 500.
- Received illegal Close code 999: Reference loopback 999, Capsid rejected with 1002.
- Synchronous 8 MiB burst: Reference completion delivery, Capsid finite queue termination.
- Server selects an unproposed subprotocol: reference accepted, Capsid rejected.
- Still dialing after HTTP completion: The reference no longer delivers completion events, while Capsid continues to complete.
- Sudden EOF after connection: The readyState/reason within the close callback remains different.

The first three items are described in detail in the field-specific exceptions and budget-related sections of `tools/workerd-reference.mjs`. The remaining three
items are described in `tools/workerd-outbound-contract.mjs`. Error messages are categorized and formatted
precisely; any additional “clean-close errors” related to references should only refer to specific, reproducible variations, while maintaining
the original event details. All of these details should be available in the compatibility documentation. It cannot be considered as fully aligned.

### 23.3 There is historical evidence of code that has indeed reduced the intensity of testing

Review and save the `continuation-codex-20261015/`
`glm-delta-tests_test_workerd_ws_fixtures.mjs.txt`, it can be seen that Node/undici is deleted,
Binary version, date/flags check, allows continue without driver, select all items
The fixture must be run, changed to a minimum of six items, delete the complete identity report, and write `assertions: 0`.
These guidelines have reduced the old acceptance criteria. It is no longer sufficient to rely solely on the number of items that pass the inspection at that time. These checks have been restored to their original state. For the current
round of independent evaluations, the negative control values for inbound/outbound comparators are set to **56/56**, with zero skipping allowed.

As mentioned in section 20.4, issues such as failed attempts to use the same name in WPT, weakened harnesses, successful
string matches, empty CLI aliases, etc., are all proven examples of cheating methods. These issues have been addressed and corrected. It’s important not to assume that these
historical problems still exist today, nor should any changes to test expectations be considered cheating. There must be clear references or contractual evidence to
support such changes, and any failure records from before the modifications should be retained.

### 23.4 Full Baseline, Targeted Fixes, and Unclosed Items

Two additional tests were added to the total of 512 tests that needed to be regenerated before testing. After excluding three cases that required independent packaging environments, 509 tests were finally run: **503
tests passed, 3 tests failed, 3 tests were skipped**. There was no timeout during the process. The complete output logs can
be found in `coverage-baseline.log/.xml` and `coverage-baseline-LastTest.log`.

| Project | Review Conclusion |
| --- | --- |
| channel_abi | The fake peer selects scenarios using environment variables, while the production spawn clears the child process environment, resulting in 64 assertion failures. It has been changed to a private launcher that explicitly passes parameters and uses independent logs for each run. The production empty environment contract is retained, with a polluted parent environment serving as a negative control. The real IPC regression has been passed. |
| worker_build_identity_matrix | The original working directory contains uncommitted changes, and the tests correctly rejected them. It has been approved after being copied to an isolated Git snapshot in the actual file bytes. The clean-tree check has not been deleted; this snapshot does not include the complete vendor submodules and cannot replace a complete packaged qualification. |
| MessagePort_initial_disabled WPT | Still throwing a TypeError when registering with capture:true, not entering the queue assertion; the same input reference also fails. The old D010 exception is not matched, so a broad exception cannot be added to eliminate the red light. |
| Three environments are skipped | The external main state directory, cgroup v2, and network namespace require corresponding operating prerequisites. Ordinary users can skip this step as it is not a pass. This round does not use the previous root's running results to verify the current binary. |
| Three unselected packaging items | The package contents, smoke, and reproducibility have not been fully executed this round, and therefore are not considered passed. |

In the original scenario for resource recycling, RSS size increased by 6 MiB after multiple rounds of processing. It’s possible that the last retired pool/factory remains active until
the next management operation. A new lifecycle assertion for precise objects/factories was added, but it still failed during isolated
tests using the old recycling algorithm. After integrating the backend’s join/release functionality, this approach works for all three scenarios: creation, adoption, and shutdown. It
doesn’t require triggering the release process during the next deploy/retire operation.

Currently, **514 items** are registered. After fixing, the 10 real directed regression tests resulted in **9 passed, 1 failed, zero skipped**.
Failure due to adding `websocket_managed_reclamation-idle`: Default environment RSS is 9,428 KiB
It has increased to 13,796 KiB and has not dropped back to the required range. Set in isolation diagnostic only
After `MALLOC_MMAP_THRESHOLD_=65536` and `MALLOC_TRIM_THRESHOLD_=65536`,
The same use case increased from 9,936 KiB to 10,088 KiB and passed. This indicates that the allocator retains memory.
Will affect RSS assertions. ** It cannot be determined solely based on RSS that the factory still exists, nor can this diagnosis replace the original assessment
Default Environment Qualification**. If the default fails, it will be retained as is. It is necessary to confirm that the actual Host object has been released and the memory budget is sufficient.
The acceptance methods. There is no increase in the threshold, retrying to cover failures, or setting failures as skips.

Additionally, attempts to reproduce the document audit failure using the source code snapshots of the unaffected artifacts have also failed. The links to the reviewed documents point to local evidence that has not
been stored in the database. These targets have been redefined to point to local artifacts, thereby preserving the functionality of the audit checks.
For cross-machine verification, it is necessary to transfer both the evidence directory and the summary, or generate them again using the appropriate commands. The path to the repository
documents can be different from the evidence used in previous runs; therefore, they are delivered along with the source code.

This round is still focused on review and fixing progress. The full testing of 0.3.0 cannot be qualified yet. Priority should be given to handling the event attributes in this section.
Combine the assertion for default environment recycling with the WPT profile boundaries, and continue with A–I / T12–T17
Binding, resource failure, second client, native, fuzz/soak, and publishing the evidence matrix.

## 24 2026-10-15 Event attribute completion and test premise correction <a id="section-24"></a>

This section continues from Section 23. In history, there are instances where checks for weaknesses and fake green colors were carried out, and there is code or negative evidence to
support these claims. However, this does not provide any insight into the author’s motives. Additionally, the issues discovered in the mixed workspace cannot be attributed solely to GLM.
The new addition in this round is expected to be executed on the locked workerd first, followed by causing the old implementation to fail, and finally fixing it.

### 24.1 259 Event attribute input reveals the blind spots of the old gate

Based on the 23rd section, with 40 probes, it has been expanded to 37 types of operations × 7 targets, resulting in **259 named inputs**:
WebSocket's open/message/error/close, MessagePort/message, EventTarget/message
And AbortSignal/abort. The old Capsid three modes each have **185 observed differences**, which are the number of failed inputs.
Not 185 independent root causes. Cover attribute assignment types, replacement/clearance/reset, common listeners with the same function identity,
Once, callback receiver, return value, getter/descriptor/delete, and modifications during delegation.

Locking the WebSocket and regular EventTarget of the default workerd uses the regular on* property reflection:
Read and call before the regular listener. MessagePort/AbortSignal use managed properties, independent
Register the identity, replace the original position, clear it and reset to generate a new registration. These actions cannot be unified
Replace with removeEventListener/addEventListener setter. Expected to pass with the same input reference output and
[Fixing the workerd source code](https://github.com/cloudflare/workerd/blob/8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f/src/workerd/api/basics.c%2B%2B)
Cross-verification; reference date remains 2026-08-20, flags are empty.

The official API driver has been added to the fixed list of 259 items with their respective observation values, and real-world data has been added accordingly.
Three message (clearing and restoring attributes) and close double registration checks. API expanded from 384 to each end
**648 Assertions**; refer to the previous implementation first, as the old implementation fails in `WebSocket/open/replace`.
When the native paths are isolated individually, the old implementation also fails in the message/clear sequence assertion. This temporary solution
is not considered a formal solution; all assertions are retained in the official solution.

`0040-workerd-event-handler-attributes.patch` implements two types of attribute semantics and isolates them
Global Event Prototype. The first real build still failed to expose `WebSocket/error/reset`: Global
onerror was originally added to the shared EventTarget.prototype, affecting ordinary instances. First, handle the actual situation…
Add the bootstrap condition to the test case, reproduce 16 failures, and then modify the global prototype. Individual Node
Module verification overlooked this initialization condition, so it can only serve as a diagnostic tool and cannot replace the actual Worker.

The original records are stored locally in `build-v030-dev/evidence/test-integrity-tdd-20261015/`:
`handlers-final-red.json`, `handlers-formal-reference-red.log`,
`handlers-formal-capsid-red.log`, `handlers-native-red.log`, and
`handlers-global-prototype-red.log`. The first native result, named `handlers-formal-single-worker-green.log`,
actually resulted in an exit code of 1. This case is reserved for diagnostic purposes only; it cannot be considered a successful outcome based on the file name.

### 24.2 Practical Verification of Current Implementation

Finally, 41 patches were replayed and audited in the original vendor’s sequence. All 52 files that were modified in the overlay process
were identical to those obtained during the clean reconstruction process. After that, the identity locks were updated. The full reconstruction was successful.
Reference and the three Capsid modes each pass 648 formal API assertions. The complete workerd must have a guaranteed minimum. Regression test, counterexample, eviction, main working tree, merge, handoff, teardown.
**10/10, no skipping**, all 14 inbound fixtures and complete outbound reports have been compared.
The comparator has 56 negative controls. During runtime, negative controls include the normal baseline plus **14 additional faults**, totaling 15 occurrences. Additionally,
there are errors related to the order of new properties being added, as well as mistakes in deleting common listeners by clearing their properties. All of these issues result in failed assertions.
Start-up errors, timeouts, or other assertion failures cannot be considered valid fault detections.

This round of gate is `handlers-complete-gate/run-5PCp6l`, summarized as
`handlers-complete-gate/summary.json`. The negative control original report is for this running directory.
`mutation-controls.json`. reference clean-close has registered a narrow range variant with a current count of 0.

The final Host SHA256 is
`3e58ed3805f234b3bc4ab6bb11c6f4cdf39cd129abbfc2403afda3e3eed96bc5`,
Worker SHA256 is
`ef78d7ff1b3931e24b9794f20f15ed43f29e0aaf97f4830953b1ef3c1d4b3c36`.
The build log still contains clock skew and warnings regarding existing GNU-stack links; no environmental issues have been declared to have been resolved based on this information.

### 24.3 RSS assertions must distinguish between object survival and allocator cache

Section 23: Default RSS failures are retained. The C++ precise pool/factory lifecycle assertions continue to serve as evidence for object release;
the new `reclamation-idle` integration scenario now explicitly utilizes glibc’s mechanisms for object recycling.
`MALLOC_MMAP_THRESHOLD_=65536`, `MALLOC_TRIM_THRESHOLD_=65536`,
Used to inspect large-scale retirement allocations for factories. The default allocator remains in place for replacement testing, and the default environment is retained.
Neither the RSS threshold has been increased, nor is it claimed that the default allocator, other libc functions, or
sanitizers will return memory to the operating system in the same manner.

To verify that this assumption does not conceal any leaks, we recompiled the code such that the recycling queue is only retained
for those isolated Host variants during shutdown operations. The same assertion was run under the same conditions:
with the same Worker and allocator parameters. The results showed that the normal Host RSS values were 9,872 → 14,084 → 10,024 KiB,
indicating success. However, for the variant with retained objects, the values were 10,624 → 14,840 → 12,816 KiB, indicating failure in the
specified idle recycling assertion. Both variants had their thread counts reduced to 9. The first variant failed due to inconsistent Host/Worker configurations,
but this did not count as a negative result. Only after recompiling and linking the code did we obtain
valid results. See `reclamation-allocator-final-controls.json` for details of the compilation/linking process.

These assigner parameters are only passed to the test Host; the empty environment contract for the production Worker remains unchanged.
This native variant is currently isolated evidence for reproduction and has not become a negative control for automatic build in each CI process; it cannot be written as
Continuous integration has covered this variant. The subsequent native/sanitizer matrix should be qualified for precise object release.
And apply to the footprint of this allocator, to avoid misjudging the cached instrumented allocator as a leak.

### 24.4 Full Recheck and Remaining Matters

Currently, **514 items** are registered. After excluding three items related to package contents/smoke/reproducibility, 511 items were executed serially. As a result,
**506 tasks were successful, 2 failed, 3 were skipped, and there was no timeout occurrence**. The total execution time amounted to approximately 618 seconds.
The original results are `handlers-full-summary.json`, `handlers-full.log/.xml`, and `handlers-full-LastTest.log`.
These results will not be rechecked later on; therefore, there is no need to modify these files anymore.

| Current results | Explanation and Additional Verification |
| --- | --- |
| MessagePort_initial_disabled failed | capture:true Throws a TypeError, as the old D010 AssertionError exceptions were not matched; the same applies to the input reference. The 14,060 subtests in 84 WPT files include 20 exact known failures, 1 unallowed failure, and 1 old expectation that did not occur, which cannot be considered as a standard pass. |
| worker_build_identity_matrix failed | The original working directory was not submitted, and the tests were correctly rejected. After copying and verifying 4,279 source files from the independent clean snapshot `0b97b58b6b6e331d239c53aa637255fc6e0ccca5`, the process was successful. The source code was not submitted or reset in the original working directory. This snapshot does not include the vendor sub-module and is only used for the identity matrix of worker OFF. It cannot replace the packaged qualification. |
| Three ordinary users can skip ahead. | External main state directory, cgroup v2, network namespace; After full completion, run separately in the root environment using the same Host/Worker and current testing program. Passed on 3/3 occasions. Record as `handlers-required-root.json`. Do not change the setting from skip to pass for regular users. |
| Recycling and channel ABI | The new precise object recycling and idle RSS scenarios with a clear allocator premise have been approved. The channel ABI scenario selection no longer relies on the environment variables cleared by the produced spawn. |
| Three items not selected for packaging testing | Not executed in this round, continue listed as not qualified |

This round of HR-Time checks was passed. However, during this period, 121,178 system clock samples revealed 14 instances of significant
time discrepancies exceeding 1 millisecond in duration. These discrepancies totaled approximately 20.935 seconds, with the largest
single discrepancy reaching 1.781 seconds. The maximum sample window for each group was around 0.154 milliseconds. This is a direct indication of clock
inconsistencies in the current environment. Therefore, it cannot be concluded that the clock-related issues have been resolved. It is also impossible to determine the exact causes of past
failures based on this data. The original samples and summaries are stored in the `handlers-full-clocks.csv` and
`handlers-clocks-summary.json` files. The verification on independent platforms where the clocks are stable has not yet been completed.

The registered workerd discrepancies still exist as described in section 23.2. Issues related to Binding,
resource failures, second clients, native methods, fuzz/soak, and complete release evidence from A–I/T12–T17 have not been
fully resolved. Currently, the system checks the exit status of CTest during fault injection processes. However, it is still necessary to independently verify the fixed case lists and binary identities
mentioned in the archived control reports. During this round of testing, 15 manual verifications were conducted. Therefore, it cannot be
claimed that the verification process has been automated.

## 25 2026-10-15 Gate Report Verification and Old Summary Invalidation <a id="section-25"></a>

The negative control report discrepancies identified in Section 24.4 have been corrected using TDD. This round of modifications only involves changes to testing procedures, reporting methods, and documentation.
The Host/Worker binaries remain unchanged compared to those in Section 24. The current round of checks does not replace the comprehensive results from the previous round.

### 25.1 First, confirm that missing reports will still be accepted

Translate the original door access archive read logic into `validateGateRun`. Do not change the behavior; then construct a normal scenario.
Complete report and individual changes. The normal report is approved, but there are missing documents, fewer failures, duplicate failures, different binaries.
**All 37 counterexamples for old running identifiers, error assertions, timeouts/signals, and non-execution of baselines were overlooked.**
At that time, there were 94 control measures in place. Of these, 57 were successful, and 37 failed. The failures were all due to attempts to reject certain actions that actually should have been rejected. Please refer to the local
evidence file `mutation-archive-red.log/.json` for more details. The red lights were not caused by importing functions that actually do not exist.

Added `tools/workerd-mutation-contract.mjs` to fix the order of 14 faults added to the normal baseline
List and goal assertions. The gate is now actually reading `mutation-controls.json`, checking the run identifier,
Node/undici, reference locks, all fixtures, original and shifted drivers, controller, verification modules,
Host/Worker Summary. All faults must be resolved using “exit”. 1 Hit the specified assertion. signal/error It is necessary to clarify this clearly.
null. Variant input summaries should not be reused. The baseline retains the original driver report, executing 648 API calls.
It is asserted that the observed values are consistent with the normal API results of this gate, and the exit is normal.

These are checks to prevent the omission or misplacement of evidence, and they cannot be described as signatures to prevent malicious forgery.
This round also implements 9 negative controls for each individual field in the actual and complete archives. As usual, the normal archiving process is allowed to proceed, but all nine requests are rejected. Please refer to the
`mutation-archive-real-controls.json` file for more details.

### 25.2 The full run exposed the verifier's own flawed assumptions

The first real gate's 10 CTests passed, but the new verifier incorrectly required an API-only baseline.
`complete` is true, resulting in a failed final summary. The original driver only executed the API in 14 fixtures.
One, correctly reports `complete:false`; the synthetic positive examples from the initial control test have incorrectly recorded this value.
This failure is saved in `mutation-archive-complete-gate/run-KBL3jq`, and cannot be considered a complete pass for the gate.

First, we corrected the synthetic inputs and addressed the issue of “API subsets pretending to be the entire collection”. As a result, we received a result of 0/2 red lights. Then, we allowed the verifier to perform the verification based on
the actual range of the baseline. The complete access control system still requires all 14 fixtures to be present; no changes were made to the
integrity markers to eliminate any failures. After the range was corrected, the `mutation-archive-corrected-gate/run-ujoWRI` passed
with 10/10. The actual baseline remains set to `complete:false`.

Simulate the five reported issues of the main comparator: missing/incorrect reference schema, incorrect candidates
Both schema errors and missing parity statements are acceptable. After correction, it is required to clarify the schema, and at the field level
Registered differences will also make `reference_parity` false. The API-only report originally overlooked this.
The 101/500 discrepancy in the accepted-endpoint was incorrectly claimed to be consistent; this discrepancy and the appropriate markings are now retained.
These corrections do not eliminate actual compatibility differences, nor do they add any exceptions.

### 25.3 Failed attempts should not leave behind the old successful summary

In the new implementation, the old summary is written first, ensuring success. Then, attempts to perform the operation fail due to a lack of reference data during the preflight phase. The old summary still retains
the attribute `complete:true`. As expected, these attempts fail, as indicated in the file `mutation-archive-stale-summary-red.log/.json`.
Now, every time a new attempt is made, it first records the current status and incomplete data.
Only after success does the data marked as “passed” get written to the file. Any attempts that fail, including those related to preflight operations, are recorded as “failed”.
The historical run directory is retained, and the current summary cannot claim to be a successful outcome of this operation.

Final comparator/report control **102/102, zero skips**, plus direct and symbolic link CLI entries are
**110/110, zero skips**. The final complete gate **10/10, zero skips**, the directory is
`mutation-archive-final-gate/run-bsKDQp`; 648 API assertions passed in both reference and three-mode scenarios.
The normal baseline and reports of 14 faults have been verified by the gate, and the narrow variant with reference clean-close has been passed this round.
Count is 0. Commands and results can be found in `mutation-archive-final-gate-result.json`.
`mutation-archive-final-cli-controls.json`. The local evidence root directory remains
`build-v030-dev/evidence/test-integrity-tdd-20261015/`.

The document has been updated to correct the outdated statements such as "only 11 fixtures" and "Capsid is still waiting for the T10 backend".
In the previous round, 506 tasks were successfully completed, 2 failed, and 3 were skipped and continued as their original snapshots. It is not possible to combine the numbers of tasks in this round to
obtain a new overall result. Next, we will perform three independent checks for clean builds. Additionally, we will address any remaining issues related to normal lifecycle
differences, as well as those related to A–I/T12–T17. Issues such as WPT profile conflicts, clocking issues, resource failures, second
client issues, and issues related to native code and fuzz/soak still need to be addressed.


## 26 2026-10-16 Test weakening evidence, package missed checks, and perform full recheck on Release. <a id="section-26"></a>

### 26.1 Review Conclusions and Attribution Boundaries

There are reproducable test weaknesses and false positives, which cannot be used to infer that the author intentionally cheated. The GLM saved in Section 23.3
Historical increments have indeed removed Node/undici, reference binary versions, and complete identity checks, allowing for the absence of native components.
When driving, continue, and change the required execution of all fixtures to at least six, and write `assertions: 0`.
These old issues have been rechecked, and it cannot be claimed that they still exist in the current state. The current workspace combines
elements from GLM with modifications made later on. Any new issues or defects found during this process are recorded based on their actual origins; they cannot be attributed solely to GLM.

The fact that all aspects of the workerd system are functional does not necessarily indicate full compatibility. There are still six aspects of differences that need
to be addressed: transferring responses to accepted endpoints, receiving incorrect close codes, limited queue behavior in case of major synchronization issues, unrequested subprotocols,
continuing to dial after HTTP completion, and issues related to the state and reasons for callbacks after abnormal EOF situations.
Among these, issues related to the normal lifecycle and endpoint transitions still require careful handling to ensure compatibility. For detailed information and reference
configurations, please refer to Section 23.2, as well as the files `tools/workerd-reference.mjs` and
`tools/workerd-outbound-contract.mjs`. No changes have been made to these comparators, nor have any new exceptions been introduced in this round of updates.

### 26.2 P1: The actual error SBOM still passes the full smoke test

Use the properly constructed real archive generated using the clean Release/LTO process as the base baseline. Only the four categories of SBOM values need to be modified: dependency
names, dependency versions, file SHA256 values, and the entry for deleting a file. Both the normal package and the four problematic packages have completed the old version
of the test suite. All tests, including those related to C/C++ linking, Worker requests, Host requests, and normal exit conditions, have been successfully executed. The exit code for all tests is 0.
The old assertion only checks the SPDX version, the number of packages, and the document name. It cannot determine the identity of the files or their
integrity. The original evidence consists of the `package-smoke-sbom-red.json` file and the corresponding logs.

Add `cmake/ValidatePackageMetadata.cmake`, which compares component names/versions with source code and dependency locks.
Verify the build-info corresponding to the identity, overlay, complete list of files, as well as the actual SHA256, unique SPDXID, and
Relationship set; smoke and the products of two builds all call it. The baseline package must pass first, and an error exit is also required.
Hit the specified assertion; any startup failure cannot be considered as a pass. The four copies with the same byte fault are tested normally when archived.
Package exit 0, all four variants meet their respective SBOM assertions, see `package-smoke-sbom-green.json`.

The parameter `worker_package_metadata_controls` has been officially added. This parameter utilizes the actual CPack output,
comprising a total of **19** items. There are three positive cases: normal baseline settings, adjustments to the array order, and modifications to the script invocation methods used
in older scripts. Additionally, there are 16 cases involving names, versions, summaries, lists, identities, and relationships that have failed to pass the test. Currently, all
of these cases have been successfully verified. This parameter relies on the successful `capsid_package` CTest fixture. During the first
integration test, it was found that the CMake policy was not set correctly in the script, resulting in an IN_LIST parsing failure.
This issue is recorded in the file `package-smoke-*-integration-policy-failed.*`. Only after fixing the script version did the integration test succeed. Subsequent
builds also revealed that the same issue persisted. Initially, 18 tests were passed, with 1 failing. After adjusting the module settings and restoring the policy, all invocation
methods became usable, resulting in 19/19 tests passing. The failure during the second integration test was also recorded in the file `package-fixed-ctest.xml`.

This verification focuses on determining the correspondence between the four components declared in the specifications and the actual files involved. It does not cover the entire SPDX
specification, nor does it ensure that all dependencies and licenses have been properly managed. The list of components passed along by the original generator still requires further review.

### 26.3 P1: Duplicate builds compared to the actual differences and an empty list that was overlooked

The original comparator used `string(APPEND CAPSID_REPRO_DIFFS ...)` to modify the output path variable, resulting in the binary differences
not being written to the file. Negative examples also demonstrate that situations where both parties have missing/duplicate/empty identity fields, empty manifests,
headers with changed columns, additional files not listed, incorrect sizes/digests, and duplicate entries may be overlooked.
After retaining the red light, it is converted into the actual `file(APPEND ...)` format. The list of complete files, their sizes, and summaries is verified for each package.
Finally, the file name is compared with the specified identity. The comparison process is controlled by **18/18**, as
described in `package-repro-controls-final.json`. The three new controls ensure that missing or duplicate buildIds
are detected, and any changes in the canonical provenance are also identified.

During the first real-time full-scale execution, we also identified errors related to the new checks we implemented: The canonical
records for build-info contain duplicate fields such as buildId and compatibilityId. In the positive examples, these fields are not included, resulting in incorrect
judgments regarding duplicate headers in the actual packages. First, we ensured that the positive examples had a real-time segmentation format, resulting in five red flags. Then, we required that all
top-level headers be unique, while retaining the entire build-info data for comparison. Both real-world outputs passed the test successfully. The identities of the 24 files matched the
specified criteria, and the binary summaries of the Worker and static libraries showed no differences. All relevant information has been recorded accordingly.
This project currently accepts and records such differences in the toolchain, and the test cannot be described as bit-for-bit reproducible with success.

### 26.4 Clean Release Full Results and Unclosed Items

The isolated source code snapshot `c55872b4bf9c6f77809334812bae70c80112037c` contains the actual working environment content,
The vendor module has been initialized and its dependencies are locked; the original workspace HEAD remains at `8028b88`, with no commits or clean-ups performed.
User modifications. After the Release/LTO build, the results for all **515 items, zero exclusions** are as follows:
**508 passed, 4 failed, 3 skipped**, see `package-release-full-summary.json`, JUnit, and
Original LastTest log. The internal version is still the current 0.2.2 of the project, and it cannot be described as a released 0.3.0.

| Non-pass items | Original observations and current status |
| --- | --- |
| promise-rejection-events WPT | Two delayed-handling sub-items require the original value of rejectionhandled.reason to be retained. The fixed workerd and Capsid have the same input common event observation, which is undefined. Firstly, there is a conflict between WPT and the target of the selected profile. There are also undetected omissions of non-mandatory events, as described in 26.5; expectations have not been changed. |
| MessagePort_initial_disabled WPT | capture:true is rejected as a TypeError, no entry into the original queue assertion, the old D010 expectation was not met; by default, workerd also rejects this input, which is a conflict with the profile and WPT scope, and it is not relaxed to allow any failure |
| hr-time/basic WPT | The performance improved by 1996.28 ms, while the Date field’s improvement was 3836 ms. During the same period, the independent system clock samples recorded a realtime/monotonic shift of approximately 1839.92 ms. However, these changes were not properly correlated based on precise start and end times. Therefore, it’s impossible to determine the root cause based solely on these numerical differences. |
| worker_package_reproducibility | The new inspection miscalculated the canonical field. For fixes and comparison with the actual product, see 26.3. A full failure cannot be recorded as a pass. |
| Skip three environments | External parent state directory, cgroup v2, network namespace. This is not a prerequisite for a regular user environment; it is not considered a pass. |

For the same release binary, the entire workerd process was executed **10/10**, with no skipping. The results were archived
as `package-release-workerd-gate/run-YHWLiP`. Normal baseline tests, along with 14 additional fault reports, were
also verified. In each of the three Capsid modes, 648 API assertions were performed. The corresponding Host/Worker SHA256 values are as follows:
`8d1ba445346fddfac663154c1090f381c7438e7720ea65e0365a9a54b746ee62`,
`2d1997ec0af2ef963a373e5b246bafa1973d690887b092035121d652bbdbc51a`.

Packaging correction for the first clean sub-snapshot `0db02c5f4e5ae242b763d53916b432c06fff5537`
Result: 5/6. Failure indicates an omission in the integration of the above policy. Please create a clean sub-snapshot after correction.
`13b71301fe98fafdf270c5ed6243a1364051c0e8`, targeted six items **6/6, zero skips**, see
`package-final-progress.json`, `package-final-ctest.xml`. This snapshot registers 516 items.
Compare the control of 18 items and the control of metadata of 19 items. It is not possible to add the targeted results to the previous 515 items. All items must be considered individually.
516 items passed with full marks.

Ultimately, the binary of this sub-snapshot is stored in the directory `package-final-workerd-gate/run-LBXtfD`. The
SHA256 hash of the host is: **10/10, zero skips**.
`70a91bff3d244e2e8a1d08a5ee1bb912f2925bddfa2998210068852f48046956`,
The Worker SHA256 hash value is the same as that of the aforementioned Release snapshot. These three environments use this binary code to
perform additional tests under root conditions **3/3, zero skipped**. For more details, refer to `package-final-root.json`. This does not affect the three scenarios
that require full execution by ordinary users. All modifications remain in the original workspace; only the temporarily isolated snapshots are considered for submission.

All local evidence is still located in `build-v030-dev/evidence/test-integrity-tdd-20261015/`.
A–I / T12–T17 Common Lifecycle, Binding, Resource Failure, Second Independent Client, Native,
sanitizer/fuzz/soak: Performance and final release evidence still need to be completed step by step. No overall acceptance conclusion can be drawn at this stage.


### 26.5 P1: No mandatory target events occurred for the two upstream WPT sub-items

Further examination of the two delayed-handling sections in the upstream `promise-rejection-events.js` file
Example: The `rejectionhandled` callback contains a reason assertion, but another timer will call directly.
`t.done()` does not check whether the callback has been executed. Previously, all 36 sub-items in the development build report were passed.
The Release build failed within two callbacks; two records cannot be used to infer that the old version must have reached these assertions.

To verify for omissions, select two original async_test entities and isolate them, keeping their source code unchanged, using the current actual
Worker Execution: During normal event listening, both use cases fail due to reasons that do not meet the expectations of the WPT;
Both cases reported PASS, only when the external wrapper prevents the registration of the `rejectionhandled` listener.
Process exited with code 0. Evidence is located in `wpt-handled-presence-probe/`, save upstream SHA, select list,
Wrapper, packaged code, Worker SHA, and two sets of original results. The first packaging was due to an incomplete Node PATH.
Failed logs are also retained and not included in the above running conclusions.

These are two examples of targeted diagnostics. They do not represent the entire 36-step process. They also do not indicate the specific order of execution for those that were previously
approved. Nor do they reflect any differences between the old and new versions of Capsid/workerd. In the current complete differential archive, all events with the attribute `runtimeEvents.handled`
has values of `reasonIdentity:false` or `reasonType:undefined`. The official differential API only processes events that have been handled.
Events that lack this attribute will fail due to time constraints. Therefore, it is not possible to extend this oversight to all events being tested.

The following aspects need to be addressed separately: determining the correspondence between the profiles of WPTs; providing sufficient evidence to support the occurrence of the target events;
maintaining independent references for each workerd. It is not advisable to change the reason associated with these WPTs to a value that differs
from the fixed reference value, in order to make both WPTs eligible for approval. Nor should the entire WPT file be marked as allowing any type of failures.


## 27 2026-10-16 WPT Target Event Strong Assertions and Precise Clock Diagnosis <a id="section-27"></a>

### 27.1 Retain the original assertion and first prove that no events will pass either.

This section continues from 26.5. It does not expand the expectedFailures, nor does it change the default workerd event behavior of Capsid.
First, two instances of the upstream async_test mechanism were disabled and verified using controllable event sources. All events were deemed valid. Missing, irrelevant
Promise-related events, delayed events, and duplicate handled events also managed to pass the verification. In total, 9 conditions were evaluated. The old approach
resulted in **3 successful cases and 6 failed cases**. Among these, four cases involved errors in event distribution, while the other two issues concerned layout/insertion
checks. Valid results can be seen in the file `rejection-presence-controls-red-fixed-harness.json/.log`.
The initial diagnostic model incorrectly allowed the use of fake Promises during the harness completion process, resulting in an incomplete execution of the process. As a result,
this process was terminated, and thus does not count as a valid red light indication. Only after correcting the model did we obtain the aforementioned evidence.

Add `tests/wpt/guard-rejection-events.mjs`, inserting matching Promises for these two named cases only
The callback count and cleanup assertions require exactly one occurrence. The upstream statements remain intact; these declarations are removed.
Inserted can restore the entire original file byte by byte. Generation will be rejected if the target is missing, duplicated, or the layout has changed. Duplication of runs is not allowed.
Generators may also fail to prevent silent installation or redundant reinforcement. Inspection is carried out during the cleanup phase, and borrowing from the original source is not allowed.
Release the expected-failure entries during the test phase.

9/9 The control has been successfully implemented. It covers normal, missing, irrelevant, delayed, and duplicate events. However, attempts to use the original
assertions for “reason” still fail. Layout changes and byte restoration are also handled by this mechanism. CMake’s formal build process uses this
output. The manifest clearly defines the boundaries of augmentation, tools, two sub-items, and reserved statements. Metadata-related controls also cover any missing or misconfigured elements.
This belongs to the upstream run after adding observation assertions, and does not claim that the pristine WPT or complete HTML standard has been passed.

### 27.2 Real Worker Counterexamples and Cleanup Report Corrections

The official `wpt_rejection_presence_runtime_controls` mechanism utilizes the actual Worker process, the actual C++ driver, locks esbuild,
and the source code from the upstream project. These four combinations are executed simultaneously, with detailed reports kept for analysis.

| Input | Results | Controlling the Proven Facts |
| --- | --- | --- |
| Original subject, normal monitoring | exit 1, two original reason assertions failed | By default, there are still conflicts between the workerd profile and the WPT expectations. |
| The original subject, preventing the handled listener from being registered | exit 0, both sub-items PASS | There are indeed paths in the old test cases where the system passes even without observed events. |
| Enhance the main body, normal monitoring | exit 1, original reason assertion still fails, phase=test | No deletion or bypass of the original assertion is performed. |
| Enhance the main body, prevent the handled listener from registering | exit 1, two specified callback-count assertions failed, phase=cleanup | The same missing event can now be detected. |

Another issue with the first round of real-time control exposure relates to the adapter itself: QuickJS’s Error.stack only contains information about the call stack.
The cleanup report only includes the stack data, resulting in the loss of information regarding AssertionError.message. This makes it impossible to accurately diagnose the problem.
First, complete the control of the simulated frame-only stack, **37 passed, 1 failed**; then proceed as if it were an ordinary assertion failure, simultaneously
Retain "message" and "stack", ultimately the adapter controls **38/38**, and also locks the upstream harness
The execution process also involves comparing existing statements. No specified diagnostic checks have been removed, thereby preventing the native control elements from becoming green.

The final native four combinations all met their respective expectations, as seen in `rejection-presence-runtime-final.json`.
These are negative controls; it is not possible to describe a combination that intentionally aims for failure as having passed all upstream WPTs.

### 27.3 Formal WPT Regression and Unresolved Profile Conflicts

Only source code changes are tested. The isolated snapshot `353aea020dba5fca186e567dace9ecbbcd0d0438` has been successfully configured.
Build test-worker-integration and capsid-worker, register **518 items**. Host/Worker SHA and
Section 26: Final Binary Consistency; No Re-claim of Clean Release Builds for the Entire Product or All 518 Qualifications in This Round.

First, execute the 8 controls related to the conformance label. **8/8, zero skips**. This label does not include all the worker_wpt files, so the
value `^(worker_wpt_|wpt_)` is explicitly selected. This results in the execution of **96 actions**, of which **84 upstream
files** were processed: **94 passed, 2 failed, zero skips**. Commands, registrations, JUnit tests, and original logs are stored in the
`rejection-wpt-full-*` files. HR-Time passed this time, but there were still two failures.

- The rejection support document contains 36 sub-items. Two original reason assertions failed, phase=test. New callback added.
  Check that there are no omissions, and no new exceptions have been added.
- MessagePort_initial_disabled throws a TypeError when registered with capture:true; old D010 queue assertion
  Not yet reached, cannot be released with the same name or as a generic exception.

The currently selected default workerd profile conflicts with these standards. It is necessary to clearly define the boundaries between testing hierarchical structures
and versioning profiles. A strong compatibility goal cannot be achieved by changing the product to a version that differs from the original reference.

### 27.4 Capturing the same process window that failed in HR-Time

Use the unchanged HR-Time fixture with the same actual Worker for up to 20 bounded diagnostics. For each process, record
the monotonic time at the start and end of each iteration. Independent threads are sampled every 2 ms for real-time, monotonic, and raw clock data.
All original outputs are retained. This diagnostic method does not follow the CTest retry and release strategy.

The first six windows passed without any changes. The seventh window failed: the increase in Date minus performance was
**1972.973904 ms**. The single system realtime/monotonic deviation within the process window was **1973.158120
ms**, with a difference of **0.184216 ms** between the two values. The duration of each change sample was only 0.791
us. See `clock-window-summary.json`, `clock-window-samples.csv`, and the individual logs for more details.
This provides direct evidence for this failure. It doesn’t mean that all previous failures have been resolved, nor does
it indicate that the WSL clock is now corrected. The original assertions, 30 ms tolerance, and failure records remain intact.

D004 indicates that the updates have been applied based on the evidence collected this time. It is still necessary to perform thorough testing in
environments with stable clocks. The remaining items—A–I, T12–T17—continue to be considered within
the target scope. No conclusion regarding the completion of all tests can be drawn at this stage.


## 28 2026-10-16 Exception EOF Lifecycle Completion and Context Regression <a id="section-28"></a>

### 28.1 The six groups of real inputs first check the reference, and then generate red lights.

This round continues from the ordinary lifecycle differences, without expanding the comparator. Reading the fixed release
[WebSocket implementation](https://github.com/cloudflare/workerd/blob/8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f/src/workerd/api/web-socket.c%2B%2B),
Using the same native TCP peer and the same worker bundle, probe in reference mode and three types of Capsid modes
Exception EOF: Observation Only, Send Within Callback, Close Within Callback, Send After Callback, Close After Callback, Eviction After Callback
send+close. Raw observations are stored in `abrupt-lifecycle-*-observations.json`; these probes are explicit
Mark as diagnosticOnly, and it cannot be considered as having completed the entire outbound gate.

Six groups of references show consistency: close callback with readyState=1, and the next microtask and timer are 2.
code=1006, wasClean=false, reason is `WebSocket disconnected without sending Close frame.`.
After the send/close phase completes the parameter conversion, the operation is ignored, returning undefined, without changing the state.
The old Capsid's three modes all directly become 3, with different reasons, and send still throws a TypeError.

Add these six groups of observations to the official outbound runner. Also, change the status and reason of basic exception cases from exceptions to direct
assertions. First, run the old binary: all references pass successfully. However, Capsid fails to meet the specified EOF assertions
in all three modes. See `disconnect-native-red.json` for details. The old Worker copy `/tmp/capsid-disconnect-old-worker-20261016`
is still retained, along with the source code before modifications were made.

### 28.2 Modifications to the direction status and method behavior, with the context constraint retained

Native channel cancel now distinguishes between connection interruptions and other reasons for cancellation based on a structured terminal reason.
Only kCancelDisconnect triggers a separate internal disconnect event. JavaScript occurs after the synchronous close is dispatched,
Update incomingClosed before microtask checkpoints, mark transmission destruction as disconnected. Ignore sending.
The conversion of parameters for 'send' and the validation of parameters for 'close' are still executed. Resource limits, timeouts,
Protocol errors and connection failures do not borrow branches with EOF.

The first version of the six new assertions has been successfully tested in the three-mode environment. However, the attempt to restore the original `worker_websocket` failed. The
`worker_outbound_websocket` succeeded, resulting in a score of 1/2. See `disconnect-core-ctest.xml` for more details.
The reason is that the newly added ignored sending path bypasses the final state context check of the old message callback. No modifications were made.
Instead of using C++ assertions to eliminate failures, we added an internal bridge that only verifies the origin/lifetime of requests.
A verification is also performed on the destroyed send/close fast path. The Binding heap still prohibits this bridge.
The two original C++ use cases will be followed **2/2, zero skips**, see `disconnect-context-core.xml`.

The three Host modes have once again completed the entire outbound runner, with **172 calls and 31 types of scenarios**; reference
For **171 calls, 30 scenarios**, the difference still falls within the CapSid's own dialing period. All new observations and
Direct comparison of method behaviors, removing the `abrupt-close-state-and-reason` exception; only unrequested items remain in the outbound phase
There are still two types of registered differences between subprotocol and HTTP completion, with dialing still occurring.

### 28.3 The comparator must also be able to reject erroneous observations

The compressed samples are updated using this set of actual reference/three-mode reports. The source of the reports and their SHA records are recorded in the
fixture provenance. Only 140 instances of repeated bad-Accept inputs are compressed. During testing, the results are expanded accurately, with
no fictional binary results involved. Additionally, 16 new negative control scenarios have been added, along with old reasons for errors, missing observations,
zero or repeated callbacks, repeated events, premature/late state transitions, errors in send operations, changed states during close operations, missing methods, and incorrect
return values. Together with the six sets of actual positive examples, the comparator/reporting mechanism results in **119/119, with zero skips**.
The correctness of the sample is also verified by a real runner and subsequent complete gate validation, and is not self-verified by static JSON.

This round did not modify the ABI or wire format, nor did it replace the expected value of the originally canceled test cases. The original workspace
HEAD remains at `8028b88`; code modifications have not been submitted or pushed. The following continues to synchronize the isolated source code
Clean snapshot builds all targets and performs a complete CTest and complete workerd gate. The qualification results are local.
`disconnect-full-*` and `disconnect-final-gate/` shall be considered based on the results of this section, and cannot be marked as fully green in advance.

The pending-after-HTTP state in the regular lifecycle, the acceptance of endpoint transfers, as well as issues related to WPT profiles, Binding problems,
resource failures, second clients, native methods, dynamic behavior, and release evidence matrices, have still not been resolved.


### 28.4 Complete Build, Full Testing, and Final Archiving Gate <a id="section-284"></a>

After synchronizing all the source files from the original workspace to the separate, clean snapshot
`994be005fca7f5c0fd53496e85da3e062faf3320`, the entire build process for Release/LTO targets was successfully completed.
The total number of items is **518**. With no exclusions, the final results are **512 successes, 3 failures, and 3 skipped tasks**.
Details can be found in `disconnect-full-summary.json`, `disconnect-full.xml`, and the original LastTest logs.
Three failures were reported: rejection support issues, MessagePort_initial_disabled, and HR-Time. No retries were attempted
to replace the current results. The assertions, tolerances, or exceptions related to WPT remain unchanged. There are 84 WPT files, comprising 14,060 individual items. Among these, 20 instances of failures
have been identified, along with 4 unexpected failures. Additionally, 1 case involves an item that does not meet the expected conditions. Statistics are based on the complete log files; no
forced parsing using JUnit is performed, as this might lead to incomplete results. Independent clock samples from the entire process period have also been preserved.

During this complete execution, all six aspects related to installation, packaging, comparison, and duplicate construction were successfully handled. The differences between binary versions
are still recorded in the actual files, according to the current contract. These differences cannot be reproduced at the bit level. All existing test cases
involving HTTP, protocols, Binding RPC, context, and final states have been passed. This means that all new planned improvements in those areas have been implemented.

The same binary was executed for the entire workerd process on **10/10**, with no skipping incidents. The
directory is `disconnect-final-gate/run-39wYpr`. There are 648 assertions for each of the four endpoints. Normal API
baseline conditions and 14 failure reports have been approved by the gatekeeper. Outgoing calls amounted to 171 references for the reference endpoint and 172 references
for the Capsid endpoint. All EOF states and reasons were compared directly. Only those differences between the two endpoints were recorded and archived.
This reference clean-close narrow-range variant has a count of 0.

Among the three environments included in the full set, skipping the use of the same binary is allowed under the root condition. This is confirmed by **3/3**, with
zero skips. For more details, refer to `disconnect-qualified-root.json`. This does not affect the skip behavior when running as ordinary users.
The final Host SHA256 value is `b59ceb82e76251f9435a609347f74595396f1efda83e0eeb8aa3fd5e168864a1`,
while the Worker SHA256 value is `eaa00ea8c300ca38a114c1a67a3780ed5486c3f13eb84fb3d3a10d3d1c40efe1`.
The build still contains warnings related to GNU-stack and read return values. No claims are made regarding the absence of such warnings.

No CMake, MSVC/clang, or Ninja tools were found in the Windows PATH environment. This can be confirmed
by the `native-windows-tools.json` file. However, this does not indicate that the compiler is not installed at all on the machine. It also doesn’t mean
that native Windows requirements have been met. The remaining tasks will continue to be processed according to the schedule outlined in section 28.3. The project remains in progress.

## 29 2026-10-16 HTTP owner and Uncompleted Connection Regression <a id="section-29"></a>

The local evidence directory for this section is `build-v030-dev/evidence/test-integrity-tdd-20261015/`.

### 29.1 Confirm normal lifecycle differences through input testing

In section 28, the pending-after-HTTP state cannot continue to be evaluated based solely on the callback array. The same Worker
bundle and independent TCP peers are tested in both fixed workerd mode and three different Capsid modes. The peer explicitly collects
all necessary information before sending an initial response via an HTTP request. Successful handshakes, non-101 responses, and any delays
are all triggered after the response is completed. Details can be found in `pending-lifecycle-probe.mjs`,
`pending-probe-results.json`, and the four-end `pending-*-observations.json` files.

Workers have stopped any unfinished connections and no more events will be sent. Peers have actually observed TCP closures.
The old Capsid continues to send success/failure callbacks, while stalled connections continue to consume resources. Connections that are now open on both ends can continue to communicate.
When another subsequent HTTP request attempts to retrieve the readyState of the original WebSocket, the worker may throw an error. However,
the old Capsid still returns a value. The protocol/extensions properties can still be accessed. It is important not to mistake “the object is still accessible” as an indication that
I/O operations can be used by another request. Similarly, it is incorrect to assume that underlying resources have been released simply because there is no callback function available.

### 29.2 The formal testing fails first, then it can be fixed. This is equivalent to cancellation.

The outbound runner has added seven actual calls and five types of semantic scenarios: held success, held reject, and three times.
HTTP is terminated after it is open, and the connection is completed before the body is finished. All objects are stored in
In the Worker map, three stalls are allocated to Capsid's two pending slots to prevent dependencies on GC or the default setting.
The ten-second dialing timeout masks cleanup defects. In reality, peers are closed using a monotonous clock with an upper limit of eight seconds; the default setting has not been relaxed.
Timeout occurred, and the initial result was not replaced with a retry. The streamed positive examples have been returned in the Response, and the body has not been completed.
During the handshake at the end of time, the body is concluded only after confirming open status. Subsequently, messages, wire replies, and a normal Close are checked.

The formal new use cases are first run on the old binary: the entire set passes, and the Capsid in all three modes is within the specified
Cross-request readyState assertion failed; see `pending-native-red.json`. New C++ regression in channel quota
Retain 20 objects in the order of 2, and verify the cancellation after HTTP END, as well as the interlacing of ACCEPT and cancellation.
It failed due to the lack of automatic cancellation mechanism on the old Worker nodes. Evidence of this failure can be found in the
`pending-cpp-red.json` file. It wasn’t a failure related to startup, compilation, or import processes.

Even though the independent connection owner is retained, if the dialing process is not completed, it will still be marked as being under HTTP control. When the
HTTP body is completed, any pending/assigned tasks related to existing protocols will be canceled. The queued JS events that were canceled will no longer trigger
user callbacks. The native owner continues to follow the result/terminal ACK mechanism for resource cleanup; the state remains unchanged until it is explicitly deleted.
The HTTP dependency of the open JavaScript job is only released when it actually starts, to avoid receiving only an ACCEPT or being in an unprocessed state.
After sending the ACK, the connection is handed over to the application. The readyState is updated to include the original request's ownership and final state check.
A new private bridge is still rejected in the Binding heap; this round does not modify the public ABI or FetchRPC format.

Revised original C++ WebSocket and outbound regression **2/2, zero skip**; the same official runner on both ends
Completed, reference **178 times/35 classes**, three Capsids each **179 times/36 classes**. The additional scenarios are
The Capsid has a defined dialing period. The actual report can be found in `pending-*-green-report.json`. For a complete comparison, please refer to
`pending-report-comparison.json`. After removing the exception for pending-constructor-after-HTTP behavior,
Outbound only retains the registered behavior difference that has not been requested for subprotocol.

### 29.3 Comparators must not conceal missing observations or unreleased resources

The compressed samples were generated from the four aforementioned reports. The provenance information includes the SHA256 hashes of the original files. Only 140 repetitions of the “bad-Accept”
input were compressed; three instances of “stall” and two instances of “normal echo” were retained, and the full
number of stress tests was performed during execution. Results related to the “pending-after-HTTP” scenario must now be empty arrays; no more complete
data should be included in the exceptions. All scenario names, repetition counts, and successful exits for “owned Hosts” are verified in separate lists.

The comparator first accurately verifies the categories of Error across requests and the diagnostics of each engine, and then only normalizes the message:
The complete text with explanations for workerd and the short text for Capsid are retained in the original report. Any suffixes, differences
Error categories, additional fields to be ignored, and unknown diagnostics that do not pass a broad regular expression are rejected. Other messages, sequence of events,
protocol/extensions, TCP shutdown, and wire Close continue to be compared.

New control cases include late arrival open/error/close, underlying unclosed, error transmission, one less budget reuse, and missed opportunities.
owned shutdown, incorrect readyState, missing messages/Close, streaming responses being prematurely considered as end, etc.
Additionally, it was verified that the existing reference clean-close variant operates correctly in nested configurations, and that the original report
remains unaltered. The final comparison results are **153/153, with zero skips**, as indicated in `pending-controls.log`.
This set of controls proves that the verifier will reject specified errors; the implementation validity is still verified by real four-terminal operation and full regression testing.

### 29.4 Comprehensive discovery reveals six legacy fixtures still dependent on the removed lifecycle

The first scenario involves conflicts where all resources are checked in three categories: `websocket_budget_*`, Managed
retire-deadline/replacement, and the native `host_channel_generation_lifecycle`. These resources create pending requests after the
HTTP requests are completed, but then expect these requests to continue consuming resources or reach the handshake phase. Some failures are directly indicated by
the peer receiving EOF before the handshake is completed. This scenario conflicts with the fixed reference defined in section 29.1.
The initial full set, the original fixture copy, and six failures are retained. Failures are eliminated through non-passing rollback implementation or budget reduction assertions.

The budget fixture now sends the constructed marker in the response stream, retaining the HTTP body until the actual open is performed.
Or write a complete array of events and end the response with a close callback. HTTP must be observed during the handshake period.
inflight=1, pending dials=1; refusal upon second attempt, number of peer connections not increasing, count of refusals
And high-water precise verification. After peer termination, check error/close, reset all slots, and dial again with success.
The diagnostic request and the held body use separate connections, and the static pool does not depend on the two requests coinciding with the same Worker.
All five scenarios: original service/listener channel, pending dial, two-level queue are retained.

The managed and native generation fixtures create pending dials based on the message callbacks of surviving connections.
The HTTP inflight should still be zero, ensuring that both connections and pending dials exist in reality. Retirement period should be retained intact.
TCP closure, budget return, worker reap/replacement, and quota reuse assertions; no extension of the original deadline.
It has been confirmed that both triggered connections and pending transports have reached their mandatory expiration periods. These issues fall under the product lifecycle
testing scope. The compatibility conclusions still rely on the official four-party HTTP interactions.

The six revised directives have been approved. You can find details in `pending-budget-streamed-attempt1.json` and `pending-lifecycle-corrected-attempt1.json`.
Next, all source code should be synchronized to the new clean snapshot, and the 518 tasks related to workerd
should be re-run. The old `pending-full-*` files represent failed runs during the initial phase; these files cannot be updated using the approved results. Finally, files
named `pending-corrected-full-*` and `pending-final-gate/` have been saved as valid versions. The process of accepting endpoint transfers,
WPT profiles, resource management, and handling resource failures is still ongoing. The entire process of handling
second clients, native operations, dynamic configurations, and publishing matrices is not yet complete.

### 29.5 Final Comprehensive Verification and Remaining Boundaries <a id="section-295"></a>

After correcting all six clamping issues, 4,287 bytes of project source files were synchronized to the clean snapshot
`fd12e6471e2d5ea2a50f1fb95b1f248e15ce02c9`. All build targets for Release/LTO have been successfully completed.
Second full scan **518 items, zero exclusions, 513 passed, 2 failed, 3 skipped**, took approximately 610 seconds.
See `pending-corrected-full-summary.json`, XML, and the complete LastTest log. In the first full submission, **507 cases were approved,
8 failed, and 3 were skipped**. Save the results as `pending-full-summary.json`. No rewriting or merging of the data has occurred yet.
All new features, as well as those related to HTTP, channels, Binding RPC, final states, upgrades, and memory recycling, have been implemented.
However, this does not mean that all planned additions in these areas have been fully realized.

Two profiles that still fail are rejection support and MessagePort_initial_disabled.
Conflicts, without expanding existing exceptions. The complete original WPT log consists of 84 reports and 14,060 items, including
20 known failures, 3 unexpected sub-item failures, and 1 missed old expectation; statistics do not depend on truncated data
JUnit text. This HR-Time has passed, but 58,600 external clock samples still show 19 instances of exceeding
A realtime/monotonic offset change of 1 ms, with a maximum of approximately 2.429 seconds. This observation does not replace the test results.
No causal attribution will be made for this additional failure; the precise failure window and stable platform qualification in Section 27 will remain intact.

Installation, package content, metadata control, smoke testing, duplicate build comparison control, and actual duplicate builds - six items.
Pass; The contract that records the actual binary differences is still fulfilled even when the build is repeated, and it cannot be written as bit-level consistency. Additionally,
121 Project: C/C++ Testing and fuzz code for static analysis, no standard assert() found under NDEBUG
Eliminate the usage of "usage"; this scanning does not replace the actual operation of negative control or dynamic tools.

The final complete workerd gate of the same Host/Worker is **10/10, zero skips**, and the directory is
`pending-final-gate/run-LHB4ns`. Four-end API with 648 assertions each, including normal baseline and 14 fault conditions.
All reports have passed archiving verification; the comparison/report control remains at **153/153**. Outbound reference
178 times/35 classes, 179 times/36 classes for each of the three Capsids. A direct comparison is made between the pending and EOF lifecycles.
Only those exceptions related to behaviors that have not been requested for subprotocols are retained. This reference clean-close variant represents a narrow range value, which is set to 0.
The specific commands, input identities, and observed values are included in this run directory, and no previous successful directory has been used instead.

For regular users, the three conditions involved in the full check include “skip”. Additionally, the same binary method can be used under the “root” condition to achieve
**3/3, zero skips**. See `pending-qualified-root.json` for details. The results for regular users are not marked as “pass”.
The SHA256 hash of the host is `7afb595c74d3a177fb0fd807f5c6b0553de2aed39b369e9f27649c25960b2179`, and
the SHA256 hash of the worker is `273b7e1e92e92830cda754fca9e114599c9a8aa25a817f077d9937ed3acdf29a`.

The response submission phase for the accepted endpoint, the illegally received Close code, the synchronization burst queue limit,
The unrequested subprotocol is still a registered difference; the error diagnostic text is specified in designated fields only after precise classification.
Standardization. WPT profile, stable clock platform, Binding-origin WS, CPU/heap/OOM, second
Independent clients, native platforms, v4 fuzz/sanitizer/soak, and evidence for T15–T17 releases are still pending completion.
The original working directory HEAD remains at `8028b88`. The source code for this round has not been submitted or pushed. The target remains in progress.

## 30 2026-10-16 Response Construction and Phase 101 Review <a id="section-30"></a>

### 30.1 Test Effectiveness and Differences with New Inputs

In terms of historical GLM updates, there are indeed instances where identity checks were removed, certain drivers were omitted, and
there were issues related to the minimum requirements and assertions: 0. See sections 23.3 and 26.1 for more details. These issues have been resolved. The current state
of the workspace combines various modifications from subsequent updates. Any discrepancies found should be attributed to GLM, but it cannot be concluded whether the authors intentionally cheated. The
compatibility of current features with established standards still needs to be addressed separately.

In this round, the A–I list P0B was targeted. The same workers, clients, and inputs were used, operating in locked workerd
environments and three different host modes. In addition to the previously registered accepted endpoints, two additional issues were
identified: abnormal behaviors related to non-101 status conditions, along with situations where the body length was fixed at zero.

| Input or Phase | Fixed workerd measurement | Before Correction: Capsid |
| --- | --- | --- |
| WebSocket with status 200/100/201/500 or default status | RangeError, required status 101 | TypeError |
| Empty string, BufferSource, Blob, URLSearchParams, or boxed string | Successfully constructed, body=null, no implicit content-type specified | Rejection Body |
| Empty ReadableStream | TypeError, even if the stream has been closed | Rejection; This control must be retained |
| The endpoint has been accepted, both ends are accepting, the accepted endpoint has been closed. | Valid 101 and with an application header, followed by a TCP termination, no WS Close frame | 101 Forward Return 500 |
| After the handler returns, accept the peer. | It could be read as 101 before acceptance; subsequently distributed from the previous DATA. | Do not submit the header before acceptance. |
| accept before DATA, followed by a normal Close | After accepting, send a message, then cleanly close. | This sequence is not covered. |
| A Response Created by Another HTTP Request | 101 Complete HTTP 500 | Rejection; Retain origin control |

Pre-normative check of real nonce/accept, unique handshake header, application header/subprotocol, complete
Wire bytes and termination sources. EOF and ECONNRESET are normalized only after the above checks; original header,
Nonce and final state metadata continue to be archived. The initial diagnosis in this round mistakenly counted the close after local dispose as peer.
Termination, with local endpoints reserved, prevents attempts to exit the reference normally; corresponds to the original report
Retained properties: diagnosticOnly, complete:false. These properties were not considered part of the acceptance criteria. After modifying the observer, the system successfully ran again with correct input data.
And add real TCP negative control, as local cleanup actions cannot prove that the connection has been released on the tested side.

### 30.2 Formal Regression and Independent Negative Control

The file `tests/fixtures/workerd-ws/response.mjs` has been added. The official list has been expanded from 14 to 15 items. The requirement states
that this file must be used at all four endpoints. There are 32 fixed outputs for constructs/body/clone, including DataViews that are detached from
the underlying buffer, empty views on non-zero buffers, fake values for byteLength/size/toString, and explicit content-type
declarations. It is necessary to check the exception names and messages, as well as the values of body, bodyUsed, and endpoint identity. Items with non-empty bodies,
scalar values of zero, or empty streams cannot be allowed through after the fix for empty fixed bodies.

The actual Upgrade includes eighteen failures of coupling, with three categories of six failures each: accepted/both/closed.
There are two connections that remain alive at the same time. Check the messages and close normally. There is also a delay in accepting, and DATA/Close is done ahead of time.
Cross-request Response, as well as the transfer of outgoing endpoints that are open/still CONNECTING. Capsid
The service quota is limited to two slots; the peer of the outbound fixture is explicitly stopped and does not rely on GC to release it.

`tools/workerd-response-contract.mjs` ensures that the output and scenario list are complete and unchangeable. It does not allow errors that result in identical inputs on
both sides. `tests/test_workerd_response_contract.mjs` includes various variants, such as missing inputs, attempted retries,
serial control mechanisms, incorrect nonce/headers, premature callbacks, fake EOF situations, abnormal wire operations, and incorrect result constructions. The code
is also tested against real Node.js TCP peers to verify issues like local destruction, infinite state timeouts, incomplete headers, actual
FIN/RST responses, and incorrect frames. The overall comparison/reporting mechanism is set to **209/209, with zero skips**.
This number represents the number of assertion control instances, not 209 independent product compatibility scenarios.

The sample data comes from the actual `response-workerd-red-final-v2-report.json` file, which contains 717 assertions and
original observations. It is noted that only one fixture was executed during this process, and the complete matrix was not completed. The current precise input is being rerun
on the old Host/Workers that were retained. All three modes failed to generate valid assertions. The current native testing also failed
under the “101 first submission” assertion. See `response-exact-corpus-old-result.json` for more details.
It is a reproduction of the old binary code and the final input, without rewriting the reproduction date to the first red-test time.

### 30.3 Implementation Adjustments and Regression Tested Lifecycle Issues

`js/fetch.js` retains the WebSocket-related checks. It addresses issues related to abnormal status conditions and converts empty bodies into valid
data. It also preserves the original length and content of the data received, regardless of any forged attributes. Empty streams are still rejected, and WS Responses cannot
be cloned. `js/websocket.js` separates the handling of 101 requests from endpoints belonging to the same origin. If
there is an error, the transport process is terminated immediately, preventing the generation of a second 500 HTTP response. When a peer has
not yet accepted the data, buffering of DATA or normal closure can occur. Transport failures can be interrupted, and endpoints that are not activated will not
send any callbacks. Requests that do not involve an Upgrade mechanism will still be rejected before they are processed.

After the first round of builds, native regression tests found that submitting 101 erased ResponseState, and the process waited for acceptance.
The HTTP token is treated as a legacy task. Added private provisional coupling owner, retaining the original
Token and immutable origin, with a duration that is the earliest of the original request deadline and ten seconds; activate, failure,
The owner releases the ownership both during recycling and shutdown. The recycling rules for ordinary HTTP are not relaxed. Binding
The runtime cannot call the newly added activation bridge. The deadline will stop I/O with a native CANCEL.
Even if the regular Close has been cached, the waiting must be awakened, and then the actual ACK amount must be returned.

New C++ controls have been added to retain the original cancellation logic. Twenty invalid attempts have been checked to ensure that the actual transport abort occurs correctly. The order
of DATA/Close operations has been adjusted accordingly. Two connections that were not accepted were canceled according to the original deadline. There are no application callbacks involved. ACK
signals were sent in advance, and two channels were reused. During development, additional tests were conducted to ensure that cases where Close kind=1 is incorrectly interpreted as an application-initiated event are properly
handled. According to the protocol definition, 0 represents “initiated”, and 1 represents “observed”. The cancellation logic has been refined to use precise values of 0, with no multiple values allowed.
This failure and the first poison failure are retained. The final `worker_websocket`,
`worker_outbound_websocket`, `worker_terminal_continuation` **3/3**, proof of normal operation
HTTP legacy task checking is still in progress. The deadline for acceptance has not been met. The Capsid resource policy is being verified, but this is not based on it.
It is also claimed that the never-accept timeout for workerd is exactly the same.

### 30.4 Four Ends and Complete Gate

Sync with clean Release/LTO snapshot `57fb5a76b00225edb9a3a067cb14b338526fc9cc` 4,292
Complete one project source file and complete all target builds. The four endpoints are each assigned **717 assertions**, and the original API
**648 Assertion**. Filter report retains "complete:false". Subsequent complete gate
`response-final-gate/run-FTii2w` **10/10, zero skips**, executing 15 fixtures on each of the four ends.
The normal API baseline and the fourteen failure reports have also passed the archiving verification. The API-only baseline is now parity=true.
Partial report for the complete matrix; the complete inbound parity is still false.

The previously accepted-endpoint HTTP-500 exceptions have been removed. Issues related to illegal Close codes, synchronous
budget submissions, and absence of subprotocol requests remain visible. The corrections made to HTTP owner and exceptional EOF situations continue to
apply. In the first round, 518 items were completely ruled out; this resulted in **513 passes, 2 failures due to WPT issues, and 3 cases where conditions were bypassed**. This
process took approximately 630 seconds. Original logs and XML files are stored separately in the `response-full-*` directories. Any new controls or corrections mentioned
in section 30.5 are not included in this snapshot; they will be saved separately for further review. WPT profiles, Binding-origin WS, resource failures, second independent
clients, native platforms, and
dynamic/publishing matrices remain available. Therefore, they should not be dismissed just because certain items passed the review in this round.

### 30.5 Continuing to review the identified pre-submission deadlines and missed steps in the auxiliary processes

After continuing to check for output congestion, the following settings were added: native input settings: max_header_bytes=8192; output capacity: 1024 bytes; valid
application header size: 2048 bytes; HTTP deadline: 200 ms. Headers cannot be sent to the output queue, so no requests with
code 101 have been submitted yet. The new test encountered a CHANNEL_CANCEL response (event=17, request=0), indicating that the specified condition was not met. See
`response-precommit-red-v2-result.json` for details. The temporary coupling period should not override the HTTP timeout setting. Only requests
where the committed owner setting is enabled will be processed according to the original HTTP timeout settings. Before submission, the requests will still be
processed using the original HTTP timeout mechanism. It has been decided to allow three consecutive attempts at execution, with the requirements being: HTTP
timeout, healthy HTTP responses, no premature Upgrade/Channel CANCEL, and reuse of reservations for both slots.

Although the benchmark tests included in the full set of results passed successfully, it was observed that
the `sleep 600` subprocess of the fake worker was re-hosted. The old `assertNoProcessContains(marker)` function only searched for workers
marked with this label, thereby failing to detect real subprocesses that did not have this label. This is a flaw in the auxiliary scripts and
their cleanup mechanisms. However, this cannot be used as evidence that the actual Host/Workers also have similar vulnerabilities, nor can it be attributed to GLM.

New Linux `benchmark_fake_worker_lifecycle`: Capturing actual parent/child PID from /proc
And at the start time, after SIGTERM/SIGINT, it is required that the child process has been reclaimed. The test cleanup also only operates on the same.
Example identities. The SIGTERM of the old helper leaves child processes, while SIGINT does not exit within the deadline; both
Failure is retained in `response-fake-worker-red.*`. `bench/fake/fake-worker.sh` should be changed to asynchronous.
Wait and on the exit/signal path, kill, wait its own child, maintaining the command-line identity for profiling.
The same test source code will be followed **2/2, zero skips**; the remaining platforms have not yet been controlled by this Linux qualification process.

These two additional corrections differ from the binary of the first complete gate/518 item. They must be rebuilt and archived as new items.
Gate and **519 items** are fully covered. The first 518 items maintain their own identities and results during operation, without interfering with subsequent processes.
The targeted passing through merging does not also involve the successful adaptation of the old complete gate to the new binary for the final qualification.

### 30.6 Final Review and Unclosed Scope of the Amendment Version <a id="section-306"></a>

The new, clean snapshot `decf31e40166afd5ea8462bc42a723e0ec426ca7` has synchronized **4,293** project
source files. All targets, including Release/LTO and mimalloc=OFF configurations, were successfully built. New features such as a submission deadline, real-time process recycling, along with existing outbound mechanisms, final states, and output congestion mechanisms
were implemented. This resulted in **5/5** successful builds. Additionally, **519 items were completed, with zero exceptions; 514 builds passed, 2 WPT failures, and 3
conditions were skipped**. The total time consumed was approximately 629 seconds. The original evidence can be found in
`response-corrected-core-*` and `response-corrected-full-*` files. These files do not include the first 518 items.

Two failures were still due to conflicts with the rejection support and MessagePort_initial_disabled
profiles. From the complete LastTest output, **84** WPT reports and **14,060** individual items
were identified. There were 20 known failures, 3 unexpected item failures, and 1 missed old expectation; upstream assertions
and precise failure checks were retained. HR-Time passed in both runs. External clock observations recorded 62,095/62,012 samples
respectively, with no offset changes exceeding 1 ms. The previous stability/clock acceptance criteria were still retained.

Three condition cases are presented in the same binary with root **3/3, zero skipped**, see
`response-qualified-root.json`; The results for ordinary users still retain three skips. Installation, package content,
The six items of control over metadata, smoke, repeated build comparison, and actual repeated build have been passed. The latter is still in accordance with
Document the verification of actual differences, and do not claim that the bit-level consistency is achieved or that all release matrices have been completed.

The revised version of the same Host/Worker has a complete gate named `response-corrected-final-gate/run-B54tF3`.
**10/10, no skipping**; All 15 fixtures were executed at four ends, with 717 assertions for each Response.
There are 648 assertions in the API. The control of comparators/reports is **209/209**. The normal baseline and the fourteen fault reports have passed.
Independent archiving verification. Outbound reference 178 times/35 classes, 179 times/36 classes for each of the three Capsids. This time
Reference report shows 1 previously registered narrow variant of clean-close. After checking against the existing precise contract, it is confirmed.
Standardize, keep the original trace; no new broad exceptions added.

The final Host SHA256 is `c3baaeedfab314b7d72fbeb183a86759ae637896dddad6165b7e22de7efdaf83`.
The Worker SHA256 is `9760ebf190c32023b42a5ac6f0aed8c1fdc4b942738ef91ed0da745b53ec2c2a`.
The binary identities before and after full execution, gate execution, and root condition execution are consistent. The original working directory HEAD remains `8028b88`.
The final document and evidence index are subject to separate review, and their modifications should not be mistaken for additional runtime verifications.

The differences in the submission phase with the 'accepted-endpoint' deleted in this round; illegal Close code, synchronous budget submission,
Explicit exceptions to the subprotocol are still requested, and deployment quotas such as message size continue to be subject to product configuration constraints.
The complete inbound report still indicates that parity=false. The WPT profile, Binding-origin WS,
CPU/heap/OOM metrics, second independent client, native platform, v4/WS fuzz, sanitizer/soak, and T15–T17 metrics are
still not fully completed. The goal remains ongoing, and the acceptance criteria for this section do not extend to full test completeness or complete compatibility.

## 31 2026-10-16 Node ws and Python independently implement interoperability <a id="section-31"></a>

### 31.1 Completing Implementation and Environment Identities According to the Original Task

This round continues with the execution of all scheduled tests. The previous round marked as having been successfully verified. According to the T13 requirements, Node.js and Python WebSocket libraries must each undergo
at least one set of actual inbound/outbound interactions. The new access control list failed when trying to implement it using
the old ten-item list. It was confirmed that none of the four backend servers had independent client elements. The CI installation process also failed when using
the old workflow. Corresponding test results are `interop-missing-matrix-red.*` and
`interop-workflow-red.*`. The failure to create test files or to properly manage dependencies is not considered as a successful execution.

In the early stages of development, we used the built-in Undici. Through testing, we found that it would proactively propose compression; empty Response extended header.
The desired disabling effect was not achieved. Subsequently, a diagnostic run with explicitly no compression input was performed, but the results were rechecked.
T13 confirmed that it cannot replace the Node ws package as specified in the plan. Early failures, built-in clients, and four-end records
Retained as supplemental/diagnosticOnly; not used for acceptance of Node ws. Ultimately, it is used for public purposes.
`perMessageDeflate:false` is similar to Python's `compression=None`. Refer to date/flags for no changes.
Compression is already within the scope of the 0.3.0 exclusion, so it will not be classified as new support in this iteration.

Locking CPython **3.12.3**, websockets **15.0.1**, Node **24.14.0**, and ws **8.22.0**.
Check the SHA256 value/size of Python wheels, and check the SHA256, SHA512 integrity, and size of Node tarballs.
Then install outside the workspace. `tools/setup-ws-interop-python.py` prepares both with the same command.
Dependencies, do not run the Node installation script, disable optional native addons for Node. Each client and peer
Before use, check the summaries of 47 source files in Python and 17 source files in Node, rather than relying solely on the version numbers.
After actually copying the library, only comments are added and the version remains unchanged. It is also necessary to fail the specified identity assertion; the baseline installation remains unchanged.

Python usage is [15.0.1 Client](https://websockets.readthedocs.io/en/15.0.1/reference/asyncio/client.html)
And [Server-side documentation](https://websockets.readthedocs.io/en/15.0.1/reference/asyncio/server.html) Okay;
The compression settings for Node and the client/server API can be found in [ws document](https://github.com/websockets/ws/blob/8.22.0/README.md).
The precise package URLs, identities, and protocols are recorded in `websocket-interop-lock.json` and the run report, respectively.

### 31.2 Actual 22 matrices, closures, and certificate rejections

`tests/test_websocket_interop.mjs` covers reference, single-worker, static-pool, and managed scenarios.
Run a fixed **22** scenarios per terminal. The Node ws and Python client each cover ws/wss text, empty binary,
180,000 bytes pattern, mutual active Close, unknown CA and error hostname; Python additionally in UTF-8
Ping is interspersed within the code points and between binary shards, waiting for a matching Pong. Both libraries also act as outposts.
Protocol server, verifying plaintext/TLS positive examples and rejecting both types of certificates. The positive example server also sends fragments/Pings.

Each time a challenge associated with the current run ID is passed, it corresponds to actual sending and receiving. The complete content is fixed by an independent SHA256 hash.
Not just comparing lengths or pre-defined successes. Close code/reason, two-way Close, event sequence, SNI,
Check for zero extra messages and actual TCP attempts. The certificate counterexamples of independent clients must hit specific verification errors.
Connection refusal or timeout cannot be mistaken for certificate refusal. The common events occurring at runtime exit only result in general connection failures.
Similar credible certificate positive examples, exactly one TCP arrival, and no entry into the protocol handler joint inspection cannot be based solely on
The general error string proves the reasons for failed native TLS. The actual peer has completed all tasks.
No surviving sessions, Python exited normally. Host/reference exits are bounded and do not force termination, and there is still a healthy HTTP status at the end.

Inbound WSS uses a controlled TLS front-end to receive plaintext Host, in accordance with the current Host listening model; Outbound
The wss directly verifies the TLS servers of both libraries through the native client. This does not prove that the native Host has been updated.
TLS listener does not replace evidence from the Windows/macOS platform. The first four-device running that meets the specified dependencies
Passed 22 items, see `interop-ws-four-backends-attempt1.json`; the complete gate is bound to a separate build identity.

### 31.3 Negative Control, API Missing Failure, and Forced Integration

The new comparator involves evaluating the complete scenario list, actual client/peer outputs, and independent gold checks. Additionally, four parties are
compared with each other. If either party fails to execute the Python code or returns the same error message, the process fails. The original TLS diagnosis and stderr messages
are retained. Only after verification do the client libraries and dynamic ports undergo normalization. The actual error messages from the peer ensure that both clients meet the specified
assertions. Normally, the peer must pass the test first. The negative control of the library source digest also ensures that the actual interpreter is used during the evaluation process.
The control set currently **260/260, zero skips**, includes previous responses, outbound, and report integrity control.

Further checks on the T13 configuration revealed that the requirement to avoid skipping due to missing APIs is not met. It was found that the old Capsid runner still returns a value of
77 even when the Pair parameter is missing. The isolated profile copies actually
contain **77**, not 1. See `interop-profile-skip-red.*` for details. We have changed this to require the use of WebSocketPair, thereby explicitly indicating
failures. This prevents other startup errors from being considered as valid results. The existing assertions related to missing drivers will remain in place.

For each of the four backends, one additional `websocket_interop_*` CTest is added. The total number of checks has increased from
**10** to **14**. The original 15-fixture package remains intact, and independent interoperability checks are handled separately, with their own lists and reports. The pre-checking procedures
for each backend are implemented in the same way as during actual installation, and use the same paths as CTest. Archiving checks require reports from all four parties, along with source
code, dependencies, lock identities, and the run ID. Workflows have been added to the list of dependencies and audit items that need to be prepared for this integration process. It
was also discovered that I overlooked preparing the SDK/reference files for the sanitizer job. This issue was not detected during the global string
checks. We need to address this issue by performing additional checks on the job, and then completing the installation steps for this job. See
`interop-sanitizer-setup-red.*`, `interop-sanitizer-setup-green.*`, and the corresponding provenance information for more details.
YAML parsing also confirms that both jobs have prepared their dependencies before the configuration phase. These static checks do not replace the actual execution of the
Hosted sanitizer. Details regarding the final build process, along with the results of 523 items, can be found in section 31.4. No changes were made to the results from the previous round.

The independent implementation of the T13 framework has been completed. The complete parameters/functions, fanout worker identities, registered
product differences, as well as features related to Binding, resources, dynamic/native capabilities, and those covered by T15–T17
remain intact. However, neither T13 nor the entire 0.3.0 version has been fully approved yet.

### 31.4 Independent Implementation for Final Verification

The clean snapshot `346790c4167378b1e7276aefc43fc06fc6ff3fbf` is synchronized with **4,304** projects.
Source files, Release/LTO, mimalloc=OFF, all target builds were successful. The complete workerd gate is
`interop-final-gate/run-mQIzAu`: **14/14, zero skips**, executing all 15 fixtures at each end.
There are 648 assertions for each API, and 717 assertions for each Response; there are **22** interoperable combinations of Node ws/Python.
Scenario. Comparator/Report Control **260/260**, normal API baseline and fourteen failure reports also passed archiving validation.
The source code at four ends, dependency locks, running identifiers, and binary identities are checked separately, and the original report is retained.

Then, the same build is executed **523 Items, Zero Exclusions** Taking about **635 seconds**: ** 518 Pass 2 One WPT
Failure, 3 conditions skipped **; CTest exit=8, cannot be described as fully green. Original output and summary are as follows:
`interop-full.log`, `interop-full.xml`, `interop-full-LastTest.log`
`interop-full-summary.json`. The three conditional test cases are then executed in the root mode using the same binary.
**3/3, zero skips**, see `interop-qualified-root.*`; the three skips for regular users remain unchanged.

There are **84** reports in WPT, with **14,060** sub-items. This includes 20 registered failures and 3 unexpected cases.
Failed sub-items and 1 missed old expectation. Two failed files are still reasons for rejection support.
Requirements and Conflict with MessagePort's capture:true/profile; Upstream Text, Target Event Assertion
And precise failure checks continue to be retained. The HR-Time has passed this round, but overall, the external clock observations for this entire operation are still valid.
**62,797** samples captured **19** realtime/monotonic offset jumps exceeding 1 ms.
The maximum value is approximately **819.56 ms**. This is not an exact window attribution for use cases, and therefore it cannot be used to close the stable platform time qualification.
It is not possible to change previous failures to successes; the original CSV and observation summaries should be saved separately.

Installation, package content, metadata control, smoke testing, duplicate build comparison control, and actual duplicate builds have all passed.
The last item still accepts and records toolchain binary differences permitted by the contract. These differences are not reproducible at the bit level, and do not represent
T15–T17 completed. The final Host SHA256 is
`17530dd080e943b44f5da43b5232bb56de66c7ebf80921200eddfd111516a6f4`, Worker is
`9760ebf190c32023b42a5ac6f0aed8c1fdc4b942738ef91ed0da745b53ec2c2a`; Gate,
The full set of identities is consistent with the root recheck. The original working directory HEAD remains at `8028b88`; the final documentation will be audited separately.
Do not misrepresent the document as a new round of runtime verification. The evidence is located in `interop-verification-summary.json`.

### 31.5 Areas That Still Cannot Be Covered by the Number of Passes

In the past, GLM conducted incremental checks for deleting versions/date/flags information, as well as checks related to missing drivers, minimum requirements, and zero assertions. For
more details, refer to sections 23.3 and 26.1. These old checks have been restored, but they do not constitute evidence of subjective cheating. The newly corrected issues include missing skip operations in Pair operations,
omissions in the sanitizer integration, and errors in early client selection methods. These issues are still considered valid, but they
cannot be attributed solely to GLM.

- The current fanout fixture defaults to using **one Worker** through `ws_host_backend.mjs`.
  And do not record the Worker identity. It verifies the cross-request I/O limits within a single Worker; subsequent actions must be at least
  Two Worker and application instance identifiers verify the connection groups with the same Worker and different Workers respectively.
- Three types of explicit issues: illegally received Close code, limited synchronization sending queue, and unrequested subprotocol.
  The differences persist, and the complete inbound report still has parity=false. The product message quota is also not equal to the reference default value.
- Binding-origin WS, CPU/heap/OOM, complete parameter/cleanup combination, default allocator, and resources for various platforms
  Qualification; Gate; Guaranteed Minimum; Regression Test; Counterexample; Eviction; Main Working Tree; Merge; Handoff; Teardown v4/WS fuzz, sanitizer/soak, native Windows/macOS Actual Hosted Release Evidence
  Continuing open. The existing parser smoke cannot replace the v4 multi-frame state machine and real WS dynamic verification.

These are the scope of work to be performed. No additional release exceptions will be added, and neither will the T13 or the complete test objectives be marked as completed.

## 32 2026-10-16 Error in identity and request attribution for multi-worker fanout <a id="section-32"></a>

### 32.1 Filling the red test areas from actual coverage gaps

The progress from the previous round has been completed, and the final verification process continues. In this round, we reviewed the original HEAD, workspace, and T13 configurations, as well as the latest
archived files. None of the 14 security checks or 523 tests were extended to form a complete acceptance criteria. The old fanout mechanism used only one worker
without any associated identities. The new control mechanism, which requires “all backend instances to be omitted”, resulted in an “Missing expected
exception” error when comparing with the old comparator. See `fanout-identity-red.*` for details. After additional checks, the issue was resolved and passed.
For the old fixture, first record two actual connected instances/connection UUIDs, then execute the original cross-request and Close assertions.

Linux’s static-pool uses SO_REUSEPORT. It cannot be assumed that the first four connections are allocated in a pattern of 2+2. The new matrix records
up to 64 real connections found, grouped by instance. At least two groups of connections are required per instance; otherwise, the process fails.
Excess connections also have associated identities and actual close records. This refers to two Worker services within the same
workerd, two Workers on the same host for static-pool/managed, and two independent single-worker hosts for single-worker.
Therefore, it cannot be claimed that a single single-worker host can support two Workers.

### 32.2 Fixed Reference Run vs. Real Error Differences

The workerd has already identified 24 probes that are still waiting, through the use of
send/readyState/close probes, as well as 6 target handlers. For more details, see `fanout-reference-overlap-attempt1.*`. The old
Capsid code fails in all three modes: in normal cross-request scenarios, send/close operations result in TypeError
exceptions; see Error for more details. The error types for readyState are similar, but the diagnostic details differ. Among the 24 cases, each Capsid has 12 differences in observation results;
the remaining six cases also remain as reported in the original document. These are simply counts of input values, and cannot be considered as separate causes for the issues.

The old driver only checked for errors related to specified text, thereby failing to detect errors related to types and complete messages. The new contract requires a detailed diagnosis
of both common errors and Native I/O operations, with no additional exceptions. It was also suspected that pending handlers
might bypass these checks. However, tests showed that all six conditions effectively prevented cross-request operations. This hypothesis was not confirmed. The
`fanout-overlap-observations.json` file retains the actual results and the errors I collected from initial assertions, without creating any false data to
cover up the defects. The fixes only concern those types of errors that have been identified, without changing the origin of requests or the final state of the system.

The first version of Cap’n Proto, which was tested during startup, lacked lexical spaces before constants. As a result, the startup failed, and this issue was addressed separately. Only after
corrections were made was a valid reference input available. The manual control mechanism for command naming did not include live control scenarios, as no Host/Worker
environment was provided. Therefore, this failure is not counted as a functional issue or final acceptance criterion. The new CTest/access control
mechanism now supports four types of backends, as well as actual live control scenarios. The complete build results will be archived separately later.

### 32.3 Old Outbound Diagnosis Expectations and Control Prerequisites for New Samples

The first clean build of the nine-directed regression 9/9, with a control set of 302 items; the scope includes a new matrix with four ends.
Comparator and native final state/congestion regression. The first complete 18 gate tests resulted in 15 passes and 3 failures, as listed in the table.
`fanout-final-gate/run-7BiXr9`. All three failures are due to old outbound comparators requiring a short diagnostic for Capsid.
The new implementation has returned a complete diagnostic reference; the failure is retained as it was, without excluding the corresponding tests.

First, add two controls: “All modes must accept complete diagnostics” and “All modes must reject short diagnostics”. These
controls will result in red lights being displayed. See `fanout-outbound-diagnostic-red.*` for more details. Next, remove the
messages based on engine selection, and eliminate the normalization process for messages. Instead, compare the complete objects on both sides directly. All real outbound messages need to be re-collected.
The updated samples only involve 140 failed handshake attempts during compression. All information comes directly from the actual reports. The original SHA values and
old samples are retained, and there is no need to manually replace the Capsid field to simulate the running results.

The new reference sample contains two variants of the “clean-close error” that have already been registered in the database. These are two instances
of the “unconditional reinsertion error”. Since the total number of reported errors is assumed to remain constant at 1, this results in the creation of yet another error, thereby leading to failure.
It has been checked that the target contains 0/1 narrow variants. Therefore, only one variant will be generated in case of absence of such variants. The total number of variants reported is calculated based on the count of original events encountered. Even if this
variant appears in both error messages, error diagnostics/status/order messages, or Capsid reports, it still must be considered as a failure.
For the first failure, the result is `fanout-controls-corrected.*`. After correction, the overall control status is **304/304, with no skipped operations**. See `fanout-controls-final.*`
for details. This is a preliminary correction to the control data; it does not introduce any additional exceptions during runtime.

Subsequently, a clean snapshot containing these corrections was re-established. The entire target system was rebuilt, along with 18
comprehensive tests and 527 full-scale verifications. It cannot be considered that the results of the first nine tests or the previous 523 tests serve as the final acceptance criteria for the new build.

### 32.4 Revised Version Full Recheck

The final clean snapshot `d4286e4eb6639fc0a5cadd979b97474119affb2b` synchronizes **4,308** projects
Source file, Release/LTO, mimalloc=OFF, all targets built successfully. Complete gate is
`fanout-corrected-final-gate/run-mLaVh2`, **18/18, zero skips**. Executed 15 times at each of the four endpoints.
Fixtures: 648 assertions for each API, 717 assertions for each Response, and 18 assertions for each original fanout.
There are 22 scenarios for each independent client, 24 ownership probes for each multi-worker, and 6 pending handlers.
Probe, control set **304/304**. Each backend has exactly four actual connections detected this time; bounded discovery and excess
The implementation of connection recycling does not assume this quantity or the polling order. Refer to the Close narrow variant, where this gate is 0.
The new samples with two variants are still retained, and the two sources should not be confused.

The same build is then executed for **527 items, zero exclusions**, taking approximately **638 seconds**: ** 522 passed, 2 WPTs.
Failure, 3 conditions skipped**, CTest exit=8. Output, JUnit, and complete logs are `fanout-full.log`.
`fanout-full.xml`, `fanout-full-LastTest.log`, and `fanout-full-summary.json`.
The three condition cases are also listed under the root in the same binary **3/3, zero skipped**, see
`fanout-qualified-root.*`; The three skips for regular users remain unchanged.

There are still 84 reports related to WPT, with 14,060 individual items involved. Among them, there are 20 registered failures, 3
unexpected failures, and 1 failed old expectation that did not materialize. Two of the failed cases are due to conflicts
between the rejection reasons and the profiles in MessagePort captures. The HR-Time component was approved in this round, but external clock observations revealed that during 63,177
samples, there were **19** instances of offsets exceeding 1 ms, with the maximum deviation reaching approximately **1,064.79 ms**. This does not represent
an accurate attribution within the test window. Therefore, the acceptance criteria for the stable platform time remains open. Six items were successfully passed: installation, package content, metadata, smoke testing, duplicate build comparison, and
actual duplicate builds. However, these items still do not guarantee reproducibility at the component level, nor have all aspects of the release process been fully addressed.

The host SHA256 is `b45b12bf32ce1deec82be9b4cc8014bc4435a7b2b78d73784b77b92f08a0403a`.
The Worker SHA256 is `b6767ae2bce143726fd02dc529964e2c5b12d74923766f998df9a1620499611d`.
The identity verification was carried out for both complete access control systems, as well as for all relevant conditions and the root level. The original workspace HEAD remains at `8028b88`; the final documentation
will be reviewed later. All previous failures and unsuccessful attempts will be stored separately along with the snapshots from each stage. The evidence is
stored in the `fanout-verification-summary.json` file. The full testing process will continue.

### 32.5 Windows Fixed Client Preparation and Unclosed Scope

It is detected that the local machine already has MSVC 14.50, CMake, Ninja, and vcpkg; the dependencies of vcpkg have not been installed yet.
The existing Windows Node version 24.18.0 and Python version 3.12.10 differ from this matrix lock. Therefore, in the
`build-v030-native/` directory, we have prepared a standalone Node version **24.14.0** and a portable CPython version **3.12.3**. There are
no changes made to the system’s operating environment. The Node software uses the official SHASUMS256 checksum for archiving;
the SHA256 checksums for the Python archives are obtained from the official HTTPS metadata and verified without any Sigstore signature verification being performed.
Fixed ws 8.22.0 and websockets 15.0.1 to pass the digest check during actual Windows implementation. This results in 17 and 47 source
files being considered valid, as indicated in `native-client-identities.json`.

In actual Windows environments, the `test_websocket_interop_contract.mjs` file is executed.
**47/47, no skipping** is performed. This includes actual Node/Python TCP examples, error messages, and controls for library source manipulation. The temporary
directory is located in a dedicated native directory within the workspace; see `native-client-controls.*`. These are client/report-related controls. The native Capsid
Host/Worker has not been compiled or run yet. Therefore, they cannot be used for Windows product interoperability testing.

Next steps involve continuing with native dependencies, Host/Worker builds, complete parameters, and other origin/lifecycles.
Combination, Binding-origin WS, CPU/heap/OOM, v4/WS fuzz, sanitizer/soak, Mac and actual
Hosted Release Matrix. Illegal received Close code, limited synchronization sending queue, unrequested subprotocol
The three types of clear product differences remain, and the WPT profile and time qualification have not been closed. T13 and the complete objectives are not selected as a whole.

## 33 2026-10-16 Testing Entrance, Native Windows, and Still-Failing Qualifications <a id="section-33"></a>

### 33.1 Review Conclusions and Attribution Scope

There is reproducible evidence that the test intensity can be reduced and false passes can be achieved, but this does not constitute evidence of intentional cheating by the author.
The GLM increment saved in section 23.3 removes the version/date/flags check. It continues to function even when no driver is available. Only six items
are required, and a zero-assertion report is generated. The SHA256 hash of the saved file remains the same.
`0a92d8dd05644b5de476443b36706c7edba715de75ad031e1af7f267d8033e96`.
These historical checks have been restored. All issues identified during this round of testing have been addressed. However, errors related to subsequent code
updates, auxiliary probes, and various platform-related issues are not attributed to GLM.

Originally, Windows only had 47 records related to client setup and reporting mechanisms. There was no evidence of actual product build/execution
using Capsid. In this build, MSVC 14.50, Release mode was used, with Host/Worker set to ON. Static CRT
was utilized, and strict warnings were enabled. Mimalloc was disabled. Windows continued to follow the existing policy,
keeping LTO set to OFF. No builds with Host/Worker set to OFF were used as alternatives. The repository’s HEAD remained at `8028b88`, and snapshots were
only stored in isolated directories. There were 4,311 source files in the project.

### 33.2 Corrected False Passes and Omissions

- The six CTest commands for the Hosted workflow originally did not have `--no-tests=error`. Actual native
  The default value returned in the empty selection control is 0, and in strict mode, it returns 8. Parameters have been filled in item by item; the audit continues line by line for command parsing.
  Parameters with the same name placed in other assignments or comments cannot be passed off as valid.
- The old static audits would overlook private negative controls such as "only changing the Windows job host to OFF", although other
  The task is still ON. Now, check the unique Host/Worker, strict warnings, allocator according to Windows task management.
  Release, MSVC, and static triplet parameters. The final private workflow control ensures that 26/26 checks are met.
  Expected, see `native-workflow-final-controls-final2.json`; it is not hosted execution.
- The original native manifest actually registered four unusable managed scenarios, missing two of the portable modes' respective
  interop/fanout. List of shared platform modes; Linux retains three Capsid modes and fixed references.
  Windows/macOS: Register single-worker/static-pool. Add direct read of CTest JSON
  `websocket_platform_matrix` has 19 controls to prevent omissions, pattern/driver aliases, disabling, or
  Required tests skip; Windows must register its startup environment for regression testing. The checklist passes, not equal to functional execution pass.
- Windows CI is ready to lock Python 3.12.3, Node ws/Python websockets, and to locate Git.
  The accompanying OpenSSL CLI generates test certificates. The local certificate tool is OpenSSL 3.5.4; Host link
  vcpkg OpenSSL version is 3.5.2. The two are not mixed into the runtime identity. The new setup-python is fixed by the official source.
  Version 6.0.0 submitted, metadata for node24 verified, Hosted job not yet run.
- ABI guard originally compiles attached-worker runtime_error and requests OOM in Windows.
  And destroy-after-OOM. After correcting the current implementation, these branches will actually be 171 on the private copy.
  Passed the check, and these two conditional compiles have been removed from the formal testing. The complete log of the full profile is recorded again.
  171 items passed. The original 143 items cannot be extended to cover these paths, and the 171 items also cannot be extended to fully cover the resource matrix.

### 33.3 Strict Build and Actual Red Testing

In the first round of strict build, the implicit narrowing to bytes of the Close kind is checked out. After correction, the private checkouts continue.
The begin_request helper function is located in the C linkage, but it throws an exception again, has local variable name conflicts, and
The C++11 parser uses C++17 namespace syntax. Explicit conversions have been performed, and private helper functions have been reserved.
C++ linkage, renaming, and restoration of C++11 namespace notation, without closing `/W4 /WX`.
MSVC `/EHsc` assumptions regarding C linkage are visible
[Microsoft Exception Model Documentation](https://learn.microsoft.com/en-us/cpp/build/reference/eh-exception-handling-model).

Subsequently, all targets were successfully compiled. The actual Host and Worker WS tests still failed before reaching the READY state. C ABI
Diagnosis confirms that both v3/v4 have exit=1; isolation stubs show that the inherited handle is valid, but the first socket
Access to 10038, the TJS initialization is the same before and after. Only after adding the SystemRoot obtained from the OS API, the two ends
All have reached READY. Please refer to `native-worker-environment-probe.json`. This is not a rebuild of the socket.
Or adjust the evidence for the expected results.

The new `worker_windows_environment` has been added. First, the compilation is successful on the actual libraries/Workers before any corrections are made. However,
this only results in a failure due to the READY assertion. The official correction involves providing the SystemRoot obtained from the OS API to the Windows child
process. The application environment still uses the HELLO mechanism. The new issue involves incorrect setting of the parent process’s name values. This is used to verify the behavior during actual v3/v4 startup scenarios, as well
as during HTTP status/body processing. Additionally, separate native peers are used to verify that the physical environment only has the specified values, and there is no need for the parent process’s sentinel values.
This example has been approved. The old failed binaries and libraries are stored in the directory specified in
`native-preserved-pre-environment-binaries.json`. The initial compilation/parameter errors of the companion probes are not considered part of the product’s beta testing phase.

The first complete native profile snapshot is `ad828262ee161f965f1feb0ddeedd31218a2cd20`, and all targets have been compiled successfully.
The SHA256 hash of the host name is `43bd4966c609a38cb069beaf07c768e35f5e2b9222ae1d857724470b1614d81b`, and
the hash of the worker name is `0f4392f23249035420cb2e63c0a7fccb23ad44a0ce524e9f54a4dc8de4d1a948`.
Linux uses independent snapshots of the same project file bytes, `54e25d46e523adac0f255d032976ca82b272dd49`.

### 33.4 The complete failure range of the Native profile

The first round of native manifests was expanded from 240 items to 244 items; one of them was due to the completion of the OpenSSL CLI
Additional TLS registration. Exclude four worker_package cases according to the current Windows CI rules, and perform the execution.
**240 Item, 212 Pass 16 Failure, 12 Skip, CTest exit= 8**, approximately 305 seconds.
The complete output, JUnit, LastTest, and registration list are stored in the file `native-full-ci-profile.*`. The source code, SDK,
runId, and actual binary files are kept separately. It does not involve any Windows-specific checks or complete workerd validation processes.

Failure cannot be attributed to 16 independent root causes. The current evidence is grouped as follows:

| Scope | Actual observations and subsequent qualification |
| --- | --- |
| Total of 11 items: fixtures/mutation/session/outbound/budget/fanout. | Multiple instances have failed the normal exit assertion, with common codes being null and signals set to SIGTERM. The static-pool fixture also reports 2147483651, which requires separate verification. A real Windows shutdown path should be provided, maintaining the code=0 requirement, and the original error in the scenario should be preserved to avoid the exit assertion in finally overriding it. |
| worker_outbound_websocket | Private logs are only added to confirm that the first quota error is a filler request 101. Only after processing the specified errors 100–115 in the diagnostic copies does the target channel assertion still fail; neither of the diagnostic copies is a qualification. The original test remains failed. The next step is to add actual phase barriers and verify the ACK/CANCEL/data sequence. All errors or dependencies of 100/250 ms sleep cannot be ignored. |
| Two SDK interop | The Python peer of single-worker timed out, with a log indicating a Windows Proactor reset; the Node client of static-pool closed before receiving the expected message. These are not simple stop assertions and must not be passed in the 22 scenario of Linux by extrapolation. |
| host_dns_dialer_lifecycle | No actual DNS query has been received; it is necessary to verify the Windows c-ares/query/timer path and test preconditions. The numerical address path cannot replace the actual DNS. |
| benchmark_tool_boundaries | The first round of zero-QPS sub-process diagnosis only yields undefined; subsequent confirmations directly call python3, resulting in ENOENT. The interpreter found by CMake has been bound, and the spawn error is retained, see 33.6; it does not equal the completion of the performance matrix. |

Additionally, a unified exit verification is required: the old `test_host_single_worker.mjs` accepts SIGTERM signals as a way to terminate it. However, this does not
ensure that Windows will properly stop the process. The newer drivers retain the code=0 flag, which effectively marks any operation that fails as a
failure. It is necessary to adjust the platform-specific control mechanisms accordingly. Additionally, the newer assertions should not be changed to allow for forced termination.

The additional execution of metadata controls excluded by the CI process was handled automatically by CTest, which incorporated
these controls into the package contents. Both attempts were successful for the first time. The initial collector failed to account for the automatically added fixtures; as a result, the outer return value was set to 1. The original records
and the `native-package-metadata-qualified-reconciled.json` file were retained. When the smoke test
was executed again, the condition regarding the “archive mtime must be changed” failed. As the smoke test did not proceed due to previous
failures, see `native-package-smoke-qualified.*`. It cannot be considered a repeatable or successful attempt. Further efforts
are needed to ensure that the freshness of the archive time is properly controlled, and that repeated executions are carried out effectively.

### 33.5 Linux Comparison and Complete Objectives

All target builds for the same-source code Release/LTO have been successful; fixed workerd gate **18/18, zero skips**.
The number of control sets remains 304, see `native-linux-required-gate/summary.json`.
The first round of 529 items resulted in **524 passes, 2 WPT failures, and 3 condition skips**, with CTest=8.
The three items are then executed separately under the root condition, in the same binary, 3/3 times. The original JUnit and the full completion status are retained.
However, this round of LastTest has been covered by the new checklist checks introduced during this round. Therefore, it cannot be considered as a complete output for the sub-items. The old logs
from item 527, or the JUnit truncation fields, should not be used as part of this round’s comprehensive report. For corrections and re-execution procedures, please refer to section 33.6.
Currently, new Windows conditions are added. Platform/workflow checks do not alter any of the three types of differences that have been registered for workerd:
illegal Close code receipts, limited synchronization of the sending queue, and absence of requests for subprotocols.

Node/Python native interoperability, exit control, DNS, stress/stage barriers, repeated packaging is still necessary.
WPT profile/time acceptance, Binding-origin WS, complete resources/OOM/GC/cancel combinations, v4/WS state machine fuzz,
sanitizer/soak mechanisms, native Mac-based systems, and actual hosted/released evidence – all of these aspects remain open for consideration.
T13 and the complete objectives are not selected together; this round represents existing progress, and the completion of the complete objectives cannot be marked.

### 33.6 List Log Retention and Explanation of TDD Corrections

The new list checks in this round directly invoke CTest `--show-only` within the currently running builds. In Linux,
the LastTest only contains the start and end lines. This raw, incomplete log has been saved, rather than being restored to a complete output. In Windows,
the first round of LastTest still includes 240 segments. These segments are checked individually, without any general extrapolations being made.

New controls have been added to manage the reading of the root directory and nested configurations in CTest. Pre-configured values include LastTest,
temporary logs, failure lists, and cost data. The number of bytes and file lists remain unchanged. In previous versions, 19 tests passed; now, only the
currently generated CTestTestfile.cmake files are copied to a separate temporary directory for processing. The original configurations and subdirs
are retained. Controls are set to 20/20. The temporary directory does not undergo product testing, so these controls do not simulate native operation.
Evidence is found in `native-inventory-log-red.*` / `native-inventory-log-green.*`.

Also, using the same PATH as the native CTest to diagnose the old `spawnSync('python3', ...)`, the actual error is
ENOENT; Private copies are only approved after absolute interpretation by the interpreter. Official `benchmark_tool_boundaries`
Please specify the required `--python` parameter from CMake. Check for spawn errors and retain the exit code and signal.
And stderr. The three controls of configuration path, missing parameters, and non-existent interpreter all meet expectations. Used for this tool
The interpreter and the fixed WebSocket SDK identity are recorded separately, not pretending to be the workerd client version for qualification.

For the subsequent portion of the output, use the verbose method for collection purposes. Also, ensure that the number of LastTest segments is equal to the number selected in JUnit.
The new native/Linux source code and build records are respectively stored in `native-source-logfix2.json` /
`native-build-logfix2.json`, `native-logfix2-linux-source.json` /
Save `native-logfix2-linux-build-result.json`; original failure records will not be overwritten.

Full target strict of the corrected native snapshot `29fca8eb69971cebb14046c78aea1d122c63aeb0`
Build passed. Re-run with the same CI profile ** 240 Item: 213 Pass 15 Failure, 12 Skip,
CTest=8**, approximately 300 seconds; benchmark_tool_boundaries has been successfully passed, while the remaining failures remain unchanged.
LastTest has 240 segments, which is consistent with the options chosen for JUnit and full registration. The SHA256 hash of the host is
`bbb61ebe27dfad386cd00bb2841de3ad76ab9bf5a74cbe4ee25b309265926f34`. The worker’s identifier is…
`b84c297c28a32acd1af36f900678b3abf7f7a163e54d7b092b840be10e0521a8`.
Document recorded as `native-logfix2-ci-profile.*`. The failure details can be found in `native-logfix2-failure-triage.json`.

The additional packaging sequence for the same revised snapshot is also retained: contents + metadata is processed initially on **2/2**. Subsequently, the processing of contents fails, and smoke
is not executed due to dependencies being unmet. The value of CTest=8 remains unchanged. The CPack output actually indicates that the process has been completed, and a package has been
generated. However, the SHA256 checksum and mtime of the final zip file remain unchanged. Therefore, the current check that requires “mtime to change”
cannot serve as a valid criterion for determining whether the process was executed twice. See `native-package-logfix2-sequence.json` for more details.
In the original LastTest, there are still cases where the “Command” or “Test Passed” fields are empty for tests that are not running.
JUnit records the failures as “NotRun”. Therefore, it’s not possible to simply count the number of “Passed” cases to determine the acceptance criteria. It’s necessary to verify the number of failures, skips, and
the actual execution status. Next, we need to isolate this issue temporarily. At the same time, we should reject any tests that involve no-op
operations, old files, or third-party products. We won’t accept any old ZIP files or ignore any fixtures that fail. This will help to fix the test issues.

The corrected Linux snapshot is `82d2f1201ae88a18b38142ca1e120fdc5cb86195`, which is the native version.
The 4,311 project files are consistent byte by byte; all build targets for Release/LTO have passed. Fixed gate has passed again.
**, 18/18 – No skipping required.**: Full coverage. **, 529 – No exclusions. 524: Passed. 2: One WPT failed. 3: Multiple conditions required.
Skip, CTest=8**, approximately 631 seconds. The**529 segments** of the complete LastTest are consistent with JUnit.
The original complete output is `native-logfix2-linux-full.*`, and logs that have been overwritten in the first round are no longer used.
The same binary is also executed under the root condition **3/3, zero skipped**, maintaining independent statistics for both runs.

In this round of WPT tests, a total of 84 reports and 14,060 individual tests were collected. Among these, 20 tests were reported as expected failures, 3
as unexpected failures, and 1 as a failure that did not occur again. This still corresponds to two failed CTest results. It cannot be concluded that all tests were compliant based
solely on the number of reports or the results of these tests. Observations revealed 62,418 samples, with 19 instances of offset jumps exceeding 1 ms in
duration. The maximum duration was approximately 1,761.135 ms. These observations were taken over the entire round of tests, and no attempt was made to determine the stable window of operation for any particular test.

The final evidence files can be found in the `native-verification-summary.json` and `index.json` files
located in the `build-v030-dev/evidence/test-integrity-tdd-20261015/` directory. All records of
complete failures, skipped tasks, excluded native packages, and additional packaging steps are retained. The `native-final-source-check.json` file ensures that the frozen
source code, source code and binary identities are correct, and that the HEAD of the original repository remains unchanged. After the snapshot was taken, only six review/execution documents
were updated; the tested code, tests, or workflows were not modified anymore. The `artifact-digests.json` file contains detailed summaries of each
artifact generated in this directory; older summaries are saved separately. Version 0.3.0 has not been submitted, pushed, or released from the original repository.
This round of review and corrections has completed verification. The complete objectives T13–T17 are still ongoing.

## 34 2026-10-16 Package the output, reproduce the input, and complete necessary tests for Windows CI. <a id="section-34"></a>

### 34.1 Replicating the Misjudgments and Omissions of the Old Checker

The previous round was about actual progress. This round continues with the complete objectives, focusing on addressing the packaging gaps that have been reproduced.
`test_package_contents.cmake` originally used the mtime to determine whether a new package was generated. This method could lead to false alarms due to duplicate
packages with the same content. Additionally, it only checks the capsid prefix, meaning it can handle incorrect package names as well.

A new parameter, `package_freshness_controls`, has been added. This parameter allows for the execution of zip and tar.gz formats
using either real CMake package targets or independent synthesized archives. Each of these two formats includes 13 categories: first-generation versions, identical versions,
versions from third parties, empty operations, legacy packages, third-party/mixed versions, incorrect package names, multiple Capsid packages, failed targets, missing
manifests, and cases where the work directory is located outside of the build process or is equal to the build itself. Old versions must be retained, both in cases of success and failure.
The composite package only verifies the inspection logic of the product and the manifest, and does not pretend to be an executable distribution package.

Excluding the package of the fixture itself, after retaining the configuration error, the normal positive examples on both platforms are passed first; valid
The red tests are `native-controls-red3.*` and `linux-controls-red2.*`, with 10 passes each.
16 failures. Do not consider these 16 failures as independent root causes of the product. Earlier attempts with baselinePassed=false
Retained as fixture preparation errors. The red-tested source code is saved in `source-red/tests/`, and the old gate SHA256 is
`18142b71fd4100cdae1a0e5e098fe1c600e6cdb900b237c83eeadd9ba1fa40b2`.

### 34.2 Correct the source of the modified artifacts and retain existing files

First, ensure that the work directory is a strict subdirectory of build. Then, create a separate run directory. Before moving, re-evaluate the actual
destination directory. The old archive, along with its SHA256 hash list, is retained in the previous-archives folder.
No longer recursively clearing the entire work/build, nor relying on mtime or requiring changes in content.

Then, call the actual command `cmake --build --target package`. This command prevents the generation of empty artifacts, third-party artifacts,
multiple packages, and non-standard files. Next, use the generated CPackConfig file to verify the correct package names. Finally, extract and check the frozen manifest.
When the target fails, the previous archive files are still retained. Both platforms achieve a control level of **26/26**, with zero failures.
This is reflected in the files `native-controls-green2.*` and `linux-controls-green2.*`. However, this only addresses the issue
related to product identification. It does not indicate that all testing, runtime, or release objectives have been completed.

### 34.3 Real distribution packages and corrections excluded from CI

The first round of freezing processes was successful for the Native `991b295a1a3f331bbc7fc82f49a1cfc4221adab1`
and Linux `4a0ea08c83b07a24ebb3f9f02f325ee62bd30984` platforms. All target builds have been successfully completed.
Actual Windows contents, metadata, freshness controls, and consumer smoke **4/4**,
Subsequently, again contents + smoke **2/2**, and the current reproducibility is comparable to **1/1** for gate.
Linux six-package related tests **6/6**. All have been executed, with no skipping; none of the previous NotRun tasks have been successfully rewritten.

Windows CI has removed the exclusion of the entire worker_package, allowing only the exclusion of unconfigured WPTs.
The audit process involves checking for uniqueness based on the actual CTest commands. Duplicate or unreferenced items, as well as those excluded due to incorrect package names, labels,
regular expressions, or file lists, are not allowed to be included in the test items. First, ensure that any old workflows are rejected by the new checks. Then, modify the workflow accordingly.
Added persistent `testing_validity_workflow_controls`, **35/35** for both platforms, including health
The baseline includes 34 actual private file variants; it is registered with CTest, no longer relying solely on manual negative control in the workspace.
This is a static workflow control, and the Hosted job has not been submitted yet.

### 34.4 The dependency prefixes for the reproduction build must not be omitted.

The actual Native build was successful. After setting the values of CMAKE_PREFIX_PATH to release/debug, the build process was completed.
The new cache only retains the first item. After adding assertions that match the complete input to the cache, the actual configuration is performed.
The specified diagnosis failed in the red test, see `repro-prefix-red.*`; retain the precise red test script.
The CMake list separator has been escaped. Continue to retain this assertion, which will be verified by the reproduction tests in the subsequent formal full-scale implementation.

The current comparison of existing access control systems involves comparing a baseline package with a newly built version, and identifying any allowed binary differences between them. However, there is still
need to complete the process of creating two completely new versions of the system, as well as ensuring that every byte of the T17 requirements is accurately replicated. Therefore, the current iteration of the gate process cannot be used as a substitute for completing these tasks.

### 34.5 Stages of Native Interoperability Failure Further Classification

The independent diagnostic copies only include phase and message logs, and are not used for acceptance criteria. In the single-worker scenario, 22
observations were collected. The failure occurred due to a Python peer stop condition: snapshots were returned, but the “stopped” report timed out.
The log contained a Proactor ConnectionResetError. The final comparison and normal exit checks have not yet been completed. Therefore,
it cannot be claimed that all 22 observations meet the acceptance criteria. Additionally, this failure cannot be mistakenly attributed to a failure in the message echo phase.

In the first Node ws exchange, the first two lines of text and the empty binary data were echoed. After
sending 180,000 bytes, the result was **1006, empty reason, wasClean=false**—which clearly contradicts the expected outcome.
This involves actual data paths, and it’s necessary to continue distinguishing between segmented IO operations, credit-related processes, and final-state processing tasks. These issues cannot be attributed
to any Python-related problems or relaxed message assertions. Evidence can be found in files
`interop-diagnostic-results.json` and `interop-close-diagnostic-results.json`. The actual code and assertions are not replaced by these diagnostic reports.

The next steps involve enabling Python-based TLS/STOP mechanisms for independent lifecycle processes, implementing segmented IO barriers for large messages in the
static-pool mechanism, and continuing with the normal shutdown process of the Windows Host system. Additionally, DNS queries and stress tests will be terminated.
WPT profile, clear differences between the three types of workerd, Binding-origin WS, resources/OOM/GC/cancel,
state machine fuzz, sanitizer/soak, native macOS, Hosted, and T15–T17 are all kept open for use.

### 34.6 Final freeze and complete regression

The corrected version of the Native file is `c099f568853df5c29121e5961863f0849270e9a3`. For Linux systems, it is
`6446c85e0655c806df54b735e7ddcf9b9d03641f`. Both versions contain the same 4,313 project files.
The build process was carried out with great care. The complete Native profile includes all necessary packages, and no elements from Linux system are excluded.
The final results are stored separately in the directory `build-v030-dev/evidence/package-freshness-tdd-20261016/`.
The actual results and complete outputs will be stored in this directory. They will not be combined with those that achieved success in the initial phase to form a comprehensive list of successful outcomes.

Complete Windows **246 items: 218 passed, 16 failed, 12 skipped, CTest=8**, approximately 559 seconds.
All gate checks passed; formal reproducibility verified with cache retained, release/debug still in place.
Two prefixes, see `native-repro-prefix-green.json`. This round is `worker_fetch_cancel_lifecycle`.
Another failure has occurred where no connection closure was observed. It is necessary to check the reproduction conditions. We cannot simply continue with the previous 15 failures.
The complete LastTest 246 sections are consistent with JUnit, and the binary identity remains unchanged throughout the round.

For Linux systems, the workerd access control is set to **and 18/18. There is no skipping for**. All items from ** to 531 are included; no exclusions are applied for 526.
2 WPT failures, 3 conditions skipped, CTest=8**, approximately 662 seconds. The same binary was subsequently in the root.
Under the conditions, run **3/3, no skipping**. Count separately under both conditions. The complete LastTest has 531 segments.
The 84 WPT reports contain 14,060 sub-items: 20 registered expected failures, 3 unexpected failures, 1
No more expected failures have occurred. It is still not possible to claim that all WPTs and all workerd behaviors are compatible.

Frozen Binary SHA256 (Host / Worker): Windows
`3985ae020e8c4bd494376ccdf90a3d43856c7fce93da0b86d8a8395ec1571590` /
`beda9228ba78f527dd81edc362125b6be154fa8de7bd034bfc26723e2bcf4fc1`; Linux
`3419fff26b2238ac7999ecdd66c75bf536945e92778dfc37ae6fe77dcfb01979` /
`c9a68c4f59c5dd4f9b25c8e01b1a72fe24f0fd6ea6ae193f694e6f9b808c7931`.
The subsequent socket executor regression will be included in the next source code snapshot. We have already covered all the necessary tests in this complete regression.

## 35 2026-10-16 Windows Socket Executor: Red Testing and Handing Over to the Next AI <a id="section-35"></a>

This round is organized according to user requirements for handover. The next entry point is [Next AI will take over the task.](next-steps-handoff.md).
Currently, the red testing has been completed, and the product implementation is pending refinement. The complete objectives have not been marked as completed.

### 35.1 The heap corruption has been further localized to two reactors

An independent Release copy, under the same 180,000 bytes of input, resulted in a Host exit error of `0xC0000409`.
The exception observer log for this copy, `0xC0000374`, has a heap corruption. An independent MSVC ASan has been performed subsequently.
Reproducing the exception of `StreamDecoder::consume`: The call stack is located through `on_event/pump_input`
T3 shard thread; assignment for the same `input_` comes from `WebSocketSession::read`
Callback, located in the T4 accepting thread. The original error type is ASan `unknown-crash`, not
A data competition report for TSan has been run.

Source code checks are consistent: In Windows, the pool acceptor uses `accept_ioc_`.
Moving a socket to `adopt_socket` does not change its executor; instead, worker events are sent to
the shard’s `ioc_`. Network callbacks and worker credits are handled by different threads, which violates the principle of single-thread operation.
The hypothesis that the array used for previous reads becomes ineffective when the vector grows in size was incorrect. In reality, the `read_bytes_` value remains fixed throughout the reading process.
Therefore, there is no need to relax the constraints on the buffer size or the protocol’s upper limits based on this fact.

The evidence includes `interop-status-diagnostic-results.json`, `interop-exception-diagnostic-results.json`,
and `interop-asan-runtime-diagnostic-static-pool.log`. Private diagnostic codes are used solely for logging purposes;
they do not replace official interoperability assertions. The final comparison/normal exit status still fails.

The ASan preparation process is also retained: only the first round of processing involves vendor-specific STL annotation handling. In the second round,
global C/C++ instrumentation is used to complete the construction process successfully.
The ASan Worker still needs to have a matching runtime DLL placed alongside it, as the minimum physical environment does not have a PATH.
The attempt to mix ASan Host with Release Worker has been correctly rejected due to compatibility issues with identity checks.
These diagnostic premises have not yet formed into project ASan engineering fixes or a complete sanitizer qualification.

### 35.2 New tests have failed on the old implementation

The snapshot `d5bd28c4bd01ab791316c1286e50ab8cd523466b` is a fully functional build. It retains the old product implementations.
The following tests have been added/enhanced:

- `host_single_worker_controllable_lifecycle` adds actual foreign-executor connections.
  The old implementation failed due to a clear failure to accept the connection; the fixture reclaimed the thread and Worker before the report.
- `host_static_pool_websocket_io_1` / `_4` utilize 1/4 of the Worker resources, with four-way concurrency.
  text/binary each check 0, 1, 125, 126, 65535, 65536, 65537, 180000, 1048576
  The complete content and types of bytes are checked for normal Close, normal stop, and Worker recycling.

Currently registered 248 items. This time, only the above **3 items** are selected. 0 passed, 3 failed, zero skipped. CTest=8.
The data cases appear to have occurred at indices 10060/10054. It cannot be determined whether each failure was caused by another instance of heap corruption. These cases still require
verification through healthy Linux tests and successful repairs. The original JUnit files, the complete LastTest results, as well as
information regarding the build/code sources, can be found in the files `native-socket-red.*`, `native-source-socket-red.json`,
and `native-build-socket-red.json`. No complete set of 248 acceptance criteria has been met, and there are no successful cases after repairs.

### 35.3 Handover Boundaries

**The product code has not been modified yet to fix the issue with the socket executor.** Next, let the accepting process use the target shard executor. Keep the ownership
verification and legitimate ownership cases intact. Overcome the challenges related to worker exit, pending acceptance, cessation, and the absence of available
workers. Then, convert the red tests to green ones, and perform formal SDK tests, comprehensive regression tests, and ASan tests with the same input conditions.
Fail to reduce messages, delete cleanup assertions, or add CI exclusions for fixes.

Subsequently, the following operations were performed: handling Python peer TLS/stop issues, normal termination of actual Windows hosts, real DNS
queries, cancellation of stress tests, and failure in canceling the current fetch process. In section 34, the complete regression process remained intact, with its independent snapshots being preserved. Differences
between WPT, various types of workerd, Binding mechanisms, resources, dynamic/native configurations, Hosted environments, and T15–T17 scenarios were also preserved.
The original repository HEAD remains unchanged. There are no original repository commits, pushes, or releases. There are no running tasks to be collected during the handover.

## 36 2026-10-16 socket executor - Local Fix and Cross-Platform Verification <a id="section-36"></a>

Take over [Next AI will take over the task.](next-steps-handoff.md) Section 4 Task: Implementing Windows
Static pool socket executor bug fixed, three red tests turned green and related full regression tests were completed. Original repository
HEAD remains `8028b88`, not committed, not pushed, not published.

### 36.1 Product Fixes

- `SingleWorkerServer::accept_executor()` added: returns the executor with the shard `ioc_`, for
  During `async_accept`, the pool directly creates the accepted socket on the target shard reactor.
- `Impl::adopt_socket` adds an executor ownership verification: The executor of the socket does not change during movement, coming from
  All sockets of other reactors are rejected (returning false, and the caller is responsible for closing them), to prevent network interaction.
  Callback and sharded Worker events concurrently modify the same session state.
- `StaticPoolServerImpl::do_pool_accept` (Windows branch) Override: **Before acceptance**
  Current rotation rules select target shards (prefer those with available Workers; if none are available, rotation continues)
  Select to keep the shared listener accepting, and the completion will be re-verified by adopt. Subsequently,
  `accept_executor()` is an overload of the target's `async_accept` that ensures the socket is assigned from the beginning
  Target shard. The safety basis for the shard pointer: The facade holds the pool Impl during the stop/wait period and accepts
  The processor captures weak_ptr, and the shard is only destroyed after the accept thread joins. Original stop/fail recheck, transient state
  Accepting error handling and socket ownership semantics is retained.
- Positive example: The scenario of "legal ownership" has been added for `host_single_worker_controllable_lifecycle` - using
  The same `async_accept(executor, …)` path creates connections on the shard's own reactor, assertion
  Adopt: Success; HTTP round-trip completed on the same reactor and stopped normally.

### 36.2 Verification (All evidence is retained in package-freshness-tdd-20261016)

| Verification | Evidence | Results |
| --- | --- | --- |
| Linux Health Baseline (Red test cases based on old implementations) | This round of run records | `host_single_worker_controllable_lifecycle` is red (Deterministic Affinity Assertion); `host_static_pool_websocket_io_1/_4` is green (Linux shard-owned listener, does not use pool acceptance path) |
| All groups on the Linux host (development build) | — | 232/232 passed (including three socket tests; the jitter of `host_channel_generation_lifecycle` under `-j3` parallelism was passed in serial and direct runs 3/3 times) |
| Windows Snapshot/Construction | `native-source-socket-fix1.json`, `socket-fix2`, `native-build-socket-fix2.json` | Snapshot `f6c01194`, Release build passed |
| Windows Three Red Tests | `native-socket-green1.json` | 3/3 passed, zero skips, binary identity recorded |
| Windows-related Regression Tests | `native-socket-related1.json` | 14/18; all 4 failures were due to existing lists (Python peer, SIGTERM stop semantics) |
| Windows Full CI Profile | `native-socket-full1.json` | 248 items: 221 passed, 15 failed, 12 skipped; **Zero new failures**, `worker_fetch_cancel_lifecycle` passed this round; Intermittent failures due to cancellation have not been identified and are not attributed to this fix; The remaining 15 items are consistent with the list of 16 items from the previous snapshot. |
| Independent ASan Build (After Fixing) | `native-asan-socket-fix2-build.json` (new script `native-asan-socket-fix2-build.py`, global instrumentation, retains the old `asan-20261016` red-test build) | Build passed; host/worker SHA256 has been recorded |
| ASan is equivalent to re-running the input. | `interop-asan-socket-fix2-static-pool{,-run2/3/4}.log` | 4 rounds of static-pool interoperability: zero AddressSanitizer reports; Node swap of 180,000 bytes completed per round (run1 terminated due to Python peer deadline, run2–4 also passed even without Python peer, ultimately only documented Windows SIGTERM normal termination assertions remaining) |
| Linux Snapshot/Construction/Full Set | `socket-fix2-linux-source.json`, `socket-fix2-linux-build-result.json`, `socket-fix2-linux-full-summary.json` | Snapshot `36ea8fbe`; 533 items tested: 528 passed, 2 failed, 3 skipped. Two failures are identical to those in the previous snapshot (due to WPT promise-rejection and MessagePort issues). Two additional items passed using websocket_io. All binary identities, LastTest integrity checks, and list consistency tests were passed. |
| Linux Fixed Workerd Gate | `socket-fix2-linux-required-gate-result.json` | exit 0 |
| Linux root condition recheck | Unimplemented | This session does not require password-free sudo access. The two sandbox condition tests do not overlap with socket-related fixes. We retain evidence from the previous snapshot 3/3. The next participant will conduct additional tests in a rootable environment. |

### 36.3 Boundaries

- Fixations are only for the ownership (socket and Worker events are the same reactor); the rest recorded in section 35
  Pending touches: Python peer TLS/stop, Windows Host truly stopped normally, real DNS
  query, pressure cancellation, differences between the three types of workerd, Binding/Resources/fuzz/soak, Hosted,
  T15–T17 are not closed. The two sets of completely rebuilt code and byte-by-byte reproducibility tests are still not completed.
- The official interoperability runner and the red test assertions have not been rewritten; the ASan verification uses the official runner and the official version.
  Testing, no private diagnostic sources used. `native-run-ctest.py` has a unique label for each round, with no coverage.
- All snapshot submissions in this section are located in the isolated clone (`source-20261016`) and
  `/tmp/capsid-package-snapshot-20261015`), the original repository working directory remains dirty.

## 37 2026-10-16 socket Fix Review and Python Peer Continuation <a id="section-37"></a>

### 37.1 Review Conclusions and Record Corrections

The socket attribution fix can be retained. Compared to `d5bd28c4`, the product changes are limited to the sharding executor interface.
Adopt a rejection of ownership and accept the connection path of Windows; original data is tested red, formal interoperability is ensured, and CI excludes non-reduced versions.
The HTTP positive example with the executor has verified that network callbacks can indeed work with the sharded reactor. Adopt in calling
After the entry and IO delivery are completed, the availability of Workers is checked; the stop path terminates the accept thread first, followed by the destruction.
Sharding. The statement "pending handler holds a shared_ptr" in the source code is inaccurate. It has been changed to "the facade holds a shared_ptr in stop/wait".
During the period, Impl is held; the handler actually captures weak_ptr. Further coverage of preselected shard exit and suspension acceptance is still needed.
  The completed precise interleaving should not be considered as proof of all interleaving by treating the existing start/stop stress testing as such.

Evidence verification can be found in `socket-review-peer-tdd-20261016/audit-new-changes.json`: The native code
freeze results show that 4,314 files match exactly with the original repository’s results. There are only two cases where the results differ from this snapshot. In Windows, there are 3 green tests, 18 related
regression tests, and 248 full-scale tests. In Linux, there are 533 full-scale JUnit tests. The results are consistent with the provided data.
The binary hash values match those of the full-scale regression tests. No instances of green testing were found due to deleting test cases, modifying
message content, replacing protocol SDKs, or expanding the skip ranges.

Three P2 records that need to be corrected: In the previous round, the `handoff-summary.json` still indicated that the issue was not fixed by the product team. The source-check
function is still linked to the old red-tagged versions. Only `index.json` contains discrepancies in its digest. The attempt to cancel the fetch operation
was mistakenly recorded as having been fixed. This round, we will retain the old evidence and provide the current summary using the new directory structure. The intermittent failures that were
previously closed will continue to be allowed to proceed. It should be noted that the testing path does not use external-accept, resulting in a lack of causal evidence.

The root condition verification for the socket fix snapshot `36ea8fbe` has been passed. The actual test run was conducted
using `wsl -u root`: 3/3, zero skips, unchanged binary code. All three sections of the JUnit and LastTest tests have been archived. Therefore,
the limitation imposed by the previous round of testing—“No sudo access allowed”—is no longer applicable in this environment.

Section 36 development build 232/232 lacks corresponding archive paths compared to the old Linux health baseline. Re-review does not include it.
They are considered additional independent qualifications; a verifiable complete snapshot test has already covered the relevant items. ASan four-wheel log
Prove that the described major message exchange is correct, and no ASan errors are reported. In the event of a complete interoperability failure, the logs will still be retained as originally recorded.

It is necessary to tighten the coverage description for one point: The static pool Beast client uses default automatic sharding, and currently it is locked to Boost.
The default write buffer is 4 KiB. Therefore, inputs like 65,535/65,536 are **message size** boundaries, and cannot be treated as such.
They claim to cover end-to-end processing of single frame 64-bit length conversions. `test_ws_stream.cc` contains the actual code for this.
180,000 bytes per test frame. Formal Node/Python interoperability also includes large messages; precise frame boundaries should still be maintained.
Add scenarios where automatic sharding is disabled and observe the actual frame header. This gap does not deny the concurrent red testing of sockets.
However, the overall agreement matrix remains unfinished.

### 37.2 The effective red-testing and correction range of Python peer

The new directory `peer-formal-red1-run1.log` uses the original formal runner, fixed SDK, and existing Release.
Binary reproduction of Proactor `_call_connection_lost(None)` with result 10054 and peer stopped
Timeout. Fixed local bytecode for CPython 3.12.3
[Upstream Corresponding Implementation](https://github.com/python/cpython/blob/v3.12.3/Lib/asyncio/proactor_events.py)
Mismatch: An exception occurred during socket shutdown, and subsequent close and server detach operations were not executed.

Windows auxiliary programs now use the Selector event loop provided by CPython. It only uses TCP/TLS.
stdin is read through `asyncio.to_thread`, not relying on asynchronous pipeline/subprocess APIs that Selector does not support.
Platform capabilities can be found in [Python asyncio Platform Description](https://docs.python.org/3.12/library/asyncio-platforms.html).
The versions of Python, websockets, Node/ws, as well as the source code details remain unchanged. All aspects of the original protocol, error handling mechanisms, and requirements for
stopping and normal exit procedures are also retained. In Linux systems, the original default settings are still used.

New fields have been added: `websocket_python_peer_{idle,exchange,tls-reset,reset-session,close-reset}`.
These fields track various conditions encountered during communication with real Node/ws
clients and Python peers, including idle states, complete text/binary messages, round-trip limits of 180,000 bytes, fragmentation/Pong
mechanisms, certificate rejection issues, RSTs during TLS handshake, RSTs within sessions, and RSTs after sessions are closed. It is important to retain any failure logs and to exit
the process with a non-zero status. Failure to do so should not be considered a successful outcome. These independent controls were also successfully implemented in the old cycle. However, they
cannot be treated as deterministic Proactor tests. A valid test would be conducted during the official interoperability tests mentioned above.

The five types of controls have been modified and approved. In three consecutive rounds, the static-pool system generated reports indicating that the 22 scenarios were operational. However, the peers
still experienced issues such as peer stopped, active=0, exit=0, with no stderr output. Ultimately, the system still failed due
to the Host’s SIGTERM exit status. The “peer-green” designation in the file names refers only to the peer’s lifecycle, and does not indicate that the entire system has passed all official testing criteria.

### 37.3 Construction of frozen structure and complete regression

The Native snapshot is `b6c587886a49f0ffa826cbd337076049126b4954`. The Linux snapshot is
`cf2833eb155dd1ae9baf735dcc7cada622e7e833`. Both snapshots contain **4,315 project files**.
Both platforms have successfully passed all build validation criteria. Linux continues to use LTO for its operations. Only the result documents will be updated in the future;
the code, tests, and workflows remain unchanged from the initial state.

| Verification | Results | Records in the current evidence directory |
| --- | --- | --- |
| Two-platform socket, peer, and registration-related CTest | Each 10/10, no skips; Registration control per platform 43/43 | `native-peer-related1.*`, `linux-peer-related1.*` |
| Windows Complete Current CI Profile | 253 items: 226 passed, 15 failed, 12 skipped. CTest=8, approximately 567 seconds. | `native-peer-full1.*` |
| Linux Full Zero Exclusions | 538 items: 533 passed, 2 WPT failures, 3 conditional skips, CTest=8, approximately 657 seconds | `peer-fix2-linux-full-*` |
| Linux Fixed Workerd Gate | 18/18, zero skips | `peer-fix2-linux-required-gate/summary.json` and JUnit/LastTest in the run |
| Subsequent root condition verification for the same Linux binary | 3/3, zero skips, exit 0 | `peer-fix2-linux-qualified-root.*` |

The list of failed cases during the full regression is identical to that of the previous socket repair snapshot. There are no new failures reported. Both interoperability
tests in Windows modes include observations from scenario 22. The Python peer processes terminated normally, with no stderr output. However,
the final assertion based on code/signal failed due to host shutdown. Overall, interoperability cannot be considered successful.
Fetch-cancel has passed this round again, and the intermittent issue remains open; root cause identification has not been replaced with single or continuous passes.

All packaged gates have actually passed. The complete LastTest has 253/538 sections, and JUnit matches the registered list.
The binary hash values remain unchanged throughout each round. The latest Linux WPT contains 84 reports with 14,060 items: 20 items
Expected failures, 3 unexpected failures, and 1 non-recurring expected failure; no extended waiver or rewriting upstream
Assertion. Clock samples are also saved with the complete output, but it's just an observation over the entire cycle, and should not be considered a stable window for each test case.

The post-mortem audit also identified 49 surviving historical diagnostic processes in WSL (26 Hosts, 23 Workers).
From `/mnt/e/capsid/build-v030-dev` and `/home/eroszhao/capsid/build-v030-dev`, started on
10 Month 14 – 15 Day. In this round `/tmp/capsid-package-build-20261015` There are no surviving processes under the path.
The current round of Windows Host/Worker/Construction/CTest processes has also ended. Historical PID, commands, and startup times.
Archive separately in `linux-process-audit.json` / `linux-process-classification.json`, keep
For subsequent verification and targeted cleanup tasks; this round does not terminate the processes of these other builds, nor does it claim to encompass the entire environment.
No remnants remain. These environmental facts need to be verified separately when comparing performance, resources, and the number of global processes in subsequent comparisons.

The current evidence entry is `socket-review-peer-tdd-20261016/index.json`; the final summary comes before the digest.
Write. The index mismatch of the old directory remains unchanged and is recorded separately for audit purposes. The original repository HEAD `8028b88` remains unchanged.
Not submitted, pushed, or published. The next step is to observe the normal shutdown of Windows and the teardown for preservation of first principles;
DNS/Cancellation, Pending Acceptance and Worker Exit Interlaced, Precise Single Frame Boundary, WPT, Remaining Workerd
Differences, resources/Binding/fuzz/sanitizer/soak, native macOS, Hosted, and T15–T17 are still unfinished.

## 38. 2026-10-16 Handoff: Normal Exit, Backpressure, and Precise Frame Boundaries <a id="section-38"></a>

This iteration continues from section 37. The product and tests have been modified, and the verification process has been completed. The HEAD of the original repository
remains `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`. No commits, pushes, or releases were performed for the original workspace.
Evidence directory: `build-v030-dev/evidence/parent-stop-tdd-20261016/`. This section replaces the previous description of the number of tasks pending and current failures; historical failures are retained.

Windows MSVC Strict Release full set **282 items: 270 passed, 0 failed, 12 skipped conditions**; Linux Strict Release/LTO full set **570 items: 565 passed, 2 existing WPT failures, 3 skipped conditions**. Fixed workerd gate **18/18, zero skips**, followed by the same Linux binary tested with root **3/3, zero skips**.

### 38.1 Product and Test Corrections

1. `ParentStopMonitor` and the optional `--stop-fd` have been integrated into two local modes. Reading the bundle or starting.
   The Worker has a conflict with the previous validation descriptor, type, and READY, so an non-inherited copy is used; requests are stopped by bytes or EOF.
   Cancel both startup failures and normal endings, join listener. The static pool waits for the path to respond to an explicit stop; POSIX
   The process signals are distributed via Asio, and Windows retains the console event interface.
2. The active inbound WebSocket sends a 1001 Close when it stops normally, waits up to one second for a response, and then ends the transmission.
   Stop the Worker. The original order of requesting Runtime shutdown and directly disconnecting TCP has been adjusted. Idempotency is retained.
   Stop, naturally empty the io_context and Worker join. Add real process scenarios for Windows 24 items, POSIX
   26 items, covering idle, EOF, repeated stop, active WS, no Close response, pending Upgrade, startup
   Failed and invalid descriptors, POSIX also includes the actual SIGTERM. Normal operation persists with code=0, signal=null.
   No forced termination and actual Worker PID recycling.
3. When the write queue is full, the Worker response header retains a bounded pending header, which precedes the body/end; cancellation or error.
   The final state discards the uninitialized head. Both protocol versions v3/v4 have added a 16-owner completion barrier, using a 4 KiB wire queue.
   Check the complete bytes of the main text, short texts, null body, streaming main text, representation, and their respective final states. Original
   The outbound result-ACK for stress scenarios and queue saturation tests has also passed.
4. Windows DNS test clients receiving delayed responses from the canceled resolver will receive ICMP Port Unreachable.
   The next UDP receive report is 10054. The testing side configures this report using Winsock's `SIO_UDP_CONNRESET` setting.
   Do not swallow actual DNS errors; each round of cancellation should use a separate query name, and cancellation should only occur after the current query is seen. Old messages should be discarded.
   It cannot serve as a progress barrier. The actual A/AAAA, NXDOMAIN, 64 cancellations, and late responses are retained. This CTest
   There are still three Linux-only stalled-TCP sub-scenarios, so it cannot be claimed that the Windows TCP phase has been fully verified.
5. The host test helper stops the local host through a private pipeline and waits for the child process to close the stdio; managed and
   reference retains their respective process signal paths. fixtures/budget uses `withCleanup` for cleanup, while retaining
   Primacy, error cleanup, and cause; four types of combined control and real Node child processes concurrency pass. Original
   Node forces the entry point of `process.exit()` to set the exitCode and end naturally; native mutation
   The baseline and all 14 faults still require precise exit states and specified assertions; no relaxation of the large exit code is allowed.
6. The complete regression found a competition between Python peer close-reset: `close()` does not throw an incomplete handshake exception.
   Firstly, check the bidirectional Close operation. This will result in RST being classified as an AssertionError. For now, let’s use recv to determine whether there is a connection loss. Then…
   Verify normal handshake; faulty clients send RSTs without responding after the SDK parses the actual server Close.
   All 32 failures must be of the ConnectionClosedError type and must include the phrase "no close frame received"; normal
   The code/reason that still requires two-way Close must remain identical. The SDK version and the source code summary remain unchanged.
7. Added independent original TCP frame scenarios, with no automatic splitting by Beast/SDK: text and binary covered separately
   0, 1, 125, 126, 65535, 65536, 1048576 bytes. Check FIN, mask, RSV, specification length encoding.
   Exact text and Close. The static pool uses a matrix of four connections. Windows has two modes, Linux has three modes.
   Pass. This item is the end-to-end verification on the Capsid side, and the workerd original frame difference with the same precise size has not been listed individually yet.
8. CTest registration requires that the aforementioned stop and frame cases actually exist and cannot be DISABLED/WILL_FAIL/skip.
   The latest synthesis inventory control is 132/132; this is only a registration verification, and the actual execution will be confirmed by a complete regression test.

### 38.2 Boundaries of Red Testing and Preparation Errors

- `stop-red2` is a valid six-item old implementation red test: READY, receiving a stop notification after a genuine health request still results in a timeout.
  `stop-red1` lacks a fixture pool configuration, which is a preparation error. The active WS observation for `stop-green1` is
  1006, corrected the Close sequence and turned it green; `stop-green2` is active 24/24.
- First version v3: There is a configuration error in the stress test capability; `native-head-red2-dns-green1` is the correct version for v3/v4.
  The valid red test with the response header encoding failed. `native-head-green1` only selects four items.
  The saturation regular did not match the old tests; the `native-head-mixed2` indeed executed the original saturation CTest.
- `native-head-mixed1` has an incorrect assumption about the fixed-body flag for an empty string; explicit null is used in the final implementation.
  The regular head/end routing of the body maintains the complete content and sequence assertions. The original frame fixture was omitted in the first version.
  arraybuffer, the default string representation of Blob, corresponds to workerd, which is a fixture error.
- Two attempts with cleanup-close did not reproduce the old waiting issue and cannot be counted as a red test. What is actually in the mutation?
  The observed libuv exit assertions are retained; the waiting for child to close has been modified together with the natural exit and cannot be attributed separately.
- The first round of full `native-stop-full1` resulted in 280 items, with 267/1/12 completed. The only failure was in the close-reset process; Linux
  `stop7-linux-full` consists of 567 items, with 562/2/3 remaining. Both rounds have been archived as they were originally planned. Certainty
  `peer-close-reset-red` → Only after `peer-close-reset-green` is the final source code frozen and the execution is re-run completely.

### 38.3 Final Verification of Identity

| Project | Evidence |
| --- | --- |
| Windows freeze of source code | `4b5ec82e10211a7e085748e319308b33ade02e22`; `native-source-stop8.json` |
| Linux Frozen Source Code | `b8c76aabad62311f445e6d301f2d0ceedc71c4a9`; `stop8-linux-source.json`, identical to Windows, contains 4320 project file bytes |
| Windows Complete Regression | `native-stop-full2.json/.xml` and the complete LastTest, 593.343 seconds |
| Linux Complete Regression | `stop8-linux-full-summary.json`, JUnit, Complete LastTest, 676.246 seconds |
| Fixed workerd | `stop8-linux-required-gate/summary.json` and this run's JUnit/LastTest, 18/18 |
| Root condition verification | `stop8-linux-qualified-root.json`, Summary for the Same Linux Host/Worker |
| Formal Native Interoperability | Two modes, each with 22 scenarios; Host exits normally, Python peer in active=0, exit=0, signal=null, stderr empty |
| Independent Frame Cases | native single-process 14, static pool with 56 complete echo frames; Linux includes 14 managed instances |
| WPT | 84 reports, 14060 sub-tests; still expected to be 20, unexpected in 3, missing expected in 1 |

Each time CTest is executed in a serial order based on the build process, it first archives the complete LastTest/JUnit files, and then proceeds to the next round. All the summaries of Host/Worker processes before
and after the final execution are consistent. `summary.json`, `index.json`, and the generated digest are the key evidence for this round of operations.
The result document can be delayed compared to the frozen source code. Any discrepancies must be specified and limited to the documentation. The product, testing, and workflow sizes must remain consistent.

### 38.4 The 0.3.0 project that is still unfinished

Retain the two WPT default-workerd profiles of divergence and outdated expectations, without modifying the standard assertion to green.
The three types of differences in workerd that remain closed: close-code 999, limited sending queue, and subprotocol not requested.
It is also necessary to accurately handle issues related to pending-accept/selected-Worker-exit interleaving, Windows stalled-TCP scenarios, cancellation phase barriers,
fetch-cancel recurrence verification, failure control situations where both the actual host factor and teardown fail simultaneously, differences in original
frame references, as well as parameters/origin/Binding/CPU/heap/OOM/GC/resource matrix issues.
MSVC ASan vendor annotation, project fixes, state machine fuzz, sanitizer/soak, native macOS,
Hosted, T15–T17, and two newly built byte copies have been fully qualified and accepted. All T-level items are still proceeding as planned.
Keep the status unfinished. It cannot be claimed that the entire 0.3.0 version is completed or fully compatible with workerd just because of this round of Windows failures.

## 39. 2026-10-16 Fixing the original workerd frame comparison and gate reinforcement <a id="section-39"></a>

Continuing with the original frame gap in section 38, it has been implemented and verified. The product source code at this stage is the same as stop8. The modifications focus on testing, fixtures, CTest registration, and gate management.
The original working directory HEAD remains `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, and has not been committed, pushed, or published.
Evidence entry `build-v030-dev/evidence/raw-frame-reference-tdd-20261016/index.json`, the final summary and digest are in the same directory.

Windows Strict Release total: **283 items: 271 passed, 0 failed, 12 skipped conditions**; Linux Strict Release/LTO total: **572 items: 567 passed, 2 existing WPT failures, 3 skipped conditions**. The fixed workerd gate has been expanded to **22/22, zero skips**, and the same Linux binary root has been tested **3/3, zero skips**.

### 39.1 The same real transmission matrix

- Extract the common `tests/fixtures/websocket-frame-boundaries.mjs`, and execute it on the locked workerd and Capsid with single-worker, static-pool, and managed configurations.
  Each backend has four real TCP connections, with text and binary data covering **0, 1, 125, 126, 65535, 65536, 1048576 bytes**, resulting in **56 complete echo frames**.
  Client handwrote a FIN masked frame, unwrapped the TCP header, and wrote the contents. It parsed the actual returned frame's FIN, mask, RSV, opcode, and the specified 7/16/64-bit length, as well as the complete text.
  The large text contains UTF-8 emoji; both the large text and binary data are marked with the current challenge. All connections have completed the actual two-way 1000 Close, TCP termination, with no additional frames, and subsequent health requests have been successful.
- Windows executes the same four connection matrix in both modes, with 56 frames per mode. Linux's four backends report using the same gate runId, allowing for comparison item by item; there is no dependency on SDK for automatic splitting.
- `tools/workerd-frame-contract.mjs` verifies a complete and strictly ordered list, and independently reconstructs the summary of the specified challenge.
  Even if all backends simultaneously test small frames and forge the same hash, they cannot pass by being equal to each other. The verification includes the profile/client version, four input summaries, binary summaries, health results, and the actual stop status.
  Write `complete=true` only after the cleanup is complete; running or failed cases will be overwritten to indicate an incomplete report. The candidate Host still requires code=0, signal=null, and forced=false.
- The fixed workerd version, compatibility date/flags, and SDK source lock have not changed. The actual shutdown status of the reference is still verified separately, and no relaxation of the candidate Host is allowed based on the reference signal.

### 39.2 Reproducible gate missed detections and positive/negative control

`gate-frame-red1` uses an old gate and new controls: the full baseline passed, but all eight rejection cases reported `Missing expected exception`.
Prove that the old gate indeed did not check these original frame reports. After expanding the required CTest from 18 items to 22 items, the access verifier was used.
`gate-frame-green1` is **9/9**. Eight scenarios involve removing the backend, marking as incomplete, changing the runId, altering the input summary, causing all backends to lose long frames at the same time.
Let all backends simultaneously modify the payload summary, the Worker summary, and the normal exit code.

A new file `tests/test_workerd_frame_contract.mjs` has been added, accounting for **46/46** of the total. This file also covers scenarios involving FIN/mask/RSV, binary types, encoding boundaries at 125/126
and 65535/65536, duplicate/unordered lists, failures in Close/TCP processes, as well as issues related to SDK/profile/input/binary drift. The CTest configuration is registered independently, and the same file is also used for controlling Linux-based systems.
The combination of cross-report tests and the root directory of the actual CTest registration control results is **282 items: 281 passed, 0 failed, 1 Linux installer single test skipped under Windows conditions**.
These archive candidate samples are synthetic copies reported by actual workers, and are used solely for verifying the rejection logic. The actual execution on the four backends is additionally verified by necessary gate checks and complete regression tests.

The preparation error is also retained: `reference-sample` failed because the historical naming runId did not meet the initial UUID restrictions. It belongs to the testing metadata preparation. The final acceptance was a bounded security. label/UUID is still strictly bound to this round of challenges.
`frame-contract-green1` Subprocess actually exits= 0 However, the script arranged at the end of the log triggered a GBK encoding error during printing. Green2 corrected the output.
`all-contracts-green1` does not provide an explicit executable path required by the real CTest registration control. green2 has passed after environment supplementation. None of these three items counts as product red testing.
The early three candidate probes used stop8 binary; the final conclusion only refers to the execution after re-freezing and building.

### 39.3 Final Verification of Identities and Boundaries

| Project | Evidence for this round |
| --- | --- |
| Native freeze of source code | `d84cde6c02319c6b3c2ed2b5e67527f84e9fd42a`, `native-source-frame1.json` |
| Linux Frozen Source Code | `593d016550a7dd129f44c76d6e35f5a7c7704f45`, `frame1-linux-source.json`; The same project file byte as 4324 |
| Native Full Set | `native-frame-full1.json/.xml`, complete LastTest and reports; 599.937 seconds |
| Linux Full List | `frame1-linux-full-summary.json`, JUnit, complete LastTest; 690.668 seconds |
| Fixed workerd must have a gate of qualification | `frame1-linux-required-gate/summary.json` and the JUnit/LastTest for this run; 22/22 |
| New Original Frame Comparison | 4: Backend components; 4: Connection; 56: Framing; All values are strictly equal; Windows has two additional modes for actual execution |
| root qualification | `frame1-linux-qualified-root.json`, 3/3, Same Host/Worker Summary |
| WPT | Report 84, subtest 14060; 20 expected, 3 unexpected, 1 missing expected. Still the same two failed files as before. |

For the CTest serial of the same build, archive first and then proceed to the next round; all binary summaries are the same before and after execution. The Native official SDK interoperability still consists of two modes with 22 scenarios each.
Host exits normally, Python peer active=0/exit=0/signal=null/stderr is empty. Differences between the result document and the frozen source code are only allowed for documentation purposes. The final summary is presented in a single column.

This section closes the gap left by section 38, "Precise dimensions have not been run individually on workerd reference." Conflicts between two WPT/profile, differences in workerd among three categories,
pending-accept/selected-Worker-exit precise overlap, Windows stalled-TCP sub-scenario, cancellation barrier and fetch-cancel recurrence, real Host dual failure cleanup control,
Parameters / origin/Binding/CPU/heap/OOM/GC / Resource Matrix, MSVC ASan Vendor Annotation, State Machine Fuzz, sanitizer/soak, Native macOS, Hosted,
T15–T17 and two sets of newly built byte-by-byte reproductions have not yet been fully qualified. 0.3.0 remains unfinished, with no modifications to the WPT waiver, nor any three types of differences to expand compatibility claims.

## 40. 2026-10-16 Cross-thread Race Condition Fixes for Handoff and Stop <a id="section-40"></a>

After section 39, further review of the Windows static pool receive path revealed and fixed an actual data race.
`Impl::adopt_socket()` is called by the external receiving thread. The entry reads the ordinary bool `shutting_down_`. `on_signal()` will write the same byte in the shard reactor.
`worker_available()` uses a relaxed load, and cannot synchronize the previous reactor to external threads. The product source code has been corrected, but the version has not been released.

### 40.1 Valid TSan Red/Green Evidence

- The default TSan minimum environment probe compilation is successful, with an exit code of 66, reporting an `unexpected memory mapping`. After executing `setarch x86_64 -R` on the process level, the same probe reports an exit code of 0.
  Only the ASLR personality of the debugging child process was changed, without modifying the system's global configuration, nor was race suppression added.
- First product attempt `tsan-adopt-red1` attempted to assign TSan Hosts to ordinary Workers. The compatibility ID check failed as expected, which is considered a preparation error.
  Continue to conduct inspections and use the same configuration Worker; its compilation and summary should be saved separately.
- `tsan-adopt-red2` is a valid red test: The actual acceptor socket is born in the shard executor, and it waits for normal shutdown to reach the reactor before handing over the responsibility.
  TSan Precise Report `adopt_socket` One-byte read and `on_signal` A byte of write, **Exactly one data race, exit= 66**.
  The program has completed the normal cleanup and outputted PASS, indicating that the ordinary functional assertions are not sufficient to detect this issue.
- Simultaneously recreated the test with the candidate implementation `tsan-adopt-candidate1`, **exit=0, no TSan report**; only replaced the entry state reading.
  Old source code, candidate source code, reproduction tests, compilation and linking commands, actual binary summaries, and complete logs have been archived.
- The final permanent project is executed using the Debug/GCC TSan build, within the same source code. It passes both CTest **1/1** and zero reports. Details can be found in `tsan-integrated-result.json`. The results include JUnit tests and a complete LastTest report.
  The range is `host_single_worker_controllable_lifecycle`; the complete TSan gate, overlapping other lifecycles, and the sanitizer matrix are still pending execution.

### 40.2 Minimum Fixes and Permanent Regression Tests

The cross-thread entry for `adopt_socket` is rewritten to read the atomic `stop_requested_`. Only the atomic lifecycle state is used for quick judgment.
The handler that has been delivered to the reactor continues to review `shutting_down_` and Worker availability in the owner thread, maintaining the existing executor ownership checks.
The other ordinary members of the reactor have not been changed to cross-thread access, nor has the WebSocket's external behavior or compatibility profile been altered.

`tests/test_host_single_worker_lifecycle.cc` is added for the permanent scenario: establishing a real connection, confirming acceptance, requesting termination, using the actual worker’s available status as a progress barrier, and then
attempting to transfer control to another worker. It is necessary to reject the request, join the worker, and observe the actual peer’s EOF/reset events. This permanent test case includes additional TCP release assertions compared to the temporary TSan scenario.
The old implementations of the ordinary Release may pass these functional assertions, with race conditions being detected by the accompanying TSan. Functional tests are not used to pass as counter-examples for race conditions.

### 40.3 Final Verification and Unfinished Scope

Windows Strict Release total: **283 items: 271 passed, 0 failures, 12 skipped conditions**; Linux Strict Release/LTO total: **572 items: 567 passed, 2 existing WPT failures, 3 skipped conditions**. Fixed workerd gate **22/22, zero skips**, same Linux binary root verification **3/3, zero skips**.

| Project | Final Identity |
| --- | --- |
| Native Source Code | `932d8c2d61310f7bddd8126a150046fd2441c72f`; `native-source-adopt1.json` |
| Linux Source Code | `555a4b6a3141c3381dedad1f1f6aed929c3e1b3c`; `adopt1-linux-source.json`; Same project file byte 4324 |
| Native Full Set | `native-adopt-full1.json/.xml`, complete LastTest, official reports; 609.984 seconds |
| Linux Full List | `adopt1-linux-full-summary.json`, JUnit, complete LastTest; 663.821 seconds |
| Fixed workerd | `adopt1-linux-required-gate/summary.json` and the JUnit/LastTest for this run; 22/22 |
| Debug TSan | `tsan-integrated-result.json`, final Linux source code, 1/1, no report |
| root qualification | `adopt1-linux-qualified-root.json`, identical ordinary Linux Host/Worker, 3/3 |

Evidence directory: `build-v030-dev/evidence/adopt-stop-race-tdd-20261016/`. The entry files include `summary.json` and `index.json`, along with the final digest. The HEAD of the original repository remains at
`8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`. No commits, pushes, or releases have been performed; all changes made in historical working directories are retained.
The project changes are relatively minor compared to Section 39. There is only one copy of the Host source code, one lifecycle test, and one result document. The SDK lock remains unchanged from the original assertion.
Complete regression with no exclusions, in a build sequence, first archiving then the next CTest; consistent binary summaries before and after.
The realtime/monotonic sampling of Linux has recorded clock offset adjustments, and the duration in the table is taken as monotonic. The CSV and clocks summary files are retained, and no performance qualification conclusions will be made for this round.

WPT still consists of the original two failed files: 84 report, 14060 subtest, with 20 expected/3 unexpected/1 missing expected results.
The two modes have 22 scenarios for formal native SDK interoperability. The Host exits normally, and the Python peer is in an active state with parameters of 0/exit=0/signal=null/stderr empty.
Section 39: The exact original frame four backend comparison still passes. The race condition involving normal stop and handoff has been resolved. However, the exact overlap between pending accept and the abnormally exiting selected Worker still requires further testing.
Windows issues related to TCP congestion, cancellation of stage barriers/fetch-cancel operations, handling of real-host double failures, resource matrices, two types of WPT/profile boundaries, three categories of differences between existing workerd implementations, ASan integration into the
engineering framework, complete TSan/fuzz/soak/native macOS/Hosted implementations. The full implementation of T15–T17 and the byte-level reproduction processes still need further refinement before they can be accepted as a complete solution.

## 41. Pending acceptance on 2026-10-16, exit regression with the actual Worker <a id="section-41"></a>

This round continues the verification of gaps from section 40. All three controlled sequences have been passed on the existing product implementation, so there were no changes made to the Host/Worker product source code for this round of testing.
Added `tests/test_host_external_accept_exit.mjs`, which drives the dedicated mode of the existing C++ lifecycle testing program; real child processes, actual TCP, and HTTP are all involved in the assertions.

### 41.1 New Scenarios and Assertion Boundaries

| CTest suffix | Control Sequence | Results that must be observed |
| --- | --- | --- |
| `before-completion` | accept is suspended; exit the selected Worker; confirm PID recycling and server unavailability; re-establish TCP | Actually completed one acceptance process; Transfer rejected. EOF/reset from the counterparty, zero response bytes. |
| `before-adoption` | TCP has accepted and holds; exiting the selected Worker; confirming PID recycling and server unavailability; attempting to handoff again | Same refusal and closure; the connection cannot be reallocated to another Worker |
| `cancel-pending` | accept is suspended and not connected to the client; exit the selected Worker; close the acceptor | The actual completion is `operation_aborted`, exactly once; there are no rejections or healthy receipts counts. |

The first two also accept a new connection through the same listener, which is handed over to another healthy Worker. The actual HTTP response must return the complete text of the random challenge in this round, to prevent fixed success values from masking issues.
Enumerate and verify the executable paths of actual child processes, exiting only the selected Workers. Normal termination requires fixture code=0, signal=null, forced=false. Both Workers' PIDs are reclaimed, and the listening port rejects connections.
Entering EOF or encountering an error will trigger the fixture's destruction and cleanup, as well as the join to receive threads and two servers. It is not possible to forcefully kill the fixture as a success.

This is the true integration regression for `SingleWorkerServer` external-accept, using the same `async_accept(target->accept_executor())` syntax as in Windows static pools.
It is not equal to the complete `StaticPoolServer` selector failure matrix, nor does it control the exact point at which an exit occurs between a "passing entry pre-flight" and a situation where the handler has been delivered to the reactor but has not yet been executed.
The detailed submission process, complete selector pool, and platform resource matrix remain pending. macOS only has a synthetic control for registration verification, and this round was not tested on macOS.

### 41.2 Test Effectiveness Control

- Windows and Linux, **4/4** initial real-world scenarios: three new scenarios and existing lifecycle cases; initial snapshots and final snapshot identities are archived separately.
- Temporary host copy deliberately removed the entry availability gate; both denial paths are **exit=1**, with a clear report of `unavailable Worker accepted a peer`. There is no timeout fallback, no forced termination, and no Worker cleanup failure.
  This comparison was performed in the first round of Linux snapshots and does not include product changes from the final snapshot. Temporary source code, objects, binaries, commands, SHA, and two complete logs are retained.
- CTest registered validators added three mandatory items; **151/151** validators controlled the pass, including 19 new controls, rejecting missing items, aliases, disables, inverses, skips, and unbounded timeouts.
  The first round of independent execution failed due to an unconfigured ctest PATH. Subsequently, the sample synthesis timeout was fixed to 120 seconds, which violated the newly added upper limit of 60 seconds. It has been changed to a timeout that is determined by rendering each sample's own declaration.
  The original failure log is retained; the formal test does not relax the registration limit of 45 seconds or the audit upper limit of 60 seconds.
- The final source code has been tested with Debug GCC TSan for four actual use cases, with **4/4, zero reports**. The process continues in section 40, using `setarch -R`, without any suppression or system-wide configuration modifications.

### 41.3 Fully Exposed Fetch-Cancel Fuzzing Flaw

The first round of Windows full `native-exit-full1` resulted in 286 items: 273 passed, 1 failed, 12 skipped due to conditions; the failure was the actual observation from `worker_fetch_cancel_lifecycle`.
The same binary independent rerun has been passed, and the first-round failure cannot be dismissed based on this. Subsequently, a bounded 900 ms delay was constructed to cancel the test. The original fixture reported `errno=5/winsock=10060` and exited the monitoring, resulting in a false positive that the connection was not closed.
The comparison copy only adds `WSAETIMEDOUT` idle retries. Exit=0 on the same Worker indicates original failure, independent rerun, and both comparison results are retained.

The `tests/test_fetch_cancel.cc` file is subject to the following review: Windows' `WSAETIMEDOUT` and EAGAIN/EWOULDBLOCK errors on both platforms will continue to be awaited. EINTR will be retried, and other reception errors will remain for further diagnosis.
Leaving the idle time after the deadline does not count as an EOF, nor does it count as a successful cancellation. It is still necessary to observe the actual closure of the counterpart, whether repeated cancellations are acceptable, and whether the same request ID can be reused successfully.
The original immediate cancellation scenario is retained, and another round of execution is performed with a cancellation scenario that uses the actual idle-timeout count as a barrier. A fixed sleep is not used to replace the permanent progress barrier.
In both scenarios, real Workers are launched, and the original stage deadlines are retained. The total budget for CTest is changed from 15 seconds to 30 seconds for both scenarios.
The failure has been changed from 'exit' to 'exception return', and the cleanup is performed using Worker RAII and listening thread destructors. When the temporary copy of the permanent test is restored with the old timeout behavior, the pass is immediately canceled. In idle scenarios, 'exit=1' is explicitly set, and both real Worker PIDs are reclaimed.
Revised Windows Special Project 5/5, Final complete regression test `native-exit-full2` And `exit3-linux-full`. The intermediate Linux full scan/gate results are also retained, without pretending to be the final snapshot results.
This closes the false positives caused by the idle receive timeout, but it does not mean that the entire fetch cancellation phase matrix has been completed.

### 41.4 Complete Verification and Runner Issues

Windows Strict Release full set **286 items: 274 passed, 0 failures, 12 skipped conditions**; Linux Strict Release/LTO full set **575 items: 570 passed, 2 existing WPT failures, 3 skipped conditions**. Fixed workerd official gate **22/22, zero skips**; same Linux binary root verification **3/3, zero skips**; Debug GCC TSan lifecycle special **4/4, zero reports, zero skips**.

| Project | Evidence and Identity |
| --- | --- |
| Native freeze of source code | `0be38031eadcfc5e5cd22126e776f5bf618dcad9`, `native-source-exit3.json` |
| Linux Frozen Source Code | `45014aad88cd6d7fa24799d5b8e14104c4d5dd84`, `exit3-linux-source.json`; 4325 project files have the same byte count |
| Windows Full Set | `native-exit-full2.json/.xml`, complete LastTest and official SDK/frame reports; 631.141 seconds |
| Linux Full List | `exit3-linux-full-summary.json`, JUnit, Complete LastTest; 676.780 seconds |
| Formal Workerd Gatekeeping | `exit3-final-linux-required-gate/summary.json`, 22/22; Run independently after the build is completed and full archiving has been performed |
| Same as binary root verification | `exit3-linux-qualified-root.json`, 3/3 |
| TSan | `tsan-final-result.json`, the same final Linux source code, 4/4 |

The reused first-round gate runner only checks the exit=0 of the most recent step. After successful configuration, it may be mistakenly believed that the entire build is complete.
This round's first comparison was indeed earlier than the end of the last test target link, so it is retained as a preliminary record. For formal qualification, the fully archived `exit3-final-linux-required-gate` will be used.
The formal gate entry requires that both configure/build steps be successful, the full state should be finished, LastTest should be complete, and the binary SHA should be consistent.
The full regression process did not exclude any testing steps. The full Linux build process began after the complete construction was completed. The summaries of Host/Worker processes before and after the build are identical. All original logs and the final digest are located at `build-v030-dev/evidence/pending-accept-exit-tdd-20261016/`.

WPT still has the 84 report, 14060 subtests, with 20 expected/3 unexpected/1 missing expected results. Two failed files, 12/3 skipped conditions, consistent with section 40.
The original fixed workerd version, SDK lock, WPT expectation, and product implementation have not been changed for this round of modifications; the known semantic differences between the three types of workerd remain unresolved.
0.3.0 Overall unfinished. Qualification for T02/T04/T06–T08/T10–T17 is not based on this. The next batch still requires delivery of handler overlap, Windows stalled-TCP, cancellation of stage barriers/fetch-cancel, real Host double failure cleanup, and resource matrix.
ASan engineering integration, complete TSan/fuzz/soak, native macOS/Hosted, and complete byte-by-byte reproduction with dual builds are still not completed. The original workspace has not been submitted, pushed, or released.


## 42. 2026-10-16 Worker: Recycling of Idle HTTP Connections After Termination <a id="section-42"></a>

Continuing from section 41, we implemented real queued-adoption overlap. We identified and fixed a product defect: the connection handoff was queued, and the reactor only performed the handoff after the Worker actually exited.
The transfer handler may execute before the exit notification, and the local availability remains true. Therefore, a Session is created and awaits HTTP communication.
The subsequent exit notification only processes queue_ and requests_; sessions that have not received a complete request are not included, and thus TCP and reactor continue to exist.
This also affects the keep-alive connections that have been accepted and the responses that have been completed; the product changes for this round are limited to `src/host/single_worker_server.cc`.

### 42.1 Real Red/Green Evidence

- Initial freeze of Native `6ecb1a7d83350debcf792b4020e390e207db6f23`, Linux `77959925b454436ef98f93f8fbc0c9752a940fc3`.
  The original implementation of `host_external_accept_exit_queued-before-exit` has a true failure on both platforms, with no skipping. Complete JUnit, LastTest, and binary SHA records are retained.
- The executor has been paused publicly; the actual TCP reception is completed, and adopt_socket returns true. The actual handover of the handler and the successor marker have been delivered.
  The driver then exits and confirms that the selected Worker PID disappears, after which the reactor is released. The marker must record workerAvailable=true, and subsequently, the owner must be made unavailable.
  This assertion excludes the case where test cases are secretly turned into "entry denial"; the old implementation still waits for the TCP final state when the owner is unavailable, and a failure is declared after 8 seconds.
- Both fixtures in the two red tests naturally exited with code=1/signal=null/forced=false, and the driver retained the precedence and reclaimed the Worker. No conclusions were drawn based on the forced termination of the fixture.
- Linux temporary selection only replaces the product object, maintaining the C++ test objects, Node drivers, and Workers from the red testing. The actual exit status is 0, TCP EOF/zero extra bytes, and the healthy Worker responds dynamically to challenges.
  `same-test-candidate.json` and `tdd-inputs.json` bind the original source code, driver SHA, same test object SHA, commands, and actual records.
  The final product source code is identical to the candidate byte; the permanent regression subsequently extended two adjacent states, and the extended fixture cannot be mistaken for the original byte-based red-green input.

### 42.2 Product Fixes and Permanent Regressions

Session records whether HTTP requests are being parsed within the owner reactor. When workers exit, individual snapshots are taken and these sessions are closed, to prevent iterator failures when calling close() while sessions_ is being modified.
The local response being written does not get truncated by this scan; when write completion re-enters read_request, the dead/stopping/closed gate closes the connection, and the read is not re-issued.
Closed sessions will not process requests even if a delayed successful read completion is received. The queuing/executing Worker exits, and 503 uses Connection: close for this purpose.
The remaining timeout and request failure branches remain unchanged. The availability status of Workers is still updated by the owner reactor after consuming events.

| New CTest suffix | Progress barriers and assertions |
| --- | --- |
| `queued-before-exit` | Reactor paused; real handover queueing; Worker PID disappeared; handover after recovery occurred before the owner exited observation execution; subsequently, the actual TCP closed. |
| `idle-after-adoption` | The transfer handler has been executed, and the Worker is still available; then the Worker exits; connections that did not send HTTP are closed. |
| `keepalive-after-adoption` | The selected Worker returns a complete HTTP/1.1 200 response for the dynamic challenge; the socket is actually returned to the keep-alive agent; it closes after exiting without additional response bytes. |

All three items are handed over to the healthy Worker through a separate connection via the same listener, to verify the complete response to the new challenge. Normal fixture code=0/signal=null/forced=false, two Worker PIDs are reclaimed, and the ports are released.
HTTP/1.1 keep-alive It is not required that the response explicitly includes a Connection: keep-alive. A complete response, without a close token, and with the agent returning the barrier is the basis for assertion.
The original three exit scenarios are retained; six exit scenarios, the original lifecycle, and the following 503 scenarios are all executed in TSan. The registration audit control has been increased to 160/160. Essential items are not allowed to be disabled, reversed, or skipped.

First round of Native full scan: 277/0/12; Linux full scan: 572/3/3. The original archives for the newly added failed `host_admission_worker_death_returns_503` are retained.
The old test cases send the next request to the idle keep-alive connection after the Worker exits and the listener closes, depending on the connection continuing to wait. This conflicts with the semantics of this round of recycling.
The 503 mapping in §10.3 design addresses the scenario where a Worker/IPC failure occurs before the response header for an existing request. This scenario cannot be verified by allowing idle connections to persist indefinitely.
Correction `tests/test_host_admission.cc`: One execution slot, one queue slot; send three real requests that never complete on their own, observe a complete 429 confirmation where both slots are filled, without assuming a connection processing order, and without using sleep to replace progress.
The only actual Worker that exits from the same path thereafter; both accepted requests must return a complete 503/precise body/Connection: close, followed by a TCP EOF, no excess response bytes, and the PID is reclaimed.
Raising exceptions allows the server to be properly destructed, preventing assertion failures from bypassing cleanup. Retain the original CTest name and strict 503 assertions, covering requests_ and queue_ respectively.
`admission2-contract-controls.json` uses the same test objects and Worker: The normal product has exit=0; specifically, the product numbers in queue_ and requests_ are changed from 503 to 502. Both copies have complete 503 assertions with exit=1.
This does not change failure to allowing EOF or skipping; idle connections being closed and 503 status indicating already accepted requests are checked through real use cases. The final product source code remains byte-for-byte the same as the initial candidate.
This replacement covers the scenario where "existing executing/queued requests encounter exit before the response header," which is not the same as a separate qualification where the new request returns a 503 status when there are no READY Workers at the front end. The latter remains unfinished with the complete pool selector matrix.
The source code inspection also shows that the Windows shared acceptor will choose the old shard when there is no available shard, and will close the TCP connection after adopting a refusal; the POSIX shard will close its listener when exiting.
It is necessary to clarify the capacity limit of 503 for the local mode separately and implement real frontend scenarios. The 503 status that has been accepted in this round should not be used to cover other cases. The original admission source code should be saved as `admission-before-idle-reclamation.cc`, and its SHA should be consistent with the first-round freeze list.
The native build `native-build-queue-final2.json` strictly checked the intptr_t descriptor from the poll, which was passed to the C4244 integer encapsulation. Failed builds were retained.
Explicitly restore the known descriptor types created by int encapsulation, retain /W4 /WX, final build, and complete regression pass; the warm-up also requires a real EOF to release the execution slot.

### 42.3 Final Regression and Boundaries

Windows Strict Release total: **289 items: 277 passed, 0 failures, 12 skipped conditions**; Linux Strict Release/LTO total: **578 items: 573 passed, 2 existing WPT failures, 3 skipped conditions**. Fixed workerd official gate **22/22, zero skips**; same Linux binary root verification **3/3, zero skips**; Debug GCC TSan lifecycle/503 special case **8/8, zero reports, zero skips**.

| Project | Final Identity and Evidence |
| --- | --- |
| Native Source Code | `633af6a18635099f6c55f9a09c325c48429f6c14`, `native-source-queue-final4.json` |
| Linux Source Code | `6acb1e8b8d7c76638196b81cd2c0c8f29cfdf97a`, `queue-final4-linux-source.json`; 4325 project files have the same byte count |
| Windows Full Set | `native-queue-full4.json/.xml`, complete LastTest and SDK/frame reports; 631.609 seconds |
| Linux Full List | `queue-final4-linux-full-summary.json`, JUnit, complete LastTest; 688.078 seconds |
| Fixed workerd | `queue-final4-required-gate/summary.json`, archived in full and independent 22/22 |
| root qualification | `queue-final4-linux-qualified-root.json`, same as the regular Linux binary 3/3 |
| TSan | `tsan-final4-result.json`, identical to the final Linux source code, 8/8 |

Evidence directory: `build-v030-dev/evidence/queued-adoption-exit-tdd-20261016/`; complete regression with zero exclusions, each round of LastTest/JUnit is archived completely before starting the next round; consistent SHA for Host/Worker before and after.
Before the official gate is activated, bind the complete archive and build results with a SHA. The first-round failures, temporary objects and binaries, input source code, and final raw reports are all sealed together.
WPT still has the 84 report, 14060 subtests, 20 expected/3 unexpected/1 missing expected; two failed files and 12/3 condition skips are consistent with section 41.
The workspace HEAD remains `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, not committed, pushed, or published; no modifications were made to the Worker, SDK/reference locks, or WPT expectations in this round.

The measured sequence is for the scenario where "transfer of delivery occurs, and physical process exit happens before handler execution"; the shorter window between entry check and actual post still requires independent verification. The complete StaticPoolServer selector failure matrix has not been verified based on this scenario.
The above description of retention writing comes from implementing boundaries and complete regression tests, and does not equate to a separate controlled coverage that overlaps with the local response write completion.
Windows stalled-TCP, remaining cancellation phases, real Host dual failure cleanup, resource matrix, two WPT/profile items, three types of existing workerd differences, and ASan/complete TSan/fuzz/soak/native macOS/Hosted/T15–T17/new dual build byte-by-byte reproduction is still not completed.
0.3.0 The overall project remains unfinished. No complete closure will be carried out for T02/T04/T06–T08/T10–T17.


## 43. Transferring entry checks to the actual post for race conditions on 2026-10-16 <a id="section-43"></a>

Continuing from the preflight → post window left in section 42, we need to realistically reproduce and fix another instance of connection latency.
The original adopt_socket function, after the accept thread passes the atomic entry check, still requires the allocation of transport, construction of handlers, and finally the post operation.
The owner may handle the exit/stop/drain process in the middle, release the external_guard_, and end the run(). Subsequently, the post function is still called, but the socket remains in a queue without consumers, until the Impl is destroyed and it is released.
This round of product changes is still limited to `src/host/single_worker_server.cc`. The previous HTTP 503/idle connection corrections are retained.

### 43.1 Original Failures and Comparisons with Input Fixes

- The initial Native red testing and the Native red testing after strict compilation fixtures are retained; for official pairing, Native `native-submit-red2` is used in conjunction with Linux `submit-red2-linux-focused`.
  Both platforms' original products failed on 1/1 occasions, with zero skips. Complete JUnit/LastTest reports, build records, and binary summaries are retained.
- The allocation barrier exists only in the test program: The thread-local switch is closely tied to the actual adopt_socket call, pausing the first transport allocation, while other threads continue with their allocations.
  Use this platform to actually calibrate the allocation size for make_shared<tcp::socket>; Native records 176 bytes, Linux 104 bytes, both requiring the actual value to be equal to the calibrated value.
  Do not inject branches or callbacks into the product, and do not impersonate race conditions with connections that have been rejected at the entry point. The real Worker must remain available during pauses.
- The real TCP has been accepted. The Worker exit notification has been processed, and selected.wait() has joined the reactor and Worker executor. Only after the actual io_context.stopped() returns true is the producer resumed.
  The original product record indicates accepted=true, but a TCP final state has not been reached after 8 seconds; fixture code=1/signal=null/forced=false, and both real Worker PIDs have been reclaimed.
- Linux candidate only replaces product objects, continuing to use the C++ test objects, Node drivers, and Workers from the red testing phase. All three new scenarios actually exit with 0.
  The final product source code is identical to the candidate byte. `same-test-candidate.json`, `tdd-inputs.json`, and the archived source code, drivers, and actual `.o` binding input summaries.
  Linux red-test orchestration utilizes green post-conditions, as the expected failed list triggers the wrapper's AssertionError. The defect conclusion is based on the actual CTest exit status of 8, JUnit results, and a specified TCP deadline. The wrapper exceptions are not considered as product failures.

### 43.2 Product Fixes and Permanent Regressions

Added a monotonic adoption_open_ gate protected by mutex. The allocation of transport and std::function is prepared outside the lock; subsequently, the gate and lifecycle are checked inside the lock, and the actual post is completed.
Close the path by first closing the door within the same lock, and then release external_guard_; covering Worker kExit, on_signal, start_drain_on_io, complete_drain.
This closes the synchronization rejection when it is completed first and releases the transport. The io_context has the work pending execution upon completion, and the owner cannot end the run() first, leaving it in the queue.
Retain the review of the handler's owner status; do not execute the user processing function within the lock, and do not pass the reactor-local shutting_down_ to the accept thread for reading.

| New CTest suffix | Real Lifecycle Boundaries |
| --- | --- |
| `pre-post-worker-exit` | Drive the actual child process to exit after verifying the path/PID; restore the handoff after the owner joins. |
| `pre-post-stop` | Do not inject process signals; resume the handoff after normal exit from Worker/owner via request_stop |
| `pre-post-drain` | Do not inject process signals; idle begin_drain(0) completes and resumes handoff after joining. |

All three requirements require accepted=false, a true TCP EOF/reset, and zero response bytes. The next connection from the same listener is handed over to another healthy Worker, and the new dynamic HTTP challenge is verified.
The final fixture code is set to 0/signal=null/forced=false. Both Worker PIDs are reclaimed, and ports are released. The previous six exit scenarios are retained. Among them, cancel-pending does not have any actual connections or healthy HTTP requests. A separate check is performed for cancellation completion, with one and zero handoffs.
The post is first verified by the queued-before-exit barrier. The verification of the barrier at the first allocated point of this round is also closed. It is not claimed that each allocated/instruction position within the post is paused separately.
The overlap between drain with active requests, the complete StaticPoolServer selector, and the local response write completion still requires an independent matrix.

The testing code replaces the matching new/delete operations. When no barriers are used, malloc/free functions are utilized instead. GCC -O3 causes internal calls to become invalid after the Boost exception handling mechanism is invoked, resulting in false reports of mismatched-new-delete errors. The first round of strict build failures was not resolved; however, this
issue has been addressed by using non-inline releases for boundary management. No diagnostic warnings were turned off, nor the -Werror flag was relaxed. The notthrow new/delete idiom remains intact.
The registration audit control is now 169/169; required items include Linux 64, Windows 55, and Darwin 56. It is not allowed to disable, reverse, or skip these items.
It was discovered that the calling environment did not provide ctest, so the tool PATH was fixed. The first PATH fix audit still retained the old nested count of 61, which was corrected to 64 after adding three new items. Both failed logs and the final pass were retained.

### 43.3 Final Regression and Delivery Boundaries

Windows Strict Release Full **292 items: 279 passed, 1 fetch-cancel failed, 12 conditions skipped**; Linux Strict Release/LTO Full **581 items: 576 passed, 2 existing WPTs failed, 3 conditions skipped**. Fixed workerd official gate **22/22, zero skips**; same Linux binary root verification **3/3, zero skips**; Debug GCC TSan lifecycle/503 special case **11/11, zero reports, zero skips**.

| Project | Identity and Evidence |
| --- | --- |
| Native Source Code | `2d07028670b6e5d31140540407a8115136f3f749`, `native-source-submit-final1.json` |
| Linux Source Code | `7ceb08fcee2dc12940077a3d796cde4d9607919f`, `submit-final1-linux-source.json`; 4325 project files have the same byte count |
| Windows Full Set | `native-submit-full1.json/.xml`, complete LastTest, SDK/frame reports; 630.281 seconds |
| Linux Full List | `submit-final1-linux-full-summary.json`, complete JUnit/LastTest; 683.378 seconds |
| Formal Workerd | `submit-final1-required-gate/summary.json`, fully archived and independent 22/22 |
| root qualification | `submit-final1-linux-qualified-root.json`, same as the regular Linux binary 3/3 |
| TSan | `tsan-final-result.json`, same as the final Linux source code 11/11, zero report |

General build targeted verification for Windows 11/11, Linux 12/12; TSan includes nine eviction scenarios, original lifecycle, and strict 503 scenarios.
Complete regression with no exclusions. Testing is performed only after both configure/build steps are completed. The same build is serialized for CTest, and for each complete LastTest/JUnit, it is archived before proceeding to the next round.
The fixed workerd accurately frames each of the four backend nodes using 4 to connect to /56. In the Native SDK mode, 22 is used in both scenarios. The SHA values of Host/Worker are consistent across all processes. No remnants remain from this build process, and the historical processes of other builds were not terminated arbitrarily.
WPT still has 84 reports/14060 sub-tests, 20 expected/3 unexpected/1 missing expected results; Linux has two failed files and 12/3 conditions skipped unchanged, with no modifications to SDK/reference locks or WPT expectations.
All Windows worker_fetch_cancel_lifecycle failures cannot be ignored. In the first scenario, 3, no outbound connections were observed to be disconnected within seconds. receive_errno equals 0, and receive_finished equals 0. The receiving monitoring threads are still working.
In 32 independent verifications where the test program, Worker, and fixture remained unchanged, 31 verifications were passed, and 1 failed. The number of passes cannot cover all the failures.
Only after the independent Worker object joins the trace, the 4 Replication again. The log shows that after the abort arrangement, the process still enters CLIENT_APPEND_HANDSHAKE_HEADER (completed= 1), the handshake code then resets the same connection timeout.
This is consistent with the timing of closing the timeout, which is covered by separate candidates and controlled regression testing. The diagnostic Worker is not a formal qualification binary, and the formal source code and build have not been altered.
The native-verify.py program exits with a failure after archiving the complete results. Subsequent process audits are conducted separately and are deemed passed. No falsification of the entire orchestration with complete=true was performed.
Evidence `build-v030-dev/evidence/adoption-submit-gate-tdd-20261016/`, which includes the original failures, candidate objects, final reports, source code/binarity verification, and the original six documents; first verify with git diff --check, then seal with SHA-256.
Section 42, file 301, with a size of 127004806 bytes, remains unchanged. The workspace HEAD is still `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, and it has not been committed, pushed, or published.

Fixed the defects that remained from the preflight→post phase. The 503 status for new requests when there is no READY Worker, and the overlap of active drain/local write completion has not yet been fully accepted.
The actual failure of Windows fetch-cancel remains open; stalled-TCP, other cancellation phases, real Host double failure cleanup, resource matrix, WPT/profile, and differences in the three types of workerd, as well as ASan/complete TSan/fuzz/soak/native macOS/Hosted/T15–T17/new double build byte-by-byte reproduction are still not completed.
0.3.0 The overall project remains unfinished. No complete closure will be carried out for T02/T04/T06–T08/T10–T17.


## 44. Cancel overlapping handshake with pending execution on 2026-10-16 <a id="section-44"></a>

Continuing from section 43, it was determined that the worker_fetch_cancel_lifecycle process failed during the Windows reinstallation process. We have identified another path where the actual outbound connection is still active and need to fix it.
In section 43, all items of 292 were retained in the original archive. In section 279/1/12, the results of 32 were also preserved. However, there were several failures in section 1. Independent tracking was performed for sections 4 and 1, and their failures were also retained in the original archive. No further modifications were made to these files by overwriting them.
The receiving thread remains active, with errno=0, which is different from the fixture issue where the monitor of receive-timeout prematurely exits, as described in section 41.

### 44.1 Root Cause Analysis and Product Fixes

The native HttpClient.abort method previously set h->completed to true, reported ABORTED, and then arranged for asynchronous shutdown using the connected protocol-state timeout.
The connection/HTTP handshake for the same event batch that has not been completed may continue, and the same timeout will be reset to wait for a server response, overriding the scheduled shutdown.
The original trace does indeed record CLIENT_APPEND_HANDSHAKE_HEADER(completed=1) after abort/armed, and there is still no TCP final state after 3 seconds.

Added `0041-capsid-http-abort-pending-handshake.patch`: Arrange the last scheduled close using a separate application timer.
The existing TIMER path executes teardown after seeing completed=true and returns -1. Retain a callback for ABORTED completion, and close the process after returning from the current lws callback.
Do not synchronize the destruction of WSI within the ondata callback stack. No modifications were made to Worker/runtime, bootstrap/fetch JS, the original test_fetch_cancel.cc, or the public ABI.

After adding the patch, both platform configurations have been rejected by the original build identity verification. The verification has not been deleted or bypassed.
First, perform an audit on the clean vendor, fixed tag/gitlink, and the order of all 42 patches on both platforms using the overlay key/manifest. Then, only update the two overlay summaries in docs/txiki-upgrade-baseline.json.
The vendors, QuickJS, and deviation list remain unchanged. The new key is `9eedf5ab503c8f257339ae5de6bf41f7fc33ea297d46664973f05263cd5a0d62`, and the manifest is `b735f30dd5f5da3754aecd2f306b01156fd4d0e423220cd95ead2adfabd98375`.
The C source code after Windows preparation is CRLF, while on Linux it is LF. The original summaries are recorded separately, but after standardization, they are the same as the product candidate `4dffde2fa271ea0f7c8ac2830c64bb82239c53605d55b8cab7d7a176acd99aea`.

### 44.2 Permanent Regression and Object Comparison

Added `tjs_fetch_abort_pending_handshake`, which directly links to the real txiki HttpClient used in this build; uses embedded interfaces to set up the local testing environment, without injecting product testing hooks.
In real TCP accept, the boundary is canceled twice with zero HTTP bytes; uv_walk/uv_fileno and getpeername confirm the actual socket of the current client.
Only when WSAPoll/poll confirms that it is actually writable, will the libwebsockets interface with the lws_service_fd function proceed to advance the pending network events before closing the timer scheduling. It is required that lws consumes this event.
Do not set a direct handshake timeout, do not replace the callback, nor forge the close flag. Subsequently, check the unique ABORTED completion, active receive monitoring, actual EOF/reset, and complete the assertion before the TJS runtime is terminated.
This embedded layer test itself does not create a Worker; the original Worker end-to-end test continues to verify signal propagation, repeated cancellation, and the reuse of request id=61.

- Final C++ 11 red test, `native-abort-red3`, `abort-red3-linux` each selected, 1 item, all actually CTest exit= 8, failed 1, zero skipped; actual writeable events 2 times, canceled before 0 bytes, received by the other party after 169 bytes handshake, 3 seconds still not closed, errno= 0.
- Each component retains its original test files `.obj`/`.o`, and only compiles and prelinks to the HTTP product objects. The original test objects and other libraries remain unchanged. The Native/Linux candidates were tested 5 times each, and all passed.
  `same-test-native.json` and `same-test-linux.json` bind test source code, real objects, products, and link commands; the SHA `88789a4ce7c27fb7402093ae454dded0247ddfaf90479a4ffeb2d597900c43ea` of the test source code has not changed.
- The independent ordinary candidate passed the 64 tests and 32 trace tests before the fix, and this is recorded as a diagnostic. For formal qualification, the frozen build, complete regression tests, controlled unit tests, and actual network observations are still taken into consideration.
- The final Native/Linux tests were continuously executed 32 times on the unmodified original Worker. Each execution was canceled after the immediate receive-timeout, and the ID reuse was successful. All tests were retained without failures, retries, or sample filtering.
- Cross-platform registration audit added necessary items and removed/aliased/disabled/inverted/skipped without timeout control, 178/178 passed; necessary items: Linux 65, Windows 56, Darwin 57. Darwin only verifies registration rules here; native macOS operation is still pending.

In the survey, several embedded probes failed to distinguish between the original product and the candidate, and were not listed as regression proofs. The first probe lacked cookie environment configuration, resulting in assertions, premature egress callbacks, and abortions. There are no records of wsi hits, prepare sequence misses, etc., but these records are retained.
Early overlap-* probes manually provided writable flags, without confirming OS readiness first, and were not listed as formal proofs. Permanent use cases and formal object candidates both use actual WSAPoll/poll for confirmation. For details, see diagnostic-limitations.json.
The first round of Linux strict compilation for the permanent fixture identified a C++11 aggregate initialization issue. This issue has been resolved by changing to an explicit constructor, without relaxing the standards or issuing warnings. The registered audit's true show-only feature requires the ability to parse a single executable file path. The scratch files that never execute have been corrected, and the verifier has not been relaxed.
The first Linux object orchestration failed before the Native input was archived, and the product was not compiled or run. It was changed to wait for the input to be archived and then proceed in sequence, and the orchestration exception was not considered a product redtest.

### 44.3 Final Results and Boundaries

Windows Strict Release total: **293 items: 281 passed, 0 failed, 12 skipped conditions**; Linux Strict Release/LTO total: **582 items: 577 passed, 2 existing WPT failures, 3 skipped conditions**. Fixed workerd official gate **22/22, zero skips**; same Linux binary root verification **3/3, zero skips**; Debug GCC TSan lifecycle/503/cancelled special project **12/12, zero reports, zero skips**.

| Evidence | Results or Identity |
| --- | --- |
| Native freeze of source code | `4833352db74883a3fd9b559fcf0cec940f074a26` |
| Linux Frozen Source Code | `5e11438618d71d1c69a1d9b94d102b4f9462eb8a`; 4327 project files are identical to Native byte files |
| Windows Complete Regression | `native-abort-full1.json/.xml`, complete LastTest and SDK/frame reports, zero exclusions |
| Linux Complete Regression | `abort-final2-linux-full-summary.json`, complete JUnit/LastTest, zero exclusions |
| Original Worker Cancel Test | Two platforms, 32/32 times each; two scenarios per time, all exited normally = 0 |
| Formal Workerd | `abort-final2-required-gate/summary.json`, fully archived and independent 22/22 |
| root/TSan | Same as the final standard Linux binary root 3/3; same as the final source code Debug GCC TSan 12/12 |

Directional regression for Windows 12/12, Linux 13/13; new controlled cases are actually executed in both normal full load and TSan scenarios. TSan has no suppression, and setarch x86_64 -R only applies to commands.
After checking compile_commands, it was found that the standard TSan configuration includes instrumentation, while the httpclient.c file from the vendor does not have any instrumentation. Therefore, I have explicitly enabled TSan using the same final C source code. I also linked the system to an existing test object that already has instrumentation. As a result, the network testing using 1/1 was successful, and no reports of issues were reported.
The archived objects indeed reference __tsan_read*/__tsan_write*; the tsan-http-product-result.json file preserves the complete commands and symbols. Other vendors/dependencies remain in their original configuration. The full vendor instrumentation qualification remains open; it cannot be presumed to be completed by the 12 special projects or this one.
The fixed workerd precise frame still consists of 4 connections per backend, totaling 56 frames. For the Native SDK, there are 22 scenarios in each of the two modes; the SDK/reference lock has not been changed.
WPT maintains 84 reports/14060 sub-tests, with 20 expected/3 unexpected/1 missing expected results. Two existing files have failed, and 12/3 conditions have been skipped without change.
Tests are conducted after the configure/build phase is completely completed. For each build, the CTest sequence is performed in serial, and the complete LastTest/JUnit is archived before proceeding to subsequent runs. The WSL clock offset records are used only for environment diagnostics and are not used for performance qualification.
The evidence `build-v030-dev/evidence/fetch-abort-handshake-tdd-20261016/` contains failed attempts, object candidates, frozen source code, complete results, six final result documents, and a SHA-256 signature list.
Section 43, File 251/40741067 bytes of evidence and earlier sealed items remain unchanged. HEAD is still `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, and has not been submitted, pushed, or published in the original working area.

This round of closure involves the cancellation of the lost path for the pending-handshake that has been reproduced. Other cancellation phases, complete StaticPoolServer selector failure matrix, no new requests from the READY Worker front-end 503, Activity drain/local writing completion overlap, Windows stalled-TCP, real Host dual failure, resource matrix, WPT/profile There are three types of differences among existing workerd, and further acceptance of broader toolchains/platforms is still required.
0.3.0 Overall unfinished, T02/T04/T06–T08/T10–T17 will not be closed as a whole.


## 45. Worker exit during the complete pool waiting to accept connections on 2026-10-16 <a id="section-45"></a>

Continuing from section 44, we need to fix a bug in the StaticPoolServer selector mechanism. This bug occurs in Windows environments, where the pool attempts to select a shard before accepting any new TCP
connections. If the worker exits before a connection is established, the current implementation will close the first client connection, even if there are still healthy workers available in the pool.
This round only involves changes to the pool implementation, the real process fixtures, and their registration auditing. There are six project files in total. The SingleWorkerServer, Runtime, JS API, public ABI, vendor, WPT, and SDK/reference locks remain unchanged.

### 45.1 Fixes and Controlled Comparisons

`dispatch_pool_peer` re-evaluates the original target after accepting completion; if it is found to be unavailable, a healthy shard is selected, the old native socket is released, and the reactor is assigned to the alternative shard. Only then is the original `adopt_socket` called.
HTTP/WebSocket requests that have not been read before the transfer are connected; this adjustment occurs at the connection assignment stage, and retries after the request is sent still need to be individually qualified according to the original policy.
The term "ordinary move" retains the old executor and cannot handle the reactor transfer here. In the case of a release failure, the ownership is retained by the old socket. In the case of an assign failure, the released handle is explicitly closed. The stop/exit mechanism, along with the serial gate of "post", follows the implementation described in Section 43.

This compilation unit is arranged as follows: [Existing minimum requirements for Windows 10](../../guides/build-on-windows.md#prerequisites) Statement: API target, no expansion of platform support.
For details on the transfer of ownership of Boost to release and the statements not supported before Windows 8.1, see [Fixed 1.88.0 source code](https://raw.githubusercontent.com/boostorg/asio/boost-1.88.0/include/boost/asio/basic_socket.hpp) The actual native IOCP behavior is verified through the following network red-green comparison.

| Input | Real results |
| --- | --- |
| `native-pool-red5`, original pool implementation | Two failures and one healthy control passed; the first HTTP/first WebSocket received ECONNRESET, the fixture exited normally and reclaimed the actual Worker. |
| `candidate-3-result.json`, only replace the pool product objects | Three scenarios, three rounds each, total 9/9 passed; the red test for C++ objects, SingleWorkerServer objects, Runtime/Host core, Workers, C++ source code, and Node driver summaries remain unchanged. |
| `move-only-1-result.json`, deliberately omitted re-binding | The first HTTP/first WebSocket failed again, but the health check passed; it proves that simply re-selecting the target is not enough to fix the issue. |
| `pool-red5-linux-focused`, original Linux implementation | Same scenario 3/3 passed; Linux originally uses SO_REUSEPORT, so this Windows-specific bug is not described for Linux red testing. |

Formal red-test of Native source code `72363b0b617335b130b33fbdc0cc232ca43fba53`; original pool source code SHA `19f0906b99c1b705357b484bbdf2435346f075cac881c787046dc7d7656baf95`.
Object candidate and final product source code SHA `992aef283c9b9ab9c94d3f2ef55fc5a9e2998a1d9af129a46d9d8c2e95ca559f`; `product-diff.patch` lists the differences with section 44 of the product.
To test the original product, red3/red4/red5 isolated snapshots were used for archiving old source code for pool products. productOverride records the source and summary; the original workspace always retains the candidate, and the final snapshot does not have this coverage.

### 45.2 Practical Assertions for Permanent Regression Testing and Previous Corrections

Add `test_host_static_pool_faults.cc/.mjs`, start the full pool and two actual Workers directly; confirm the target identity through the parent PID and executable path.
Windows identifies the first shard to start up based on a strictly increasing OS creation time. During initialization, there are no HTTP probes or other TCP clients involved.
The driver terminates the specified Worker, and only initiates the first connection after the active_workers count decreases from 2 to 1, and the PID is reclaimed.
The three scenarios are first-worker-exit, last-worker-exit, and first-worker-exit-websocket. The third option allows the actually re-bound connection to directly handle the WebSocket.

Each pass includes two dynamic HTTP challenges, the same surviving Worker instance, a complete response, and a TCP final state; four concurrent WebSocket sessions test the 0, 1, 125, 126, 65535, 65536 bytes of the text/binary response.
Each client generates 12 frames per 262646 payload bytes. The final code value is 1000, and the reason indicates that the process has completed. The fixture operates normally, following the stop/wait mechanism. Both original Worker PID’s are no longer active. New connections to the public port are effectively rejected. The fixture does not terminate in a forced manner.
Tests did not introduce product timing hooks, forged responses, or manually override worker_available. Cross-platform registration audit **197/197**, required items: Linux 68, Windows 59, Darwin 60; Darwin only verifies registration, native operation still needs improvement.

All previous failures are retained, and they are categorized based on the reasons for failure:

- The WebSocket fixture of red1 uses the default Blob, resulting in a binary echo type error. After explicitly setting arraybuffer, the health comparison of red2 is passed.
- Linux red2 does not forward fixed Node ws paths, resulting in failures at the loadNodeWS entry point, with processes not yet started. It has been changed to use the explicit --node-package parameter consistent with existing SDK tests, without relaxing the SDK content lock.
- When adding the third item for the first time, the target selection only matched the old 'first-worker-exit' string, resulting in the wrong selection of the last Worker. The item for red4 passed without failure proof. The correction was made by selecting 'last-worker-exit' as the last item, and ultimately, red5 verified that the first PID/actual two failed.
- The first candidate arrangement used Windows' default GBK to read UTF-8 source code, resulting in a failure before compilation. It has been changed to explicitly use UTF-8. Error logs and the original script are retained.
- For the first time, the SDK's new directory was configured using environment variables. The pool test failed to declare the path before the SDK cache was declared; the real CMake new directory 'red' confirmed that all three items were declared incorrectly. After the declaration was moved forward, another new directory 'green' correctly forwarded all three items. This verification only checks configuration/registration and does not replace application execution; the complete 'full1' archive is retained, and the final 'full2' completes full testing across both platforms.

After a full freeze, it was found that the Darwin branch of the new driver copied an unsupported ps -P parameter. The issue has been addressed. [Apple ps Manual](https://raw.githubusercontent.com/apple-oss-distributions/adv_cmds/main/ps/ps.1) Change to enumerating PID/PPID for filtering, and fix the output locale.
This is the only code difference after freezing. post-freeze-driver-change.json records the old/new summaries, complete diffs, and the original code. The Windows/Linux branches remain unchanged byte by byte.
Run the same full2 binary with the final root directory as the driver, in both normal Windows/Linux scenarios, 3/3 times each. For Windows, additional tests are performed for the same object candidates and for those that only move copies, resulting in all candidates passing and two failed instances of the erroneous copy.
The complete full version and TSan's running inputs remain unchanged frozen snapshots; no live testing has been conducted on macOS, and they cannot be corrected by commands or presumed to pass based on registration audits. The old external handovers and parent-stop-driven Darwin enumeration methods still need to be unified and processed.

### 45.3 Freezing the Source Code, Full Regression Testing, and Remaining Boundaries

Windows Strict Release Full **296 items: 284 passed, 0 failures, 12 skipped conditions**; Linux Strict Release/LTO Full **585 items: 580 passed, 2 existing WPT failures, 3 skipped conditions**. Fixed workerd official gate **22/22, zero skips**; same Linux binary root verification **3/3, zero skips**; Linux Debug GCC TSan special **15/15, zero reports, zero skips**.

| Verification | Evidence and Identity |
| --- | --- |
| Native freeze of source code | `17649d1c38c1a4cb6a495ca9487c37d500d2e9d4`, `native-source-pool-final2.json` |
| Linux Frozen Source Code | `a2829d2b9521163ca3334e58952ca44c4e375de6`, `pool-final2-linux-source.json`; 4329 project files are consistent with Native byte, one item is delayed in the root directory due to the above Darwin driver correction and six result documents |
| Windows Full Set | `native-pool-full2.json/.xml` along with complete LastTest, SDK/frame reports, with no exclusions |
| Linux Full List | `pool-final2-linux-full-summary.json/.xml` and the complete LastTest, with no exclusions |
| Ordinary Special Project | Windows 15/15, Linux 16/16, including those previously canceled and nine transfer scenarios |
| Formal Workerd / Root | `pool-final2-required-gate/summary.json`, fully archived and independent after completion on 22/22; same as the regular Linux binary root 3/3 |
| TSan | `tsan-final2-result.json`, same as the final Linux source code 15/15, no suppression; setarch only applies to commands |

TSan covers the real pool, handover, and lifecycle paths of Linux; the Windows release/assign branch is verified by native normal builds and controlled red-green comparison. It cannot be inferred from Linux TSan that this branch has completed the sanitizer qualification.
The complete TSan instrumentation for vendors/dependencies has not been completed; the additional HTTP C object instrumentation evidence from section 44 is retained in the original snapshot. The test for the cancellation of this round of fetch is carried out using the original source code, and no new consecutive 32-repeat results have been declared.
The fixed workerd precise frame count remains four per backend. 4 Connection /56 Frames. Both modes in the Native SDK have frames. 22 Scenario: WPT remains the same. 84 Report /14060 Subtests, 20 expected/3 unexpected/1 missing expected.
The tests are only conducted after both platforms have successfully completed the configure/build process. During the build phase, the CTest process is executed in serial. The complete LastTest/JUnit results are archived, and then the process is repeated in the next round. The binary summaries are consistent across all iterations.

Evidence: `build-v030-dev/evidence/static-pool-fallback-tdd-20261016/`, containing the original texts and summaries of the final six documents; the first 535 files/159936560 bytes are sealed, and earlier seals remain intact.
The workspace HEAD still contains `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, with no commits, pushes, or releases. The WSL clock diagnosis is retained, and no performance conclusions are drawn based on it.

Pending completion: Overlaps with local responses such as write completion, when there are no READY requests from the front end, resulting in a 503 status (currently closing connections in Windows, POSIX closing shard listeners), exiting after the target is re-selected, active drain/connection not yet transferred, and overlapping with target re-selection.
Other cancellation phases, Windows stalled-TCP, real Host dual failure and resource matrix, WPT/profile and differences between the three types of existing workerd, ASan/complete TSan/fuzz/soak/native macOS/Hosted/T15–T17/complete dual build byte-by-byte reproduction.
In this round, only the failed windows with pending-accept targets that have been tested are closed. The complete selector matrix and the overall status of 0.3.0 remain unfinished. Items in T02/T04/T06–T08/T10–T17 will not be closed completely.

## 46. 2026-10-17 Frontend 503 after all Workers exit the pool <a id="section-46"></a>

Continuing from section 45 on the no-READY gap: In the original implementation, Windows closes new connections directly after the entire pool exits, while POSIX closes the public listener with the last shard.
The actual clients have received ECONNRESET and ECONNREFUSED, both of which do not meet the 503-level response required by §10.3. This round of fixes addresses the issue with the local static-pool window, and the managed frontend continues to use its existing implementation.

### 46.1 Front-end Ownership and Responsibility Boundaries

All platforms use the same pool for holding the public acceptor. Worker/shard still has its own session reactor; available connections continue to be assigned to healthy shards. The target is rechecked, and the native handling method is used. The reactor of release/assign is transferred over.
When there are no READY Workers, transfer the sockets that have not yet started requesting IO to the accept reactor, where the reactor completes a bounded HTTP exchange. Do not queue network operations on shards that have already completed.
POSIX no longer relies on the SO_REUSEPORT listener on each shard to maintain access to services. This approach prevents ineffective listeners from sharing resources with healthy shards. This is an internal topology adjustment, and there are no performance equivalent conclusions or native macOS acceptance criteria related to this change.

The new path returns a 503 status, "Body worker unavailable", with a Content-Length of 18 and a Connection: close. A HEAD request only writes response headers, and a legitimate WebSocket upgrade request receives an HTTP 503 response.
Using the existing ListenerCors engine, retain the CORS handling for requests; do not read or save the request body, and do not replay ongoing requests.
No more than 128 connections without Worker swapping are allowed; connections exceeding this limit will be closed directly. The maximum request header size is 8 KiB, with read and write responses each limited to a maximum of 5 seconds. Smaller non-zero request/write timeouts are given priority.
stop/drain: Shut down listeners and complete unfinished exchanges. Wait to release the shard only after the accept reactor is finished. The start/stop control of acceptors is serialized by a mutex. Acceptors, sessions, and timers on the reactor are only operated by their owner.

The product behavior is only modified in static_pool_server.cc; corresponding headers, main comments, configuration/platform documents, and test synchronization are all maintained. There are a total of 11 project files involved, followed by the writing of six result documents.
The implementation of SingleWorkerServer, Worker/runtime/JS, public ABI, 42 vendor patches, SDK/reference locks, and WPT expectations are all identical to the actual final state described in Section 45.
Original product SHA: 992aef283c9b9ab9c94d3f2ef55fc5a9e2998a1d9af129a46d9d8c2e95ca559f; Final product SHA: 2aec6aaa2cb1cfbbb6919ed495ae0092db9d5bbbfb1c4bab95d5987f3f6276d0. The complete diff can be found in the product-diff.patch.

### 46.2 Valid Red Tests, Object-Candidate, and Reverse Control

Expand the original complete pool fixture, still using the common lifecycle/capacity interface; the two actual Workers are identified by the parent PID and executable path, driving the sending of the actual process exit, then waiting for active_workers==0 and both PIDs to be reclaimed.
Inject a fault into all-workers-exit and all-workers-exit-websocket before the first public TCP connection; verify the dynamic challenges and concurrent WebSocket connections of the surviving Workers before exiting the last Worker.

| Comparison | Windows | Linux |
| --- | --- | --- |
| Final permanent testing, original product RED | Three passes/success and three failures in six items. The scenario without READY fails. | Three passes/success and three failures in six items. The scenario without READY fails. |
| Only replace pool product objects, retaining RED C++ objects/drivers/other objects/libraries/Workers | Three rounds in total, 18/18 | Three rounds in total, 18/18 |
| 503 was intentionally changed to 502. | Three new scenarios failed, and three healthy controls passed | Same as before. |
| Intentionally omitting reactor transfers without Worker connections | Three new scenarios failed, and three healthy controls passed | Same as before. |
| Intentionally canceled 128 maximum exchanges | The capacity boundary scenario failed, while the other five passed. | Same as before. |

Each new scenario checks ten complete HTTP exchanges (including HEAD, eight concurrent requests, with the first scenario being a legitimate WS handshake), precise status/body/CORS/length, TCP normal termination, no response until the segment head is completed, as well as stopping the close process for unfinished requests and releasing the actual port.
The capacity boundary actually maintains 128 half-headed requests, with the 129th connection ending with zero bytes; subsequently, the first 128 complete requests result in a 503 status, and then the new requests are verified to successfully reuse the quota.
Idle scenario verification ends with zero bytes TCP within a limited period. Windows timeout closure may manifest as ECONNRESET. Idle timeout and excessive receive allowances allow this error; unfinished requests are stopped, requiring only zero bytes TCP termination. A complete HTTP response still requires error-free termination, with no relaxation in the assertions for body and length.
The healthy scenario still requires two independent challenges for the same surviving Worker, four concurrent WS clients with 12 frames per 262646 payload bytes, and normal code=1000/reason=done.
All official/candidate/error copies must ensure that the fixture exits normally without forced termination, and all actual Workers must be reclaimed. The controlled-results.json file and the corresponding same-test directories should retain objects, binaries, drivers, compilation commands, and original outputs.

In the preliminary Native candidate, the first run returned all ten correct HTTP responses. However, the assertion of "must close without errors" due to idle timeout was too strict. After retaining the failure record, the above issue was clearly corrected, and capacity boundaries were added.
Ultimately, red2 reuses the complete permanent driver to verify the original product. Both platforms still have three true failures each. All official candidates and error copies are then used with exactly the same driver.
There is another old item in the registration audit round, 68, whose count has not been updated with the new required items (205/206). This issue will be corrected in 206/206. Controls such as case missing/ substitution/skip/inversion are retained.
Platform registration required items: Linux 71, Windows 62, Darwin 63; Darwin is only a registration control, not actual hardware evidence. The Darwin driver modified in Section 45 has been included in this round of freeze input, with no new exceptions after freezing.

### 46.3 Final Source Code and Complete Qualification

| Execute | Results |
| --- | --- |
| All targets for Native Strict Release should be configured using configure/build. | All exit=0; Source code 442795fddd7b7d079c0798af4fcfd864198cd78d |
| Linux is a strict system for Release/LTO. For all targets, it’s configure/build. | All exit=0; Source code 20a180a2dd7acc28af5741c873427b1a3c9ebe1b; 4329 project files are identical to Native byte by byte. |
| Windows General Special / Complete Regression | 29/29; Total of 299 items, 287 passed/0 failed/12 skipped due to conditions, zero exceptions |
| Linux General Project / Complete Regression Tests | 30/30; Total of 588 items, 583 passed/2 items with WPT failures/3 conditions skipped, zero exclusions |
| Formal fixed workerd / root qualification verification | The complete log files have been archived independently on 22/22; the same ordinary Linux binary root is on 3/3. |
| Debug GCC TSan | Same as the final Linux source code 29/29, zero reports/zero skips/no suppression; includes six pool failures, original pool lifecycle/WS, and nine handover regression tests |

The actual compilation command for TSan confirms that eight C++ inputs contain instrumentation; POSIX common accept, 503 exchange, and reactor transfer are all within the scope of execution.
Windows IOCP specific APIs still only have native native build/gated network proof; the default vendor HTTP C and broader dependencies have not completed the full TSan instrumentation, and additional C object verification as per section 44 is retained in its original state.
The SDK/reference retains the original input; the official precise frame consists of 4 connections per backend, totaling 56 frames. The Native SDK has 22 scenarios in both modes, and both Host/Python peers complete normally.
WPT maintains 84 reports/14060 sub-tests, with 20 expected/3 unexpected/1 missing expected results. Two existing files failed, and platform 12/3 conditions were skipped without hiding.
Tests are conducted only after the configure/build process on both platforms is successful. The CTest for the same build is run in sequence. The complete LastTest/JUnit is archived first, and then the next round is executed. The binary identities remain consistent before and after testing.
Observation values are extracted from the corresponding scenarios of the complete LastTest. JUnit is used for names/status/selection; complete observations are not derived from the truncated passed stdout that exceeds 1024 bytes.

Evidence build-v030-dev/evidence/static-pool-unavailable-tdd-20261017/; The root directory relative to the final frozen input only contains the subsequent six result documents, which are consistent with the product and test inputs.
Front wheel 394 file/66136293 bytes sealed and earlier seals remain intact; current build/diagnostic process has been reclaimed, and 49 processes from another historical build are retained.
The workspace HEAD still contains 8028b88adff26cbc1abcb9a2fafce2ffc3d8090f, and has not been committed, pushed, or published in the root directory. The WSL clock diagnosis remains, and no performance conclusions have been drawn.

Subsequent actions: Exit after re-selection of targets, active drain/connection not yet transferred, and local response write completion overlap; other cancellation phases, Windows stalled-TCP, real Host dual failure, and resource matrix.
WPT/profile differs from the three existing workerd types; MSVC ASan, complete vendor TSan/fuzz/soak/native macOS/Hosted/T15–T17/fully byte-by-byte reproduction for new dual builds.
After this round of closure, all connections have been removed, and the 503 issue that was reproduced has been resolved. The complete selector matrix and the overall 0.3.0 version are still not completed. T02/T04/T06–T08/T10–T17 will not be closed as a whole.

## 47. Another exit before actual handover after re-selection of Worker on 2026-10-17 <a id="section-47"></a>

Section 46 ensures that the front-end can still return a bounded 503 status even when there are no READY Workers available. However, there are still some limitations: after the pool selects healthy Workers, adopt_socket is used to prepare the transport/callback mechanisms. This process aims to ensure that the tasks can be completed before the final post-gating step occurs.
The old interface receives sockets by value, and when a connection is rejected, the resources have already been consumed. The caller ignores false values, neither handing them over to the remaining healthy Workers nor handling a 503 response for the full pool with no capacity. The actual original connection results in ECONNRESET on both platforms.

### 47.1 Connection of Ownership and Limited Re-election

The addition of try_adopt_socket(socket&) to SingleWorkerServer: First, allocate an unopened transport and a callback, then enter the gate of the existing adoption_mutex.
false retains the caller's socket/native handle/executor; the actual connection is moved and posted only after successful gating. There is an adopt_socket(socket by value) that continues to consume the connection through local parameters, and the behavior of the existing caller remains unchanged.
The abnormal path allocation of post moves the connection in transport back and throws it again; this exception boundary is based on the Boost.Asio source code checked during this round of installation, and it cannot be used to claim that allocation failure/OOM has been fully executed.
After a post is successful, the owner handler still reviews the exit status. Any exit or stop that occurs thereafter can close the queued connections. No asynchronous re-selection or request replay is introduced in this round.

The pool only reselects before requesting IO begins. Each time, it first checks the stop/receive closed status, then selects the available shard, explicitly rehome the native connection, and then attempts to handoff.
After rejection, the connection is still held by the frontend, with the option to proceed to the next shard. Up to N configured Workers are tried to avoid unbounded loops. If there are no available shards and the frontend is still open, the original connection enters section 46 with a bounded 503 status. If a stop/drain or shard gating is found to be closed before the next attempt, the connection is terminated and handed over.
The product has been changed to static_pool_server.cc, single_worker_server.cc/.h; the remaining five project files are registration files, two new fixtures, and registration control. Worker/runtime/JS, public ABI v7, 42 vendor patches, SDK/reference locks, and WPT expectations are the same as the actual state in section 46.
Pool SHA 2aec6aaa2cb1cfbbb6919ed495ae0092db9d5bbbfb1c4bab95d5987f3f6276d0 → b4c9d866930701a40cce69b6c6b6a07d6a285e167da37c646d0d1814fd9d4176; SingleWorkerServer SHA 00fd4731524148cf7a1fcc0ce25e07630cc36afd230e6e2b3454b48b1eb2b9e6 → 88091986ed64025a5e3627f61528a5ac5000c691f5d14d697990baf25afb4ac8. For the complete changes, see product-change.diff.

### 47.2 Real Preconditions, Red Testing, and Reverse Comparison

New test_host_static_pool_adoption.cc/.mjs has been added. It starts three complete real Workers, obtains ports through private READY, and confirms the shard order by OS parent PID/executable path/greatly increasing creation time.
In the normal failure scenario, the original target exits before the first public TCP, ensuring that active==2 and the original PID has been reclaimed. Subsequently, only the actual transport allocation of the calibrated size is captured within the test executable file, and it is checked that the call stack belongs to the real adopt_socket/try_adopt_socket.
Windows reads the local PDB before any Worker starts, verifies its own private function symbols, and saves the address range of the actual transferred function. The race condition window does not execute DbgHelp queries. POSIX uses the actual exported function symbols and backtrace/dladdr. The product does not have conditional compile timing hooks, and the optimization level follows the build configuration.
Darwin-driven system first verifies the parent PID/path using ps, and then queries the microsecond creation time through the fixture's XNU KERN_PROC_PID. Field definition verification is performed. [XNU sysctl.h](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/sys/sysctl.h) and [proc.h](https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/bsd/sys/proc.h) This has only completed the code/registration preparation. There is no native Mac compilation and running for acceptance yet.

| Scenario | Prerequisites and Actual Assertions |
| --- | --- |
| reselect-worker-exit | During pauses, two Workers are available. When exiting, the target is re-selected. The active state is set to 1/PID, and it is released after being reclaimed. HTTP must reuse the original TCP. The 200 response body is correct. Four concurrent WebSocket connections each send 8 frames with a payload size of 131326 bytes. Subsequently, the new HTTP request is returned by the same surviving Worker, with a separate challenge. |
| reselect-last-workers-exit | Exit the remaining two Workers at the same real pause point, active==0/ both PIDs are recycled; both the original TCP and the subsequent new TCP must receive a complete 503/worker unavailable/Content-Length 18/Connection close response. |
| pre-post-stop | Write a complete GET on the paused connection first, then request_stop; release after the two remaining Workers have exited, and the original TCP must end with zero response bytes. |
| pre-post-drain | Write the complete GET statement first. The idle shard will begin_drain and then end normally. Release the paused connection with zero response bytes, without calling stop to promote the wait of the successful path. |
| active-drain | First, establish and complete the WS transmission and reception in three actual partitions. When the original target exits, its connection is set to 1006. The remaining two connections complete the transmission and reception barriers after draining, proving that the owner is still available. After releasing the paused connection, the connection ends with zero response bytes. Currently, two WSs continue to transmit and receive, accumulating 24 frames per 393978 payload bytes, and shutting down normally at 1000/done. |

All successful scenarios require the fixture to exit with exit=0, no forced cleanup, all actual Workers to be reclaimed, and public listeners to be released. A complete HTTP response is required to have a complete and normal TCP termination; a system can be closed with an ECONNRESET report when the stop/drain state reaches a zero byte end state.
drain_metrics.finished performs an AND operation on all shards. Shards that have already been evicted before draining will not complete draining again. Therefore, this round's honest assertion is draining=true, finished=false, forced_cancellations=0. With waits not calling stop, zero active Workers, and OS recycling, it is proven that the remaining shards will naturally end without reporting the aggregate as finished=true.

| Comparison | Windows | Linux |
| --- | --- | --- |
| Final red5, original section 46 three product documents | Two failures and three passes in five attempts | Same as before. |
| Only two product objects and API declarations need to be replaced. The RED fixture objects/drivers/libraries/workers remain unchanged. | Three wheels 15/15 | Three wheels 15/15 |
| Intentionally ignoring the rejection of try_adopt_socket | Two re-selections failed, three comparisons passed | Same as before. |
| Intentionally moving connections before the final gating | Two re-selections failed, three comparisons passed | Same as before. |
| Intentionally omitted the adoption_open gate control | Only active-drain failed, the other four passed. | Same as before. |

All three types of error copies failed at the actual network assertion point, with the fixture exiting normally without forced cleanup; the error copy of the activity drain actually returned response bytes that should have denied the connection.
Early testing preparation failed, all retained: Initial version error requiring pool level finished=true; Windows symbols not resolved/query delay caused a pause in preconditions failure; Inadvertent compilation of header files with incorrect order and incomplete local include paths. They are not considered as product RED, nor are they considered as candidates for approval.
Ultimately, red5 only adopted the permanent fixture. The original implementation resulted in two network failures on each platform. Subsequently, the candidate and erroneous copies used the same C++ test objects and drivers. The source code/objects/binaries/PDB/compilation commands and original outputs have been summarized and archived.
Registration Control 231/231, required items: Linux 76, Windows 67, Darwin 68; five new items added to prevent disappearance, and rejecting incorrect commands, aliases, DISABLED/WILL_FAIL/SKIP_RETURN_CODE, and missing deadlines. Registration verification is not an application runtime evidence.

### 47.3 Final Input, Complete Regression, and Compatibility

| Execute | Results |
| --- | --- |
| All targets for Native Strict Release should be configured using configure/build. | All exit=0; Source code fc36ccd28a8b849b953d3df542d93e1dd5e7274c |
| Linux is a strict system for Release/LTO. For all targets, it’s configure/build. | All exit=0; Source code ee17b13a735d67fb9fcc016b054f6e83b617e223; 4331 project files are identical to Native byte. |
| Windows General Special / Full Set | 34/34; 304 passes to 292/0 fails/12 skips conditions, zero elimination |
| Linux General Project / Full Set | 35/35; 593 items passed, 588 passed/2 failed with WPT issues/3 conditions skipped, zero exclusions |
| Fixing the official gate for workerd / conducting supplementary inspections for root | After complete archiving, independently 22/22, with no skips; same ordinary Linux binary root 3/3 |
| Linux Debug GCC TSan | Compared with the final Linux source code, 34/34… No reports, no skipping, no suppression. In reality, 11… The C++ input involves instrumentation. |

The complete version includes five new pool failures, six original pool failures, nine external transfers, and adjacent lifecycle aspects, in addition to TSan. TSan has actually verified the compilation commands for new pool selection and the code for SingleWorker transfer, as well as the read-write instrumentation symbols for fixtures.
Windows IOCP specific APIs still only have native ordinary build/controlled network evidence; the default vendor HTTP C and broader dependencies have not completed the full TSan instrumentation. Additional HTTP C object evidence from section 44 is retained in its original state and not rerun in this round.
Fixed workerd v1.20261010.1, compatibility date 2026-08-20, flags=[], and the SDK content lock remains unchanged; the official precise frame is still 4 connections per backend/56 frames, with 22 scenarios in both Native SDK modes, and both Host/Python peers complete normally.
WPT maintains 84 reports/14060 subtests, with 20 expected/3 unexpected/1 missing expected results. Both existing files (promise-rejection-events, MessagePort_initial_disabled) and Windows 12/Linux 3 conditions were fully included in the results. The three Linux condition items were also passed with the same binary root.
Each build is subjected to its normal runtime tests only after successful completion. It follows the same serial order as build CTest, and after archiving the LastTest/JUnit, it proceeds to the next round. The normal Host/Worker binary summaries remain unchanged before and after testing. The observed values are extracted from the complete LastTest scenario segments, avoiding the truncation of 1024 bytes from the passed JUnit stdout.

Evidence build-v030-dev/evidence/static-pool-adoption-retry-tdd-20261017/. Only six result documents are written in the root directory after the final freeze; there are no exceptions for late changes in product and testing.
Section 46, File 415/312173896 bytes and earlier sealed items are retained as they are; the current build/diagnostic process has been reclaimed, and 49 processes from another historical build are retained. The root directory HEAD remains at 8028b88adff26cbc1abcb9a2fafce2ffc3d8090f, and has not been committed, pushed, or released.

### 47.4 Subsequent Qualification Scope

This round of closure re-selection targets the re-emerged window that has exited again. After a successful post, the owner can still exit and close the queued connections. It is necessary to continue calling drain to close the owner's gate, and to incorporate the queued state with normal service/stop/drain, as well as the local 503 write completion into the complete selector matrix. The limited re-selection of this round cannot be described as an automatic retry of the executed requests.
Front-end exceptions/overlimits, request body/CORS preflight, reception limit and exit concurrency; other fetch cancellation phases and Windows stalled-TCP; real Host priority/teardown double failures, origin/Bindings/CPU/heap/OOM/GC/resource recycling matrix still open.
Three types of differences exist in workerd (close-code 999, limited sending queue, unrequested subprotocol), WPT workerd/standards profile, and outdated expectations. MSVC ASan, complete vendor TSan/fuzz/soak/native macOS/Hosted/T15–T17, and full byte-by-byte reproduction for the new dual build have not yet been accepted.
0.3.0 Overall unfinished, T02/T04/T06–T08/T10–T17 will not be closed as a whole.

## 48. 2026-10-17 Drain call and pending external handoff <a id="section-48"></a>

Section 47 closes the window for re-selection of targets in the complete pool before post, allowing for further exits. This round checks the shared external input ports of SingleWorkerServer along with its pending tasks.
The original implementation separates the atomic marking of begin_drain from the gate closure on the owner: when try_adopt_socket is called after it has already returned, and the owner has not yet been executed, it can still return true; the handoff callback that has already been posted does not check for draining.
The existing WebSocket activity will keep the Worker available, so it cannot be relied upon to quickly drain idle resources and reject new requests. The original implementations on Windows and Linux actually returned an HTTP 200 response, which should have been used to reject the connection.

### 48.1 Fixes and Precision Range

- The final admission mutex now includes a new acquire read and draining mechanism. Rejections occur before moving to the original connection; false values still retain the original native handle and executor.
- The queued owner callback checks for draining again before creating a Session. After starting the draining process, the connection is terminated directly. The original HTTP/WS session continues to be processed according to the original draining semantics.
- The header files only supplement the above contract description. Function declarations and ABI remain unchanged. The consumption behavior of the adopt_socket function is maintained according to the original contract.
- static_pool_server.cc has not been changed; the new fixture in this round directly calls the common SingleWorkerServer API. It uses one worker that needs to be tested, along with one independent health worker. This cannot be considered as an addition to the full pool selector. 47 The migration of five complete pools back to their original state, as well as the restoration in case of failures in six complete pools, are still being carried out within this round of operations. All relevant data is also being processed using TSan.

The seven project documents for this round can be found in pre-final-root-delta.json and product-change.diff; the project files for Native 0a796c0413b6aff3ab7ae2df4832947e0e21daf4 and Linux 34f69f8c402ac3a50414737ebe586cc9094f3494 are byte identical in 4333.
SingleWorkerServer SHA 88091986ed64025a5e3627f61528a5ac5000c691f5d14d697990baf25afb4ac8 → b8ef778544b849d9c6ad7fc4c5dc8d9b90df477f5450396789e1e4af914f9567.
After the product/test freeze, six result documents, including this section, were written. The other project files in the root directory have exactly the same inputs as the freeze. The Worker/runtime/JS/ABI v7, 42 patch, SDK/reference locks, and WPT expectations are carried over from the original inputs.

### 48.2 Real Preconditions and Five Scenarios

New test_host_external_drain.cc/.mjs has been added, which determines the identities of the two Workers through the actual parent PID/executable path. The test itself blocks the callback to the executor under test, and there is no scheduling hook in the product. The TLS allocation barrier for preparing the scenario only surrounds the actual try_adopt_socket call, and the allocation size for the actual transport that is not open is calibrated accordingly.
In each scenario, a real WebSocket is established before draining resources. After the owner recovers, communication continues to proceed. The independent workers also need to return different random challenges along with the complete HTTP requests for the same instance. 200 There are already WSs available. 24 frames /393978 Payload bytes. text/binary Each covers different areas. 0, 1, 126, 65536 Ultimately… 1000/done Closed normally.

| Scenario | Real Order and Successful Assertions |
| --- | --- |
| drain-before-submit | Block the owner, return from begin_drain with draining=true/available=true, then commit the original TCP; if false, retain handle/executor, and the TCP returns zero bytes. |
| drain-during-prepare | The owner is blocked, the real transport allocation is paused, and draining=false/available=true; the allocation is released after begin_drain; ultimately, false retains the original connection, with zero response bytes from TCP. |
| queued-before-drain | When the owner is blocked, try_adopt_socket returns true, followed by begin_drain returning, and then the owner is released. The queue callback must reject, and the TCP response is zero bytes. |
| owner-drain-before-submit | The WS remains active; the owner marked after the drain task confirms that it has been processed, then submitted again; false connection is retained, with zero response bytes from TCP. |
| healthy-control | The owner is also blocked, but does not drain resources; true ownership transfer, after recovery, the original TCP returns a complete HTTP 200 response, with a correct random challenge, different from the instance of the independent Worker. |

The HTTP client in the rejection scenario observed no actual response/ECONNRESET. The original TCP capture bytes must be zero. Normal response checks include complete, Connection close, actual TCP close, and dynamic payload. The successful drain scenario is completed through natural wait, without the need for request_stop to terminate the Worker under test. finished=true, forced_cancellations= 0. Both Worker PIDs have been reclaimed, both public listeners have been verified to be released, and the fixture exit= 0, No Mandatory Closure.

| Original Test Object Comparison | Windows | Linux |
| --- | --- | --- |
| Section 47: Product | Three failures in the first three items, two pass in the last two items | Same as before. |
| Only the SingleWorkerServer product objects are replaced. | Three wheels 15/15 | Three wheels 15/15 |
| Remove the draining check for the final gate | The first two items return true; the remaining three items pass. | Same as before. |
| Remove the draining check for the owner callback | Only queued-before-drain returns an actual HTTP 200 failure. | Same as before. |
| Reject previous consumer connections | The handle/executor for the three simultaneous rejections retains an assertion failure; the remaining two pass. | Same as before. |

All candidate/error copies use the same RED C++ objects, drivers, header files, libraries, original product inputs, and real Workers. The differences in header files during this round of processing are merely annotations. Error copies also undergo WS/health HTTP verification, proper exit of the test fixtures, and recycling of Workers. There is no forced cleanup process, which indicates that all assertions can correctly detect corresponding degradations.
Initial registration for control of update failures in nested inventory numbers, the size of a single native link file, and syntax errors in comparison scripts have been archived. These do not count towards product red testing or successful runs. The native process begins formal RED testing only after a successful retry of the complete configure/ALL build.

### 48.3 Complete Regression and Compatibility with workerd

| Qualification | Actual results |
| --- | --- |
| Windows Strict Release Full Copy | 309 items: 297 passed, 0 failed, 12 skipped due to conditions; zero exclusions; Special case 39/39 |
| Linux strict Release/LTO Full Version | 598 items: 593 passed, 2 with WPT failures, 3 skipped due to conditions; zero exclusions; special case 40/40 |
| Fix the official gate for workerd | After complete archiving, it is the same as ordinary Linux binaries 22/22, with no skipping; precise frame four backend has 4 connections/56 frames each. |
| The same ordinary Linux binary root | Three condition test cases 3/3, zero skips |
| Linux GCC Debug TSan Special Project | 39/39, zero reports, zero skips; verified 14 actual compilation commands, 13 of which have C++ input instrumentation, and checked the __tsan_read*/write* references of the new fixture objects |
| Registration for authenticity control | 256/256; Required: Linux 81, Windows 72, Darwin 73; New items cannot be deleted, scenario changes, driver/fixture/Worker/Node replacement, or disable/invert/skip/timeout cancellation |

There are two modes for the Native SDK. 22 Scenarios, Host/Python Peer exits normally. workerd/SDK/reference The lock has not been moved. WPT remains the same. 84 Report /14060 Subtests, 20 expected/3 unexpected/1 missing expected.
Linux has two failure events, namely promise-rejection-events and MessagePort_initial_disabled. The conditions for Windows 12/Linux 3 are skipped for full disclosure. Darwin is prepared for source code/registration purposes, but no native Mac acceptance testing has been conducted. Windows IOCP does not have a sanitizer for acceptance testing, and the default vendor HTTP C and other dependencies have not been fully instrumented. The conclusions of TSan are limited to this selected project.
Evidence build-v030-dev/evidence/external-drain-admission-tdd-20261017/; comprehensive observations are found in drain-observations.json, adoption-observations.json, and fault-observations.json corresponding to the LastTest section. The passed JUnit stdout is truncated to 1024 bytes, used solely for checking names, selections, and statuses.
Section 47, File 481/2440915497 bytes and earlier: Seven sealed files have been reviewed and remain unchanged. This round of processes has been recycled, and 49 processes from another historical build have been retained. The root HEAD is still 8028b88adff26cbc1abcb9a2fafce2ffc3d8090f. There are no root directory commits, pushes, or releases. Performance conclusions cannot be drawn based on WSL clock observations.

### 48.4 Keep Open

The drain-call/ready/queued-owner window with shared entry points has been fixed. The overlap between the complete pool's cross-shard selection, the owner exit/stop after successful post, and the local 503 write completion with stop/drain still requires independent tests. It cannot be derived by the two direct API Workers in this round.
Front-end anomalies/overlimits: head-level issues, body/CORS pre-flight checks, reception limits and concurrent operations; other fetch operations fail, Windows stalls in TCP scenarios. Real-host-related issues and teardown failures also occur. origin/Bindings/CPU/heap/OOM/GC/resource recycling mechanisms remain active.
There are three types of differences: workerd close-code 999, limited sending queue, and no request for subprotocol. Additionally, there are issues with WPT profile/old expectations, MSVC ASan, and the complete vendor TSan/fuzz/soak/native macOS/Hosted/T15 – T17/new dual build, which has been reproduced byte by byte without any acceptance.
0.3.0 Overall unfinished, T02/T04/T06–T08/T10–T17 will not be closed as a whole.

## 49. 2026-10-17 Local 503 Late arrival callback and complete pool closure regression <a id="section-49"></a>

Close the task along section 48 with a local response. This round includes five real network regression tests. The current production implementation is passed as is through the added matrix, and no new product defects have been reproduced. Therefore, there are no product REDs or product fixes declared.
The changes only involve six project files: cmake/build_tests.cmake, three new test files, two registration verification files. The static_pool_server.cc, SingleWorkerServer, Worker/runtime/JS/ABI v7, 42 patch, SDK/reference lock, and WPT expectation remain the same as the previous actual final state.

### 49.1 Two-Layer Realistic Coverage

| Testing | Actual Prerequisites and Assertions |
| --- | --- |
| host_unavailable_lifecycle_get-close-before-completion | The original product UnavailableSession uses real TCP; the independent client io_context sends the complete GET first, and the owner only runs one read completion handler; the client has received 503 bytes and finished=0. Subsequently, the same owner calls close twice and removes the registry hold; to perform writes, it must continue to hold the Session; after emptying the real completion/cancellation callbacks, finished remains at 1, the weak hold expires, the registry is empty, and there is an HTTP 503/body and TCP closure. |
| host_unavailable_lifecycle_head-close-before-completion | In the same order, the actual HEAD must have a Content-Length of 18 and no body. Late callbacks should not repeat the end or leave any objects behind. |
| host_unavailable_lifecycle_get-normal-completion | Maintain the same position as "bytes have been written, but the completion callback has not been executed", then complete the process normally. Check for worker unavailable, CORS, Connection close, one-time termination, and object release. |
| host_static_pool_local_shutdown_stop | The complete dual-worker pool first sends two dynamic HTTP challenges from two different instances, and completes WS text/binary in eight frames. After both actual sub-PIDs exit and are reclaimed, verify the local GET/HEAD and 503. Allow/deny the preflight request 204/403. Create three incomplete GET/HEAD/OPTIONS requests, with a fourth complete 503 response confirming that the previous three requests have been accepted and reading has begun. Repeat request_stop three times. All three TCP connections must close within 2 seconds, and both concurrent wait calls must be completed. |
| host_static_pool_local_shutdown_drain | For the same complete pool and pre-matrix, repeat begin_drain three times; the end connection of the read head is the same, and the end is pushed by normal wait and request_stop without a successful path. The ports are released, all sub-PIDs are reclaimed, and there is no forced cleanup. |

The private session tests directly include the original src/host/static_pool_server.cc, using real anonymous classes in the same test translation unit. No session implementation is copied, private access control is not changed, and product scheduling hooks are not inserted. The single-step owner and independent client io_context ensure that receiving packets does not execute the completion callback for the owner.
Two new targets follow the GNU -Wno-maybe-uninitialized option of the adjacent fixtures; GCC triggers the subobject-linkage diagnostic additionally for the original.cc files included, with a new exemption for this specific include case. All other strict compilation checks, runtime assertions, and sanitizer instrumentation remain enabled. There are no suppressions for TSan.
The acceptance test does not initiate the Worker; the complete pool will verify the actual parent PID/path, the dynamic response of two Workers, the WS eight frames/131326 payload bytes, as well as the actual 1006 after exit. Their responsibilities are respectively the late write callback lifecycle and the complete pool pending read head recycling, and they cannot replace each other.
All Workers exit before draining, so the complete pool with draining=true/finished=false is a true aggregate; forced_cancellations=0, concurrent waits, and OS recycling are proven to complete the shutdown separately. In the stop scenario, draining=false/finished=false.

### 49.2 Repeat Run and Reverse Control

Final input consists of three rounds of five items each for both platforms, totaling 15/15. The initial synthetic CTest inventory lacks placeholder new native executables, and the proprietary diagnostics for GCC's include and corresponding corrections are retained; these preparations failed and are not counted towards the product RED. For Windows, there were three additional rounds with 15/15, which were not included in the final input statistics.

| Error Copy | The combined results of the two platforms |
| --- | --- |
| The callback function is called after the translation is completed. Only raw pointers are retained. | Two "close-before-completion" tasks failed at "the queue must continue holding the Session"; normal completion and two full pools passed in comparison. |
| Remove the closed idempotency protection and retain the finished callback copy. | Two of the "close-before-completion" tasks failed at the "one-time completion" stage; the other three passed. |
| Missing cleanup of unavailable sessions when pool is closed | The complete pool's stop/drain operations failed at the TCP close deadline; three direct sessions passed the check. |

The first two types of copies maintain the original test source code, only recompiling the.cc files that include these products. They cannot be described as retaining the unit objects. The third type only replaces the complete pool of product objects, while retaining the fixture objects, SingleWorker objects, libraries, Workers, and Node drivers.
Each copy records the actual compilation/linking commands, input summaries, differences, and original output. The original binary and library summaries remain the same before and after. Both unit errors exit with a visible assertion of 1, and ineffective callbacks are not executed after incorrect holds are detected in advance. Copies of the complete pool errors retain the main assertion, with fixtures exiting with exit=0, no forced cleanup, PID recycling, and port release.

### 49.3 Full Recheck with workerd

| Qualification | Results |
| --- | --- |
| Windows Strict Release Full Copy | 314 items: 302 passed, 0 failed, 12 skipped due to conditions; zero exclusions; Special case 44/44 |
| Linux strict Release/LTO Full Version | 603 items: 597 passed, 3 WPT files failed (including this round's hr-time), 3 conditions skipped; zero exclusions; Special case 45/45 |
| Fix the official gate for workerd | After complete archiving, it is the same as ordinary Linux binaries 22/22, with no skipping; four backends with 4 connections per frame/56 frames. |
| The ordinary Linux binary root | 3/3, Zero Skips |
| Linux GCC Debug TSan | 44/44, zero reports, zero skips; verified 18 actual compilation commands, with 17 projects having C++ input instrumentation; real private sessions are instrumented with test TU instrumentation, and their objects have __tsan_read*/write* references |
| Registration Control | 288/288; Required items: Linux 86, Windows 77, Darwin 78; None of the five scenarios/commands, disable/invert/skip, or limited timeouts should be deleted, replaced, disabled/inverted/skipped. |

The original six complete pool failures, five complete pool handovers, five external-drain events, nine external-accept events, and the adjacent lifecycle continue to be executed in the special project, full volume, and TSan scenarios. There are 22 scenarios for each of the two modes of the Native SDK, and the Host/Python peers exit normally.
WPT consists of… 84 Please provide a complete output report. /14060 Subtests, 20 expected/4 unexpected/1 The expected results are not achieved. Specifically, 81 Accepted by the verifier. 3 One piece of evidence indicating failure was rejected. Two existing documents failed; the reasons are “promise-rejection-events” and “”. MessagePort_initial_disabled There is another round of this process as well. hr-time/basic.any.js.Windows 12/Linux 3 The conditions are skipped entirely; it is not claimed that all platforms or WPT have met these requirements.
hr-time/basic.any.js failed in this round of full-scale addition. Date/performance has a small increment difference. 1433.497 ms. The initial orchestration was aborted due to addition failures and retained the exit records. It will be executed independently later under the root/gate procedure. The original 30 ms tolerance, WPT expectation, or product clock has not been changed, and rerunning has not covered the full scale. Fixed 20, secondary diagnosis retained. 19 passed, 1 failed; there was a small increment difference within the failed process. 1325.998 ms corresponds to REALTIME/MONOTONIC jump. 1325.120 ms, sampling read span. 0.960 µs, supports the judgment of clock jumps in the environment. The full scale only has minute-level test case times and cannot be used as equivalent precision for process-level localization; Linux full scale is still 3 failed, stable clock environment retesting not completed.
The 4336 project files of Native e5ee2016583a9c5826a3288ad2f17d908f333b33 and Linux c5686af18d70c61cd7c37e757c1daf0ac3e10e66 have identical byte content. After the product/test freeze, only six result documents are written later. Evidence can be found in build-v030-dev/evidence/local-unavailable-lifecycle-tdd-20261017/. The observed values are from the corresponding segments of the complete LastTest. The passed JUnit stdout is truncated to 1024 bytes for verification purposes, specifically for checking names, selections, and status information.
Section 48, File 344/626450749 bytes and earlier, eight sealed files have been reviewed and remain unchanged. This round of processes has been recycled, and 49 processes from another historical build have been retained. The original repository HEAD remains at 8028b88adff26cbc1abcb9a2fafce2ffc3d8090f, which has not been committed, pushed, or published. The WSL clock records are not used for performance conclusions. Each platform has archived five original objects and two test binaries (a total of seven), and the original 16 input summaries have been checked for subsequent error copies for review.

### 49.4 Continued Open

This round has determined coverage for the holding/end of private 503 sessions' late write callbacks, as well as the recycling of read heads for complete pool stop/drain, and the HEAD/CORS protocol. The blocking/write timeout when the complete pool kernel write is still incomplete, as well as the more complex arrangements of write completion and switch/stop/drain, still require independent controlled prerequisites and cannot be derived from the two-layer tests in this round into a complete matrix.
After the complete pool crosses shards and is successfully posted, combinations of owner exit/stop, exceptions, or exceeding limits in head/body/limits and exit occur; other cancellations, Windows stalled-TCP, real Host precedence/teardown double failures, origin/Bindings/CPU/heap/OOM/GC/resource recycling remain open.
The three types of differences in workerd, WPT profiles/old expectations, HR-time stable clock environment reverification, MSVC ASan, complete vendor TSan/fuzz/soak/native macOS/Hosted/T15–T17/fully built-up byte-by-byte reproduction have not yet been accepted. By default, the vendor HTTP C/larger dependencies have not been fully instrumented. Windows IOCP lacks a sanitizer for acceptance, and Darwin only has source code/registration preparation.
0.3.0 Overall unfinished, T02/T04/T06–T08/T10–T17 will not be closed as a whole.

## 50. 2026-10-17: Writing Pass-Fail Tests and Regression Tests on the Complete Pool Backpressure <a id="section-50"></a>

This round of audit found that the original host_write_timeout_cancels cannot prove write timeout: require_connection_closed_after_ignoring actually reads immediately after sending the request. A complete 4 MiB response would also pass the assertion of "fast close" after the Connection is closed.
Both platforms simply changed the arm_write_timer of the actual SingleWorkerServer to an empty operation, retaining the original test objects, pool objects, libraries, Workers, and other product logic. The original main test cases and the two control scenarios were still all passed. The Linux main test case took approximately 73 ms, and the Windows one took about 985 ms. Even without the write timer, the test cases were still passed, providing direct reverse evidence. This only proves that there was a test omission, but it cannot be used to conclude that there was any human cheating.
After the tests were corrected, the source code passed as is. There were no new product defects or product fixes reported in this round. Four project files have been changed: tests/test_host_write_timeout.cc, cmake/build_tests.cmake, and two platform registration checks. Four new CTest scenarios have been added, while the original three names have been retained with tighter behavior. The runtime, ABI v7, 42 patch, SDK/reference locks, and WPT expectations remain unchanged.

### 50.1 Real Preconditions and Seven Actions

The main text generates a response of 8 MiB in size, with a Content-Length of 8388608. The client sets a receive buffer of 1 KiB before connecting, records the actual value from the OS, and uses MSG_PEEK to observe the complete 200 response headers and the starting text. MSG_PEEK does not consume any data.
The inflight limit for each shard is 1; the queue is closed. First, dynamic challenge/UUID responses are used to ensure that each worker is functioning properly. Then, each worker starts generating large responses that have not been read yet. For normal health checks, a 429 response must be received from each shard, indicating that the capacity has been fully utilized. A complete pool requires two different actual worker instances.
Before any text is read, a 500 ms write timer must restore the health challenge of all original Workers by 200, with a limit of 2.5 seconds, which is less than the independent request timeout of 10 seconds. Only after that, the client window can be expanded, and the actual TCP connection can be cleared. It is necessary to retain the original 200/Content-Length and the prefix "a" being non-empty. Beast confirms that the text is truncated/partial_message; a complete response cannot be disguised as a cancellation.

| CTest | Core Assertions |
| --- | --- |
| host_write_timeout_cancels | Single Worker, capacity recovery occurs before client reading, actual original response is truncated, subsequent dynamic requests are normal. |
| host_worker_deadline_independent | Worker has a slow response time of 500 ms. Independent requests time out for 200 ms, resulting in a 504 error. The write timeout is set to 5 seconds. |
| host_fast_response_untouched | Actual small response 200 and dynamic text are complete. Writing beyond the deadline will not interfere with the normal completion process. |
| host_write_timeout_disabled | write_timeout_ms=0, remains 429 after not reading for 800 ms; subsequently, all of the original 8 MiB is fully read, and then it is restored to 200 |
| host_pool_write_timeout_cancels | The complete dual Worker pool has two unread large responses occupying both shards. Both responses are read only after the full actual Worker capacity is restored. Both responses are truncated. |
| host_pool_drain_write_deadline | The same barrier is followed by three calls to begin_drain(5000), keeping the client reading and waiting normally. A write timeout of 500 ms ends the process within 2.5 seconds. Both draining/finished are true, with forced=0. The successful path does not require using request_stop to initiate the wait. |
| host_pool_stop_pending_write | Same barrier, write timeout is 5 seconds, request_stop three times; normal wait within 2.5 seconds, no forced drain, both actual responses are truncated. |

All assertions use the public API and real TCP, without copying the product implementation, modifying private parts, using product timing hooks, or deliberately ignoring failures. Exceptions are properly handled through RAII's normal stop/wait mechanism. Reverse copies also require normalCleanup=true and exit=1. Subsequent OS process verification and full archiving are completed independently.
The full pool of this round follows the response path of the surviving Workers. The local 503 response, UnavailableSession, requires completion of kernel writing/writing timeout and more overlapping conditions, which cannot be derived from this round. The late write callback in Section 49 and the pending read head test remain retained.

### 50.2 Reverse Engineering and Evidence Scope

Final input: Windows, Linux, three rounds, seven items, 21/21. Linux was input earlier with another 21/21, which was not incorporated into the final input. Errors in initial MSVC strict signed comparison, registered numbers/CTest paths, and tag arrangement preparation were recorded, but not counted towards the product RED.

| Only replacing the erroneous copies of product objects is required. | Results of the two platforms |
| --- | --- |
| arm_write_timer is an empty operation. | Failure in three criteria: Single Worker timeout, Pool timeout, Pool drain. Success in four criteria: Independent request timeout, Fast response, Disabled write timeout, and Pool stop. |
| write_timeout_ms=0, still set to 500 ms | Only the failure of the write timeout is disabled, and the other six items pass. |

The final two copies retain the same test objects, pool product objects, all libraries, original Worker, and header files. Only the SingleWorkerServer product objects are recompiled and replaced, with records of the actual compilation/linking commands and the SHA-256 values before and after the changes.
The old test objects remain unchanged during the initial audit period. The same common target is then rebuilt using the new tests. The old source, old copy binaries, original logs/commands/summaries are retained, and it is not claimed that the current common target is still from the old tests. In the end, three original objects and one normal test binary are archived for each of the two platforms. The library and Worker are bound to the summary.
Registration Control **365/365**; Required items: Linux **93**, Windows **84**, Darwin **85**. Seven items to be verified individually: name, direct executable/Worker/scenario, limited timeout, non-disappearance, alias, disable/invert/skip; Darwin only requires synthetic registration verification.

### 50.3 Complete Regression and Compatibility

Windows strict Release full set **318 items: 306 passed, 0 failures, 12 conditions skipped**; Linux strict Release/LTO full set **607 items: 602 passed, 2 WPT files failed, 3 conditions skipped**. Fixed workerd official gate **22/22, zero skips**; same as regular Linux binary root **3/3, zero skips**; GCC Debug TSan special case **51/51, zero reports, zero skips**.
Ordinary special projects: Windows 51/51, Linux 52/52. TSan verified 21 actual compilation commands, of which 20 included C++ input instrumentation, along with new write timeout fixtures and two product objects generated by compilation. No new suppressions were added; the default vendor HTTP C is still not instrumented, and Windows IOCP and native Mac do not have sanitizers/real machine qualifications.
The original faults, handovers, external-drain, local 503, and adjacent evictions continue to be executed in the special, full, and TSan phases, and are observed from the verification values corresponding to the complete LastTest segment; the passed JUnit stdout is truncated to 1024 bytes for use in selection, naming, and status purposes.
The fixed workerd access control mechanism is implemented using the same standard Linux binary code after all data has been fully archived. Four backend systems communicate with 4, and /56 transmits frames. The Native SDK operates in two modes: 22 handles certain scenarios, while Host/Python handles normal peer interactions. This access control mechanism does not indicate that any differences between the three types of workerd systems have been eliminated.
WPT consists of… 84 Please provide a complete output report. /14060 Subtests, 20 expected/3 unexpected/1 missing expected; 82 Accepted by the verifier. 2 One piece of evidence indicating failure was rejected. Events related to promise-rejection-events… MessagePort_initial_disabled Two related files failed to be retained. Windows 12/Linux 3 The conditions are not hidden.
This round of hr-time/basic.any.js has passed. The complete 3 failures and 20 clock diagnostics from section 49 are retained in the original archive. The results of this round do not cover them. There are no changes to the original 30 ms tolerance or WPT expectation. The stability of the clock environment is still tested, and WSL records are not used for performance conclusions.
The project files for Native 7fb20bffdba35de7206d6be573fbc3957137eafa, Linux 239e595cbe725f18778eb0e23f49d6af67290344, and 4336 have the same byte count. After the product/test freeze, only six result documents are written later. Evidence: build-v030-dev/evidence/write-deadline-integrity-tdd-20261017/; section 49, files 378/506940761, and the previous nine sealed reviews remain unchanged. This round of processes is closed; the processes from another historical build, 49, are retained. The root HEAD remains unchanged, and no commits, pushes, or releases have been made.

### 50.4 Continued Open

In total, 22 test cases were executed on both platforms, resulting in TOKEN LEAK errors during clean exit processes. However, CTest still passed all tests. There were 13 test cases involved, including those related to old fetch/Host/platform scenarios, as well as those involving host_worker_deadline_independent features that were modified in this round of updates; For details on the segment-by-segment recording, please refer to runtime-exit-observations.json. The “normalCleanup” field in this round only indicates the successful completion of public waits and the termination of processes. It does not provide any information regarding the exit codes of individual workers, nor does it indicate whether there were any residual tokens or internal termination events. Therefore, it is not possible to verify these states based on this field. The diagnostic output for WorkerRuntime returns 1. Some integration drivers call abortive capsid_worker_destroy after responding, resulting in a return value of 0. No normal drain state was observed. Therefore, the current results cannot prove that there are no remnants of the Runtime token. It is also impossible to simply attribute the phenomena occurring during the abortive termination process to normal leaks; Additional facilities are required for normal operations drain/abortive Terminating the separate finite states and negative contrast matrices.
This round covers the real backpressure writing timeouts for public Worker response paths, disabling configurations, and complete pool stop/drain. Pool-level non-zero write_timeout configurations, different writing stages such as head/body/end, 503 local response pending kernel writing, writing completion, and switching/teardown still require independent matrices for more permutations.
Cross-shard selection for owner exit/stop after successful post, exceptions/overlimit of head/body/limits, other cancellations/Windows stalled-TCP, real Host double failures, origin/Binding/CPU/heap/OOM/GC/resource recycling continue to be open.
There are three types of differences: workerd close-code 999, limited sending queue, and no request for subprotocol. The WPT profile/old expectations/stable clock environment, MSVC ASan, complete vendor TSan/fuzz/soak/native Mac/Hosted/T15–T17, and brand-new dual build for byte-by-byte reproduction have not yet been accepted. Overall, items for T02/T04/T06–T08/T10–T17 and 0.3.0 have not been completely closed.

## 51. 2026-10-17 Worker: Real exit and fetch establishment failure cleanup <a id="section-51"></a>

After 22 rounds of analysis and review of TOKEN LEAK issues related to section 50, the following problems were identified: In some normal scenarios, only the response was checked, and the process was terminated immediately without checking the actual exit code of the worker. The value “CAPSID_EVENT_EXIT” with a status of 0 indicates that this field is not used. When shutdown/EOF occurs after the normal response is completed, the deadline timer is stopped immediately, leaving no time to collect the garbage from completed promises. If an exception occurs during the initial synchronization phase of the fetch operation, the oncomplete event will not be triggered. The activeClients and abort listeners are not cleared, resulting in the retention of the request context. It was also found that in the poisoned state, the event loop stops normally, returning a value of 0, thereby disguising the actual termination of the abnormal realm as a successful exit.

These are executable counterexamples, not results of artificial cheating. The original Runtime, with enhanced normal integration drivers, resulted in three out of five real exits on 1 for Windows, and two out of five real exits on 1 for Linux. Both the diagnosis and the original states have been archived. After adding an additional exit GC, recovery was only possible on the Windows platform. Two fetches still failed, and gc1 was recorded as a failure without being marked as a success. After correcting the duplex: 'half' in the streaming fixture, the original fetch cleanup/poison return value in red3 failed four times out of seven on both platforms: three real exits on 1 for the main working tree, and a real exit on 0 for poison, which was rejected by the negative contract. The control of clear timer, immediate shutdown of live timer, and unreachable cycles was successful.

### 51.1 Final Implementation

- src/worker_runtime.cc performs garbage collection before exiting without poisoning or having a ResponseState. It only collects unreachable values and does not continue executing the JavaScript job or event loop. The original refs>1 missed-checking rule is not relaxed; the final reference is released when the healthy token only has registry references. The still-living timer/reaction remains visible and cannot be replaced by GC as if it were normally recycled.
- The poison state always returns a exit code of 1, even when TJS_Run itself returns 0; since there are already exceptions that return non-zero values. There are no extended poison grace periods, no rules to erase TOKEN LEAK, nor adjustments to the reclaim time limit.
- New patch added: patches/txiki/0043-capsid-fetch-open-failure-cleanup.patch. This patch addresses issues related to failed opens and failures in preparing the payload. It involves removing activeClients, disabling abort listeners, closing the client/response stream, and retaining the original rejection status. For initial empty payloads, payloads with known lengths, and streaming payloads, openClient is used instead of the original method. The size of the 42 bytes in the original patches remains unchanged, and the changes made to the vendor/QuickJS commit code also remain unchanged.
- Completing the patch number verification shows 43 patches. Applying all 43 patches in sequence from the original vendor files, first reconstructing the old 42 patch manifest, then checking the content of the new files and the manifest. Subsequently, updating the overlay key/manifest in docs/txiki-upgrade-baseline.json. The new key is 08075c9aba96e9ad577ae1abf4db701b35db11767b538af59fb9d97078e6bff9, and the manifest is 95cc5c81a5e7b03900a2cdceff4dab443ee115e91e7d06b23814fd721b17f6e4. Formal build and independent vendor/overlay testing are conducted again for verification.
- The overlay manifest belongs to the bytecode compatibility ID input. It is regenerated with the corresponding identity this round. When taking over, the supporting Worker, compiler, and Host are used to re-verify the old bytecode cache with the new identity.
- The layout, values, and calls of ABI v7 remain unchanged. runtime.h explicitly states that EXIT.status is not an OS exit code. The public test helper on Windows first fixes the process handle and checks the return value of GetExitCodeProcess, while POSIX has a limited waitpid. Wait for batch reading of IPC, do not fake exit after the absolute time expires. A successful assertion requires flushing, an end state of IPC, and a real exit 0, No Wait/Protocol Errors.

### 51.2 Seven New Scenarios and Original Test Refinements

| CTest | Results that must be observed |
| --- | --- |
| worker_exit_cleared_timer | The actual timer is cleared first, and after a complete response, it gracefully shuts down, with the OS exiting with 0. |
| worker_exit_retained_timer | The actual request-owned timer is still alive. After a complete response, the request is shut down, with the OS exiting with an exit code of 1, and clean=false. |
| worker_exit_poisoned_timer | Do not request shutdown; let the actual poison terminate itself, OS exit 1, do not consider a return of 0 as a success. |
| worker_exit_gc_cycle | Inaccessible Promise/reaction cycles can be reclaimed by the GC. After a complete response, the OS exits with 0. |
| worker_exit_denied_empty | The attempt to establish empty text failed and was caught. The context was reclaimed while retaining the parent signal. The OS exited with 0. |
| worker_exit_denied_known | It was reported that the establishment of the asynchronous preparation failed. The same requirement is that the OS should exit with 0. |
| worker_exit_denied_stream | Legal duplex: Failed to establish the streaming text of half, also requires OS to exit with 0. |

The three fetch scenarios retain the AbortController globally within the module, ensuring that it cannot be obscured by the controller during garbage collection. The timer is created for the real API, and the live timer delay is significantly longer than 2.5 seconds, allowing the observation window to exit. The normal integration driver requires the real exit to be 0 after responding to assertions; branches that intentionally submit invalid header/request-end are still verified according to their incorrect contracts. The worker_exit_fields continue to require a zero field for EXIT, while independently observing Windows TerminateProcess(1) or POSIX SIGKILL, and must be determined to be clean. The worker_bounded_destroy waits for uncooperative processes for 80 ms, without claiming exit, and then verifies that abortive destruction is completed within 1.5 seconds.

The final two platform candidates each have **15/15** items met the requirements. For the second version of the product object control, **15 items met the predetermined results**: all six items on the baseline passed; cycle must fail, poison/OS exception control passed; poison changed back to returning 0, only poison-timer must fail, the remaining five items passed. Only the actual Runtime objects in the private Worker were replaced. The normal test-driven, other objects, linked libraries, and normal Worker hash values remained unchanged.
In the first version, only the control mechanisms related to exiting the GC were removed. There was a timing conflict: Native protected-denied was detected, while host-denied passed. In Linux, the periodic GC could complete its cleanup tasks. All three types of cleanup operations were successful. This version does not constitute a reliable proof of isolated GC operations. The original JSON/log files remain intact. The second version explicitly tests the scenario where both GCs are absent. It does not describe this as merely verifying the presence of one additional GC. The success assertions from normal tests remain unchanged. For more details, refer to preparation-notes.md and the documentation related to both versions of worker-controls.

### 51.3 Fullness, Compatibility, and Evidentiary Status

Windows strict Release full **325 items: 313 passed, 0 failures, 12 skipped conditions**; Linux strict Release/LTO full **614 items: 609 passed, 2 WPT files failed, 3 skipped conditions**. Fixed workerd official gate **22/22, zero skips**, same as regular Linux binary root **3/3, zero skips**; special Windows **66/66**, Linux **67/67**, GCC Debug TSan **66/66, zero reports, zero skips**.
Native d86dab96e25bf7d704e336327d01864e12f6cf47, Linux cdcbe868b1792633997ecffd5712ca410a89146b, **have 4338 project files with the same byte count**. Compared to section 50, 12 project files have changed, with two new files added. The result document has been updated six times. Three host product source code/files, SDK tool dependencies, and workerd reference locks have not changed; the WPT expectation remains unchanged. The overlay lock has been updated with the audited new patch. The root HEAD remains at 8028b88adff26cbc1abcb9a2fafce2ffc3d8090f. The historical workspace is retained, and no commits, pushes, or releases have been made to the root repository.

WPT retains **84 complete reports/14060 sub-tests**, with 82 accepted by the verifier, 2 rejected. The expected count is 20, the unexpected count is 3, and there is 1 missing expected report. The failed files are:

- worker_wpt_html_webappapis_scripting_processing_model_2_unhandled_promise_rejections_support_promise_rejection_events_js
- worker_wpt_webmessaging_MessagePort_initial_disabled_any_js

hr-time: This round passed completely; Section 49 failed completely, and 20 clock diagnostics were fixed and continued to be retained. The original 30 ms tolerance remains unchanged from the expectation. The stable clock environment is still tested, and WSL records are not used for performance conclusions.
The workerd gate is executed after the complete and normal full log archiving is completed. There are four connections per frame for the four backends, totaling 56 frames. The Native SDK has 22 scenarios in two modes. The TSan actually verified 95 selected projects for C++ compilation/object input, including the Worker Runtime. By default, the vendor's HTTP C/ broader dependencies have not yet been fully instrumented. Windows IOCP/GC does not have a native sanitizer for acceptance, and there are still no real machine results for Darwin.

Evidence build-v030-dev/evidence/worker-exit-integrity-tdd-20261017/. The complete LastTest and JUnit files should be archived before the next CTest (including show-only); the truncated stdout of passed JUnit files will not replace the complete original text. Both platforms retain the original Runtime objects, integration drive objects, and both Worker/drive binaries; other fixed inputs maintain a hash binding, and it is not claimed that all libraries/objects have been archived. A Linux diagnostic build/snapshot update was rejected by HEAD inspection before the CTest; this batch is not considered qualified. Subsequent red3r builds will reconfigure/ALL build from the same snapshot, ultimately resulting in a fully serial process. Section 50, document 345, with 546333932 bytes and ten subsequent sealed files remain read-only; this round of processes is recycled, and 49 processes from a previous build are retained.

### 51.4 Remaining Scope

This round of normal integration-driven clean=true has now proven that the actual OS exit is 0; the success of Host's normalCleanup/wait does not equal the exit 0 of each sub-worker. Ultimately, except for the deliberate live-timer scenario, there are still 4 test cases with TOKEN LEAK diagnosis executing, involving: host_admission_keepalive_streamed, host_worker_deadline_independent. The original text and CTest status are saved in worker-exit-observations.json; cases of abortive termination, protocol violations, and request cancellations need to be further verified according to their respective contracts, and the entire Worker exit matrix cannot be marked as completed.
Next, we need to refine the checks related to the actual states of these final states, Host child processes, as well as the normal/anormal exits of other drivers. After that, we can proceed locally. 503 Pending completion of kernel write operations/timeout scenarios, as well as handling cases where the pool level value is non-zero. head/body/end And switching/exiting the arrangement; across shard/post Later owner exit/stop Abnormal/overlimit inputs, cancellation of other operations, Windows-related TCP issues, real-host failures, etc. origin/Bindings/CPU/heap/OOM/GC /Resource matrix. Differences among the three types of workerd; WPT profile/old expectations; stable clock environment; MSVC ASan; complete vendor information. TSan/fuzz/soak/native Mac/Hosted/T15 – T17/ The process of reproducing each byte based on the two different constructions is still available. 0.3.0 Overall, the task has not been completed. T02/T04/T06 – T08/T10 – T17 Do not close all items at once.

## 52. 2026-10-17 Host drain signal waiting and actual exit status <a id="section-52"></a>

Latest evidence: `build-v030-dev/evidence/host-exit-status-20261017/`, reviewed as follows [Section 52](takeover-review.md#section-52) This round of modifications involved 15 project files, with 9 CTest files added for Windows 8/Linux. The 4341 project file bytes in Native `43b16a5848a3a22f4ced7728d6ea18bc2c57fefa` and Linux `32ce7158f16ee941c4be4f8c97049ed2f1c9279d` are identical, and the root HEAD and existing modifications are retained.

Fix: The single Worker Host that enabled the process signal omitted canceling the Asio signal wait in `complete_drain()`, resulting in the Worker being evicted and resources being reclaimed, while `wait()` still did not return. The normal, idle, and expired drains on both platforms for the real RED were timed out. The comparison with the signal-off state was conducted. Ultimately, this wait was canceled within the same owner loop, maintaining a natural conclusion. The final change to the product file and executor target only affected the build dependencies, with the test source code byte identical.

New internal C++ code `WorkerExitStatus` is added, which saves the OS exit result using the original unique recycling path. Snapshots are provided by the executor, single Workers, and static pools. The semantics of unused fields in C ABI v7, `EXIT.status=0`, the Worker Runtime, and the 43 overlay patches remain unchanged. Unknown results cannot be considered successful, and POSIX ECHILD remains unknown. Windows retains the actual exit code, with POSIX distinguishing between normal exit and signaled exit. Additionally, it is noted whether the destroy function triggers a termination for fallback purposes. `destroyForced` only records the fallback termination issued by the destroy function itself. Windows exit codes themselves cannot distinguish between normal return and signaled return. 1 and external TerminateProcess(1) are combined in this round to judge based on targeted scenarios and token logs. `wait()` still indicates that the Host has completed cleanup, and the Worker status needs to be checked separately. Additionally, the issue of missing dependencies for the SingleWorkerServer implementation in the executor test under Debug/TSan is fixed. The test is not removed, and the full interruption logs are retained for the first time. The final full logs and TSan both come from the same project bytes after completing the dependencies.

Windows strict Release full **333: 321 passed, 0 failed, 12 skipped conditions**; Linux strict Release/LTO full **623: 618 passed, 2 WPT files failed, 3 skipped conditions**. Specific tasks for Windows 76/76, Linux 78/78; GCC Debug TSan **77/77, zero reports, zero skips**, 98 selected projects with actual instrumentation during C++ compilation. Default vendor C, GCC support for some atomic fences, and native Windows/macOS sanitizers still have limitations.

Workerd’s official access control system **22/22 Zero skipped** Run after complete archiving of all logs; similar to ordinary Linux binary qualified root processes. **3/3 Zero skipped** WPT reserves this right. 84 A complete report /14060 The test failed. The result remains unchanged. Neither the expectation values nor any other adjustments were made. 30 Time tolerance. Two modes in Windows SDK 22 Scenario: Both precise frame mode and two-way connection mode are available, with four connections each. /56 Frames; Formal access control is also implemented for the four backend connections. /56 Frame.

The diagnostic methods for the two types of Host exits have now been clearly distinguished: unclosed flows, ignored cancellation of long timers, and drain cancellations that actually result in exit 1; normal/idle drains, cooperative cancellations that clear timers, and completed single/double Worker write cleanup requirements that actually result in exit 0. The original cancellation paths are still executed; long timers are used to prevent scheduling delays, turning cancellations into natural completions without adding additional latency to hide the diagnostic issue. Ultimately, all 7 token diagnostic records are retained, involving `host_admission_keepalive_streamed, host_admission_keepalive_streamed_drain_deadline, host_worker_deadline_independent, worker_exit_retained_timer`, and poison diagnostics are listed separately; for Native non-cooperative timeouts, the first step is to enter the detached-resource poison, which actually results in exit 1, without forcing a certain occurrence of a later exit token log. Details are provided in `worker-exit-observations.json`; the OS status of other custom drivers still requires further review.

The two-platform private control only replaces the `client.cc` product object, while maintaining the real drivers and all other recorded objects/libraries unchanged. Both successful and failed states are expected to be detected as intended. The control results for Windows 27/Linux 30 all meet expectations. The original test/Worker/Host binaries and two key objects are archived in nine copies per platform. Other recorded inputs are hashed and bound, and implicit system libraries are not archived file by file.

0.3.0 The overall task is still unfinished. Further work will be carried out on the OS state managed by other drivers, the 503 kernel local issues to be written, pool-level coverage, cross-shard exit arrangements, differences between the three types of existing workerd, WPT profiles/old expectations/stable clocks, Windows sanitizer, complete vendor TSan/fuzz/soak/native Mac/Hosted/T15–T17, and new dual build simulations. The failure in preparing the original build and the first control script coding errors are retained in `preparation-notes.md` and are not considered part of product qualification.

List of complete failures for Linux in this round: `worker_wpt_html_webappapis_scripting_processing_model_2_unhandled_promise_rejections_support_promise_rejection_events_js, worker_wpt_webmessaging_MessagePort_initial_disabled_any_js`. Full logs, original JUnit, fixed tools and binary identities, RED/control/projects/TSan, and source code/process verification are all in the same evidence directory. There are no excluded items; no need to rerun locally to replace failed results.

Location of entrances: `src/client.cc`'s `destroy_worker_with_status` and two platforms `wait_for_child`; `src/host/worker_executor.cc` is the only reaper; `src/host/single_worker_server.cc`'s `complete_drain()`; `tests/test_host_admission.cc` has five options; stop/drain comparison; `tests/test_host_write_timeout.cc` for cooperation/non-cooperation timeout and pool write cleanup; `tests/test_worker_destroy_status.cc`'s real clean/terminate/null/ECHILD; `tests/test_worker_lifecycle.cc`'s real forced termination.

## 53. 2026-10-17 Managed Preheating Worker Handoff Failure Reclamation <a id="section-53"></a>

Latest evidence: `build-v030-dev/evidence/generation-adoption-cleanup-20261017/`; For detailed review, please refer to [Section 53](takeover-review.md#section-53) This round of modifications involves four project files, with one new test file added; Windows/Linux each have 9 CTest files added. The native `8fbb81e29abd102c62b5fb5f670b137945d7cdec` and `7edd2380d97f593797e3d4b0c50e05b716bdf77d` versions have 4342 project file bytes in common; the root HEAD and existing modifications are retained, and six result documents are written at the end after verification.

Fixed an issue where the ownership reclamation of `GenerationPool::create_adopted()` was overlooked: During the transfer of ownership at the beginning of the process, if `adopt()` fails, the current implementation only destroys the failed portion and reclaims the prefixes that have been taken over. As a result, parts of the system that haven’t been accessed yet remain intact. Now, both the failed portions and prefixes are completely destroyed, and the responsibility for reclaiming the prefixes remains with the original executor. The C ABI for v7, the IPC EXIT semantics, Worker Runtime, 43 overlay patches, SDK/reference locks, and WPT expectations remain unchanged.

Both platforms ran nine real process scenarios on the original product implementation: leaving two Workers after the first empty position failed, one after the middle position failed; leaving one Worker after the last position failed, mismatching quantities, zero Workers, incomplete identities, invalid strategies, missing factories, and successfully taking over. After fixing the issues, the same test source code passed on both platforms. After successfully taking over three real Workers, they were naturally drained, with requirements to actually cause the OS to exit with 0, not to be forcibly terminated, and without wait errors. It was also verified that the immutability of not re-creating Workers, callback occurring only once, and repeated stops was met.

The assertion is that the independent observation of the real OS process is recovered. In POSIX, a product is considered successful only after the reap operation has completed and all processes are ECHILD. If a test encounters a zombie process, the test will fail. In Windows, the independent process handle is retained before handoff, and the exit status is checked. No further access is made to the C API pointers, which may have been released after handoff. In case of failure, the surviving processes are first recorded, and then the test cleans up the child processes it created. A failure rollback only proves the recovery, and it cannot be claimed that all OS exit codes are 0.

The final source code for the private product objects fully meets expectations in comparison to Windows 27/Linux 27: Only the actual `generation_pool.cc` object is replaced, while the actual test objects, other recorded objects/libraries, and Workers remain unchanged. Restoring the old logic will cause failures at the first or intermediate stages, and omitting the recycling of four verification branches will result in failures for five types of invalid configurations. Seven key original inputs are archived per platform, while the rest are bound by hashing. Implicit system and toolchain inputs are not archived file by file. The 27 controls from the previous native final1 are retained separately and not mixed with the final 54 items.

Windows strict Release full **342: 330 passed, 0 failed, 12 skipped conditions**; Linux strict Release/LTO full **632: 627 passed, 2 WPT files failed, 3 skipped conditions**, none of which excluded testing. Special Windows **86/86**, Linux **89/89**; GCC Debug TSan **86/86, zero reports, zero skips**, 100 selected projects with actual instrumentation during C++ compilation. The first Debug link for the new test target lacked the implementation of SingleWorkerServer, which has been supplemented according to the existing executor targets, Boost dependencies, and consistent GCC Beast compilation strategy. Logs from the first build failure are retained, not counted as TSan test results, nor is this version run for full acceptance testing. The final full and TSan results for both platforms come from the same source code after supplementing dependencies.

The process of fixing the workerd official access control mechanisms, **, 22/22, with no skipping allowed for**, should be carried out after the complete Linux log archives are completed. The same applies to the regular Linux binary qualified root mechanisms, **, 3/3, with no skipping allowed for**. The precise frames from the four backend mechanisms should be connected to /56 respectively. In the Native mode, connections should also be made to /56 respectively. The SDK modes involve 22 scenarios. WPT retains complete reports in **, 84, and sub-tests in 14060,**. A complete list of failures is provided in `worker_wpt_html_webappapis_scripting_processing_model_2_unhandled_promise_rejections_support_promise_rejection_events_js`, `worker_wpt_webmessaging_MessagePort_initial_disabled_any_js`. Neither the expectation values nor the 30 milliseconds tolerance should be modified. It is not necessary to rerun partial runs to replace all failed attempts. The diagnostic data from the complete logs is stored in token/poison, and can be saved as `adoption-observations.json`. However, these diagnostics are not included in the nine scenarios of this round.

Two WPT failures have been documented in section 26.4 – 26.5/27.3 and the fixed reference in section `docs/workerd-websocket-semantics.md`. The same evidence is used as an example: the default workerd is also omitted from the rejectionhandled section, and the capture:true for the MessagePort case is rejected. As a result, this case does not proceed to the original queue assertion. Section 27 also strengthens the check that the handled event must occur. These evidences should be continued and reviewed, to achieve an accurate mapping of the versioned profile and the standard WPT report boundaries. Upstream tests should not be considered outdated, nor should the reason be changed to a value different from the fixed reference, or any failure should be allowed to pass without further investigation.

0.3.0 Overall tasks are still unfinished. The next round should focus on completing the aforementioned compatibility checks. Other unfinished tasks include those related to managed resources. replacement/retire/quarantine /Canceled OS states are covered in detail, including other custom drivers and local settings. 503 To be completed: writing kernel settings and pool configurations, as well as handling cross-… operations. shard/owner exit/stop Arrangement, differences among the three types of workerd, stability verification of clocks, as well as Windows sanitizer and comprehensive vendor information. TSan/fuzz/soak/native Mac/Hosted/T15 – T17 And complete byte-by-byte reproduction through dual construction methods. By default, vendor C, GCC-related atomic operations, and native functions are utilized. Windows/macOS The sanitizer still covers the edges. Page … 52 The twelve sealed documents from earlier periods remain intact. They will be verified one by one during the final sealing process. This round of operations does not involve submitting, pushing, or releasing any documents to the root repository.

Entry points: The failed branch of `src/host/generation_pool.cc`, the ownership notes of `generation_pool.h`, the independent process observations of `tests/test_host_generation_adoption.cc` and nine scenarios, as well as the permanent registration in `cmake/build_tests.cmake`. The evidence entry points are `summary.json`, `verification.json`, `adoption-observations.json`, `implementation.diff`, `preparation-notes.md`, and `artifact-digests.json`.

## 54. 2026-10-17 workerd WPT profile: Precise Mapping and Evidence Verification <a id="section-54"></a>

Latest evidence: `build-v030-dev/evidence/workerd-wpt-profile-20261017/`; for detailed review, please refer to [Section 54](takeover-review.md#section-54) The project files for Native `8511dbd9944dacf5605baee9b00b6bc64feeebef` and Linux `4e77cd6f2139eb7c6dc1c0cc91cc31fb11032590` are identical in 4345. There are nine changes to the project files this round, with three new files added. Two new declarations/negative controls have been added for CTest on each platform. The standard WPT remains as the 84 file/14060 subtest, and the original two files are retained in case of failure. The root HEAD and existing modifications are retained, with six result documents written at the end.

New file added: `tests/wpt/workerd-default-profile.json`, version `CAPSID-WORKERD-v1.20261010.1-default-v1`. This file is bound to the workerd release/source/binary SHA, compatibility date/flags, WPT commit, and the SHA of the two original source files. The independent tool `tools/workerd-wpt-profile.mjs` corresponds to three precise standards and four real backend API observations. The original standard judges, 21 expectations, event occurrence guards, C ABI, Worker Runtime, all C++ product source code, 43 overlay patches, and the reference/SDK lock remain unchanged.

The verifier requires the LastTest, JUnit, and registration lists of the complete standard package to be consistent with the officially built gate registration list. It also needs to verify the identities of the current Host/Worker, drivers, and the two actual bundles generated. The names of the three failed sub-tests, the test phases, the types of exceptions, and the first lines of the diagnostics must match exactly. The old expectations for D010 unmetions must also still exist. The four backends are required to have handled event and promise identities, the original unhandled reason identities, capture registration rejections, and independent queue observations before and after start. Failure in the cleanup phase, timeout, missing events, local files, additional failures, and lost comparisons cannot be classified under this category.

Successfully classified as **`profile-matched-with-standard-failures`**, while retaining **`standardCTestFilesPassed:false`, `ordinarySuitePassed:false`, `standardsConformanceClaimed:false`**. It covers two standard files and corresponding four backend observations; does not ensure consistency across all Web APIs or standard compliance conclusions. The output path checks input files, hardlink aliases, original gate archives, and CTest status; failed attempts will render existing tool success reports invalid.

CI first retains the normal full registration and original judgment. Even if this step fails, the LastTest/JUnit is archived completely, and an independent profile verification is run. Failures in normal steps continue to block the workflow. Workflow review adds controls to prohibit skipping/ignoring normal full registration failures, prohibiting filtering by WPT criteria, and requiring the archiving, binding, and uploading of profile evidence. The newly added filtering regular expression prevents the omission of `-E` in the first occurrence, which is controlled by the actual private workflow. This issue has been fixed and RED is retained. This positioning does not relax the requirements; additional skipping controls will be added in the future. Therefore, it is not claimed that the entire RED test source code is identical to the final file bytes. The initial errors in negative fixture preparation and CTest footer/label parsing are also retained in `preparation-notes.md`.

Windows strict Release full **344: 332 passed, 0 failed, 12 skipped conditions**; Linux strict Release/LTO full **634: 629 passed, 2 existing WPT files failed, 3 skipped conditions**, with zero exclusions. Specialized Windows **10/10**, Linux **16/16**; both platforms executed **57 profile controls** and **44 workflow controls**. The fixed workerd gate remained **22/22, zero skips**, and the qualified root was **3/3, zero skips**. Four backends had four connections per frame/56 frames; Native modes had four connections per frame/56 frames, and the SDK had 22 scenarios. Hosted CI was not actually executed by local control; no rerun of TSan was performed in this round. The results of 86 out of 100 C++ inputs in section 53 are still independent historical evidence.

Additionally, using the same actual Worker/C++ driver, 36 sub-tests were executed: For the normal bundle, two original reasons led to failures, and the corresponding profiles were generated; for the private bundle, only the handled listeners were omitted. Original events were detected during the cleanup phase, but the profiles were rejected. Both drivers returned a result of 1. Therefore, the verifier must determine the actual reasons for the failures, rather than simply classifying them based on the exit code. The hash values of the original Worker, driver, and bundle before and after generation remain unchanged. The complete report, private copies, and key binaries/standalone bundles are included in the output files.

This completes the versioning precise mapping and local evidence chain for two known conflicts. Two standard failures remain unresolved: rejectionhandled.reason and the conflict with MessagePort capture:true/default-profile. Standard upstream assertions and unmet D010 expectations are visible. The document-scoped lifecycle HTML has not been executed, there are three existing workerd differences, other managed OS states are pending writing, and cross-shard arrangements, Windows sanitizer, complete vendor TSan/fuzz/soak/native Mac/Hosted/T15–T17, and a brand-new double build byte-for-byte reproduction are still not closed. 0.3.0 is overall incomplete, and no full closure such as T02/T04 has been performed; section 53 and the thirteen sealed documents remain as they are.

## 55. 2026-10-17 Managed Lifecycle Response and Actual OS Exit Verification <a id="section-55"></a>

Latest evidence `build-v030-dev/evidence/managed-exit-observations-20261017/`, for details see [Revise Section 55](takeover-review.md#section-55) The 4345 project files from Native `80c556484dceb937036552040fce15dffa824d73` and Linux `c91c4db2e554f69d9d9b10e24e9a2a331999b8ad` are identical. In this round, only the `tests/test_host_managed.cc` and `cmake/build_tests.cmake` project files have been modified. Five new Linux CTest items have been added. The product C++, ABI, Runtime, 43 overlay items, reference/SDK locks, and WPT original judgments remain unchanged.

The old request helper function only waits for the inflight state to be cleared. It can also meet the requirement when the Worker exits abnormally. The old replacement check might read the old READY=3 state before the exit handling. This round requires an actual complete 200 responses, precise text and objectives for the target executor. It is necessary to clearly terminate the observed Worker and wait for the original target to exit and for the independent new Worker to restore capacity. Both controlled inputs (incorrect text, no replacement occurred) exit 0 in the old driver, and exit 1 in the enhanced driver. Complete private source code, compilation/linking commands, output, and restricted child process cleanup records are retained.

New single/three Worker Admin generation replacements, cancellation of single/three Worker requests, and health recycling of three Workers to durable quarantine. Failed rollbacks with replacement, retire, and persist functions are complemented by idempotent re-activations to reflect the actual OS state. The state is copied in the completed-drain callback to avoid reading after the executor is terminated. Nine Managed regression observations per round **result in 27 exits: 26 natural codes 0, 1 real SIGKILL**, and **15 complete responses** are verified. After the product-exclusive reaper is completed, testing only confirms recycling with ECHILD, and it cannot rely on testing to reap zombies and obtain green status. Cancellations must be continued by the same actual Worker, and cannot be disguised as exits, replacements, or quarantines.

The new Debug TSan build exposes issues with the `test-host-managed` leaky SingleWorkerServer implementation. The Release optimization once concealed this issue. The testing objectives and linked dependencies have been addressed according to the actual Boost conditions. GNU Boost.Beast warnings exceptions follow the scope of capsid-host. The fully executed interruptions before the correction were retained separately, without considering them as qualifications. The final2 build was recompiled from a new snapshot. Initial optional access compilation errors, private driver include preparation failures, and all previous attempts can be found in the `preparation-notes.md`.

Final Windows strict Release **344: 332 passed, 0 failed, 12 skipped conditions**; Linux strict Release/LTO **639: 633 passed, 3 WPT files failed, 3 skipped conditions**, with zero excluded from the full set. Failures included the original two files and the current HR-Time basic. Native portable adoption special case **9/9**, Linux Managed special case **9/9**, both with zero skipped. New snapshot Debug TSan **9/9, zero skipped, zero reports**, with verification of 67 selected C++ compilation inputs containing `-fsanitize=thread`, covering drivers, Managed host core, and Worker Runtime. The default vendor C and broader dependencies have not yet been fully instrumented, resulting in no conclusions regarding native Windows sanitizers.

Fixing the access control for workerd **22/22**, qualified root **3/3** Skip all zeros. The standard WPT remains the same **File 84/14060 Sub-testing**. In this round, the measured performance for HR-Time is approximately 1996.206 milliseconds, while the time taken for Date processing is approximately 4873 milliseconds. The difference between the two is 2876.794 milliseconds; Independent Python sampling recorded a realtime/monotonic offset of 2876.701 ms; the difference is less than 1 ms. This supports the explanation of discontinuous environmental clocks. The original 30 ms tolerance and this failure are still retained. The HR-Time diagnostic for exit 8 is a standalone tool. It serves only for diagnosis purposes and cannot replace the 639-item comprehensive assessment. Due to failed additional criteria, the independent profile verification was rejected as designed `profile-audit2.json` Keep it `status:failed`, `complete:false`; This round did not result in any successful profile classification. There were also no successful results for standard compliance assessments. The hosted CI did not actually execute any operations.

This round of closing the above nine regression responses and observing the exit gaps, along with fixing the corresponding Debug test links, does not represent the completion of the entire acceptance for the Managed full path or version 0.3.0. The broader OS state of the actual Host executable/proprietary drivers, combined with concurrent deploy/retire/cancel combinations, 503 status to be written, cross-shard arrangements, D005 HTML, differences between the three existing workerd instances, Windows sanitizers, complete vendor TSan/fuzz/soak/native Mac/Hosted/T15–T17, and the complete byte-by-byte reproduction continue to be retained. T02/T04/T06–T08/T10–T17 are not completely closed.

## 56. Review of the Lifecycle of Observation and Replacement Callbacks for Managed Processes on 2026-10-17 <a id="section-56"></a>

Latest evidence `build-v030-dev/evidence/managed-process-observation-20261017/`, for details see [Reviewer Section 56](takeover-review.md#section-56) This round of fixes addressed two issues in the product lifecycle, with three additional cross-platform regression tests added; the overall progress for 0.3.0 is still incomplete.

1. The managed main program once stored the borrowed `capsid_worker*` in an observation map. The new generation is prepared to overwrite the monitoring PID, and a rollback might restore the old pointers that have been released by the unique reaper. Even when the exit callback returns nullptr, it cannot match and clear the old entries. Now, the executor caches the PID before the thread starts, and the monitoring only selects available processes from the published route snapshots. The old pointers of the map and ActivationPlan are deleted. Preparation, rollback, and the exit of the old generation do not require writing a second monitoring state. Multiple apps determine the first available process by route name; this is an individual Worker RSS observation, not a collective fleet aggregation.
2. The replacement counter is decremented before calling `on_worker_started`, preventing the executor from being released prematurely. The counter continues until the callback returns, ensuring that the callback is executed outside of the pool mutex. Controlled tests pause the callback of the actual replacement, confirming that the pump has processed its EXIT, and then checking that emptying cannot be completed first. Only the old product snapshot RED for the test is marked as exit 1. After fixing the issue, all platforms passed the test. Using exactly the same test source code and the final real Worker to compare the old/new drivers, it remains **exit 1/0**. The source code, drivers, and Worker SHA have been verified, with no changes to the full evidence. RED verifies this lifecycle, but does not claim to capture a complete UAF scene with old Managed pointers. The boundaries of the initial RED Worker that were not archived in time are retained, as described in the preparation notes and `same-input-controls.json`.

Newly added `host_generation_replacement_observer_drain`, `host_process_observation_publication`, `host_process_observation_replacement`; enhanced the retention assertion for the PID of the adopted executor. Covering preparation/rollback, old generation exit after submission, multi-app eviction rollback, concurrent publishing/reading, and healthy replacement. Each round of addition records **8 actual natural OS code 0 exits, 4 complete 200/precise text responses**. The process observation test also verifies the recycling through Windows independent process handling or Linux product reap, and cannot rely on the test to automatically reclaim zombies and obtain green status. Native Windows only verifies the portable Host internal components; no new support for the Managed coordinator platform has been added.

Windows strict Release full **347: 335 passed, 0 failed, 12 skipped due to conditions**; Linux strict Release/LTO full **642: 637 passed, 2 WPTs failed, 3 skipped due to conditions**, with zero exclusions. Native special case **13/13**, Linux special case **24/24**, current Debug TSan **16/16**, fixed workerd **22/22**, qualified root **3/3**, with zero skips for all special cases. TSan selected compilation closure included 76 C++ inputs, covering real Host main, new drivers, and Worker Runtime; actual running of real Host metrics endpoint. Default vendor C and broader dependencies are not yet fully instrumented, no Windows sanitizer, native Mac, or Hosted CI pass conclusions available.

The standard WPT still remains the **84 document/14060 sub-test**. The failed documents are: `worker_wpt_html_webappapis_scripting_processing_model_2_unhandled_promise_rejections_support_promise_rejection_events_js`, `worker_wpt_webmessaging_MessagePort_initial_disabled_any_js`. This round retains complete real-time clock/monotonic clock independent sampling. The original 30 ms tolerance of HR-Time has not been changed, nor has any additional rerunning been performed to cover all results. No failures in HR-Time were encountered in this round, unlike the 55th round. This does not replace the complete failures of item 639 in the 55th round, nor does it prove that the clock in this environment is always stable. A strict profile verification was successful; this only identifies known conflicts in the profile, not a general compliance or standard compliance pass. The full test on standard Linux still fails. The ABI, C Runtime, IPC EXIT, 43 overlays, JS/WPT original assertions, reference/SDK locks have not changed. The local deprecation diagnostics from the first MSVC strict build were fully retained. The final correction only performs local diagnostics for calls that are compatible with existing shared_ptr atomic operations, without canceling `/WX`.

The **4346** project files from Native `070ece00c021aa49f24f36ab30b01983b47089d9` and Linux `b8a6f639814f563b400c5f6ce83a1cee980c52b4` are identical. The files of 14 change documents from this round were frozen before final verification, with six result documents updated at the end. The root HEAD remained unchanged, and existing dirty modifications were retained. Evidence can be found in `summary.json`, `verification.json`, `red-result.json`, `tsan2-result.json`, `final-audit-review.json`, and `artifact-digests.json`. The first sixteen sealed files were kept as they were.

In the next round, three code validation points will be checked: the quarantine callback waits for AppOperationLock within the pool mutex, while the deployment process involves using the old pool’s AppOperationLock. This leads to a circular lock sequence, and the callback does not meet the expected generation conditions. Additionally, the SingleWorkerServer IPC metrics still utilize worker handles, which may overlap with those of the reaper. `fleet_exited()` Only checking slots. Whether the drain-complete/capacity release occurs in time when the retired executor exits late. For details on how to reproduce this issue and the relevant scenarios, please refer to the evidence provided. `follow-up-audit.md` None of these factors took into account the current round of closures. Continued to assess the actual status of the host/other drivers, as well as concurrency levels. deploy/retire/replacement /Cancel 503 To be written/timeout, cross shard/post/owner exit/stop Stable clock: WPT. D005 HTML has three differences compared to workerd. Additionally, there are differences between Windows ASan and the full vendor version. TSan/fuzz/soak/native Mac/Hosted/T15 – T17/ Exact byte-by-byte reproduction. T02/T04/T06 – T08/T10 – T17 Do not close all items at once.

## 57. 2026-10-17 quarantine: Order, generational verification, and process capacity review <a id="section-57"></a>

Latest evidence `build-v030-dev/evidence/managed-quarantine-ordering-20261017/`, for details see [Reviewer Section 57](takeover-review.md#section-57) This round of updates has completed tasks such as quarantine/deployment coordination, retired components, and fixing issues related to capacity accounting. The goal of achieving strong compatibility between **, Linux/Windows, and 0.3.0 has not yet been achieved for**. Details of these achievements are recorded in `goal-requirements-audit.json`.

1. The pool no longer performs quarantine persistence within the mutex. The pump only submits weak reference tasks to the management thread outside the lock. After obtaining the AppOperationLock, the management thread first checks the ownership of the released pool and the expected generation, and then reads/writes the active status. Reactivating the same generation is still instance-based; tasks that have been aborted/replaced will not be mistakenly written into a new generation tombstone. Tasks do not hold a strong reference to the pool, avoiding waiting for management tasks when the pool is destroyed within the app lock during termination. Normal quarantine waits for the persistence attempt to end before stopping the healthy worker; explicit owner drain can end the aborted pool without relying on the management thread to acquire the lock first.
2. `fleet_exited()` checks both slots and retired executors. Drain-complete and capacity release must wait for all actual processes to complete reaping; it is not enough to release just by receiving an EXIT notification or switching out a slot. Portable regression pauses the exit notification of the original executor to verify boundaries in the actual OS state, and this internal component test is also performed on Windows.
3. Each pool incarnation is recorded using a separate capacity lease, prepared/active/draining/released. The submission is settled first, followed by the release and emptying of the old pool. Finally, the resources are reclaimed only once according to the correct categories. This fixes the issue where active quarantine leads to a reduction in surge counts, causing issues with rollback and subsequent app deployment. When prepare returns null or the transaction configuration is incomplete, the startup reservation is also returned. Failed persistence is reclaimed by the new process before being returned. Additionally, absolute capacity checks after retired exceed the nominal surge limit are added, as well as the boundaries for UINT64_MAX. Continued review reveals that stale guards may leak directory handles before RAII takes over. New real regression tests prove that old candidates, 100, skip releasing handles, and 100. The issue is fixed by handing over the App directory to RAII immediately after acquisition, with no additional handles being added by 100 guards.

Added **Windows 8 items, Linux 21 CTest items**: two pools for sequential/recycling regression tests, two counting boundaries, four lease states/concurrent regression tests. Linux also includes nine quarantine transactions, two startup failures, and two real Host executable scenarios. It covers old generations, different instances of the same generation, prepare commit/abort, retirement, real file write failures, queue stoppage, expiration notifications, and the old pool reaching the quarantine threshold during lock-holding deployment. Write failures are triggered by applying RLIMIT_FSIZE after READY, not by forging return values. The new real Host scenarios verify actual HTTP 200/precise body and restart recovery.

Special projects **Windows 25/25, Linux 55/55**, all zero skipped. Two new pool cases added, with **4 natural OS code 0 exits** per round; Linux has an additional **19 natural OS code 0 exits** for total 23 cases, with complete responses recorded. A abort start only confirms that the product has been reap, and the reason for exit is unavailable, hence `exitStatusAvailable:false` is retained. New real Host forward cases do not have fixtures for cleanup; the driver only enables subreaper for its own descendants. If a normal Host stops and residual child processes are found, it fails. Intentional abort of Host or failure of fixture cleanup is noted as `FIXTURE_CHILD_CLEANUP`.

Candidates 5 the 663 There are additional items listed in the list. `websocket_managed_replacement-recursion` Failure. Original exit path used. 8 Retained. This old fixture eliminated zombies and replaced them with visible workers to complete the task. Based on the order of locking, counting categories, and stages of failure, it is likely that the next deployment will occur earlier than the actual capacity allows. Historical Admin responses only include information related to failures; no specific errors were reported. The cause of the issue can be inferred from the code and logs. Finally, the candidate processes are included in all the owned child processes that have not yet been reclaimed. They will be released after actual recycling/lease-back operations are completed. `worker_events_total{event="generation_drained"}` Next round of deployment begins; no retries for failed operations, no fixed delays, no relaxation of restrictions. RSS/VmSize Assertion. This loop is designed for activating existing versions of the system. prepared/abort The reserved resources are still settled by the coordinator when the process is terminated. It’s not possible to assume that all unsubmitted reservations have been cleared based solely on this event. The old Hosts fail to provide a proper comparison with the new state at the time of completion. This issue has been resolved. 16 All aspects related to actual deployment, response, recycling, and completion of events have been successfully carried out.

The old implementation RED is retained in comparison with the final input: two pools, actual Host capacity, two pure accounting boundaries, which were first tested in the old product and failed. The test of the stale guard directory handle also encountered 100 leaks and failed in Candidate 4. Additionally, **13 pairs of old/new input combinations** were tested using the final test source code and the final real Worker, resulting in 26 runs. The old implementation failed all assertion tests, while the repaired version passed all tests. The old coordinator only added signature adaptation to link the new driver, retaining the old behavior of non-generational quarantine. The two pools used drivers compiled from the old/new products for regression testing, with the same test source code. The executable program scenarios shared the final driver and replaced the Host. The coordinator adopted signature adaptation; the specific inputs for other combinations are recorded individually. Private source code, compilation/linking commands, library member replacements, SHA of each input, and all outputs can be reviewed. It is not claimed that all combinations share the same driver binary. Negative failure cleanups do not count as natural exits. The verification of fixed inputs and the complete suite of archives remains unchanged. The fixtures that failed due to two incorrect routeings and one incorrect singleflight concurrency assumption are retained as is, as described in `preparation-notes.md`. They are not counted as part of the product RED.

| Verification | Actual results |
| --- | --- |
| Windows strict Release, full version, zero exclusions | **355: 343 passed, 0 failed, 12 conditions skipped** |
| Linux strict Release/LTO full, zero exclusions | **663: 658 passed, 2 WPT files failed, 3 conditions skipped**; Retained CTest exit 8 |
| Current Debug TSan | **54/54**, zero reports, zero skips; verified 82 C++ compilation inputs, including the actual Host main, new dispatcher, ledger/lease driver, and Worker Runtime |
| Fix the official gate for workerd | **22/22**, same ordinary Linux binary, zero skips |
| The same ordinary Linux binary qualified root | **3/3**, skip zero |
| Standard WPT | **84 Document/14060 Subtest**; Original Assertions, Expectations, and Failures Unchanged |
| Supplementing the native Windows WPT execution | **84 Document/14060 Subtest, 81 Report Acceptance, 3 Rejections**; Native Node verified 111 upstream documents one by one, using portable JS packages with the same byte size; Not included in native CTest |

This round's failed files: `worker_wpt_html_webappapis_scripting_processing_model_2_unhandled_promise_rejections_support_promise_rejection_events_js`, `worker_wpt_webmessaging_MessagePort_initial_disabled_any_js`. A complete run yielded another 69905 real-time clock/monotonic clock samples, with a maximum adjacent offset variation of 4321.004 ms; the original 30 ms assertion was retained, and no additional reruns were performed to cover the results. A strict profile verification was successful, and only known conflicts of the fixed version were categorized; the full scope still failed, and it cannot be claimed that the standard WPT was passed. The fixed workerd v1.20261010.1 version is compatible with the date of 2026-08-20, with flags=[] and an unchanged SDK lock. The default vendor C, broader dependencies, and native Windows are not included in this round's Linux TSan conclusions.

The **4349** project files from Native `7129964d2505bd5e6943cefd6925ee23b187dbc6` and Linux `3a0dbf9cd78f7337cc780bcab7509d0bb893cbb1` are identical. There are 17 product/test changes in this round, with six result documents written and archived separately. The root HEAD `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f` remains unchanged, and all existing modifications are retained. No commits/pushes/releases are made to the root repository. The public C ABI, C Runtime, IPC EXIT, product JS, 43 overlays, WPT original assertions, and reference/SDK locks have not been changed. Evidence includes `verification.json`, `implementation.diff`, `private-controls6-result.json`, `tsan4-result.json`, `profile-audit4.json`, `final-audit-review6.json`. The first seventeen sealed items were reviewed file by file, and after final sealing, they are read-only.

Supplementary native WPT findings **GB18030's 18 platform differences**: All of Windows' 275 GB18030 original assertions are met; Linux has 18 GB18030- 2022 assertions that fail and have been accepted by D011. When using unexpected failure metadata, Windows refuses to report due to the absence of "expected failure" for these 18 assertions. The other two rejected files still show the aforementioned event differences. The complete supplementary report retains **2 expected / 3 unexpected / 19 missing expected**. This set of JS packages was generated by Linux's existing fixture build, and native Windows execution and upstream blob audits were completed. However, the native WPT generation process and CTest/Hosted registration have not yet been accepted; `native-wpt-supplemental6/analysis.json` retains file-by-file differences and does not claim that the standard WPT is fully passed based on these differences.

Next, we will prioritize completing the **native Windows WPT build/registration, worker comparison, and MSVC ASan** aspects. The fixed workerd has been tested with the same complete GB18030 package, resulting in 275 sub-tests, 0 actual failures, and 18 missing expected results, which are the same as on Windows (see `workerd-gb18030-probe2/result.json`). Next, we will correct the four-byte pointer aliases of the existing JS decoder on Linux, and then remove D011 after successful implementation on both platforms. Hidden differences cannot be concealed by relaxing expectations. Currently, no WPT_ROOT is set for Windows, and no worker_wpt_* is registered. The conditions for the two reference cases are skipped. The 22 gateways on Linux cannot replace this gap. According to the existing platform contract, the common scope includes Runtime, Worker, single-worker, and static-pool. The Managed coordinator still only supports Linux. Additionally, we need to reproduce and fix the race condition between the handle and reaper when IPC metrics are enabled in SingleWorkerServer, and to control the registration of delete/replace commands for the new cases added this round. Finally, a review of the source code revealed that Supervisor's current_pool provider returns a bare pool, and Supervisor also reads the bare executor from health-config across platforms. The reclaimer/finalize_drain can release objects concurrently. This item has not been controlled or fixed yet, and it belongs to code lifetime inference. It cannot be excluded by the zero report of TSan this round. See `follow-up-audit.md`.

503 Pending writing/timeout, cross-shard/post/owner exit/stop, non-zero configuration at the pool level, replacement of instantaneous actual process count and resource matrix, more real Host dual failures, stable clock WPT, D005 document HTML, three existing workerd diagnostic differences still pending processing; complete vendor sanitizer/fuzz/soak/native Mac/Hosted/T15–T17 and brand-new independent byte-by-byte dual builds have not been closed. T02/T04/T06–T08/T10–T17 has not been fully completed.

<!-- capsid-review-phase-58 -->
## 58. GB18030 Unified Character Set vs. Native Windows WPT (2026-10-17) <a id="section-58"></a>

This round has completed a practical platform compatibility fix, and has advanced Windows WPT from being imported into Linux artifacts to native generation and complete CTest. The evidence is located in `build-v030-dev/evidence/gb18030-workerd-compatibility-20261017/`, and the overview can be found in `verification.json`. This round does not close the entire 0.3.0 target.

- **Real Red Test**: First, only 18 D011 expected failures were removed. The product decoders and 275 upstream assertions remained unchanged. Linux experienced 18 actual failures, while Windows passed all 275 assertions. The old Worker, drivers, source code, and test packages were all archived in time. The corrected four-byte intermediate version also reported `InternalError: host encoding is unavailable` when used with the newly started Managed Worker. Archiving the intermediate Worker with the current Host combination can reproduce this failure, with a normal stop code of 0.
- **Final Implementation**: GB18030/GBK shares a fixed 207-byte range within the WHATWG four-byte range, with the exception of U+E7C7. A complete 23,940-byte two-byte index is also consulted. This path no longer calls the host encoder, nor does it rely on 22 local patches. The four-byte range is derived from the locked WPT, and is verified against the current official tables. The SHA of the two-byte raw text is `746b3c55f1a8ec4b90b451f384437a17fd37cd51cd668456a28071a758d10784`, and the packed source is little-endian uint16. The source and SHA are locked in `tools/encoding-data/gb18030.lock.json`. The data in the other four encoding tables remains unchanged bit by bit.
- **Real-world comparison**: The same proprietary observation fixtures were executed in workerd v1.20261010.1 (date=2026-08-20, flags=[]) as well as on Windows with single-worker/static-pool and Linux with single-worker/static-pool/managed configurations. Each Capsid configuration compared the actual code points of 23,940 double-byte pointers and all 1,587,600 syntactically valid quad-byte pointers; the illegal quad-byte regions were also retained with replacement results. Each configuration also included 368 sets of vectors and 2,568 stream operations, covering aliases, each cutpoint, byte-by-byte input, EOF, fatal errors, reuse after errors, valid private regions, and Unicode limits; the complete observations were identical to the reference. Default decoding exhaustive does not equal exhaustive combination of all options.
- **Windows Local Qualification**: 111 inputs from the independent pinned WPT checkout were checked against Git blobs one by one. All 84 WPT fixtures were generated using native Node. The MSVC strict Release configuration was completed for ALL. The Linux-only instructions in `docs/windows.md` and CMake have been corrected. Currently, Hosted Windows jobs still do not have WPT configured; local evidence cannot be considered as Hosted completed.
- **Final snapshots and complete results**: Windows `e8439d61541510c03663200684d9799a2c22417d`, Linux `6c1a58c34a08e99e97304e3cd3beefebbe9d55e8` the 4, 356 All project files have the same byte count; six result documents have been updated after verification. Full Windows version available. **449 = 435 Through / 2 Failure / 12 Skip ahead**; Full version of Linux **668 = 663 Through / 2 Failure / 3 Skip ahead** The original return codes are all… 8 Exclude zero values. Focus on Windows systems. 6/6, Linux 7/7 Three scenarios involving environmental permissions are executed under root conditions. 3/3; Linux is a required option for access control systems. 22/22 Zero skipped. Strict profile verification is completed. true.
- **Standard results remain unchanged**: Both platforms executed 84 files across 14,060 sub-tests. 82 files were accepted, and 2 files failed. Observations included expected=2, unexpected=3, missingExpected=1. Two failures were still attributed to rejection events related to supporting files and MessagePort initial-disabled, which were associated with three existing diagnostics related to locking the default workerd. The first-line diagnostics for Windows are the same as those for Linux, but Linux's 22 gateways were not recorded as native Windows reference gateways. The 275 GB18030 test packages generated by both platforms were also executed on the actual workerd, with all results passing=true, no unexpected failures or missing expectations.
- **Test Effectiveness**: The 15 observation validators for tampering control and the 3 data generation tampering controls were all rejected. The former used validation units based on real-world observations, not false success evidence from Workers. The real red tests for both new and old Workers are kept separate. The WPT source assertions, ABI, IPC EXIT, and 43 overlays remain unchanged. Clock monitoring yielded 70119 samples, with maximum adjacent offset changes of approximately 3.921 seconds; the original 30 ms assertion is retained, and no additional HR-Time runs were performed to replace the results. The Windows CTest time headers contain local encoding, and the analyzer only segments according to ASCII and strictly decodes the WPT JSON. The original log bytes are retained.

### Subsequent Execution

First, handle the IPC diagnostic handle's lifetime and the final actual count: Sampling is only performed after calling `executor_->worker()` currently, and `destroying_` cannot protect pointers that have already been returned. There are also Windows format issues confirmed in this round: The actual source code converts uint64_t to unsigned long and outputs it with `%lu`; MSVC x64 experiments with the same format pattern output 1099511627779 as 3, while Linux outputs the full value. This experiment does not actually capture large counts in production, nor does it correct the format generator in this round of fixes. Subsequently, Supervisor's hold on pool/executor and old detection targets were corrected and verified (still observation of unrecovered source code), and native workerd, MSVC ASan, and Hosted WPT were added. Also, §57 and the registration deletion/command substitution controls added in this round were addressed.

`follow-up-plan.md` outlines the specific sequence and validation boundaries. The remaining unfinished tasks include physical capacity/actual recycling, 503 wait-time tasks, cross-shard lifecycle combinations, D005 HTML, D004 stable clock, complete dependency sanitizer/fuzz/soak, Hosted/T15–T17, and independent byte-by-byte reproduction. No rerun of the sanitizer was performed in this round; the TSan 54/54 in §57 serves as independent evidence from the snapshot at that time. The current project package version is still 0.2.2; 0.3.0 has not been submitted, pushed, or released. The Managed coordinator still only supports Linux. `txiki-upgrade-baseline.json` retains the old state of D011 as a historical comparison baseline, with the current state based on the Closed implementation gap in conformance.md.

<!-- capsid-review-phase-59 -->
<a id="capsid-review-phase-59"></a>
## 59. IPC metrics: retention period, final count, and Windows 64-bit output (2026-10-18) <a id="section-59"></a>

Fixed the sampling duration of IPC metrics for the host's actual path and the final count after exit, and corrected the output truncation of uint64 in MSVC x64. Added necessary registration/command checks for rounds 57–59 and WPT configuration. Evidence directory: `build-v030-dev/evidence/ipc-metrics-ownership-20261017/`, overview: `verification.json`. **The strong compatibility target for Linux/Windows 0.3.0 is still incomplete**.

- `WorkerExecutor::snapshot_client_metrics()` performs actual sampling while holding the owner mutex. The reaper transfers the final count within the same lock and revokes the handle release, then destroys the lock externally and waits for the actual process to reclaim it. The final bank can be read after exiting, and it is set to zero after reading. `SingleWorkerServer::wait()` outputs the final increment after joining. A new internal bank transfer function has been added; the public ABI and EXIT protocol remain consistent with the original contract.
- Extract the actual JSON formatter, use `PRIu64` for the 26 uint64 counts, and continue using `%zu` for size_t high watermarks. Verify the zero values of independent expected fields, as well as those exceeding 2^32, 2^40, and UINT64_MAX. JSON floating-point comparisons are not used to avoid losing the precision of large integers.
- The real RED is the interface extraction for the retention behavior of old logic, followed by the execution of actual product functions. The Windows exit count and formatting failed, while the Linux exit count failed, but the old format passed. The Windows actual formatter's `host.asio_posts` expected 4294967310, but observed 14. The final segment could not be read after the actual OS code's 0/reap phase. The old drivers, Host, Workers, and source code have been archived in a timely manner. No UAF crashes of the old implementation were observed, and no crashes captured due to static lifetime defects were recorded.
- Newly added 4 Examples of metrics: factory/adopted Actual requests and actual exits, along with all of them. 14 The atomic field is transferred in one operation. The actual format of the integer is… 4 One reading thread and… 8 Real Worker concurrency sampling and recycling. The last case involved mistakenly treating an EOF construct as a parsing frame; this issue occurs on both platforms. 9/10 The failure is retained. The private observation program only outputs the error messages, retaining the original failure assertions. Actual frame sending tests have been conducted. 2 Parse the frames. 1 Ultimately, only those additional tests were changed to precise assertions. 2/1 Product code has been selected from the candidates. 1 Ready to be selected 2 Same, both platforms have been corrected and approved.
- Added 2 registration cases; currently, Windows checks 103 necessary registrations, and Linux checks 117. Both include all 84 commands configured with WPT and the original input fixture. The independent expectation list covers six combinations of platform/WPT switches, **rejecting 5,924 metadata tampering attempts**; the actual Node CLI also reads the private CTest registry and refuses to delete/replace it. This is under the controller's validation control and does not impersonate the product. The existing WebSocket registration also uses a private metadata copy to avoid overwriting the LastTest from the same build.

| Review | Windows Native | Linux |
| --- | --- | --- |
| Strict Release: complete CTest, zero exceptions | **455: 441 passed, 2 failed, 12 skipped; exit=8** | **674: 669 passed, 2 failed, 3 skipped; exit=8** |
| Current addition of IPC metrics and registered test cases | 6/6, passed with the complete suite | 6/6, passed with the complete suite |
| Standard WPT | 84 files / 14,060 sub-tests, 2 files failed | 84 files / 14,060 sub-tests, 2 files failed |
| Debug TSan Life Cycle/Concurrency | Not applicable | **61/61, zero reports, zero skips; 84 projects with C++ input verified** |
| The workerd must have a formal gate of acceptance. | Native reference has not been executed yet. | **22/22, no skipping** |
| Qualified root environment scenarios | Not applicable | **3/3, No Skips** |
| MSVC ASan | Configuration successful, ALL links failed; runtime qualification not passed | No new ASan runs were performed in this round. |

Both complete ordinary suites retain the original return code 8. The failure still constitutes a rejection event, supported by two WPT files with initial-disabled MessagePort. On both platforms, there are 82 accepted / 2 rejected cases, with expected=2 / unexpected=3 / missingExpected=1. The first-line diagnostics are consistent. The Linux strict versionized profile accepts complete=true, which does not allow the conclusion that the ordinary standard suites are completely green. The original 30 ms clock assertion is retained, with 74817 clock samples collected. No additional HR-Time runs were conducted in this round to replace the complete results.

The creation of a Windows new MSVC ASan configuration actually failed in the Worker link: The C++ project objects have `/fsanitize=address`, while the Ada static library does not. The 0/1 settings for `annotate_string`/`annotate_vector` are inconsistent, resulting in LNK2038/LNK1319 issues. **No ASan runtime tests were executed**. Additionally, when running the existing instrumentation gate in practice, it only recognizes `-fsanitize=address`, rejecting all `active_state.cc` compilations that have MSVC parameters. The original failures and read-only collection records are stored in `native-asan-red-result.json` and `native-asan-instrumentation-audit-red.json`, respectively. The next round should fix dependencies on instrumentation and validators, and then perform native ASan qualification again. For consistency regarding STL annotations, see [Microsoft Documentation](https://learn.microsoft.com/en-us/cpp/sanitizers/error-container-overflow?view=msvc-170).

Finally, it’s time to create a Windows snapshot. `2ec5c42c4494a86f2db570290fd62b3d3e18beaf`, Linux `2860408a3edf54df1a14c9f3a2b66b7804bd9efd`, **4, 361 The contents of the project file are consistent across all bytes.**; Compared to the first one… 58 Finally, Root was changed. 14 A product/test file, which includes… 5 One new entry has been added. Six result documents have been written after all verifications are completed. Actual situation: Host/Worker Drivers, internal libraries, and resources for both platforms 84 The WPT input has been archived. It’s now part of Windows systems. 111 The original upstream input has been verified again against the locked Git blob and the previous snapshot. The root HEAD remains unchanged. `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, not yet commit/push/release The package version remains the same. 0.2.2 Linux TSan includes 84 It’s a C++-based compilation tool, but it doesn’t include all the necessary vendor code or external dependencies. It cannot replace Windows in its functionality.

Next, we will address the two instances where the native ASan function failed. Additionally, we will reproduce and handle the issue related to the borrowing of the pool/executor tag by the Supervisor process. We will also continue to address issues related to native Windows workerd processes, Hosted WPT, physical process capacity, 503’s pending writes/timesouts, and cross-shard/post/owner/exit/stop interactions. D004/D005 and the default profile configuration will also be addressed. The existing diagnostics, as well as all dependencies on sanitizer/fuzz/soak/native, Mac/Hosted/T15–T17, and the independent byte-by-byte reconstruction process, will remain in place. For more details, please refer to `build-v030-dev/evidence/ipc-metrics-ownership-20261017/follow-up-plan.md`. This round of inspections has completed the evaluation of T02/T04/T06–T08/T10–T17.


<a id="capsid-review-phase-60"></a>
<!-- capsid-review-phase-60 -->
## 60. Progress of the 60th Review and Handoff (2026-10-18): Outbound ACK Pressure Prerequisite and Real Worker Fixtures <a id="section-60"></a>

The current code is candidate **11d**, with three frozen sources containing **4,376 byte-per-item files**. The strict ALL tests for Windows Release, native MSVC ASan, and Linux Release/LTO have passed. This correction addresses the issue where the outbound stress test did not demonstrate "ACCEPT has read, result ACK delayed due to a full wire queue." Independent test cases and instrumentation verification have been added. There are no modifications to the WebSocket's JS semantics or the relaxation of event assertions. The package version remains **0.2.2**, and the strong compatibility with 0.3.0 is not yet completed.

| Current 11d Inspection | Results | Scope |
| --- | --- | --- |
| Windows Release Special Project | **46/46, zero failures/skipped** | Inbound and outbound WebSocket, ACK/response header pressure, Supervisor, Pool/executor/capacity, Actual packet consumption and registration |
| Native Windows ASan Special Project | **47/47, zero failures/skipped** | Similar actual operation and plug-in verification |
| Linux Release/LTO Special Project | **84/84, zero failures/skipped** | Also includes managed recovery/recycling, Binding, stack and metrics. |
| Homomorphic Project Debug TSan | **81/81, Zero Report/Skipped** | Verified **102 C++ input projects**; complete vendors/ broader dependencies are still pending |
| Linux Locking of workerd for Formal Gatekeeping | **22/22** | Current same Host/Worker binary; native Windows references are not yet integrated |
| Private Native ASan Control | **1 The baseline passed, 4 real defect variants were rejected, 1 observable equivalence variant in this case was passed** | The original surviving variants are fully retained and are not considered detected errors. |

The old test still fails in the current 10e ASan binary. It waits for a fixed 100/250 ms after sending 16 fillers, and it cannot be proven that ACK does not enter the output queue before close. The new fixture compiles all **12 source inputs** of the regular Worker, with only dedicated inputs added; the startup, bootstrap, JS engine, protocols, and queue implementations are reused from the original code. The actual objects for the four configurations and the verification of compiled inputs confirm that the common source bytes are the same, with only the test entry selection macros differing in preprocessing definitions; each compiled item of the shared runtime/main and fixture main is verified against the corresponding ASan/TSan parameters, with 2 additional metadata checks added. The fixture is not installed in the release package.

The controlled writer first hands over the actual head and the initial body to the socket, then returns EAGAIN for the next frame. The worker actually observes a queue of **4,096/4,096 bytes**, the assigned owner has not been committed, and the resulting ACK cannot be queued; the original **200 ms** for JS close leads to an assigned CANCEL before writing resumes. For each of the four configurations, there are two records of actual states, both with unique result ACKs and unique assigned CANCESls, with no DATA/open Close. The normal handshake, rejection/cross cancellation, 20 orphaned instances, and two slots reused still execute the normal capsid-worker; the head pressure from the v3/v4 mixed response headers with 16 owners remains unchanged.

The pressure filling has been changed to a realistic 1 MiB response, which is directly proven under the premise of a full queue state. At the same time, it checks all bytes, the unique head/RESPONSE_END, subsequent healthy responses, and the natural zero exit of the sole reaper, which is stronger than the original assertion that ignored the filler completion. The original request budget of **10,000 ms**, **5 s** event deadline, and **30 s** CTest timeout remain unchanged. A new `worker_outbound_result_ack_pressure` has been added, and the audit registration also requires the complete suite to be passed through the actual worker and fixture paths, without missing any pressure branches.

Four rejected variants closed the controlled stall, missed the ACK blocking proof, missed the assigned CANCEL, and were distributed in advance. The variant with missed proof still failed the test with the actual exit code **73**, even if the normal protocol process was completed. The variant that was deleted from the native pending-open cleanup still passed in this case, as the JS's CONNECTING/already closed protection continued to suppress open; it was retained as an observable equivalent control, not claimed to cover other lifecycle combinations. The 5 s failure caused by the prototype's incomplete filler response credit, the return value compilation warnings after the main function was refactored, and the private link/input audit path preparation for correction were all retained separately and not considered as product RED.

The fixes for the instance/stop verification after blocking read in the two-layer Supervisor of Candidate 10e, the explicit thread join and the replacement deadline millisecond conversion remain in place. Seven real regression tests are still being conducted within the current suite. For Candidate 9, the SDK Config/version entry, the export of ASan parameters, the identity/ventory/SBOM of three DLLs, and the actual C/C++ and Host consumption after shifting have also been rechecked within this round. Independent reconstruction is currently underway to verify the deterministic text and identity, and the binary differences are recorded. However, not all binary data has been reproduced byte by byte.

The latest zero-exclusion complete matrix still belongs to the historical candidate **8b**, and cannot be considered as the current 11d full set:

| Candidate 8b Complete Matrix | Pass | Failure | Condition skipping | Original exit code |
| --- | --- | --- | --- | --- |
| Windows Release, 464 items | 450 | 2 | 12 | 8 |
| Native Windows ASan, 465 items | 448 | 5 | 12 | 8 |
| Linux Release/LTO, 683 items | 678 | 2 | 3 | 8 |

The qualified root of level 8b is **3/3**, with a standard WPT **84 files / 14,060 subtests (82 accepted, 2 rejected)**. The strict profile audit retains it as a historical result. The profile only classifies 3 conflicts in the workerd default behaviors, and retains 1 failed old expectation, which cannot be interpreted as a standard full green. The current 11-day period has not completed a full normal/ASan matrix or a new homologous root/profile qualification. The ASan Hono lifecycle of the current source code has been recreated within the original **500 ms** budget, as evidenced by `hono12-native-asan-baseline.json`. It cannot be closed just because it has been greened out of an outbound special case.

The items continue in order as follows:

1. Measure and resolve the actual interruptions during the matcher construction in the first concurrent requests of the current native ASan Hono. Maintain a budget of **500 ms** and the semantics of cold start, and do not close the process by increasing the budget or extending the warm-up period. Both 2 MiB ASan feature stack and the default 1 MiB boundary are retained.
2. The C17 scheduling differences of the native Windows pinned workerd require precise counterexamples and formal platform gatekeeping. The existing 10 samples cannot replace formal reference approvals, and strict expectations are placed on not relaxing this requirement.
3. Complete the zero-exclusion of common/ASan full matrix and homologous root/reference/profile for the final source code. Retain the standard two WPT failures and unclosed issues.
4. Capacity/resource mapping, replacement overlap, 503 pending writing, cross-shard and owner lifecycle combinations, D004 clock, D005 HTML, complete dependency sanitizer/fuzz/soak, native Mac/Hosted/T15–T17, and independent byte-by-byte reconstruction continue for the remaining rounds of round 59.

Evidence is located in `build-v030-dev/evidence/native-asan-qualification-20261018/`: `outbound11d-verification.json`, `outbound11-native-asan-original.*`, `outbound11b/11c-native-asan-probe*`, `outbound11d-*-targeted.*`, `outbound11d-linux-focused.*`, `tsan-outbound11d-result.json`, `outbound11d-fault-controls-summary.json`, `outbound11d-*-worker-inputs.json`, `outbound11d-linux-reference-gate/`, and `progress.json`. Binary data, libraries, packages, consumers, and instrumentation logs are stored separately. Historical failures are retained, while those that did not succeed in this round are not archived. The Root HEAD remains at `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`; commit/push/release is not used. Six result documents have been verified and recorded as `result-documents-outbound11d.json`. The earlier records can be understood based on their respective snapshots.


This round has also completed and retained the fixes for counterexamples:

- The native MSVC ASan instrumentation extends from the project's C++ code to the actual TJS/QuickJS/libuv/Ada/mbedtls/WAMR, as well as consumers. Structured incremental compilation auditing complements MSVC parameter recognition and provides reverse control against duplicate inputs/pseudomarks/disabled instrumentation. Early Ada link failures are not considered as runtime tests.
- Binding READY is expected to be derived from the actual startup configuration. In Windows, the actual value without memory limitations is 0, and when there is a limit, it is 1. Two actual Binding calls maintain an exact match for all fields; the reverse value must fail.
- The MSVC TJS embedded program reserves 8 MiB for the native stack. The JS stack is still 1 MiB by default, and the explicit 256 KiB recursion boundary continues to be verified. The RangeError is caught in the last two rounds of recovery. The Hono ASan feature budget is separately indicated as 2 MiB.
- WebSocket string has been converted to native format, fixing four major text boundary/I/O timeouts. The original pinned workerd's send function retains the isolated proxy item WTF-8, and the close reason uses alternative semantics. Original frame-by-frame bytes, NULs, object conversion counts, Symbols, exception identities, and method replacements have been verified. There are 722 assertions for the API. The initial incorrect USVString assumptions and the old Worker RED are retained.
- After the successful module evaluation, a rejection checkpoint is added to prevent the expiration of processor timers from erasing the records to be reported. The same 8 variants are controlled in the old ASan Worker RED and the repaired GREEN, maintaining the original upstream code, 10 ms timers, and reason assertions; all three sets of 8b full sets have passed the control.
- The WorkerSupervisor loop is now reading its stopped_ flag. The old implementation failed to handle local stop/no external flag/destructor issues. After the same test object was replaced with a new implementation, 4/4 passed. Three sets of 8b full code and selected TSan passed. The initial version of the test misused C++17; it has been changed to C++20 required by Host. Records of strict compilation failures are retained.

The process records include: 34 connection failures caused by incorrect path in Node ws when restoring the Linux environment; fixing the path resulted in full binary consistency; the original 30 ms assertion of HR-Time and independent clock offset variations; Windows reference runner's dependency on sha256sum and Node's native SHA256 fix; correcting private consumption-driven Ninja/UTF-8 preparation; failure of true generation with space-prefixed prefixes; and Windows system USERENV dependency exposed after the ASan package continued execution. USERENV can be verified through the GetUserProfileDirectoryW call in vendored libuv; the static CRT requirements for ordinary binary files remain, with only the recorded ASan→VC runtime import being accepted separately.

<!-- capsid-review-phase-61 -->
<a id="capsid-review-phase-61"></a>
## 61. Round 61 Review and Handoff Progress (2026-10-18): Concurrency Failure Diagnosis and Hono Cold Start Time <a id="section-61"></a>

The current candidate is **12a**, and three frozen sources contain **4,378 byte project files**. The strict ALL criteria for Windows Release, native MSVC ASan, and Linux Release/LTO have been met. This round of modifications includes shared framework testing driven and registration checks, as well as new failure diagnostic cases for real Workers. The `src/`, `include/`, `js/`, vendor patches, and runtime compilation options remain unchanged compared to 11d. The package version is still **0.2.2**, and the strong compatibility target for 0.3.0 has not been completed.

| Current 12a Inspection | Actual results | Scope |
| --- | --- | --- |
| Windows Release | **52/52, zero failures/skipped** | All registered framework cases, JS stack boundaries, registration and instrumentation audit controls |
| Native Windows ASan | **52 Passes / 1 Failures / 0 Skipped, Total 53 Items** | The only failure was for the original `worker_hono_lifecycle`, with the original CTest exit code **8** |
| Linux Release/LTO | **52/52, zero failures/skipped** | Similar frameworks, stacks, and audit special projects |
| Homomorphic Project Debug TSan | **7/7, Zero Report/Skipped** | New diagnostics, stacks, and audits; verification of **102 C++ input items**, not all of which rely on qualifications |
| Linux Locking of workerd for Formal Gatekeeping | **22/22** | Current identical Host/Worker binaries; does not represent native Windows; reference completed |
| Private ASan Diagnostic Reverse Control | **1 The baseline passed, 3 copies of errors were specified for verification** | Misreported owner, misreported timeout type, lost original payload; saved as old driver RED. |

The shared driver originally masked all concurrent errors by reporting `concurrent framework request failed`, obscuring the actual requests and their causes. Now, the request ID, ERROR/REQUEST_TIMEOUT, flags, and complete payload are retained. A new parameter, `worker_framework_concurrent_diagnostics`, is added to directly run the actual Worker, checking five scenarios: normal concurrency, first/second request errors, first/second request timeouts. This also involves verifying the error stack, complete normal responses, and driver exit. Ordinary errors cannot masquerade as timeouts, and fake drivers that report errors cannot pass the health checks. The independent registration contract requires the actual driver, Worker, fixture, command, and timeout for this test case. The metadata control covers six combinations of three platforms and two WPT configurations, resulting in **7,444 error variants**. These metadata controls are not considered part of the product execution.

Hono's original **500 ms** request budget, cold start, initial slow/sfast requests, and all assertion and feature stack settings remain unchanged. The current official ASan raw log clearly states: `request_id=1001 event=REQUEST_TIMEOUT flags=1 payload=request deadline exceeded`, and `InternalError: interrupted` was re-captured in the stack of `insert` / `buildMatcherFromPreprocessedRoutes` / `buildAllMatchers`. The previous reservations regarding the specific stack were only generic FATAL, which have been supplemented by this identical official log.

The private benchmarking results show that the time taken for tasks **, 510–524,** is attributed to the synchronization and cold path construction processes. Thread-based CPU timing also indicates that these tasks are being executed extensively. Built-in sampling mechanisms are used to further record the time spent on matcher construction and sorting processes. This does not necessarily prove that there are errors in timing calculations. Experiments involving the use of private entries `/Ob2`, `/O1`, `/Os`, and those involving split entry methods failed to pass the original lifecycle testing. Splitting the entry points only reduced the measurement time to approximately **–504–507 ms**. However, this still falls short of the desired performance. **: These engine/compilation methods were not integrated into the overall system. No additional budget was allocated, no external preheating was performed, ASan or STL annotations were not disabled, and any failed items were not removed.**: These times only reflect the performance of the current machine and instrumentation setup. They do not provide a guarantee of overall performance.

The proof of the real 4,096-byte ACK queue pressure for candidate 11d, a complete 1 MiB filler/terminal/natural exit check, a 10e Supervisor hold with deadline fixes, and actual package consumption fixes for the SDK continue to be retained. Their 46/47/84, 81 items related to TSan and other numbers from previous special projects are still in scope for this round. The latest zero-excluded complete matrix remains the historical candidate **8b**, and the current 12a is still incomplete for the full matrix of ordinary/ASan and the same-origin qualified root/profile. Two standard WPT failures, Windows pinned workerd C17 scheduling differences, and other tasks not closed in the 59th round continue to be retained, and the profile cannot be classified as standard full green.

Next, locate the JS/native calls for cold routing and allocate/ASan stack costs. Solve the issue within the same application, first request, and budget, and then verify. Do not use failed private tests from this round as fixes. Continue with precise scheduling/counter-example gate for native Windows C17, complete source code for normal/ASan/root/reference/profile, resource and owner lifecycle combinations, D004/D005, complete dependencies on sanitizer/fuzz/soak, and established platform/deliverables.

Evidence is located at `build-v030-dev/evidence/native-asan-qualification-20261018/`: `framework12a-verification.json`, three sets of actual CTest original logs/JUnit/registry, `tsan-framework12a-result.json`, `framework12a-linux-reference-gate/`, `framework12a-fault-controls/result.json`, `hono12-diagnostics-original-red.json`, and `hono12-*` private observations. The run/build inputs have been independently archived, and failure preparation is noted as `framework12a-operator-incidents.json`. Six result documents have been written after verification, with discrepancies and archiving records as `result-documents-framework12a.json`; the evidence for this round has not yet been sealed. The Root HEAD remains `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, and commit/push/release.

<!-- capsid-review-phase-62 -->
<a id="capsid-review-phase-62"></a>
## 62. Progress of the 62nd Review and Handoff (2026-10-18): Full validation of engine fixes and counterexamples for C17/C18 circuits <a id="section-62"></a>

The current candidates are **, 13a, and**. The three sets of frozen source codes include **, 4, 382, and files with the same byte sequence**. The Windows Release, native MSVC ASan, and Linux versions Release/LTO all passed the strict ALL test. All three sets of complete CTest results have been executed, with all failures and skipped cases recorded. **cannot confirm that 0.3.0 has passed the acceptance test**. The package versions remain **, 0.2.2, and**. The Root HEAD remains `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`; commit/push/release has not been processed yet.

| Current 13a Inspection | Actual results | Scope |
| --- | --- | --- |
| Windows Release | **476 items: 459 passed / 5 failed / 12 skipped conditionally** | Original CTest exit code **8**, zero exceptions |
| Native Windows ASan | **477 items: 462 passed / 3 failed / 12 skipped due to conditions** | Original CTest exit code **8**, zero exceptions |
| Linux Release/LTO | **695 items: 690 passed / 2 failed / 3 skipped under conditions** | Original CTest exit code **8**, zero exceptions |
| Three sets of frameworks/stacks/engines/overlays/registration initiatives | **Windows 59/59, ASan 60/60, Linux 59/59, zero skipping** | Actual Worker, Direct C API, and Vendor Source Verification |
| Homomorphic Project Debug TSan | **9/9, Zero Reports, Zero Skips** | Verified **103 C++ input items**; complete dependencies still require TSan verification |
| Linux Locking of workerd for Formal Gatekeeping | **22/22, no skipping** | After full archiving, execute again using the same Host/Worker. The following independent test cases illustrate that the coverage is still insufficient. |
| The same binary qualified root | **3/3, No Skips** | The same binary as the current full Linux version |

Added `0044-quickjs-call-dispatch-and-regexp-captures.patch`. The number of vendor patches is **44**, which have been updated and verified for the overlay lock. Native calls are dispatched before entering the large bytecode interpretation frame, and the shared entry still retains a single interrupt check. Small regular expression captures use local arrays, and when exceeding the boundary, the original heap allocation and cleanup paths are retained. The `src/`, `include/`, `js/`, and runtime compilation options remain unchanged from 12a. The original Hono application, the first cold request, the **500 ms** request budget, and the existing **2 MiB Hono feature stack configuration** remain unchanged. All lifecycle scenarios in the three complete matrices have passed. The default and 256 KiB recursive/recovery checks continue to pass. This is a verification for the current machine and configuration, not a guarantee of general performance.

Added eight types of call/regular expression scenarios for actual Workers, as well as direct C API interruption scenarios. The eight types of responses are consistent with the official workerd binary binaries for both Windows and Linux, bit-for-bit. Private reverse control mechanisms have been introduced to catch out-of-bounds situations, incorrect stack array releases, and missed native call interruptions. These issues are detected through specified ASan reports or C API assertions. Preparation failures for error copies and diagnostic scripts are saved separately, without replacing formal objects or reducing assertions.

The complete Windows Release exposes inconsistencies in compiler identity and related SDK failures. The Ninja records for the compiler objects are **0 header file dependencies**, still carrying old compatible IDs. By refreshing the private compiler objects with the same source code, current headers, and original registration options, the actual tests for both the original round-trip and Worker/library identities passed. The new minimum MSVC/Ninja header file modifications were tested normally, **and cannot be attributed to general MSVC/linguistic environment defects**. Further work is required to fix the explicit dependencies of key build headers and check the integrity of cache dependencies. The ASan SDK's negative control actually triggered the expected `LNK2038`, but CMake's decoding corrupted the `annotate_string/vector` symbols, causing false positives in the checks. Only the private copy with `ENCODING NONE` was added to retain the original byte output, and all six controls passed. These two corrections have not yet been merged into this candidate, and the full set of red lights remains.

Independent further investigation revealed **actual inconsistencies in the C17/C18 lines**. Locking in two platforms of workerd, each executed four groups of 30 tests, resulting in **240 connections**. Among these, **120 original TCP observations** received `queued` text frames from C17, or `a-before` text frames and `1000/done` Close frames from C18. The same Linux Node/undici clients did not report these messages, making it impossible to infer that the server rolled back the transmissions. In the current Capsid, **280 connections** across two backends for Windows Release/ASan and three backends for Linux showed **empty line frames in 140 original TCP observations**, and the other 140 Node event observations also showed no messages. The original test interpreted the loss of client events as insufficient evidence that the server discarded them; the original 22 gateways, by not covering this counterexample, **could not prove complete WebSocket consistency**.

The private ASan Worker only processes the synchronized errors by submitting the accepted staged send/close in order. A total of **80 connections** from two backends have recovered to the same text/Close frames as the two workerd assets. The formal source code and original assertions remain unchanged. The next step is to incorporate strict C17/C18 original frame regression and reverse control, then fix the submission semantics, separate the verification lines from Node event observation, and complete the integration with the native Windows official reference assets/gate. It is not possible to simply replace the original `[]` with any acceptable result.

The original failures of the standard WPT, the existing boundaries of the default workerd profile, and the unmet old expectations continue to be retained. The completion of profile auditing does not mean that the standard is completely green. Matters such as capacity/resource mapping, replacement overlap, 503 status to be written, combinations of cross-shard and owner lifecycle, D004/D005, complete dependency sanitizer/fuzz/soak, established platform/delivery, and independent byte-by-byte reconstruction will continue as planned.

Evidence is located in `build-v030-dev/evidence/native-asan-qualification-20261018/`: `engine13a-verification.json`, three sets of `engine13a-*-full` original logs/JUnit/registry, `tsan-engine13a-result.json`, `engine13a-linux-final-reference-gate/`, `profile-audit-engine13a.json`, `engine13a-fault-controls-v3/result.json`, `engine13a-additional-findings.json`. New run/build inputs have been independently archived; six result documents have been updated after verification, and their differences and hashes have been saved. No documents were sealed in this round.

<!-- capsid-review-phase-63 -->
<a id="capsid-review-phase-63"></a>
## 63. Progress of the 63rd Review and Handoff (2026-10-18): Formal Fixes for SDK and Regression Testing of Original Frames C17/C18 <a id="section-63"></a>

The current candidate is **wire14a**. Three frozen source codes contain **4,385 byte-per-item files**, and three sets of strict ALL builds have passed. The following table shows the special verification of the current source code. **Three complete matrices for this candidate have not been run yet, and the 0.3.0 strong compatibility testing is still incomplete**. The package version remains **0.2.2**, and the Root HEAD is maintained as `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, without any commit/push/release.

| Current wire14a inspection | Actual results | Scope |
| --- | --- | --- |
| Windows Release | **43/43 passed, zero failures, zero skips** | Actual Product Special Project; Total registrations: 479, not the full result of this round |
| Native Windows ASan | **44/44 passed, zero failures, zero skips** | Actual Product Special Project; Total registrations: 480, not the full results of this round |
| Linux Release/LTO | **55/55 passed, zero failures, zero skips** | Actual Product Special Project; Total registrations: 699, not the full results of this round |
| Linux Locking of workerd for Formal Gatekeeping | **23/23 Pass, zero skips** | The original checks for complete fixtures and independent clients are extended to include new comparison and verification rules for circuits; three supported Host backends |

Two SDKs have been officially updated: The four key translation units now explicitly rely on the generated data. `build_identity.h`; SDK uses negative control for CMake child processes `ENCODING NONE` Retaining the original diagnostic bytes. Previously sdk13b The specific task involves Windows operating system-related activities. **29/29**, ASan **30/30**, Linux **29/29** The current candidates continue to pass through the original round-trip process, as well as the compiler/worker/library identities, the actual SDK smoke testing, and six aspects related to consumption control. 13b The old ASan Ninja caches that are archived are now… 0 One automatic header file dependency required. **wire14a Both of the reconstructed Windows records contain identity headers.** ASan currently has the following records: 159 Dependencies are explicitly defined, ensuring that the edges are compatible with the actual compiler, and the IDs are also consistent. Old cached evidence is still retained. The common MSVC/language environment issues have been resolved through this restoration process. Original configuration remains unchanged. 13a The full set of red lights has not been rewritten.

`js/websocket.js` now submits accepted send/close operations in the synchronization callback in the order of dispatch, and handles any callback errors that occur. The independent `websocket_throw_wire_*` function executes with eight connections per backend on Windows Release/ASan and three backends on Linux, resulting in a total of **56 original TCP connections**. It asserts the `queued` text frames of C17, as well as the `a-before` text frames followed by `1000/done` Close frames of C18, byte by byte. It also checks for real handshake scenarios, failure of listeners without further execution, subsequent HTTP recovery, real peer termination, and normal host termination. The original frames have been incorporated into CMake, platform registration audits, and independent test lists, and cannot be passed by simply removing registrations or adding bypasses.

The formal API fixture also includes an original line check, with the baseline executing **748 assertions**. The comparator verifies the precise frame content and the limited client trajectory, preserving the actual EOF/reset, message, and close event results. **15 additional verification rules** are introduced to check for missing/damaged/duplicated/ disordered frames, timeouts/local destruction, erroneous termination, and erroneous client trajectories. The gate also rejects pseudo-consistent reports where both parties lose transmission or Close. The actual fixture fault variants expand to **19**: the baseline passes without changes, and all 18 variants must fail with pre-specified assertions, including the newly added discard of C17 transmissions and destruction of C18 close codes. Old products fail at the new precise byte assertions, with isolation corrections and locking of Linux workerd passing at the same assertions; failure is extended to any array or any close result masking frame loss.

The executable lines for the fixture remain unchanged, with only the old comments for C18 corrected. The SHA-256 hash of this candidate fixture is `1677587be168720383a5194215afe8dbbf89a54e7caaca9710870ad5094a4ed2`. In the 62nd round, 240 connections from official assets, 280 connections from legacy products, and 80 connections from private modifications are retained as they were originally, and they cannot serve as the complete native Windows gate for this candidate.

**Transmission boundaries remain open**: It has been observed that Capsid C18 ends with EOF, while the two workerd official assets end with a TCP reset. The queued events of Node/undici may vary depending on the platform and scheduling. The new gate proves that the accepted text/Close frames are consistent and have bounded termination, **but does not prove that the TCP termination methods or client scheduling are exactly the same**. The native Windows official assets lock and the complete gate still need to be accessed. The 23 results of Linux cannot be considered as a reference passing for Windows.

Windows also has a clear registration gap: `workerd_ws_gate_controls` is currently restricted by CMake to Linux, **not included in the 43/44 CTest items for Windows**. The gate rule tests that run directly on Windows have bypassed the Linux-only installer control; this direct test run is not considered a Windows reference pass. It is necessary to incorporate native asset locks, platform required tests, and the actual registration/execution of this set of controls. It is not sufficient to just adjust the binary hash and claim coverage for the entire system.

The last three complete matrices are still under **13a**: Windows **476 Item, 459/5/12**, ASan **477 Items, 462/3/12**, Linux **695 Items 690/2/3** (Passed/Failed/Skipped), original exit codes are all 8. The current SDK special project has verified the relevant corrections, but the final review of this candidate full-scale, same-origin TSan, guaranteed minimum, regression test, counterexample, eviction, main working tree, merge, handoff, and teardown is still pending. Old 13a's 9 Item Project TSan and Root 3 Items cannot be renamed directly to the current pass. Standard WPT original failures, default workerd profile boundaries, D004/D005, Capacity/Resources/Interconnection shard/owner Combined, complete dependencies sanitizer/fuzz/soak The established platform/ delivery and independent reconstruction processes will continue as planned. Hono maintains the original application, cold requests 500 ms Budget and Existing 2 MiB Function Stack Configuration.

Evidence is located in `build-v030-dev/evidence/native-asan-qualification-20261018/`: `sdk13b-verification.json`, `wire14a-verification.json`, three sets of logs for `wire14a-*-targeted`/`wire14a-linux-focused`, JUnit, LastTest, and registry files, `wire14a-linux-reference-gate/`, `wire14a-prebuild-controls.json`, and `wire14a-preparation-incidents.json`. The current actual run/build input archive contains **74 files**, including generated bootstrap JS/C/objects, actual binaries, identity headers, key objects, packages, and Ninja graphs; 18 source code files are saved separately. Original failures, preparation errors, and historical observations are retained, and this round of work is not subject to release qualification sealing.

<!-- capsid-review-phase-64 -->
<a id="capsid-review-phase-64"></a>
## Current progress of round 64 (2026-10-19): Windows native references and complete matrix, closed and error events process

The current candidate is **handler14i**. Three frozen source code files contain **4,392 byte project files**, and three sets of strict ALL builds have passed. The package version remains **0.2.2**, with the Root HEAD maintained at `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, and no commit/push/release has been performed. **The evidence for this round has been reviewed, but the overall strong compatibility and release qualification for 0.3.0 are still pending.**

| Complete CTest Matrix | Total Registrations | Pass | Failure | Condition skipping | Original exit code |
| --- | ---: | ---: | ---: | ---: | ---: |
| Windows Release | 485 | 473 | 2 | 10 | 8 |
| Native Windows ASan | 486 | 474 | 2 | 10 | 8 |
| Linux Release/LTO | 701 | 696 | 2 | 3 | 8 |

The above complete matrix does not use selection or exclusion parameters; all registered items are included, with conditions skipped and listed separately. Original failures, conditions skipped, JUnit, LastTest, registry, and exit codes are retained. The original failures from the two standard WPT files are still visible: `support_promise_rejection_events_js` and `MessagePort_initial_disabled_any_js`. The audit of the default workerd profile retains three standard conflicts and one unmet old expectation, and standard failures are not changed to pass.

This round of corrections addresses issues where the old counterexamples misjudged the Close kind 3, and the absolute path of Windows JUnit was misjudged by the old audit. New elements include the final Close/ability positive and negative vectors, as well as evidence paths, original CTest exit codes, reference installers, and fault control for mandatory gatekeeping. The actual special projects this round include Windows **23/23**, native Windows ASan **24/24**, and Linux **25/25**, with zero failures and zero skips. The complete reference gates for the same product are **19/19, 19/19, 24/24**, with zero skips. Windows uses its own official x64 asset lock, while Linux uses a Linux lock. The version, compatibility dates, and flags remain as established in the profile. Windows' official gatekeeping has been integrated with the installer and CI; this round is an actual local native verification, **with no claim that the hosted CI has been run**.

The following three real-world product behaviors have been corrected: In C18, when accepting a Close and encountering an error, and the client does not respond, the transmission ends after the accepted frame is written. In C17, the sending in the queued microtask is accepted, and the Host empties the accepted socket write queue before terminating the confirmation. The `late` frame is no longer lost. A trusted `ErrorEvent` is dispatched once the synchronization exception is completed in the current microtask. The state becomes CLOSED, and the observable fields of the seven built-in exceptions and the four non-Error thrown values are retained with a new exception object. The `send()` after closure is consistent with the locking reference.

The original line test reserves each supported backend **12 Article** Original test cases, additional execution **23 Article** Default error event cases and **1 Article** Special Resources Boundary Cases. The canonical API baseline is the actual one **1,072 Assertion**. Failure variants are **21 Items**, including the originally passed baseline 20 A copy of the error that must hit the specified diagnostic criteria. Add control to reject errors when both parties miss simultaneously, premature CLOSED, and discard `late` Frame and other pseudo-consistency. There are pre-validation controls for **812/812**, This workflow control **70/70** Pass. The actual gate of this candidate continues to verify registration, bytes, events, and fault diagnosis.

The resource boundaries are clearly defined: When the error listener throws an error again, the transmission of C17 remains open. The official Linux reference process cannot exit normally after this input, and forced termination of the recording is retained; it is not counted as a normal exit from the shared reference gate. This branch is still being tested on a product-independent line, with clear records of at least 100ms of open observation and client cleanup. It is required that the Host exits normally, and **client termination is not counted as server termination**. The corresponding branch of C18 is still verifying real peer termination.

The newly added internal capability bit `CAP_CHANNEL_CLOSE_FINAL` (bit 5, dependent on CONFIRM) and the Close kind 3 now treat accepted closes and transmission termination as a single operation. The original rules for normal closes, repeated closes, and closes with DATA remaining unchanged. The existing write/close limits, 4-second observation time limit, Hono cold request budget of **500ms**, and the existing **2 MiB** functional stack configuration remain unchanged. The actual compatible IDs of the two Windows compilers are consistent with the generated headers. During this first audit of the candidate, the Release compiler unit automatically depends on **0**, and ASan is **159**. Both explicit dependency edges are correct. Subsequently, by updating the identity header timestamps in the real Ninja diagram, **four key unit recompiles** were triggered, and strict ALL relinking was completed. The header content and frozen source code remain unchanged. Automatic dependency capture is still unstable in this environment, and it is not claimed that general language/encoding issues have been completely resolved. Detailed pre- and post-measurements can be found in `handler14i-identity-dependency-audit.json`.

The qualified root of the Linux product **3/3** has passed, and the skipped items in the original complete matrix are still retained. The current source code project TSan **15/15** has zero reports and zero skips. The actual check involved **104 C++ compilation inputs** for projects; the scope does not equal the complete vendor/dependency sanitizer qualification. The actual binaries, packages, bootstrap generation, key objects, framework/reference run inputs, and **37 key change files** have been archived, with the index being `handler14i-verification.json`.

The remaining work will proceed as planned: standard/profile boundaries with D004/D005, broader exception object forms and asynchronous error listeners, combinations of capacity/backpressure/trans-shard/owner/cancellation/stop, stable MSVC automatic dependency capture, complete dependency sanitizer/fuzz/soak, established platform delivery, hosted CI and independent byte-by-byte reconstruction. Single clock observations, specific pass-fail results, and project C++ TSan are not considered as overall closure criteria.

The subsequent handoffs will be carried out in the following order. For each item, first preserve a reproducible counterexample, then modify the implementation and verify it:

1. Extend the exception inputs in `api-compat.mjs` and `ws_handler_errors.mjs`: custom Error/objects, special stringification, Symbol/BigInt, global built-in overrides, and error listeners that return a rejected Promise. First, lock the official asset records for events, states, bytes, and termination results. Then, update the strict comparator and fault control. Branches where the upstream process cannot be properly terminated will not count as a successful exit from the shared gate.
2. The test involves a combination of `test_channel_races.cc`, `test_channel_io.cc`, static pool failures, and Supervisor ownership scenarios such as capacity exhaustion, ACK pending writing, cross-closure, owner exit, cancellation, and stop operations. It verifies actual quota/holding release and bounded stopping, while also maintaining boundaries for new capabilities, old capabilities, and rejected unknown bits. The existing budget and queue limits are not increased due to test failures.
3. Perform additional verifications for the established objectives of D004 clock and D005 HTML. The complete standard results and the default workerd profile continue to be recorded separately. Observations in the actual environment do not replace counterexamples of clock changes, and the profile classification does not eliminate standard failures.
4. After the final implementation is stable, rerun the entire ordinary/ASan matrix and the homologous project TSan/root/reference/profile to complete the dependencies on instrumentation, fuzz/soak, and the actual hosted/native Mac delivery, as well as the reconstruction of the independent new directory. The items that were not run are clearly reserved, and 0.2.2 will be maintained until the release qualification is completed.

Evidence directory: `build-v030-dev/evidence/native-asan-qualification-20261018/`. The current indexes include `handler14i-verification.json`, `handler14i-*-full*`, `handler14i-*-reference-gate/`, `handler14i-linux-final-reference-gate/`, `tsan-handler14i-result.json`, `profile-audit-handler14i.json`, and `handler14i-code-identity.json`. Files related to issues such as full backups in version 14d, misuse of report fields in version 14e, counterexamples for subsequent errors in version 14f, missing `late` frames in version 14g, and failed attempts due to old vectors/workflows identified in version 14h will be retained. These files will not be renamed until they are deemed suitable for use.

> The content of the 63rd round and earlier is historical snapshots; the current implementation, complete test results, and unfinished items are subject to the 64th round.
