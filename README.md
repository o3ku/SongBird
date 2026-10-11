# SongBird

**A lightweight native proxy client for Windows and macOS, written in Qt/C++ — a V2Ray / Xray / sing-box / Mihomo front-end with no Electron, no .NET and no WebView2.**

SongBird manages V2Ray-style proxy servers — VMess, VLESS, Shadowsocks, Trojan, Hysteria2, TUIC, WireGuard, AnyTLS, Naive, SOCKS and HTTP — from a single small desktop application. It imports subscription links, turns the Windows or macOS system proxy on and off, runs server and speed tests, and drives three interchangeable cores: **Xray**, **sing-box** and **Mihomo**.

## Download

| Platform | File | Where |
|---|---|---|
| Windows 10/11 (64-bit) | `songbird.exe` — portable, no installer | [latest release](https://github.com/o3ku/SongBird/releases/latest) |
| macOS (Apple silicon and Intel) | `SongBird-macos-universal.dmg` | [v2.5.0-beta1](https://github.com/o3ku/SongBird/releases/tag/v2.5.0-beta1) — pre-release |

- `songbird.exe` is self-contained. There is nothing to install first: no runtime, no installer, no registry entry.
- The macOS package is a **universal** DMG, so one file runs on both Apple silicon and Intel Macs. It is published as a pre-release while macOS support is still being shaken out.
- Both packages are built by CI from the tagged commit. Neither sends telemetry: the only outbound traffic is the proxy itself, plus subscription, core and geodata downloads and a manual update check.

## Why SongBird

- **Native and small.** A Qt/C++ application — no Electron, no .NET, no WebView2, no bundled browser engine. It starts quickly and stays out of the way in the tray.
- **Three cores behind one interface.** Xray, sing-box and Mihomo are interchangeable, and SongBird picks a sensible default per protocol. Switching a server to another core does not change how the app works.
- **Portable configuration.** Everything lives in one JSON config file, so backing up or moving to another machine is a file copy. There is also a built-in backup and restore.
- **Two front-ends from one codebase.** **SongBird** is the manual client; **SongBirdAuto** selects a working node by itself and keeps it running.
- **English and Simplified Chinese** user interface.
- **Light and dark themes.**

## Features

**Servers and subscriptions**

- Add, edit, delete, reorder and filter servers, grouped by subscription.
- Import servers from the clipboard or from share links; copy a share link and preview its QR code.
- Update subscriptions, and run URL tests and latency (speed) tests.
- Show the outbound location the running proxy actually exits from, so a node that is up but not working is visible as such.

**Running a proxy**

- Start and stop the core with a single toggle, with live logs in the main window.
- Toggle the **Windows** or **macOS** system proxy, and grant UWP loopback exemptions on Windows.
- **TUN mode** on Windows.
- Configure routing rules and DNS behaviour: separate remote, direct and bootstrap DNS servers (including DNS-over-HTTPS endpoints), domain strategy for direct and proxied traffic, a hosts table, and expected direct IPs.
- Run from the tray, start hidden, start at login, and stay in the background.
- Single-instance handling, and an elevation prompt where a feature needs it.

**Housekeeping**

- Download, switch and update the core binaries (Xray, sing-box, Mihomo) and the geodata they need.
- Check for application updates.
- Back up and restore the configuration.

## Core Engines and Protocols

SongBird ships three interchangeable core engines — **Xray**, **sing-box** and **Mihomo** — and resolves a default core per protocol. Not every protocol runs on every core:

| Protocol | Xray | sing-box | Mihomo |
|---|:---:|:---:|:---:|
| VMess, VLESS, Trojan, Shadowsocks, Socks, HTTP, Hysteria2 | ✓ | ✓ | ✓ |
| WireGuard | ✓ | ✓ | — |
| TUIC, AnyTLS, Naive | — | ✓ | — |
| Custom | ✓ | ✓ | ✓ |

- AnyTLS, Naive and TUIC are sing-box only, so servers using them require the sing-box core.
- Mihomo does not support WireGuard.
- When more than one core can run a protocol, sing-box is preferred, then Mihomo, then Xray.
- The generated sing-box config loads on **sing-box 1.13.x and 1.14.x** alike. sing-box 1.14 removed the `strategy` DNS rule action option and introduced `preferred_by` in its place, so the generator emits neither: the per-rule strategies collapse into the global `dns.strategy`, and the hosts table is reached through a plain exact-domain rule list. `independent_cache` is dropped too, since 1.14 deprecates it and 1.16 removes it.

## Supported Platforms

| Capability | Windows | macOS |
|---|:---:|:---:|
| System proxy | ✓ | ✓ |
| Start at login | ✓ | ✓ |
| TUN mode | ✓ | — |
| UWP loopback exemption | ✓ | not applicable |

**A note on macOS.** macOS support is new. CI builds the bundle, signs it, starts the application and runs the test suite on macOS, so a broken build or a failure to launch cannot ship — but CI cannot confirm that the system proxy or the login item really took effect on a particular Mac. If something misbehaves there, please open an issue with the version from the About dialog.

## Applications

- **SongBird** — the main client, and the one shipped in releases.
- **SongBirdAuto** — a second front-end that selects a working node automatically (first-available or lowest-latency strategy) and keeps it running. It is built from the same sources but is not shipped as a release asset yet.

## Building From Source

SongBird needs CMake, a C++20 compiler and Qt 5.15. The CI workflows are the reference for a working setup: `.github/workflows/release.yml` for Windows (vcpkg, statically linked Qt) and macOS (Homebrew `qt@5`).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Add `-DBUILD_TEST=ON` to build the test suite, and run it with `ctest --test-dir build`.

## License

SongBird is licensed under the GNU General Public License v3.0, the same license terms as the upstream v2rayN project. See [LICENSE](LICENSE) for the full text.

## Acknowledgements

- [v2rayN](https://github.com/2dust/v2rayN) — the model SongBird follows: one native client driving several cores.
- [sing-box](https://github.com/SagerNet/sing-box)
- [Xray](https://github.com/XTLS/Xray-core)
- [Mihomo](https://github.com/MetaCubeX/mihomo)
