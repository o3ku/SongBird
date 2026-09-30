#pragma once

#include <QCoreApplication>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QString>

#include "common/UrlProbeLatency.h"
#include "domain/models/Config.h"

#include <functional>
#include <mutex>
#include <optional>
#include <set>

namespace SpeedTestServiceInternal {

struct ReadyProxy
{
    QNetworkProxy::ProxyType type = QNetworkProxy::DefaultProxy;
    int port = 0;
};

enum class UrlProbeStatus {
    Accessible,
    Timeout,
    Failed
};

struct UrlProbeResult
{
    UrlProbeStatus status = UrlProbeStatus::Failed;
    qint64 latencyMs = -1;
    QString errorText;
};

inline std::mutex& reservedProxyPortsMutex()
{
    static std::mutex mutex;
    return mutex;
}

inline std::set<int>& reservedProxyPorts()
{
    static std::set<int> ports;
    return ports;
}

// httpPort == 0 means "reserve only the SOCKS port" (batch runner path, one
// SOCKS inbound per entry); a zero locationProbePort is likewise ignored.
inline bool reserveProxyPorts(int socksPort, int httpPort, int locationProbePort = 0)
{
    if (socksPort <= 0
        || (httpPort != 0 && (httpPort <= 0 || socksPort == httpPort))
        || locationProbePort == socksPort
        || (httpPort != 0 && locationProbePort == httpPort)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(reservedProxyPortsMutex());
    std::set<int>& ports = reservedProxyPorts();
    if (ports.contains(socksPort)
        || (httpPort > 0 && ports.contains(httpPort))
        || (locationProbePort > 0 && ports.contains(locationProbePort))) {
        return false;
    }

    ports.insert(socksPort);
    if (httpPort > 0) {
        ports.insert(httpPort);
    }
    if (locationProbePort > 0) {
        ports.insert(locationProbePort);
    }
    return true;
}

inline void releaseProxyPorts(int socksPort, int httpPort, int locationProbePort = 0)
{
    std::lock_guard<std::mutex> lock(reservedProxyPortsMutex());
    std::set<int>& ports = reservedProxyPorts();
    ports.erase(socksPort);
    if (httpPort > 0) {
        ports.erase(httpPort);
    }
    if (locationProbePort > 0) {
        ports.erase(locationProbePort);
    }
}

inline void resetGlobalState()
{
    {
        std::lock_guard<std::mutex> lock(reservedProxyPortsMutex());
        reservedProxyPorts().clear();
    }
}

inline std::optional<ReadyProxy> detectReadyProxy(
    int socksPort,
    int httpPort,
    const std::function<bool(int)>& isPortReady)
{
    if (isPortReady(httpPort)) {
        return ReadyProxy{QNetworkProxy::HttpProxy, httpPort};
    }

    // Prefer the local HTTP inbound because browser/system-proxy traffic uses
    // that path, so URL test results better match the "set current server"
    // experience seen by users.
    if (isPortReady(socksPort)) {
        return ReadyProxy{QNetworkProxy::Socks5Proxy, socksPort};
    }

    return std::nullopt;
}

inline Config makeUrlTestRuntimeConfig(Config config)
{
    config.allowLanConnection = false;
    config.logEnabled = false;
    config.tun().tunModeItem.enableTun = false;
    config.dns().enableCacheFile4Sbox = false;
    config.collection().servers.clear();
    config.collection().subscriptions.clear();
    config.policy().coreTypeItems.clear();

    // Keep URL tests close to the actual "set current server" data path.
    // TUN is disabled because the test exercises the local proxy listener,
    // but routing/DNS/hosts/fake-ip should remain intact so the result still
    // reflects the user's effective runtime behavior.

    return config;
}

inline UrlProbeResult classifyUrlProbeResult(
    bool success,
    bool timedOut,
    qint64 latencyMs,
    const QString& errorText)
{
    if (success) {
        return UrlProbeResult{UrlProbeStatus::Accessible, latencyMs, {}};
    }

    if (timedOut) {
        return UrlProbeResult{UrlProbeStatus::Timeout, -1, QCoreApplication::translate("SpeedTestController", "Timeout")};
    }

    return UrlProbeResult{UrlProbeStatus::Failed, latencyMs, errorText.trimmed()};
}

inline QString formatUrlProbeResult(const UrlProbeResult& result)
{
    switch (result.status) {
    case UrlProbeStatus::Accessible:
        return result.latencyMs >= 0
            ? QStringLiteral("%1 ms").arg(result.latencyMs)
            : QCoreApplication::translate("SpeedTestController", "Blocked");
    case UrlProbeStatus::Timeout:
        return QCoreApplication::translate("SpeedTestController", "Timeout");
    case UrlProbeStatus::Failed:
    default:
        return result.errorText.isEmpty()
            ? QCoreApplication::translate("SpeedTestController", "Blocked")
            : result.errorText;
    }
}

inline bool tryParseUrlProbeLatency(const QString& value, double& latencyMs)
{
    return UrlProbeLatency::tryParseLatencyMs(value, latencyMs);
}

} // namespace SpeedTestServiceInternal
