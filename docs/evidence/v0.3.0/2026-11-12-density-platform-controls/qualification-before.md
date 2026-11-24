# v0.3.0 Current Qualification and Compatibility Boundaries

Update time: 2026-11-11T21:21:07.603411+00:00. **v0.3.0 has not yet completed full platform and formal performance qualification.** The current frozen candidate is `2979e33b7b21909624ea571de0fc0acf10ef3009`, with 5131 source files and SDK78. The code in the frozen list is byte-for-byte consistent with the main working tree; this document and additional evidence are updated independently. The main working tree HEAD remains `8028b88adff26cbc1abcb9a2fafce2ffc3d8090f`, with no updates to the main branch, tags, Release, or Pages.

[This Round Evidence Index](evidence/v0.3.0/2026-11-12-queued-http-handshake/index.json), [Candidate Identities](evidence/v0.3.0/2026-11-12-queued-http-handshake/candidate.json), [Qualification Checkpoints](evidence/v0.3.0/2026-11-12-queued-http-handshake/checkpoint.json) and [Pre-modified State](evidence/v0.3.0/2026-11-12-queued-http-handshake/qualification-before.md) The entire range is retained. All the requirements for R1-R9 and historical data are still stored in the workspace file `current-goal-requirements.json`, without any cuts or deletions.

## This round of fixes and actual verification

The real socket control replicates the data loss from the old handshake: 104 bytes are written to the request, but only 52 bytes are actually sent. The old implementation then releases the unsent portion. `0065` now holds the remaining bytes through the standard `lws_issue_raw` queue and continues to send them, then releases the temporarily allocated buffer. The original request target continues to maintain its complete path, queries, and escapes; the receiving service window remains `4096 + LWS_PRE`, and the production scheduling is not modified.

| Verification | Actual results | Boundaries |
| --- | --- | --- |
| Permanent Native Abbreviation Regression | normal/short × 64/8192, a total of 4 items passed; the old implementation's 2 items passed normally, while 2 items failed due to minor issues. | GNU ELF test link wrapper, actually sends prefix; no production test hook |
| Complete differences between the Worker and the pinned workerd | Forced true positive shorthand 20 scenarios, 40 end executions passed; source connection FIN and actual Worker exit assertions retained | Corresponding to two components, not marked as the current candidate complete build. |
| Practical TLS | Both sets of original test cases for CA and TLS 1.2 RSA-PSS passed the forced true short write test, while the old implementation failed. | CA/Host Name counterexample, connection reuse, and retention of the original 30-second budget; does not claim to cover actual zero-byte WANT_READ/WANT_WRITE states |
| Original Reading Model | Windows/Linux: 152 scenarios passed, totaling 304 | 8/96 MiB gzip and raw/gzip overlap/cancel/abort for six shards; budget, inputs, and assertions remain unchanged. |
| Resource Observation | The actual POSIX recycling test passed; all descriptor/subprocess zero-counter example cases were rejected. | Native execution on Linux; the Darwin branch is still pending native compilation and running |
| Temporary directories and isolation fixtures | Old fixtures failed under the ancestor directory alias, but after correction, 20 contracts passed; the actual mutation baseline passed, and 35 fault variants were expected to be rejected by assertion. | The archiving path verification has not been relaxed; the isolation fixtures have completed two workerd locks for Darwin. |

[Permanent and Worker Abbreviations Records](evidence/v0.3.0/2026-11-12-queued-http-handshake/short-write-permanent-and-worker.json), [Real Difference](evidence/v0.3.0/2026-11-12-queued-http-handshake/real-worker-short-writes.json), [TLS](evidence/v0.3.0/2026-11-12-queued-http-handshake/linux-tls.json), [Resource/Path Control](evidence/v0.3.0/2026-11-12-queued-http-handshake/posix-recovery-and-temp.json) and [Gripper fault control](evidence/v0.3.0/2026-11-12-queued-http-handshake/fixture-mutations.json) Items can be reviewed individually. The compilation has been strictly revised to correct the issue of `%c` mismatch with `int*` in the process statistics parsing. The target has been changed to `char`.

## Building Identities and macOS

`5850d24` [Native macOS Task](https://github.com/ErosZy/capsid/actions/runs/43100912796) During the configuration phase, an identity verification rejection was encountered: after the patch change, the lock file still contains the old overlay key/manifest. The actual number of tests conducted during the run is **0**. [Original Archive](evidence/v0.3.0/2026-11-12-queued-http-handshake/macos-configure-original.zip) and [Log](evidence/v0.3.0/2026-11-12-queued-http-handshake/macos-configure-job.log) Maintained intact. The previous component verification did not cover this complete configuration issue.

Windows/Linux use the project sharing algorithm to independently generate the same 78 patch keys and 67 manifest files affected by the patches. Currently, only the two fields related to locked files have been synchronized in the sub-candidate. The vendor/QuickJS version and compatibility discrepancies remain unchanged, and the build identity verification is retained. The current candidate is tested on clean Linux source code. [Complete production configuration and 78 patch audit](evidence/v0.3.0/2026-11-12-queued-http-handshake/linux-clean-configure-and-audit.json) Passed, but the candidate complete ALL is not yet completed.

New macOS [43101912311](https://github.com/ErosZy/capsid/actions/runs/43101912311) The current candidate code ancestor `d9fd796bd86de6c8aa7826606ebd86ed968bd493` is checked against the exact code ancestor. There are still 26 items remaining from the original budget recheck and the current request/recovery cases. The latest observation is `in_progress`: the native configuration has passed, and ALL is being compiled; there is no conclusion yet regarding native test pass. The results of `2ee13a5`'s 71 items that passed and the closed failures of old 52 items are still valid. The complete macOS matrix has not been qualified.

## Full and Formal Measurement

| Objects | Actual Source | Current Conclusion |
| --- | --- | --- |
| Windows Unfiltered Full Set | `7d393a0` | 845 items: 832 passed, 5 with WPT failures, 8 declared platform skips; strictly referring to the current `mandatory-platform-reference-gate-running` |
| Linux production/LTO full version | `7d393a0` | 1101 items: 1096 passed, 5 failed with existing WPT, 0 skipped; 171 required references and strict compatibility audits passed; ordinarySuitePassed=false |
| Current Candidate Linux Full Process | `2979e33` | Clean source code is ready; independent queue `waiting-for-original-windows-strict-reference-terminal`. After the original Windows strict reference ends, TLS, ALL, unfiltered full volume, and strict reference will be executed in sequence. |

The complete results of the parent snapshot have not been re-labeled as the full version after the current production correction. The original five standard documents that failed and the corresponding sub-items that timed out are still retained. The current candidate has been verified. [Actual Linux Registration](evidence/v0.3.0/2026-11-12-queued-http-handshake/linux-registration-check.json) For 1107 items: The timeout, skip codes, and tags of the original 1101 items are retained; 6 additional items with a non-passable exit code 77 are skipped. Registration verification does not replace actual execution.

Continued requirements include: the current candidate's ALL/complete matrix for three platforms, ASan/TSan/UBSan and RSS/resource entry points, the original budget fuzz (including 10000 runs, max_len=65536, etc.), ABI/allocation failures and combined qualification, as well as the formal QPS/cold/density for the same quiet session. The formal performance point remains **0**, with complete R3/R5/R6/R7/R8 combinations and R9 open for consideration.

The density continues to be maintained for one app, with the least-loaded/lowest slot priority and 64 concurrent processes. All eight workers have been reached in the hot and cold phases; among the 65 READY workers, only 49 have been observed, and thus have not yet been qualified. No increase in concurrency or modification of scheduling has been made. The system must still meet all subsequent requirements, including PSS, 2048 MiB, two equal positive readings, and exit evidence.

The historical data on the performance page is still considered unverified. Screenshots and layout checks have been conducted for the 1280/960/820/600 four-viewports. The display will be checked again after the official data is loaded. Users do not need to connect to a browser.
