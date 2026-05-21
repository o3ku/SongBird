# Repository Guidelines

## Project Structure & Module Organization
`src/` contains the application and is organized by responsibility: `app/` startup and bootstrap, `domain/models/` config data types, `services/` business logic, `runtime/` core process and config writers, `subscription/` import/export parsers, `ui/` dialogs, models, tray, and main window, `persistence/` JSON config storage, and `platform/windows/` Windows-only integration. `tests/` mirrors these areas with Qt Test executables such as `MainWindowTests.cpp`, `JsonConfigRepositoryTests.cpp`, and `RoutingServiceTests.cpp`. `translations/` stores `.ts` files, `scripts/` contains packaging helpers, and `build/` is generated output and should not be edited by hand.

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

## Commit & Pull Request Guidelines
Recent commits use concise, imperative summaries, for example `Data-driven Core tab with protocol-core compatibility filtering`. Keep commit titles specific to user-visible behavior or architectural intent. Pull requests should include a short problem statement, the chosen approach, test coverage, and screenshots for UI changes. Link any related issue and call out config, packaging, or platform-specific impact.

## Security & Configuration Tips
Treat the primary config file such as `songbird.json`, its derived state file such as `songbird.state.json`, generated runtime configs, and Windows-specific settings as user data. Do not commit local secrets, subscription URLs, saved server test results, UI state, or machine-specific paths. Avoid manual edits under `build/`; regenerate through CMake instead.
