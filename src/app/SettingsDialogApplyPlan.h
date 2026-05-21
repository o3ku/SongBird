#pragma once

#include <QString>

#include "app/TunSettingsApplyDecision.h"
#include "domain/models/Config.h"

struct SettingsDialogApplyPlan {
    TunSettingsSaveBehavior tunSaveBehavior;
    bool autoRunChanged = false;
    bool systemProxySettingsChanged = false;
    bool languageChanged = false;
    bool themeChanged = false;
    bool runtimeSettingsChanged = false;
};

inline QString normalizeSettingsLanguageCode(QString value)
{
    value = value.trimmed().toLower();
    value.replace(QChar('-'), QChar('_'));
    return value;
}

inline QString normalizeSettingsThemeName(QString value)
{
    value = value.trimmed();
    if (value.compare(QStringLiteral("Dark"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Dark");
    }
    return QStringLiteral("Light");
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
    plan.autoRunChanged = previousConfig.ui().autoRunEnabled != updatedConfig.ui().autoRunEnabled;
    plan.systemProxySettingsChanged = previousConfig.localPort != updatedConfig.localPort
        || previousConfig.systemProxyAdvancedProtocol != updatedConfig.systemProxyAdvancedProtocol;
    plan.languageChanged =
        normalizeSettingsLanguageCode(previousConfig.ui().languageCode)
        != normalizeSettingsLanguageCode(updatedConfig.ui().languageCode);
    plan.themeChanged =
        normalizeSettingsThemeName(previousConfig.ui().themeName)
        != normalizeSettingsThemeName(updatedConfig.ui().themeName);
    return plan;
}
