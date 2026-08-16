# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

SongBird 是面向 Windows 的原生代理 GUI 客户端，使用 Qt5/C++20 开发，不依赖 .NET/Electron/WebView2。配置文件为 `songbird.json`。

构建产出两个可执行文件：

- `SongBird.exe`（target `songbird`）——主程序
- `SongBirdAuto.exe`（target `songbird_auto`）——自动选路程序，源码在 [src/auto/](src/auto/)，有独立的 `main.cpp` 与主窗口

## 构建命令

**CMakePresets.json 未纳入版本控制**（见 [.gitignore](.gitignore)）。新克隆的仓库没有预设，`--preset` 会失败，必须手动配置：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TEST=ON \
  -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build --parallel
```

本地若已有 preset 文件，可用 `cmake --preset msvc-debug` / `msvc-release`；它们通过 `CMAKE_PREFIX_PATH=$env{QT5_PREFIX_PATH}` 定位 Qt，需要设置该环境变量。

唯一的第三方依赖是 **Qt5（Widgets、Network、Svg；测试另需 Test）**，通过 vcpkg 的 `x64-windows-static-md` triplet 静态链接，因此产物是单个 exe。没有 gRPC/Protobuf 依赖。

## 测试

```bash
# 日常跑测（排除 smoke）
ctest --test-dir build -LE smoke --output-on-failure

# 按名称跑单个测试
ctest --test-dir build -R subscription-parser --output-on-failure
```

CTest 名称（权威列表在 [tests/CMakeLists.txt](tests/CMakeLists.txt)，搜 `songbird_add_qt_test`；增删测试时**同步更新本节**）：

`backend-boundaries`、`share-url-transports`、`add-server-dialog-roundtrip`、`settings-dialog-download`、`tun-settings-apply-decision`、`settings-dialog-apply-plan`、`startup-admin-elevation`、`app-bootstrap-tun-runtime`、`proxy-session-state`、`core-update-coordinator`、`runtime-state`、`main-window-log-scroll`、`client-config-writer-tun-compat`、`tun-compat-core-requirement`、`json-config-repository-defaults`、`config-backup-state-document`、`proxy-availability-check`、`speed-test-service-internal`、`subscription-service`、`subscription-url-import-service`、`routing-service`、`system-proxy-mode`、`auto-country-selection`、`auto-country-inference`、`auto-runtime-defaults`、`user-agent`、`app-update-service`、`app-update-check-coordinator`、`core-update-service`、`geo-resource-update-service`、`subscription-parser`、`server-service`、`protocol-core-compat`、`end-to-end-smoke`

两个特殊测试：

- **`backend-boundaries`** 不是 QtTest，而是 PowerShell 脚本 [scripts/check-backend-boundaries.ps1](scripts/check-backend-boundaries.ps1)，**依赖 `rg`（ripgrep）在 PATH 上**，缺失会失败。它不需要编译，无 Qt 环境时也能单独跑：
  ```powershell
  pwsh -NoProfile -File scripts/check-backend-boundaries.ps1 -SourceRoot src
  ```
- **`end-to-end-smoke`** 带 `LABELS "smoke"` 且 `TIMEOUT 7200`，会真实下载核心/订阅并启动进程，**不要包含在常规跑测里**。

## 发布

由 GitHub Actions 完成，见 [.github/workflows/release.yml](.github/workflows/release.yml)。推送 `v*` 标签即触发：vcpkg 装 Qt5 → 构建 Release → `ctest -LE smoke` → `gh release create` 上传 `songbird.exe`。也可用 `workflow_dispatch` 手动触发（不发布）。

首次运行需从源码编译 Qt5（约 1.5 小时），之后命中 `vcpkg_cache` 缓存。缓存用 `cache/restore` + `cache/save`（`if: always()`）分离，确保构建失败时不丢弃已编译产物。

[.codex/skills/songbird-release/SKILL.md](.codex/skills/songbird-release/SKILL.md) 记录了发布流程的约定，其中版本确认与「构建/测试失败不得打 tag 或发布」的 guardrail 仍然适用。注意该文档描述的是本机构建流程，实际构建已迁移到 CI。

版本号权威源是根 [CMakeLists.txt](CMakeLists.txt) 的 `project(SongBird VERSION x.y.z)`，经 `src/CMakeLists.txt` 以 `SONGBIRD_APP_VERSION` 宏注入两个 target。两个 `main.cpp` 里的 `#ifndef SONGBIRD_APP_VERSION` fallback 在正常构建中不可达（宏总是被定义），仅为避免源码中出现互相矛盾的版本数字而保持同步。

## 架构

### 分层结构

| 层 | 目录 | 角色 |
|----|------|------|
| App | [app/](src/app/) | 组合根（`AppBootstrap`）、入口、启动逻辑、代理会话状态机 |
| Auto | [auto/](src/auto/) | `SongBirdAuto.exe` 的独立实现：自动选路协调器、国家推断/选择 |
| UI | [ui/](src/ui/) | Qt 控件；`mainwindow/` 按控制器拆分，`dialogs/` 中 `SettingsDialog` 按页面拆分 |
| Services | [services/](src/services/) | 业务逻辑：服务器、订阅、测速、路由、策略组、配置备份、应用/核心/Geo 资源更新 |
| Runtime | [runtime/](src/runtime/) | 核心进程生命周期、配置写入器、核心目录与描述符注册表（`runtime/core/`） |
| Backends | [backends/](src/backends/) | 各代理内核的具体实现：`xray/`、`singbox/`、`mihomo/` |
| Subscription | [subscription/](src/subscription/) | 分享 URL 构建/解析、订阅内容解析（share-url / sing-box JSON / Clash YAML） |
| Domain | [domain/models/](src/domain/models/) | 纯结构体：`Config`、`VmessItem`、`SubItem`、`RoutingItem` |
| Persistence | [persistence/](src/persistence/) | `JsonConfigRepository` 加载/保存 `songbird.json` |
| Platform | [platform/windows/](src/platform/windows/) | Windows 专属：系统代理、PAC 服务器、全局热键、自启、单例 |
| Common | [common/](src/common/) | 小型值类型：`OperationResult`、`SystemProxyMode`、`DialogUtils`、`GitHubUrls` |

### 架构边界（由 `backend-boundaries` 测试强制）

违反会导致测试失败，不是风格建议：

1. `app/`、`ui/`、`services/`、`runtime/` **不得** `#include "backends/..."` —— 只能通过 `runtime/core/ICoreBackend.h` 抽象访问内核
2. `backends/` **不得** `#include "(app|ui|services|platform)/..."`
3. `runtime/core/xray`、`runtime/core/singbox` 目录不得存在（旧结构，内核实现已移至 `backends/`）

### 内核后端

`CoreType` 只有三个真实内核：**Xray、SingBox、Mihomo**（外加 `Unknown`）。每个内核在 `backends/<name>/<Name>CoreDescriptor.cpp` 中通过静态 `CoreDescriptorRegistration` 自注册，`CoreDescriptor` 声明其 `supportedConfigTypes`、可执行文件名、`protocolPriority`（数值越小越优先：SingBox=10、Mihomo=15、Xray=20）等。

`ICoreBackend`（[runtime/core/ICoreBackend.h](src/runtime/core/ICoreBackend.h)）是内核的统一契约：配置生成（`buildClientRoot`）、启动参数、版本探测、服务器校验、发布仓库等。新增内核 = 加一个 descriptor + 一个 backend 实现，其余各层无需改动。

**descriptor 的 `supportedConfigTypes` 必须与后端实际实现一致。** 曾出现 Xray 声明支持 AnyTLS/Naive 却无实现，导致启动 xray.exe 却喂 sing-box 格式配置。[ProtocolCoreCompat.h](src/runtime/ProtocolCoreCompat.h) 的全部解析逻辑都建立在这份声明可信的前提上。

`ConfigType`（协议）：VMess、VLESS、Shadowsocks、Trojan、Socks、HTTP、Hysteria2、TUIC、WireGuard、AnyTLS、Naive、Custom。

### 依赖装配

[AppBootstrap](src/app/AppBootstrap.h) 是组合根 —— 以 `unique_ptr` 成员持有全部服务，并在 `wireMainWindow()` 中通过 signal/slot 连到 [MainWindow](src/ui/mainwindow/MainWindow.h)。没有 DI 容器；依赖在构造函数中显式构造与传递。跨层但需可测试的回调用 `std::function` 注入（参考 `FunctionRuntimeAdapters`、`IUserFeedback`）。

### 数据流

1. 启动时 `JsonConfigRepository` 将 `songbird.json` 加载到 `Config`
2. `ServerService` 管理服务器列表；`RoutingService` 管理路由规则
3. 启动核心时 `ClientConfigWriter` 依据 `resolveSelectedCoreType()` 选出内核，委托给对应 `ICoreBackend::buildClientRoot()` 生成运行时配置
4. `QtCoreProcessHost` 通过 `QProcess` 启动核心进程
5. 所有变更通过 `JsonConfigRepository::save()` 持久化

### 配置持久化

主配置 `songbird.json` 通过 `JsonConfigSerialization` 序列化；运行时/UI 状态存于同目录的 `.state.json` 边车（如 `songbird.state.json`），通过 `JsonConfigStateSerialization` 序列化，包含 UI 状态（选中标签页、列宽）和每服务器状态（`serverStates[].testResult`）。

### 运行时状态机

代理激活由 [ProxySession](src/app/ProxySession.h) 的 `Phase` 驱动：`Stopped` → `EnvironmentCleanup` → `ValidateCoreApplication` → `ValidateRuntimeResources` → `ValidateCoreConfig` → `StartTunRuntime` → `StartCoreProcess` → `CheckOutboundLocation` → `ApplySystemProxy` → `Proxying`（或 `Stopping`）。

关键不变量：

- **Outbound location 是硬启动条件**：`queryServerLocation()` 在无法检测位置时会使启动失败；`computeProxyUiState()`（[RuntimeStateSnapshotBuilder.cpp](src/app/RuntimeStateSnapshotBuilder.cpp)）仅在运行时为 `Proxying`、核心就绪、系统代理已启用且位置非空时才报告 `Active`
- **UI 状态流经 RuntimeStateSnapshot**：`AppBootstrap::syncStatusIndicators()` 构建快照 → `RuntimeState::applySnapshot()` 发射 `snapshotApplied` → `MainWindow::applyRuntimeState()` 更新 UI。不要为核心/代理状态新增独立的 MainWindow 布尔值，应扩展快照或 `ProxyUiState`
- **START/STOP 工具栏状态**由 `ProxyToolbarController` 从 `ProxyUiState` 派生，不要在控制器外直接编辑 `QAction` 状态
- **代理启动阻塞后台任务**：`BackgroundTaskCoordinator` 使用 `AppBootstrap::isProxyActivationInProgress()` 作为阻塞谓词。新的订阅更新、测速、导入或资源更新必须尊重此协调器
- **缺失内核会自动下载并续跑启动流程**（`ProxySession::downloadMissingCoreAndResume()`），而非直接失败

### TUN 边车模式

启用 TUN 且使用 Xray 时，用 sing-box 边车进程处理 TUN 网卡（`songbird_tun` Wintun 设备；`singbox_tun` 是遗留名，仍保留在清理列表中）。`AppBootstrap` 为此辅助核心管理第二组 `QtCoreProcessHost`/`CoreLifecycleService`。决策逻辑见 [TunCompatCoreRequirement.h](src/runtime/TunCompatCoreRequirement.h)。Mihomo 原生支持 TUN，不需要边车（其 `auxiliaryTunCoreTypes` 为空）。

### 关键模式

- 可失败操作返回 `OperationResult`（success + message + requiresRestart）
- 枚举为普通 `enum class`，同 header 内提供 `inline` 自由辅助函数
- Domain 模型是纯结构体，无方法、无继承、无虚函数
- 仅在必要处使用接口抽象：`ICoreBackend`、`ICoreProcessHost`、`IConfigRepository`、`IUserFeedback`
- **纯决策函数采用 header-only**，便于在不链接整个 app 的情况下单测：[TunSettingsApplyDecision.h](src/app/TunSettingsApplyDecision.h)、[TunCompatCoreRequirement.h](src/runtime/TunCompatCoreRequirement.h)、[CoreLaunchCompatDecision.h](src/runtime/CoreLaunchCompatDecision.h)、[StartupAdminElevation.h](src/app/StartupAdminElevation.h)
- 每个测试只编译它需要的源文件（在 [tests/CMakeLists.txt](tests/CMakeLists.txt) 中显式列出），不编译整个 app
- 头文件使用 `#pragma once`

### CLI 参数

`--config <path>`、`--start-hidden`、`--skip-core`、`--non-interactive`、`--quit-after-ms <ms>`、`--disable-single-instance`。`--non-interactive` 和 `--skip-core` 在测试中很有用。

## 约定

### 测试

新增测试时：在 [tests/](tests/) 下创建独立 `.cpp`，在 [tests/CMakeLists.txt](tests/CMakeLists.txt) 中**仅列出它依赖的源文件**，用 `songbird_add_qt_test(<target> <ctest-name> SOURCES ... LIBRARIES ...)` 注册（该函数已统一处理 Qt5/Qt6 差异、include 路径、`QT_QPA_PLATFORM=windows` 环境）。测试名要描述被测行为。

### 代码风格

4 空格缩进、左大括号独占一行、`PascalCase` 类名、`camelCase` 函数与局部变量、测试文件命名 `*Tests.cpp`。适当使用 `constexpr`/`QStringLiteral`。仓库没有格式化工具配置 —— **完全匹配周围代码风格**。代码文件中只用英文，仅在必要处加注释。

### 改动尺度

**优先在相关模块内做小而专注的改动，避免跨切割重写。** 即便发现周边代码有改进空间，也不要在本次任务中顺手重构 —— 留作独立 PR。

### 设置保存

通过 `evaluateSettingsDialogApplyPlan()`（[SettingsApplyCoordinator.cpp](src/app/SettingsApplyCoordinator.cpp)）和 `AppBootstrap::applySettingsDialogResult()` 进行。保持 dirty-plan 模型：未变更的设置不应保存，核心重启应由 plan 的 runtime/TUN 决策驱动，而非原始配置文件比较。

### 用户数据

`songbird.json`、生成的运行时配置、Windows 专属设置都属于用户数据 —— 不要提交本地密钥或机器特定路径。不要手动编辑 `build/`，通过 CMake 重新生成。

[AGENTS.md](AGENTS.md) 是权威贡献指南，覆盖风格、PR、提交规范的全部细节；本文件是 Claude Code 自动加载的子集。
