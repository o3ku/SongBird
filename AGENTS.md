# Repository Guidelines

## Project Structure & Module Organization
`src/` contains the application and is organized by responsibility: `app/` startup and bootstrap, `auto/` the standalone SongBirdAuto front-end (a second application layer that reuses `services/` and `runtime/` the same way `app/` does), `backends/` per-core config fragments and descriptors, `common/` small shared helpers, `domain/models/` config data types, `services/` business logic, `runtime/` core process and config writers, `subscription/` import/export parsers, `ui/` dialogs, models, tray, and main window, `persistence/` JSON config storage, and `platform/windows/` Windows-only integration.

Layering rules, enforced by `scripts/check-backend-boundaries.ps1`: `common/` may be included from anywhere; `backends/` must not include `app/`, `ui/`, `services/`, or `platform/` (it may include `runtime/`); and `app/`, `ui/`, `services/`, `runtime/` must not include `backends/`. Keep pure, I/O-free helpers in their own header (for example `auto/AutoCoordinatorLogic.h`, `auto/AutoCountryInference.h`, `common/EndpointParser.h`) so they can be unit tested without a live config file or process.

`tests/` mirrors these areas with Qt Test executables such as `MainWindowTests.cpp`, `JsonConfigRepositoryTests.cpp`, and `RoutingServiceTests.cpp`. `translations/` stores `.ts` files, `scripts/` contains packaging helpers, and `build/` is generated output and should not be edited by hand.

`AppBootstrap::run()` is a composition root and must stay short: it only calls the per-domain phases in order (`wireCoreServices`, `wireProxyStack`, `wireUiObjects`, `wireUpdateCoordinators`, `wireWorkflowCoordinators`, `wireServerCoordinators`, `wireShutdownHooks`) and then starts the UI. Each phase lives in its own `app/AppBootstrap*Wiring.cpp` and may only construct `objects_` entries and inject callbacks — it must not touch the UI or start background work. Add new objects and callback wiring to the matching `*Wiring.cpp` rather than growing `run()`. Note that `AppBootstrap.cpp` and its `*Wiring.cpp` files belong to the `songbird` target only, not to `SONGBIRD_AUTO_SOURCES`.

The member order in `AppBootstrapObjects.h` is load bearing, not cosmetic. Members are destroyed in reverse declaration order, and each wiring phase hands earlier members into later ones — either as a `Dependencies { T& }` reference or as a `QObject* parent`. Both mean the constructed object must be destroyed before the member it borrows. Declaring such an object *above* the member it borrows makes `~QObject()` delete it first and the owning `std::unique_ptr` free it again, which is a double free at every shutdown. So: declare a borrowed member before its borrowers, and only append new members at the end. `scripts/check-appbootstrap-member-order.ps1` enforces this; references captured inside a lambda body are deferred uses and are deliberately ignored.

## Build, Test, and Development Commands
Use the checked-in CMake presets on Windows.

- `cmake --preset msvc-debug` configures a local Qt5 debug build.
- `cmake --build --preset msvc-debug --parallel` builds the app.
- `cmake --preset msvc-release` configures a local Qt5 release build.
- `cmake --build --preset msvc-release --parallel` builds the release app.
- `cmake --preset msvc-debug -DBUILD_TEST=ON` reconfigures the debug build with test targets enabled.
- `cmake --build --preset msvc-debug --parallel` builds the enabled test targets.
- `ctest --test-dir build/msvc-debug --output-on-failure` runs the full Qt Test suite.
- `pwsh -File scripts/package-windows.ps1 -QtPrefixPath <Qt-path>` creates a Windows package.

Requirements are CMake 3.24+, Ninja, MSVC with C++20, Qt 5, the `QT5_PREFIX_PATH` environment variable, and the vcpkg toolchain at `D:/vcpkg`.

## Coding Style & Naming Conventions
Follow the existing C++ style: 4-space indentation, braces on their own line, `constexpr`/`QStringLiteral` where appropriate, and include ordering similar to `src/app/main.cpp`. Use `PascalCase` for classes (`CoreLifecycleService`), `camelCase` for functions and locals, and keep tests named `*Tests.cpp`. Prefer small, focused changes inside the relevant module instead of cross-cutting rewrites. There is no repo-local formatter config, so match surrounding code exactly.

## Configuration Persistence
The app uses `JsonConfigRepository` for configuration storage. The primary config path is supplied at runtime, commonly as a file such as `songbird.json`. Runtime/UI state is saved separately next to it using the primary file base name plus `.state.json`; for example, `songbird.json` pairs with `songbird.state.json`.

Primary config data is serialized through `JsonConfigSerialization` and related collection, policy, root, TUN, and UI serializers. State-only data is serialized through `JsonConfigStateSerialization`; this includes UI state such as selected tabs and column widths, plus per-server state such as `serverStates[].testResult`. Do not assume legacy names such as `guiNConfig.json` unless current code explicitly references them.

## Testing Guidelines
Tests use Qt Test through CTest. Add or update tests in `tests/` whenever logic changes in `runtime/`, `services/`, `subscription/`, `persistence/`, or UI behavior that already has coverage. Prefer descriptive test names that encode the behavior under test, following existing patterns such as `generateClientConfigsAdds...`. Reconfigure `msvc-debug` with `-DBUILD_TEST=ON` and run `ctest --test-dir build/msvc-debug --output-on-failure` before submitting changes, unless the current task explicitly says not to run tests.

## SongBird Runtime Rules
Treat proxy activation as a staged state machine owned by `AppBootstrap::RuntimePhase`: `EnvironmentCleanup`, `ValidateCoreApplication`, `ValidateCoreConfig`, `ValidateRuntimeResources`, `StartTunRuntime`, `StartCoreProcess`, `CheckOutboundLocation`, and `ApplySystemProxy` lead to `Proxying` or `Stopped`. The core config is validated before the runtime resources because generating the runtime config needs the resolved core info and rule-set validation parses that generated config. Keep user-facing startup checklist wording and runtime phase transitions aligned when changing startup behavior.

Outbound location is a hard startup condition. `queryCurrentServerLocation()` fails startup when no location is detected, and `computeProxyUiState()` only reports `Active` when the runtime is `Proxying`, the core is ready, the system proxy is enabled, and `currentServerLocation_` is non-empty. Do not relax this requirement to make the toolbar appear active.

Runtime UI state should flow from `RuntimeStateSnapshot`. `AppBootstrap::syncStatusIndicators()` builds the snapshot, `RuntimeState::applySnapshot()` emits `snapshotApplied`, and `MainWindow::applyRuntimeState()` updates the UI from that full snapshot. Avoid adding new independent MainWindow booleans for core/proxy state; extend the snapshot or `ProxyUiState` instead.

The START/STOP toolbar state is derived through `ProxyToolbarController` from `ProxyUiState`, outbound location availability, TUN state, existing cores, and the active server. Avoid direct `QAction` state edits for proxy controls outside the controller.

Proxy startup blocks background work. `BackgroundTaskCoordinator` uses `AppBootstrap::isProxyActivationInProgress()` as its blocking predicate, covering the activation phases through `ApplySystemProxy`. New subscription updates, speed tests, imports, or core/resource updates must respect this coordinator instead of bypassing it.

## Settings And Platform Rules
Settings saves go through `evaluateSettingsDialogApplyPlan()` and `AppBootstrap::applySettingsDialogResult()`. Keep the dirty-plan model: unchanged settings should not be saved, and core restarts should be driven by the plan's runtime/TUN decisions rather than by a raw config-file comparison.

TUN startup cleanup is part of environment cleanup. When TUN is enabled, stale `singbox_tun` adapter removal must happen before launching the managed core unless the environment has already been checked in the current startup flow. Preserve the cleanup/resume semantics when changing startup retry or crash-restart behavior.

UWP loopback support is a Windows platform tool, opened from Help, implemented by `UwpLoopbackDialog` and `WindowsUwpLoopbackService`. It must use `CheckNetIsolation.exe` for loopback exemptions, check availability before use, and use the elevated temporary-script path when SongBird is not already running as administrator.

## Commit & Pull Request Guidelines
Recent commits use concise, imperative summaries, for example `Data-driven Core tab with protocol-core compatibility filtering`. Keep commit titles specific to user-visible behavior or architectural intent. Pull requests should include a short problem statement, the chosen approach, test coverage, and screenshots for UI changes. Link any related issue and call out config, packaging, or platform-specific impact.

## Security & Configuration Tips
Treat the primary config file such as `songbird.json`, its derived state file such as `songbird.state.json`, generated runtime configs, and Windows-specific settings as user data. Do not commit local secrets, subscription URLs, saved server test results, UI state, or machine-specific paths. Avoid manual edits under `build/`; regenerate through CMake instead.
