#pragma once

// ProxySession's host boundary (documentation only -- no runtime types).
//
// One composition root constructs ProxySession: the app (src/app/AppBootstrap, via
// AppBootstrapProxyWiring.cpp). Headless tests (tests/ProxySessionTests.cpp) construct the
// same class directly, so they are the second host shape. There is no second front end.
//
// The declared boundary structures for this class are: `ProxySession::Dependencies`
// (injected services), `ProxySession::StartRequest` (per-start state), the injected
// callbacks (setCoreSwitchConfirmation / setAvailabilityCheck / setDirectConnectivityCheck),
// and `ProxyHealthWatchdog::SessionStrings` (the satellite's own boundary). A variation a
// host needs goes on one of those structures -- as an injected implementation, a request
// field or a callback -- and carries a comment saying what the default is. A bare method or
// flag bolted next to the general surface is a seam the shared class should not grow.
//
// ---------------------------------------------------------------------------
// Seams on a declared boundary
// ---------------------------------------------------------------------------
//
//  setCoreSwitchConfirmation(CoreSwitchConfirmation)
//      Serves:        the app's modal "Core Compatibility" prompt
//                     (AppBootstrapMainWindowWiring). Unset hosts (tests) accept a core
//                     switch without prompting.
//      Boundary:      injected callback.
//
//  setAvailabilityCheck(AvailabilityCheck, intervalMs)
//      Serves:        the app's real ProxyAvailabilityCheckService probe; tests supply a
//                     scripted answer plus a shortened intervalMs. Without an injected
//                     check no watch arms.
//      Boundary:      injected callback (+ setter parameter for the test-only interval).
//
//  setDirectConnectivityCheck(DirectConnectivityCheck)
//      Serves:        headless tests (a scripted "can this machine reach the probe hosts
//                     directly" answer; no test can stand up the public internet).
//      Default:       the session's real liveness probe runs as the fallback direct probe.
//      Boundary:      injected callback.
//
//  ProxyHealthWatchdog::SessionStrings::isShuttingDown()
//      Serves:        the satellite's "arm no new probe or timer during teardown" guards.
//                     It reads the session's own shuttingDown_ flag through this injected
//                     predicate, so there is no second flag that can be left unset while
//                     the session is going down.
//      Boundary:      SessionStrings.
//
//  StartRequest::showOverlay / warnOnFailure / skipTunCleanup
//      Per-start state. showOverlay drives the startup checklist overlay; warnOnFailure
//      selects the failure wording; skipTunCleanup says the environment was already
//      cleaned by a resume path. All three are set by the app's composition root.
//      Boundary:      StartRequest fields.
//
//  StartRequest::config.ui().activeLocationGroupId + config.clashApiPort/clashApiSecret
//      Group activation: a non-empty group id makes the primary outbound the core's native
//      auto-select group over resolveGroupServers()'s members (anchor first, tag "member-N"
//      by order); the transient Clash API endpoint is picked per group start so the app's
//      ProxyGroupStatusPoller can follow which member the core is on. Both live in the
//      request's config copy and are never serialized from here.
//      Boundary:      request-carried config fields.
//
//  IRuntimeProfileResolver::resolveGroupServers(locationId)
//      Serves:        the group activation member list (app: LocationGroupScan over the
//                     config, custom configs excluded, capped; headless tests: default
//                     "no member" so a group start fails early instead of half-starting).
//      Boundary:      injected resolver call.
//
//  Dependencies (all nine members)
//      Every member is constructed by the app (AppBootstrapServiceWiring.cpp) and by the
//      headless test harness. The difference between hosts lives in the implementations
//      injected, never in extra members.
//      Boundary:      Dependencies itself.
//
//  IProxyActivationCoordinator's snapshot trio (capturedSystemProxySnapshot /
//  ensureSystemProxySnapshotCaptured / restoreSystemProxySnapshot, defaulted in
//  ProxyRuntimeInterfaces.h)
//      Serves:        the app wires all three (AppBootstrapServiceWiring). The defaults
//                     serve hosts that do not manage the system proxy -- headless tests.
//      Boundary:      Dependencies-adjacent interface with defaulted virtuals.
//
// ---------------------------------------------------------------------------
// Bare surface with a single owner (recorded, not seams)
// ---------------------------------------------------------------------------
//
//  adoptManagedSystemProxy(bool) / requestChecklistOverlay() / setServerWarning(QString)
//      Called only by the app's system-proxy, settings and server-switch wiring. They are
//      commands over shared state rather than behaviour switches, so they stay on the
//      session while there is one composition root.
//
//  restartIfRunning() / switchServer() / stopForCoreUpdate()
//      General lifecycle commands over PostStopAction. Not seams.
//
//  handleCoreExitedForTest() / startInternalForTest()
//      Headless-test-only entries. Deliberately not gated on QT_TESTLIB_LIB (the
//      declaration comment explains why).
//
// ---------------------------------------------------------------------------
// Rule cross-reference
// ---------------------------------------------------------------------------
//
// "A shared `appcore/` class must not grow a seam that exists for one front end only. [...]
// when a seam that serves one front end has to be added, it goes through the declared
// boundary structure for that class (for the session that is ProxySession::Dependencies, the
// injected callbacks, or ProxyHealthWatchdog::SessionStrings), carries a comment naming the
// front end it serves and what the other front end does by default, and must not be a bare
// second overload/flag bolted next to the general one. If the seam cannot be expressed that
// way, that is the signal the shared class wants a per-front-end shell above it, not another
// switch inside it."
//     -- AGENTS.md, Repository Guidelines
// With a single front end there is no "other front end" to name; the default a test host
// gets is what each entry above records instead. Keep AGENTS.md's wording and this
// inventory in agreement when either changes.
