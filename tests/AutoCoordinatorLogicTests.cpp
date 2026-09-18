#include <QtTest/QtTest>

#include "auto/AutoCoordinatorLogic.h"

namespace {

AutoNodeEvaluation evaluation(
    const QString& countryDisplay,
    qint64 latencyMs,
    bool available,
    const QString& error = {})
{
    AutoNodeEvaluation result;
    result.countryDisplay = countryDisplay;
    result.latencyMs = latencyMs;
    result.available = available;
    result.error = error;
    return result;
}

AutoCountrySummary country(const QString& countryCode, int availableCount)
{
    AutoCountrySummary result;
    result.countryCode = countryCode;
    result.availableCount = availableCount;
    return result;
}

VmessItem server(
    const QString& indexId,
    const QString& subId,
    const QString& remarks,
    const QString& address,
    int port = 443,
    const QString& testResult = {})
{
    VmessItem item;
    item.indexId = indexId;
    item.subId = subId;
    item.remarks = remarks;
    item.address = address;
    item.port = port;
    item.testResult = testResult;
    return item;
}

// Builds a config holding the given servers with currentIndexId set.
Config configWith(const QList<VmessItem>& servers, const QString& currentIndexId)
{
    Config config;
    config.collection().servers = servers;
    config.currentIndexId = currentIndexId;
    return config;
}

} // namespace

class AutoCoordinatorLogicTests : public QObject
{
    Q_OBJECT

private slots:
    void normalizeSubscriptionUrlTrimsAndDropsComments();
    void normalizeSubscriptionUrlKeepsInlineHash();
    void normalizeAutoSelectionStrategyFallsBackToLowestLatency();
    void evaluationStateTextFormatsAvailableAndFailedNodes();
    void firstCountryPrefersCountryWithNodes();
    void firstCountryFallsBackToFirstEntryThenEmpty();
    void findServerMatchesExactIndexIdOnly();
    void preserveActiveServerDetachesFromSubscription();
    void preserveActiveServerLeavesOtherServersAlone();
    void preserveActiveServerDoesNotDoubleTagRemarks();
    void preserveActiveServerFallsBackToAddressWhenRemarksEmpty();
    void reconcileFoldsActiveCopyOntoSubscriptionServer();
    void reconcileReturnsFalseWhenActiveServerStillHasSubscription();
    void reconcileReturnsFalseWhenActiveServerMissing();
    void reconcileReturnsFalseWhenAlreadyCurrentAndNoDuplicate();
    void reconcileSetsCurrentIndexIdWhenNoDuplicateExists();
};

void AutoCoordinatorLogicTests::normalizeSubscriptionUrlTrimsAndDropsComments()
{
    using AutoCoordinatorLogic::normalizeSubscriptionUrl;

    QCOMPARE(normalizeSubscriptionUrl(QStringLiteral("  https://a.example/sub  ")),
             QStringLiteral("https://a.example/sub"));
    QCOMPARE(normalizeSubscriptionUrl(QStringLiteral("# comment")), QString());
    // The '#' is only a comment marker once the line is trimmed.
    QCOMPARE(normalizeSubscriptionUrl(QStringLiteral("   # comment")), QString());
    QCOMPARE(normalizeSubscriptionUrl(QString()), QString());
    QCOMPARE(normalizeSubscriptionUrl(QStringLiteral("   ")), QString());
}

void AutoCoordinatorLogicTests::normalizeSubscriptionUrlKeepsInlineHash()
{
    using AutoCoordinatorLogic::normalizeSubscriptionUrl;

    QCOMPARE(normalizeSubscriptionUrl(QStringLiteral("https://a.example/#frag")),
             QStringLiteral("https://a.example/#frag"));
}

void AutoCoordinatorLogicTests::normalizeAutoSelectionStrategyFallsBackToLowestLatency()
{
    using AutoCoordinatorLogic::kAutoStrategyFirstAvailable;
    using AutoCoordinatorLogic::kAutoStrategyLowestLatency;
    using AutoCoordinatorLogic::normalizeAutoSelectionStrategy;

    QCOMPARE(normalizeAutoSelectionStrategy(QStringLiteral("firstAvailable")), kAutoStrategyFirstAvailable);
    QCOMPARE(normalizeAutoSelectionStrategy(QStringLiteral("FIRSTAVAILABLE")), kAutoStrategyFirstAvailable);
    QCOMPARE(normalizeAutoSelectionStrategy(QStringLiteral("  firstAvailable  ")), kAutoStrategyFirstAvailable);
    QCOMPARE(normalizeAutoSelectionStrategy(QStringLiteral("lowestLatency")), kAutoStrategyLowestLatency);

    // An unknown or corrupt stored value must not silently behave like
    // "first available"; it degrades to the latency-based pick.
    QCOMPARE(normalizeAutoSelectionStrategy(QStringLiteral("garbage")), kAutoStrategyLowestLatency);
    QCOMPARE(normalizeAutoSelectionStrategy(QString()), kAutoStrategyLowestLatency);
    QCOMPARE(normalizeAutoSelectionStrategy(QStringLiteral("   ")), kAutoStrategyLowestLatency);
}

void AutoCoordinatorLogicTests::evaluationStateTextFormatsAvailableAndFailedNodes()
{
    using AutoCoordinatorLogic::evaluationStateText;

    QCOMPARE(evaluationStateText(evaluation(QStringLiteral("US"), 42, true)),
             QStringLiteral("US 42 ms"));
    QCOMPARE(evaluationStateText(evaluation(QString(), -1, false)), QStringLiteral("Failed"));
    // The error text is trimmed before it is shown.
    QCOMPARE(evaluationStateText(evaluation(QString(), -1, false, QStringLiteral("  timeout  "))),
             QStringLiteral("timeout"));
    // An available node ignores the error field entirely.
    QCOMPARE(evaluationStateText(evaluation(QStringLiteral("JP"), 7, true, QStringLiteral("stale"))),
             QStringLiteral("JP 7 ms"));
}

void AutoCoordinatorLogicTests::firstCountryPrefersCountryWithNodes()
{
    using AutoCoordinatorLogic::firstCountryWithNodesOrFirst;

    const QList<AutoCountrySummary> countries{
        country(QStringLiteral("JP"), 0),
        country(QStringLiteral("US"), 2),
        country(QStringLiteral("HK"), 1),
    };

    // US is the first entry that actually has nodes, even though HK has one too.
    QCOMPARE(firstCountryWithNodesOrFirst(countries), QStringLiteral("US"));
}

void AutoCoordinatorLogicTests::firstCountryFallsBackToFirstEntryThenEmpty()
{
    using AutoCoordinatorLogic::firstCountryWithNodesOrFirst;

    const QList<AutoCountrySummary> noneAvailable{
        country(QStringLiteral("JP"), 0),
        country(QStringLiteral("US"), 0),
    };
    QCOMPARE(firstCountryWithNodesOrFirst(noneAvailable), QStringLiteral("JP"));

    QCOMPARE(firstCountryWithNodesOrFirst({}), QString());

    const QList<AutoCountrySummary> single{country(QStringLiteral("HK"), 3)};
    QCOMPARE(firstCountryWithNodesOrFirst(single), QStringLiteral("HK"));
}

void AutoCoordinatorLogicTests::findServerMatchesExactIndexIdOnly()
{
    using AutoCoordinatorLogic::findServerInConfig;

    const Config config = configWith(
        {server(QStringLiteral("alpha"), QStringLiteral("sub"), QStringLiteral("A"), QStringLiteral("a.example")),
         server(QStringLiteral("Beta"), QString(), QStringLiteral("B"), QStringLiteral("b.example"))},
        QStringLiteral("alpha"));

    QVERIFY(findServerInConfig(config, QStringLiteral("alpha")) != nullptr);
    QVERIFY(findServerInConfig(config, QStringLiteral("Beta")) != nullptr);

    // A blank id must not match the first server.
    QVERIFY(findServerInConfig(config, QString()) == nullptr);
    QVERIFY(findServerInConfig(config, QStringLiteral("   ")) == nullptr);
    QVERIFY(findServerInConfig(config, QStringLiteral("gamma")) == nullptr);
    // The lookup is case-sensitive, unlike the country/strategy normalization.
    QVERIFY(findServerInConfig(config, QStringLiteral("beta")) == nullptr);
}

void AutoCoordinatorLogicTests::preserveActiveServerDetachesFromSubscription()
{
    using AutoCoordinatorLogic::preserveActiveServerForSubscriptionUpdate;

    Config config = configWith(
        {server(QStringLiteral("a"), QStringLiteral("sub-1"), QStringLiteral("Tokyo"),
                QStringLiteral("tokyo.example"))},
        QStringLiteral("a"));

    preserveActiveServerForSubscriptionUpdate(config, QStringLiteral("a"));

    const VmessItem& preserved = config.collection().servers.constFirst();
    QVERIFY(preserved.subId.isEmpty());
    QCOMPARE(preserved.remarks, QStringLiteral("Active copy | Tokyo"));
    // The address is untouched; only the subscription link is severed.
    QCOMPARE(preserved.address, QStringLiteral("tokyo.example"));
}

void AutoCoordinatorLogicTests::preserveActiveServerLeavesOtherServersAlone()
{
    using AutoCoordinatorLogic::preserveActiveServerForSubscriptionUpdate;

    Config config = configWith(
        {server(QStringLiteral("a"), QStringLiteral("sub-1"), QStringLiteral("Tokyo"), QStringLiteral("tokyo.example")),
         server(QStringLiteral("b"), QStringLiteral("sub-1"), QStringLiteral("Osaka"), QStringLiteral("osaka.example"))},
        QStringLiteral("a"));

    preserveActiveServerForSubscriptionUpdate(config, QStringLiteral("b"));

    QCOMPARE(config.collection().servers.at(0).subId, QStringLiteral("sub-1"));
    QCOMPARE(config.collection().servers.at(0).remarks, QStringLiteral("Tokyo"));
    QVERIFY(config.collection().servers.at(1).subId.isEmpty());
}

void AutoCoordinatorLogicTests::preserveActiveServerDoesNotDoubleTagRemarks()
{
    using AutoCoordinatorLogic::preserveActiveServerForSubscriptionUpdate;

    Config config = configWith(
        {server(QStringLiteral("a"), QStringLiteral("sub-1"), QStringLiteral("Active copy | Tokyo"),
                QStringLiteral("tokyo.example"))},
        QStringLiteral("a"));

    preserveActiveServerForSubscriptionUpdate(config, QStringLiteral("a"));

    // Re-running must not produce "Active copy | Active copy | Tokyo".
    QCOMPARE(config.collection().servers.constFirst().remarks, QStringLiteral("Active copy | Tokyo"));
}

void AutoCoordinatorLogicTests::preserveActiveServerFallsBackToAddressWhenRemarksEmpty()
{
    using AutoCoordinatorLogic::preserveActiveServerForSubscriptionUpdate;

    Config config = configWith(
        {server(QStringLiteral("a"), QStringLiteral("sub-1"), QString(), QStringLiteral("tokyo.example"))},
        QStringLiteral("a"));

    preserveActiveServerForSubscriptionUpdate(config, QStringLiteral("a"));

    QCOMPARE(config.collection().servers.constFirst().remarks,
             QStringLiteral("Active copy | tokyo.example"));
}

void AutoCoordinatorLogicTests::reconcileFoldsActiveCopyOntoSubscriptionServer()
{
    using AutoCoordinatorLogic::reconcilePreservedActiveServer;

    // The preserved copy has no subscription, and the freshly downloaded server
    // points at the same endpoint, so they share a reuse key.
    Config config = configWith(
        {server(QStringLiteral("a"), QString(), QStringLiteral("Active copy | Tokyo"),
                QStringLiteral("tokyo.example"), 443, QStringLiteral("88 ms")),
         server(QStringLiteral("b"), QStringLiteral("sub-1"), QStringLiteral("Tokyo"),
                QStringLiteral("tokyo.example"))},
        QStringLiteral("b"));

    QVERIFY(reconcilePreservedActiveServer(config, QStringLiteral("a")));

    // The downloaded server inherits the preserved id and its test result, and
    // the placeholder copy is gone.
    QCOMPARE(config.collection().servers.size(), 1);
    QCOMPARE(config.collection().servers.constFirst().indexId, QStringLiteral("a"));
    QCOMPARE(config.collection().servers.constFirst().testResult, QStringLiteral("88 ms"));
    QCOMPARE(config.collection().servers.constFirst().subId, QStringLiteral("sub-1"));
    QCOMPARE(config.currentIndexId, QStringLiteral("a"));
}

void AutoCoordinatorLogicTests::reconcileReturnsFalseWhenActiveServerStillHasSubscription()
{
    using AutoCoordinatorLogic::reconcilePreservedActiveServer;

    Config config = configWith(
        {server(QStringLiteral("a"), QStringLiteral("sub-1"), QStringLiteral("Tokyo"),
                QStringLiteral("tokyo.example"))},
        QStringLiteral("a"));

    // Still attached to a subscription: nothing to reconcile, and the caller
    // must not be told to save.
    QVERIFY(!reconcilePreservedActiveServer(config, QStringLiteral("a")));
}

void AutoCoordinatorLogicTests::reconcileReturnsFalseWhenActiveServerMissing()
{
    using AutoCoordinatorLogic::reconcilePreservedActiveServer;

    Config config = configWith(
        {server(QStringLiteral("b"), QStringLiteral("sub-1"), QStringLiteral("Tokyo"),
                QStringLiteral("tokyo.example"))},
        QStringLiteral("b"));

    QVERIFY(!reconcilePreservedActiveServer(config, QStringLiteral("missing")));
    QVERIFY(!reconcilePreservedActiveServer(config, QString()));
}

void AutoCoordinatorLogicTests::reconcileReturnsFalseWhenAlreadyCurrentAndNoDuplicate()
{
    using AutoCoordinatorLogic::reconcilePreservedActiveServer;

    // Detached server, no matching subscription server, and already current:
    // there is nothing left to change.
    Config config = configWith(
        {server(QStringLiteral("a"), QString(), QStringLiteral("Active copy | Tokyo"),
                QStringLiteral("tokyo.example"))},
        QStringLiteral("a"));

    QVERIFY(!reconcilePreservedActiveServer(config, QStringLiteral("a")));
    QCOMPARE(config.currentIndexId, QStringLiteral("a"));
}

void AutoCoordinatorLogicTests::reconcileSetsCurrentIndexIdWhenNoDuplicateExists()
{
    using AutoCoordinatorLogic::reconcilePreservedActiveServer;

    // The detached server is not current yet and no duplicate exists, so the
    // only change is promoting it to current.
    Config config = configWith(
        {server(QStringLiteral("a"), QString(), QStringLiteral("Active copy | Tokyo"),
                QStringLiteral("tokyo.example")),
         server(QStringLiteral("b"), QStringLiteral("sub-1"), QStringLiteral("Osaka"),
                QStringLiteral("osaka.example"))},
        QStringLiteral("b"));

    QVERIFY(reconcilePreservedActiveServer(config, QStringLiteral("a")));
    QCOMPARE(config.currentIndexId, QStringLiteral("a"));
    QCOMPARE(config.collection().servers.size(), 2);
}

QTEST_MAIN(AutoCoordinatorLogicTests)

#include "AutoCoordinatorLogicTests.moc"
