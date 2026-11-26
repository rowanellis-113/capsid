# Getting started

Run one trusted Fetch handler on loopback. The published beta and the development branch use different local Host commands. Choose the instructions for the binaries on your PATH.

- **Published `v0.3.1-beta.1`** — [install on Linux or macOS](install-packages.md#install-on-linux-or-macos) or [install on Windows](install-packages.md#install-on-windows), then follow [the beta steps](#run-the-published-beta).
- **Source branch `dev/v0.3.2`** — [build the Host and worker](build-from-source.md), then follow [the development steps](#run-the-development-branch).

Use a Host and worker from the same build. For untrusted applications, see the [Linux strict sandbox](../reference/linux-sandbox.md) and [security policy](../../SECURITY.md).

## Create the handler

Create a working directory owned by the account that will run the Host, then
save this as UTF-8 `bundle.mjs`. The Host checks configuration ownership and permissions. In WSL, use your Linux home or another
private native-Linux directory: a Windows-mounted file can have a different
reported owner. Do not disable the checks or change unrelated files' ownership.

```js
export default {
  fetch(request) {
    return Response.json({
      message: "hello from Capsid",
      path: new URL(request.url).pathname,
    });
  },
};
```

This is already a self-contained ESM: it needs no package installation or
bundling step. Applications with dependencies must be bundled ahead of time;
see the [framework guides](../reference/frameworks/README.md).

## Run the development branch

For a `dev/v0.3.2` source build, put `bundle.mjs` and this UTF-8 `capsid.json` in the same directory:

```json
{ "apiVersion": "capsid/app-v2" }
```

Put the build directory containing both executables on your PATH. From the directory containing the two application files, start the local Host on Linux, macOS or Windows:

```sh
capsid-host --app-dir . --listen 127.0.0.1:8080 --ready-fd 1
```

After the Host prints a `capsid-host-ready-v1` record, request the application root in a second terminal:

```sh
curl http://127.0.0.1:8080/
```

Use `curl.exe` in Windows PowerShell. Expect `{"message":"hello from Capsid","path":"/"}`. Stop the Host with Ctrl+C. For pool sizes, tenant quotas or deployment to the Linux managed Host, see the [Config v2 examples](../design/config-v2.md).

## Run the published beta

Use the guide shipped with the installed binaries. It includes the matching manifest,
Host invocation, readiness record and request URL for each platform.
[Linux and macOS quick start](https://github.com/rowanellis-113/capsid/blob/v0.3.1-beta.1/docs/getting-started.md#linux-and-macos) · [Windows PowerShell quick start](https://github.com/rowanellis-113/capsid/blob/v0.3.1-beta.1/docs/getting-started.md#windows-powershell)

## Troubleshooting

| Symptom | Check |
| --- | --- |
| `missing --...` | In managed mode, supply both `--host-config` and `--worker`. |
| `--ready-fd is not an open descriptor` | The chosen descriptor must be inherited and open. The interactive example uses stdout, descriptor 1. |
| Listener bind failure | Confirm the address is loopback and the port is free; stop only processes you own. |
| Import or outbound Fetch denied | Add the narrow request to `capsid.json`; do not broaden policy or disable security checks to hide a failure. |
| Configuration is not owned by the current user | Create the example files under the same account on a suitable filesystem. In WSL, prefer a private native-Linux directory rather than a Windows-mounted workspace; do not bypass the ownership check. |
| Strict sandbox unavailable | Use the Linux prerequisites for untrusted workloads. Turning isolation off is only appropriate for trusted development. |
| Bytecode compatibility rejection | Use artifacts and compiler from the matching build; do not bypass identity checks. |

## Next Steps

- [Application configuration](configure-application.md): modules, egress, resources and
  entry points.
- [Host modes and options](../reference/host-configuration.md) for local packages and Linux managed deployment.
- [Framework guides](../reference/frameworks/README.md): bundling TypeScript or
  npm dependencies (e.g. with `esbuild`) into a self-contained ESM.
- [Host integration](../reference/embedding-api.md): embedding the C/C++ runtime.
- [Testing guide](run-tests.md): what a local smoke test does—and does not—prove.
