#include <QtTest>

#include "auto/AutoSelectionEngine.h"

namespace {

qint64 now()
{
    // Wall-clock is fed to RoutingScorePolicy in every test so the breaker cooldown and the hold
    // window are driven without sleeping. AutoSelectionEngine's own methods read the clock
    // internally, so every time-driven case here is written against the production defaults the
    // engine runs with rather than a frozen `now`.
    return QDateTime::currentMSecsSinceEpoch();
}

AutoNodeEvaluation probe(
    const QString& id,
    const QString& countryCode,
    qint64 latencyMs,
    bool available,
    bool tested = true,
    bool inconclusive = false)
{
    AutoNodeEvaluation result;
    result.indexId = id;
    result.countryCode = countryCode;
    result.countryName = countryCode;
    result.countryDisplay = countryCode;
    result.latencyMs = latencyMs;
    result.available = available;
    result.tested = tested;
    result.inconclusive = inconclusive;
    return result;
}

} // namespace

// The engine that owns SongBirdAutoCoordinator's per-node scoring state. RoutingScorePolicy
// (frozen, pinned by RoutingScorePolicyTests) supplies the arithmetic -- EWMA, breaker, margin,
// hold window; what this suite pins is the state around it:
//
//   * recordProbeResults()   -- a completed probe is evidence and feeds the policy; an
//                               inconclusive or untested one measured nothing and is skipped.
//   * recordHealthCheckFailure() -- a failed check is evidence even before any probe ran, but a
//                               node with no state at all is not fabricated into one.
//   * bestServerIdForCountry() -- ranks through the one selection loop
//                               (bestAutoServerIdForCountryByScore) by effective score, so a
//                               tripped node loses to a slower-but-alive one.
//   * shouldSwitch()         -- the wrapper both coordinator call sites go through; it must keep
//                               honouring the margin, the hold window and the breaker, and refuse
//                               to compare a side it has no state for.
class AutoSelectionEngineTests : public QObject
{
    Q_OBJECT

private slots:
    // --- probe feeding ---
    void recordProbeResultsSkipsInconclusiveProbes();
    void recordProbeResultsSkipsUntestedProbes();
    void recordProbeResultsFeedsFailuresToTheBreaker();

    // --- health-check evidence ---
    void recordHealthCheckFailureTripsTheBreakerAfterTheConfiguredFailures();
    void recordHealthCheckFailureIgnoresAnUnknownNode();

    // --- ranking ---
    void bestServerIdForCountryRanksBySmoothedScore();
    void bestServerIdForCountryExcludesTheFailedServer();
    void bestServerIdForCountryReturnsEmptyForNoCandidate();

    // --- breaker + ranking together ---
    void trippedNodeIsNotSelectedWhileASlowerNodeIsAvailable();

    // --- reset ---
    void resetDiscardsNodeHistory();

    // --- the switch gate wrapper ---
    void shouldSwitchRefusesAnUnmeasuredSide();
    void shouldSwitchRefusesASlightlyBetterChallenger();
    void shouldSwitchMovesForAGenuinelyBetterChallengerOutsideTheHoldWindow();
    void shouldSwitchHoldsAFreshlyActivatedNode();
    void shouldSwitchReplacesATrippedCurrentNodeEvenInsideTheHoldWindow();

    // --- measured lookup ---
    void hasMeasuredNodeDistinguishesSeedAndProbeFromAbsence();
};

void AutoSelectionEngineTests::recordProbeResultsSkipsInconclusiveProbes()
{
    AutoSelectionEngine engine;
    // An inconclusive probe measured nothing: no core ever ran (a temporary port could not be
    // reserved). Counting it as a failure is what used to push healthy nodes toward the breaker.
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), -1, false, true, true)});

    QVERIFY(!engine.hasMeasuredNode(QStringLiteral("node-a")));
    // And it must not even count toward the breaker: three consecutive inconclusive probes still
    // leave a node that has never been measured eligible for a switch comparison after one real
    // probe, rather than pre-tripped.
    AutoSelectionEngine tripped;
    for (int i = 0; i < 5; ++i) {
        tripped.recordProbeResults(
            {probe(QStringLiteral("node-a"), QStringLiteral("US"), -1, false, true, true)});
    }
    QVERIFY(!tripped.hasMeasuredNode(QStringLiteral("node-a")));
}

void AutoSelectionEngineTests::recordProbeResultsSkipsUntestedProbes()
{
    AutoSelectionEngine engine;
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 120, true, false)});
    QVERIFY(!engine.hasMeasuredNode(QStringLiteral("node-a")));
}

void AutoSelectionEngineTests::recordProbeResultsFeedsFailuresToTheBreaker()
{
    AutoSelectionEngine engine;
    const QList<AutoNodeEvaluation> evaluations{
        probe(QStringLiteral("node-a"), QStringLiteral("US"), 30, true),
        probe(QStringLiteral("node-b"), QStringLiteral("US"), 800, true),
    };
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 30, true)});
    engine.recordProbeResults({probe(QStringLiteral("node-b"), QStringLiteral("US"), 800, true)});
    QCOMPARE(
        engine.bestServerIdForCountry(evaluations, QStringLiteral("US")),
        QStringLiteral("node-a"));

    // Three failed probes (tested, not inconclusive) are real evidence: they must advance the
    // breaker and demote the fast node behind the slower-but-alive one. The evaluation entries
    // still carry the old good reading; the engine's state is what moves.
    for (int i = 0; i < engine.policy().config().failuresToTrip; ++i) {
        engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), -1, false)});
    }
    QCOMPARE(
        engine.bestServerIdForCountry(evaluations, QStringLiteral("US")),
        QStringLiteral("node-b"));
}

void AutoSelectionEngineTests::recordHealthCheckFailureTripsTheBreakerAfterTheConfiguredFailures()
{
    AutoSelectionEngine engine;
    // A node the probes already measured; three failed health checks then trip its breaker.
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 100, true)});
    for (int i = 0; i < engine.policy().config().failuresToTrip; ++i) {
        engine.recordHealthCheckFailure(QStringLiteral("node-a"));
    }
    // The consequence is observable through the gate: a tripped current node is replaceable even
    // inside its hold window, by a challenger that is alive but slower.
    engine.recordProbeResults({probe(QStringLiteral("node-b"), QStringLiteral("US"), 400, true)});
    QVERIFY(engine.shouldSwitch(
        QStringLiteral("node-a"), QStringLiteral("node-b"), now() - 1000));
}

void AutoSelectionEngineTests::recordHealthCheckFailureIgnoresAnUnknownNode()
{
    AutoSelectionEngine engine;
    engine.recordHealthCheckFailure(QStringLiteral("never-probed"));
    // Fabricating state for a node nobody measured would turn the gate's missing-side refusal
    // into a comparison the evidence does not support.
    QVERIFY(!engine.hasMeasuredNode(QStringLiteral("never-probed")));
    QVERIFY(!engine.shouldSwitch(
        QStringLiteral("never-probed"), QStringLiteral("also-unknown"), 0));
}

void AutoSelectionEngineTests::bestServerIdForCountryRanksBySmoothedScore()
{
    AutoSelectionEngine engine;
    // One 900 ms outlier behind a 100 ms node moves the smoothed value to 380 (alpha 0.35), which
    // is still ahead of a steady 500 ms node -- ranking by the raw last reading (900) would have
    // put it behind.
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 100, true)});
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 900, true)});
    engine.recordProbeResults({probe(QStringLiteral("node-b"), QStringLiteral("US"), 500, true)});
    engine.recordProbeResults({probe(QStringLiteral("node-b"), QStringLiteral("US"), 500, true)});

    const QList<AutoNodeEvaluation> evaluations{
        probe(QStringLiteral("node-a"), QStringLiteral("US"), 900, true),
        probe(QStringLiteral("node-b"), QStringLiteral("US"), 500, true),
    };
    QCOMPARE(
        engine.bestServerIdForCountry(evaluations, QStringLiteral("US")),
        QStringLiteral("node-a"));
}

void AutoSelectionEngineTests::bestServerIdForCountryExcludesTheFailedServer()
{
    AutoSelectionEngine engine;
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 100, true)});
    engine.recordProbeResults({probe(QStringLiteral("node-b"), QStringLiteral("US"), 300, true)});
    const QList<AutoNodeEvaluation> evaluations{
        probe(QStringLiteral("node-a"), QStringLiteral("US"), 100, true),
        probe(QStringLiteral("node-b"), QStringLiteral("US"), 300, true),
    };
    // The failover path ranks with the failed node excluded so it is not re-picked as its own
    // replacement.
    QCOMPARE(
        engine.bestServerIdForCountry(
            evaluations, QStringLiteral("US"), QStringLiteral("node-a")),
        QStringLiteral("node-b"));
}

void AutoSelectionEngineTests::bestServerIdForCountryReturnsEmptyForNoCandidate()
{
    AutoSelectionEngine engine;
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 100, true)});
    const QList<AutoNodeEvaluation> evaluations{
        probe(QStringLiteral("node-a"), QStringLiteral("US"), 100, true),
        // Untested and unavailable entries are not candidates.
        probe(QStringLiteral("node-b"), QStringLiteral("US"), 50, true, false),
        probe(QStringLiteral("node-c"), QStringLiteral("JP"), 50, true),
    };
    QVERIFY(engine.bestServerIdForCountry(evaluations, QStringLiteral("JP")).isEmpty());
    QVERIFY(engine.bestServerIdForCountry(evaluations, QStringLiteral("US"), QStringLiteral("node-a")).isEmpty());
}

void AutoSelectionEngineTests::trippedNodeIsNotSelectedWhileASlowerNodeIsAvailable()
{
    AutoSelectionEngine engine;
    engine.recordProbeResults({probe(QStringLiteral("node-fast"), QStringLiteral("US"), 30, true)});
    engine.recordProbeResults({probe(QStringLiteral("node-slow"), QStringLiteral("US"), 800, true)});
    // The failover shape: the probes last saw node-fast healthy (the evaluation still says
    // available with its good latency), and only the health checks have failed since. The breaker
    // is what keeps the stale-good-reading node from winning, not the evaluation's flag.
    for (int i = 0; i < engine.policy().config().failuresToTrip; ++i) {
        engine.recordHealthCheckFailure(QStringLiteral("node-fast"));
    }
    const QList<AutoNodeEvaluation> evaluations{
        probe(QStringLiteral("node-fast"), QStringLiteral("US"), 30, true),
        probe(QStringLiteral("node-slow"), QStringLiteral("US"), 800, true),
    };
    QCOMPARE(
        engine.bestServerIdForCountry(evaluations, QStringLiteral("US")),
        QStringLiteral("node-slow"));
}

void AutoSelectionEngineTests::resetDiscardsNodeHistory()
{
    AutoSelectionEngine engine;
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 100, true)});
    engine.reset();
    QVERIFY(!engine.hasMeasuredNode(QStringLiteral("node-a")));
    // A recycled id must not inherit another node's history: the gate again refuses to compare
    // the id against anything.
    engine.recordProbeResults({probe(QStringLiteral("node-b"), QStringLiteral("US"), 100, true)});
    QVERIFY(!engine.shouldSwitch(QStringLiteral("node-a"), QStringLiteral("node-b"), 0));
}

void AutoSelectionEngineTests::shouldSwitchRefusesAnUnmeasuredSide()
{
    AutoSelectionEngine engine;
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 100, true)});
    // node-b has no state: an unmeasured challenger is not evidence, so the move is refused
    // exactly as a missing map entry was before the extraction.
    QVERIFY(!engine.shouldSwitch(QStringLiteral("node-a"), QStringLiteral("node-b"), 0));
    QVERIFY(!engine.shouldSwitch(QStringLiteral("node-b"), QStringLiteral("node-a"), 0));
}

void AutoSelectionEngineTests::shouldSwitchRefusesASlightlyBetterChallenger()
{
    AutoSelectionEngine engine;
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 100, true)});
    // 90 is better, but only by 10 %, under the 25 % switch margin.
    engine.recordProbeResults({probe(QStringLiteral("node-b"), QStringLiteral("US"), 90, true)});
    QVERIFY(!engine.shouldSwitch(QStringLiteral("node-a"), QStringLiteral("node-b"), 0));
}

void AutoSelectionEngineTests::shouldSwitchMovesForAGenuinelyBetterChallengerOutsideTheHoldWindow()
{
    AutoSelectionEngine engine;
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 100, true)});
    // Required is 100 * 0.75 = 75, so 70 qualifies. The active node's activation time is unknown
    // (0), which is how the hold window is skipped.
    engine.recordProbeResults({probe(QStringLiteral("node-b"), QStringLiteral("US"), 70, true)});
    QVERIFY(engine.shouldSwitch(QStringLiteral("node-a"), QStringLiteral("node-b"), 0));
}

void AutoSelectionEngineTests::shouldSwitchHoldsAFreshlyActivatedNode()
{
    AutoSelectionEngine engine;
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 400, true)});
    engine.recordProbeResults({probe(QStringLiteral("node-b"), QStringLiteral("US"), 100, true)});
    // The challenger is well past the margin; the hold window is what keeps the freshly started
    // node in place (activation a moment ago). A rotation subscription would otherwise restart
    // the core on every probe round.
    QVERIFY(!engine.shouldSwitch(
        QStringLiteral("node-a"),
        QStringLiteral("node-b"),
        now() - 1000));
}

void AutoSelectionEngineTests::shouldSwitchReplacesATrippedCurrentNodeEvenInsideTheHoldWindow()
{
    AutoSelectionEngine engine;
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 50, true)});
    for (int i = 0; i < engine.policy().config().failuresToTrip; ++i) {
        engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), -1, false)});
    }
    // Deliberately worse than the current node's last known latency: a tripped node is failing,
    // so anything that still works is the improvement, and the hold window does not protect it.
    engine.recordProbeResults({probe(QStringLiteral("node-b"), QStringLiteral("US"), 900, true)});
    QVERIFY(engine.shouldSwitch(
        QStringLiteral("node-a"),
        QStringLiteral("node-b"),
        now() - 1000));
}

void AutoSelectionEngineTests::hasMeasuredNodeDistinguishesSeedAndProbeFromAbsence()
{
    AutoSelectionEngine engine;
    QVERIFY(!engine.hasMeasuredNode(QStringLiteral("node-a")));
    engine.recordProbeResults({probe(QStringLiteral("node-a"), QStringLiteral("US"), 100, true)});
    QVERIFY(engine.hasMeasuredNode(QStringLiteral("node-a")));
    engine.reset();
    QVERIFY(!engine.hasMeasuredNode(QStringLiteral("node-a")));
}

QTEST_GUILESS_MAIN(AutoSelectionEngineTests)
#include "AutoSelectionEngineTests.moc"
