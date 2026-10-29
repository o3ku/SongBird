#pragma once

// ProxySession's front-end seam inventory (documentation only — no runtime types).
//
// This header is the declared record required by the repository rule in AGENTS.md ("A shared
// `appcore/` class must not grow a seam that exists for one front end only"), applied to the
// class that rule was written about. Two front ends construct ProxySession: the main app
// (src/app/AppBootstrap, via AppBootstrapProxyWiring.cpp) and SongBirdAuto
// (src/auto/SongBirdAutoCoordinator, via createProxySession()). A third host shape is the
// headless test (tests/ProxySessionTests.cpp, tests/SongBirdAutoCoordinatorStopTests.cpp).
//
// The declared boundary structures for this class are: `ProxySession::Dependencies` (injected
// services), `ProxySession::StartRequest` (per-start state), the injected callbacks
// (setCoreSwitchConfirmation / setAvailabilityCheck / setDirectConnectivityCheck), and
// `ProxyHealthWatchdog::SessionStrings` (the satellite's own boundary — the model to follow).
// A seam on one of those structures, with a comment naming the front end it serves and what
// the other front end does by default, is compliant. A bare method or flag next to the general
// surface is not; it is a candidate for a per-front-end shell instead.
//
// Audited 2026-10-29 against the call sites in src/app/, src/auto/ and tests/. Re-run the
// audit by grepping each public member for `proxySession_->` / `proxySession->` in both
// front ends before extending this table.
//
// ---------------------------------------------------------------------------
// A. Seams already living on a declared boundary (compliant — keep them here)
// ---------------------------------------------------------------------------
//
//  setCoreSwitchConfirmation(CoreSwitchConfirmation)
//      Serves:        main app (modal "Core Compatibility" prompt, AppBootstrapMainWindowWiring).
//      Other default: Auto and headless hosts leave it unset, which accepts a core switch
//                     without prompting (the headless mode the declaration comment names).
//      Boundary:      injected callback.
//
//  setAvailabilityCheck(AvailabilityCheck, intervalMs)
//      Serves:        main app (real ProxyAvailabilityCheckService probe); tests (scripted
//                     answer plus shortened `intervalMs`).
//      Other default: Auto leaves it unset — it already runs its own health check every 60 s
//                     and must not get a second one; with no check injected, no watch arms.
//      Boundary:      injected callback (+ setter parameter for the test-only interval).
//
//  setDirectConnectivityCheck(DirectConnectivityCheck)
//      Serves:        headless tests (a scripted "can this machine reach the probe hosts
//                     directly" answer; no test can stand up the public internet).
//      Other default: both front ends leave it unset — the session's real liveness probe runs
//                     as the fallback direct probe.
//      Boundary:      injected callback.
//
//  StartRequest::showOverlay
//      Serves:        main app (startup checklist overlay; also requestChecklistOverlay below).
//      Other default: Auto always passes false (no checklist overlay UI exists there).
//      Boundary:      StartRequest field.
//
//  StartRequest::warnOnFailure
//      Serves:        both front ends, deliberately with different values (main app passes its
//                     setCurrentActivationPending_ flag; Auto always passes true so its own
//                     failure display gets the "Please verify manually" warning).
//      Boundary:      StartRequest field. Recorded because it is the one StartRequest field the
//                     two front ends set differently by design, not because it serves one only.
//
//  StartRequest::skipTunCleanup
//      Serves:        main app historically — the environment-cleanup resume that re-entered
//                     startup passed true ("TUN cleanup already happened, do not redo it").
//                     That resume now lives inside ProxySession (startInternal re-entry), and
//                     today every external caller in both front ends passes false. Kept on
//                     StartRequest because it is the declared way a host says "the environment
//                     was already cleaned" if a caller ever needs it again.
//      Other default: Auto always passes false — Auto owns its TUN runtime and starts it
//                     before the session (issue #8's ordering), so there is nothing to skip.
//      Boundary:      StartRequest field.
//
//  Dependencies (all nine members)
//      Serves:        both front ends construct every member; none is front-end-specific. The
//                     difference lives in the implementations each front end injects: the main
//                     app supplies its concrete services (AppBootstrapServiceWiring.cpp), Auto
//                     supplies FunctionRuntimeEnvironment / FunctionProxyActivationCoordinator
//                     over its own lambdas (SongBirdAutoCoordinator::wire*Environment). That
//                     is exactly the shape the rule wants — differences as injected
//                     implementations, never as extra members.
//      Boundary:      Dependencies itself.
//
//  IProxyActivationCoordinator's snapshot trio (capturedSystemProxySnapshot /
//  ensureSystemProxySnapshotCaptured / restoreSystemProxySnapshot, defaulted in
//  ProxyRuntimeInterfaces.h)
//      Serves:        both front ends wire all three today (main: AppBootstrapServiceWiring;
//                     Auto: wireActivationCoordinator, with the self-referential snapshot
//                     sanitisation). The defaults serve hosts that do not manage the system
//                     proxy — headless tests and future minimal hosts.
//      Boundary:      Dependencies-adjacent interface with defaulted virtuals; recorded here
//                     because it is part of the session's host boundary even though the member
//                     list above stays clean.
//
// ---------------------------------------------------------------------------
// B. Bare surface that exists for one front end (the documented debt)
// ---------------------------------------------------------------------------
//
//  undoManagedSystemProxyAfterAbortedStartup()
//      Serves:        Auto only (SongBirdAutoCoordinator::startProxySession's TUN-failure
//                     branch, issue #8: a TUN failure aborts before ProxySession::start() ever
//                     runs, while the registry may still hold an earlier round's proxy).
//      Other default: the main app never calls it — its TUN runtime is started inside the
//                     session's own startup phases, so a failed startup reaches
//                     cleanupAfterFailedStartup() instead.
//      Status:        bare public command, not expressible as injected data (it triggers a
//                     session-side ownership decision the host cannot make). This is the one
//                     entry that would move into AutoSessionShell if that shell is ever built;
//                     see "Future shells" below.
//
//  adoptManagedSystemProxy(bool)
//      Serves:        main app only — external adoption for registry writes the session itself
//                     did not make (SystemProxyCoordinator's setMode/applyModeOnExit paths,
//                     the settings-apply wiring, and the adopt-managed-system-proxy startup
//                     step).
//      Other default: Auto never calls it; its proxy is always session-applied, so
//                     managedSystemProxyActive_ is written only by the session itself.
//      Status:        bare method. Tolerable while it has a single conceptual owner; it moves
//                     into the main app's shell if one is ever built.
//
//  requestChecklistOverlay()
//      Serves:        main app only (SystemProxyCoordinator: system-proxy enable requested
//                     while an activation is already pending — surface the checklist).
//      Other default: Auto never calls it; no checklist overlay exists there.
//      Status:        bare method, UI-shaped; a main-app-shell candidate.
//
//  setServerWarning(QString)
//      Serves:        main app only (clearing the warning when the user switches servers).
//      Other default: Auto never calls it; its warnings travel through signals.
//      Status:        bare setter on shared state rather than a behaviour switch — low risk,
//         recorded for completeness.
//
//  restartIfRunning() / switchServer() / stopForCoreUpdate()
//      Serves:        main app only as callers (core restart after settings apply, default
//                     server switch, core update pause). Not seams: they are general
//                     lifecycle commands over PostStopAction that one front end happens not
//                     to use. Recorded so the next audit does not re-litigate them.
//
//  handleCoreExitedForTest() / startInternalForTest()
//      Serves:        headless tests only. Deliberately not gated on QT_TESTLIB_LIB (the
//                     declaration comment explains why). Not front-end seams.
//
//  Signals and getters with main-app-only consumers (checklistUpdated, checklistCleared,
//  statusSyncRequested, coreUpdateResumeRequested, serverSwitchResumeRequested; phase(),
//  serverLocation(), serverWarning(), activationId(), isActivationInProgress(),
//  isManagedProxyActive()): shared event/state surface, not seams. Auto consumes the rest
//  (phaseChanged, activated, serverLocationChanged, failed, stopped, coreOutput,
//  auxiliaryCoreOutput, logMessage, systemProxyUndoFailed; isActive(), isCoreRunning(),
//  isTransitioning(), outboundReachable()).
//
// ---------------------------------------------------------------------------
// Future shells
// ---------------------------------------------------------------------------
//
// AutoSessionShell: if the Auto-only surface grows past
// undoManagedSystemProxyAfterAbortedStartup() — a second auto-only entry point of any kind is
// the trigger — the fix is a shell class above ProxySession in src/auto/ that owns the
// TUN-before-session ordering and whatever else accumulates, not a new method or flag on the
// shared session. Until that trigger fires, the single method above stays, documented.
//
// The terminal proxy (see AGENTS.md, "The terminal proxy is implemented but deliberately not
// wired into the app") must NOT add any new ProxySession seam at all when it is wired. It is
// a platform side effect (platform/ITerminalProxyService, environment variables — not the
// WinINET registry the session's ownership decisions are about). Its apply/restore calls
// belong beside the system-proxy apply/restore points in each front end's composition root
// (an AppBootstrapObjects member for the main app, the coordinator's activation wiring for
// Auto). If wiring it ever seems to need ProxySession to know about it, that is a design
// error to stop at, not a seam to add.
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
//     — AGENTS.md, Repository Guidelines
// When adding an entry to either table above, extend this file in the same change and keep
// the AGENTS.md rule's wording and this inventory in agreement.
