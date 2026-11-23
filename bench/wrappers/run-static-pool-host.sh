#!/usr/bin/env bash
# Benchmarks-only static-pool wrapper (NOT a managed production path). Same
# CAPSID_BENCH_* contract as run-host.sh, plus CAPSID_BENCH_WORKERS for the
# fixed pool size (1/2/4/6/8): materializes an isomorphic local app dir +
# host-v2 config and execs capsid-host with --workers "$workers".
set -euo pipefail

identity_out="${CAPSID_BENCH_IDENTITY_OUT:-}"
if [ -n "$identity_out" ]; then
    bundle_sha="$(sha256sum "${CAPSID_BENCH_BUNDLE:?}" | cut -d' ' -f1)"
    worker_sha="$(sha256sum "${CAPSID_BENCH_WORKER:?}" | cut -d' ' -f1)"
    printf '{"bundle_sha256":"%s","worker_sha256":"%s"}\n' \
        "$bundle_sha" "$worker_sha" >"$identity_out"
fi

workers="${CAPSID_BENCH_WORKERS:-1}"
case "$workers" in
1|2|4|6|8) ;;
*) echo "run-static-pool-host: CAPSID_BENCH_WORKERS must be 1, 2, 4, 6 or 8 (got $workers)" >&2; exit 2 ;;
esac

workdir="$(mktemp -d "${TMPDIR:-/tmp}/capsid-bench-pool.XXXXXX")"
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
  "pool": { "workers": $workers },
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
  "capacity": { "workersTotal": $workers },
  "maximums": { "request": { "timeout": "3600s" }, "pool": { "workers": $workers } },
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
    --workers "$workers" \
    --strict-sandbox off \
    --ready-fd "${CAPSID_BENCH_READY_FD:-3}"
