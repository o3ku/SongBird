#pragma once

#include <optional>

#include <QString>

// What the system proxy looked like *before* this program touched it.
//
// Why this type exists: the app used to write HKCU\...\Internet Settings and, on exit, delete
// ProxyServer / ProxyOverride outright. A user who was running a company proxy (or another
// client) when SongBird started lost that configuration on the next quit -- the app had no
// concept of "what was here before me", so "restore" could only mean "clear".
//
// The three fields use std::optional rather than QString on purpose. There are two distinct
// states that a plain string cannot tell apart, and restoring the wrong one is visible to the
// user:
//
//   nullopt      -- the value did not exist. Restoring means *deleting the key*.
//   ""           -- the value existed and was empty. Restoring means *writing an empty value*.
//
// Writing "" for the first case leaves behind a ProxyServer key the user never had; deleting
// for the second removes a key that was there. std::optional says which of the two a snapshot
// holds in the type, rather than leaving it to a comment and a convention.
//
// `autoConfigUrl` is carried here because a PAC script takes precedence over
// ProxyEnable/ProxyServer. A snapshot that ignored it could not restore a machine that was
// using one -- and the enable path clears it (see WindowsSystemProxyService), so it has to be
// put back or the user's PAC is silently gone.
struct SystemProxySnapshot {
    // False until capture() has run. A default-constructed snapshot must never be restored:
    // doing so would mean "delete everything", which is exactly the failure this type fixes.
    bool valid = false;
    bool enabled = false;
    std::optional<QString> server;
    std::optional<QString> overrideList;
    std::optional<QString> autoConfigUrl;

    bool operator==(const SystemProxySnapshot& other) const
    {
        return valid == other.valid
            && enabled == other.enabled
            && server == other.server
            && overrideList == other.overrideList
            && autoConfigUrl == other.autoConfigUrl;
    }

    bool operator!=(const SystemProxySnapshot& other) const { return !(*this == other); }
};

// What to restore when the captured snapshot already points at *our own* port.
//
// This is the crashed-session case: the process was killed while proxying, so the registry
// still holds 127.0.0.1:<our port>. Restoring that value faithfully would point the system at a
// port nothing is listening on, which reads to the user as "the internet is broken". The only
// safe target is "proxy off".
//
// `server` is the value this program would write ("127.0.0.1:10808"); an empty one disables the
// check, because without it there is nothing to compare against.
SystemProxySnapshot sanitizeSelfReferentialSnapshot(
    const SystemProxySnapshot& snapshot,
    const QString& ourServer);

// True when the snapshot describes a state worth putting back -- i.e. capture() ran and there is
// at least one field to restore. A snapshot that captured "proxy off, no keys set" is still
// restorable (it means "turn the proxy off and leave the keys alone"), so only `valid` gates it.
inline bool isRestorableSystemProxySnapshot(const SystemProxySnapshot& snapshot)
{
    return snapshot.valid;
}
