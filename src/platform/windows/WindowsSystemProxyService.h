#pragma once

#include <QString>

#include "common/SystemProxyMode.h"
#include "platform/ISystemProxyService.h"

class WindowsSystemProxyService : public ISystemProxyService {
public:
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
};
