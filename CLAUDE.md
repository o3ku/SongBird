# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

v2rayq is a pure Qt/C++ rewrite of v2rayN (a Windows proxy/GUI client for V2Ray/Xray). It provides full compatibility with v2rayN's `guiNConfig.json` format while being implemented in Qt/C++ with C++20. The upstream C# reference implementation lives in `ref/v2rayn/`.

## Build Commands

```bash
# Configure + build using presets (vcpkg toolchain at D:/vcpkg)
cmake --preset msvc-debug
cmake --build --preset msvc-debug --parallel

# Configure + build with tests
cmake --preset msvc-debug-test
cmake --build --preset msvc-debug-test --parallel

# Run all tests
ctest --preset msvc-debug-test

# Run a single test by name
ctest --preset msvc-debug-test -R share-url-transports
ctest --preset msvc-debug-test -R clash-config-writer

# Manual configure without presets (requires Qt prefix path)
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<Qt-path>
cmake --build build --target qt_v2rayn --parallel

# Package (Windows PowerShell)
pwsh -File scripts/package-windows.ps1 -QtPrefixPath <Qt-path>
```

Output binary is named `v2rayq.exe`. Tests are opt-in via `-DBUILD_TEST=ON`.

## Test Names

Each test is a separate executable using QtTest. CTest names (for `-R` filter):
`share-url-transports`, `clash-config-writer`, `client-config-writer-tun-compat`, `add-server-dialog-roundtrip`, `settings-dialog-download`, `tun-settings-apply-decision`, `startup-admin-elevation`, `global-hotkey-dialog-roundtrip`, `windows-global-hotkey-binding-mapping`, `windows-global-hotkey-service-pause`, `main-window-log-scroll`, `tun-compat-core-requirement`, `json-config-repository-defaults`, `config-file-import-parser-stream-settings`

## Architecture

### Dependency Wiring

`AppBootstrap` is the composition root — it owns all services as `unique_ptr` members and wires them to `MainWindow` via signal/slot connections in `wireMainWindow()`. There is no DI container; dependencies are constructed in the `AppBootstrap` constructor and passed explicitly.

### Layer Structure

| Layer | Directory | Role |
|-------|-----------|------|
| App | `app/` | Composition root (`AppBootstrap`), entry point, startup logic |
| UI | `ui/` | Qt widgets: dialogs, main window, table models, tray |
| Services | `services/` | Business logic: servers, subscriptions, speed tests, statistics |
| Runtime | `runtime/` | Core process lifecycle, config writers (Client/Server/Clash) |
| Subscription | `subscription/` | Share URL build/parse, subscription content parsing |
| Domain | `domain/models/` | Plain structs: `Config`, `VmessItem`, `SubItem`, `RoutingItem` |
| Persistence | `persistence/` | `JsonConfigRepository` loads/saves `guiNConfig.json` |
| Platform | `platform/windows/` | Windows-specific: system proxy, PAC server, global hotkeys, auto-run, single-instance |
| Common | `common/` | Small value types: `OperationResult`, `SystemProxyMode`, formatters |

### Data Flow

1. `JsonConfigRepository` loads `guiNConfig.json` into `Config` struct at startup
2. `ServerService` manages server list; `RoutingService` manages routing rules
3. On core start: `ClientConfigWriter` (Xray/V2Ray) or `ClashConfigWriter` (Clash/Meta) generates runtime JSON
4. `QtCoreProcessHost` spawns the core process via `QProcess`
5. `GrpcStatisticsBackend` or `ClashRestStatisticsBackend` polls traffic stats
6. All changes persist via `JsonConfigRepository::save()`

### TUN Sidecar Pattern

When TUN mode is enabled with an Xray core, a sing-box sidecar process handles the TUN adapter (the `singbox_tun` Wintun device). `AppBootstrap` manages a second `QtCoreProcessHost`/`CoreLifecycleService` pair for this auxiliary core. The decision logic lives in `TunCompatCoreRequirement.h`.

### Key Patterns

- Return type for fallible operations: `OperationResult` (success bool + message + requiresRestart flag)
- Enums: `CoreType`, `ConfigType`, `SystemProxyMode` — plain `enum class` with free `inline` helper functions in the same header
- Domain models are plain structs with no methods (no inheritance, no virtuals)
- Interface abstraction only where needed: `ICoreProcessHost`, `IConfigRepository`
- Header-only logic for pure decision functions (e.g., `TunSettingsApplyDecision.h`, `TunCompatCoreRequirement.h`, `StartupAdminElevation.h`) — these are easily unit-tested without linking the full app
- Tests compile only the sources they need (listed explicitly in `tests/CMakeLists.txt`), not the whole app
- Headers use `#pragma once`

### Core Types and Config Types

`CoreType` (which proxy engine): Auto/Xray, V2Fly, SingBox, Clash, ClashMeta, Hysteria, NaiveProxy
`ConfigType` (which protocol): VMess, VLESS, Shadowsocks, Trojan, Socks, HTTP, Hysteria2, TUIC, WireGuard, AnyTLS, Naive, Custom

### gRPC Statistics

Proto file: `src/proto/Statistics.proto`. Generated sources go to `${CMAKE_BINARY_DIR}/generated/proto`. Controlled by `QT_V2RAYN_ENABLE_GRPC` CMake option (ON by default, auto-detected).

### CLI Flags

The app supports `--config <path>`, `--auto-start`, `--start-hidden`, `--skip-core`, `--non-interactive`, `--quit-after-ms <ms>`, `--disable-single-instance`, `--disable-global-hotkeys`. The `--non-interactive` and `--skip-core` flags are useful for testing.

## Conventions

### Testing

When adding a new test, create a standalone `.cpp` file in `tests/`, list only the source files it needs in `tests/CMakeLists.txt`, and register it with `add_test(NAME <name> COMMAND <executable>)`. Use the `QT_QPA_PLATFORM=windows` environment property. Follow the existing pattern for the Qt version conditional (`qt_add_executable` vs `add_executable`).

### Upstream Parity

The `.codexpotter/kb/` directory tracks parity gaps between this Qt rewrite and the upstream C# v2rayN. When adding or fixing features, check the KB for known gaps in the relevant area. The `ref/v2rayn/` directory contains the upstream reference implementation for comparison.

## Build Requirements

- CMake 3.24+, Ninja, C++20 (MSVC on Windows)
- Qt 5 or Qt 6 (Widgets, Network)
- vcpkg with triplet `x64-windows-static-md` (presets expect `D:/vcpkg`)
- Protobuf + gRPC (optional, for statistics backend)
