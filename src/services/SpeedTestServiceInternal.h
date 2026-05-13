#pragma once

#include <QNetworkProxy>
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

inline std::mutex& startupSlotsMutex()
{
    static std::mutex mutex;
    return mutex;
}

inline int& activeStartupSlots()
{
    static int count = 0;
    return count;
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

inline bool tryAcquireStartupSlot(int maxActiveSlots)
{
    if (maxActiveSlots <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(startupSlotsMutex());
    int& count = activeStartupSlots();
    if (count >= maxActiveSlots) {
        return false;
    }

    ++count;
    return true;
}

inline void releaseStartupSlot()
{
    std::lock_guard<std::mutex> lock(startupSlotsMutex());
    int& count = activeStartupSlots();
    if (count > 0) {
        --count;
    }
}

inline void resetGlobalState()
{
    {
        std::lock_guard<std::mutex> lock(reservedProxyPortsMutex());
        reservedProxyPorts().clear();
    }
    {
        std::lock_guard<std::mutex> lock(startupSlotsMutex());
        activeStartupSlots() = 0;
    }
}

inline std::optional<ReadyProxy> detectReadyProxy(
    int socksPort,
    int httpPort,
    const std::function<bool(int)>& isPortReady)
{
    if (isPortReady(socksPort)) {
        return ReadyProxy{QNetworkProxy::Socks5Proxy, socksPort, QStringLiteral("socks")};
    }

    if (isPortReady(httpPort)) {
        return ReadyProxy{QNetworkProxy::HttpProxy, httpPort, QStringLiteral("http")};
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
    config.sniffingEnabled = false;
    config.routeOnly = false;

    // Keep the test runtime focused on bringing up a local proxy quickly.
    // Full routing and remote geosite/geoip rule-set initialization can block
    // startup and cause false "startup timeout" failures during URL testing.
    config.routingItems.clear();
    config.routingCustomRules.clear();
    config.routingIndex = 0;
    config.enableRoutingAdvanced = false;

    // Let the test runtime fall back to core/system defaults instead of
    // building a full DNS pipeline with detours and hosts/caches.
    config.remoteDns.clear();
    config.directDns.clear();
    config.bootstrapDns.clear();
    config.dnsHosts.clear();
    config.addCommonHosts = false;
    config.useSystemHosts = false;
    config.fakeIp = false;
    config.globalFakeIp = false;
    config.blockBindingQuery = false;
    config.parallelQuery = false;
    config.directExpectedIps.clear();

    return config;
}

} // namespace SpeedTestServiceInternal
