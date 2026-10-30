# SongBird 架构与功能问题分析 - TODO 列表

> **项目**: SongBird (本地代理客户端)  
> **分析日期**: 2025  
> **分析状态**: 已完成  
> **总体健康度**: 8.4/10 (优秀，有少量关键问题需要修复)

---

## 📊 **总体评估**

### ✅ **优秀方面**
- **架构设计**: 分层清晰 (app/ -> appcore/ -> runtime/ -> backends/)，接口抽象合理
- **测试覆盖**: 70+ 单元测试文件，守护测试完善 (selftest-guards.ps1)
- **平台支持**: Windows/macOS 支持，平台抽象接口良好
- **代码质量**: 命名规范，注释完整，代码风格一致
- **功能完善**: 支持多协议 (VMess, VLESS, Trojan, Shadowsocks, Hysteria2, TUIC, WireGuard, AnyTLS, Naive)
- **核心管理**: 支持 Xray、sing-box、Mihomo 三种核心，可互换

### ⚠️ **关键问题统计**
| 优先级 | 数量 | 说明 |
|--------|------|------|
| 🔴 高 | 4 | 需要立即修复的内存和功能问题 |
| 🟡 中 | 8 | 需要中期修复的设计和实现问题 |
| 🟢 低 | 6 | 可以后期优化的问题 |

---

## 🔴 **P0 - 紧急修复 (立即处理)**

### 1. 内存管理脆弱性 - AppBootstrapObjects
**文件**: `src/app/AppBootstrapObjects.h`  
**风险**: Use-after-free, 双重释放  
**描述**: 成员析构顺序依赖声明顺序，注解明确说明 "Members are destroyed in REVERSE declaration order, so declaration order is load bearing"  

**问题详情**:
```cpp
// 当前设计
std::unique_ptr<JsonConfigRepository> repository;    // 必须第一个
std::unique_ptr<ServerService> serverService;        // 依赖 repository
std::unique_ptr<LogFileWriter> logFileWriter;        // 被 mainWindow 使用
std::unique_ptr<MainWindow> mainWindow;              // 依赖 logFileWriter
```
- 开发人员必须记住声明顺序的重要性
- 添加新成员时需要仔细考虑依赖关系
- 使用 `QObject::setParent()` 与 `unique_ptr` 混合管理，存在双重所有权

**修复方案**:
- [ ] 重构为使用 `std::shared_ptr` 实现共享所有权
- [ ] 或者重新设计为完全使用 `QObject` 父子关系自动管理内存
- [ ] 移除 `AppBootstrapObjects` 结构，改用依赖注入容器
- [ ] 添加单元测试验证内存管理正确性

**预期结果**: 内存管理更加健壮，不再依赖声明顺序

---

### 2. 订阅 JSON 解析逻辑缺陷
**文件**: `src/subscription/ClashSubscriptionParser.cpp`  
**风险**: 功能 Bug - JSON 格式订阅无法正确导入  
**描述**: YAML 解析器会先于 JSON 解析器执行，由于 JSON 是有效的 YAML，导致部分解析后产生错误结果  

**问题详情**:
- `ClashSubscriptionParser::parseContent()` 先尝试 YAML 解析
- JSON 是有效 YAML，YAML 解析器部分解析后产生空结果
- 只有当文档没有 `proxy-groups:` 或 `rules:` 时才回退到 JSON 解析
- 导致 Clash JSON 格式订阅无法正确导入

**修复方案**:
- [ ] 修改解析器顺序：首先尝试 JSON 解析，失败后再尝试 YAML
- [ ] 或者检查内容是否是有效的 JSON（通过 JSON 解析器验证），如果是则直接使用 JSON 解析
- [ ] 添加测试用例验证各种订阅格式
- [ ] 确保解析器链顺序正确：share URLs → sip008 → sing-box → Clash → base64

**预期结果**: 所有订阅格式都能正确解析

---

### 3. TUN 失败处理逻辑不完善
**文件**: `src/appcore/ProxySession.cpp`, `src/auto/SongBirdAutoCoordinator.cpp`  
**风险**: 系统代理状态不一致，用户网络中断  
**描述**: SongBird 和 SongBirdAuto 的 TUN 失败处理逻辑存在差异，可能导致系统代理无法正确恢复  

**问题详情**:
- SongBirdAuto 的 TUN 失败后没有正确恢复系统代理
- 失败后没有正确清理系统代理状态
- 用户可能发现自己整个机器 offline，但状态栏显示 "Stopped"
- 相关 CHANGELOG 条目："SongBirdAuto's TUN-failure branch now undoes a system proxy this session applied"

**修复方案**:
- [ ] 统一 SongBird 和 SongBirdAuto 的 TUN 失败处理逻辑
- [ ] 确保在任何 TUN 失败情况下都调用 `undoManagedSystemProxyAfterAbortedStartup()`
- [ ] 实现统一的系统代理恢复机制
- [ ] 添加测试验证 TUN 失败后的系统代理恢复

**预期结果**: TUN 失败后系统代理状态始终一致

---

### 4. macOS 系统代理支持不完善
**文件**: `src/platform/macos/MacSystemProxyService.cpp`  
**风险**: SongBirdAuto 在 macOS 上无法正常工作  
**描述**: macOS 系统代理捕获机制不完善，导致 SongBirdAuto 无法恢复用户代理设置  

**问题详情**:
- 注解说明："macOS reports capture() as invalid deliberately, because its per-network-service settings cannot be re-applied faithfully"
- SongBirdAuto 无法捕获 macOS 系统代理状态
- 导致 SongBirdAuto 在 macOS 上无法正常工作
- 用户可能无法恢复到原来的代理设置

**修复方案**:
- [ ] 为 macOS 实现更健壮的代理管理
- [ ] 实现 macOS 的代理快照功能 (capture/restore)
- [ ] 在 macOS 上禁用 SongBirdAuto 的自动代理功能，或向用户明确说明限制
- [ ] 添加测试验证 macOS 代理管理

**预期结果**: SongBirdAuto 在 macOS 上能够正常工作，或者向用户明确说明限制

---

## 🟡 **P1 - 重要修复 (中期处理)**

### 5. 核心版本兼容性问题
**文件**: `src/runtime/XrayTlsCompatibility.cpp`  
**风险**: 配置兼容性问题，旧配置无法在新版本中工作  
**描述**: 不同 Xray 版本对配置的解析存在差异，需要版本检测和兼容性适配  

**问题详情**:
- Xray 1.8+ 不再支持 `allowInsecure: true`
- Trojan 导入和旧配置需要默认启用 TLS
- 不同版本对 `pinnedPeerCertSha256` 格式要求不同
- 需要运行时版本检测

**修复方案**:
- [ ] 实现核心版本检测机制
- [ ] 添加兼容性适配层 (`XrayTlsCompatibility`)
- [ ] 向用户提供明确的错误信息
- [ ] 添加测试验证版本兼容性

**预期结果**: 不同核心版本的配置都能正确工作

---

### 6. 前端对后端直接依赖 (违反分层原则)
**文件**: `src/app/`, `src/auto/`, `src/backends/`  
**风险**: 架构退化，循环依赖  
**描述**: 检查脚本 `check-backend-boundaries.ps1` 存在，但需要验证是否严格执行  

**问题详情**:
- 可能存在 `app/` 或 `auto/` 直接包含 `backends/` 头文件
- 可能存在 `backends/` 包含 `app/`/`appcore/`/`ui/`/`services/`/`platform/` 头文件
- 可能存在上层依赖下层的循环依赖

**修复方案**:
- [ ] 运行 `check-backend-boundaries.ps1` 检查是否存在违规
- [ ] 确保所有前端只通过接口访问后端
- [ ] 确保 `backends/` 不包含任何上层模块头文件
- [ ] 将守护测试集成到 CI/CD

**预期结果**: 分层架构严格执行，无循环依赖

---

### 7. 异步操作生命周期管理
**文件**: `src/appcore/ProxySession.cpp`, `src/services/*`  
**风险**: Use-after-free, 内存访问异常  
**描述**: 后台线程回调可能捕获已销毁对象的 this 指针  

**问题详情**:
```cpp
// 问题代码模式
AvailabilityCheck = std::function<OperationResult(const Config& config)>
// Called on a background thread, so the implementation must not touch this object.
```
- 回调函数在后台线程执行
- 可能捕获 `this` 指针
- 如果对象在回调执行前被销毁，会导致 use-after-free

**修复方案**:
- [ ] 使用 `std::weak_ptr` 捕获 `this` 指针
- [ ] 添加生命周期守护机制 (`std::shared_ptr<char> lifetimeGuard_`)
- [ ] 确保回调在对象销毁前完成
- [ ] 使用 `check-async-owners.ps1` 检查异步代码

**预期结果**: 异步操作安全，无内存访问异常

---

### 8. 错误处理机制不一致
**文件**: 全局  
**风险**: 错误可能被忽略，调试困难  
**描述**: 代码库中存在多种错误处理方式，需要统一  

**问题详情**:
- `OperationResult` 结构体
- 返回值 + 错误代码
- 日志记录 + UI 提示
- 异常 (虽然很少使用)

**修复方案**:
- [ ] 统一使用 `OperationResult` 作为主要错误处理方式
- [ ] 确保所有错误都被适当处理和记录
- [ ] 添加错误处理指南文档
- [ ] 使用静态分析工具检查错误处理

**预期结果**: 错误处理统一，所有错误都被正确处理

---

### 9. 启动性能优化
**文件**: `src/app/AppBootstrap.cpp`  
**风险**: 用户体验差  
**描述**: 启动时有多个同步操作，可能阻塞 UI  

**问题详情**:
```cpp
sequencer.addStep(QStringLiteral("cleanup-orphan-core-processes"), ...);
sequencer.addStep(QStringLiteral("refresh-existing-core-types"), ...);
sequencer.addGate(QStringLiteral("reload-config"), ...);
```
- 启动时有多个同步操作
- 这些操作可能阻塞 UI
- 影响用户体验

**修复方案**:
- [ ] 将非关键操作移到后台线程
- [ ] 使用异步加载
- [ ] 提供启动进度反馈
- [ ] 优化启动序列

**预期结果**: 启动更快，用户体验更好

---

## 🟢 **P2 - 优化改进 (后期处理)**

### 10. 代码重复 - 端口计算逻辑
**文件**: `src/common/LocalInboundPorts.h`  
**风险**: 维护困难  
**描述**: 端口计算逻辑在多个地方重复，容易导致不一致  

**问题详情**:
- 端口计算逻辑分散在多个地方
- 修改端口逻辑需要同时修改多个地方
- 容易导致不一致

**修复方案**:
- [ ] 将端口计算逻辑完全集中到 `common/LocalInboundPorts`
- [ ] 确保所有地方都使用统一接口
- [ ] 移除重复代码
- [ ] 添加单元测试验证端口计算

**预期结果**: 端口计算逻辑统一，无重复代码

---

### 11. 配置解析逻辑分散
**文件**: `src/persistence/`, `src/subscription/`  
**风险**: 维护困难，解析不一致  
**描述**: 配置相关的解析逻辑分散在多个模块  

**修复方案**:
- [ ] 集中配置解析逻辑
- [ ] 使用统一的解析器框架
- [ ] 提供明确的接口
- [ ] 添加文档说明解析流程

**预期结果**: 配置解析逻辑统一，维护更简单

---

### 12. 安全性加强 - 敏感信息保护
**文件**: `src/persistence/JsonConfigRepository.cpp`  
**风险**: 敏感信息泄露  
**描述**: 配置文件是明文的，可能包含敏感信息  

**问题详情**:
- 配置文件包含服务器密码
- 可能包含订阅 URL
- 可能包含 API 密钥
- 共享配置文件时可能泄露敏感信息

**修复方案**:
- [ ] 提供配置文件加密选项
- [ ] 使用系统密钥库存储敏感信息
- [ ] 在共享配置时自动清除敏感信息
- [ ] 添加敏感信息检测和警告

**预期结果**: 敏感信息得到更好保护

---

### 13. 安全性加强 - 核心文件完整性验证
**文件**: `src/services/CoreUpdateService.cpp`  
**风险**: MITM 攻击  
**描述**: 核心文件下载时需要验证完整性  

**修复方案**:
- [ ] 实现 SHA-256 校验和验证
- [ ] 添加数字签名验证
- [ ] 确保使用 HTTPS 下载
- [ ] 添加完整性验证失败的处理逻辑

**预期结果**: 核心文件下载更安全

---

### 14. 内存使用优化
**文件**: `src/ui/models/ServerTableModel.cpp`  
**风险**: 大配置文件处理时内存消耗大  
**描述**: 处理数千个服务器时可能消耗大量内存  

**修复方案**:
- [ ] 实现惰性加载
- [ ] 使用分页显示
- [ ] 优化数据结构
- [ ] 添加内存使用监控

**预期结果**: 内存使用更高效

---

### 15. 国际化完善
**文件**: 全局  
**风险**: 硬编码字符串无法翻译  
**描述**: 仍然存在一些硬编码字符串  

**修复方案**:
- [ ] 使用 `QCoreApplication::translate()` 或 `tr()` 替换硬编码字符串
- [ ] 定期运行 `check-localization-coverage.ps1` 检查
- [ ] 更新翻译文件
- [ ] 添加缺失的翻译

**预期结果**: 国际化支持更完善

---

## 📋 **实施计划**

### 第一阶段 (2-4 周) - 紧急修复
**目标**: 修复所有 P0 级别问题

| 任务 | 预估时间 | 状态 |
|------|----------|------|
| P1: AppBootstrapObjects 内存管理 | 3-5 天 | ⏳ 待开始 |
| P2: JSON 订阅解析逻辑 | 2-3 天 | ⏳ 待开始 |
| P3: TUN 失败处理逻辑 | 3-5 天 | ⏳ 待开始 |
| P4: macOS 代理支持 | 5-7 天 | ⏳ 待开始 |

**交付物**:
- 所有 P0 问题修复
- 对应的单元测试
- 集成测试验证

---

### 第二阶段 (4-6 周) - 重要修复
**目标**: 修复所有 P1 级别问题

| 任务 | 预估时间 | 状态 |
|------|----------|------|
| P5: 核心版本兼容性 | 3-5 天 | ⏳ 待开始 |
| P6: 前端后端依赖 | 2-3 天 | ⏳ 待开始 |
| P7: 异步操作生命周期 | 3-5 天 | ⏳ 待开始 |
| P8: 错误处理统一 | 3-5 天 | ⏳ 待开始 |
| P9: 启动性能优化 | 3-5 天 | ⏳ 待开始 |

**交付物**:
- 所有 P1 问题修复
- 性能基准测试
- 代码审查完成

---

### 第三阶段 (6-8 周) - 优化改进
**目标**: 实施 P2 级别改进

| 任务 | 预估时间 | 状态 |
|------|----------|------|
| P10: 端口计算统一 | 1-2 天 | ⏳ 待开始 |
| P11: 配置解析集中 | 3-5 天 | ⏳ 待开始 |
| P12: 敏感信息保护 | 5-7 天 | ⏳ 待开始 |
| P13: 核心文件验证 | 2-3 天 | ⏳ 待开始 |
| P14: 内存优化 | 3-5 天 | ⏳ 待开始 |
| P15: 国际化完善 | 2-3 天 | ⏳ 待开始 |

**交付物**:
- 所有 P2 改进实施
- 性能和内存使用优化
- 安全性加强

---

## 📊 **验证清单**

### 架构验证
- [ ] 运行 `check-backend-boundaries.ps1` - 确保分层架构正确
- [ ] 运行 `check-appbootstrap-member-order.ps1` - 确保内存管理正确
- [ ] 运行 `check-async-owners.ps1` - 确保异步代码正确
- [ ] 运行 `check-hardcoded-colors.ps1` - 确保颜色使用正确
- [ ] 运行 `check-version-consistency.ps1` - 确保版本一致
- [ ] 运行 `check-readme-assets.ps1` - 确保 README 资源正确
- [ ] 运行 `selftest-guards.ps1` - 确保所有守护测试通过

### 功能验证
- [ ] 所有订阅格式正确解析 (YAML, JSON, sip008, sing-box, base64)
- [ ] TUN 模式在所有平台正常工作
- [ ] 系统代理在启动/停止/崩溃时正确管理
- [ ] 核心版本兼容性正常
- [ ] macOS 支持正常

### 性能验证
- [ ] 启动时间 < 2 秒 (冷启动)
- [ ] 内存使用 < 100MB (正常使用)
- [ ] 大配置文件 (1000+ 服务器) 加载正常

### 安全性验证
- [ ] 配置文件敏感信息得到保护
- [ ] 核心文件完整性验证正常
- [ ] 无敏感信息泄露风险

---

## 📝 **备注**

1. **优先级定义**:
   - 🔴 **P0 - 紧急**: 可能导致崩溃、数据损坏或安全漏洞的问题
   - 🟡 **P1 - 重要**: 可能导致功能异常或用户体验差的问题
   - 🟢 **P2 - 优化**: 可以提升代码质量、性能或可维护性的改进

2. **预估时间**: 基于单个开发者的工作量估算，实际可能因团队规模而变化

3. **测试策略**:
   - 每个修复都需要对应的单元测试
   - 所有修复都需要集成测试验证
   - 关键修复需要手动验证

4. **风险缓解**:
   - 高风险的修复 (如内存管理) 需要更多的测试和验证
   - 建议在分支上进行开发，通过 CI/CD 自动测试
   - 关键修复需要代码审查

5. **文档要求**:
   - 所有重要修复都需要更新相关文档
   - API 变更需要更新接口文档
   - 架构变更需要更新架构文档

---

**分析完成日期**: 2025  
**下次审查日期**: 建议每季度审查一次  
**负责人**: 待指定
