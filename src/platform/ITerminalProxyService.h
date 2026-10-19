#pragma once

#include <optional>

#include <QString>

// The four environment variables this app manages, with `std::optional` so "the variable was
// absent" is distinguishable from "the variable was present and empty". That distinction is load
// bearing for the same reason it is in SystemProxySnapshot: an empty-but-present variable is a real
// state a user can create, and restoring it must not delete the value.
struct TerminalProxySnapshot {
    std::optional<QString> httpProxy;
    std::optional<QString> httpsProxy;
    std::optional<QString> allProxy;
    std::optional<QString> noProxy;

    bool valid = false;
};

// The terminal/process proxy environment variables, reduced to the operations its callers use.
//
// Why it exists: the system proxy is a WinINET setting, which covers IE, Edge, Chrome and anything
// built on the WinINET/WinHTTP stack. It does not cover curl, git, npm, pip, ssh, or a WSL shell --
// those read HTTP_PROXY / HTTPS_PROXY / ALL_PROXY / NO_PROXY from the environment. So a user could
// see "system proxy on" in the app and still watch `git clone` go out unproxied, with nothing on
// screen explaining why.
//
// The three-state contract mirrors ISystemProxyService exactly, and for the same reason: writing
// these variables is a change to the user's environment, so quitting must restore what was there
// rather than delete it. A user who had HTTP_PROXY set for their own proxy before starting SongBird
// must still have it after quitting.
//
// What this does NOT do: change the environment of an already-running process. On Windows the
// variables live in HKEY_CURRENT_USER\Environment and a new process inherits them; the broadcast
// tells already-running programs (Explorer, and anything that listens for WM_SETTINGCHANGE) to
// re-read. A terminal that is already open keeps its copy -- that is a Windows limitation, not a
// choice made here, and the UI says so.
class ITerminalProxyService {
public:
    virtual ~ITerminalProxyService() = default;

    // Sets HTTP_PROXY, HTTPS_PROXY and ALL_PROXY to `proxyUrl`, and NO_PROXY to `noProxy`.
    // An empty `noProxy` leaves any existing NO_PROXY untouched rather than clearing it: the
    // variable usually holds the user's own bypass list, and overwriting it with our defaults would
    // silently route their LAN traffic through the proxy.
    virtual bool enable(const QString& proxyUrl, const QString& noProxy) const = 0;

    // Removes only the variables this app wrote, restoring the ones that existed before.
    virtual bool disable() const = 0;

    // True when HTTP_PROXY or HTTPS_PROXY currently points at `proxyUrl`, i.e. this app's own value
    // is in place. Used on startup to decide whether a previous session left it behind.
    virtual bool isEnabledFor(const QString& proxyUrl) const = 0;

    // Reads the four variables as they are right now. `valid == false` when the platform cannot
    // read them, and a caller that gets one must not restore it.
    virtual TerminalProxySnapshot capture() const = 0;

    // Puts `snapshot` back. Returns false when any write failed.
    virtual bool restore(const TerminalProxySnapshot& snapshot) const = 0;
};
