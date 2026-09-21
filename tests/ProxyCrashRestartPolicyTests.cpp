#include <QtTest>

#include <QProcess>
#include <QTranslator>

#include "app/ProxyCrashRestartPolicy.h"

namespace {

// Knows only the ProxySession context and tags whatever it is asked for, so a test can prove a
// message actually travelled through the translation machinery. The regression this guards
// against is a return to QStringLiteral(), which compiles and reads perfectly well but is
// never translated -- exactly how these five messages ended up English-only.
class TaggingTranslator : public QTranslator
{
public:
    QString translate(
        const char* context,
        const char* sourceText,
        const char* disambiguation,
        int n) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        if (qstrcmp(context, "ProxySession") != 0) {
            return {};
        }
        return QStringLiteral("[zh]%1").arg(QString::fromUtf8(sourceText));
    }
};

} // namespace

// Characterization tests for the crash-restart decision table.
//
// Why they exist: ProxyCrashRestartPolicy.cpp was listed in a test source set but no test case
// ever referenced it, so it had zero coverage -- which is what blocked rewriting its five
// user-visible log messages (they reach the log panel through ProxySession's `emit logMessage`).
// These pin the behaviour first, so the message rewrite that follows can be shown not to have
// moved any decision.
//
// The decision table, as read off the implementation:
//
//   1. !auxiliary && windows && tunEnabled && conflictDetected && retriesUsed < 1
//        -> ScheduleRestart, 1000 ms, auxiliary=false, consumes the one TUN retry
//   2. !auxiliary && conflictDetected            (retry already used, or off Windows / TUN off)
//        -> DisableAfterTunConflict, 0 ms
//   3. otherwise, per-core consecutive-failure count (reset when the run lasted > 60 s):
//        count > 5 -> DisableRestart, 0 ms
//        else      -> ScheduleRestart, min(3000 * count, 30000) ms
//
// The 30000 ms cap in (3) is unreachable: count never exceeds 5, so the backoff tops out at
// 15000 ms. That is pre-existing; these tests pin the values that can actually occur.
class ProxyCrashRestartPolicyTests : public QObject {
    Q_OBJECT

private slots:
    // --- the TUN adapter conflict path ---
    void retriesOnceWhenATunAdapterConflictHappensOnWindowsWithTunEnabled();
    void disablesRestartWhenTheTunAdapterConflictSurvivesTheRetry();
    void doesNotRetryTunConflictsOffWindows();
    void doesNotRetryTunConflictsWhenTunWasNotEnabled();
    void doesNotRetryTunConflictsForTheAuxiliaryCore();

    // --- the crash / exit restart path ---
    void backsOffThreeSecondsPerConsecutiveFailure();
    void givesUpAfterFiveConsecutiveFailures();
    void resetsTheFailureCountAfterAStableRun();
    void keepsTheMainAndAuxiliaryCountersIndependent();
    void reportsWhetherTheDecisionIsForTheAuxiliaryCore();

    // --- message content ---
    void namesTheCoreTheExitKindAndTheCodeInTheMessage();
    void routesTheRestartMessagesThroughTheTranslationMachinery();
};

void ProxyCrashRestartPolicyTests::retriesOnceWhenATunAdapterConflictHappensOnWindowsWithTunEnabled()
{
    ProxyCrashRestartPolicy policy;

    const ProxyCrashRestartPolicy::Decision decision =
        policy.decide(1, QProcess::CrashExit, false, true, true, true, 0);

    QCOMPARE(decision.action, ProxyCrashRestartPolicy::Action::ScheduleRestart);
    QCOMPARE(decision.delayMs, 1000);
    QVERIFY(!decision.auxiliary);
    QVERIFY(!decision.message.isEmpty());
}

void ProxyCrashRestartPolicyTests::disablesRestartWhenTheTunAdapterConflictSurvivesTheRetry()
{
    ProxyCrashRestartPolicy policy;

    // First conflict consumes the single retry.
    QCOMPARE(
        policy.decide(1, QProcess::CrashExit, false, true, true, true, 0).action,
        ProxyCrashRestartPolicy::Action::ScheduleRestart);

    const ProxyCrashRestartPolicy::Decision second =
        policy.decide(1, QProcess::CrashExit, false, true, true, true, 0);
    QCOMPARE(second.action, ProxyCrashRestartPolicy::Action::DisableAfterTunConflict);
    QCOMPARE(second.delayMs, 0);
    QVERIFY(!second.auxiliary);
    QVERIFY(!second.message.isEmpty());

    // resetTunConflictRetries() puts the retry back.
    policy.resetTunConflictRetries();
    QCOMPARE(
        policy.decide(1, QProcess::CrashExit, false, true, true, true, 0).action,
        ProxyCrashRestartPolicy::Action::ScheduleRestart);
}

void ProxyCrashRestartPolicyTests::doesNotRetryTunConflictsOffWindows()
{
    ProxyCrashRestartPolicy policy;

    const ProxyCrashRestartPolicy::Decision decision =
        policy.decide(1, QProcess::CrashExit, false, false, true, true, 0);

    QCOMPARE(decision.action, ProxyCrashRestartPolicy::Action::DisableAfterTunConflict);
}

void ProxyCrashRestartPolicyTests::doesNotRetryTunConflictsWhenTunWasNotEnabled()
{
    ProxyCrashRestartPolicy policy;

    const ProxyCrashRestartPolicy::Decision decision =
        policy.decide(1, QProcess::CrashExit, false, true, false, true, 0);

    QCOMPARE(decision.action, ProxyCrashRestartPolicy::Action::DisableAfterTunConflict);
}

void ProxyCrashRestartPolicyTests::doesNotRetryTunConflictsForTheAuxiliaryCore()
{
    ProxyCrashRestartPolicy policy;

    // A conflict reported for the auxiliary core must not take the TUN branch at all: it falls
    // through to the ordinary backoff, tagged as auxiliary.
    const ProxyCrashRestartPolicy::Decision decision =
        policy.decide(1, QProcess::CrashExit, true, true, true, true, 0);

    QCOMPARE(decision.action, ProxyCrashRestartPolicy::Action::ScheduleRestart);
    QCOMPARE(decision.delayMs, 3000);
    QVERIFY(decision.auxiliary);
}

void ProxyCrashRestartPolicyTests::backsOffThreeSecondsPerConsecutiveFailure()
{
    ProxyCrashRestartPolicy policy;

    const QList<int> expectedDelays{3000, 6000, 9000, 12000, 15000};
    for (int delayMs : expectedDelays) {
        const ProxyCrashRestartPolicy::Decision decision =
            policy.decide(1, QProcess::CrashExit, false, true, false, false, 0);
        QCOMPARE(decision.action, ProxyCrashRestartPolicy::Action::ScheduleRestart);
        QCOMPARE(decision.delayMs, delayMs);
    }
}

void ProxyCrashRestartPolicyTests::givesUpAfterFiveConsecutiveFailures()
{
    ProxyCrashRestartPolicy policy;

    for (int i = 0; i < 5; ++i) {
        QCOMPARE(
            policy.decide(1, QProcess::CrashExit, false, true, false, false, 0).action,
            ProxyCrashRestartPolicy::Action::ScheduleRestart);
    }

    const ProxyCrashRestartPolicy::Decision sixth =
        policy.decide(1, QProcess::CrashExit, false, true, false, false, 0);
    QCOMPARE(sixth.action, ProxyCrashRestartPolicy::Action::DisableRestart);
    QCOMPARE(sixth.delayMs, 0);
    QVERIFY(!sixth.message.isEmpty());
}

void ProxyCrashRestartPolicyTests::resetsTheFailureCountAfterAStableRun()
{
    ProxyCrashRestartPolicy policy;

    for (int i = 0; i < 5; ++i) {
        QCOMPARE(
            policy.decide(1, QProcess::CrashExit, false, true, false, false, 0).action,
            ProxyCrashRestartPolicy::Action::ScheduleRestart);
    }

    // A run longer than the 60 s stability threshold clears the count, so the next failure
    // starts the backoff over instead of giving up.
    const ProxyCrashRestartPolicy::Decision afterStableRun =
        policy.decide(1, QProcess::CrashExit, false, true, false, false, 60001);
    QCOMPARE(afterStableRun.action, ProxyCrashRestartPolicy::Action::ScheduleRestart);
    QCOMPARE(afterStableRun.delayMs, 3000);

    // Exactly at the threshold is not yet "stable".
    ProxyCrashRestartPolicy boundary;
    for (int i = 0; i < 5; ++i) {
        boundary.decide(1, QProcess::CrashExit, false, true, false, false, 0);
    }
    QCOMPARE(
        boundary.decide(1, QProcess::CrashExit, false, true, false, false, 60000).action,
        ProxyCrashRestartPolicy::Action::DisableRestart);
}

void ProxyCrashRestartPolicyTests::keepsTheMainAndAuxiliaryCountersIndependent()
{
    ProxyCrashRestartPolicy policy;

    for (int i = 0; i < 5; ++i) {
        policy.decide(1, QProcess::CrashExit, false, true, false, false, 0);
    }
    QCOMPARE(
        policy.decide(1, QProcess::CrashExit, false, true, false, false, 0).action,
        ProxyCrashRestartPolicy::Action::DisableRestart);

    // Exhausting the main core's budget must not touch the auxiliary core's.
    const ProxyCrashRestartPolicy::Decision auxiliary =
        policy.decide(1, QProcess::CrashExit, true, true, false, false, 0);
    QCOMPARE(auxiliary.action, ProxyCrashRestartPolicy::Action::ScheduleRestart);
    QCOMPARE(auxiliary.delayMs, 3000);
    QVERIFY(auxiliary.auxiliary);

    // ...and the other way round.
    ProxyCrashRestartPolicy mirrored;
    for (int i = 0; i < 5; ++i) {
        mirrored.decide(1, QProcess::CrashExit, true, true, false, false, 0);
    }
    QCOMPARE(
        mirrored.decide(1, QProcess::CrashExit, true, true, false, false, 0).action,
        ProxyCrashRestartPolicy::Action::DisableRestart);
    QCOMPARE(
        mirrored.decide(1, QProcess::CrashExit, false, true, false, false, 0).delayMs,
        3000);
}

void ProxyCrashRestartPolicyTests::reportsWhetherTheDecisionIsForTheAuxiliaryCore()
{
    ProxyCrashRestartPolicy policy;

    QVERIFY(!policy.decide(1, QProcess::CrashExit, false, true, false, false, 0).auxiliary);
    QVERIFY(policy.decide(1, QProcess::CrashExit, true, true, false, false, 0).auxiliary);
}

void ProxyCrashRestartPolicyTests::namesTheCoreTheExitKindAndTheCodeInTheMessage()
{
    // Each case gets a fresh policy so the consecutive-failure counters start at zero.
    ProxyCrashRestartPolicy crashMain;
    const QString crashMainMessage =
        crashMain.decide(7, QProcess::CrashExit, false, true, false, false, 0).message;
    QVERIFY(crashMainMessage.contains(QStringLiteral("Core crash detected (code=7).")));
    QVERIFY(crashMainMessage.contains(QStringLiteral("Restarting in 3s... (attempt 1/5)")));

    ProxyCrashRestartPolicy exitMain;
    const QString exitMainMessage =
        exitMain.decide(7, QProcess::NormalExit, false, true, false, false, 0).message;
    QVERIFY(exitMainMessage.contains(QStringLiteral("Core exited (code=7).")));
    QVERIFY(!exitMainMessage.contains(QStringLiteral("crash")));

    ProxyCrashRestartPolicy crashAuxiliary;
    QVERIFY(crashAuxiliary.decide(7, QProcess::CrashExit, true, true, false, false, 0)
                .message.contains(QStringLiteral("Auxiliary core crash detected (code=7).")));

    ProxyCrashRestartPolicy exitAuxiliary;
    QVERIFY(exitAuxiliary.decide(7, QProcess::NormalExit, true, true, false, false, 0)
                .message.contains(QStringLiteral("Auxiliary core exited (code=7).")));

    // The give-up frame names how many consecutive failures it took to exhaust the budget: five
    // restarts were allowed, so this sixth failure is the one that disables them.
    ProxyCrashRestartPolicy givingUp;
    for (int i = 0; i < 5; ++i) {
        givingUp.decide(7, QProcess::CrashExit, false, true, false, false, 0);
    }
    const QString disabledMessage =
        givingUp.decide(7, QProcess::CrashExit, false, true, false, false, 0).message;
    QVERIFY(disabledMessage.contains(QStringLiteral("Core crash detected (code=7).")));
    QVERIFY(disabledMessage.contains(QStringLiteral("Auto-restart disabled after 6 consecutive failures.")));
    QVERIFY(!disabledMessage.contains(QStringLiteral("Restarting in")));

    // The TUN conflict messages are standalone sentences and name the exit code.
    ProxyCrashRestartPolicy tunConflict;
    QVERIFY(tunConflict.decide(7, QProcess::CrashExit, false, true, true, true, 0)
                .message.contains(QStringLiteral("TUN adapter conflict detected (code=7).")));
    QVERIFY(tunConflict.decide(7, QProcess::CrashExit, false, true, true, true, 0)
                .message.contains(QStringLiteral(
                    "TUN adapter conflict persisted after cleanup retry. Auto-restart disabled.")));
}

void ProxyCrashRestartPolicyTests::routesTheRestartMessagesThroughTheTranslationMachinery()
{
    TaggingTranslator translator;
    QCoreApplication::installTranslator(&translator);

    ProxyCrashRestartPolicy restarting;
    const QString restartingMessage =
        restarting.decide(7, QProcess::CrashExit, false, true, false, false, 0).message;

    ProxyCrashRestartPolicy givingUp;
    for (int i = 0; i < 5; ++i) {
        givingUp.decide(7, QProcess::CrashExit, false, true, false, false, 0);
    }
    const QString disabledMessage =
        givingUp.decide(7, QProcess::CrashExit, false, true, false, false, 0).message;

    ProxyCrashRestartPolicy tunConflict;
    const QString tunRetryMessage =
        tunConflict.decide(7, QProcess::CrashExit, false, true, true, true, 0).message;
    const QString tunDisabledMessage =
        tunConflict.decide(7, QProcess::CrashExit, false, true, true, true, 0).message;

    QCoreApplication::removeTranslator(&translator);

    // The restart frames carry two markers: the embedded "Core crash detected" sentence is
    // translated on its own, then the frame around it is translated again. A QStringLiteral
    // regression drops both, which is the whole point of this test.
    QCOMPARE(restartingMessage.count(QStringLiteral("[zh]")), 2);
    QCOMPARE(disabledMessage.count(QStringLiteral("[zh]")), 2);
    QVERIFY(restartingMessage.contains(QStringLiteral("Core crash detected (code=7).")));
    QVERIFY(restartingMessage.contains(QStringLiteral("Restarting in 3s... (attempt 1/5)")));

    // The TUN messages are single sentences, so one marker each.
    QCOMPARE(tunRetryMessage.count(QStringLiteral("[zh]")), 1);
    QCOMPARE(tunDisabledMessage.count(QStringLiteral("[zh]")), 1);
}

QTEST_MAIN(ProxyCrashRestartPolicyTests)
#include "ProxyCrashRestartPolicyTests.moc"
