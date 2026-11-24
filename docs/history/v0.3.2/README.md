# v0.3.2 design and acceptance

As of 2026-11-24 the `dev/v0.3.2` working tree includes fixes for the three P1 findings from the design review. The checks below cover those fixes. Release-build performance, density and the remaining design commitments each require their own evidence. [Config v2 and elastic pools](../../design/config-v2.md) · [Release status](../../reference/release-status.md)

| Design commitment | Observed after the fix |
| --- | --- |
| Honor the application's `worker.memoryMax` | With an inherited 64 MiB JS heap, an explicit 72 MiB cap took effect exactly as specified. A 32 MiB cap caused an error before worker startup. |
| Enforce tenant fairness at both eviction entry points | Watermark scans and contention for elastic slots considered all resident tenants. When the over-budget tenant had only protected workers, the tenant within its share kept its worker. Processes awaiting exit still counted against their owner. |
| Enforce a tenant's `hardMemory` across running processes | With the Linux cgroup v2 memory controller delegated, sibling applications, old and new generations, and a worker serving an open SSE stream shared one 384 MiB parent cgroup. A second tenant had a separate 512 MiB limit. When `isolation.cgroupRoot` was omitted, deployment and scale-up admission used sampled memory. |

The selected design tests passed 53/53 on Windows and 65/65 in each Linux configuration (ordinary, ASan+UBSan and TSan). The tenant hard-limit check also passed in a real delegated Linux cgroup. The independent design probes passed 3/3. Open release checks cover the maximum number of processes awaiting asynchronous reclamation, reason reporting in metrics, Admin status and logs, restart backoff jitter, retirement of old configuration formats, and release-build cold start and density. [Design review and acceptance criteria](design-review.md)

## Review records

| Record | Scope |
| --- | --- |
| [Initial technical review](runtime-review.md) | Runtime and configuration findings at `e44d6ad`. |
| [Independent follow-up](runtime-followup.md) | Verification of minimum pool recovery, slot accounting and request dispatch after the fixes. |
| [Review brief](review-brief.md) · [Regression review](regression-review.md) | Working tree based on `85b9e1e`, acceptance steps and remaining counterexamples at the time. |
| [Design conformance review](design-review.md) | Original observations, reproduction inputs and performance measurement scope for milestones A, B and C. |

## Rerun the independent probes

Set `BUILD_DIR` to the Linux Ninja build directory containing `compile_commands.json`. From the repository root, run:

```sh
python3 docs/history/v0.3.2/probes/run_pool_probes.py "$BUILD_DIR" --design
```

With no option, the runner executes the four original pool cases. With `--recheck` it runs the five follow-up cases. On Windows, `test-worker-integration` runs the [Web contract cases](probes/web_contract.mjs) and [follow-up Web cases](probes/recheck_web.mjs) on a real worker. The [Linux tenant cgroup regression](../../../tests/sandbox/run-tenant-memory-cgroup.sh) requires a writable delegated subtree. Exit code 77 means that prerequisite was unavailable.
