#pragma once

#include <QProcess>
#include <QString>

#include <functional>

#include "common/OperationResult.h"
#include "runtime/CoreInfo.h"

class ICoreProcessHost {
public:
    using ExitedCallback = std::function<void(int exitCode, QProcess::ExitStatus status, bool stopRequested)>;

    virtual ~ICoreProcessHost() = default;

    virtual OperationResult start(
        const CoreInfo& coreInfo,
        const QString& configFilePath,
        std::function<void(const QString&)> outputReceived,
        ExitedCallback exited = {}) = 0;

    virtual OperationResult stop() = 0;
    virtual OperationResult reload() = 0;
    virtual bool isRunning() const = 0;
};
