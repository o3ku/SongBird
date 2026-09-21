#pragma once

#include <QString>

#include "common/SystemProxyMode.h"

// The system-proxy side effect, reduced to the operations its callers use.
//
// The Windows implementation writes HKEY_CURRENT_USER and pokes wininet, so it cannot be made
// to fail on demand: a policy-locked registry or a denied write is unreachable from a test, which
// left the "the write failed" branch of every caller unverified. Callers take this interface
// instead, so a test can inject a stand-in that reports failure and check that the failure
// actually reaches the user rather than being dropped.
//
// Deliberately narrower than WindowsSystemProxyService: only the three operations with callers
// appear here. Keeping the seam at "what is used" means a stand-in stays small enough to write
// correctly in a test.
class ISystemProxyService {
public:
    virtual ~ISystemProxyService() = default;

    virtual bool update(
        SystemProxyMode mode,
        int httpPort,
        int socksPort,
        const QString& proxyExceptions,
        const QString& advancedProtocol) const = 0;
    virtual bool isEnabled() const = 0;
    virtual void resetOnShutdown() const = 0;
};
