# v2rayq

A lightweight Qt/C++ rewrite of [v2rayN](https://github.com/2dust/v2rayN) — the popular Windows proxy client for V2Ray/Xray.

Drop-in compatible with v2rayN's `guiNConfig.json`. No .NET runtime, no NuGet packages, no DLL hell — a single static-linked native binary.

## Why v2rayq?

### Lower Resource Usage

- **Native C++, no .NET runtime** — no CLR overhead, no JIT warm-up, no runtime package downloads. Static-linked with vcpkg (`x64-windows-static-md`) producing a single self-contained `v2rayq.exe`.
- **Minimal dependencies** — only Qt Core/GUI/Widgets/Network. Optional protobuf+gRPC auto-detected at configure time.
- **Plain struct domain models** — no vtables, no heap allocation for data objects. Trivial copy/move everywhere.
- **O(1) server lookups** — `ServerTableModel` uses hash-based index mapping instead of linear scans through server lists.

### Robust Process Management

- **Orphaned core process cleanup** — PID-file tracking detects and terminates zombie core processes left behind by crashes or forced exits. v2rayN has no equivalent mechanism.
- **Forced-kill timer** — graceful `terminate()` followed by a 1500ms `kill()` escalation. No leaked child processes, even on hang.
- **Stale TUN adapter cleanup** — detects and removes orphaned `singbox_tun` Wintun adapters at startup and after core stop. Prevents network interface accumulation.
- **Thread-safe shutdown** — background threads (speed test, subscription update, core update) are tracked and joined in ordered teardown with lifetime guards preventing use-after-free on async callbacks.

### Simplified Operations

- **One dialog for all protocols** — a unified server dialog that adapts fields dynamically (VMess, VLESS, Shadowsocks, Trojan, Hysteria2, TUIC, WireGuard, AnyTLS, Naive, Custom). v2rayN uses separate dialog forms per protocol.
- **Data-driven settings** — a single tabbed settings dialog replaces v2rayN's scattered forms. The Core tab auto-generates protocol-to-core mapping rows with live version detection.
- **Protocol-core auto-detection** — a compatibility matrix auto-selects the best installed core for each protocol, with sing-box as the default fallback. No manual core type configuration needed.
- **Route-derived proxy exceptions** — automatically extracts `domain:`/`full:` patterns from routing rules with `direct` outbound and adds them to the system proxy bypass list. v2rayN requires manual configuration.
- **Automatic admin elevation** — reads the config before constructing QApplication; if TUN mode is enabled and the process is not elevated, seamlessly re-launches as administrator. No manual elevation step.
- **Process-restart handoff** — coordinated restart (for admin elevation or language change) with PID-based wait and single-instance lock retry. No "port still in use" errors.

### Automation & CLI

- `--config <path>` — custom config file location
- `--auto-start` / `--start-hidden` — launch behaviors
- `--skip-core` / `--non-interactive` — suppress core launch and all blocking dialogs (ideal for CI)
- `--quit-after-ms <ms>` — auto-exit after a timeout
- `--disable-single-instance` / `--disable-global-hotkeys` — opt out of platform features

### Clean Architecture

- **Composition root** — `AppBootstrap` owns all services as `unique_ptr` and wires dependencies explicitly. No singletons, no global state, no service locator.
- **Layered separation** — UI (pure presentation), Services (business logic), Runtime (process lifecycle), Domain (plain data), Platform (Windows-specific). Each layer has clear boundaries.
- **Header-only decision functions** — pure logic (TUN settings, admin elevation, protocol-core compatibility) extracted into testable inline headers with zero linking cost.
- **`OperationResult` return type** — uniform `{success, message, requiresRestart}` for all fallible operations, replacing v2rayN's inconsistent mix of bools, exceptions, and null checks.

### 26 Dedicated Tests

Decision logic, config generation, services, UI dialogs, platform code, and parsing — each test links only the sources it needs for fast, isolated compilation. v2rayN has no comparable test infrastructure.

## Feature Compatibility

| Feature | v2rayq | v2rayN |
|---------|--------|--------|
| Config format | `guiNConfig.json` compatible | Original |
| Server management | Add/edit/duplicate/remove/drag-reorder/filter/QR | Same |
| Protocols | VMess/VLESS/SS/Trojan/Hysteria2/TUIC/WireGuard/AnyTLS/Naive | Same |
| Core engines | Xray, V2Ray, sing-box, Clash, ClashMeta | Same |
| TUN mode | sing-box sidecar with auto-cleanup | sing-box |
| Statistics | gRPC + REST API | Same |
| Speed test | Ping/TCP Ping/Real Ping/Download | Same |
| System proxy | PAC/HTTP/SOCKS with route-derived exceptions | PAC/HTTP/SOCKS |
| Global hotkeys | Yes | Yes |
| Translation | Embedded `.qm` resources, single-file distribution | External files |
| GitHub mirror | Multi-CDN fallback for core downloads | Single source |
| Drag reorder | Yes | Up/Down buttons only |

## Build

```bash
cmake --preset msvc-debug
cmake --build --preset msvc-debug --parallel
```

Both checked-in presets (`msvc-debug`, `msvc-release`) are pinned to Qt5 and expect `QT5_PREFIX_PATH` to be set.

Requirements: CMake 3.24+, Ninja, C++20 (MSVC), Qt 5 (Widgets, Network), vcpkg (`D:/vcpkg`).

## Test

```bash
cmake --preset msvc-debug -DBUILD_TEST=ON
cmake --build --preset msvc-debug --parallel
ctest --test-dir build/msvc-debug --output-on-failure
```

## Package

```powershell
pwsh -File scripts/package-windows.ps1 -QtPrefixPath <Qt-path>
```

## License

This project is licensed under the same terms as the upstream v2rayN project.
