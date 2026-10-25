# SongBird

[English](README.md) · **简体中文**

**用 Qt/C++ 编写的轻量原生代理客户端，支持 Windows 与 macOS —— 一个 V2Ray / Xray / sing-box / Mihomo 前端，不含 Electron、不含 .NET、不含 WebView2。**

SongBird 在一个小型桌面程序里管理 V2Ray 系代理服务器 —— VMess、VLESS、Shadowsocks、Trojan、Hysteria2、TUIC、WireGuard、AnyTLS、Naive、SOCKS 与 HTTP。它可以导入订阅链接、开关 Windows 或 macOS 系统代理、执行 URL 测试与延迟（测速）测试，并驱动三个可互换的核心：**Xray**、**sing-box** 与 **Mihomo**。

它是原生程序而非内嵌浏览器，因此启动快，平时安静地待在托盘里。所有配置都放在一个 JSON 文件里，备份或换机器就是复制一个文件。界面提供英文与简体中文，支持浅色与深色主题。

![SongBird 主窗口，服务器按订阅分组，并显示每个节点的延迟结果](docs/screenshots/main-window.png)

服务器按订阅分组，结果列显示每个节点最后一次测量的数值 —— 红色的 `Timeout` 与一个高速节点同样醒目。下方的日志面板显示核心自身的输出。

<details>
<summary>设置</summary>

![设置对话框，可见常规页以及订阅、路由、核心、TUN、DNS 标签页](docs/screenshots/settings-general.png)

</details>

## 下载

从 [最新发布](https://github.com/o3ku/SongBird/releases/latest) 下载对应平台的版本。三个都是绿色版 —— 无安装程序、无运行时依赖、不写注册表。

| 平台 | 文件 |
|---|---|
| Windows | `songbird.exe` |
| Windows 离线包 | `SongBird-<version>-offline-win-x64.zip` |
| macOS | `SongBird-macos-universal.dmg` |

- 离线包还包含 sing-box 核心及其规则集数据库。`songbird.exe` 本身不捆绑核心，首次启动会提示下载一个；离线包是给无法访问 GitHub 的机器准备的 —— 解压到 **`SongBird.exe` 旁边**，所需文件就已就位，之后仍可通过设置更新。
- macOS 的 DMG 是 **通用包**，同一个文件在 Apple 芯片与 Intel 机器上都能运行。
- 两个包都由 CI 从打标签的那个提交构建。它们都不发送遥测：唯一的对外流量是代理本身，加上订阅、核心、地理数据下载以及手动检查更新。

## 功能

**服务器与订阅**

- 添加、编辑、删除、排序与筛选服务器，按订阅分组显示。
- 从剪贴板或分享链接导入服务器；复制分享链接并预览其二维码。
- 更新订阅，执行 URL 测试与延迟（测速）测试。
- 订阅标签显示已用流量和总额度；悬停仅显示已用、总量、剩余流量和下次重置时间，上传下载用量、到期日期和数据更新时间等完整信息可从标签右键菜单的“订阅详情”查看。数据由服务商提供，随订阅更新刷新。
- 显示运行中的代理实际出口位置，因此「节点在线但不工作」这种情况会直接暴露出来。

**运行代理**

- 一个开关即可启动与停止核心，主窗口内实时显示日志。
- 首次启动时若找不到任何核心，SongBird 会提示下载一个。
- 开关 **Windows** 或 **macOS** 系统代理、开机自启、最小化到托盘、启动时隐藏。
- Windows 上支持 **TUN 模式** 与 UWP 回环豁免。
- 配置路由规则与 DNS 行为：分别设置远端、直连与引导 DNS（含 DNS-over-HTTPS 端点）、直连与代理流量的域名策略、hosts 表以及期望的直连 IP。
- 单实例保护，以及在需要特权的功能上提示提权。

**日常维护**

- 下载、切换与更新核心程序（Xray、sing-box、Mihomo）及其所需的地理数据。
- 检查程序更新。
- 备份与恢复配置。

**一份代码，两个前端**

- **SongBird** —— 手动客户端，也是发布版提供的那个。
- **SongBirdAuto** —— 自行挑选可用节点（先到先得或最低延迟）并保持运行。用同一份源码构建，但尚未作为发布资产提供。

## 核心与协议

三个核心可互换，SongBird 按协议选择默认核心。并非每个协议都能在每个核心上运行：

| 协议 | Xray | sing-box | Mihomo |
|---|:---:|:---:|:---:|
| VMess、VLESS、Trojan、Shadowsocks、Socks、HTTP、Hysteria2 | ✓ | ✓ | ✓ |
| WireGuard | ✓ | ✓ | — |
| TUIC、AnyTLS、Naive | — | ✓ | — |
| 自定义 | ✓ | ✓ | ✓ |

- AnyTLS、Naive 与 TUIC 只有 sing-box 支持，因此使用这些协议的服务器需要 sing-box 核心。
- Mihomo 不支持 WireGuard。
- 当多个核心都能运行某个协议时，优先 sing-box，其次 Mihomo，最后 Xray。
- 生成的 sing-box 配置在 **sing-box 1.13.x 与 1.14.x** 上都能加载。sing-box 1.14 移除了 DNS 规则动作里的 `strategy` 选项并引入 `preferred_by` 取而代之，因此生成器两者都不输出：每条规则的策略折叠进全局 `dns.strategy`，hosts 表则通过一条普通的精确域名规则列表实现。`independent_cache` 同样被去掉，因为 1.14 已弃用它，而 1.16 将移除它。

## macOS

macOS 支持是新增的。CI 会在 macOS 上构建、签名、启动程序并运行测试套件，因此构建损坏或启动失败无法发布出去 —— 但 CI 无法确认系统代理或登录项在某台具体的 Mac 上真的生效了。如果那里有问题，请附上「关于」对话框中的版本号提交 issue。

## 从源码构建

SongBird 需要 CMake、C++20 编译器与 Qt 5.15。CI 工作流是可用配置的参考：`.github/workflows/release.yml` 里 Windows 用 vcpkg 静态链接 Qt，macOS 用 Homebrew 的 `qt@5`。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

加上 `-DBUILD_TEST=ON` 可构建测试套件，用 `ctest --test-dir build` 运行。

## 许可证

SongBird 采用 GNU 通用公共许可证第 3 版，与上游 v2rayN 项目的许可条款相同。完整文本见 [LICENSE](LICENSE)。

## 致谢

- [v2rayN](https://github.com/2dust/v2rayN) —— SongBird 沿用的模式：一个原生客户端驱动多个核心。
- [sing-box](https://github.com/SagerNet/sing-box)
- [Xray](https://github.com/XTLS/Xray-core)
- [Mihomo](https://github.com/MetaCubeX/mihomo)
