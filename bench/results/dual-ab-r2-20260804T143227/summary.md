# Two A/B comparisons for the repair batch (fresh build)

## A/B 1. Throughput cliff at a 65,537-byte response

At 64 connections, `bytes65537` went from 177 QPS at `a148005` (three errors and a hang) to 1,035 QPS with zero errors in the current build, a 5.8× increase.

## A/B 2. Regression check against `3f7e8b2`

| Fixture at 64 connections | `3f7e8b2` (QPS) | Current build (QPS) | Change |
| --- | ---: | ---: | ---: |
| bytes64k | 1,071 | 1,070 | 0% |
| bytes65537 | 1,063 | 1,035 | −2.6% |
| json | 1,638 | 1,642 | +0.2% |
| json16k | 1,129 | 1,132 | +0.3% |

The repair removed the throughput cliff. The phase changes, OutboundBuffer changes and stronger tests showed no regression in this comparison. The 1K and 16K results also held up against the pre-repair baselines of 1,594 and 1,134 QPS. This directory retains the manifests for both sides (commit, binary, load generator and bundle hashes, and parameters), CPU tick and RSS profiles, and raw `samples.*.jsonl` files.
