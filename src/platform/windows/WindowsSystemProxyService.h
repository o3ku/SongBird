#pragma once

#include <QString>

#include "common/SystemProxyMode.h"
#include "common/SystemProxySnapshot.h"
#include "platform/ISystemProxyService.h"

class WindowsSystemProxyService : public ISystemProxyService {
public:
    // `settingsPath` selects the registry key this service writes; an empty path means the
    // real HKCU\...\Internet Settings key. It is a parameter because the real key may not be
    // mutated by a test run, so without it the class has no reachable coverage at all: a
    // scratch key exercises the success path, and a location that refuses writes is the only
    // way to reach the failure path.
    explicit WindowsSystemProxyService(QString settingsPath = {});

    // `update()` is the only entry point for *applying*, deliberately. It covers both directions
    // through SystemProxyMode (ForcedChange / ForcedClear), so the `enable(...)` / `disable()`
    // wrappers that used to sit here had no callers left and were removed rather than kept as a
    // second way in. SystemProxyCoordinator has its own enable()/disable(), but those are a
    // different class and route through setMode() -> update() as well.
    //
    // update(ForcedClear) deletes ProxyServer/ProxyOverride. That is right when the user asked for
    // the proxy to be off, and wrong when the intent is "undo what we did" -- use restore() for
    // the latter.
    bool update(
        SystemProxyMode mode,
        int httpPort,
        int socksPort,
        const QString& proxyExceptions,
        const QString& advancedProtocol) const override;
    bool isEnabled() const override;
    void resetOnShutdown() const override;

    // Reads ProxyEnable / ProxyServer / ProxyOverride / AutoConfigURL, recording which of them
    // existed. On a non-Windows build `valid` is false.
    //
    // An existing-but-empty key is *not* a failure: it reports "proxy off, no values set", which
    // is a restorable state. QSettings cannot distinguish that from a registry path that does not
    // exist (both read as NoError with no values), so `valid` only means "the read completed",
    // not "the key exists". Restoring such a snapshot turns the proxy off and deletes our own
    // leftovers, which is the right outcome for both readings.
    SystemProxySnapshot capture() const override;

    // Writes back exactly what capture() saw: a key that existed is rewritten (even if empty), a
    // key that did not exist is removed. PAC is restored first, because it takes precedence over
    // ProxyEnable/ProxyServer and the notification below has to read the final state.
    bool restore(const SystemProxySnapshot& snapshot) const override;

private:
    bool setProxy(const QString& proxyServer, const QString& proxyExceptions, bool enabled) const;
    // False when the key could not be removed. A PAC script takes precedence over
    // ProxyEnable/ProxyServer, so a removal that fails silently leaves the system pointing at
    // a PAC URL while this service reports that the proxy was applied or cleared.
    bool clearAutoConfigUrl() const;
    // Writes a value that capture() saw, or removes it when the snapshot recorded nullopt.
    // Returns false when the write (or removal) failed.
    bool applyCapturedValue(const QString& name, const std::optional<QString>& value) const;
    bool notifyProxyChange() const;

    QString settingsPath_;
};
