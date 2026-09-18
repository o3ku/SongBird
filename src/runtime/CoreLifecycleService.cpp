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
        [this](const QString& message) {
            emit started(message);
        });
}

OperationResult CoreLifecycleService::stop(bool immediate)
{
    return host_.stop(immediate);
}

OperationResult CoreLifecycleService::reload()
{
    return host_.reload();
}

bool CoreLifecycleService::isRunning() const
{
    return host_.isRunning();
}
