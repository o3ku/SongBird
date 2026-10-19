#pragma once

#include <QString>

#include "common/SystemProxyMode.h"
#include "common/SystemProxySnapshot.h"

// The system-proxy side effect, reduced to the operations its callers use.
//
// The Windows implementation writes HKEY_CURRENT_USER and pokes wininet, so it cannot be made
// to fail on demand: a policy-locked registry or a denied write is unreachable from a test, which
// left the "the write failed" branch of every caller unverified. Callers take this interface
// instead, so a test can inject a stand-in that reports failure and check that the failure
// actually reaches the user rather than being dropped.
//
// capture()/restore() were added after a real defect: update(ForcedClear) *deletes*
// ProxyServer/ProxyOverride, so quitting the app wiped a proxy the user had configured before
// starting it. The three-state contract is:
//
//   capture()  read the current values, remember which ones existed
//   update()   write our own values
//   restore()  put the captured values back -- deleting only the keys that were absent
//
// restore() must be used instead of update(ForcedClear) everywhere the intent is "undo what we
// did". update(ForcedClear) remains for the intent "the user asked for the proxy to be off", where
// deleting the keys is the correct outcome.
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

    // Returns a snapshot with `valid == false` when the platform cannot read its proxy settings.
    // A caller that gets one must not restore it.
    virtual SystemProxySnapshot capture() const = 0;

    // Puts `snapshot` back. Returns false when any write failed; a partial restore is reported
    // rather than retried, because the caller's next step (shutting down, or telling the user)
    // is the same either way. An invalid snapshot is a no-op that reports true: there is nothing
    // to undo, and treating "never captured" as a failure would turn every non-Windows test into
    // a red case.
    virtual bool restore(const SystemProxySnapshot& snapshot) const = 0;
};
