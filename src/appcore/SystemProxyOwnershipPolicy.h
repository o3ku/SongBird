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
