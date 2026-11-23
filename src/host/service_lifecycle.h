#ifndef CAPSID_HOST_SERVICE_LIFECYCLE_H
#define CAPSID_HOST_SERVICE_LIFECYCLE_H

#include "host/active_state.h"

#include <optional>
#include <string>
#include <string_view>

namespace capsid::host {

// In-memory control-plane state. A pending retire retains its source active
// or quarantined document until persistence resolves; after a committed
// retired tombstone it retains the retired document until drain completes.
// A pending quarantine retains its source active document.
enum class ServiceLifecyclePhase {
    kAbsent,
    kActive,
    kRetiring,
    kRetired,
    kQuarantining,
    kQuarantined,
    kDurabilityUncertain,
    kFailedClosed,
};

struct ServiceLifecycleState {
    ServiceLifecyclePhase phase = ServiceLifecyclePhase::kAbsent;
    std::optional<ActiveStateDocument> document;
};

} // namespace capsid::host

#endif
