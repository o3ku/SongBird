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

`backend-boundaries`、`appbootstrap-member-order`、`backend-contract`、`share-url-transports`、`add-server-dialog-roundtrip`、`settings-dialog-download`、`tun-settings-apply-decision`、`settings-dialog-apply-plan`、`startup-admin-elevation`、`app-bootstrap-tun-runtime`、`proxy-session-state`、`core-update-coordinator`、`runtime-state`、`main-window-log-scroll`、`client-config-writer-tun-compat`、`tun-compat-core-requirement`、`json-config-repository-defaults`、`config-backup-state-document`、`proxy-availability-check`、`speed-test-service-internal`、`subscription-service`、`subscription-url-import-service`、`routing-service`、`system-proxy-mode`、`auto-country-selection`、`auto-country-inference`、`auto-runtime-defaults`、`auto-coordinator-logic`、`user-agent`、`app-update-service`、`app-update-check-coordinator`、`core-update-service`、`geo-resource-update-service`、`subscription-parser`、`server-service`、`protocol-core-compat`、`end-to-end-smoke`

三个特殊测试（均非 QtTest，是 PowerShell 脚本，不需要编译、无 Qt 环境也能单独跑）：

- **`backend-boundaries`** — [scripts/check-backend-boundaries.ps1](scripts/check-backend-boundaries.ps1)，检查分层规则（见 [AGENTS.md](AGENTS.md)）。有 `rg` 时用它加速，没有则回退到等价的纯 PowerShell 扫描，**不再依赖 `rg` 在 PATH 上**。
  ```powershell
  pwsh -NoProfile -File scripts/check-backend-boundaries.ps1 -SourceRoot src
  ```
- **`appbootstrap-member-order`** — [scripts/check-appbootstrap-member-order.ps1](scripts/check-appbootstrap-member-order.ps1)，检查 `AppBootstrapObjects.h` 的成员声明顺序是否满足析构顺序要求（构造时借用另一个成员的对象必须声明在其**之后**）。lambda 体内的引用是延迟使用，有意忽略。
  ```powershell
  pwsh -NoProfile -File scripts/check-appbootstrap-member-order.ps1 -SourceRoot src
  ```
  注意：这两个脚本都以 `exit 1` 结束，**在当前 PowerShell 会话里直接 `& script.ps1` 会把会话一起退出**（输出还没落盘）。要在会话内验证，用 [.workbuddy-ai/tools/run-check-in-runspace.ps1](.workbuddy-ai/tools/run-check-in-runspace.ps1) 把它跑在子 runspace 里。
- **`end-to-end-smoke`** 带 `LABELS "smoke"` 且 `TIMEOUT 7200`，会真实下载核心/订阅并启动进程，**不要包含在常规跑测里**。

**`backend-contract`** 守护 descriptor 声明与后端实现的一致性：遍历每个注册内核 × 其声明的每个协议，断言能生成配置，且两个不同协议不会映射到同一 wire protocol（后者用于捕获「落入 default 分支」的漂移）。新增协议支持时必须同时改 descriptor 与后端实现，否则此测试会失败。它还断言辅助 TUN 根的两种路由形态：`outbounds` 必须是**扁平的对象列表**（嵌套数组正是 sing-box 报 `cannot unmarshal array into Go struct field _Options.outbounds` 而拒载的形态）、中继态含 `proxy` 出站且 `route.final=proxy`、直连态只有 `direct`/`block` 且 `route.final=direct`。

## 发布

由 GitHub Actions 完成，见 [.github/workflows/release.yml](.github/workflows/release.yml)。推送 `v*` 标签即触发：vcpkg 装 Qt5 → 构建 Release → `ctest -LE smoke` → `gh release create` 上传 `songbird.exe`。也可用 `workflow_dispatch` 手动触发（不发布）。

首次运行需从源码编译 Qt5（约 1.4 小时），之后应当命中 `vcpkg_cache`。缓存分两层，各自回答不同的问题：Actions 缓存负责把 vcpkg 的二进制仓库目录在两次运行之间搬运过去，而**某个包能否复用由 vcpkg 按 ABI 哈希逐包判断**——该哈希包含编译器版本、vcpkg 工具版本与端口配方，所以 runner 镜像一更新就会整批失效。镜像之所以能**精确预测**这个指纹，是因为 vcpkg 用的是镜像自带的 `VCPKG_INSTALLATION_ROOT`（`C:\vcpkg`），端口配方随镜像版本走。`cache/restore` 靠前缀 `restore-keys` 取最近一次保存，前缀里带 **runner 镜像版本**作命名空间；`cache/save` 每次运行都用唯一 key（镜像版本-`run_id`-`run_attempt`），带 `if: always()`，并在**仓库未变时跳过上传**（见下）。

⚠️ **`cache/restore` 的 `key` 必须是一个从未被保存过的值**：它做**精确匹配优先**，一旦命中就**不再走 `restore-keys` 前缀**，整个「取最近一次保存」的机制就失效了。这里踩过一次——`key` 曾是老工作流的固定 key `...-v1`，而那条 2026-09-23 的条目一直都在，于是 2026-10-01 与 10-06 两次运行都精确命中它、vcpkg 报 `Restored 0 package(s)`、白编 1h13m，新仓库还存到一个下一轮永远不会看的 key 上。现在 `key` 里嵌了 `run_id`，让 miss 成为结构性的（保存 key 是同一前缀加 `run_id`-`run_attempt`，裸 `run_id` 永不写入；re-run 时 `run_id` 不变、`attempt` 变，仍保证 miss）。诊断特征：`gh cache list` 里所有 per-run 条目的 `lastAccessedAt == createdAt`（从未被读过），只有那条固定 key 的 `lastAccessedAt` 在动。

⚠️ **前缀里必须带 runner 镜像版本**，否则**镜像滚动期会陷入互相顶掉的重编循环**。滚动时新旧两代镜像同时在池子里，每次运行随机落到一代；每一代存下的仓库都会成为**未加命名空间的前缀**下「最近创建」的那一条，于是下一轮（落在另一代上）恢复的是**别人那代**的仓库，只有 ABI 没被端口版本变更波及的包能复用（2026-10-07 实测 **19/35**），其余 16 个重编约 1.5 小时，再存下自己这代——然后反过来，如此往复。实测证据：两次 `20260927.320.1` 的运行 35 条 `package ABI:` 逐字节相同，而 `20261004.326.1` 的那次有 16 条不同（变的是端口版本，如 `3.6.4#1→3.6.5`、`14.5.0→14.5.1`、`10.47#1→10.49`，级联到 16 个包）。加了镜像命名空间后每代各有自己的仓库，每代只需一轮预热，之后每轮都是 35/35 全命中。**`restore-keys` 要两条按序**：先是带镜像版本的，再是**不带镜像版本的通用回退**——回退那条不能省，否则全新镜像的第一轮会退化成冷启动（0/35、白编 1.4 小时），而它存在时至少还能拿到另一代的 19/35。镜像版本取自 `$env:ImageVersion`（runner 在**机器级**设置，见 actions/runner-images 的 `images/windows/scripts/build/Configure-SystemEnvironment.ps1`；缺省回落 `unknown` 只用于保证 key 形状合法）。

**保存端绝不能按 `cache-hit` 跳过**：Actions 的缓存 key 不可覆盖，一旦跳过，ABI 整批变化那天编译出来的产物会被**永远**丢弃，之后每次运行都重新编译 Qt5（2026-09 实测每次 1h23m，而缓存条目停留在漂移前的 09-23）。

⚠️ 但这不等于「无条件每次都保存」，区别在于**按实测跳过**还是**按猜测跳过**。`cache-hit` 只是对 vcpkg 那个 ABI 判断的猜测，而「仓库里的 archive 名字集合没变」是实测：`Install Qt5` 步在 vcpkg 跑之前记下名字集合写进 `$GITHUB_OUTPUT`，裁剪步在裁剪后再记一次并比较（**比名字不比数量**——漂移会让数量仍是 35 而**换掉每一个名字**），相同就输出 `skip=true`，`cache/save` 的 `if` 才跳过；`store-before` 缺失或裁剪步失败时输出为空，条件 `!= 'true'` 成立 ⇒ 照常保存（fail-open，判定只会往「保存」这个安全方向出错）。`always()` 保留，所以仓库真变了时即使后续步失败也照存。实测 2026-10-07 一次全命中运行：`store changed: False`、`cache/save` 显示为跳过、仓库仍停在 8 条 / 7.78 GiB，而修复前每轮都会再加 ~1 GB（其中 5 条是写了从没被读过的死重），而默认的仓库级额度是 **10 GB 大小上限 + 7 天保留（保留期从最后一次访问算，所以 restore 本身就会续期）**。超限时 GitHub **按 LRU 自动驱逐**最久未访问的条目，7 天没被访问的条目也会被驱逐，所以「列表无限增长」并不会让缓存失效——真正的代价是每轮白传 ~1 GB，并把预算花在没人能用的条目上。

保存前还有一步「按本次实际安装的包裁剪仓库」（`installed/vcpkg/status` 里的 `Abi:` 行即保留集），把仓库稳定在单一 ABI 世代（约 1 GB）；否则它每随镜像漂移一次就多一份完整 Qt5，白白吃掉仓库级缓存额度、并让列表里堆满永远不会被读到的条目。⚠️ **别把这一步的目的写成「不裁剪就会保存失败、缓存彻底失效」**——那是错的：超出额度时 GitHub **按 LRU 自动驱逐**最久未访问的条目，`cache/save` 不会失败。（这条错误论断曾写进 workflow 注释与本文档，2026-10-07 查 `actions/cache` README 与官方 changelog 后更正。）

GitHub 会删除**超过 7 天未被访问**的缓存条目（驱逐检查自 2025-09 起改为每小时一次），所以 workflow 里有一条每周两次（周一/周四 UTC 03:00）的 `schedule` 保活：它跑在默认分支上，`Restore vcpkg cache` 本身即刷新最后访问时间，`Publish GitHub release` 因 ref 不是 tag 而保持跳过，同时兼作 main 的每周构建+测试健康检查。这个保活不是多余的——2026-08-17 之后的 5 周闲置导致缓存被驱逐，下一次构建耗时 **1h49m51s**，而暖缓存只需 **19m44s**。

[.codex/skills/songbird-release/SKILL.md](.codex/skills/songbird-release/SKILL.md) 记录了发布流程的约定：版本确认、翻译校验、以及「构建/测试失败不得打 tag 或发布」。该文档以 CI 为唯一正式发布路径——推 `main` 预热 vcpkg 缓存并过门禁，推 `v*` tag 由 CI 构建并发布；本机构建只作为 CI 不可用时的备案路径。

发布资产只包含 `SongBird.exe`（CI 只 stage 这一个可执行文件），`SongBirdAuto.exe` 目前不随 Release 分发。

`workflow` token scope 只在**走 HTTPS + token** 推送时才是必需的：这个限制由 OAuth App 机制施加，而 SSH 密钥没有 scope 概念，所以走 SSH 推送新增或修改 `.github/workflows/` 不受限制。本仓库 origin 即 SSH，并通过 `~/.ssh/config` 将 `github.com` 重定向到 `ssh.github.com:443`，以绕过对 22 端口的封锁。若改用 HTTPS，缺失该 scope 时用 `gh auth refresh -h github.com -s workflow` 补授权（补完还需 `gh auth setup-git`）。

版本号权威源是根 [CMakeLists.txt](CMakeLists.txt) 的 `project(SongBird VERSION x.y.z)`，经 `src/CMakeLists.txt` 以 `SONGBIRD_APP_VERSION` 宏注入两个 target。两个 `main.cpp` 里的 `#ifndef SONGBIRD_APP_VERSION` fallback 在正常构建中不可达（宏总是被定义），仅为避免源码中出现互相矛盾的版本数字而保持同步。

## 架构

### 分层结构

| 层 | 目录 | 角色 |
|----|------|------|
| App | [app/](src/app/) | 组合根（`AppBootstrap`）、入口、启动逻辑、songbird 专用协调器 |
| AppCore | [appcore/](src/appcore/) | 两个前端共享的应用服务：`ProxySession`、核心发现/清理、TUN 运行时、后台任务协调、运行时解析 |
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

1. `app/`、`appcore/`、`auto/`、`ui/`、`services/`、`runtime/` **不得** `#include "backends/..."` —— 只能通过 `runtime/core/ICoreBackend.h` 抽象访问内核
2. `backends/` **不得** `#include "(app|appcore|ui|services|platform)/..."`
3. `runtime/core/xray`、`runtime/core/singbox` 目录不得存在（旧结构，内核实现已移至 `backends/`）
4. **每个含 `main.cpp` 的目录都必须在消费者名单里**。这条守卫是补的：`auto/` 曾因「目录没被列进脚本」而**静默逃过第 1 条**，自己手工拼 sing-box 的 TUN 配置（还因此拼出了 sing-box 拒绝加载的 `"outbounds": [[...]]` 嵌套数组）。新增前端 = 加进脚本的消费者名单，否则测试失败。
5. **前端是依赖顺序的顶**：含 `main.cpp` 的目录（现为 `app/`、`auto/`）**不得被下层 include**。这条规则是补的：`appcore/` 曾向上 include `app/` 的三个头文件，于是**共享层依赖了某个可执行文件的代码**，且任何 `appcore/` 消费者都**传递性**继承该依赖（「`auto/` 对 `app/` 依赖 = 0」当时只在**直接 include** 层面成立）。判据是「目录里有没有 `main.cpp`」，所以新增前端自动纳入。

前端要拿「内核专属配置」时走运行时层的窄入口，而不是 include 后端。TUN 设备是**独立内核进程**创建的，其配置就是内核产物：入口是 `runtime/AuxiliaryTunConfig.h` 的 `AuxiliaryTunConfig::buildRoot(coreType, config, routing)`，由 `ICoreBackend::buildAuxiliaryTunClientRoot(config, routing)` 实现（`AuxiliaryTunRouting::RelayToLocalProxy` 中继进本地代理 / `DirectOnly` 只走直连，用于代理会话停止后保住网卡不重建）。

### 本地化与 English surface（由 `localization-coverage` 测试强制）

**只有 SongBird.exe 是可本地化的产物**：生成的 `translations.qrc` 只被追加进 `SONGBIRD_SOURCES`，所以 `.qm` 只嵌进 SongBird.exe。`translations/SongBird_zh_CN.ts` 手工维护（无 `location`、无 lupdate 构建目标），编译产物提交在 `translations/compiled/`；改过 `.ts` 必须跑 `scripts/check-translations-fresh.ps1 -Update` 重新生成 `.qm` 与 sha256 清单。

**`src/auto/` 是 English surface**：SongBirdAuto 按产品决定保持英文 —— `src/auto/main.cpp` 不装 `QTranslator`，该目标也不嵌任何 `.qm`。因此该目录下的字符串必须是裸 `QStringLiteral`，**不得**出现 `tr()` / `QCoreApplication::translate()` / `QT_TR_*`；检查脚本的 Check E 会因此失败（`-EnglishSurfaceRoots`，默认 `src/auto`）。

判据是「链接它的**每个**可执行文件都是英文」，不是「文件提到 SongBirdAuto」：`src/services/`、`src/appcore/` 等共享代码保留 `translate()` —— SongBird 链接它们并渲染中文，而 SongBirdAuto 下因为没装 translator 自然保持英文。

### 内核后端

`CoreType` 只有三个真实内核：**Xray、SingBox、Mihomo**（外加 `Unknown`）。每个内核在 `backends/<name>/<Name>CoreDescriptor.cpp` 中通过静态 `CoreDescriptorRegistration` 自注册，`CoreDescriptor` 声明其 `supportedConfigTypes`、可执行文件名、`protocolPriority`（数值越小越优先：SingBox=10、Mihomo=15、Xray=20）等。

`ICoreBackend`（[runtime/core/ICoreBackend.h](src/runtime/core/ICoreBackend.h)）是内核的统一契约：配置生成（`buildClientRoot`）、辅助 TUN 根（`buildAuxiliaryTunClientRoot`，带 `AuxiliaryTunRouting`）、启动参数、版本探测、服务器校验、发布仓库等。新增内核 = 加一个 descriptor + 一个 backend 实现，其余各层无需改动；不支持 TUN 的内核不必覆写 `buildAuxiliaryTunClientRoot`，基类默认返回空对象。

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

代理激活由 [ProxySession](src/appcore/ProxySession.h) 的 `Phase` 驱动：`Stopped` → `EnvironmentCleanup` → `ValidateCoreApplication` → `ValidateRuntimeResources` → `ValidateCoreConfig` → `StartTunRuntime` → `StartCoreProcess` → `CheckOutboundLocation` → `ApplySystemProxy` → `Proxying`（或 `Stopping`）。

关键不变量：

- **Outbound location 是硬启动条件**：`queryServerLocation()` 在无法检测位置时会使启动失败；`computeProxyUiState()`（[RuntimeStateSnapshotBuilder.cpp](src/app/RuntimeStateSnapshotBuilder.cpp)）仅在运行时为 `Proxying`、核心就绪、系统代理已启用且位置非空时才报告 `Active`
- **UI 状态流经 RuntimeStateSnapshot**：`AppBootstrap::syncStatusIndicators()` 构建快照 → `RuntimeState::applySnapshot()` 发射 `snapshotApplied` → `MainWindow::applyRuntimeState()` 更新 UI。不要为核心/代理状态新增独立的 MainWindow 布尔值，应扩展快照或 `ProxyUiState`
- **START/STOP 工具栏状态**由 `ProxyToolbarController` 从 `ProxyUiState` 派生，不要在控制器外直接编辑 `QAction` 状态
- **代理启动阻塞后台任务**：`BackgroundTaskCoordinator` 使用 `AppBootstrap::isProxyActivationInProgress()` 作为阻塞谓词。新的订阅更新、测速、导入或资源更新必须尊重此协调器
- **缺失内核会自动下载并续跑启动流程**（`ProxySession::downloadMissingCoreAndResume()`），而非直接失败

### TUN 边车模式

启用 TUN 且使用 Xray 时，用 sing-box 边车进程处理 TUN 网卡（`songbird_tun` Wintun 设备；`singbox_tun` 是遗留名，仍保留在清理列表中）。`AppBootstrap` 为此辅助核心管理第二组 `QtCoreProcessHost`/`CoreLifecycleService`。决策逻辑见 [TunCompatCoreRequirement.h](src/runtime/TunCompatCoreRequirement.h)。Mihomo 原生支持 TUN，不需要边车（其 `auxiliaryTunCoreTypes` 为空）。

### 关机与后台线程

后台线程经 [BackgroundThreadTracker](src/appcore/BackgroundThreadTracker.h) 跟踪，停止一律走 [ThreadShutdown.h](src/common/ThreadShutdown.h)：请求中断 → 短等待 → **写持久记录** + 长等待 → 到硬上限就放弃，**绝不无限等**。`waitForAll()` 返回是否有 worker 没停下来；返回 false 时调用方**不得释放 worker 还能触及的对象**（worker 会 `QMetaObject::invokeMethod()` 它的 owner，那会解引用 owner），因此退出路径（`~ProxySession`、`~AppBootstrap`）改为 `abandonProcessAfterStuckThread()` 直接结束进程而不展开栈。记录同时落盘（`shutdown-hang.log`）—— GUI 进程的 stderr 没人看得见，而它是关机卡死留下的唯一线索，所以必须在进程可能消失**之前**写。预算（`ThreadShutdownBudget`）是构造函数参数，测试据此驱动放弃分支，不必等满生产上限的 30 s。

### 关键模式

- 可失败操作返回 `OperationResult`（success + message + requiresRestart）
- 枚举为普通 `enum class`，同 header 内提供 `inline` 自由辅助函数
- Domain 模型是纯结构体，无方法、无继承、无虚函数
- 仅在必要处使用接口抽象：`ICoreBackend`、`ICoreProcessHost`、`IConfigRepository`、`IUserFeedback`
- **纯决策函数采用 header-only**，便于在不链接整个 app 的情况下单测：[TunSettingsApplyDecision.h](src/appcore/TunSettingsApplyDecision.h)、[TunCompatCoreRequirement.h](src/runtime/TunCompatCoreRequirement.h)、[CoreLaunchCompatDecision.h](src/runtime/CoreLaunchCompatDecision.h)、[StartupAdminElevation.h](src/appcore/StartupAdminElevation.h)
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
