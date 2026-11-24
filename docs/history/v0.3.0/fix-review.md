# Capsid v0.3.0 independent review of the GLM fixes in rounds 3 and 4

<!-- capsid-historical-context-20261117 -->
> **Historical plan/review — not current execution instructions.** The original
> body, counts, failures and pending language below retain their captured source/date.
> Stable is v0.2.2 and experimental 0.3.0-beta is now published. Use
> [current status](../../reference/release-status.md) for delivered scope, exact qualification and
> measured-source data; do not restart old campaigns from this record.
<!-- /capsid-historical-context-20261117 -->

The current results can be found here. [2026-10-15 Fifth to Seventh Round of Proofreading](rounds-5-7-review.md).
This document retains the history of inputs and evidence from the third and fourth rounds. The pending status below has been updated item by item by the new reviewer.

Date: 2026-10-14. Review of GLM's `index-abortfix.json`, `index-c0116.json`,
Current source code and products, compare [Front-wheel merge feedback](combined-review.md) and
[Test Completeness List](test-coverage-review.md). HEAD remains `8028b88`
Changes are not submitted. This round only involves proofreading, running probes, and updating the documentation. The implementation is not modified.

Conclusion: The issue of failed worker/thread deployments has been resolved. The inputs for C01–C14, C16, and C17 have also been corrected. Indeed, the GLM’s collection
containing 15/66 instances includes two new managed cases. However, the fix only addresses some underlying issues related to large number of code
conversions, non-WebSocket values in responses, and errors that occur after closing connections. The inconsistency between the
Promise-based listener responses and the original state remains a problem. Case C15 is still open for further investigation;
its strong compatibility cannot be confirmed yet.

For details on the actual handoffs and transfers, please refer to sections 10–11: There are still discrepancies in the
null/digital/boolean/object protocol headers of C12. The acceptance process delays after the Response return, and in
all three modes, the status is 503. Repeated handoffs fail after an idle period of 8 seconds in all modes. The original “16 corrections” refer only to
the observations made during the initial testing phase; the full functionality of C12/C14 has not yet been implemented.

## 1 Confirmed Fixes and Verifications

| Project | This round of independent results |
| --- | --- |
| The pool has strong ownership. | `pool_box` changed to weak; after three consecutive failures of writing to the same real Host, the Worker remains at 1, and the thread remains at 8; two rechecks are consistent, and the old v1 continues to respond. |
| Duplicate Parking References | abort_activation/abort_retire and drain perform callback cleanup for all matching references; the source code has corrected the issue where prepare was inserted without being undone. |
| Original 17 differences | C01–C14, C16, C17 correspond to the original input; C15 remains in serial, and GLM has clearly recorded it as not repaired. |
| Formal Regression | The new `api-compat.mjs` file is now in three-host mode and reference mode. New real hosts will fail to persist, and all replacement items will be included in the necessary GLM collection. |
| GLM Archive Collection | 15/15, 66/66, zero skipped; JUnit and actual names have been verified; both newly added Managed items are included |
| This round of independent collections | Original 66 items plus GC/job-context/request context/construction identity four items, total 70 items: 69/70, zero skips |

It turns out that the previously consistent control inputs still need to be adjusted accordingly. Out of the original 27 API points of observation,
14 of them no longer show any discrepancies. However, there are two additional control inputs that now exhibit new discrepancies. It cannot be assumed that the parameter
conversions are complete simply by reviewing the discrepancies listed in tables C01–C17.

After expanding the scope to 38 API monitoring points, 11 differences were identified among the APIs. Additionally, three abnormal paths and the known C15 were also detected.
The differences are categorized based on their causes. It should be noted that the total number of probes used should not be considered as the total number of API differences.

## 2 P1: The 16-bit modulo operation in code introduces numerical semantics for regression testing

[websocket.js](../../../js/websocket.js) The `toUint16()` method is used for both close and CloseEvent purposes.
Locking workerd to accept `1000.5` does not imply that all code will be converted to unsigned 16-bit form.

| Input | Lock down workerd | Current Capsid |
| --- | --- | --- |
| `close(66536)` | InvalidAccessError | Accept, cut to 1000 |
| `new CloseEvent('x', {code:65537}).code` | 65537 | 1 |
| `close(-64536)` | InvalidAccessError | Accept, cut to 1000 |
| `new CloseEvent('x', {code:-1}).code` | -1 | 65535 |
| `close(1000n)` | TypeError | Accept |
| `new CloseEvent('x', {code:1000n})` | TypeError | The construction was successful, with a code score of 1000. |

The first two lines are now consistent with the reference specifications. This fix ensures that they revert to the correct behavior. The normalization of negative/large numbers and the
conversion to BigInt should be performed according to the actual reference specifications. Do not rely solely on the browser’s WebIDL names to
determine the conversion rules. The explicit conversion of `Number(BigInt)` cannot replace the parameter conversion that rejects BigInt values.

It is necessary to retain the corrections for decimal representations, errors related to invalid codes, issues with large numbers and negative values, as well as exceptions related to BigInt. These issues should be
checked against the constructed values and the actual wire Close values. This will help prevent any damage to the control input during the C03 process.

## 3 P1: After closing, synchronization errors occurred, and the final state was not generated in time.

Replication handler:

```javascript
server.send('a-before');
server.close(1000, 'done');
throw new Error('expected listener error');
```

Reference discards the temporary message and ends with 1006, wasClean=false; the current Capsid has not delivered any messages.
However, there were no close/errors within the 2-second observation window, and the probe was ultimately closed by itself.

The reason comes from the new stage: `close()` sets `outgoingClosed=true` before the actual submission; synchronization
Mark the stage as abort, and `abortFromHandler()` is triggered directly due to outgoingClosed
Return. Pending closure, no submission, transport abort not executed. The old C17 only tested after send followed by throw.
Therefore, this branch can be passed without omission.

It is necessary to distinguish between JS calls to close and native Close being submitted/transport being terminated. Exceptional branches are not allowed.
Only with outgoingClosed, one can leave early. It is necessary to complete send→close→throw, close→throw, multiple listeners,
After the temporary sending quota fails, combine operations such as close, and assert the timely final state and the return of owner/channel/budget.
This round only confirmed that there is a lack of a final state within 2 seconds, and it has not been described as a proven permanent retention.

## 4 P1: Response construction disables WebSocket type checking

To fix C14, `Response` has moved the endpoint readiness verification to transfer, which is a reasonable stage.
Adjustment; But [fetch.js](../../../js/fetch.js) Just check that socket is not null/undefined.
Even type checking has been removed:

| `new Response(null, {status:101, webSocket:…})` | workerd | Capsid |
| --- | --- | --- |
| `{}` | TypeError | Success, status 101 |
| `true` | TypeError | Success, status 101 |
| `1` | TypeError | Success, status 101 |

This represents a new regression during the construction phase. During construction, it is necessary to verify the WebSocket type. Additionally, during the transfer process, it
is important to check conditions such as accept/role/redundant transfer. It is not possible to revert the C14 fixes by using the old
complete readiness checks. Similarly, it is not acceptable to allow the actual transfer to fail, thereby sacrificing compatibility during the construction phase.

## 5 P1: The rejection of the Promise returned by the listener is still considered a fatal failure.

Two additional scenarios: the async handler rejects immediately after sending; after sending, it waits for 80 ms before rejecting.
In both scenarios, the system continues to receive the next message and delivers it as `b-after`. Finally, the client closes the process normally (1000,
wasClean=true). The Capsid value is set to 1006 after processing the first message.
wasClean=false.

These are boundary checks related to the current C17 fixes. They cannot be considered as new regression issues. The original
approach required the use of synchronous throw, asynchronous reject, and multi-callback mechanisms. In this round of testing, only synchronous throw is still used.
`collector` waits for the Promise returned by the listener. The abort operation is performed through the outer catch block. This
behavior does not apply automatically to the listener returns values associated with references.

It is necessary to distinguish between event dispatch, asynchronous resource owners, and handling of failures returning a Promise. Additional handling includes rejecting,
Assertions such as rejecting after await, rejecting before and after send, the message being still available after the next one, and ultimately canceling/recycling.
The issues related to concurrent distribution and the original frame loss root cause in C15 remain unresolved. Therefore, the order of repair is not determined by abandoning the owner/GC accounting method.

Additionally, confirm two conversion gaps: when the CloseEvent reason is a Symbol, the reference throws a TypeError.
Capsid is accepted as `Symbol(s)`; if binaryType is a Symbol, a TypeError is thrown by reference.
The Capsid silently retains the blob. This falls within the supplementary compatibility boundary and is not claimed to be newly added in this round.

## 6 P2: After a failed rollback, the metrics of the old Worker did not recover

The old v1 Worker still remains alive and responsive after three failures. Its PID is 432937. After a rollback, the metrics indicate that the process belongs to the newly
terminated Worker with PID 432958. The RSS value is 0, and it continues to remain in this state for an additional 1.5 seconds. The initial metrics correctly indicate that the process
belongs to v1, with a RSS value of 9,801,728 bytes.

`on_worker_started` overrides active_workers/worker_pid during the preparation phase, and a failed new pool is evicted
Once again, clear the observation records; abort only restores the route, without restoring the observation identity of the old Worker that is still serving.
These are the observable issues remaining from the failed transaction. Do not misreport them as the Worker not having reclaimed them.

It is necessary to restore the corresponding active generation/Worker for the metrics, so that the service version and actual PID can continue to be available even after failures.
Consistency with the metric PID, as well as the observation switching after the new Worker is successfully activated again.

## 7 Testing Stability and Gatekeeping

The only failure occurred in 70 independent tests, specifically in `websocket_outbound_static-pool`. The assertion regarding the dialing interval of 9–12 seconds
was met. The assertions related to errors/closures were successfully passed. Subsequent tests conducted three times independently also yielded successful results. Since the actual elapsed
time was not logged during the original failures, it cannot be determined whether the failures were caused by CPU load, clock issues, or specific code issues.
Retain 69/70 and three passes; do not only record the last green light.

It is necessary to record the actual elapsed time and the duration of the dialing process. Use a monotonic clock to verify the duration, and check the status of event
scheduling/cancellation at the appropriate times. Do not simply relax the requirements for reviewing the time window at the end.

The addition of real Host persistence failures has indeed improved coverage, but it should also be confirmed that each new Worker is READY.
And the failure occurred during the persist phase. Avoiding premature failure in the future is also considered a pass for the rollback. Current chmod
The method also depends on the file permissions of the executing user; clear persistent fault injection conditions should be used to cover different executions.
Users. At the object level, factory/pool is released. The recycling process that occurs after the last retirement operation still needs to be confirmed directly.

Currently registered 412 items, which cannot be considered as complete unit tests or claimed to have been fully run. Existing reference conditions
Registration, CI Node `24` does not match the exact lock, lack of locking the workerd installation, and strict report comparison are missing.
The issue has not been corrected. The new api-compat C15 branch asserts the current serial gap of Capsid, and the documentation has been updated accordingly.
Acknowledged that no fixes were made; this pass only indicates that the known behavior remains unchanged, and cannot be considered a strong compatibility pass.

Document review passed (40 documents, 12 modules). Additional findings from the workspace `git diff --check`
The existing trailing spaces in `src/worker_runtime.cc:2689` will be retained as source code without modification this round.

## 8 Subsequent corrections and qualification sequence

1. Solidify the numerical conversion control for this round, close→throw final state, and regression for the Response type, and implement the fixes.
2. Please complete async reject, Symbol/BigInt/object conversion, and C12/C14 real header/transfer matrix.
   Fix C15 concurrency and frame loss root cause, while maintaining the assertions of owner, GC, cancellation, and budget final states.
3. Modify the failure transaction metrics to include clear persistence failures and object-level return assertions; retain the replacement and real scenarios.
   Host regression, analyzing independent sets for time failure.
4. Complete strict reference gate, dependency locking, complete case list; then proceed with Binding, CPU/
   heap/OOM, second client, native platform, v4/WS fuzz and soak.

**2026-10-14 GLM Correction Results (§2–§5, §10, §11):** All aspects have been finalized and are now officially implemented. (API
compatibility is now ensured for both modes; each mode requires 91 assertions.) §2: Verification of the original values using “close” function. Values of 1000.5 are considered acceptable.
66536/-64536 → InvalidAccessError, BigInt → TypeError); CloseEvent code =
trunc does not take modulus (65537/-1 is retained), reason/binaryType Symbol → TypeError. §3:
Separate nativeClosed/outgoingClosed—send → close → throw to discard the temporary storage pairs.
1006/wasClean=false Timely final state. §4: During the construction phase, only WebSocket types are verified ({} / true / 1 →
TypeError), accept/role/repeated handoff leaves the sending period unmet. §5: async reject swallows (subsequent messages)
Continue, cleanly close), only synchronize the throw and commit failures to abort. §10: C12 Nine-input real header
Matrix (raw TCP peer records request headers and selects the first token; non-array iterable → SyntaxError)
In harness, the socket.protocol getter referenced is always null, and the getter does not enter the golden zone.
§11: Delayed acceptance with bounded waiting (timer cleared) for successful upgrade and echo; repeated handoff and already
accept endpoint handover explicitly 500 final state (waiting only to hang if still pass the verification). **Discovered and during the process
Fixing real defects**: The unsuspended setTimeout on the terminal token continues to trigger.
"Terminal continuation leak" poisoning. Regression: four tests on workerd_ws, outbound/budget
Three-mode, channel surface, fast 15/15, complete 66/66 with zero skips, quickjs/context/identity fully green
Evidence: static-pool has been tested once according to §7 for known timing jitter, with a pass rate of 3/3 times.
`build-v030-dev/evidence/continuation-managed/index-fixround5.json`. §6 Metrics
Identity and §7 Gate are still pending in the next round.

**2026-10-14 GLM Updates (C15 and §6):** C15 has been fixed. The root cause lies in asynchronous context on the sending side and disordered
submission due to interleaved distribution (not related to concurrent processes themselves). Settled tokens are retired based
on the token registry; send/close operations operate based on the channel ID when no synchronization tokens are available. The submission order is ensured through
the flushing mechanism at each endpoint. It was observed that microtasks that were processed first but then added later due to dispatch:await issues still occur.
The active_event gate has been removed. C15 gold sequence dual-mode assertion is performed, with ping-flood measures conducted 6/6 times. Corresponding fixes have been implemented: failure conditions have been addressed.
send() throws TypeError (/pending late side effect dependent on observable rejection; wire layer still controlled by registry phase)
§6 Metrics Identity: ActivationPlan Backup service observation identity, abort for recovery.
deploy-persist-failure Ensures consistent metrics.pid values after each failure. Full green status indicates successful testing for workerd_ws after four attempts.
Worker WS suite, fast 15/15, complete 66/66 with zero skips, quickjs/context/identity 4/4.
Evidence `build-v030-dev/evidence/continuation-managed/index-c15final.json`.

## 9 Evidence

This iteration's index (located at `build-v030-dev/evidence/review-glm-fixes-20261014/index.json`) contains information on the current
source code/scripts/product summaries, GLM report snapshots, 70 JUnit tests/complete LastTest evaluations, three static re-runnings, 27
original probes and 38 extended probes. It also includes details regarding three types of abnormal paths, failed deployments, and metric reviews.

| Product | SHA-256 |
| --- | --- |
| Host | `eb5cf1f61b6c3f9d0356d3de03fd8a19ef8cf51c08a05e245bd9e0c09259d33e` |
| Worker | `d652cf3b796532b4927b7adf0f216e1c2e054d51a15137e363b9f5f84041b06b` |

Reference indicates v1.20261010.1; date: 2026-08-20; flags: `[]`. Node version: 24.14.0; Undici version: 7.21.0. It is necessary to verify the version/hash
before starting the process. The source code and output during this review cycle remain unchanged. The independent probe
Host/reference has completed its tasks successfully. Evidence has been ignored by Git; it needs to be archived or re-run across different machines.

## 10 Continued review: The C12 real protocol header is still incompatible

No new changes have been made to the source code, binaries, and GLM index after the previous round. We will continue to check the actual handshakes and handoffs.
For the first protocol in the same controlled TCP peer echo request, both ends record the actual header, the
`ws.protocol` value at the time of opening, and the value at the time of closing. The results are consistent across the single-worker, static-pool,
and managed modes.

| Constructors protocols input | Workerd Request Header / Selected Protocol | Capsid |
| --- | --- | --- |
| Excerpt or undefined | No header, protocol null | Match |
| null | `null` / `null` | No header, protocol null |
| 123 | `123` / `123` | No header, protocol null |
| true | `true` / `true` | No header, protocol null |
| toString returns the ordinary object of chat. | `chat` / `chat` | No header, protocol null |
| String chat | `chat` / `chat` | Match |
| Array chat/super | `chat, super` / `chat` | Match |
| Ordinary iterable objects, yield chat/super | Constructing syntax error | Sent `chat, super` and succeeded |

The C12 input only verifies that the process proceeds correctly without any errors, but this does not indicate a complete fix. Currently, the condition
`protocols !== null` and the handling of any value using `Array.from()` still differ from the way it works when workerd is locked.
It is necessary to fix the actual headers, select the appropriate protocols, and handle any errors in the construction process. Also, ensure that
array and non-array values are properly handled. Retain duplicate/empty tokens, symbols, and any custom conversions that may have caused errors. Do not apply browser-specific rules directly.

## 11 Continued review: C14 delays in accepting and handoff failures

The three Host modes use isolated instances, and they perform retesting of the 6 transfer operation and clone:

| Operations | workerd | Capsid Three Modes |
| --- | --- | --- |
| After normal acceptance, return 101. | Upgrade, echo, close 1000 | Match |
| First construct the Response, then accept, and finally return. | Upgrade, echo, close 1000 | Match |
| Implement a post-timer of 80 ms before accepting. | Upgrade, echo, close 1000 | 503, no WebSocket messages |
| Returning the accepted endpoint | Emits HTTP 101 first, then aborts with a `TypeError`. | No response head within the 8-second window; probe had to cancel. |
| The same Response is transferred for the first time | Upgrade, echo, close 1000 | Match |
| The same Response is handed over across requests for the second time. | HTTP 500, recording cross-request I/O errors | No response from the head within the 8-second window, requires the probe to cancel. |
| WebSocket-based Response.clone() | TypeError | Match |

The successful delay accepts the input as:

```javascript
const [client, server] = new WebSocketPair();
server.onmessage = event => server.send(event.data);
const response = new Response(null, { status: 101, webSocket: client });
setTimeout(() => server.accept(), 80);
return response;
```

`validateResponseWebSocket()` ensures that the peer is already accepted when the transfer begins, which differs from the delayed
acceptance supported by the reference implementation. The C14 construction phase has been corrected, but the entire lifecycle is not yet complete. To
maintain strong compatibility, it is necessary to re-evaluate the status of pending-acceptance, queuing, quotas, and cancellations. It is not
possible to simply remove the guard mechanism and then send messages before accepting the peer, or to wait indefinitely for peers that have not been accepted.

In cases of illegal/duplicate transfers, it is necessary to clearly define the state of failure. In the three-mode CAPSID scenario, no response headers were received within 8 seconds, and
there were errors during the verification process. This situation should not be considered as a permanent hang-up situation. There is no requirement
to replicate the process that involves sending a 101 response to the faulty endpoint. Upon acceptance, it is necessary to ensure that error responses are handled properly, and all resources are returned to their original state.

Evidence refers to the `continued_transport_review` index: 9 construction/handshake inputs, 6 types of transfers, and clones are
executed in three different modes. Items that are omitted/undefined, normal strings/arrays, normal transfers, and clones all
have matching controls. There are no duplicate executions of the same source code for the 70 items involved. Only the above-mentioned coverage is added in this round.

In the early stages of the hybrid scenario, peers that never accept connections remain unaccepted, and the cleanup of references is
not complete. Subsequent errors related to shared Capsid instances will also be affected by the previous scenario. These two stages should be considered as part of the diagnostic process. The final
result is achieved by isolating hosts that have been transferred to other hosts, ensuring that references are
properly handled for all official hosts. There are no forced stops occurring. It is recommended to add protocols related to pending acceptance and
failed transfers, and to submit these details along with the corrections and test results outlined in sections 2–8 to GLM.
