# History and evidence

Use this index to trace an implementation decision, a review finding or a published result. Dates, commands and pass/fail counts in a review belong to the source revision it examined. [Current documentation](../README.md) · [Release identities](../reference/release-status.md)

## Reviews and design records

| Subject | Records |
| --- | --- |
| v0.3.2 Config v2, elastic pools and tenant isolation | [Findings, subsequent verification, open items and probes](v0.3.2/README.md) |
| v0.3.0 Host, FetchRPC and native fetch | [Implementation design](v0.3.0/implementation-design.md) · [FetchRPC v4 plan](v0.3.0/fetchrpc-v4-plan.md) · [Native fetch plan](v0.3.0/native-fetch-plan.md) · [Delivery plan](v0.3.0/delivery-plan.md) |
| v0.3.0 reviews | [Review brief](v0.3.0/review-brief.md) · [Takeover review](v0.3.0/takeover-review.md) · [Combined review](v0.3.0/combined-review.md) · [Fix review](v0.3.0/fix-review.md) · [Rounds 5–7](v0.3.0/rounds-5-7-review.md) · [Test coverage review](v0.3.0/test-coverage-review.md) |
| v0.3.0 handoffs | [Continuation](v0.3.0/continuation-handoff.md) · [Next steps](v0.3.0/next-steps-handoff.md) |
| Binding v1 | [Security remediation plan](bindings-v1/security-remediation-plan.md) |

The byte-locked WebSocket fixture cites the test coverage review by its source-era filename, `v0.3.0-test-completeness-review.md`. The [test coverage review](v0.3.0/test-coverage-review.md) retains that record.

Published `v0.3.1-beta.1` headers cited two specifications by their source-era names (`fetchrpc-v4.md` is now the [wire contract](../reference/fetchrpc-v4.md), and `binding-technical-design.md` is now the [Binding v1 design](../design/bindings-v1.md)).

## Release evidence

- **v0.3.1** — [beta.1 publication and asset digests](../evidence/v0.3.1/2026-11-22-beta1-publication/beta1-publication-final-r1.json).
- **v0.3.0** — [Qualification and known limits](v0.3.0/release-qualification.md). Publication records are available for [beta.5](../evidence/v0.3.0/2026-11-21-beta5-publication/beta5-publication-final-r1.json), [beta.3](../evidence/v0.3.0/2026-11-18-beta3-publication/beta3-publication-final-r1.json) and [beta.2](../evidence/v0.3.0/2026-11-13-ci-resumption/beta2-publication-final-r1.json). The [evidence index](../evidence/v0.3.0/2026-11-10/index.json) and [availability audit](../evidence/v0.3.0/2026-11-10/historical-evidence-availability.json) identify the inputs retained at capture.
- **Performance** — Match the commit, machine and protocol in the [benchmark record](../reference/benchmarks.md) to the measurement you are comparing.

## Historical journals

Read the journals for [FetchRPC](archives/2026-11-17/fetchrpc-v4.md.txt), [WebSocket](archives/2026-11-17/workerd-websocket-semantics.md.txt), the [Host](archives/2026-11-17/host-technical-design-review.md.txt), [performance](archives/2026-11-17/performance-benchmarks.md.txt), [testing](archives/2026-11-17/testing.md.txt) and [qualification](archives/2026-11-17/v0.3.0-current-qualification.md.txt) in English. The reference pages link to individual sections. The [archive inventory](archives/2026-11-17/index.json) records the original byte lengths and digests. The dated evidence indexes likewise retain the original hashes for `qualification-before.md`. The documentation audit verifies those captures from Git revision `845ae5dee5833c1bed1c2630cfb65e6ed76af7d7` and checks the English text separately against the coverage inventory.

The [coverage inventory](../data/documentation-coverage.json) records the role and digest of each page. When recording a new result, include the exact source, build, platform, command, exit status, raw reports and prerequisite skips.
