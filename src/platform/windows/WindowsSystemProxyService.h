#pragma once

#include <QString>

#include "common/SystemProxyMode.h"
#include "platform/ISystemProxyService.h"

class WindowsSystemProxyService : public ISystemProxyService {
public:
    // `settingsPath` selects the registry key this service writes; an empty path means the
    // real HKCU\...\Internet Settings key. It is a parameter because the real key may not be
    // mutated by a test run, so without it the class has no reachable coverage at all: a
    // scratch key exercises the success path, and a location that refuses writes is the only
    // way to reach the failure path.
    explicit WindowsSystemProxyService(QString settingsPath = {});

    // `update()` is the only entry point, deliberately. It covers both directions through
    // SystemProxyMode (ForcedChange / ForcedClear), so the `enable(...)` / `disable()` wrappers
    // that used to sit here had no callers left and were removed rather than kept as a second
    // way in. SystemProxyCoordinator has its own enable()/disable(), but those are a different
    // class and route through setMode() -> update() as well.
    bool update(
        SystemProxyMode mode,
        int httpPort,
        int socksPort,
        const QString& proxyExceptions,
        const QString& advancedProtocol) const override;
    bool isEnabled() const override;
    void resetOnShutdown() const override;

private:
    bool setProxy(const QString& proxyServer, const QString& proxyExceptions, bool enabled) const;
    // False when the key could not be removed. A PAC script takes precedence over
    // ProxyEnable/ProxyServer, so a removal that fails silently leaves the system pointing at
    // a PAC URL while this service reports that the proxy was applied or cleared.
    bool clearAutoConfigUrl() const;

    QString settingsPath_;
};
