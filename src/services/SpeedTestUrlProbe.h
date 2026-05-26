#pragma once

#include <atomic>

#include <QString>

#include "domain/models/VmessItem.h"
#include "services/SpeedTestServiceInternal.h"

namespace SpeedTestUrlProbe {

QString normalizeErrorText(const QString& value);
bool shouldRetry(const SpeedTestServiceInternal::UrlProbeResult& result);

SpeedTestServiceInternal::UrlProbeResult probeReadyProxyWithRetry(
    const SpeedTestServiceInternal::ReadyProxy& proxy,
    const QString& url,
    int timeoutMs,
    const std::atomic_bool& cancelled);

SpeedTestServiceInternal::UrlProbeResult probeSocksWithRetry(
    int socksPort,
    const QString& url,
    int timeoutMs,
    const std::atomic_bool& cancelled);

SpeedTestServiceInternal::UrlProbeResult probeUpstreamProxyWithRetry(
    const VmessItem& server,
    const QString& url,
    int timeoutMs,
    const std::atomic_bool& cancelled);

} // namespace SpeedTestUrlProbe
