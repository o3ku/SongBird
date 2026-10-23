#include <QtTest>

#include "appcore/RoutingScorePolicy.h"

// Characterization tests for the node-scoring decision table.
//
// Three mechanisms are pinned separately, because each can fail on its own:
//
//   * EWMA      -- one outlier must not reorder the list, but a sustained change must be followed.
//   * Breaker   -- N consecutive failures take a node out of consideration for a cooldown, and a
//                  success closes it early.
//   * Hysteresis-- a challenger must be better by a margin, so noise does not churn the active
//                  node, but a tripped current node is replaced without a margin. A freshly chosen
//                  node is also held for a minimum time, which is what a better-but-not-urgent
//                  challenger waits out.
//
// The policy is stateless, so every test owns its own NodeState. Timestamps are passed in rather
// than read from the clock, so the cooldown is driven without sleeping.
class RoutingScorePolicyTests : public QObject {
    Q_OBJECT

private slots:
    // --- seeding ---
    void seedGivesAnUntestedNodeTheInitialScore();
    void seedDoesNotDiscardAnExistingSample();

    // --- EWMA ---
    void firstProbeSetsTheScoreDirectly();
    void oneOutlierMovesTheScorePartially();
    void sustainedChangeConvergesTowardTheNewLatency();
    void alphaOfOneDisablesSmoothing();

    // --- unusable-latency handling ---
    void reachableButUnusableLatencyCountsAsFailure();
    void unreachableIgnoresTheLatencyArgument();

    // --- circuit breaker ---
    void breakerTripsAfterTheConfiguredNumberOfFailures();
    void breakerStaysClosedBelowTheThreshold();
    void trippedBreakerSortsLastRegardlessOfScore();
    void breakerClosesWhenTheCooldownElapses();
    void successClosesTheBreakerEarly();
    void failingNodeKeepsItsSmoothedScore();

    // --- hysteresis ---
    void noSwitchWithoutSamplesOnBothSides();
    void noSwitchWhenChallengerIsOnlySlightlyBetter();
    void switchesWhenChallengerIsBetterByTheMargin();
    void replacesATrippedCurrentNodeWithoutAMargin();
    void noSwitchWhenBothAreTripped();

    // --- minimum hold ---
    void holdsAFreshlyChosenNodeEvenAgainstABetterChallenger();
    void switchesOnceTheHoldWindowElapses();
    void holdDoesNotDelayReplacingATrippedNode();
    void holdIsSkippedWhenTheActivationTimeIsUnknown();
    void zeroHoldDisablesTheWindow();

    // --- failover evidence without a probe ---
    void failuresAloneNeverProduceASample();
    void seededThenFailedCurrentIsReplacedEvenInsideTheHoldWindow();

    // --- config clamping ---
    void clampsOutOfRangeConfig();

private:
    static qint64 now() { return 1000000; }
};

void RoutingScorePolicyTests::seedGivesAnUntestedNodeTheInitialScore()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState state;
    policy.seed(&state);
    QVERIFY(state.hasSample);
    QCOMPARE(state.score, policy.config().initialScore);
}

void RoutingScorePolicyTests::seedDoesNotDiscardAnExistingSample()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, true, 120, now());
    const double before = state.score;
    policy.seed(&state);
    QCOMPARE(state.score, before);
}

void RoutingScorePolicyTests::firstProbeSetsTheScoreDirectly()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, true, 150, now());
    QVERIFY(state.hasSample);
    QCOMPARE(state.score, 150.0);
}

void RoutingScorePolicyTests::oneOutlierMovesTheScorePartially()
{
    RoutingScorePolicy policy;  // alpha 0.35
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, true, 100, now());
    policy.recordResult(&state, true, 900, now());
    // 0.35 * 900 + 0.65 * 100 = 380
    QCOMPARE(state.score, 380.0);
    // The outlier must not have taken the score all the way to 900.
    QVERIFY(state.score < 900.0);
}

void RoutingScorePolicyTests::sustainedChangeConvergesTowardTheNewLatency()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, true, 100, now());
    for (int i = 0; i < 20; ++i) {
        policy.recordResult(&state, true, 500, now());
    }
    // After twenty consistent samples the smoothed value must be close to the new latency.
    QVERIFY2(state.score > 480.0, qPrintable(QString::number(state.score)));
    QVERIFY(state.score < 500.0);
}

void RoutingScorePolicyTests::alphaOfOneDisablesSmoothing()
{
    RoutingScorePolicy::Config config;
    config.alpha = 1.0;
    RoutingScorePolicy policy(config);
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, true, 100, now());
    policy.recordResult(&state, true, 700, now());
    QCOMPARE(state.score, 700.0);
}

void RoutingScorePolicyTests::reachableButUnusableLatencyCountsAsFailure()
{
    RoutingScorePolicy policy;  // unusableLatencyMs 2000
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, true, 100, now());
    policy.recordResult(&state, true, 2500, now());
    QCOMPARE(state.consecutiveFailures, 1);
    // The good score survives; only the failure counter moved.
    QCOMPARE(state.score, 100.0);
}

void RoutingScorePolicyTests::unreachableIgnoresTheLatencyArgument()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, false, 5, now());
    QCOMPARE(state.consecutiveFailures, 1);
    QVERIFY(!state.hasSample);
}

void RoutingScorePolicyTests::breakerTripsAfterTheConfiguredNumberOfFailures()
{
    RoutingScorePolicy policy;  // failuresToTrip 3
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, true, 100, now());

    policy.recordResult(&state, false, 0, now());
    QVERIFY(!policy.isBreakerOpen(state, now()));
    policy.recordResult(&state, false, 0, now());
    QVERIFY(!policy.isBreakerOpen(state, now()));
    policy.recordResult(&state, false, 0, now());
    QVERIFY(policy.isBreakerOpen(state, now()));
}

void RoutingScorePolicyTests::breakerStaysClosedBelowTheThreshold()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, true, 100, now());
    policy.recordResult(&state, false, 0, now());
    policy.recordResult(&state, false, 0, now());
    QVERIFY(!policy.isBreakerOpen(state, now()));
}

void RoutingScorePolicyTests::trippedBreakerSortsLastRegardlessOfScore()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState fast;
    policy.recordResult(&fast, true, 30, now());
    RoutingScorePolicy::NodeState tripped;
    policy.recordResult(&tripped, true, 800, now());
    for (int i = 0; i < 3; ++i) {
        policy.recordResult(&tripped, false, 0, now());
    }

    QVERIFY(policy.isBreakerOpen(tripped, now()));
    QVERIFY(policy.effectiveScore(tripped, now()) > policy.effectiveScore(fast, now()));
}

void RoutingScorePolicyTests::breakerClosesWhenTheCooldownElapses()
{
    RoutingScorePolicy policy;  // cooldown 60000
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, true, 100, now());
    for (int i = 0; i < 3; ++i) {
        policy.recordResult(&state, false, 0, now());
    }
    QVERIFY(policy.isBreakerOpen(state, now()));
    QVERIFY(!policy.isBreakerOpen(state, now() + policy.config().breakerCooldownMs));
}

void RoutingScorePolicyTests::successClosesTheBreakerEarly()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, true, 100, now());
    for (int i = 0; i < 3; ++i) {
        policy.recordResult(&state, false, 0, now());
    }
    QVERIFY(policy.isBreakerOpen(state, now()));

    // Well before the cooldown would expire.
    policy.recordResult(&state, true, 110, now() + 1000);
    QVERIFY(!policy.isBreakerOpen(state, now() + 1000));
    QCOMPARE(state.consecutiveFailures, 0);
}

void RoutingScorePolicyTests::failingNodeKeepsItsSmoothedScore()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState state;
    policy.recordResult(&state, true, 100, now());
    const double before = state.score;
    policy.recordResult(&state, false, 0, now());
    QCOMPARE(state.score, before);
}

void RoutingScorePolicyTests::noSwitchWithoutSamplesOnBothSides()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    QVERIFY(!policy.shouldSwitch(current, challenger, now()));

    policy.recordResult(&current, true, 500, now());
    QVERIFY(!policy.shouldSwitch(current, challenger, now()));
}

void RoutingScorePolicyTests::noSwitchWhenChallengerIsOnlySlightlyBetter()
{
    RoutingScorePolicy policy;  // switchMargin 0.25
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    policy.recordResult(&current, true, 100, now());
    // 90 is better, but only by 10 %, under the 25 % margin. Checked outside the hold window so
    // the margin, not the window, is what refuses.
    policy.recordResult(&challenger, true, 90, now());
    QVERIFY(!policy.shouldSwitch(current, challenger, now() + policy.config().minimumHoldMs, now()));
}

void RoutingScorePolicyTests::switchesWhenChallengerIsBetterByTheMargin()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    policy.recordResult(&current, true, 100, now());
    // Required is 100 * 0.75 = 75, so 70 qualifies.
    policy.recordResult(&challenger, true, 70, now());
    QVERIFY(policy.shouldSwitch(current, challenger, now()));
}

void RoutingScorePolicyTests::replacesATrippedCurrentNodeWithoutAMargin()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    policy.recordResult(&current, true, 50, now());
    for (int i = 0; i < 3; ++i) {
        policy.recordResult(&current, false, 0, now());
    }
    // Deliberately worse than the current node's last known latency: a tripped node is failing, so
    // "anything that still works" is the improvement, not "anything faster".
    policy.recordResult(&challenger, true, 400, now());
    QVERIFY(policy.isBreakerOpen(current, now()));
    QVERIFY(policy.shouldSwitch(current, challenger, now()));
}

void RoutingScorePolicyTests::noSwitchWhenBothAreTripped()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    for (RoutingScorePolicy::NodeState* state : {&current, &challenger}) {
        policy.recordResult(state, true, 100, now());
        for (int i = 0; i < 3; ++i) {
            policy.recordResult(state, false, 0, now());
        }
    }
    QVERIFY(!policy.shouldSwitch(current, challenger, now()));
}

void RoutingScorePolicyTests::holdsAFreshlyChosenNodeEvenAgainstABetterChallenger()
{
    RoutingScorePolicy policy;  // minimumHoldMs 600000
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    policy.recordResult(&current, true, 400, now());
    // Well past the 25 % margin, so the margin alone would move the connection.
    policy.recordResult(&challenger, true, 100, now());
    QVERIFY(policy.shouldSwitch(current, challenger, now()));

    const qint64 activatedAt = now();
    QVERIFY(!policy.shouldSwitch(current, challenger, activatedAt + 1000, activatedAt));
    QVERIFY(!policy.shouldSwitch(
        current, challenger, activatedAt + policy.config().minimumHoldMs - 1, activatedAt));
}

void RoutingScorePolicyTests::switchesOnceTheHoldWindowElapses()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    policy.recordResult(&current, true, 400, now());
    policy.recordResult(&challenger, true, 100, now());

    const qint64 activatedAt = now();
    QVERIFY(policy.shouldSwitch(
        current, challenger, activatedAt + policy.config().minimumHoldMs, activatedAt));
}

void RoutingScorePolicyTests::holdDoesNotDelayReplacingATrippedNode()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    policy.recordResult(&current, true, 50, now());
    for (int i = 0; i < 3; ++i) {
        policy.recordResult(&current, false, 0, now());
    }
    policy.recordResult(&challenger, true, 900, now());

    // The node is failing, so the hold window does not protect it: waiting is what a working node
    // gets, not one that has stopped answering.
    const qint64 activatedAt = now();
    QVERIFY(policy.isBreakerOpen(current, activatedAt + 1));
    QVERIFY(policy.shouldSwitch(current, challenger, activatedAt + 1, activatedAt));
}

void RoutingScorePolicyTests::holdIsSkippedWhenTheActivationTimeIsUnknown()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    policy.recordResult(&current, true, 400, now());
    policy.recordResult(&challenger, true, 100, now());

    // 0 is the caller saying "I do not know when this node started", which is not evidence for a
    // hold -- imposing one would freeze the connection for ten minutes on the first round.
    QVERIFY(policy.shouldSwitch(current, challenger, now(), 0));
}

void RoutingScorePolicyTests::zeroHoldDisablesTheWindow()
{
    RoutingScorePolicy::Config config;
    config.minimumHoldMs = 0;
    RoutingScorePolicy policy(config);
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    policy.recordResult(&current, true, 400, now());
    policy.recordResult(&challenger, true, 100, now());
    QVERIFY(policy.shouldSwitch(current, challenger, now() + 1, now()));
}

// The failover path records health-check failures into the policy for a node that may never have
// been probe-tested. These two cases pin the mechanism that path depends on: failures alone must
// not manufacture a sample (a node must not earn a score from failing), and a seeded node whose
// breaker then trips is replaceable even inside the hold window.
void RoutingScorePolicyTests::failuresAloneNeverProduceASample()
{
    RoutingScorePolicy policy;
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    policy.recordResult(&challenger, true, 100, now());

    for (int i = 0; i < 5; ++i) {
        policy.recordResult(&current, false, 0, now());
    }
    // The breaker may well have tripped, but an unmeasured side is still not comparable, so the
    // gate stays closed. This is why the failover path seeds the node before recording.
    QVERIFY(!current.hasSample);
    QVERIFY(!policy.shouldSwitch(current, challenger, now(), now()));
}

void RoutingScorePolicyTests::seededThenFailedCurrentIsReplacedEvenInsideTheHoldWindow()
{
    RoutingScorePolicy policy;  // failuresToTrip 3, minimumHoldMs 10 min
    RoutingScorePolicy::NodeState current;
    RoutingScorePolicy::NodeState challenger;
    policy.recordResult(&challenger, true, 100, now());

    // The node never answered a probe; the seed stands in for "exists, unmeasured".
    policy.seed(&current);
    for (int i = 0; i < 3; ++i) {
        policy.recordResult(&current, false, 0, now());
    }
    QVERIFY(policy.isBreakerOpen(current, now()));

    // Inside the hold window: a tripped current node loses its claim without waiting out the
    // window, because "broken" is a different reason to move than "not faster".
    const qint64 activeSince = now() - 1000;
    QVERIFY(policy.shouldSwitch(current, challenger, now(), activeSince));
    QVERIFY(!policy.shouldSwitch(challenger, current, now(), activeSince));
}

void RoutingScorePolicyTests::clampsOutOfRangeConfig()
{
    RoutingScorePolicy::Config config;
    config.alpha = 0.0;
    config.unusableLatencyMs = 0;
    config.failuresToTrip = 0;
    config.breakerCooldownMs = -1;
    config.switchMargin = -0.5;
    config.initialScore = 0.0;
    config.minimumHoldMs = -1;
    RoutingScorePolicy policy(config);

    QVERIFY(policy.config().alpha > 0.0);
    QVERIFY(policy.config().unusableLatencyMs > 0);
    QCOMPARE(policy.config().failuresToTrip, 1);
    QCOMPARE(policy.config().breakerCooldownMs, 0);
    QCOMPARE(policy.config().switchMargin, 0.0);
    QVERIFY(policy.config().initialScore > 0.0);
    QCOMPARE(policy.config().minimumHoldMs, 0);
}

QTEST_GUILESS_MAIN(RoutingScorePolicyTests)
#include "RoutingScorePolicyTests.moc"
