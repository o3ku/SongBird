# SongBird

SongBird is a lightweight native Windows proxy client written in Qt/C++. It started as a rewrite and simplification of [v2rayN](https://github.com/2dust/v2rayN), with a smaller runtime footprint and a single JSON configuration file.

Current version: **2.0.0**

SongBird uses `songbird.json` for user configuration. It does not require the .NET runtime, NuGet packages, or external UI assets at runtime; the application is built as a native `SongBird.exe`.

## Highlights

### Native Runtime

- **Qt/C++ application** built with C++20 and Qt 5.
- **Single native executable** with embedded translations, themes, and SVG resources.
- **Minimal application dependencies**: Qt Core, GUI, Widgets, Network, and Svg.
- **Structured config persistence** split by responsibility for collections, policies, state, TUN, and UI settings.

### Proxy Lifecycle

- **Managed core startup** validates the core application before enabling proxy state.
- **Startup overlay details** show progress such as version checks and download percentage without expanding long URLs into the UI.
- **Safe core switching** stops the current core before starting another selected server.
- **Deferred QProcess cleanup** avoids destroying the process sender from inside the finished-signal callback.
- **System proxy cleanup on normal exit** clears the managed Windows system proxy state.

### TUN Handling

- **Automatic admin elevation** when TUN needs to be enabled and the current process is not elevated.
- **Cancel means no change**: cancelling the elevation prompt leaves TUN off and does not restart the app.
- **Core compatibility checks** keep TUN tied to supported sing-box versions.
- **Stale TUN cleanup** removes orphaned runtime state around core stop/start paths.

### User Interface

- **Light and dark themes** share the same layout metrics and resource set.
- **Central toolbar layout** avoids dock-area padding differences and keeps margins consistent.
- **Server table** keeps the Result column auto-expanding while preserving user-adjusted column widths.
- **Information panels** can be collapsed by clicking the header.
- **Tray menus** support routing and server switching with bounded server lists and ellipsized long names.
- **Active proxy icon** switches to the fire logo while proxy mode is running.

### Server Testing

- **URL test results** are persisted in the sidecar state file next to `songbird.json`.
- **Subscription updates and deletion** synchronize stored test-result data so stale results are removed.
- **Tray switch-server menu** sorts candidates by latency, keeps the current server first, and displays only `xxx ms`, `unavailable`, or blank for each result.
- **Group test action** is available from the server tab context menu.

### Automation & CLI

- `--config <path>`: use a specific `songbird.json` file.
- `--start-hidden`: hide to tray or minimize after startup.
- `--skip-core` / `--non-interactive`: suppress core launch and blocking dialogs for automation.
- `--quit-after-ms <ms>`: exit automatically after a timeout.
- `--disable-single-instance`: opt out of the single-instance guard.

## Feature Compatibility

| Feature | SongBird |
|---------|----------|
| Config format | `songbird.json` |
| Server management | Add, edit, duplicate, remove, reorder, filter, QR import/share |
| Protocols | VMess, VLESS, Shadowsocks, Trojan, Hysteria2, TUIC, WireGuard, AnyTLS, Naive, Custom |
| Core engines | Xray, V2Ray, sing-box, Clash, ClashMeta |
| TUN mode | sing-box with admin elevation and cleanup |
| Speed test | Ping, TCP ping, real ping, download, URL test result persistence |
| System proxy | Managed Windows global proxy on/off |
| Translation | Embedded `.qm` resources |
| Themes | Embedded light and dark QSS themes |
| GitHub mirror | Configurable mirror fallback for core and geo resource updates |

## Build

```bash
cmake --preset msvc-debug
cmake --build --preset msvc-debug --parallel
```

Release build:

```bash
cmake --preset msvc-release
cmake --build --preset msvc-release --parallel
```

The checked-in presets are Windows/MSVC presets pinned to Qt 5 through `SONGBIRD_FORCE_MAJOR_VERSION=5` and expect `QT5_PREFIX_PATH` to be set.

Requirements:

- CMake 3.24+
- Ninja
- MSVC with C++20 support
- Qt 5 with Widgets, Network, and Svg
- vcpkg toolchain at `D:/vcpkg`

## Test

```bash
cmake --preset msvc-debug -DBUILD_TEST=ON
cmake --build --preset msvc-debug --parallel
ctest --test-dir build/msvc-debug --output-on-failure
```

The repository currently contains 28 Qt Test executables covering config persistence, runtime decisions, services, subscription parsing, UI behavior, and an end-to-end smoke path.

## Package

```powershell
pwsh -File scripts/package-windows.ps1 -QtPrefixPath <Qt-path>
```

## License

This project is licensed under the same terms as the upstream v2rayN project.
