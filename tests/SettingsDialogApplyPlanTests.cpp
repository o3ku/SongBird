#include <QtTest>

#include "app/SettingsDialogApplyPlan.h"
#include "app/StartupAdminElevation.h"

class SettingsDialogApplyPlanTests : public QObject {
    Q_OBJECT

private slots:
    void computesTunProxyAndLanguageFlagsFromUpdatedSettings();
    void blocksRuntimeHotApplyWhenConfiguredTunNeedsElevation();
    void normalizesLanguageCodesBeforeRestartPromptComparison();
};

void SettingsDialogApplyPlanTests::computesTunProxyAndLanguageFlagsFromUpdatedSettings()
{
    Config previous;
    previous.localPort = 10808;
    previous.autoRunEnabled = false;
    previous.languageCode = QStringLiteral("en");
    previous.systemProxyExceptions = QStringLiteral("localhost");
    previous.systemProxyAdvancedProtocol = QStringLiteral("{ip}:{http_port}");

    Config updated = previous;
    updated.localPort = 20808;
    updated.autoRunEnabled = true;
    updated.languageCode = QStringLiteral("zh-CN");
    updated.systemProxyExceptions = QStringLiteral("localhost;127.*");
    updated.tunModeItem.enableTun = true;

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
    updated.tunModeItem.enableTun = true;

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
    previous.languageCode = QStringLiteral("zh_CN");

    Config updated = previous;
    updated.languageCode = QStringLiteral("zh-CN");

    const SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, true, false);

    QVERIFY(!plan.languageChanged);
    QVERIFY(!shouldPromptForLanguageRestartAfterSettingsSave(plan.languageChanged, false));
}

QTEST_MAIN(SettingsDialogApplyPlanTests)

#include "SettingsDialogApplyPlanTests.moc"
