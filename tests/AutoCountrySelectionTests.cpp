#include <QtTest/QtTest>

#include "auto/AutoCountrySelection.h"
#include "auto/AutoCountrySupport.h"

namespace {

AutoNodeEvaluation evaluation(
    const QString& id,
    const QString& countryCode,
    qint64 latencyMs,
    bool available = true,
    const QString& countryName = {})
{
    AutoNodeEvaluation result;
    result.indexId = id;
    result.countryCode = countryCode;
    result.countryName = countryName;
    result.countryDisplay = countryCode;
    result.latencyMs = latencyMs;
    result.available = available;
    return result;
}

AutoNodeEvaluation testedEvaluation(
    const QString& id,
    const QString& countryCode,
    qint64 latencyMs,
    bool available = true,
    const QString& countryName = {})
{
    AutoNodeEvaluation result = evaluation(id, countryCode, latencyMs, available, countryName);
    result.tested = true;
    return result;
}

AutoCountrySummary summaryFor(const QList<AutoCountrySummary>& summaries, const QString& countryCode)
{
    for (const AutoCountrySummary& summary : summaries) {
        if (summary.countryCode == countryCode) {
            return summary;
        }
    }
    return {};
}

} // namespace

class AutoCountrySelectionTests : public QObject
{
    Q_OBJECT

private slots:
    void summariesExposeDefaultCountriesForEmptyResults();
    void summariesUseFixedCountriesAndCounts();
    void summariesGroupUnsupportedCountryNameOnlyResultsAsUnknown();
    void summariesKeepUnknownLast();
    void summariesKeepVerifiedResultInInferredGroup();
    void bestServerSkipsExcludedServerAndNormalizesCountry();
    void bestServerRequiresTestedEvaluation();
    void bestServerSupportsUnknownResults();
    void availableCountNormalizesCountry();
    void availableCountExcludesUntestedInferredNodes();
    // Ranking by a precomputed score rather than raw latency -- the entry point the auto
    // coordinator uses once RoutingScorePolicy has smoothed the samples.
    void scoreRankingPicksTheLowestScore();
    void scoreRankingSkipsNodesWithNoScore();
    void scoreRankingHonoursTheExcludedServer();
    void scoreRankingRequiresAvailableAndTested();
    void scoreRankingKeepsListOrderOnTies();
};

void AutoCountrySelectionTests::summariesExposeDefaultCountriesForEmptyResults()
{
    const QList<AutoCountrySummary> summaries = buildAutoCountrySummaries({});

    QCOMPARE(summaries.size(), autoFixedCountryCodes().size());
    for (int i = 0; i < summaries.size(); ++i) {
        QCOMPARE(summaries.at(i).countryCode, autoFixedCountryCodes().at(i));
        QCOMPARE(summaries.at(i).displayName, autoCountryDisplayName(autoFixedCountryCodes().at(i)));
        QCOMPARE(summaries.at(i).availableCount, 0);
        QCOMPARE(summaries.at(i).bestLatencyMs, -1);
        QVERIFY(summaries.at(i).bestServerId.isEmpty());
    }
}

void AutoCountrySelectionTests::summariesUseFixedCountriesAndCounts()
{
    const QList<AutoNodeEvaluation> evaluations{
        testedEvaluation(QStringLiteral("jp-slow"), QStringLiteral("jp"), 180),
        testedEvaluation(QStringLiteral("us-fast"), QStringLiteral("US"), 60),
        testedEvaluation(QStringLiteral("jp-fast"), QStringLiteral("JP"), 90),
        testedEvaluation(QStringLiteral("hk-down"), QStringLiteral("HK"), 20, false),
        testedEvaluation(QStringLiteral("sg"), QStringLiteral("SG"), 40),
    };

    const QList<AutoCountrySummary> summaries = buildAutoCountrySummaries(evaluations);

    QCOMPARE(summaries.size(), autoFixedCountryCodes().size());
    QCOMPARE(summaries.at(0).countryCode, QStringLiteral("US"));
    QCOMPARE(summaries.at(1).countryCode, QStringLiteral("HK"));
    QCOMPARE(summaryFor(summaries, QStringLiteral("JP")).availableCount, 2);
    QCOMPARE(summaryFor(summaries, QStringLiteral("JP")).bestServerId, QStringLiteral("jp-fast"));
    QCOMPARE(summaryFor(summaries, QStringLiteral("JP")).bestLatencyMs, 90);
    QCOMPARE(summaryFor(summaries, QStringLiteral("HK")).availableCount, 0);
    QCOMPARE(summaryFor(summaries, QStringLiteral("SG")).availableCount, 1);
    QCOMPARE(summaryFor(summaries, QStringLiteral("US")).availableCount, 1);
}

void AutoCountrySelectionTests::summariesGroupUnsupportedCountryNameOnlyResultsAsUnknown()
{
    const QList<AutoNodeEvaluation> evaluations{
        testedEvaluation(QStringLiteral("unknown-slow"), QString(), 140, true, QStringLiteral("Atlantis")),
        testedEvaluation(QStringLiteral("unknown-fast"), QString(), 70, true, QStringLiteral("atlantis")),
    };

    const QList<AutoCountrySummary> summaries = buildAutoCountrySummaries(evaluations);

    const AutoCountrySummary unknown = summaryFor(summaries, autoUnknownCountryCode());
    QCOMPARE(unknown.displayName, QStringLiteral("Unknown"));
    QCOMPARE(unknown.availableCount, 2);
    QCOMPARE(unknown.bestServerId, QStringLiteral("unknown-fast"));
    QCOMPARE(unknown.bestLatencyMs, 70);
}

void AutoCountrySelectionTests::summariesKeepUnknownLast()
{
    const QList<AutoNodeEvaluation> evaluations{
        evaluation(QStringLiteral("unknown-1"), QStringLiteral("UNKNOWN"), -1),
        evaluation(QStringLiteral("unknown-2"), QStringLiteral("UNKNOWN"), -1),
        evaluation(QStringLiteral("hk-1"), QStringLiteral("HK"), -1),
    };

    const QList<AutoCountrySummary> summaries = buildAutoCountrySummaries(evaluations);

    QCOMPARE(summaries.constLast().countryCode, QStringLiteral("UNKNOWN"));
}

void AutoCountrySelectionTests::summariesKeepVerifiedResultInInferredGroup()
{
    AutoNodeEvaluation verified = testedEvaluation(QStringLiteral("node-1"), QStringLiteral("US"), 80);
    verified.inferredCountryCode = QStringLiteral("HK");
    verified.countryDisplay = QStringLiteral("US United States");

    const QList<AutoCountrySummary> summaries = buildAutoCountrySummaries({verified});

    QCOMPARE(summaryFor(summaries, QStringLiteral("HK")).availableCount, 1);
    QCOMPARE(summaryFor(summaries, QStringLiteral("HK")).bestServerId, QStringLiteral("node-1"));
}

void AutoCountrySelectionTests::bestServerSkipsExcludedServerAndNormalizesCountry()
{
    const QList<AutoNodeEvaluation> evaluations{
        testedEvaluation(QStringLiteral("jp-slow"), QStringLiteral("JP"), 180),
        testedEvaluation(QStringLiteral("jp-fast"), QStringLiteral("JP"), 90),
        testedEvaluation(QStringLiteral("us-fast"), QStringLiteral("US"), 60),
    };

    QCOMPARE(
        bestAutoServerIdForCountry(evaluations, QStringLiteral("jp")),
        QStringLiteral("jp-fast"));
    QCOMPARE(
        bestAutoServerIdForCountry(evaluations, QStringLiteral("jp"), QStringLiteral("jp-fast")),
        QStringLiteral("jp-slow"));
}

void AutoCountrySelectionTests::bestServerRequiresTestedEvaluation()
{
    const QList<AutoNodeEvaluation> evaluations{
        evaluation(QStringLiteral("jp-pending"), QStringLiteral("JP"), -1),
        testedEvaluation(QStringLiteral("jp-tested"), QStringLiteral("JP"), 120),
    };

    QCOMPARE(
        bestAutoServerIdForCountry(evaluations, QStringLiteral("JP")),
        QStringLiteral("jp-tested"));
}

void AutoCountrySelectionTests::bestServerSupportsUnknownResults()
{
    const QList<AutoNodeEvaluation> evaluations{
        testedEvaluation(QStringLiteral("unknown-slow"), QString(), 140, true, QStringLiteral("Atlantis")),
        testedEvaluation(QStringLiteral("unknown-fast"), QString(), 70, true, QStringLiteral("atlantis")),
    };

    QCOMPARE(
        bestAutoServerIdForCountry(evaluations, autoUnknownCountryCode()),
        QStringLiteral("unknown-fast"));
    QCOMPARE(
        bestAutoServerIdForCountry(evaluations, autoUnknownCountryCode(), QStringLiteral("unknown-fast")),
        QStringLiteral("unknown-slow"));
}

void AutoCountrySelectionTests::availableCountNormalizesCountry()
{
    const QList<AutoNodeEvaluation> evaluations{
        testedEvaluation(QStringLiteral("jp-1"), QStringLiteral("jp"), 90),
        testedEvaluation(QStringLiteral("jp-2"), QStringLiteral("JP"), 100),
        testedEvaluation(QStringLiteral("jp-down"), QStringLiteral("JP"), 10, false),
    };

    QCOMPARE(countAvailableAutoServersForCountry(evaluations, QStringLiteral("Jp")), 2);
}

void AutoCountrySelectionTests::availableCountExcludesUntestedInferredNodes()
{
    const QList<AutoNodeEvaluation> evaluations{
        testedEvaluation(QStringLiteral("jp-1"), QStringLiteral("JP"), 90),
        // Name-inferred nodes are marked available before they are ever tested;
        // they must not count as usable until a real test result lands.
        evaluation(QStringLiteral("jp-inferred"), QStringLiteral("JP"), -1),
    };

    QCOMPARE(countAvailableAutoServersForCountry(evaluations, QStringLiteral("JP")), 1);
}

void AutoCountrySelectionTests::scoreRankingPicksTheLowestScore()
{
    const QList<AutoNodeEvaluation> evaluations{
        testedEvaluation(QStringLiteral("jp-slow"), QStringLiteral("JP"), 40),
        testedEvaluation(QStringLiteral("jp-fast"), QStringLiteral("JP"), 900),
    };
    // Deliberately inverted against latencyMs: the score is what decides, so a node whose last
    // probe was slow but whose smoothed score is best must still win.
    const QHash<QString, double> scores{
        {QStringLiteral("jp-slow"), 300.0},
        {QStringLiteral("jp-fast"), 120.0},
    };

    QCOMPARE(
        bestAutoServerIdForCountryByScore(evaluations, QStringLiteral("jp"), scores),
        QStringLiteral("jp-fast"));
}

void AutoCountrySelectionTests::scoreRankingSkipsNodesWithNoScore()
{
    const QList<AutoNodeEvaluation> evaluations{
        testedEvaluation(QStringLiteral("jp-untracked"), QStringLiteral("JP"), 10),
        testedEvaluation(QStringLiteral("jp-tracked"), QStringLiteral("JP"), 500),
    };
    const QHash<QString, double> scores{{QStringLiteral("jp-tracked"), 400.0}};

    // A node the caller has no evidence for must not win just because its raw latency looks good.
    QCOMPARE(
        bestAutoServerIdForCountryByScore(evaluations, QStringLiteral("JP"), scores),
        QStringLiteral("jp-tracked"));
}

void AutoCountrySelectionTests::scoreRankingHonoursTheExcludedServer()
{
    const QList<AutoNodeEvaluation> evaluations{
        testedEvaluation(QStringLiteral("jp-best"), QStringLiteral("JP"), 10),
        testedEvaluation(QStringLiteral("jp-second"), QStringLiteral("JP"), 50),
    };
    const QHash<QString, double> scores{
        {QStringLiteral("jp-best"), 10.0},
        {QStringLiteral("jp-second"), 50.0},
    };

    QCOMPARE(
        bestAutoServerIdForCountryByScore(
            evaluations, QStringLiteral("JP"), scores, QStringLiteral("jp-best")),
        QStringLiteral("jp-second"));
}

void AutoCountrySelectionTests::scoreRankingRequiresAvailableAndTested()
{
    AutoNodeEvaluation untested = evaluation(QStringLiteral("jp-untested"), QStringLiteral("JP"), 10);
    AutoNodeEvaluation unavailable = testedEvaluation(
        QStringLiteral("jp-down"), QStringLiteral("JP"), 10, false);
    const QList<AutoNodeEvaluation> evaluations{
        untested,
        unavailable,
        testedEvaluation(QStringLiteral("jp-ok"), QStringLiteral("JP"), 800),
    };
    const QHash<QString, double> scores{
        {QStringLiteral("jp-untested"), 1.0},
        {QStringLiteral("jp-down"), 2.0},
        {QStringLiteral("jp-ok"), 800.0},
    };

    // The two low scores belong to nodes that cannot carry traffic, so they are not candidates.
    QCOMPARE(
        bestAutoServerIdForCountryByScore(evaluations, QStringLiteral("JP"), scores),
        QStringLiteral("jp-ok"));
}

void AutoCountrySelectionTests::scoreRankingKeepsListOrderOnTies()
{
    const QList<AutoNodeEvaluation> evaluations{
        testedEvaluation(QStringLiteral("jp-a"), QStringLiteral("JP"), 100),
        testedEvaluation(QStringLiteral("jp-b"), QStringLiteral("JP"), 100),
    };
    const QHash<QString, double> scores{
        {QStringLiteral("jp-a"), 100.0},
        {QStringLiteral("jp-b"), 100.0},
    };

    QCOMPARE(
        bestAutoServerIdForCountryByScore(evaluations, QStringLiteral("JP"), scores),
        QStringLiteral("jp-a"));
}

QTEST_MAIN(AutoCountrySelectionTests)

#include "AutoCountrySelectionTests.moc"
