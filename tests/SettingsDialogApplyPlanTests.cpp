#include <QtTest>

#include "app/SettingsDialogApplyPlan.h"
#include "app/StartupAdminElevation.h"

class SettingsDialogApplyPlanTests : public QObject {
    Q_OBJECT

private slots:
    void computesTunProxyAndLanguageFlagsFromUpdatedSettings();
    void blocksRuntimeHotApplyWhenConfiguredTunNeedsElevation();
    void normalizesLanguageCodesBeforeRestartPromptComparison();
    void detectsThemeChangeAfterNormalizingNames();
};

void SettingsDialogApplyPlanTests::computesTunProxyAndLanguageFlagsFromUpdatedSettings()
{
    Config previous;
    previous.localPort = 10808;
    previous.ui().autoRunEnabled = false;
    previous.ui().languageCode = QStringLiteral("en");
    previous.systemProxyAdvancedProtocol = QStringLiteral("{ip}:{http_port}");

    Config updated = previous;
    updated.localPort = 20808;
    updated.ui().autoRunEnabled = true;
    updated.ui().languageCode = QStringLiteral("zh-CN");
    updated.tun().tunModeItem.enableTun = true;

    const SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, true, true);

    QVERIFY(plan.autoRunChanged);
    QVERIFY(plan.systemProxySettingsChanged);
    QVERIFY(plan.languageChanged);
    QVERIFY(plan.runtimeSettingsChanged);
    QVERIFY(plan.tunSaveBehavior.applyDecision.tunSettingsChanged);
    QVERIFY(plan.tunSaveBehavior.applyDecision.shouldRestartRunningCore);
}

void SettingsDialogApplyPlanTests::blocksRuntimeHotApplyWhenConfiguredTunNeedsElevation()
{
    Config previous;

    Config updated = previous;
    updated.localPort = previous.localPort + 1;
    updated.tun().tunModeItem.enableTun = true;

    const SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, false, true);

    QVERIFY(plan.systemProxySettingsChanged);
    QVERIFY(!plan.runtimeSettingsChanged);
    QVERIFY(plan.tunSaveBehavior.shouldPromptForAdminRestart);
    QVERIFY(plan.tunSaveBehavior.applyDecision.requiresAdminForConfiguredTun);
}

void SettingsDialogApplyPlanTests::normalizesLanguageCodesBeforeRestartPromptComparison()
{
    Config previous;
    previous.ui().languageCode = QStringLiteral("zh_CN");

    Config updated = previous;
    updated.ui().languageCode = QStringLiteral("zh-CN");

    const SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, true, false);

    QVERIFY(!plan.languageChanged);
    QVERIFY(!shouldPromptForLanguageRestartAfterSettingsSave(plan.languageChanged, false));
}

void SettingsDialogApplyPlanTests::detectsThemeChangeAfterNormalizingNames()
{
    Config previous;
    previous.ui().themeName = QStringLiteral("light");

    Config updated = previous;
    updated.ui().themeName = QStringLiteral("Dark");

    SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, true, false);
    QVERIFY(plan.themeChanged);

    updated.ui().themeName = QStringLiteral("LIGHT");
    plan = evaluateSettingsDialogApplyPlan(previous, updated, true, true, false);
    QVERIFY(!plan.themeChanged);
}

QTEST_MAIN(SettingsDialogApplyPlanTests)

#include "SettingsDialogApplyPlanTests.moc"
