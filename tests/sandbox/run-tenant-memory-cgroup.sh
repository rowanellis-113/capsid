#!/bin/sh
set -eu

mount=/sys/fs/cgroup
if [ ! -w "$mount" ] || ! grep -qw memory "$mount/cgroup.subtree_control"; then
    echo "SKIP: test requires a writable cgroup v2 subtree with memory delegated"
    exit 77
fi

# The Host and its test workers are the only users of this temporary subtree.
group=$(mktemp -d "$mount/capsid-tenant-memory.XXXXXX")
trap 'find "$group" -depth -type d -delete || exit 1' EXIT
echo +memory >"$group/cgroup.subtree_control"
"$1" host_managed_tenant_hard_memory_cgroup "$2" "$3" "$group"
