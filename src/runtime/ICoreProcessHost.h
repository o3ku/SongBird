#pragma once

#include <QProcess>
#include <QString>

#include <functional>

#include "common/OperationResult.h"
#include "runtime/CoreInfo.h"

class ICoreProcessHost {
public:
    using StartedCallback = std::function<void(const QString& message)>;
    using StartFailedCallback = std::function<void(const QString& message)>;
    using ExitedCallback = std::function<void(int exitCode, QProcess::ExitStatus status, bool stopRequested)>;

    virtual ~ICoreProcessHost() = default;

    // No default arguments on the virtual: a default here would be supplied by whatever static
    // type the call is made through, so the same call would pass a different argument list
    // depending on whether it saw the interface or a concrete host. `MainCoreHostBase` below is
    // the one place the two trailing defaults are written down, and it reaches this override.
    virtual OperationResult start(
        const CoreInfo& coreInfo,
        const QString& configFilePath,
        std::function<void(const QString&)> outputReceived,
        StartedCallback started,
        StartFailedCallback startFailed,
        ExitedCallback exited) = 0;

    virtual OperationResult stop(bool immediate = false) = 0;
    virtual OperationResult reload() = 0;
    virtual bool isRunning() const = 0;
};

// The defaulted entry point, as a real (non-virtual) overload. A call site that passes only the
// callbacks -- `start(info, path, output, started)` -- resolves through this base and lands in the
// override, including when the host is a double that overrides only the virtual. A double that
// derived from ICoreProcessHost directly would have to repeat the defaults on its own override;
// one that forgot would answer a different signature than the one the production call sites reach,
// which is a double whose calls are silently not the ones being tested.
class MainCoreHostBase : public ICoreProcessHost {
public:
    using ICoreProcessHost::start;

    OperationResult start(
        const CoreInfo& coreInfo,
        const QString& configFilePath,
        std::function<void(const QString&)> outputReceived,
        StartedCallback started = {},
        StartFailedCallback startFailed = {},
        ExitedCallback exited = {})
    {
        return ICoreProcessHost::start(
            coreInfo,
            configFilePath,
            std::move(outputReceived),
            std::move(started),
            std::move(startFailed),
            std::move(exited));
    }
};
