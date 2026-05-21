#pragma once

#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"

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
    return lhs.enableTun == rhs.enableTun
        && lhs.autoRoute == rhs.autoRoute
        && lhs.strictRoute == rhs.strictRoute
        && lhs.stack == rhs.stack
        && lhs.mtu == rhs.mtu
        && lhs.enableIPv6Address == rhs.enableIPv6Address
        && lhs.icmpRouting == rhs.icmpRouting
        && lhs.enableLegacyProtect == rhs.enableLegacyProtect;
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
    return !tunDecision.requiresAdminForConfiguredTun
        && (previous.localPort != updated.localPort
            || previous.allowLanConnection != updated.allowLanConnection
            || previous.policy().coreTypeItems != updated.policy().coreTypeItems
            || previous.collection().enableRoutingAdvanced != updated.collection().enableRoutingAdvanced
            || previous.collection().routingIndex != updated.collection().routingIndex
            || previous.collection().routingItems != updated.collection().routingItems
            || previous.collection().routingCustomRules != updated.collection().routingCustomRules
            || tunDecision.tunRuntimeChanged);
}
