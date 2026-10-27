#include <QtTest>

#include "app/SubscriptionImportFailure.h"

class SubscriptionImportFailureTests : public QObject {
    Q_OBJECT

private slots:
    void explainsASingleImportThatImportedNothing();
    void explainsABatchThatImportedNothing();
    void ignoresASuccess();
    void ignoresAFailureWithNothingToSay();
    void ignoresACancellation();
    void ignoresABatchThatKeptServersOnTheTable();
    void reasonStepsOverTheBatchSummaryToTheFailureLine();
    void reasonOfASingleFailureIsTheMessageItself();
};

void SubscriptionImportFailureTests::explainsASingleImportThatImportedNothing()
{
    // A share-link or clipboard import fails in one sentence. The empty table is
    // now this import's doing, so the panel is the right place for the sentence.
    const OperationResult result = OperationResult::fail(
        QStringLiteral("cnb.cool returned no supported servers. skipped 3 node(s): hysteria, ssh\n"
                       "Subscription parse: pass 0 clash count=0"));
    QVERIFY(SubscriptionImportFailure::shouldExplainInEmptyServerPanel(result));
}

void SubscriptionImportFailureTests::explainsABatchThatImportedNothing()
{
    // The batch summary names the node count, and this run put nothing on the
    // table -- which is exactly the state the panel exists to explain.
    const OperationResult result = OperationResult::fail(
        QStringLiteral("Updated 0 of 2 subscription(s), imported 0 server(s).\n"
                       "[FAIL] feed returned no supported servers. skipped 2 node(s): hysteria, snell\n"
                       "Subscription parse: pass 0 clash count=0\n"
                       "[FAIL] dead download failed: Connection refused"));
    QVERIFY(SubscriptionImportFailure::shouldExplainInEmptyServerPanel(result));
}

void SubscriptionImportFailureTests::ignoresASuccess()
{
    QVERIFY(!SubscriptionImportFailure::shouldExplainInEmptyServerPanel(
        OperationResult::ok(QStringLiteral("Updated 1 of 1 subscription(s), imported 12 server(s)."))));
}

void SubscriptionImportFailureTests::ignoresAFailureWithNothingToSay()
{
    // A silent failure would replace the panel's first-run copy with an "Import
    // failed" title and no reason under it, which is worse than saying nothing.
    QVERIFY(!SubscriptionImportFailure::shouldExplainInEmptyServerPanel(OperationResult::fail(QString())));
    QVERIFY(!SubscriptionImportFailure::shouldExplainInEmptyServerPanel(OperationResult::fail(QStringLiteral("   \n  "))));
}

void SubscriptionImportFailureTests::ignoresACancellation()
{
    // "Cancelled by shutdown" is not a verdict about the subscription, and the
    // panel would keep saying it long after the shutdown that produced it.
    QVERIFY(!SubscriptionImportFailure::shouldExplainInEmptyServerPanel(
        OperationResult::cancel(QStringLiteral("Subscription update cancelled by shutdown."))));
}

void SubscriptionImportFailureTests::ignoresABatchThatKeptServersOnTheTable()
{
    // One subscription failing while another succeeds leaves rows in the table, so
    // the panel is not showing and has nothing to explain. Reading the node count
    // is what separates this from the total failure above; both are "[FAIL]" batches.
    const OperationResult result = OperationResult::fail(
        QStringLiteral("Updated 1 of 2 subscription(s), imported 12 server(s).\n"
                       "[FAIL] dead download failed: Connection refused"));
    QVERIFY(!SubscriptionImportFailure::shouldExplainInEmptyServerPanel(result));
}

void SubscriptionImportFailureTests::reasonStepsOverTheBatchSummaryToTheFailureLine()
{
    // The panel shows one line, and a batch starts with "Updated 0 of 2
    // subscription(s), imported 0 server(s)." -- the state, not the reason. Showing
    // that left the user with an empty table and a sentence that restated it.
    const QString reason = SubscriptionImportFailure::reasonForEmptyServerPanel(
        QStringLiteral("Updated 0 of 2 subscription(s), imported 0 server(s).\n"
                       "[FAIL] feed returned no supported servers. skipped 2 node(s): hysteria, snell\n"
                       "Subscription parse: pass 0 clash count=0\n"
                       "[FAIL] dead download failed: Connection refused"));
    QCOMPARE(reason, QStringLiteral("feed returned no supported servers. "
                                    "skipped 2 node(s): hysteria, snell"));
}

void SubscriptionImportFailureTests::reasonOfASingleFailureIsTheMessageItself()
{
    // Nothing to step over: the sentence and the trail are all there is, so the panel
    // gets the whole thing and keeps the trail behind its first line in the tooltip.
    const QString message = QStringLiteral(
        "cnb.cool returned no supported servers. skipped 3 node(s): hysteria, ssh\n"
        "Subscription parse: pass 0 clash count=0");
    QCOMPARE(SubscriptionImportFailure::reasonForEmptyServerPanel(message), message);
}

QTEST_MAIN(SubscriptionImportFailureTests)

#include "SubscriptionImportFailureTests.moc"
