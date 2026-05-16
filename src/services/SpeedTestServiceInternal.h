#pragma once

#include <QNetworkProxy>
#include <QRegularExpression>
#include <QString>

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
    QString name;
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

inline bool reserveProxyPorts(int socksPort, int httpPort)
{
    if (socksPort <= 0 || httpPort <= 0 || socksPort == httpPort) {
        return false;
    }

    std::lock_guard<std::mutex> lock(reservedProxyPortsMutex());
    std::set<int>& ports = reservedProxyPorts();
    if (ports.contains(socksPort) || ports.contains(httpPort)) {
        return false;
    }

    ports.insert(socksPort);
    ports.insert(httpPort);
    return true;
}

inline void releaseProxyPorts(int socksPort, int httpPort)
{
    std::lock_guard<std::mutex> lock(reservedProxyPortsMutex());
    std::set<int>& ports = reservedProxyPorts();
    ports.erase(socksPort);
    ports.erase(httpPort);
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
        return ReadyProxy{QNetworkProxy::HttpProxy, httpPort, QStringLiteral("http")};
    }

    // Prefer the local HTTP inbound because browser/system-proxy traffic uses
    // that path, so URL test results better match the "set current server"
    // experience seen by users.
    if (isPortReady(socksPort)) {
        return ReadyProxy{QNetworkProxy::Socks5Proxy, socksPort, QStringLiteral("socks")};
    }

    return std::nullopt;
}

inline Config makeUrlTestRuntimeConfig(Config config)
{
    config.allowLanConnection = false;
    config.enableStatistics = false;
    config.logEnabled = false;
    config.tunModeItem.enableTun = false;
    config.enableCacheFile4Sbox = false;

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
        return UrlProbeResult{UrlProbeStatus::Timeout, -1, QStringLiteral("Timeout")};
    }

    return UrlProbeResult{UrlProbeStatus::Failed, latencyMs, errorText.trimmed()};
}

inline QString formatUrlProbeResult(const UrlProbeResult& result)
{
    switch (result.status) {
    case UrlProbeStatus::Accessible:
        return result.latencyMs >= 0
            ? QStringLiteral("%1 ms").arg(result.latencyMs)
            : QStringLiteral("Blocked");
    case UrlProbeStatus::Timeout:
        return QStringLiteral("Timeout");
    case UrlProbeStatus::Failed:
    default:
        return result.errorText.isEmpty()
            ? QStringLiteral("Blocked")
            : result.errorText;
    }
}

inline bool tryParseUrlProbeLatency(const QString& value, double& latencyMs)
{
    static const QRegularExpression expression(
        QStringLiteral("^(?:accessible\\s+)?([+-]?\\d+(?:\\.\\d+)?)\\s*ms$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = expression.match(value.trimmed());
    if (!match.hasMatch()) {
        return false;
    }

    bool ok = false;
    const double parsed = match.captured(1).toDouble(&ok);
    if (!ok) {
        return false;
    }

    latencyMs = parsed;
    return true;
}

} // namespace SpeedTestServiceInternal
