#pragma once

#include "common/SystemProxyMode.h"
#include "common/SystemProxySnapshot.h"

// The three decisions the proxy session makes about the system proxy, kept here as pure
// functions so they can be tested without a registry, a core process or a running app.
//
// Why they are separate from the session: the defect that motivated them was a boolean logic
// error, not a missing feature. `cleanupAfterFailedStartup()` used to clear the proxy whenever
// the *configuration asked for it* OR the registry happened to be enabled -- and either of those
// is true for a user who was running their own proxy before starting this app, so one failed
// startup deleted their settings. A decision that can be stated in one line and tested in one
// line does not belong buried in a 60-line teardown routine.

// Should the proxy session apply the system proxy at all? True only when the user asked for it
// AND the startup gate has already confirmed traffic leaves the machine. The gate is deliberately
// not re-derived here; the caller passes its verdict in, because "reachable" is the one outbound
// signal startup is allowed to act on.
inline bool shouldApplySystemProxy(SystemProxyMode configuredMode, bool outboundReachable)
{
    return configuredMode == SystemProxyMode::ForcedChange && outboundReachable;
}

// What a failed startup should do about the proxy.
//
// The single question that matters is "did *we* change it?", which is exactly
// `managedSystemProxyActive`. The configuration's sysProxyType says what the user wanted, not
// what happened; `isSystemProxyEnabled()` says the registry is on, not who turned it on. Neither
// is evidence of ownership, and using either one is what made a failed startup wipe a proxy the
// user had set up themselves.
//
// When we did change it, there are two ways to undo that:
//   Restore -- put back the captured snapshot (the user's own proxy comes back)
//   Clear   -- delete the keys (what the app did before snapshots existed)
// Restore is preferred whenever a restorable snapshot exists. The fallbacks are ordered so a
// missing snapshot degrades to the old behaviour instead of to "do nothing".
enum class FailedStartupProxyAction {
    LeaveAlone,  // we never touched it
    Restore,     // we touched it and know what was there before
    Clear        // we touched it and do not know what was there before
};

inline FailedStartupProxyAction decideFailedStartupProxyAction(
    bool managedSystemProxyActive,
    bool snapshotRestorable)
{
    if (!managedSystemProxyActive) {
        return FailedStartupProxyAction::LeaveAlone;
    }
    return snapshotRestorable ? FailedStartupProxyAction::Restore
                              : FailedStartupProxyAction::Clear;
}

// What a normal stop should do about the proxy. Same ownership rule as above, with the same two
// ways of undoing it.
//
// `systemProxyEnabled` is still consulted, but only to answer a different question: if the
// registry no longer says "enabled", something else already turned the proxy off and rewriting
// the snapshot would undo *that* change instead. It is an AND with ownership, never a substitute
// for it.
inline FailedStartupProxyAction decideStopProxyAction(
    bool managedSystemProxyActive,
    bool systemProxyEnabled,
    bool snapshotRestorable)
{
    if (!managedSystemProxyActive || !systemProxyEnabled) {
        return FailedStartupProxyAction::LeaveAlone;
    }
    return snapshotRestorable ? FailedStartupProxyAction::Restore
                              : FailedStartupProxyAction::Clear;
}

// What startup should do about a system proxy that points at our own port.
//
// The crash case: the process was killed while proxying, so the registry still holds
// 127.0.0.1:<our port> and no core is listening on it any more. The snapshot restore
// is the pre-existing behaviour for that case; the two guards are what keep it from
// touching a proxy that is not a leftover. The registry must mention our own endpoint
// -- a user's own proxy is never ours to remove -- and nothing may be listening on
// the port: a listener means the proxy may be working (another instance, or another
// tool that uses the same port), and removing it would break a working setup.
//
// When the registry does not mention our endpoint, the listening check has nothing to
// say: the snapshot restore is the pre-existing behaviour (put the user's own settings
// back), not a leftover removal.
enum class StartupLeftoverProxyAction {
    LeaveAlone,  // not ours, or may be working
    Restore,     // dead leftover, and we know what was there before
    Disable      // dead leftover, and no snapshot survived to restore
};

inline StartupLeftoverProxyAction decideStartupLeftoverProxyAction(
    const SystemProxySnapshot& current,
    const QString& ourServer,
    bool listening,
    bool snapshotRestorable)
{
    if (!systemProxySnapshotMentionsServer(current, ourServer)) {
        return snapshotRestorable ? StartupLeftoverProxyAction::Restore
                                  : StartupLeftoverProxyAction::LeaveAlone;
    }
    if (listening) {
        return StartupLeftoverProxyAction::LeaveAlone;
    }
    return snapshotRestorable ? StartupLeftoverProxyAction::Restore
                              : StartupLeftoverProxyAction::Disable;
}
