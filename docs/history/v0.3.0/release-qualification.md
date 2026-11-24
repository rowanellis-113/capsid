# v0.3.0 Qualification and Release Boundary

Updated 2026-11-21 (historical v0.3.0 qualification record). [`v0.3.0-beta.5`](https://github.com/rowanellis-113/capsid/releases/tag/v0.3.0-beta.5)
was the final `v0.3.0` experimental prerelease, later superseded by `v0.3.1-beta.1`. This page preserves completed,
source-bound acceptance and its limits for that release. See [current status](../../reference/release-status.md)
for the current stable and prerelease selections and [releases](../../guides/install-packages.md) for
installation.

## Qualified Published Input

| Identity | Value |
| --- | --- |
| Public product commit | `c7a80b9bdafe36d415a440be7ed3384f7830d4f8` |
| Product tree | `876e29c2792899b654310fea72aac979bd8bb48f` |
| Frozen git entries | 5469 |
| Vendor | `1a230d31183f062fae7a6c4fd2cff466cecc1787` |
| Interface | ABI v7; existing v3 default plus additive negotiated v4 |
| Package platforms | `linux-musl-x86_64`, `linux-musl-arm64`, `darwin-arm64`, `darwin-x86_64`, `windows-x86_64` |
| Release flags | Public (`draft:false`, `prerelease:true`, not GitHub Latest) |

The [final publication verification](../../evidence/v0.3.0/2026-11-21-beta5-publication/beta5-publication-final-r1.json) records the exact source, effective
job executions, verified package identities and 11 published asset digests.
The [release-status capture](../../data/release-status.json) records the published
state, independently of the build or documentation revision.

## Completed Required Gates

Commit `c7a80b9` passed branch [Testing validity](https://github.com/rowanellis-113/capsid/actions/runs/44778427101/attempts/2)
on attempt 2 (all 8 jobs); attempt 1 retains the cancelled macOS job and failing
evidence index. [Formatting](https://github.com/rowanellis-113/capsid/actions/runs/44778427087) passed on attempt 1.
The [tag Release](https://github.com/rowanellis-113/capsid/actions/runs/44851404278) then passed independently on
attempt 1 (8 validation jobs and 5 native package jobs).

The five archives, original `.sha256` sidecars, manifests, SPDX metadata,
executable CPU headers and Linux static policy were independently checked.
All 11 remote asset digests, including the tagged installer, matched after
publication. A later documentation edit does not requalify those binaries.

### Earlier Preserved Beta Records

- **`v0.3.0-beta.4` (`07db439`)**: no published Release. Its
  [tag run](https://github.com/rowanellis-113/capsid/actions/runs/44722065927) failed validation with a Windows
  ordinary Fetch test failure and a cancelled macOS runner job.
- **`v0.3.0-beta.3` (`af8acc1`)**: its source, verification runs and assets
  are preserved in [its publication record](../../evidence/v0.3.0/2026-11-18-beta3-publication/beta3-publication-final-r1.json).
- **`v0.3.0-beta.2` (`87ed4630fa57e360ac0a3a8158218398250c8035`)**: now marked
  `prerelease:true`; passed its full Release/LTO, sanitizer/provider,
  Debug/1800-second soak (`43881121477`, `43881141077`), three-platform preflight
  (`43881160382`), and tag Release (`43912740060`, including one explicitly
  authorized capacity retry on macOS job `133716098279` after `133565929203`
  never acquired a runner). Preserved in
  [beta2-publication-final-r1.json](../../evidence/v0.3.0/2026-11-13-ci-resumption/beta2-publication-final-r1.json).
- **`v0.3.0-beta.1` (`16f4354d353e6ab044394efe85438f040741c033`)**: preserved
  without published assets after its tag checkout failed before builds/tests.

## Real Failures Retained, Not Erased

Earlier inputs and their acceptance are not interchangeable:

- The b2a ASan soak convergence failure was a real ordinary failure and its
  campaign was NO-GO. The later metrics fix defers an already-queued memory
  sample until ordinary cancellation reclamation; it does not force GC,
  sleep longer or relax convergence.
- An earlier managed fixture selected a port and released it before Host bind.
  The fix keeps Host ownership from bind(0) onward and observes that child's
  actual listener; it adds no business/startup retry or larger deadline.
- The DNS dialer test observed a provisional phase before ACCEPT was queued.
  Its original wait now requires both phase and actual queued command, while
  the exact-one/type and lifecycle assertions stay intact. Missing original
  Windows executable/queue evidence limits reconstruction of that old failure.
- The fully qualified beta.1 source was tagged, but its Release checkout failed
  before builds/tests because Git received conflicting tag ref mappings. The
  user chose a preserved beta.1 tag plus beta.2 with event-SHA checkout.

Causal/source-bound records remain linked from the preserved historical
snapshot and [history index](../README.md). Later passes do not prove a
missing historical schedule or erase the original result.

## Compatibility Acceptance Is Not Full Conformance

The 84-file pinned WPT selection retains five raw file failures. The v2
workerd-default policy accepts only its exact recorded conflicts alongside
complete matching reference/context reports. The original ordinary suite
remains nonzero: `ordinarySuitePassed:false` and
`standardsConformanceClaimed:false`.

Genuine platform/prerequisite skips remain visible. Required delegated Linux
sandbox/profile gates cannot be replaced by an ordinary skip, and native
Windows/macOS development is not Linux-equivalent production isolation.
See [conformance](../../reference/web-conformance.md), [testing](../../guides/run-tests.md) and
[security](../../../SECURITY.md).

## Performance and Open Historical Work

The [performance reference](../../reference/benchmarks.md) includes a complete WSL2
session from a locally rebuilt `v0.3.0-beta.5` source checkout. That result
measures the local build, not the distributed packages, and is separate from
release acceptance. The earlier `82eaab9` development record stays separate.

This release does not claim every historical R1–R9 goal is closed. Missing
original snapshots, several old Windows schedules/diagnostics, earlier runner
cancellations, physical-host equivalence and unmeasured code changes remain
bounded by their own evidence. Old “current/pending/not released” text is a
captured statement, not permission to rerun those campaigns.

## Documentation/Data Refresh

The `dev/v0.3.0` documentation update synchronizes references, release/standards
status and the latest accepted measured snapshot with complete sealed evidence.
An update to these docs or the Release notes does not rebuild or requalify
`v0.3.0-beta.5`, move any tag, change any published asset or alter historical
samples. The new commit's own CI must report its own outcome;
acceptance is not inherited merely because a documentation commit descends
from the qualified release source.

## Historical source and preserved anchors

Read the dated checkpoints and their recorded measurements in the
[English journal](../archives/2026-11-17/v0.3.0-current-qualification.md.txt). They are not current release instructions.

<details>
<summary>Legacy section links</summary>

<a id="archived-section-125"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L1) — v0.3.0 Current Qualification and Compatibility Boundaries

<a id="archived-section-128"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L3) — Beta.2 has been released and reviewed (2026-11-17 00:45 UTC)

<a id="archived-section-131"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L11) — Snapshot before Beta.2 source code release (2026-11-16 UTC); all qualifications for beta.1 have been passed, but the tag checkout actually failed. The tag will be retained for a beta.2 release.

<a id="archived-section-134"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L33) — Previous round of complete CI conclusions (2026-11-16 UTC; actual failure, beta status NO-GO retained)

<a id="archived-section-137"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L55) — Prior phase: Local retest after headers and deployment merge (2026-11-16 UTC; first-pass independent acceptance)

<a id="archived-section-140"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L75) — Phase 9c History (UTC 2026-11-15; original snapshot at the time of clean build and completion of 43 regression tests)

<a id="archived-section-143"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L95) — Reviewed facts of parent source b59 (Windows original matrix failed; no migration to conclusion for 9c)

<a id="archived-section-146"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L119) — This round of optimization and actual identity lock failure

<a id="archived-section-149"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L129) — dd589 same-source platform checks: reviewed (not full release qualification)

<a id="archived-section-152"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L151) — User confirmation of existing benchmarks; the 11th dd589 session has been accepted.

<a id="archived-section-155"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L159) — Final qualification 421: Reviewed, but TSan actually failed

<a id="archived-section-158"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L181) — 77 three-lane performance qualification and unverified legacy baseline (historical retention)

<a id="archived-section-161"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L189) — 77 Platform Review and New Windows Failure

<a id="archived-section-164"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L203) — Previous d6 archive candidate: reviewed scope

<a id="archived-section-167"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L209) — Previous round of 6a9 frozen candidates

<a id="archived-section-170"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L213) — 6a9 Candidate: Items that have been reviewed and those still open

<a id="archived-section-173"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L239) — The previous 6ef4 candidate's consistency review results

<a id="archived-section-176"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L261) — Fifth official performance failure: Problem with the new warm connection phase

<a id="archived-section-179"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L271) — Prior listener candidate: reviewed scope and original failures

<a id="archived-section-182"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L296) — This round of fixes and verifications

<a id="archived-section-185"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L298) — Activation phase App directory FD repeated closure

<a id="archived-section-188"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L306) — Admin HTTP Incremental Parsing

<a id="archived-section-191"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L316) — macOS Split Test Controller

<a id="archived-section-194"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L322) — Startup readiness must verify an active listener, not only socket-file existence

<a id="archived-section-197"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L340) — Fuzz: Qualified by candidate reviewer, historical failures retained

<a id="archived-section-200"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L354) — Homogeneous R3/R5/R6 Report Replay

<a id="archived-section-203"></a>
[Historical section](../archives/2026-11-17/v0.3.0-current-qualification.md.txt#L360) — All requirements and remaining thresholds

</details>
