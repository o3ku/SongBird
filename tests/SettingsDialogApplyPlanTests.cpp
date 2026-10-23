#include <QtTest>

#include "app/SettingsDialogApplyPlan.h"
#include "appcore/StartupAdminElevation.h"
#include "common/LocalPortOption.h"

class SettingsDialogApplyPlanTests : public QObject {
    Q_OBJECT

private slots:
    void computesTunProxyAndLanguageFlagsFromUpdatedSettings();
    void blocksRuntimeHotApplyWhenConfiguredTunNeedsElevation();
    void normalizesLanguageCodesBeforeRestartPromptComparison();
    void detectsThemeChangeAfterNormalizingNames();
    void treatsALoneFollowSystemThemeChoiceAsAChange();
    void treatsALoneSkipOutboundCheckToggleAsAChange();
    void treatsALoneSpeedTestUrlEditAsAChange();
    void ignoresWhitespaceOnlySpeedTestUrlEdits();
    void treatsALoneProxyServerDnsEditAsAChange();
    void treatsAnUnsetLocalPortAsTheDefaultItStandsFor();
    void treatsARealLocalPortEditAsAProxyAndRuntimeChange();
    void treatsAnOutOfRangeLocalPortAsUnset();
};

void SettingsDialogApplyPlanTests::treatsAnUnsetLocalPortAsTheDefaultItStandsFor()
{
    // 0 on disk means "never set", and the settings spin box shows it as 10808. The general-page
    // comparison already normalised it; the system-proxy comparison did not, so the same unchanged
    // pair of configs answered "equal" and "changed" to two different questions and a save with
    // nothing edited reapplied the system proxy.
    Config previous;
    previous.localPort = 0;

    Config updated = previous;
    updated.localPort = kDefaultLocalPort;

    const SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, true, true);

    QVERIFY(!plan.dirty);
    QVERIFY(!plan.systemProxySettingsChanged);
    QVERIFY(!plan.runtimeSettingsChanged);
}

void SettingsDialogApplyPlanTests::treatsARealLocalPortEditAsAProxyAndRuntimeChange()
{
    Config previous;
    previous.localPort = 0;

    Config updated = previous;
    updated.localPort = 20808;

    const SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, true, true);

    QVERIFY(plan.dirty);
    QVERIFY(plan.systemProxySettingsChanged);
    QVERIFY(plan.runtimeSettingsChanged);
}

void SettingsDialogApplyPlanTests::treatsAnOutOfRangeLocalPortAsUnset()
{
    // A port outside 1..65535 is not a port. Folding it into the default is what the spin box
    // does, so the plan has to agree -- otherwise the dialog shows 10808, the plan sees -1, and
    // OK is treated as a change that was never made.
    Config previous;
    previous.localPort = -1;

    Config updated = previous;
    updated.localPort = kDefaultLocalPort;

    const SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, true, true);

    QVERIFY(!plan.dirty);
    QVERIFY(!plan.systemProxySettingsChanged);
}

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

// Same shape as the General-page regressions below, but for the theme preference.
// "System" used to be folded into "Light", so choosing Follow system (or leaving
// it) looked like no change: the plan stayed clean, the save step returned without
// writing, and the choice was silently dropped on the next launch.
void SettingsDialogApplyPlanTests::treatsALoneFollowSystemThemeChoiceAsAChange()
{
    const auto planFor = [](const QString& previousTheme, const QString& updatedTheme) {
        Config previous;
        previous.ui().themeName = previousTheme;

        Config updated = previous;
        updated.ui().themeName = updatedTheme;

        return evaluateSettingsDialogApplyPlan(previous, updated, true, true, false);
    };

    const SettingsDialogApplyPlan toSystem = planFor(QStringLiteral("Light"), QStringLiteral("System"));
    QVERIFY(toSystem.dirty);
    QVERIFY(toSystem.themeChanged);

    // The reverse has to persist too, otherwise a user who tries Follow system and
    // then goes back cannot undo it.
    const SettingsDialogApplyPlan fromSystem = planFor(QStringLiteral("System"), QStringLiteral("Light"));
    QVERIFY(fromSystem.dirty);
    QVERIFY(fromSystem.themeChanged);

    // Aliases the rest of the theme code accepts resolve to the same preference, so
    // they must not read as a change from an already-stored "System".
    const SettingsDialogApplyPlan alias = planFor(QStringLiteral("System"), QStringLiteral("follow"));
    QVERIFY(!alias.dirty);
    QVERIFY(!alias.themeChanged);
}

// The settings save step returns without persisting when the plan is not dirty,
// so every field the General page writes has to reach the comparison. These two
// fell outside it once: checking the box (or editing the speed test URL) alone
// pressed OK into a no-op, and the dialog reopened with the edit discarded.
void SettingsDialogApplyPlanTests::treatsALoneSkipOutboundCheckToggleAsAChange()
{
    Config previous;

    Config updated = previous;
    updated.skipOutboundCheck = true;

    const SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, true, true);

    QVERIFY(plan.dirty);
    // Neither field is baked into the generated core config -- both are read at
    // activation time -- so saving must not restart a running core.
    QVERIFY(!plan.runtimeSettingsChanged);
}

void SettingsDialogApplyPlanTests::treatsALoneSpeedTestUrlEditAsAChange()
{
    Config previous;

    Config updated = previous;
    updated.defaults().speedPingTestUrl = QStringLiteral("https://mirror.example/generate_204");

    const SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, true, true);

    QVERIFY(plan.dirty);
    QVERIFY(!plan.runtimeSettingsChanged);
}

void SettingsDialogApplyPlanTests::ignoresWhitespaceOnlySpeedTestUrlEdits()
{
    Config previous;
    previous.defaults().speedPingTestUrl = QStringLiteral("https://mirror.example/generate_204");

    Config updated = previous;
    updated.defaults().speedPingTestUrl = QStringLiteral("  https://mirror.example/generate_204  ");

    const SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, true, true);

    // The page trims before writing, so surrounding whitespace is not a change.
    QVERIFY(!plan.dirty);
}

void SettingsDialogApplyPlanTests::treatsALoneProxyServerDnsEditAsAChange()
{
    Config previous;

    Config updated = previous;
    updated.dns().proxyServerDns = QStringLiteral("https://1.1.1.1/dns-query");

    const SettingsDialogApplyPlan plan =
        evaluateSettingsDialogApplyPlan(previous, updated, true, true, true);

    // Same shape as the skip-outbound regression above: the DNS page writes this
    // field, so a lone edit must persist (dirty) and must restart a running core
    // (the value is baked into the generated sing-box DNS config).
    QVERIFY(plan.dirty);
    QVERIFY(plan.runtimeSettingsChanged);
}

QTEST_MAIN(SettingsDialogApplyPlanTests)

#include "SettingsDialogApplyPlanTests.moc"
