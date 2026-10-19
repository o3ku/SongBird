#pragma once

#include "common/SystemProxySnapshot.h"

#include <QList>
#include <QString>
#include <QStringList>

#include "common/SystemProxyMode.h"
#include "platform/ISystemProxyService.h"

// The macOS system-proxy side effect, written through `networksetup`.
//
// macOS has no single proxy setting: every network service (Wi-Fi, Ethernet, the
// Thunderbolt bridge, ...) carries its own HTTP, HTTPS and SOCKS entries, so "turn the
// system proxy on" means repeating the same commands once per enabled service. That is
// also why update() returns false as soon as one of them cannot be written -- a partially
// applied proxy is the failure mode worth reporting, not a silent one.
//
// `networksetup` rewrites the root-owned network preferences, so a plain invocation fails
// without admin rights. update() therefore runs the commands directly first and only
// falls back to an `osascript ... with administrator privileges` wrapper when that fails.
// The direct attempt keeps the call prompt-free whenever the process already has the
// rights, and the wrapper is what makes the feature work at all for a normal user
// session, at the cost of the standard macOS authorization dialog.
class MacSystemProxyService : public ISystemProxyService {
public:
    MacSystemProxyService() = default;

    bool update(
        SystemProxyMode mode,
        int httpPort,
        int socksPort,
        const QString& proxyExceptions,
        const QString& advancedProtocol) const override;
    bool isEnabled() const override;
    void resetOnShutdown() const override;

    // Always invalid, deliberately.
    //
    // Windows has one proxy setting per user, so a snapshot is four registry values and the
    // restore is exact. macOS has one setting *per network service*, each with its own HTTP,
    // HTTPS, SOCKS and bypass list, and the set of services changes while the app runs (a
    // VPN comes up, Wi-Fi drops). Snapshotting that faithfully means capturing the state of
    // every service and then re-applying it to a service list that may no longer match -- and
    // a partial re-apply is worse than the clear this app has always done.
    //
    // So the contract is: capture() reports "not restorable" and callers fall back to
    // update(ForcedClear), which is exactly the pre-existing behaviour. Doing this in the type
    // rather than by leaving the methods out keeps every platform on the same call path.
    SystemProxySnapshot capture() const override;

    // No-op that reports success, for the same reason: there is never a valid snapshot on this
    // platform, and the fallback in the callers is what actually runs.
    bool restore(const SystemProxySnapshot& snapshot) const override;

private:
    // The services `networksetup` reports as enabled, in its own order. An empty list
    // means the machine currently has no active network service.
    static QStringList enabledNetworkServices();
};

// Windows keeps its proxy exceptions in a single semicolon-separated ProxyOverride value;
// `networksetup -setproxybypassdomains` takes them as separate arguments. Exposed for the
// tests, which is the only reason it is not file-local.
QStringList macProxyBypassDomains(const QString& proxyExceptions);

// The networksetup argv lists that carry `enable` out for every service, in the order they
// have to run. Kept as argv rather than as a shell string so the direct path can hand them
// straight to QProcess and the elevated path can quote them itself.
//
// A free function for the same reason as the converter above, and one more: this file is only
// added to the build on APPLE, so nothing on the other platforms would ever compile it. The
// tests build it everywhere and drive it from here.
QList<QStringList> macProxyCommands(
    const QStringList& services,
    bool enable,
    int httpPort,
    int socksPort,
    const QStringList& bypassDomains);
