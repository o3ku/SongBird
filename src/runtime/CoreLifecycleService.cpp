#include "runtime/CoreLifecycleService.h"

CoreLifecycleService::CoreLifecycleService(ICoreProcessHost& host, QObject* parent)
    : QObject(parent)
    , host_(host)
{
}

OperationResult CoreLifecycleService::start(const CoreInfo& coreInfo, const QString& configPath)
{
    return host_.start(
        coreInfo,
        configPath,
        [this](const QString& line) {
            emit outputReceived(line);
        },
        [this](int exitCode, QProcess::ExitStatus status, bool stopRequested) {
            emit exited(exitCode, status, stopRequested);
        });
}

OperationResult CoreLifecycleService::stop()
{
    return host_.stop();
}

OperationResult CoreLifecycleService::reload()
{
    return host_.reload();
}

bool CoreLifecycleService::isRunning() const
{
    return host_.isRunning();
}
