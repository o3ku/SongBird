#pragma once

#include "platform/ISystemProxyService.h"

// Stand-in for platforms whose proxy settings SongBird does not write.
//
// The contract is the same three-state one as the Windows implementation, and each method
// answers the way a platform with nothing to write has to:
//
//   capture()  returns an invalid snapshot, which every caller already treats as "cannot
//              re-apply on this platform" -- the same answer macOS gives, and the callers'
//              documented fallback is to clear instead of restore
//   update()   reports failure, because there is no registry to write and claiming success
//              would let the app believe the system proxy is on
//   isEnabled() reports false, so computeProxyUiState() never advertises Active for a proxy
//              that was not applied
class NullSystemProxyService : public ISystemProxyService {
public:
    bool update(
        SystemProxyMode mode,
        int httpPort,
        int socksPort,
        const QString& proxyExceptions,
        const QString& advancedProtocol) const override
    {
        Q_UNUSED(mode);
        Q_UNUSED(httpPort);
        Q_UNUSED(socksPort);
        Q_UNUSED(proxyExceptions);
        Q_UNUSED(advancedProtocol);
        return false;
    }

    bool isEnabled() const override { return false; }

    // Nothing was ever applied, so there is nothing to undo. Reporting true is deliberate:
    // a shutdown path that treats "no proxy to reset" as a failure would log an error on
    // every exit.
    void resetOnShutdown() const override {}

    SystemProxySnapshot capture() const override { return SystemProxySnapshot{}; }

    bool restore(const SystemProxySnapshot& snapshot) const override
    {
        Q_UNUSED(snapshot);
        return true;
    }
};
