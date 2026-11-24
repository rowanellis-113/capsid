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

Save this as UTF-8 `capsid.json`. It requests no capability modules or outbound
Fetch targets:

```json
{
  "apiVersion": "capsid/app-v1",
  "permissions": {
    "modules": [],
    "fetch": { "allow": [] }
  },
  "pool": { "minReady": 1, "maxWorkers": 1 }
}
```

Do not put credentials in either file. The [configuration guide](configure-application.md)
explains how to request additional authority explicitly.

Use a free loopback port. If startup fails, read stderr and correct the cause;
there is no automatic port selection or business-request retry in this example.

### Linux and macOS

After installing to the default prefix:

```sh
CAPSID_BIN="$HOME/.local/bin"

"$CAPSID_BIN/capsid-host" --mode single-worker \
  --worker "$CAPSID_BIN/capsid-worker" \
  --source-bundle ./bundle.mjs \
  --source-name "file://$PWD/bundle.mjs" \
  --application hello --listen 127.0.0.1:8080 \
  --routing path --public-scheme http \
  --public-authority 127.0.0.1:8080 \
  --request-timeout-ms 5000 --strict-sandbox off \
  --ready-fd 1 --capsid-json ./capsid.json
```

Set `CAPSID_BIN` to the directory containing both executables if you extracted the archive manually.

### Windows PowerShell

Point `$capsidBin` at the actual extracted package's `bin` directory:

```powershell
$capsidBin = 'C:/tools/capsid/bin'
$bundle = (Resolve-Path -LiteralPath './bundle.mjs').Path
$sourceName = ([System.Uri]$bundle).AbsoluteUri

& "$capsidBin/capsid-host.exe" --mode single-worker `
  --worker "$capsidBin/capsid-worker.exe" `
  --source-bundle $bundle --source-name $sourceName `
  --application hello --listen 127.0.0.1:8080 `
  --routing path --public-scheme http `
  --public-authority 127.0.0.1:8080 `
  --request-timeout-ms 5000 --strict-sandbox off `
  --ready-fd 1 --capsid-json ./capsid.json
```

### Wait for Readiness

`--ready-fd 1` explicitly directs the one-line readiness record to stdout for
this interactive example. Diagnostics go to stderr. Successful startup produces:

```json
{"schema":"capsid-host-ready-v1","app":"hello","address":"127.0.0.1","port":8080}
```

Wait for this record before sending the request. File existence, process
creation or an empty log is not readiness. Supervising Hosts should use a
separate inherited readiness descriptor and, when needed, the `--stop-fd`
protocol; see [Host configuration](../reference/host-configuration.md).

### Send a request

Leave the Host running. In another terminal:

```sh
curl http://127.0.0.1:8080/@capsid/hello/
```

In Windows PowerShell, use `curl.exe` rather than the `curl` alias:

```powershell
curl.exe http://127.0.0.1:8080/@capsid/hello/
```

Expected JSON:

```json
{"message":"hello from Capsid","path":"/"}
```

The `path` routing mode removes `/@capsid/hello` before constructing the
application request URL. Stop the foreground Host when finished, normally with
Ctrl+C. This example does not install a system service or expose a public port.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| `missing --...` | Use the complete command. On the published `v0.3.1-beta.1` local CLI, public authority, strict-sandbox mode and a valid readiness descriptor are explicit CLI inputs; on `dev/v0.3.2`, `--mode managed` requires `--host-config` and `--worker`. |
| `--ready-fd is not an open descriptor` | The chosen descriptor must be inherited and open. The interactive example uses stdout, descriptor 1. |
| Listener bind failure | Confirm the address is loopback and the port is free; stop only processes you own. |
| Import or outbound Fetch denied | Add the narrow request to `capsid.json`; do not broaden policy or disable security checks to hide a failure. |
| Configuration is not owned by the current user | Create the example files under the same account on a suitable filesystem. In WSL, prefer a private native-Linux directory rather than a Windows-mounted workspace; do not bypass the ownership check. |
| Strict sandbox unavailable | Use the Linux prerequisites for untrusted workloads. Turning isolation off is only appropriate for trusted development. |
| Bytecode compatibility rejection | Use artifacts and compiler from the matching build; do not bypass identity checks. |

## Next Steps

- [Application configuration](configure-application.md): modules, egress, resources and
  entry points.
- [Host modes and options](../reference/host-configuration.md): static pools, Linux managed mode and
  trusted bytecode precompilation (`capsid-bytecode-compile` + offline signing).
- [Framework guides](../reference/frameworks/README.md): bundling TypeScript or
  npm dependencies (e.g. with `esbuild`) into a self-contained ESM.
- [Host integration](../reference/embedding-api.md): embedding the C/C++ runtime.
- [Testing guide](run-tests.md): what a local smoke test does—and does not—prove.
