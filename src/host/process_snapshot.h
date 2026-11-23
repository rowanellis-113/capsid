#ifndef CAPSID_HOST_PROCESS_SNAPSHOT_H
#define CAPSID_HOST_PROCESS_SNAPSHOT_H

#include "host/metrics.h"

#include <atomic>
#include <cstdint>
#include <functional>
#if defined(_WIN32)
#include "win32_compat.h"
#else
#include <sys/types.h>
#endif

namespace capsid::host {

// Select identity when metrics are rendered. Managed mode supplies a
// selector over its published routing table, so warming and rollback do
// not need a second mutable copy of the serving worker identity.
MetricsRegistry::ProcessSnapshotProvider
process_snapshot_provider(std::function<pid_t()> worker_pid);

// M2 item 7 (design §12.1 "process family"): the default render-time
// snapshot provider. Reads the Host's own RSS/PSS from /proc/self and its
// CPU time from /proc/self/stat; reads the currently active worker's RSS
// from /proc/<pid>/statm.
//
// worker_pid is a process-shared atomic the Host keeps updated when a
// worker activates (managed mode: at activation; single-worker/static-pool:
// at spawn). A zero pid renders a zero worker series — never fabricated
// data from an unknown process. The provider is called on the /metrics
// render path only, so its /proc reads never block the counter methods.
MetricsRegistry::ProcessSnapshotProvider
default_process_snapshot_provider(const std::atomic<pid_t>* worker_pid);

// Read-only process memory footprint in bytes for eviction accounting.
// On Linux prefers PSS from /proc/<pid>/smaps_rollup and falls back to RSS;
// on Windows and macOS reads the process working set / resident size.
bool read_process_footprint_bytes(std::int64_t pid, std::uint64_t* bytes_out);

} // namespace capsid::host

#endif
