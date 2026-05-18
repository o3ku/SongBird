#pragma once

#include <QString>

#include "app/TunSettingsApplyDecision.h"
#include "domain/models/Config.h"

struct SettingsDialogApplyPlan {
    TunSettingsSaveBehavior tunSaveBehavior;
    bool autoRunChanged = false;
    bool systemProxySettingsChanged = false;
    bool languageChanged = false;
    bool runtimeSettingsChanged = false;
};

inline QString normalizeSettingsLanguageCode(QString value)
{
    value = value.trimmed().toLower();
    value.replace(QChar('-'), QChar('_'));
    return value;
}

inline SettingsDialogApplyPlan evaluateSettingsDialogApplyPlan(
    const Config& previousConfig,
    const Config& updatedConfig,
    bool isWindows,
    bool isProcessElevated,
    bool isCoreRunning)
{
    SettingsDialogApplyPlan plan;
    plan.tunSaveBehavior = evaluateTunSettingsSaveBehavior(
        previousConfig,
        updatedConfig,
        isWindows,
        isProcessElevated,
        isCoreRunning);
    plan.runtimeSettingsChanged = shouldHotApplyRuntimeSettings(
        previousConfig,
        updatedConfig,
        plan.tunSaveBehavior.applyDecision);
    plan.autoRunChanged = previousConfig.autoRunEnabled != updatedConfig.autoRunEnabled;
    plan.systemProxySettingsChanged = previousConfig.localPort != updatedConfig.localPort
        || previousConfig.systemProxyExceptions != updatedConfig.systemProxyExceptions
        || previousConfig.systemProxyAdvancedProtocol != updatedConfig.systemProxyAdvancedProtocol;
    plan.languageChanged =
        normalizeSettingsLanguageCode(previousConfig.languageCode)
        != normalizeSettingsLanguageCode(updatedConfig.languageCode);
    return plan;
}
