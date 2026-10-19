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
//                  node, but a tripped current node is replaced without a margin.
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
    // 90 is better, but only by 10 %, under the 25 % margin.
    policy.recordResult(&challenger, true, 90, now());
    QVERIFY(!policy.shouldSwitch(current, challenger, now()));
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

void RoutingScorePolicyTests::clampsOutOfRangeConfig()
{
    RoutingScorePolicy::Config config;
    config.alpha = 0.0;
    config.unusableLatencyMs = 0;
    config.failuresToTrip = 0;
    config.breakerCooldownMs = -1;
    config.switchMargin = -0.5;
    config.initialScore = 0.0;
    RoutingScorePolicy policy(config);

    QVERIFY(policy.config().alpha > 0.0);
    QVERIFY(policy.config().unusableLatencyMs > 0);
    QCOMPARE(policy.config().failuresToTrip, 1);
    QCOMPARE(policy.config().breakerCooldownMs, 0);
    QCOMPARE(policy.config().switchMargin, 0.0);
    QVERIFY(policy.config().initialScore > 0.0);
}

QTEST_GUILESS_MAIN(RoutingScorePolicyTests)
#include "RoutingScorePolicyTests.moc"
