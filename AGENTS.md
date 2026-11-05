# Repository Guidelines

## Language

Filenames, code, comments, and commit messages are English only.

## Project Structure & Module Organization

`src/`: `app/` SongBird composition root; `appcore/` services shared by both front-ends; `auto/` SongBirdAuto front-end; `backends/` per-core config fragments; `common/` helpers; `domain/models/` config data types; `services/` business logic; `runtime/` core process and config writers; `subscription/` parsers; `ui/` dialogs/models/tray/main window; `persistence/` JSON storage; `platform/windows/` Windows integration. `tests/` mirrors these; `build/` is generated — never edit by hand.

- Layering (enforced by `scripts/check-backend-boundaries.ps1`): `common/` includable anywhere; `backends/` must not include `app/ appcore/ ui/ services/ platform/` (may include `runtime/`); `app/ appcore/ auto/ ui/ services/ runtime/` must not include `backends/`; a directory with `main.cpp` (`app/`, `auto/`) is a top of the order — nothing below may include it.
- Shared sources register once in `SONGBIRD_SHARED_SOURCES` (`src/CMakeLists.txt`); single-front-end leftovers go in `SONGBIRD_SOURCES` / `SONGBIRD_AUTO_SOURCES`.
- Keep pure, I/O-free helpers in their own header so they unit-test without a config file or process.
- `AppBootstrap::run()` stays short: `wire*` phases + named steps on `StartupSequencer`. New startup work = a named method registered as a step; new objects/connects go in the matching `app/AppBootstrap*Wiring.cpp` (`songbird` target only).
- `AppBootstrapObjects.h` member order is load-bearing: borrowed members before borrowers, new members appended at the end (double free otherwise).
- Off-GUI-thread continuations post to `this` or test a lifetime token — never a raw QObject pointer (`scripts/check-async-owners.ps1`).
- Colors only in `src/ui/theme/AppThemePalette.cpp` and `src/auto/AutoWindowPalette.cpp` (`scripts/check-hardcoded-colors.ps1`).
- New guard rules get mutation cases in both `scripts/selftest-guards.ps1` and `scripts/python/selftest_guards.py`.

A shared `appcore/` class must not grow a seam that exists for one front end only. The history is ProxySession: `undoManagedSystemProxyAfterAbortedStartup`, `setAvailabilityCheck`'s opt-out, `CoreSwitchConfirmation`'s headless mode and `StartRequest::skipTunCleanup` all exist because SongBirdAuto owns its TUN runtime and starts it before the session, and each one arrived as a separate method or flag rather than as part of a declared boundary — so the shared layer quietly grew frontend-specific surface. The rule going forward: when a seam that serves one front end has to be added, it goes through the declared boundary structure for that class (for the session that is `ProxySession::Dependencies`, the injected callbacks, or `ProxyHealthWatchdog::SessionStrings`), carries a comment naming the front end it serves and what the other front end does by default, and must not be a bare second overload/flag bolted next to the general one. If the seam cannot be expressed that way, that is the signal the shared class wants a per-front-end shell above it, not another switch inside it.

`src/auto/` inherits `src/app/`'s decomposition discipline. The layering checks below stop at "a front end is a top of the dependency order"; they do not prevent a front end from growing a god class. `app/` was decomposed (short `run()`, one `*Wiring.cpp` per phase, small coordinators, `StartupSequencer`) precisely so the composition root stays readable, and `auto/` builds the same kind of product (subscriptions, testing, selection, TUN rollback, recovery) out of the same shared layers. A `src/auto/` class that accumulates more than one of those responsibilities should be split the same way: pure decision logic into its own header (`AutoCoordinatorLogic.h` is the precedent), state-machine orchestration separate from UI. The one selection loop stays in one place (`RoutingScorePolicy` + one caller); `SongBirdAutoCoordinatorStopTests.cpp`'s `createProxySession()` entry must remain constructible without a config file or a core.

New guard rules are written once, in `scripts/python/`, and registered through `tests/CMakeLists.txt`'s `songbird_add_python_guard` — plus a self-test case in `scripts/python/selftest_guards.py`. The `.ps1` originals stay frozen for the rules they already implement (the release job and Windows developers still run them), and the equivalence of the two halves for those rules is checked by `guard-equivalence` (`scripts/python/guard_equivalence.py`, run on a host that has both pwsh and python, i.e. the GitHub Windows runners): for every rule it mutates a throwaway copy of the tree and requires the `.ps1` and the `.py` to report the same violations. A rule changed on one side without the other now fails CI instead of waiting for someone to notice. When a `.ps1` rule has to change, change the port in the same commit and the equivalence job proves they agree.

## Build, Test, and Development Commands

- `cmake --preset msvc-debug` / `cmake --build --preset msvc-debug --parallel` (same for `msvc-release`).
- Tests: `cmake --preset msvc-debug -DBUILD_TEST=ON`, rebuild, `ctest --test-dir build/msvc-debug --output-on-failure`. Run before submitting unless told otherwise.
- Requirements: CMake 3.24+, Ninja, MSVC C++20, Qt 5, `QT5_PREFIX_PATH`, vcpkg at `D:/vcpkg`.
- Releases are GitHub-Actions-only (`v*` tag → `.github/workflows/release.yml`, publishes `SongBird.exe` only). `.cnb.yml` is the Linux PR gate; guard rules are ported to `scripts/python/` there — keep both halves in step.
- Linux builds use `platform/null/` stand-ins for auto-run/system-proxy; macOS keeps real ones. A new platform service needs more than a Windows class.

## Coding Style & Naming Conventions

4-space indent, braces on their own line, `constexpr`/`QStringLiteral` where appropriate, include ordering like `src/app/main.cpp`. `PascalCase` classes, `camelCase` functions/locals, tests named `*Tests.cpp`. No repo formatter — match surrounding code.

## Configuration Persistence

`JsonConfigRepository`; primary config at runtime (e.g. `songbird.json`), state beside it as `<base>.state.json`. Primary data via `JsonConfigSerialization`; state-only data (UI state, `serverStates[].testResult`) via `JsonConfigStateSerialization`. No legacy names like `guiNConfig.json`.

## Testing Guidelines

Qt Test via CTest; add/update tests in `tests/` for changes in `runtime/`, `services/`, `subscription/`, `persistence/`, or covered UI. Descriptive names (`generateClientConfigsAdds...`). The state file is `<primary base name>.state.json` — never hard-code `SongBird.state.json`. Assert the coordinate a widget was asked for, never where `QMenu` actually opened. README screenshots update in the same commit as layout changes (`--skip-core` demo run, crop log panel/status bar); feature list/download table/protocol matrix changes go into both `README.md` and `README_zh_CN.md`.

## SongBird Runtime Rules

- Proxy activation is the `AppBootstrap::RuntimePhase` state machine (`EnvironmentCleanup → … → ApplySystemProxy → Proxying|Stopped`); keep checklist wording aligned with phases.
- An inconclusive node probe (`AutoNodeEvaluation::inconclusive`) is never fed to `RoutingScorePolicy::recordResult()`.
- Node selection goes through `RoutingScorePolicy::shouldSwitch()` (EWMA + breaker + 25% margin + 10 min hold) — never raw rank comparison, no inline cooldowns; active node = `activeServerIdFor(config, activeServerId_)`. Failed health checks are recorded as failures *before* asking whether to move.
- Outbound: `OutboundLivenessProbeService` is the only gate; `OutboundLocationProbeService` is display-only and never feeds `computeProxyUiState()`.
- Runtime UI flows from `RuntimeStateSnapshot`; toolbar state through `ProxyToolbarController` only.
- Background work (subscription updates, speed tests, imports) respects `BackgroundTaskCoordinator` during activation.
- Subscription chain invariants (pinned by tests): parser order (share URLs → sip008 → sing-box → Clash → base64), JSON-parsable Clash documents read as JSON, fresh `QJsonObject` per YAML node, quote-aware `splitKeyValue()`/`stripComment()`, `skippedTypes` union across passes, parse failure ≠ download failure (`failStreak`), conditional requests + ≤3 retries with interruption checks inside the loop, batches ordered not parallel (`SubscriptionUpdateOrder`; `updateInternal()` stays serial).
- `NodeFingerprint::compute()` (coarse, dedup/export) and `SubscriptionService::serverReuseKey()` (fine, test-result carry) are distinct — never merge.
- Export is three layers: stateless `services/ExportService`, `MainWindow` via injected providers (`ui/` never includes `ExportService`), `AppBootstrap` resolving ids. New format = provider + action, never teach the window `VmessItem`.
- Terminal proxy (`ITerminalProxyService`) is implemented and tested but **deliberately not wired** — wiring needs config field + checkbox + plan decision + member + apply/restore calls + startup leftover check, not the checkbox alone.
- `HealthStripPolicy` (24 hour cells, worst-in-hour, strict 24-cell decode) is persisted but not displayed yet — deliberate.
- One version declaration: `project(SongBird VERSION x.y.z)`; never hardcode elsewhere (`tests/` exempt).
- Cores run in a Windows Job Object (handle never closed); PID file + `CoreProcessCleanupService` remain fallback.
- Shutdown is bounded (`BackgroundThreadTracker` + `ThreadShutdown.h`); stuck worker → `abandonProcessAfterStuckThread()`, never unwind.

## Settings And Platform Rules

- Settings saves go through `evaluateSettingsDialogApplyPlan()` + `applySettingsDialogResult()` (dirty-plan model; restarts follow the plan, not file comparison).
- System proxy has an ownership model: `SystemProxySnapshot` (three-state `std::optional<QString>`, captured once, persisted to `.state.json`); teardown decisions come from `SystemProxyOwnershipPolicy.h` — ownership, never `config.sysProxyType` or registry state. Prefer `restore(snapshot)`; `ForcedClear` only when no snapshot survived or the user asked for off.
- An exit has exactly one undo — the session's. A fallback is an *else* branch, never a clear after a stop. Pinned by `tests/ProxySessionTests.cpp` + `tests/SongBirdAutoCoordinatorStopTests.cpp`.
- Auto is single-instance (`SongBirdAuto-single-instance` key; `--disable-single-instance` opts out) and recovers a killed run's proxy from the persisted snapshot.
- macOS reports `capture()` as invalid on purpose — do not synthesize a snapshot.
- TUN enabled ⇒ stale `singbox_tun` adapter removal before launching the managed core.
- UWP loopback uses `CheckNetIsolation.exe`, availability check first, elevated temp script when not admin.

## Commit & Pull Request Guidelines

Concise imperative titles on user-visible behavior or architectural intent. PRs: problem, approach, test coverage, screenshots for UI changes, related issues, config/packaging/platform impact.

## Security & Configuration Tips

Config, state, generated runtime configs and Windows settings are user data: never commit secrets, subscription URLs, test results, UI state, or machine-specific paths.
