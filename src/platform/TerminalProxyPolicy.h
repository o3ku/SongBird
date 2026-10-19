#pragma once

#include <QString>

#include "platform/ITerminalProxyService.h"

// Pure decision logic for the terminal-proxy environment variables.
//
// Kept separate from WindowsTerminalProxyService for the same reason
// SystemProxyOwnershipPolicy is separate from WindowsSystemProxyService: the write path touches
// HKEY_CURRENT_USER\Environment and broadcasts a window message, neither of which a unit test can
// exercise, but the decisions around it are ordinary string logic that can and should be pinned.
namespace TerminalProxyPolicy {

// The proxy URL written into HTTP_PROXY / HTTPS_PROXY / ALL_PROXY.
//
// Always `http://`, never `socks5://`: ALL_PROXY is the only variable a consumer might read as SOCKS,
// and most consumers that read ALL_PROXY treat a bare host:port as HTTP anyway. The app's local
// listener accepts HTTP CONNECT, so an http:// URL is correct for all three.
QString proxyUrl(int httpPort);

// The default bypass list, used only when NO_PROXY is not already set.
//
// Covers loopback in the spellings different tools accept (`localhost`, `127.0.0.1`, `::1`) plus the
// RFC1918 ranges, because routing a LAN address through the proxy is never what a user wants and
// costs them access to their own printer or NAS.
QString defaultNoProxy();

// Decides whether a captured snapshot should be restored on shutdown.
//
// Restoring an *invalid* snapshot is a no-op, and restoring one that points at our own port would
// put back a dead endpoint -- the same reasoning as the system-proxy snapshot, so the two stay
// consistent.
bool shouldRestoreOnShutdown(const TerminalProxySnapshot& snapshot, int httpPort);

// Decides whether a value already present in the environment is ours.
//
// Used on startup: if HTTP_PROXY already equals what this app would write for `httpPort`, a previous
// session died without restoring, and the value is ours to clean up rather than the user's to keep.
bool isOurValue(const QString& value, int httpPort);

} // namespace TerminalProxyPolicy
