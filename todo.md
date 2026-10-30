# SongBird 架构与功能问题分析 - TODO 列表

> **项目**: SongBird (本地代理客户端)  
> **分析日期**: 2026-10-30  
> **基于提交**: `12831fb docs: add architecture analysis and improvement roadmap`  
> **分析状态**: 已完成（含对仓库内 `todo.md` 原 15 项的逐项核验）  
> **总体健康度**: 8.2/10 (架构与测试基础扎实，存在少量真实功能缺陷与工程化缺口)

---

## 0. 本机验证结果

- CTest（`msvc-debug`，`BUILD_TEST=ON`）：共 98 项，3 项失败。
- 失败项：
  - `py-localization-coverage`：`AttributeError: 'WindowsPath' object has no attribute 'is_relative_to'`
  - `py-translations-fresh`：`TypeError: unlink() got an unexpected keyword argument 'missing_ok'`
  - `py-guard-selftest`：因前两项在干净树上误报
- 原因：CMake `find_package(Python3 ...)` 找到 Python 3.7，而脚本至少需要 Python 3.8/3.9。
- 守卫脚本结果：
  - `check-backend-boundaries.ps1`：通过
  - `check-async-owners.ps1`：通过（58 个调用点 / 248 个文件）
  - `check-appbootstrap-member-order.ps1`：通过（45 个成员 / 39 个构造期依赖）

---

## 1. 原 `todo.md` 15 项核验结论

| # | 原问题 | 核验结论 | 说明 |
|---|--------|----------|------|
| 1 | AppBootstrapObjects 内存管理脆弱 | 部分成立 | 当前声明顺序正确且有守卫；双重所有权是设计选择，但存在维护风险 |
| 2 | Clash JSON 解析缺陷 | 不成立 | 已修复，解析器先 JSON 后 YAML，且有回归测试 |
| 3 | TUN 失败处理不完善 | 不成立 | `rollbackStartAfterTunFailure` / undo 已实现并有测试 |
| 4 | macOS 系统代理不完善 | 部分成立 | `capture()` 返回 invalid 是刻意设计；Auto 回退为 `ForcedClear`，是限制而非崩溃 |
| 5 | Xray 版本兼容性 | 不成立 | `XrayTlsCompatibility` 已实现并有测试 |
| 6 | 前端直接依赖后端 | 不成立 | 守卫通过，无违规 |
| 7 | 异步生命周期管理 | 不成立 | 守卫通过，所有延续均已守护或投递 |
| 8 | 错误处理不一致 | 部分成立 | `OperationResult` 是主模式；真实缺口是未捕获的 `qrcodegen` 异常 |
| 9 | 启动性能 | 未证实 | 启动序列均为有界文件扫描；无测量数据；真正阻塞在代理启动而非应用启动 |
| 10 | 端口计算重复 | 不成立 | 已集中到 `common/LocalInboundPorts` |
| 11 | 配置解析分散 | 不成立 | `persistence/` 与 `subscription/` 是不同领域，分离合理 |
| 12 | 敏感信息明文 | 成立但属设计 | 配置含密码/订阅 URL/API key；无加密；取决于威胁模型 |
| 13 | 核心文件完整性验证 | 成立 | 下载链路无 SHA-256/签名验证 |
| 14 | 内存使用优化 | 未证实 | `ServerTableRow` 约 200 字节/行，1000 服务器仅 ~200KB，当前规模无需分页 |
| 15 | 国际化不完善 | 不成立 | 硬编码中文仅在 `DialogUtils` 且受 `UiLanguage::isChinese()` 门控；守卫通过 |

### 1.1 原条目详细说明

#### 原 #1 `AppBootstrapObjects`（部分成立，P2）
- `src/app/AppBootstrapObjects.h` 的析构顺序注解是事实，但当前声明顺序正确。
- `check-appbootstrap-member-order.ps1` 通过；守卫只覆盖构造期依赖，不覆盖 `logFileWriter` 的运行期借用。
- 风险点：`logFileWriter` 被 `mainWindow` 借用，但守卫未显式建模该运行期依赖。
- 建议：保持现状，补充守卫 mutation case，避免“修守卫但不修代码”的盲区。

#### 原 #2 Clash JSON（不成立）
- `SubscriptionContentParser` 已按 `share URLs → sip008 → sing-box → Clash → base64` 顺序执行，Clash 先 JSON 后 YAML。
- `tests/SubscriptionParserTests.cpp` 已覆盖 JSON Clash 文档。

#### 原 #3 TUN 失败（不成立）
- `ProxySession::undoManagedSystemProxyAfterAbortedStartup()` 与 Auto 的 `rollbackStartAfterTunFailure()` 已实现。
- `tests/SongBirdAutoCoordinatorStopTests.cpp::aFailedTunStartGivesEveryFlagBack` 已固定“单次 undo”不变量。

#### 原 #4 macOS（部分成立，P2）
- `MacSystemProxyService::capture()` 返回空快照是刻意设计（每网络服务无法忠实恢复）。
- Auto 在无效快照时回退 `ForcedClear`，会清除用户原有代理，而非恢复。
- 影响：macOS 上 Auto 可用，但无法恢复启动前的代理设置；这是限制，不是崩溃。

#### 原 #5 Xray 兼容（不成立）
- `runtime/XrayTlsCompatibility.cpp` 已实现版本检测、`allowInsecure` 宽限期、pin 格式迁移。
- `tests/XrayTlsCompatibilityTests.cpp` 覆盖版本边界。

#### 原 #6 分层（不成立）
- `scripts/check-backend-boundaries.ps1` 通过：0 违规。

#### 原 #7 异步生命周期（不成立）
- `scripts/check-async-owners.ps1` 通过：58 个调用点全部守护或投递。

#### 原 #8 错误处理（部分成立，P1）
- 主模式是 `OperationResult`；真正的缺口是 `QrCodeRenderer` 未捕获 `qrcodegen::data_too_long` 异常（见新问题 N1）。

#### 原 #9 启动性能（未证实）
- `AppBootstrap::run()` 步骤均为有界操作（进程枚举、目录扫描、配置读取）。
- 未见启动期同步网络/版本探测（版本探测发生在代理启动时，见 N4）。
- 建议先测量冷启动时间，再决定是否异步化。

#### 原 #10 端口计算（不成立）
- `common/LocalInboundPorts.h` 是唯一派生点；`AutoRuntimeDefaults` 与 `AutoNodeEvaluationService` 均调用它。

#### 原 #11 配置解析（不成立）
- `persistence/` 负责本应用配置 schema；`subscription/` 负责外部订阅格式；职责分离正确。

#### 原 #12 明文敏感信息（成立但属设计，P2）
- `JsonConfigRepository` 以明文 JSON 存储密码、订阅 URL、API key。
- 对本地单用户桌面应用是常见做法；若需共享配置或更高威胁模型，再考虑加密/密钥链。

#### 原 #13 核心文件完整性（成立，P2）
- `CoreUpdateService`/`CoreUpdateOperations` 仅使用 HTTPS 下载，未校验 SHA-256 或签名。
- GitHub Release 元数据解析未读取 digest。

#### 原 #14 内存优化（未证实）
- `ServerTableModel` 每行约 200 字节；1000 服务器约 200KB，远低于“大量内存”。
- 配置本身已持有全部服务器数据；分页收益有限，需基准数据支持。

#### 原 #15 国际化（不成立）
- 唯一的硬编码中文在 `common/DialogUtils.h`，且全部受 `UiLanguage::isChinese()` 门控。
- `src/auto/` 是刻意的英文-only 表面，守卫已排除。

---

## 2. 本次评审新增/确认的问题

### 🔴 P0 - 紧急

#### N1. QR 码渲染在超长输入时抛未捕获异常并崩溃
- **文件**: `src/ui/qr/QrCodeRenderer.cpp`；`src/third_party/qrcodegen/qrcodegen.cpp:279`；`src/ui/mainwindow/SharePanelWidget.cpp:259`
- **现象**: `qrcodegen::encodeText` 在数据超容量时抛 `data_too_long`；UI 线程同步调用，无 `try/catch`，直接崩溃。
- **影响**: 用户分享面板输入超长链接/文本时应用崩溃。
- **修复方向**:
  - 在渲染层捕获异常并返回失败结果；
  - 对输入长度做预检并给出友好提示；
  - 考虑将渲染移到后台或限制输入。
- **验证**: 新增单元测试覆盖超长输入返回失败而非抛异常。

### 🟡 P1 - 重要

#### N2. Python 守卫脚本与本机 Python 版本不匹配
- **文件**: `scripts/python/check_localization_coverage.py`（`Path.is_relative_to`，需 3.9）、`scripts/python/check_translations_fresh.py`（`unlink(missing_ok=...)`，需 3.8）
- **现象**: CMake 找到 Python 3.7，导致 3 个 CTest 项失败（含 selftest）。
- **修复方向**:
  - 要么在 CMake 中要求 Python >= 3.9；
  - 要么用兼容写法（`os.path.commonpath` / `try/except FileNotFoundError`）。
- **验证**: 在 Python 3.7 与 3.9 下均能运行。

#### N3. 订阅下载无大小上限
- **文件**: `src/services/SubscriptionUpdateService.cpp:623`（`reply->readAll()`）
- **现象**: 订阅响应体无上限读取；恶意/异常服务器可造成内存膨胀。
- **修复方向**: 设置最大字节数、分块读取、超限即失败并记录。
- **验证**: 模拟超大响应，确认返回失败且内存有界。

#### N4. 代理启动路径在 GUI 线程同步阻塞
- **文件**: `src/runtime/ClientConfigWriter.cpp:62`（`ProcessRunner::runToCompletion` 执行 `xray -version`）；`src/appcore/TunRuntimeService.cpp:120`（最多 32s）；`src/appcore/ProxySession.cpp:239`（`start()`）
- **现象**: 版本探测与 TUN 适配器清理同步执行，可能阻塞 UI 数秒到数十秒。
- **修复方向**:
  - 版本探测提前到启动/后台线程，缓存结果；
  - TUN 清理移至后台线程并报告进度；
  - 保持状态机不变。
- **验证**: 启动/切换节点时 UI 无卡顿；现有状态机测试不回归。

#### N5. 后台订阅更新的快照保存可能覆盖并发 UI 修改
- **文件**: `src/app/SubscriptionWorkflowCoordinator.cpp:197,300`；`src/services/SubscriptionUpdateService.cpp:270,422,437`
- **现象**: 工作线程加载独立配置快照、更新后整体保存；若用户在更新期间修改节点/设置，保存会覆盖。
- **修复方向**:
  - 更新期间禁用相关编辑，或
  - 保存前重新加载并合并差异，或
  - 增加乐观并发控制。
- **验证**: 新增并发修改测试。

#### N6. `defaultAllowInsecure = true` 默认值过宽
- **文件**: `src/domain/models/Config.h:69`
- **现象**: 新配置默认允许不安全 TLS；虽然 `XrayTlsCompatibility` 处理版本差异，但默认值仍偏宽松。
- **修复方向**: 新配置默认 `false`，保留迁移逻辑对旧配置的兼容处理；同步更新测试与文档。
- **验证**: 配置序列化/迁移测试。

### 🟢 P2 - 优化/加固

#### N7. `schemaVersion` 只写不读
- **文件**: `src/persistence/JsonConfigRootSerialization.cpp`
- **现象**: 写入 `schemaVersion`，但加载路径不读取或校验；无法拒绝/升级未来格式。
- **修复方向**: 加载时读取版本，未知版本明确失败或迁移。

#### N8. `AppBootstrapObjects` 所有权仍脆弱
- **文件**: `src/app/AppBootstrapObjects.h`
- **现象**: `unique_ptr` 与 `QObject` 父子关系混用；守卫未覆盖 `logFileWriter` 的运行期借用。
- **修复方向**: 补充守卫 mutation case；长期考虑更明确的所有权模型。

#### N9. 大对象/上帝类
- **文件**: `src/app/AppBootstrap.cpp`（~1000+ 行）、`src/appcore/ProxySession.cpp`、`src/auto/SongBirdAutoCoordinator.cpp`
- **现象**: 单文件职责过多，维护成本高。
- **修复方向**: 按领域拆分 wiring 与状态机，保持行为不变。

#### N10. 核心注册表分散
- **文件**: `src/runtime/core/CoreBackendRegistry.cpp`、`src/runtime/core/CoreCatalog.h`、`src/backends/*`
- **现象**: 后端描述、能力、版本命令分散在多处。
- **修复方向**: 以注册表为单一数据源，减少重复映射。

#### N11. 系统代理“是否我们的端口”用子串匹配
- **文件**: `src/common/SystemProxySnapshot.cpp`
- **现象**: 使用子串判断，可能误判。
- **修复方向**: 精确比较 host/port。

#### N12. 未接入/未展示的既有功能
- **文件**: `src/common/HealthStripPolicy`（已持久化未展示）、`src/platform/ITerminalProxyService`（已实现未接线）、构建信息中的 `__DATE__`/`__TIME__`
- **现象**: 代码与测试存在，但用户不可用或不可复现。
- **修复方向**: 明确接线或移除，避免“半成品”长期滞留。

#### N13. 核心更新无完整性校验
- **文件**: `src/services/CoreUpdateService.cpp`、`src/services/CoreUpdateOperations.cpp`
- **现象**: 无 SHA-256/签名验证。
- **修复方向**: 解析并校验 release digest；失败时中止安装。

#### N14. 测试缺口
- **缺口**: QR 超长输入、订阅大小上限、并发配置保存、Python 版本兼容、macOS 代理恢复限制。
- **修复方向**: 补充对应测试，优先覆盖 P0/P1。

#### N15. 国际化守卫盲区（低）
- **文件**: `scripts/check-localization-coverage.ps1`
- **现象**: `src/auto/` 被视为英文-only 表面；若未来在那里加入用户可见字符串，守卫不会提醒。
- **修复方向**: 仅在需要时调整 allowlist；当前无缺陷。

---

## 3. 合并后的实施顺序

### 第一阶段（1-2 周）
1. N1 QR 崩溃
2. N2 Python 守卫版本
3. N3 订阅大小上限
4. N4 代理启动阻塞
5. N5 配置并发保存

### 第二阶段（2-4 周）
6. N6 默认 `allowInsecure`
7. N7 schemaVersion
8. N8 所有权守卫
9. N13 核心完整性
10. N14 测试缺口

### 第三阶段（4-8 周）
11. N9 大对象拆分
12. N10 核心注册表
13. N11 代理端口匹配
14. N12 半成品功能决策
15. 原 #12/#13/#14 的长期安全与性能改进

---

## 4. 验证清单

### 架构验证
- [ ] `check-backend-boundaries.ps1` 通过
- [ ] `check-appbootstrap-member-order.ps1` 通过
- [ ] `check-async-owners.ps1` 通过
- [ ] `check-hardcoded-colors.ps1` 通过
- [ ] `check-version-consistency.ps1` 通过
- [ ] `selftest-guards.ps1` 通过（含 Python 守卫）

### 功能验证
- [ ] 所有订阅格式正确解析（YAML/JSON/sip008/sing-box/base64）
- [ ] QR 超长输入不崩溃并给出提示
- [ ] TUN 失败后系统代理状态一致
- [ ] macOS 代理限制已文档化
- [ ] 核心版本兼容性正常

### 性能与安全
- [ ] 代理启动不阻塞 UI
- [ ] 订阅下载内存有界
- [ ] 并发配置保存不丢数据
- [ ] 核心更新具备完整性校验

---

## 5. 备注

1. 本文件已合并原 `todo.md` 的 15 项结论与本次评审的新问题；不成立/未证实的条目不再作为待办。
2. 优先级定义：🔴 P0 可能导致崩溃/数据损坏/安全漏洞；🟡 P1 功能异常或体验差；🟢 P2 质量/性能/可维护性。
3. 所有修复需附单元测试与集成验证；关键修复需代码审查。
4. 本次评审未在 macOS 实机或真实代理网络下验证；结论基于静态阅读与本机测试。