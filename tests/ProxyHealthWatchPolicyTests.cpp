#include <QtTest>

#include <QTranslator>

#include "appcore/ProxyHealthWatchPolicy.h"

namespace {

// Knows only the ProxySession context and tags whatever it is asked for, so a test can prove a
// message actually travelled through the translation machinery. The regression this guards
// against is a return to QStringLiteral(), which compiles and reads perfectly well but is never
// translated -- the same failure ProxyCrashRestartPolicyTests pins for the restart messages.
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

// Characterization tests for the node-health-watch decision table.
//
// Why they exist: the main app had no way to notice that the running core was alive while its
// active node had gone dark, so a node could be dead for hours with no user-visible signal. The
// policy that fixes that is deliberately split out of ProxySession -- it owns the decision, the
// session owns the timer and the probe -- which only pays off if the decision itself is pinned.
//
// The decision table, as read off the implementation:
//
//   reachable                -> consecutiveFailures = 0
//                               warningShown was true  -> Clear
//                               warningShown was false -> None
//   !reachable               -> ++consecutiveFailures
//                               warningShown already true            -> None (stay silent)
//                               consecutiveFailures < threshold      -> None (tolerate the blip)
//                               otherwise                            -> Warn, warningShown = true
//
// The threshold defaults to 2, so the first failure is always tolerated. A warning is shown at
// most once per outage: further failures keep counting but return None, and the warning is
// re-armed only by a success.
class ProxyHealthWatchPolicyTests : public QObject {
    Q_OBJECT

private slots:
    // --- the tolerate-then-warn path ---
    void staysSilentOnTheFirstFailure();
    void warnsOnceTheConfiguredNumberOfFailuresIsReached();
    void doesNotRepeatTheWarningWhileItIsStillShowing();
    void warnsOnTheFirstFailureWhenConfiguredTo();
    void treatsANonPositiveThresholdAsOne();

    // --- the clear path ---
    void clearsTheWarningOnTheFirstSuccessAfterAWarning();
    void staysSilentOnSuccessWhenNoWarningWasShown();
    void restartsTheCountOnSuccessSoOneFailureDoesNotWarnAgain();
    void resetClearsTheCountAndTheWarningFlag();

    // --- message content ---
    void appendsTheProbeReasonToTheWarning();
    void omitsTheSeparatorWhenThereIsNoProbeReason();
    void routesTheWarningAndClearMessagesThroughTheTranslationMachinery();
};

void ProxyHealthWatchPolicyTests::staysSilentOnTheFirstFailure()
{
    ProxyHealthWatchPolicy policy;

    const ProxyHealthWatchPolicy::Decision decision =
        policy.recordResult(false, QStringLiteral("Availability check: -1 ms"));

    QCOMPARE(decision.action, ProxyHealthWatchPolicy::Action::None);
    QVERIFY(decision.message.isEmpty());
    QCOMPARE(policy.consecutiveFailures(), 1);
    QVERIFY(!policy.warningShown());
}

void ProxyHealthWatchPolicyTests::warnsOnceTheConfiguredNumberOfFailuresIsReached()
{
    ProxyHealthWatchPolicy policy;

    QCOMPARE(
        policy.recordResult(false, QStringLiteral("Availability check: -1 ms")).action,
        ProxyHealthWatchPolicy::Action::None);

    const ProxyHealthWatchPolicy::Decision second =
        policy.recordResult(false, QStringLiteral("Availability check: -1 ms"));

    QCOMPARE(second.action, ProxyHealthWatchPolicy::Action::Warn);
    QVERIFY(second.message.contains(
        QStringLiteral("Node unavailable: 2 consecutive health checks failed.")));
    QCOMPARE(policy.consecutiveFailures(), 2);
    QVERIFY(policy.warningShown());
}

void ProxyHealthWatchPolicyTests::doesNotRepeatTheWarningWhileItIsStillShowing()
{
    ProxyHealthWatchPolicy policy;

    policy.recordResult(false, QString());
    QCOMPARE(policy.recordResult(false, QString()).action, ProxyHealthWatchPolicy::Action::Warn);

    // The outage continues. The count keeps rising -- it is what the message reports -- but the
    // caller must not be told to show the warning again.
    QCOMPARE(policy.recordResult(false, QString()).action, ProxyHealthWatchPolicy::Action::None);
    QCOMPARE(policy.recordResult(false, QString()).action, ProxyHealthWatchPolicy::Action::None);
    QCOMPARE(policy.consecutiveFailures(), 4);
    QVERIFY(policy.warningShown());
}

void ProxyHealthWatchPolicyTests::warnsOnTheFirstFailureWhenConfiguredTo()
{
    ProxyHealthWatchPolicy policy(1);

    const ProxyHealthWatchPolicy::Decision decision = policy.recordResult(false, QString());

    QCOMPARE(decision.action, ProxyHealthWatchPolicy::Action::Warn);
    QVERIFY(decision.message.contains(
        QStringLiteral("Node unavailable: 1 consecutive health checks failed.")));
}

void ProxyHealthWatchPolicyTests::treatsANonPositiveThresholdAsOne()
{
    // Zero and negative are meaningless as a failure count, so they are clamped to the lowest
    // value that still means something rather than being silently accepted.
    ProxyHealthWatchPolicy zero(0);
    QCOMPARE(zero.failuresBeforeWarn(), 1);
    QCOMPARE(zero.recordResult(false, QString()).action, ProxyHealthWatchPolicy::Action::Warn);

    ProxyHealthWatchPolicy negative(-3);
    QCOMPARE(negative.failuresBeforeWarn(), 1);
    QCOMPARE(negative.recordResult(false, QString()).action, ProxyHealthWatchPolicy::Action::Warn);
}

void ProxyHealthWatchPolicyTests::clearsTheWarningOnTheFirstSuccessAfterAWarning()
{
    ProxyHealthWatchPolicy policy;

    policy.recordResult(false, QString());
    QCOMPARE(policy.recordResult(false, QString()).action, ProxyHealthWatchPolicy::Action::Warn);

    const ProxyHealthWatchPolicy::Decision recovered = policy.recordResult(true, QString());

    QCOMPARE(recovered.action, ProxyHealthWatchPolicy::Action::Clear);
    QVERIFY(!recovered.message.isEmpty());
    QCOMPARE(policy.consecutiveFailures(), 0);
    QVERIFY(!policy.warningShown());

    // A second success has nothing left to clear.
    QCOMPARE(policy.recordResult(true, QString()).action, ProxyHealthWatchPolicy::Action::None);
}

void ProxyHealthWatchPolicyTests::staysSilentOnSuccessWhenNoWarningWasShown()
{
    ProxyHealthWatchPolicy policy;

    const ProxyHealthWatchPolicy::Decision decision = policy.recordResult(true, QString());

    QCOMPARE(decision.action, ProxyHealthWatchPolicy::Action::None);
    QVERIFY(decision.message.isEmpty());
    QCOMPARE(policy.consecutiveFailures(), 0);
}

void ProxyHealthWatchPolicyTests::restartsTheCountOnSuccessSoOneFailureDoesNotWarnAgain()
{
    ProxyHealthWatchPolicy policy;

    // One failure, then recovery: the count must be back at zero, otherwise the next single
    // failure would warn on the strength of a failure that has already been forgiven.
    policy.recordResult(false, QString());
    QCOMPARE(policy.recordResult(true, QString()).action, ProxyHealthWatchPolicy::Action::None);
    QCOMPARE(policy.consecutiveFailures(), 0);

    QCOMPARE(policy.recordResult(false, QString()).action, ProxyHealthWatchPolicy::Action::None);
    QCOMPARE(policy.consecutiveFailures(), 1);
}

void ProxyHealthWatchPolicyTests::resetClearsTheCountAndTheWarningFlag()
{
    ProxyHealthWatchPolicy policy;

    policy.recordResult(false, QString());
    policy.recordResult(false, QString());
    QVERIFY(policy.warningShown());

    policy.reset();

    QCOMPARE(policy.consecutiveFailures(), 0);
    QVERIFY(!policy.warningShown());

    // The warning was shown and then dropped without a success, so there is nothing to clear --
    // the next failure has to earn the warning again from zero.
    QCOMPARE(policy.recordResult(false, QString()).action, ProxyHealthWatchPolicy::Action::None);
}

void ProxyHealthWatchPolicyTests::appendsTheProbeReasonToTheWarning()
{
    ProxyHealthWatchPolicy policy;

    policy.recordResult(false, QString());
    const ProxyHealthWatchPolicy::Decision decision =
        policy.recordResult(false, QStringLiteral("Availability check: -1 ms"));

    QCOMPARE(decision.action, ProxyHealthWatchPolicy::Action::Warn);
    QVERIFY(decision.message.endsWith(QStringLiteral("Availability check: -1 ms")));
    QVERIFY(decision.message.contains(
        QStringLiteral("Node unavailable: 2 consecutive health checks failed.")));
}

void ProxyHealthWatchPolicyTests::omitsTheSeparatorWhenThereIsNoProbeReason()
{
    ProxyHealthWatchPolicy policy;

    policy.recordResult(false, QString());
    const ProxyHealthWatchPolicy::Decision decision = policy.recordResult(false, QString());

    QCOMPARE(decision.action, ProxyHealthWatchPolicy::Action::Warn);
    QVERIFY(decision.message.endsWith(QStringLiteral("failed.")));
    QCOMPARE(decision.message, decision.message.trimmed());
}

void ProxyHealthWatchPolicyTests::routesTheWarningAndClearMessagesThroughTheTranslationMachinery()
{
    TaggingTranslator translator;
    QCoreApplication::installTranslator(&translator);

    ProxyHealthWatchPolicy policy;
    policy.recordResult(false, QStringLiteral("Availability check: -1 ms"));
    const QString warning =
        policy.recordResult(false, QStringLiteral("Availability check: -1 ms")).message;
    const QString cleared = policy.recordResult(true, QString()).message;

    QCoreApplication::removeTranslator(&translator);

    // One marker each: the sentences are translated, and the appended probe reason is passed in
    // by the caller (in production it is already localized by the availability-check service).
    QCOMPARE(warning.count(QStringLiteral("[zh]")), 1);
    QCOMPARE(cleared.count(QStringLiteral("[zh]")), 1);
    QVERIFY(warning.contains(QStringLiteral("Node unavailable: 2 consecutive health checks failed.")));
    QVERIFY(warning.endsWith(QStringLiteral("Availability check: -1 ms")));
}

QTEST_MAIN(ProxyHealthWatchPolicyTests)
#include "ProxyHealthWatchPolicyTests.moc"
