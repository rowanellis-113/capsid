# Capsid documentation

**Start here:** [Run a Fetch application](guides/run-application.md). The quick start covers the unified `capsid-host --app-dir .` CLI shipped in `v0.3.2-beta.1` and continued on `dev/v0.4.0` (which adds Binding RPC v2). [Release status](reference/release-status.md) identifies the source behind each published result.

## Guides

| Task | Guide |
| --- | --- |
| Install and run | [Install a package](guides/install-packages.md) · [Run an application](guides/run-application.md) · [Configure an application](guides/configure-application.md) |
| Bindings and WebSockets | [Use Binding RPC v2 (`v0.4.0`)](guides/use-binding-rpc-v2.md) · [Use WebSockets (`FetchRPC v4`)](guides/use-websockets.md) |
| Build the runtime or SDK | [Build from source](guides/build-from-source.md) · [Build on Windows](guides/build-on-windows.md) |
| Contribute a change | [Format source](guides/format-source.md) · [Run tests](guides/run-tests.md) · [Contributing](../CONTRIBUTING.md) |

## Reference

| Subject | Contracts and data |
| --- | --- |
| Deployment | [Host configuration](reference/host-configuration.md) · [Platform support](reference/platform-support.md) · [Linux sandbox](reference/linux-sandbox.md) |
| Embedding and authority | [C and C++ embedding API](reference/embedding-api.md) · [Host capability policy](reference/capability-policy.md) · [Application module permissions](reference/module-permissions.md) · [Trusted Binding permissions](reference/binding-permissions.md) |
| Protocol and runtime | [FetchRPC v4](reference/fetchrpc-v4.md) · [WebSocket semantics](reference/websocket-semantics.md) · [Native runtime behavior](reference/runtime-behavior.md) |
| Compatibility and measurements | [Web conformance](reference/web-conformance.md) · [Framework compatibility](reference/frameworks/README.md) · [Benchmarks](reference/benchmarks.md) · [Release status](reference/release-status.md) |

## Design

- [Runtime architecture](design/runtime-architecture.md) and [Host architecture](design/host-architecture.md) describe the process boundary, request path and worker lifecycle.
- [Binding v1](design/bindings-v1.md) and [Binding RPC v2 (`v0.4.0`)](design/binding-rpc-v2.md) describe trusted packages, dual-runtime isolation, and the credit-based stream/proxy/handle RPC contract.
- [Config v2 and elastic pools](design/config-v2.md) describes the development branch. The [v0.3.2 review](history/v0.3.2/README.md) records which design goals have been verified.

## History and supporting files

The source repository contains the review history and captured evidence. SDK packages built from this branch include the guides, reference, design pages and core manifests under `share/doc/capsid/`. [Capsid source repository](https://github.com/rowanellis-113/capsid)

| Directory | What belongs here |
| --- | --- |
| [history/](history/README.md) | Dated human reviews, plans, release qualification and original journals. Start with the [v0.3.0 qualification](history/v0.3.0/release-qualification.md) or the [v0.3.2 review](history/v0.3.2/README.md). |
| [evidence/](evidence/) | Captured outcomes for a specific version and run, including [publication verification](evidence/v0.3.2/2026-11-28-beta1-publication/beta1-publication-final-r1.json). |
| [data/](data/) | Machine-readable manifests, baselines and indexes consumed by builds or tools, including the [capability manifest](data/capability-manifest.json) and [documentation inventory](data/documentation-coverage.json). |

## Filing a document

Put a procedure in `guides/` and name it with a verb and an object, such as `run-tests.md`. Put a contract or lookup table in `reference/` and name it for its subject, such as `host-configuration.md`. Put an architecture decision or subsystem design in `design/`. Append a version to the subject only when it identifies the contract, as in `fetchrpc-v4.md`. Dated reviews belong under `history/<version>/`.

Use lowercase names with hyphens and reserve `README.md` for indexes. Link the page here or from a linked index, update the [documentation inventory](data/documentation-coverage.json), then run the [documentation checks](guides/run-tests.md#documentation-checks).
