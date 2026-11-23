#!/usr/bin/env bash
# Wraps the real capsid-host binary for the benchmark runner. The runner talks
# to every component through CAPSID_BENCH_* environment variables; this script
# materializes an isomorphic local app dir + host-v2 config, reports the
# identity contract (the bundle/worker SHA-256 it is about to load), and execs
# capsid-host on the local CLI.
set -euo pipefail

identity_out="${CAPSID_BENCH_IDENTITY_OUT:-}"
if [ -n "$identity_out" ]; then
    bundle_sha="$(sha256sum "${CAPSID_BENCH_BUNDLE:?}" | cut -d' ' -f1)"
    worker_sha="$(sha256sum "${CAPSID_BENCH_WORKER:?}" | cut -d' ' -f1)"
    printf '{"bundle_sha256":"%s","worker_sha256":"%s"}\n' \
        "$bundle_sha" "$worker_sha" >"$identity_out"
fi

workdir="$(mktemp -d "${TMPDIR:-/tmp}/capsid-bench-app.XXXXXX")"
mkdir -p "$workdir/apps" "$workdir/state" "$workdir/secrets"
chmod 700 "$workdir" "$workdir/apps" "$workdir/state" "$workdir/secrets"
cp "${CAPSID_BENCH_BUNDLE:?}" "$workdir/bundle.mjs"

timeout_ms="${CAPSID_BENCH_TIMEOUT_MS:-10000}"
authority="${CAPSID_BENCH_PUBLIC_AUTHORITY:-public.example}"
listen="${CAPSID_BENCH_LISTEN:-127.0.0.1:0}"

cat >"$workdir/capsid.json" <<EOF
{
  "apiVersion": "capsid/app-v2",
  "entry": "bundle.mjs",
  "pool": { "workers": 1 },
  "request": { "timeout": "${timeout_ms}ms" }
}
EOF

cat >"$workdir/host.json" <<EOF
{
  "apiVersion": "capsid/host-v2",
  "paths": {
    "applications": "$workdir/apps",
    "state": "$workdir/state",
    "secrets": "$workdir/secrets/{application}"
  },
  "admin": { "unix": "$workdir/admin.sock" },
  "capacity": { "workersTotal": 1 },
  "maximums": { "request": { "timeout": "3600s" }, "pool": { "workers": 1 } },
  "listeners": [
    {
      "name": "public",
      "tcp": "$listen",
      "publicOrigin": "http://$authority",
      "routing": { "mode": "path" }
    }
  ]
}
EOF

export CAPSID_INITIAL_STREAM_WINDOW="${CAPSID_BENCH_WINDOW:-1024}"

exec "${CAPSID_BENCH_HOST_BIN:?}" \
    --app-dir "$workdir" \
    --app "${CAPSID_BENCH_APPLICATION:-orders}" \
    --host-config "$workdir/host.json" \
    --listen "$listen" \
    --worker "${CAPSID_BENCH_WORKER:?}" \
    --workers 1 \
    --strict-sandbox off \
    --ready-fd "${CAPSID_BENCH_READY_FD:-3}"
