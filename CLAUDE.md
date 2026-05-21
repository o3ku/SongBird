# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

SongBird 是面向 Windows 的 V2Ray/Xray 代理 GUI 客户端，使用 Qt/C++20 开发。当前配置文件为 `songbird.json`；上游 C# 参考实现位于 [ref/v2rayN/](ref/v2rayN/)。

## 构建命令

```bash
# 使用预设配置 + 构建（vcpkg 工具链位于 D:/vcpkg）
cmake --preset msvc-debug
cmake --build --preset msvc-debug --parallel

# 启用测试构建
cmake --preset msvc-debug -DBUILD_TEST=ON
cmake --build --preset msvc-debug --parallel

# 运行全部测试
ctest --test-dir build/msvc-debug --output-on-failure

# 按名称运行单个测试
ctest --test-dir build/msvc-debug -R share-url-transports --output-on-failure
ctest --test-dir build/msvc-debug -R client-config-writer-tun-compat --output-on-failure

# 不使用预设的手动配置（需要 Qt 前缀路径）
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<Qt-path>
cmake --build build --target songbird --parallel

# 打包（Windows PowerShell）
pwsh -File scripts/package-windows.ps1 -QtPrefixPath <Qt-path>
```

CMake target 名为 `songbird`，`OUTPUT_NAME` 设为 `SongBird`，所以打包产物是 `SongBird.exe`。测试通过 `-DBUILD_TEST=ON` 开启。

可用预设：`msvc-debug` 和 `msvc-release`。两者都通过 `SONGBIRD_FORCE_MAJOR_VERSION=5` 强制使用 Qt5，需要 `QT5_PREFIX_PATH` 环境变量，并固定 MSVC 2019 (`14.29.30133`) 和 vcpkg `x64-windows-static-md` triplet。

## 测试列表

每个测试是独立的 QtTest 可执行文件。当前 CTest 名称（用于 `-R` 过滤）：

`share-url-transports`、`add-server-dialog-roundtrip`、`settings-dialog-download`、`tun-settings-apply-decision`、`settings-dialog-apply-plan`、`startup-admin-elevation`、`app-bootstrap-tun-runtime`、`app-bootstrap-ui-command-plan`、`global-hotkey-dialog-roundtrip`、`windows-global-hotkey-binding-mapping`、`windows-global-hotkey-service-pause`、`main-window-log-scroll`、`client-config-writer-tun-compat`、`tun-compat-core-requirement`、`json-config-repository-defaults`、`config-file-import-parser-stream-settings`、`server-config-writer`、`proxy-availability-check`、`speed-test-service-internal`、`subscription-service`、`routing-service`、`system-proxy-mode`、`core-update-service`、`geo-resource-update-service`、`subscription-parser`、`server-service`、`protocol-core-compat`、`end-to-end-smoke`。

特别注意：

- `end-to-end-smoke` 带 `LABELS "smoke"` 且 `TIMEOUT 7200`（2 小时），会真实下载核心/订阅并启动进程，**不要默认包含在常规跑测里**。要显式排除可用 `ctest -LE smoke`，要单独跑用 `ctest -R end-to-end-smoke`。
- 权威列表在 [tests/CMakeLists.txt](tests/CMakeLists.txt)（搜 `add_test(NAME ...)`）；增删测试时**同步更新本节**。

## 架构

### 依赖装配

[AppBootstrap](src/app/AppBootstrap.h) 是组合根 —— 以 `unique_ptr` 成员持有全部服务，并在 `wireMainWindow()` 中通过 signal/slot 将它们连到 [MainWindow](src/ui/mainwindow/MainWindow.h)。没有 DI 容器；依赖在 `AppBootstrap` 构造函数中显式构造与传递。

### 分层结构

| 层 | 目录 | 角色 |
|----|------|------|
| App | [app/](src/app/) | 组合根（`AppBootstrap`）、入口、启动逻辑 |
| UI | [ui/](src/ui/) | Qt 控件：对话框、主窗口、表格模型、托盘；`mainwindow/` 已按控制器拆分（`ServerListController`、`ProxyToolbarController`、`SubscriptionViewController` 等），`dialogs/` 中 `SettingsDialog` 已按页面拆分（Core/Dns/General/Routing/Subscription/Tun） |
| Services | [services/](src/services/) | 业务逻辑：服务器、订阅、测速、统计、路由、策略组、配置备份、核心/Geo 资源更新 |
| Runtime | [runtime/](src/runtime/) | 核心进程生命周期、配置写入器（Client/Server/Clash） |
| Subscription | [subscription/](src/subscription/) | 分享 URL 构建/解析、订阅内容解析 |
| Domain | [domain/models/](src/domain/models/) | 纯结构体：`Config`、`VmessItem`、`SubItem`、`RoutingItem` |
| Persistence | [persistence/](src/persistence/) | `JsonConfigRepository` 加载/保存 `songbird.json` |
| Platform | [platform/windows/](src/platform/windows/) | Windows 专属：系统代理、PAC 服务器、全局热键、自启、单例 |
| Common | [common/](src/common/) | 小型值类型：`OperationResult`、`SystemProxyMode`、formatters |

### 数据流

1. 启动时 `JsonConfigRepository` 将 `songbird.json` 加载到 `Config` 结构体
2. `ServerService` 管理服务器列表；`RoutingService` 管理路由规则
3. 启动核心时：`ClientConfigWriter`（Xray/V2Ray）或 `ClashConfigWriter`（Clash/Meta）生成运行时 JSON
4. `QtCoreProcessHost` 通过 `QProcess` 启动核心进程
5. `GrpcStatisticsBackend` 或 `ClashRestStatisticsBackend` 轮询流量统计
6. 所有变更通过 `JsonConfigRepository::save()` 持久化

### TUN 边车模式

启用 TUN 模式且使用 Xray 核心时，会用 sing-box 边车进程处理 TUN 网卡（`singbox_tun` Wintun 设备）。`AppBootstrap` 为此辅助核心管理第二组 `QtCoreProcessHost`/`CoreLifecycleService`。决策逻辑见 [TunCompatCoreRequirement.h](src/runtime/TunCompatCoreRequirement.h)。

### 关键模式

- 可失败操作的返回类型：`OperationResult`（success bool + message + requiresRestart 标志）
- 枚举：`CoreType`、`ConfigType`、`SystemProxyMode` —— 普通 `enum class`，同 header 内提供 `inline` 自由辅助函数
- Domain 模型是纯结构体，无方法（无继承、无虚函数）
- 仅在必要处使用接口抽象：`ICoreProcessHost`、`IConfigRepository`
- 纯决策函数采用 header-only（如 [TunSettingsApplyDecision.h](src/runtime/TunSettingsApplyDecision.h)、[TunCompatCoreRequirement.h](src/runtime/TunCompatCoreRequirement.h)、[StartupAdminElevation.h](src/app/StartupAdminElevation.h)），便于在不链接整个 app 的情况下做单测
- 每个测试只编译它需要的源文件（在 [tests/CMakeLists.txt](tests/CMakeLists.txt) 中显式列出），不编译整个 app
- 头文件使用 `#pragma once`

### 核心类型与配置类型

`CoreType`（代理引擎）：Auto/Xray、V2Fly、SingBox、Clash、ClashMeta、Hysteria、NaiveProxy
`ConfigType`（协议）：VMess、VLESS、Shadowsocks、Trojan、Socks、HTTP、Hysteria2、TUIC、WireGuard、AnyTLS、Naive、Custom

### gRPC 统计

Proto 文件：[src/proto/Statistics.proto](src/proto/Statistics.proto)。生成源码输出到 `${CMAKE_BINARY_DIR}/generated/proto`。由 CMake 选项 `SONGBIRD_ENABLE_GRPC` 控制（默认 ON，自动探测）。

### CLI 参数

应用支持 `--config <path>`、`--auto-start`、`--start-hidden`、`--skip-core`、`--non-interactive`、`--quit-after-ms <ms>`、`--disable-single-instance`、`--disable-global-hotkeys`。`--non-interactive` 和 `--skip-core` 在测试中很有用。

## 约定

### 测试

新增测试时：在 [tests/](tests/) 下创建独立 `.cpp` 文件，在 [tests/CMakeLists.txt](tests/CMakeLists.txt) 中**仅列出它依赖的源文件**，并用 `add_test(NAME <name> COMMAND <executable>)` 注册。设置 `QT_QPA_PLATFORM=windows` 环境属性。沿用现有 Qt 版本条件模式（`qt_add_executable` vs `add_executable`）。测试名要描述被测行为（参考已有的 `generateClientConfigsAdds...` 风格）。

### 代码风格

4 空格缩进、左大括号独占一行、`PascalCase` 类名（`CoreLifecycleService`）、`camelCase` 函数与局部变量、测试文件命名 `*Tests.cpp`。适当使用 `constexpr`/`QStringLiteral`，include 顺序参考 [src/app/main.cpp](src/app/main.cpp)。仓库没有格式化工具配置 —— **完全匹配周围代码风格**。

### 改动尺度

**优先在相关模块内做小而专注的改动，避免跨切割重写。** 这是 AGENTS.md 中明确的工程约定。即便发现周边代码有改进空间，也不要在本次任务中顺手重构 —— 留作独立 PR。

[AGENTS.md](AGENTS.md) 是权威贡献指南，覆盖了风格、PR、提交规范的全部细节；本文件是 Claude Code 自动加载的子集。

### 上游对齐

[.codexpotter/kb/](.codexpotter/kb/) 目录跟踪本 Qt 重写与上游 C# v2rayN 之间的差距。新增或修复功能时，先查 KB 中相关领域的已知差距。[ref/v2rayN/](ref/v2rayN/) 包含上游参考实现，可对照阅读。

### 用户数据

`songbird.json`、生成的运行时配置、Windows 专属设置都属于用户数据 —— 不要提交本地密钥或机器特定路径。不要手动编辑 `build/`，通过 CMake 重新生成。

## 构建要求

- CMake 3.24+、Ninja、C++20（Windows 上使用 MSVC）
- Qt 5（Widgets、Network）
- vcpkg，triplet `x64-windows-static-md`（预设期望路径 `D:/vcpkg`）
- Protobuf + gRPC（可选，用于统计后端）


