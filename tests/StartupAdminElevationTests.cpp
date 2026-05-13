#include <QtTest>
#include <QJsonObject>

#include "app/StartupAdminElevation.h"

class StartupAdminElevationTests : public QObject {
    Q_OBJECT

private slots:
    void promptsForAdminRestartOnInteractiveWindowsStartup();
    void skipsPromptWhenTunIsNotConfigured();
    void skipsPromptOutsideInteractiveUnelevatedWindowsStartup();
    void skipsPromptWhenNonInteractiveEnvironmentIsSet();
    void readsConfiguredTunFlagFromJson();
    void relaunchArgumentsDropExecutablePath();
    void inAppAdminRelaunchArgumentsAppendAdminRelaunchMarkerOnce();
    void inAppNonAdminRelaunchArgumentsDropAdminRelaunchMarker();
    void inAppRelaunchArgumentsReplacePreviousRestartContext();
    void languageRestartPromptIsSkippedAfterAdminRelaunchStarts();
    void languageRestartPromptStillShowsWithoutAdminRelaunch();
    void restartContextRetriesSingleInstanceAcquire();
    void disableSingleInstanceBypassesAcquireChecks();
    void parsesRestartWaitPidArgument();
    void windowsShellParametersQuoteWhitespaceAndQuotes();
};

void StartupAdminElevationTests::promptsForAdminRestartOnInteractiveWindowsStartup()
{
    const StartupAdminElevationDecision decision =
        evaluateStartupAdminElevation(true, false, true, true, QStringList{
                                                                  QStringLiteral("C:/tools/v2rayq.exe"),
                                                                  QStringLiteral("--config"),
                                                                  QStringLiteral("C:/cfg/guiNConfig.json")});

    QVERIFY(decision.shouldPromptForElevation);
    QCOMPARE(
        decision.relaunchArguments,
        QStringList({QStringLiteral("--config"), QStringLiteral("C:/cfg/guiNConfig.json")}));
}

void StartupAdminElevationTests::skipsPromptWhenTunIsNotConfigured()
{
    const StartupAdminElevationDecision decision =
        evaluateStartupAdminElevation(true, false, true, false, QStringList{
                                                                   QStringLiteral("C:/tools/v2rayq.exe"),
                                                                   QStringLiteral("--config"),
                                                                   QStringLiteral("C:/cfg/guiNConfig.json")});

    QVERIFY(!decision.shouldPromptForElevation);
    QCOMPARE(
        decision.relaunchArguments,
        QStringList({QStringLiteral("--config"), QStringLiteral("C:/cfg/guiNConfig.json")}));
}

void StartupAdminElevationTests::skipsPromptOutsideInteractiveUnelevatedWindowsStartup()
{
    QVERIFY(!evaluateStartupAdminElevation(true, true, true, true, {}).shouldPromptForElevation);
    QVERIFY(!evaluateStartupAdminElevation(true, false, false, true, {}).shouldPromptForElevation);
    QVERIFY(!evaluateStartupAdminElevation(false, false, true, true, {}).shouldPromptForElevation);
}

void StartupAdminElevationTests::skipsPromptWhenNonInteractiveEnvironmentIsSet()
{
    const StartupAdminElevationDecision decision = evaluateStartupAdminElevation(
        true,
        false,
        startupAdminInteractivePromptsEnabled(false, true),
        true,
        QStringList{QStringLiteral("C:/tools/v2rayq.exe")});

    QVERIFY(!decision.shouldPromptForElevation);
}

void StartupAdminElevationTests::readsConfiguredTunFlagFromJson()
{
    QVERIFY(startupConfigHasTunEnabled(QByteArrayLiteral(R"({
        "tunModeItem": {
            "enableTun": true
        }
    })")));
    QVERIFY(!startupConfigHasTunEnabled(QByteArrayLiteral(R"({
        "tunModeItem": {
            "enableTun": false
        }
    })")));
    QVERIFY(!startupConfigHasTunEnabled(QByteArrayLiteral("{}")));
    QVERIFY(!startupConfigHasTunEnabled(QByteArrayLiteral("[]")));
    QVERIFY(!startupConfigHasTunEnabled(QByteArrayLiteral("{invalid")));
}

void StartupAdminElevationTests::relaunchArgumentsDropExecutablePath()
{
    const StartupAdminElevationDecision decision = evaluateStartupAdminElevation(
        true,
        false,
        true,
        true,
        QStringList{
            QStringLiteral("C:/Program Files/v2rayq/v2rayq.exe"),
            QStringLiteral("--config=C:/Program Files/v2rayq/guiNConfig.json"),
            QStringLiteral("--start-hidden"),
            QStringLiteral("--disable-single-instance")});

    QCOMPARE(
        decision.relaunchArguments,
        QStringList({
            QStringLiteral("--config=C:/Program Files/v2rayq/guiNConfig.json"),
            QStringLiteral("--start-hidden"),
            QStringLiteral("--disable-single-instance")}));
}

void StartupAdminElevationTests::inAppAdminRelaunchArgumentsAppendAdminRelaunchMarkerOnce()
{
    const QStringList arguments = startupRelaunchArgumentsForRunningInstance(
        QStringList{
            QStringLiteral("C:/Program Files/v2rayq/v2rayq.exe"),
            QStringLiteral("--config=C:/Program Files/v2rayq/guiNConfig.json"),
            QStringLiteral("--start-hidden")},
        true,
        4321);

    QCOMPARE(
        arguments,
        QStringList({
            QStringLiteral("--config=C:/Program Files/v2rayq/guiNConfig.json"),
            QStringLiteral("--start-hidden"),
            QStringLiteral("--restart-wait-pid=4321"),
            QStringLiteral("--admin-relaunch")}));

    const QStringList alreadyBypassed = startupRelaunchArgumentsForRunningInstance(
        QStringList{
            QStringLiteral("C:/Program Files/v2rayq/v2rayq.exe"),
            QStringLiteral("--config=C:/Program Files/v2rayq/guiNConfig.json"),
            QStringLiteral("--admin-relaunch"),
            QStringLiteral("--restart-wait-pid=1111")},
        true,
        4321);

    QCOMPARE(
        alreadyBypassed,
        QStringList({
            QStringLiteral("--config=C:/Program Files/v2rayq/guiNConfig.json"),
            QStringLiteral("--restart-wait-pid=4321"),
            QStringLiteral("--admin-relaunch")}));
}

void StartupAdminElevationTests::inAppNonAdminRelaunchArgumentsDropAdminRelaunchMarker()
{
    const QStringList arguments = startupRelaunchArgumentsForRunningInstance(
        QStringList{
            QStringLiteral("C:/Program Files/v2rayq/v2rayq.exe"),
            QStringLiteral("--config=C:/Program Files/v2rayq/guiNConfig.json"),
            QStringLiteral("--admin-relaunch"),
            QStringLiteral("--start-hidden")},
        false,
        4321);

    QCOMPARE(
        arguments,
        QStringList({
            QStringLiteral("--config=C:/Program Files/v2rayq/guiNConfig.json"),
            QStringLiteral("--start-hidden"),
            QStringLiteral("--restart-wait-pid=4321")}));
}

void StartupAdminElevationTests::inAppRelaunchArgumentsReplacePreviousRestartContext()
{
    const QStringList arguments = startupRelaunchArgumentsForRunningInstance(
        QStringList{
            QStringLiteral("C:/Program Files/v2rayq/v2rayq.exe"),
            QStringLiteral("--config=C:/Program Files/v2rayq/guiNConfig.json"),
            QStringLiteral("--restart-wait-pid=1111"),
            QStringLiteral("--start-hidden")},
        true,
        4321);

    QCOMPARE(
        arguments,
        QStringList({
            QStringLiteral("--config=C:/Program Files/v2rayq/guiNConfig.json"),
            QStringLiteral("--start-hidden"),
            QStringLiteral("--restart-wait-pid=4321"),
            QStringLiteral("--admin-relaunch")}));
}

void StartupAdminElevationTests::languageRestartPromptIsSkippedAfterAdminRelaunchStarts()
{
    QVERIFY(!shouldPromptForLanguageRestartAfterSettingsSave(true, true));
}

void StartupAdminElevationTests::languageRestartPromptStillShowsWithoutAdminRelaunch()
{
    QVERIFY(shouldPromptForLanguageRestartAfterSettingsSave(true, false));
    QVERIFY(!shouldPromptForLanguageRestartAfterSettingsSave(false, false));
}

void StartupAdminElevationTests::restartContextRetriesSingleInstanceAcquire()
{
    const StartupSingleInstanceAcquireDecision decision =
        evaluateStartupSingleInstanceAcquireDecision(false, true);

    QVERIFY(!decision.shouldBypassSingleInstance);
    QVERIFY(decision.maxAcquireAttempts > 1);
    QVERIFY(decision.retryIntervalMs > 0);
}

void StartupAdminElevationTests::disableSingleInstanceBypassesAcquireChecks()
{
    const StartupSingleInstanceAcquireDecision decision =
        evaluateStartupSingleInstanceAcquireDecision(true, true);
    int acquireCalls = 0;
    int sleepCalls = 0;

    const bool acquired = tryAcquireStartupSingleInstance(
        decision,
        [&acquireCalls]() {
            ++acquireCalls;
            return false;
        },
        [&sleepCalls](int) {
            ++sleepCalls;
        });

    QVERIFY(acquired);
    QCOMPARE(acquireCalls, 0);
    QCOMPARE(sleepCalls, 0);
}

void StartupAdminElevationTests::parsesRestartWaitPidArgument()
{
    QCOMPARE(parseRestartWaitPidArgument(QStringLiteral("4321")), 4321);
    QCOMPARE(parseRestartWaitPidArgument(QStringLiteral("0")), 0);
    QCOMPARE(parseRestartWaitPidArgument(QStringLiteral("-2")), 0);
    QCOMPARE(parseRestartWaitPidArgument(QStringLiteral("abc")), 0);
}

void StartupAdminElevationTests::windowsShellParametersQuoteWhitespaceAndQuotes()
{
    const QString parameters = buildWindowsShellExecuteParameters(QStringList{
        QStringLiteral("--config"),
        QStringLiteral("C:/Program Files/v2rayq/gui \"special\".json"),
        QStringLiteral("--start-hidden"),
        QStringLiteral("plain")});

    QCOMPARE(
        parameters,
        QStringLiteral("--config \"C:/Program Files/v2rayq/gui \\\"special\\\".json\" --start-hidden plain"));
}

QTEST_MAIN(StartupAdminElevationTests)

#include "StartupAdminElevationTests.moc"
