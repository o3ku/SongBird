# SongBird 逻辑与业务流程问题分析 - TODO 列表

> **项目**: SongBird (Qt/C++ 本地代理客户端)  
> **分析日期**: 2026-11-12  
> **分支/基线**: `main` (commit `8f17a69`)  
> **分析范围**: 核心业务生命周期、多核配置生成、位置组/单节点切换、系统代理所有权、订阅更新与节点集合维护、UI 状态同步

---

## 1. 问题汇总与优先级矩阵

| 编号 | 模块 | 缺陷类型 | 严重程度 | 简述 |
| :--- | :--- | :--- | :--- | :--- |
| **P0-1** | `AppBootstrap` | 系统代理所有权破坏 | 🔴 严重 | 未开启代理时启动应用，无条件清除系统代理和 PAC 配置 |
| **P0-2** | `AutoSwitchCoordinator` | 故障转移业务逻辑错误 | 🔴 严重 | 组内故障自动切换错误绑定 UI 当前浏览的标签页，导致断网时不触发切换 |
| **P1-1** | `LocationGroupScan` | 组分类与手势识别缺陷 | 🟡 较重 | 位置组运行期间点击已收藏的同组节点，误触发切换至 `FAVORITE` 组 |
| **P1-2** | `ServerTableModel` | 视图更新范围计算错误 | 🟡 较重 | 高索引行切至低索引行时计算错误，新激活行未派发 `dataChanged`，界面不刷新 |
| **P1-3** | `ServerActivationGestureLogic` | 交互状态机单向锁定 | 🟡 较重 | 位置组模式运行中无法通过界面点击或右键菜单切换回单节点代理 |
| **P1-4** | `ServerCollectionCoordinator` | 核心配置与状态不一致 | 🟡 较重 | 删除非 Anchor 成员节点或相关订阅时未重载核心，核心持续保留并测速失效节点 |
| **P2-1** | `SingBoxRoutingConfigFragments` | 规则生成边界缺陷 | 🟢 一般 | 剔除未发布规则集后未校验剩余条件，可能意外生成无条件的全局通配拦截规则 |
| **P2-2** | 架构与规范文档 | 规范漂移与死代码 | 🟢 一般 | `AGENTS.md` 规定的 `RoutingScorePolicy` 随 Auto 移除后已成死代码，未同步更新 |

---

## 2. 缺陷详细分析与业务影响

### P0-1: 启动应用时无条件破坏系统代理与 PAC 设置
* **涉及文件**:
  - `src/app/AppBootstrap.cpp` (`applyStartupSystemProxyPreference()`)
  - `src/app/SystemProxyCoordinator.cpp` (`disable()`)
  - `src/platform/windows/WindowsSystemProxyService.cpp` (`update()`)
* **代码溯源**:
  ```cpp
  // AppBootstrap.cpp
  void AppBootstrap::applyStartupSystemProxyPreference()
  {
      if (config_.ui().mainProxyEnabled) {
          enableSystemProxy(true);
          return;
      }
      disableSystemProxy();
  }
  ```
  若上次退出时代理未运行（`mainProxyEnabled == false`），启动流程会调用 `disableSystemProxy()` -> `SystemProxyCoordinator::setMode(SystemProxyMode::ForcedClear)` -> `WindowsSystemProxyService::update(ForcedClear)`。
  在 `WindowsSystemProxyService::update()` 中，直接执行了 `clearAutoConfigUrl()` 并将注册表 `ProxyEnable` 置 0。
* **业务危害**:
  严重违反了 `AGENTS.md` 和 `SystemProxyOwnershipPolicy.h` 的**代理所有权原则**（“Never wipe what we didn't set”）。用户若在 Windows 配置了企业代理、PAC 脚本或 Charles/Fiddler 等本地调试代理，只要打开 SongBird 且代理处于关闭状态，**外部配置的系统代理和 PAC URL 将被立即抹除**。此外，若上次程序异常退出留有待恢复的 `systemProxySnapshot`，此处也不会还原，直接造成系统代理丢失。
* **整改方案**:
  在启动时，若 `!config_.ui().mainProxyEnabled`，检查是否存在上次崩溃遗留的有效快照 `systemProxySnapshot`：若有且符合所有权策略则调用 `restoreCapturedSnapshot()` 恢复；若无快照且本应用未接管代理，**严禁主动调用 `disableSystemProxy()` 抹除注册表**。

---

### P0-2: 组内故障自动转移（Auto Switch）错误依赖 UI 浏览标签
* **涉及文件**:
  - `src/app/AppBootstrapUiWiring.cpp` (`wireUiCoordinators()`)
  - `src/app/AutoSwitchCoordinator.cpp` (`scanCurrentGroup()`, `handleSessionFailure()`)
* **代码溯源**:
  ```cpp
  // AppBootstrapUiWiring.cpp
  objects_->autoSwitchCoordinator = std::make_unique<AutoSwitchCoordinator>(
      AutoSwitchCoordinator::Dependencies{
          [this]() { return config_.ui().autoSwitchInGroup; },
          [this]() { return config_.ui().mainSelectedLocationId; }, // 注入的是 UI 当前查看的标签页
          ...
  ```
  ```cpp
  // AutoSwitchCoordinator.cpp
  AutoSwitchGroupLogic::GroupScan AutoSwitchCoordinator::scanCurrentGroup() const
  {
      const QString failedId = deps_.lastFailedServerId ? deps_.lastFailedServerId() : QString();
      return AutoSwitchGroupLogic::scanLocationGroup(
          deps_.currentServers ? deps_.currentServers() : QList<VmessItem>{},
          deps_.selectedLocationId ? deps_.selectedLocationId() : QString(),
          failedId);
  }
  ```
* **业务危害**:
  后台连接的故障转移与预热探测被绑定到了前端界面的临时查看状态。例如：用户当前正连接 `US` 节点上网，在主界面点击了 `JP` 标签查看日本节点列表或切到了“订阅”标签。此时 `US` 节点发生断网故障，`scanCurrentGroup()` 扫描的是 `JP` 组，发现失败的 `US` 节点不在当前浏览的 `JP` 组中（`scan.failedIndex < 0`），直接输出 `not this group's failure` 并退出，**自动故障切换完全失效**；同时后台预热探测 `runWarmRound()` 也一直在错误地探测日本节点。
* **整改方案**:
  `deps_.selectedLocationId` 必须解绑 UI 标签 `mainSelectedLocationId`，改为获取**当前正在运行的活动节点实际归属的位置组**（优先读取 `activeLocationGroupId`，若为单节点模式则读取 `LocationCatalog::resolvedLocationId(*activeServer)`）。

---

### P1-1: 位置组运行中点击已收藏节点误触发跨组切换
* **涉及文件**:
  - `src/appcore/LocationGroupScan.h` (`locationIdOfServer()`, `embeddableMembers()`)
  - `src/app/ServerActivationGestureLogic.h` (`resolveServerActivationGesture()`)
* **代码溯源**:
  ```cpp
  // LocationGroupScan.h
  inline QString locationIdOfServer(const VmessItem& server)
  {
      return server.favorite ? QStringLiteral("FAVORITE") : LocationCatalog::resolvedLocationId(server);
  }
  ```
  在 `embeddableMembers(servers, "HK")` 中，所有香港节点（无论是否收藏）都会被包含进 `"HK"` 位置组。
  然而当 `"HK"` 组正在运行时，用户在列表中点击一个收藏的香港节点：
  ```cpp
  // ServerActivationGestureLogic.h
  const QString clickedLocation = LocationGroupScan::locationIdOfServer(*clicked); // 返回 "FAVORITE"
  if (clickedLocation == activeGroupLocationId.trimmed()) { // "FAVORITE" != "HK"
      return {};
  }
  return {ActivationGesture::Kind::ActivateGroup, {}, clickedLocation}; // 误判为切换到 FAVORITE 组！
  ```
* **业务危害**:
  用户在特定位置组（如 HK）下点击带有收藏星标的节点时，本意是同组操作（预期为保持同组 NoOp 或选中该节点），但系统却误判为跨组切换，**直接导致整个核心的自动测速组被切换到“收藏（FAVORITE）”组**并重启核心。
* **整改方案**:
  在 `resolveServerActivationGesture` 或 `locationIdOfServer` 中增加上下文感知：如果当前正在运行的组 `activeGroupLocationId` 并非 `"FAVORITE"`，且被点击节点在物理归属上属于当前位置组，应优先将其位置判定为当前运行的位置组，避免误触发切组。

---

### P1-2: 节点列表模型激活行切换计算错误导致视图不刷新
* **涉及文件**:
  - `src/ui/models/ServerTableModel.cpp` (`setActiveIndexId()`)
* **代码溯源**:
  ```cpp
  void ServerTableModel::setActiveIndexId(const QString& indexId)
  {
      const QString trimmedId = indexId.trimmed();
      if (trimmedId == currentIndexId_) return;

      const int previousRow = rowByIndexId_.value(currentIndexId_, -1);
      const int nextRow = rowByIndexId_.value(trimmedId, -1);
      currentIndexId_ = trimmedId;

      const int firstRow = previousRow >= 0 ? previousRow : nextRow;
      const int lastRow = previousRow >= nextRow ? previousRow : nextRow;
      if (firstRow >= 0) {
          emit dataChanged(
              index(firstRow, 0),
              index(lastRow, static_cast<int>(Column::ColumnCount) - 1),
              {Qt::DisplayRole});
      }
  }
  ```
* **业务危害**:
  当活动节点从较大行号切换到较小行号（例如从第 8 行切到第 2 行）：
  - `firstRow = (8 >= 0 ? 8 : 2) = 8`
  - `lastRow = (8 >= 2 ? 8 : 2) = 8`
  - 发送的 `dataChanged` 范围仅为 `[8, 8]`，**目标行（第 2 行）完全被遗漏**。
  新选中的节点在界面上不会显示 Logo 图标，文字也不会变绿，出现明显的 UI 显示滞后或状态脱节。同时，`roles` 仅传递了 `{Qt::DisplayRole}`，未包含实际受影响的 `Qt::DecorationRole` 和 `Qt::ForegroundRole`。
* **整改方案**:
  重构首尾行计算逻辑：
  ```cpp
  int firstRow = -1;
  int lastRow = -1;
  if (previousRow >= 0 && nextRow >= 0) {
      firstRow = std::min(previousRow, nextRow);
      lastRow = std::max(previousRow, nextRow);
  } else if (previousRow >= 0) {
      firstRow = lastRow = previousRow;
  } else if (nextRow >= 0) {
      firstRow = lastRow = nextRow;
  }
  if (firstRow >= 0) {
      emit dataChanged(
          index(firstRow, 0),
          index(lastRow, static_cast<int>(Column::ColumnCount) - 1),
          {Qt::DisplayRole, Qt::DecorationRole, Qt::ForegroundRole});
  }
  ```

---

### P1-3: 位置组模式运行中无法通过界面交互切回单节点模式
* **涉及文件**:
  - `src/app/ServerActivationGestureLogic.h` (`resolveServerActivationGesture()`)
  - `src/ui/mainwindow/MainWindow.cpp`
* **代码溯源**:
  ```cpp
  if (clicked->configType == ConfigType::Custom || !coreRunning
      || activeGroupLocationId.trimmed().isEmpty()) {
      return {ActivationGesture::Kind::ActivateNode, clickedId, {}};
  }

  const QString clickedLocation = LocationGroupScan::locationIdOfServer(*clicked);
  if (clickedLocation == activeGroupLocationId.trimmed()) {
      return {};
  }
  return {ActivationGesture::Kind::ActivateGroup, {}, clickedLocation};
  ```
* **业务危害**:
  在位置组运行期间（`activeLocationGroupId` 非空），用户在界面列表中双击、按回车或右键点击“设为默认活动服务器”，同组点击为 `NoOp`，不同组点击为 `ActivateGroup`。**用户无法在核心运行期间明确选择某一个单节点**，处于被“锁定在自动测速组”中的状态，只能先停止代理或使用 Ctrl+Enter（TUN 强制切单节点）规避。
* **整改方案**:
  在节点右键菜单中增加显式的“以单节点连接/退出组测速”手势，或在 `ServerActivationGestureLogic` 中区分行单击、双击与上下文菜单指令，允许用户从自动测速组退回单节点代理。

---

### P1-4: 位置组运行中删除非 Anchor 成员或订阅未重载核心
* **涉及文件**:
  - `src/app/ServerCollectionCoordinator.cpp` (`removeServers()`, `deleteSubscription()`)
  - `src/app/SubscriptionWorkflowCoordinator.cpp` (`updateByIds()`)
* **代码溯源**:
  在 `removeServers()` 中：
  ```cpp
  const bool activeServerRemoved = activeServer.has_value()
      && indexIds.contains(activeServer->indexId);
  ...
  if (!removal.applied || !activeServerRemoved || !isCoreRunning()) {
      return; // 非 Anchor 节点被删除时直接返回，不重启核心
  }
  ```
* **业务危害**:
  位置组是由该地区的多个节点聚合生成的。若用户删除了组内的其他节点，只要被删节点不是 Anchor（首节点），核心配置不会重新生成和重载。核心的 `urltest` 测速组在内存中持续保留已删除节点的配置并对其定时探测。如果整个组的成员被全部删除，由于 `activeLocationGroupId` 未被清除，下次启动核心将因该组无可运行成员而直接崩溃报错。
* **整改方案**:
  当 `!activeLocationGroupId.isEmpty()` 时，删除节点或更新/删除订阅的操作需检查受影响的节点是否包含在当前运行组的成员列表中；若包含，需联动重新生成组配置并重载核心；若组内成员归零，需自动清除 `activeLocationGroupId` 并退回普通单节点模式。

---

### P2-1: Sing-Box 规则集迁移过滤后可能生成全匹配通配规则
* **涉及文件**:
  - `src/backends/singbox/SingBoxRoutingConfigFragments.cpp` (`migrateGeoToRuleSet()`)
  - `src/backends/singbox/SingBoxRuleSetAvailability.h` (`isSingBoxRuleSetPublishedUpstream()`)
* **代码溯源**:
  ```cpp
  if (rule.contains(QStringLiteral("geosite"))) {
      QJsonArray values = rule.take(QStringLiteral("geosite")).toArray();
      for (const QJsonValue& v : values) {
          const QString tag = QStringLiteral("geosite-%1").arg(v.toString());
          if (!isSingBoxRuleSetPublishedUpstream(tag)) {
              continue;
          }
          ruleSetTags.append(tag);
          allRuleSetTags.insert(tag);
      }
      modified = true;
  }
  ```
* **业务危害**:
  若某条路由规则仅指定了未发布的规则集（例如仅包含 `geosite:gfw` 或 `geoip:telegram`），`rule.take("geosite")` 会将其取走，而过滤后 `ruleSetTags` 为空。若该规则没有配置其他匹配字段，最终生成的 sing-box 规则将变成一个完全没有匹配条件的规则（或包含空数组 `rule_set: []`）。在 sing-box 的路由匹配中，无匹配条件的规则将作为默认全匹配规则生效，导致其后的所有分流规则被完全遮蔽。
* **整改方案**:
  在 `migrateGeoToRuleSet` 中，如果某条规则的所有条件在迁移过滤后均为空，应直接从 `rules` 数组中移除该无效规则，不得保留无条件的空规则。

---

### P2-2: `RoutingScorePolicy` 已成死代码，与 `AGENTS.md` 规范脱节
* **涉及文件**:
  - `AGENTS.md` (SongBird Runtime Rules)
  - `src/appcore/RoutingScorePolicy.h` / `.cpp`
  - `src/app/AutoSwitchGroupLogic.h`
* **问题描述**:
  `AGENTS.md` 文档中保留了如下规定：
  > “Node selection goes through `RoutingScorePolicy::shouldSwitch()` (EWMA + breaker + 25% margin + 10 min hold) — never raw rank comparison...”
  而在提交 `efa8b09` 移除 `SongBirdAuto` 之后，`RoutingScorePolicy` 在 `src/app/` 和整个运行时中已完全没有任何调用点，仅剩单测引用。现行组内切换使用的是 `AutoSwitchGroupLogic::pickWarmSwitchTarget` 的简单延迟比较。
* **整改方案**:
  - 方案 A（文档校准）：修改 `AGENTS.md`，同步当前组内自动切换采用的 warm-probe 延迟模型，清理或归档未使用的 `RoutingScorePolicy`；
  - 方案 B（功能补全）：若产品意图是在自动切换中继续使用 EWMA、断路器与保持期策略，应将 `RoutingScorePolicy` 重新接入 `AutoSwitchCoordinator` 的决策链中。

---

## 3. 实施计划与执行顺序

### 阶段一：高危逻辑修复（优先发布）
1. [x] **P0-1 启动代理所有权守护**: 修正 `AppBootstrap::applyStartupSystemProxyPreference()`，禁止在无接管状态下盲目清除注册表系统代理。
2. [x] **P0-2 自动切换组上下文解绑**: 修正 `AutoSwitchCoordinator` 的依赖注入，切换和预热探测依据活动核心的真实节点位置，而非界面查看标签。
3. [x] **P1-2 表格模型刷新范围修复**: 修正 `ServerTableModel::setActiveIndexId` 的 `min/max` 行号计算并补充 `DecorationRole` / `ForegroundRole`。

### 阶段二：组模式业务流一致性完善
4. [x] **P1-1 收藏节点所属组消除歧义**: 修正 `LocationGroupScan` 与 `ServerActivationGestureLogic`，避免同组已收藏节点误触发组切换。
5. [x] **P1-3 提供单节点选择通道**: 在节点列表右键菜单中支持显式设为单节点代理，打破位置组模式下单向锁定。
6. [x] **P1-4 组模式下节点/订阅变更联动**: 在删除节点和更新订阅流程中，识别并联动重载当前正在运行的位置组核心。

### 阶段三：配置生成健壮性与规范更新
7. [x] **P2-1 规则集迁移空规则剔除**: 在 `SingBoxRoutingConfigFragments::migrateGeoToRuleSet` 中检测并过滤空条件路由规则。
8. [x] **P2-2 规范与策略决策校准**: 评估 `RoutingScorePolicy` 是否回归决策链，或更新 `AGENTS.md` 消除文档漂移。
