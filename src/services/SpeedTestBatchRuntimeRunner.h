#pragma once

#include <atomic>
#include <functional>

#include <QList>
#include <QString>

#include "domain/models/Config.h"
#include "services/SpeedTestRequestItem.h"

namespace SpeedTestBatchRuntimeRunner {

bool runBatchedGroup(
    const Config& probeConfigTemplate,
    const QString& urlTestUrl,
    const QList<SpeedTestRequestItem>& groupItems,
    const QString& customConfigDirectory,
    const std::atomic_bool& cancelled,
    const std::function<void(const QString&)>& log,
    const std::function<void(const QString&, const QString&)>& emitResult);

} // namespace SpeedTestBatchRuntimeRunner
