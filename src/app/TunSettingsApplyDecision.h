#pragma once

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
    decision.tunSettingsChanged = !areTunModeItemsEqual(previous.tunModeItem, updated.tunModeItem);
    decision.tunRuntimeChanged = decision.tunSettingsChanged
        && (previous.tunModeItem.enableTun || updated.tunModeItem.enableTun);
    decision.requiresAdminForConfiguredTun =
        isWindows && updated.tunModeItem.enableTun && !isProcessElevated;
    decision.shouldRestartRunningCore =
        decision.tunRuntimeChanged && isCoreRunning && !decision.requiresAdminForConfiguredTun;
    return decision;
}

inline bool isTunRuntimeBlocked(const Config& config, bool isWindows, bool isProcessElevated)
{
    return isWindows && config.tunModeItem.enableTun && !isProcessElevated;
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
            || previous.enableStatistics != updated.enableStatistics
            || previous.statisticsFreshRate != updated.statisticsFreshRate
            || previous.coreTypeItems != updated.coreTypeItems
            || tunDecision.tunRuntimeChanged);
}
