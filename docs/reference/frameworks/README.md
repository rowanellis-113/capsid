# Framework compatibility

> Source/profile reviewed 2026-11-27 against `dev/v0.3.2`. The four dependency locks below describe
> the `0.3.x` matrix, not upstream-latest packages. See [current status](../release-status.md).

Capsid implements the WinterTC **ECMA-429 Minimum Common Web API** profile
`CAPSID-MIN-2025-subset-v0` (see [standards and conformance](../web-conformance.md)).
Any framework that compiles to a single self-contained ESM exporting a standard
`fetch(request)` handler can be run on Capsid, provided it stays inside the
Web-standard surface and does not depend on a Node/server adapter, a listener,
filesystem static serving, or provider-specific bindings.

Frameworks run as ordinary, self-contained ESM applications. They are not part of the Capsid Web API profile and do not enter the runtime ABI or native capability policy. The runtime has no framework detection, no special branches, and no modified npm source.

## Verified frameworks

The compatibility suite pins these versions and checks them with
differential vectors and independent absolute assertions:

| Framework | Pinned version | Differential scope | Notes |
| --- | --- | ---: | --- |
| [Hono](hono.md) | 4.12.32 | 68 vectors | Core routing, middleware, streaming, and lifecycle |
| [itty-router](itty-router.md) | 5.0.24 | 96 vectors × 3 variants | AutoRouter, Router, IttyRouter |
| [H3 v2](h3-v2.md) | 2.0.1-rc.26 | 129 vectors | Core, middleware/hooks, some Web-standard utilities |
| [Elysia](elysia.md) | 1.4.29 | 50 vectors | AOT compose, hooks, multipart, @elysiajs plugins, schema validation; three pinned app-level notes |

## Using other frameworks

Other Web-standard frameworks that target the same fetch-handler model can be
evaluated against the same rules: bundle to one audited ESM, avoid the excluded
surfaces listed below, and run the differential/lifecycle/global-surface matrix.
Only the pinned versions above carry Capsid evidence; adding a framework requires
the same reference app, vector suite, negative controls, and upgrade process as
the existing four.

Common validation path:

```text
spawn worker
  → LOAD_BUNDLE (single audited ESM)
  → READY
  → FetchRPC request/credit
  → exported fetch(Request)
  → FetchRPC response/credit
```

The differential between the reference and the real worker is only part of the evidence. Independent absolute assertions prevent a reference and runtime failing identically from appearing as a pass (via per-vector `expect` in `itty-router` and H3 v2, and via a dedicated `absoluteExpectations` map in Hono and Elysia).

Common exclusions: Node/Deno/Bun/Cloudflare adapters, server/listener, filesystem static serving, raw listeners and framework-specific WebSocket server adapters, external/remote/`file:` imports, and provider ambient bindings. These boundaries are maintained by separate expected-rejection tests and are not counted as framework core incompatibilities.

## Upgrade process

Update the exact dependency and lockfile, rebuild the unmodified upstream package into the audited entries, and rerun the differential, lifecycle, exclusion, permission, global-surface and sanitizer checks for that framework. Review any change to vector normalization or excluded behavior. Each recipe identifies the framework-specific cases to rerun.
