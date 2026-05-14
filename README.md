# v2rayq

A Qt/C++ fork of [v2rayN](https://github.com/2dust/v2rayN), the popular Windows proxy client for V2Ray/Xray.

## Features

- Full compatibility with v2rayN's `guiNConfig.json` configuration format
- Server management: add, edit, duplicate, remove, default switch, drag reorder, filter, QR toggle, subscription grouping
- Import/export: clipboard, client/server config, share links, subscription content
- Runtime config generation for Xray/V2Ray and sing-box (VMess/VLESS/Trojan/Shadowsocks/Socks)
- Routing, DNS, Reality, auto reload, tray quick switch, core lifecycle management
- Statistics polling with gRPC (Xray/V2Ray) and REST API (sing-box/Clash)
- Speed test: Ping, TCP Ping, Real Ping, Download Speedtest
- Windows platform: tray icon, single-instance, autorun, system proxy (PAC/HTTP/SOCKS), global hotkeys
- TUN mode via sing-box sidecar with Wintun adapter

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
