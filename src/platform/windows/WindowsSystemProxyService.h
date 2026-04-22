#pragma once

#include <QString>

#include "common/SystemProxyMode.h"

class WindowsSystemProxyService {
public:
    bool update(
        SystemProxyMode mode,
        int httpPort,
        int socksPort,
        const QString& proxyExceptions,
        const QString& advancedProtocol,
        const QString& pacUrl = {}) const;
    bool enable(int httpPort, int socksPort, const QString& proxyExceptions, const QString& advancedProtocol) const;
    bool disable() const;
    bool isEnabled() const;
    void resetOnShutdown() const;

private:
    bool setProxy(const QString& proxyServer, const QString& proxyExceptions, bool enabled) const;
    bool setPacProxy(const QString& pacUrl) const;
};
