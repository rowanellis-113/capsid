# Contributing

Capsid's stable line is `0.2.2`; `0.3.1-beta` is an experimental prerelease.
Public ABI, isolation and lifecycle contracts require explicit evidence. Prefer
a small, reproducible change over a broad refactor or an unexplained green run.

## Before Changing Code

- Discuss a bug, compatibility question or design change in a focused Issue.
- Report vulnerabilities privately using the [security policy](SECURITY.md).
- Discuss public ABI changes, architectural changes and new escape-level
  capabilities before implementation.
- Inspect the applicable source, tests and existing worktree changes. Do not
  reset or overwrite unrelated work.
- Keep source revision, build profile and claimed evidence aligned. A passing
  older build is not acceptance of a new input.

## Repository layout

| When changing | Start here |
| --- | --- |
| Public embedding API, worker or Host | `include/capsid/` for public headers, `src/` for native code, `src/host/` for the Host and `js/` for worker JavaScript |
| Builds, packaging and dependencies | `cmake/` for build rules, `tools/` for repository utilities, `vendor/` for pinned upstream sources, `patches/` for local changes and `licenses/` for third-party notices |
| Tests and benchmarks | `tests/` for suites and fixtures, `tests/sandbox/` for delegated Linux checks, `tests/soak/` for long-running load tests and `bench/` for performance measurements |
| Examples, documentation or publication | `examples/` for runnable integrations, `docs/` for guides and contracts, `release-notes/` for tagged release text and `perf-site/` for the published benchmark site |

## Prepare a Development Environment

Follow [building from source](docs/guides/build-from-source.md) for toolchains and platform
commands. Initialize all recorded submodules and install the locked JavaScript
dependencies: `vendor/txiki.js`, `examples/hono-reference`,
`examples/itty-router-reference`, `examples/h3-v2-reference` and
`examples/elysia-reference`. Missing framework or reference dependencies must
not be hidden by removing their tests.

Use a separate Debug build for ordinary development:

```sh
cmake -S . -B build-dev -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCAPSID_ENABLE_LTO=OFF \
  -DCAPSID_BUILD_HOST=ON -DBUILD_TESTING=ON
cmake --build build-dev --parallel 2
```

Run relevant CTests with `--no-tests=error`, then the appropriate broader
profiles from the [testing guide](docs/guides/run-tests.md). Native Windows/macOS,
Linux strict isolation, sanitizer dependency instrumentation and performance
acceptance are different obligations; one cannot stand in for another.

Release builds require clean Git provenance. Use a separate prepared checkout;
do not reset your current tree, forge an identity or bypass a clean-source
check to obtain a package.

## Source Formatting

Follow [source formatting](docs/guides/format-source.md) for the pinned development-only
clang-format, Prettier and Ruff setup. Run `npm run format` to apply maintained
source style and `npm run format:check` before committing (npm.cmd on Windows).
Use `npm run format:list` to inspect scope. Do not format vendor, generated
streams/tables, locked fixtures, historical evidence or performance archives.
The separate Formatting workflow enforces this policy without weakening the
existing runtime gates. Formatting does not transfer release qualification.

## Code and Contract Rules

- Keep the public C11 ABI and C++11 wrapper usable. Host-internal targets may
  require their declared newer language standard.
- Fix a demonstrated mechanism. Preserve original failures and add a focused
  regression or negative control where appropriate.
- Retain behavioral assertions, security checks and meaningful timing budgets.
  Do not replace a causal fix with retries, longer waits or a broader exclusion.
- Keep policy fail-closed: no silent capability, egress, sandbox or artifact
  identity degradation.
- Do not modify the generated vendor worktree. Vendor changes enter through
  reviewed patches and the locked identity/overlay flow.
- Keep unrelated formatting, dependency upgrades and refactors out of the PR.

## Documentation Rules

The [documentation index](docs/README.md#filing-a-document) describes naming, placement and the distinction between user guides, contracts and historical records.

When prose conflicts with implementation, check public headers, capability
manifests, configuration/build code and tests, then raw artifacts for the exact
source/build under discussion. Do not rewrite old evidence as if it described
new source or a different binary.

For documentation changes:

- Verify changed commands against actual CLI/configuration behavior; execute
  bounded examples where practical and report what was not run.
- Keep local relative links valid and every maintained Markdown page reachable
  from `docs/README.md`, directly or through a sub-index.
- Preserve existing historical paths, anchors and archived bytes. Moving or
  deleting them needs an explicit migration rather than silent cleanup.
- Avoid duplicating volatile test counts, pending-task lists or performance
  values across user pages. Link the source-bound record instead.
- Update documentary assertions when a real contract/status changes, but retain
  equivalent security, link, provenance and archive protections.

Write documentation and code comments in English. Preserve the original spelling of Unicode test inputs, captured process output and author names.

The documentation audit verifies the English editions against their inventory and the original captures against their recorded SHA-256 digests. It reads the captures from the published `v0.3.1-beta.1` revision. In a shallow clone, fetch that revision once before running the checks.

```sh
git fetch --depth=1 --no-tags origin 845ae5dee5833c1bed1c2630cfb65e6ed76af7d7
```

Run the documentation checks:

```sh
node tests/audit-current-docs.mjs .
node --test tests/test_documentation_archive.mjs
```

The [coverage inventory](docs/data/documentation-coverage.json) must have a reviewed
current/reference/history disposition for every page. Update its canonical-text
hashes after reviewed prose changes; immutable evidence uses exact-byte hashes.
Run `python3 tools/refresh-documentation-data.py --check` on the clean source checkout named in the [testing guide](docs/guides/run-tests.md#documentation-checks) to validate the captured release data.
Generated status does not contact GitHub or requalify a binary. Release metadata
captures and measured sources must be updated explicitly, not relabeled. For
site/transport changes, also run the archive controls and complete evidence
staging described in [site instructions](perf-site/README.md).

If the workflow audit or workflows change, also run:

```sh
node tests/audit-testing-validity-workflow.mjs .
node --test tests/test_testing_validity_workflow.mjs
```

## Pull Request Checklist

- [ ] One problem and a clear explanation of the mechanism and scope.
- [ ] Relevant tests actually executed; failures, skips and unrun profiles stated.
- [ ] New runtime behavior has appropriate regression/negative coverage.
- [ ] Public ABI, policy, configuration and platform documentation synchronized.
- [ ] No unrelated work, credentials, build directories or generated reports included.
- [ ] Performance claims have their original protocol, raw samples, correctness,
      environment and whole-session evidence—not just a CI timing result.
- [ ] No unapproved release, tag, deployment or historical-evidence changes.

CI is an input to review, not a replacement for reviewing ownership, authority
and lifecycle boundaries. If an infrastructure retry is authorized, preserve
its original outcome and exact job/attempt scope instead of presenting it as an
uninterrupted first-attempt pass.
