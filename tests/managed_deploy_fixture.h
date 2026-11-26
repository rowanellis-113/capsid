#ifndef CAPSID_TESTS_MANAGED_DEPLOY_FIXTURE_H
#define CAPSID_TESTS_MANAGED_DEPLOY_FIXTURE_H

#include "capsid/runtime.h"
#include "host/managed_host.h"

namespace capsid::test {

// Coordinator tests own the committed fleet so they can exercise each live worker.
inline host::DeployOutcome deploy_for_test(host::ManagedHostOptions options,
                                           const std::string& version,
                                           host::OperationStatus* status) {
    auto fleet = std::make_shared<std::vector<capsid_worker*>>();
    options.prepare_activation = [fleet](const std::string&, const host::DeployOutcome& prepared,
                                         std::string*) {
        *fleet = prepared.workers;
        return std::make_unique<host::ActivationPlan>();
    };
    options.commit_activation = [](host::ActivationPlan*) {};
    options.abort_activation = [fleet](host::ActivationPlan*) {
        for (capsid_worker* worker : *fleet)
            capsid_worker_destroy(worker);
        fleet->clear();
    };
    auto result = host::managed_deploy(&options, version, status);
    if (result.ok)
        result.workers.swap(*fleet);
    return result;
}

} // namespace capsid::test

#endif
