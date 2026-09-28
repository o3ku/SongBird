# SongBird

## What It Is

SongBird is a lightweight native Windows proxy client built with Qt/C++. It supports common proxy protocols and core engines for everyday proxy usage.

## Advantages

- Native application with no .NET runtime, Electron, or WebView2 dependency.
- Low resource usage and lightweight runtime behavior.
- Small application size with simple dependencies.
- Clean interface that is easy to use.
- JSON-based configuration for easy backup and migration.
- Light and dark theme support.

## Features

- Add, edit, delete, reorder, and filter servers.
- Import from clipboard, copy share links, and preview QR codes.
- Update subscriptions and run server URL tests.
- Toggle the Windows system proxy, and grant UWP loopback exemptions.
- Use TUN mode.
- Configure routing rules and DNS behavior.
- Run from the tray, start hidden, and stay in the background.
- Back up and restore the configuration.

## Core Engines And Protocols

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

## Applications

- **SongBird** — the main client, and the only executable currently published in a release.
- **SongBirdAuto** — a second front-end that selects a working node automatically (first-available or lowest-latency strategy) and keeps it running. It is built from the same sources but is not shipped as a release asset yet.

## Supported Platforms

- Windows

## License

SongBird is licensed under the GNU General Public License v3.0, the same license terms as the upstream v2rayN project. See [LICENSE](LICENSE) for the full text.

## Acknowledgements

- [v2rayN](https://github.com/2dust/v2rayN)
- [sing-box](https://github.com/SagerNet/sing-box)
- [Xray](https://github.com/XTLS/Xray-core)
- [Mihomo](https://github.com/MetaCubeX/mihomo)
- [linux.do](https://linux.do)
