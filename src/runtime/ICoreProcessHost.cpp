#include "runtime/ICoreProcessHost.h"

#include <utility>

// Unqualified on purpose. Written as ICoreProcessHost::start(...) this binds to the pure virtual
// entry point instead of dispatching, and every caller that takes the defaults then references a
// symbol nobody defines -- `end-to-end-smoke` compiles CoreLifecycleService.cpp but takes the host's
// object file from the shared objects, so the link fails only there.
OperationResult MainCoreHostBase::start(
    const CoreInfo& coreInfo,
    const QString& configFilePath,
    std::function<void(const QString&)> outputReceived,
    StartedCallback started,
    StartFailedCallback startFailed,
    ExitedCallback exited)
{
    return start(coreInfo, configFilePath, std::move(outputReceived), std::move(started), std::move(startFailed), std::move(exited));
}
