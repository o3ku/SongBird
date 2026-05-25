#pragma once

#include <atomic>
#include <functional>
#include <QString>

#include "domain/models/Config.h"
#include "services/SpeedTestRequestItem.h"

namespace SpeedTestRuntimeRunner {

QString defaultUrlTestUrl(const Config& config);

QString runUrlTest(
    const Config& probeConfigTemplate,
    const QString& urlTestUrl,
    const SpeedTestRequestItem& item,
    const QString& customConfigDirectory,
    const std::atomic_bool& cancelled,
    const std::function<void(const QString&)>& log);

} // namespace SpeedTestRuntimeRunner
