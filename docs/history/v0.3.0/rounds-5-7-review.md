# Capsid v0.3.0 independent review of GLM fixes from rounds 5–7

<!-- capsid-historical-context-20261117 -->
> **Historical plan/review — not current execution instructions.** The original
> body, counts, failures and pending language below retain their captured source/date.
> Stable is v0.2.2 and experimental 0.3.0-beta is now published. Use
> [current status](../../reference/release-status.md) for delivered scope, exact qualification and
> measured-source data; do not restart old campaigns from this record.
<!-- /capsid-historical-context-20261117 -->

**Next Status (2026-10-15):** Codex has continued to implement and completed the verification of this round of sub-items. Currently, it is
74/74 Required regression tests, strict workerd gate control 5/5, gate control 24/24, both zero, skip.
The issues of Managed connection number conflicts and Pong disorder have also been fixed. The current results and the remaining matrix are as follows:
[Continued explanation](continuation-handoff.md) and
[Handoff Record Section 17](takeover-review.md#section-17).
The following text retains the original findings and evidence from the independent review before the fix was made 69/70 The following numbers represent historical checkpoints.

Date: 2026-10-15. Check the current `8028b88` HEAD and unsubmitted workspace, compare with GLM's
`index-fixround5.json`, `index-c15final.json`, `index-gates.json`, rerun
[Front-wheel fix review](fix-review.md) Expand the independent probes and adjacent inputs.
This round only involves running verification, saving diagnostics, and updating documentation. No changes have been made to the implementation or formal testing.

Conclusion: The conversion, exception paths, C15, real protocol headers, and delayed acceptance are clearly listed in the front.
The issue has been reviewed as per the original input; the metrics related to the failed deployment have also been restored. However, the recycling of the failed deployment is still intermittent.
Failure: There are differences in the actual Upgrade of the cross-request pre-built endpoint. Additionally, there are 9 conversion observation points.
Inconformity. There are still undetected issues in the formal C12 handshaking test, and the CI reference gate is still incomplete.
T04/T10/T12/T13/T14 cannot be accepted as a whole.

## 1 P1: Additional surviving Workers will remain after failed deployment recycling <a id="section-1"></a>

The official `websocket_managed_deploy-persist-failure` has failed in 70 cases: third attempt.
After rollback, the surviving Workers are `[479129, 479156]`, and the assertion is located at
[Lifecycle testing](../../../tests/test_websocket_managed_lifecycle.mjs) The recycling waiting area.
This enumeration has excluded zombies, and it has been verified that the EXE file serves as the Worker for this fixture. It cannot be interpreted as a
situation where only the child processes that have already exited are not waiting for further processing.

| Execute | Results |
| --- | --- |
| Independent Required Sets | 69/70, zero skips; failure only in this case |
| Do not modify the code, run it again separately 1 | Passed. All three rollbacks resulted in 1 Worker and 8 threads. |
| Run again separately 2 | Failure, still having `[480115, 480136]` after the third rollback |
| Run the test again separately 3 | Passed. All three rollbacks resulted in 1 Worker and 8 threads. |
| A diagnostic copy with read-only sampling | The first rollback reproduced the issue; at around 1,020 ms, it involved 2 Workers and 10 threads on the host, followed by failed recycling attempts. |
| The same diagnostic copy will be used six times thereafter. | Passed; Keep all records, no failure overwriting |

The upper limit for waiting for production regression has reached 20 seconds, and the total time for the second individual failure is approximately 20.30 seconds. Passed.
reapMs is 1 ms; this round cannot rely on further relaxing the window qualification. The original green light of GLM at 15/66 is genuine.
Historical results, but not sufficient to resolve the intermittent issues.

The elimination of the original strong self-sustaining loop still holds, and this failure cannot be directly attributed to the same loop. Priority should be given to troubleshooting.
[generation_pool.cc](../../../src/host/generation_pool.cc) Notifier: `weak.lock()`
It may acquire strong references temporarily. If the last reference is notified by the executor to be released, then the destructor thread will handle it accordingly.
The relationship between `wait()` and /join needs to be verified directly. This is a source code troubleshooting approach, and no attributable results have been obtained yet.
The original call stack is not written as having confirmed the root cause.

Regression for deterministic interleaving: Temporarily hold the notifier in a paused state. After the prepare phase is completed, persistence fails,
and the reference is released. Then, continue using the notifier. The actual pool/factory is finally released, the executor
and pump complete their joining process, child processes are returned to their original states, and the capacity can be reused. At the same time, retain multiple rollback scenarios for
the actual Host. It is not possible to replace the production callback mechanism with a separate transaction framework.

## 2 P1: The cross-request pre-built endpoint is still different from the reference, and formal regression testing is needed.

[api-compat.mjs](../../../tests/fixtures/workerd-ws/api-compat.mjs) `/retransfer`
Every time this function is executed, the value of `globalThis.__retransfer` is replaced with a newly created Response object. Thus, on the second execution…
The item taken out is another endpoint that was pre-built for the previous request and has not yet been transferred. It is not the same Response.
Second handover. [runner](../../../tests/test_workerd_ws_fixtures.mjs) Also using the ordinary `fetch()`
Second request sent, no Upgrade. The result of 500 cannot prove that the duplicate transfer guard is effective.

This round will directly load this formal fixture without modifying the worker code. Two consecutive real Upgrade will be conducted:

| Operations | Lock down workerd | Capsid single-worker / static-pool / managed |
| --- | --- | --- |
| The first `/retransfer` | 101, echo, close 1000 | All matches |
| The second `/retransfer` attempt uses the previously established endpoint from the previous request. | 500, cross-request I/O error | All 101, echo, close 1000 |

This represents a new issue related to request attribution. The problem caused by “retransferring the same transferred response” has been resolved in the standalone old probes. In all three
modes, a 500 error is reported for this issue. It is not possible to mix these two issues together or revert the previous fix.

The formal test cases need to be divided into three categories. The second time, a real Upgrade is sent, and the resource state after a failed verification is checked:

1. Repetition of the same Response/endpoint: Success on the first attempt, clear failure on the second.
2. Untransferred pair/Response created in the previous request: Fixed cross-request limitations for the reference.
   Check the creation of ownership for endpoints and the transfer of ownership requests.
3. The new pair created by the current request: Successive two independent upgrades have been successful, considered as normal control.

For cross-request pair construction, it’s not advisable to rely solely on the `transferred` flag. Instead, it’s necessary to maintain a valid path that includes both the event owners who have already
established connections, as well as those who are delayed in accepting the requests. This approach prevents the use of a blanket prohibition mechanism
for all asynchronous callbacks.

## 3 P2: After expanding the conversion table, there are still 9 discrepancies with the observed points

Among the 38 API observation points in the front wheels, 14 additional control points were added, bringing the total to 52 points.
In all three host modes, 9 differences were identified in the table. The remaining 43 points remained consistent across modes. Only comparisons based on results/error types are considered here; it is not
necessary for the error texts to be exactly identical. The same source code and reference/client are used on both ends.

| Input or observation | workerd | Capsid |
| --- | --- | --- |
| `close(1000, 123n)` | Accept | TypeError |
| `new CloseEvent('x', {reason:123n}).reason` | `"123"` | TypeError |
| `new CloseEvent('x', {reason:null}).reason` | `"null"` | Empty strings |
| `close(Object(1000n))` | TypeError | Accept |
| `new CloseEvent('x', {code:Object(1000n)}).code` | TypeError | 1000 |
| CloseEvent code is NaN | 0 | NaN |
| CloseEvent code is Infinity | 0 | Infinity |
| The reason for the CloseEvent is an isolated `\ud800`. | U+FFFD | Retain U+D800 |
| code/reason/wasClean getter reading order | code, reason, wasClean | code, reason, code, reason, wasClean |

The rejection due to the BigInt check in the `close()` and CloseEvent methods is a result of this new rule: the reason
parameter is converted to a string format, which means it cannot share the same primitive type guard as the code’s numerical conversion. On the other
hand, the bare BigInt guard also leads to an oversight when dealing with boxed BigInts—the `Number()` method can handle them, but references to them still result in a rejection.

[websocket.js](../../../js/websocket.js) `toUSVString()` currently only rejects Symbols after
calling `String()`, without replacing lone surrogates; `?? ''` in `CloseEvent` also conflated `null` with
`undefined`, and read getters twice after pre-checking them. Code and reason conversions should be implemented separately.
Handle finite values, primitive conversions, default values, and single reads according to the reference; save local results and then construct the final
event. Complete these 14 controls while retaining the large/negative/fractional and valid inputs from prior rounds to prevent regressions.

## 4 P2: All formal C12 handshakes failed, but testing still passed

The formal runner’s protocol matrix only specifies the construction and request headers, without any statements regarding the open or selected
protocols, or the results of the operations. In this iteration, diagnostic copies that do not change the behavior of the agents, workers, or peers are used to
record the original requests, parsed keys, and `/state`. Both reference and single-worker scenarios are executed during this iteration.

- 8 valid inputs of `Sec-WebSocket-Key` were not parsed; the regular expression has a line-end anchor but lacks it.
  Multiple lines are marked, and the requested key header is not located at the end of the entire head.
- The peer calculated the Accept with an incorrect key. All 8 results have only `state:3`, code 1006, and are empty.
  `protocolAtClose`; the feature is still capable of throwing SyntaxError normally.
- Both ends of the diagnostic runner exit with 0. The so-called "getter is incorrect under this harness"
  This actually corresponds to a failed handshake, and we should not delete the golden agreement that was successfully negotiated based on this.

The correct independent peer in this round has verified the three-mode header, the protocol at open, and a close of 1000.
This corresponds to the reference, so it is an issue related to test coverage for formal testing. It cannot be written as something that is still not supported in the implementation.
C12. After parsing the peer's key and processing the frames, every valid input must assert that the actual open and selected state is true.
protocol, close 1000/clean, and add expected failure controls for errors in the Accept phase. Section Two
Both the common HTTP 500 and the 1006 here indicate that the number of assertions cannot replace verification of the target path.

## 5 P2: The registration and Node pin have been fixed. The mandatory CI and comparison are still missing.

It has been confirmed that `websocket_outbound_reference` is always registered. In future implementations, it will be read from environment
variables instead. The value of Node in three locations within `testing-validity.yml` and `release.yml` is set to `24.14.0`.
During this round, when the locking reference was provided, both reference CTest instances were actually passed.

However, the workflow still does not include the steps to obtain and verify the locked workerd, as well as setting the `CAPSID_WORKERD_REF`.
If the runner lacks a variable, it exits with a code 77. The standard CTest accepts skipping as an option. Being registered does not necessarily mean that CI must be run.
In this round, no strict comparator was found between the automatic verification three-mode and the complete reference report.

The acceptance process must be carried out within the designated Linux-based job. It is necessary to verify the compression resources and the details of the executable files, including
version, compatibilityDate/flags, and Node/undici. Specifically, two references and three types of
Capsid fixtures must be selected. The selected options should be non-empty, actual implementations, and zero skips. Additionally, a complete report should be generated.
The comparator needs to check the input source code/fixture details, complete information, scenario data, and observation fields. Only differences in individual items
are allowed to be reported. The global setting `referenceParity=false` mentioned in the report cannot serve as a general exemption mechanism.
The gate of acceptance will fail if there is a negative control verification due to lack of reference, incorrect summary, or absence of observation/fixture.

## 6. Items confirmed for fix in this round

| Project | Independent Review |
| --- | --- |
| Front wheels 38 API inputs | Zero differences; includes original code with large numbers/negative numbers/raw BigInts, Symbols, and Response values that are not endpoint values. |
| C15 | `start:a, start:b, end:b, end:a`; The second handler no longer waits for the first await to complete |
| Synchronous send→throw, send→close→throw | Pending messages are not delivered; close 1006 after error, wasClean=false |
| listener: Immediately or after await, reject. | Both continue to process the next message, ultimately resulting in a clean close of 1000. |
| C12 Practical Handshake | 9 types of input requests: header, selected protocol, and exception construction, all conform to the reference in three modes. |
| C14 delay acceptance | 80 ms after return, 101 in all three modes, echo, close 1000 |
| Transferred to Response, then transferred again | All three modes are clearly set to 500, and the original 8-second unresponsiveness has ceased to occur. |
| Returning the accepted endpoint | Capsid is clearly defined as 500; reference is first set to 101 and then aborted. This clear difference should be retained, and no replication of certain responses is required. |
| WebSocket-based Response.clone | TypeError at both ends |
| Failure Deployment Metrics Identity | After three failures and a pause, the PID still corresponds to the old Worker that is still serving, with a positive RSS value. This value is not equal to the recycling of a stabilized instance. |
| Original outbound static dialing time assertion | This round passed; actual elapsed failure information has been added. The successful path elapsed has not yet been output in the report. Do not determine the cause of the old fluctuation based on this. |

This table only applies to those tasks that have already been executed. For tasks with pending acceptance issues related to cancellation/timeout, concurrency issues involving
Binding/CPU/heap/OOM resources, delayed processing of old tasks, and issues related to token/quota depletion, it is still necessary to continue following the original T12 schedule.
Finally, after a single “retire” operation, no more management operations are performed for object-level recycling. The second client, native platform, v4
fuzz/WS soak, and the release of evidence remain pending acceptance.

## 7 Evidence, Reproduction, and Subsequent Execution Order

Evidence directory is named review-glm-fixes-20261015 (local artifact `build-v030-dev/evidence/review-glm-fixes-20261015/index.json`).
This directory is ignored by Git, containing initial source code/GLM record snapshots, commands, JUnit, complete LastTest,
Original probes, extended conversions, actual Upgrade, metrics, and diagnostic records. It is necessary to retain the original data when switching to a new machine.
The document should be run again with the same locked input; Markdown alone cannot be passed on.

| Identity | This round's value |
| --- | --- |
| Host SHA-256 | `50234e46e5c3316193b3265f7e1cdf66d73c29952110442cc66f1e7139b5b4d4` |
| Worker SHA-256 | `c851d2915989cdc6671fe842530f39a970d4de7fc11ed4080d4b617d532f299b` |
| workerd SHA-256 | `50f31156a9a4c7d549f1b4753dd61eb527e1b6b0cd416e5ba3d02d64dfdee5dd` |
| workerd / client | `v1.20261010.1`, date `2026-08-20`, flags `[]`; Node `v24.14.0` / undici `7.21.0` |
| Registration / Independent Required Collections | 412 / 70, 66 items of GLM plus four items: guaranteed minimum, regression test, counterexample, and eviction. |

Running Existing Formal Failure Items (WSL):

```bash
export PATH=/home/eroszhao/.nvm/versions/node/v24.14.0/bin:$PATH
export CAPSID_WORKERD_REF=/tmp/opencode/workerd-ref/workerd
ctest --test-dir build-v030-dev \
  -R '^websocket_managed_deploy-persist-failure$' \
  --no-tests=error --output-on-failure \
  --output-junit build-v030-dev/evidence/persist-rerun-junit.xml
```

GLM next round to be completed in sequence:

1. Section 1 has been transformed into a deterministic regression to address failed deployment recycling; retaining failures and successes of this round.
2. Correct the real Upgrade/Redundant Handover/Cross-Request Pre-Construction Pair Regression, and fix the attribution differences in Section 2;
   Modify Section 4 C12 peer and restore the open, protocol, close golden rules.
3. Section 3 has added 14 controls to become formal golden, and the conversion implementation has been revised; the first 38 points remain approved.
4. Supplementary Section 5: CI Acquisition, Mandatory Runs, Strict Comparison, and Negative Control. It does not include optional skip qualifications.
5. Expand the necessary collection by name list, complete execution with zero skips, and then proceed with the remaining T12–T17; no longer
   Revised C15, delayed acceptance or metric status is considered as the current task.

The initial startup of the managed extension failed because the probe did not synchronize the target with the host using the localhost
address. This issue was resolved after adjusting the probe configuration. The results of the three-mode testing are now complete, with any earlier logs retained separately. Attempts
to collect diagnostic samples and call stacks are not considered as successful tests; no native stack was obtained. None of the 412 tests were rerun
during this process, and the results from Linux development builds were not used to determine whether the build is suitable for Release, Windows/macOS, or full dynamic inspection modes.
