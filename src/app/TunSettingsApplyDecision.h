#pragma once

#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
#include "runtime/ProtocolCoreCompat.h"

inline QList<CoreTypeItem> normalizedCoreTypeItemsForComparison(const Config& config)
{
    QList<CoreTypeItem> items;
    const QList<ConfigType> protocols = configurableCoreProtocols();
    items.reserve(protocols.size());

    for (const ConfigType configType : protocols) {
        const CoreType configuredCore = configuredCoreTypeForProtocol(config, configType);
        const CoreType effectiveCore = configuredCore == CoreType::Unknown
            ? defaultCoreTypeForProtocol(configType)
            : configuredCore;
        items.append(CoreTypeItem{
            static_cast<int>(configType),
            static_cast<int>(effectiveCore)});
    }

    return items;
}

inline TunModeItem normalizedTunModeItem(TunModeItem item)
{
    item.stack = item.stack.trimmed();
    if (item.stack.isEmpty()) {
        item.stack = QStringLiteral("system");
    }

    if (item.mtu <= 0) {
        item.mtu = 9000;
    }

    item.icmpRouting = item.icmpRouting.trimmed();
    if (item.icmpRouting.isEmpty()) {
        item.icmpRouting = QStringLiteral("rule");
    }

    item.udpRouting = item.udpRouting.trimmed();
    if (item.udpRouting.isEmpty()) {
        item.udpRouting = QStringLiteral("direct");
    }

    return item;
}

struct TunSettingsApplyDecision {
    bool tunSettingsChanged = false;
    bool tunRuntimeChanged = false;
    bool requiresAdminForConfiguredTun = false;
    bool shouldRestartRunningCore = false;
};

struct TunSettingsSaveBehavior {
    Config configToPersist;
    TunSettingsApplyDecision applyDecision;
    bool shouldPromptForAdminRestart = false;
};

inline bool areTunModeItemsEqual(const TunModeItem& lhs, const TunModeItem& rhs)
{
    const TunModeItem normalizedLhs = normalizedTunModeItem(lhs);
    const TunModeItem normalizedRhs = normalizedTunModeItem(rhs);
    return normalizedLhs.enableTun == normalizedRhs.enableTun
        && normalizedLhs.autoRoute == normalizedRhs.autoRoute
        && normalizedLhs.strictRoute == normalizedRhs.strictRoute
        && normalizedLhs.stack == normalizedRhs.stack
        && normalizedLhs.mtu == normalizedRhs.mtu
        && normalizedLhs.enableIPv6Address == normalizedRhs.enableIPv6Address
        && normalizedLhs.icmpRouting == normalizedRhs.icmpRouting
        && normalizedLhs.udpRouting == normalizedRhs.udpRouting
        && normalizedLhs.enableLegacyProtect == normalizedRhs.enableLegacyProtect;
}

inline TunSettingsApplyDecision evaluateTunSettingsApply(
    const Config& previous,
    const Config& updated,
    bool isWindows,
    bool isProcessElevated,
    bool isCoreRunning)
{
    TunSettingsApplyDecision decision;
    decision.tunSettingsChanged = !areTunModeItemsEqual(previous.tun().tunModeItem, updated.tun().tunModeItem);
    decision.tunRuntimeChanged = decision.tunSettingsChanged
        && (previous.tun().tunModeItem.enableTun || updated.tun().tunModeItem.enableTun);
    decision.requiresAdminForConfiguredTun =
        isWindows && updated.tun().tunModeItem.enableTun && !isProcessElevated;
    decision.shouldRestartRunningCore =
        decision.tunRuntimeChanged && isCoreRunning && !decision.requiresAdminForConfiguredTun;
    return decision;
}

inline bool isTunRuntimeBlocked(const Config& config, bool isWindows, bool isProcessElevated)
{
    return isWindows && config.tun().tunModeItem.enableTun && !isProcessElevated;
}

inline Config prepareTunToggleConfigForSave(
    Config config,
    bool enabled,
    bool isWindows,
    bool isProcessElevated)
{
    config.tun().tunModeItem.enableTun = enabled;
    if (enabled && isWindows && !isProcessElevated) {
        config.ui().mainProxyEnabled = true;
        config.sysProxyType = toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange);
    }
    return config;
}

inline TunSettingsSaveBehavior evaluateTunSettingsSaveBehavior(
    const Config& previous,
    const Config& updated,
    bool isWindows,
    bool isProcessElevated,
    bool isCoreRunning)
{
    TunSettingsSaveBehavior behavior;
    behavior.configToPersist = updated;
    behavior.applyDecision = evaluateTunSettingsApply(
        previous,
        updated,
        isWindows,
        isProcessElevated,
        isCoreRunning);
    behavior.shouldPromptForAdminRestart = behavior.applyDecision.requiresAdminForConfiguredTun;
    return behavior;
}

inline bool shouldHotApplyRuntimeSettings(
    const Config& previous,
    const Config& updated,
    const TunSettingsApplyDecision& tunDecision)
{
    const int previousLocalPort = previous.localPort > 0 ? previous.localPort : 10808;
    const int updatedLocalPort = updated.localPort > 0 ? updated.localPort : 10808;
    const QList<CoreTypeItem> previousCoreTypeItems = normalizedCoreTypeItemsForComparison(previous);
    const QList<CoreTypeItem> updatedCoreTypeItems = normalizedCoreTypeItemsForComparison(updated);
    return !tunDecision.requiresAdminForConfiguredTun
        && (previousLocalPort != updatedLocalPort
            || previous.allowLanConnection != updated.allowLanConnection
            || previousCoreTypeItems != updatedCoreTypeItems
            || previous.collection().routingModeId != updated.collection().routingModeId
            || previous.collection().customRoutingItems != updated.collection().customRoutingItems
            || previous.collection().routingCustomRules != updated.collection().routingCustomRules
            || tunDecision.tunRuntimeChanged);
}
