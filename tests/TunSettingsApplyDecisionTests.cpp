#include <QtTest>

#include "app/TunSettingsApplyDecision.h"

class TunSettingsApplyDecisionTests : public QObject {
    Q_OBJECT

private slots:
    void enablingTunWhileCoreRunningRequestsRestartWhenElevated();
    void enablingTunWithoutElevationBlocksRuntimeApplyOnWindows();
    void enablingTunWithoutElevationPreservesConfiguredTunForSave();
    void changingTunParametersWhileTunDisabledDoesNotCountAsRuntimeChange();
    void configuredTunWithoutElevationBlocksOtherRuntimeHotApply();
};

void TunSettingsApplyDecisionTests::enablingTunWhileCoreRunningRequestsRestartWhenElevated()
{
    Config previous;
    Config updated = previous;
    updated.tunModeItem.enableTun = true;

    const TunSettingsApplyDecision decision =
        evaluateTunSettingsApply(previous, updated, true, true, true);

    QVERIFY(decision.tunSettingsChanged);
    QVERIFY(decision.tunRuntimeChanged);
    QVERIFY(!decision.requiresAdminForConfiguredTun);
    QVERIFY(decision.shouldRestartRunningCore);
}

void TunSettingsApplyDecisionTests::enablingTunWithoutElevationBlocksRuntimeApplyOnWindows()
{
    Config previous;
    Config updated = previous;
    updated.tunModeItem.enableTun = true;

    const TunSettingsApplyDecision decision =
        evaluateTunSettingsApply(previous, updated, true, false, true);

    QVERIFY(decision.tunSettingsChanged);
    QVERIFY(decision.tunRuntimeChanged);
    QVERIFY(decision.requiresAdminForConfiguredTun);
    QVERIFY(!decision.shouldRestartRunningCore);
    QVERIFY(isTunRuntimeBlocked(updated, true, false));
}

void TunSettingsApplyDecisionTests::enablingTunWithoutElevationPreservesConfiguredTunForSave()
{
    Config previous;
    Config updated = previous;
    updated.tunModeItem.enableTun = true;
    updated.tunModeItem.autoRoute = true;

    const TunSettingsSaveBehavior behavior =
        evaluateTunSettingsSaveBehavior(previous, updated, true, false, true);

    QVERIFY(behavior.configToPersist.tunModeItem.enableTun);
    QVERIFY(behavior.configToPersist.tunModeItem.autoRoute);
    QVERIFY(behavior.applyDecision.requiresAdminForConfiguredTun);
    QVERIFY(!behavior.applyDecision.shouldRestartRunningCore);
    QVERIFY(behavior.shouldPromptForAdminRestart);
}

void TunSettingsApplyDecisionTests::changingTunParametersWhileTunDisabledDoesNotCountAsRuntimeChange()
{
    Config previous;
    Config updated = previous;
    updated.tunModeItem.mtu = 1500;
    updated.tunModeItem.autoRoute = true;

    const TunSettingsApplyDecision decision =
        evaluateTunSettingsApply(previous, updated, true, true, true);

    QVERIFY(decision.tunSettingsChanged);
    QVERIFY(!decision.tunRuntimeChanged);
    QVERIFY(!decision.requiresAdminForConfiguredTun);
    QVERIFY(!decision.shouldRestartRunningCore);
}

void TunSettingsApplyDecisionTests::configuredTunWithoutElevationBlocksOtherRuntimeHotApply()
{
    Config previous;
    previous.tunModeItem.enableTun = true;

    Config updated = previous;
    updated.localPort = previous.localPort + 1;

    const TunSettingsApplyDecision decision =
        evaluateTunSettingsApply(previous, updated, true, false, true);

    QVERIFY(!decision.tunSettingsChanged);
    QVERIFY(decision.requiresAdminForConfiguredTun);
    QVERIFY(!shouldHotApplyRuntimeSettings(previous, updated, decision));
}

QTEST_MAIN(TunSettingsApplyDecisionTests)

#include "TunSettingsApplyDecisionTests.moc"
