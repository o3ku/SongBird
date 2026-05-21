#include <QtTest>

#include "app/TunSettingsApplyDecision.h"

class TunSettingsApplyDecisionTests : public QObject {
    Q_OBJECT

private slots:
    void enablingTunWhileCoreRunningRequestsRestartWhenElevated();
    void enablingTunWithoutElevationBlocksRuntimeApplyOnWindows();
    void enablingTunWithoutElevationPreservesConfiguredTunForSave();
    void enablingTunWithoutElevationAlsoPersistsProxyOn();
    void changingTunParametersWhileTunDisabledDoesNotCountAsRuntimeChange();
    void configuredTunWithoutElevationBlocksOtherRuntimeHotApply();
};

void TunSettingsApplyDecisionTests::enablingTunWhileCoreRunningRequestsRestartWhenElevated()
{
    Config previous;
    Config updated = previous;
    updated.tun().tunModeItem.enableTun = true;

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
    updated.tun().tunModeItem.enableTun = true;

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
    updated.tun().tunModeItem.enableTun = true;
    updated.tun().tunModeItem.autoRoute = true;

    const TunSettingsSaveBehavior behavior =
        evaluateTunSettingsSaveBehavior(previous, updated, true, false, true);

    QVERIFY(behavior.configToPersist.tun().tunModeItem.enableTun);
    QVERIFY(behavior.configToPersist.tun().tunModeItem.autoRoute);
    QVERIFY(behavior.applyDecision.requiresAdminForConfiguredTun);
    QVERIFY(!behavior.applyDecision.shouldRestartRunningCore);
    QVERIFY(behavior.shouldPromptForAdminRestart);
}

void TunSettingsApplyDecisionTests::enablingTunWithoutElevationAlsoPersistsProxyOn()
{
    Config previous;

    const Config updated = prepareTunToggleConfigForSave(previous, true, true, false);

    QVERIFY(updated.tun().tunModeItem.enableTun);
    QVERIFY(updated.ui().mainProxyEnabled);
    QCOMPARE(updated.sysProxyType, toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange));

    const TunSettingsSaveBehavior behavior =
        evaluateTunSettingsSaveBehavior(previous, updated, true, false, true);

    QVERIFY(behavior.configToPersist.tun().tunModeItem.enableTun);
    QVERIFY(behavior.configToPersist.ui().mainProxyEnabled);
    QCOMPARE(
        behavior.configToPersist.sysProxyType,
        toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange));
    QVERIFY(behavior.shouldPromptForAdminRestart);
}

void TunSettingsApplyDecisionTests::changingTunParametersWhileTunDisabledDoesNotCountAsRuntimeChange()
{
    Config previous;
    Config updated = previous;
    updated.tun().tunModeItem.mtu = 1500;
    updated.tun().tunModeItem.autoRoute = true;

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
    previous.tun().tunModeItem.enableTun = true;

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
