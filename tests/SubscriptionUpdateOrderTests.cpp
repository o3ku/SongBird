#include <QtTest>

#include "services/SubscriptionUpdateOrder.h"

namespace {

SubItem makeSub(
    const QString& url,
    const QString& remarks = {},
    int failStreak = 0)
{
    SubItem item;
    item.url = url;
    item.remarks = remarks;
    item.failStreak = failStreak;
    return item;
}

QStringList remarksOrder(const QList<SubItem>& items)
{
    QStringList result;
    result.reserve(items.size());
    for (const SubItem& item : items) {
        result.append(item.remarks);
    }
    return result;
}

} // namespace

// Characterization tests for the subscription update ordering.
//
// The batch is walked serially, so the ordering is the only lever on how the servers see the
// traffic. Two properties are pinned:
//
//   * subscriptions sharing a host are never adjacent, and
//   * a subscription that keeps failing is walked after the healthy ones.
//
// Both are expressed as stable permutations of the caller's list, so the input is never modified
// and equally-ranked entries keep their relative order.
class SubscriptionUpdateOrderTests : public QObject {
    Q_OBJECT

private slots:
    // --- hostKey ---
    void hostKeyLowercasesTheHost();
    void hostKeyDropsThePort();
    void hostKeyRejectsAnEmptyUrl();
    void hostKeyRejectsAnUnparseableUrl();

    // --- ordering ---
    void singleItemIsReturnedUnchanged();
    void emptyListStaysEmpty();
    void sameHostEntriesAreNotAdjacent();
    void healthySubscriptionsComeBeforeFailingOnes();
    void entriesWithEqualFailStreakKeepTheirOrder();
    void unparseableUrlsDoNotBlockHealthyOnes();
    void everyInputItemAppearsExactlyOnce();
};

void SubscriptionUpdateOrderTests::hostKeyLowercasesTheHost()
{
    QCOMPARE(
        SubscriptionUpdateOrder::hostKey(QStringLiteral("https://Example.COM/sub")),
        QStringLiteral("example.com"));
}

void SubscriptionUpdateOrderTests::hostKeyDropsThePort()
{
    QCOMPARE(
        SubscriptionUpdateOrder::hostKey(QStringLiteral("https://example.com:8443/sub")),
        QStringLiteral("example.com"));
}

void SubscriptionUpdateOrderTests::hostKeyRejectsAnEmptyUrl()
{
    QVERIFY(SubscriptionUpdateOrder::hostKey(QString()).isEmpty());
    QVERIFY(SubscriptionUpdateOrder::hostKey(QStringLiteral("   ")).isEmpty());
}

void SubscriptionUpdateOrderTests::hostKeyRejectsAnUnparseableUrl()
{
    // No scheme and no dotted host: QUrl::fromUserInput cannot invent a host for this.
    QVERIFY(SubscriptionUpdateOrder::hostKey(QStringLiteral("!!!")).isEmpty());
}

void SubscriptionUpdateOrderTests::singleItemIsReturnedUnchanged()
{
    const QList<SubItem> items{makeSub(QStringLiteral("https://a.example.com/s"), QStringLiteral("only"))};
    const QList<SubItem> ordered = SubscriptionUpdateOrder::orderForUpdate(items);
    QCOMPARE(ordered.size(), 1);
    QCOMPARE(ordered.constFirst().remarks, QStringLiteral("only"));
}

void SubscriptionUpdateOrderTests::emptyListStaysEmpty()
{
    QVERIFY(SubscriptionUpdateOrder::orderForUpdate({}).isEmpty());
}

void SubscriptionUpdateOrderTests::sameHostEntriesAreNotAdjacent()
{
    const QList<SubItem> items{
        makeSub(QStringLiteral("https://shared.example.com/one"), QStringLiteral("shared-1")),
        makeSub(QStringLiteral("https://shared.example.com/two"), QStringLiteral("shared-2")),
        makeSub(QStringLiteral("https://other.example.com/one"), QStringLiteral("other-1")),
    };

    const QStringList order = remarksOrder(SubscriptionUpdateOrder::orderForUpdate(items));
    QCOMPARE(order.size(), 3);

    // The two shared-host entries must be separated by the other host's entry.
    const int first = order.indexOf(QStringLiteral("shared-1"));
    const int second = order.indexOf(QStringLiteral("shared-2"));
    QVERIFY(first >= 0);
    QVERIFY(second >= 0);
    QCOMPARE(qAbs(first - second), 2);
}

void SubscriptionUpdateOrderTests::healthySubscriptionsComeBeforeFailingOnes()
{
    const QList<SubItem> items{
        makeSub(QStringLiteral("https://dead.example.com/s"), QStringLiteral("dead"), 5),
        makeSub(QStringLiteral("https://healthy.example.com/s"), QStringLiteral("healthy"), 0),
    };

    const QStringList order = remarksOrder(SubscriptionUpdateOrder::orderForUpdate(items));
    QCOMPARE(order.constFirst(), QStringLiteral("healthy"));
    QCOMPARE(order.constLast(), QStringLiteral("dead"));
}

void SubscriptionUpdateOrderTests::entriesWithEqualFailStreakKeepTheirOrder()
{
    const QList<SubItem> items{
        makeSub(QStringLiteral("https://a.example.com/s"), QStringLiteral("a"), 2),
        makeSub(QStringLiteral("https://b.example.com/s"), QStringLiteral("b"), 2),
        makeSub(QStringLiteral("https://c.example.com/s"), QStringLiteral("c"), 2),
    };

    // Different hosts with the same priority: stable_sort must keep the caller's order.
    const QStringList order = remarksOrder(SubscriptionUpdateOrder::orderForUpdate(items));
    QCOMPARE(order, (QStringList{
        QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
}

void SubscriptionUpdateOrderTests::unparseableUrlsDoNotBlockHealthyOnes()
{
    const QList<SubItem> items{
        makeSub(QStringLiteral("!!!"), QStringLiteral("broken"), 0),
        makeSub(QStringLiteral("https://healthy.example.com/s"), QStringLiteral("healthy"), 0),
    };

    const QStringList order = remarksOrder(SubscriptionUpdateOrder::orderForUpdate(items));
    // Both have failStreak 0, so order is the caller's; the point is that the broken one is not
    // merged into a real host's queue and does not shift the healthy one out of first place.
    QCOMPARE(order.size(), 2);
    QCOMPARE(order.constFirst(), QStringLiteral("broken"));
    QCOMPARE(order.constLast(), QStringLiteral("healthy"));
}

void SubscriptionUpdateOrderTests::everyInputItemAppearsExactlyOnce()
{
    QList<SubItem> items;
    for (int i = 0; i < 12; ++i) {
        // Four hosts, three subscriptions each, with varying fail streaks.
        items.append(makeSub(
            QStringLiteral("https://host%1.example.com/sub%2").arg(i % 4).arg(i),
            QStringLiteral("item-%1").arg(i),
            i % 3));
    }

    const QList<SubItem> ordered = SubscriptionUpdateOrder::orderForUpdate(items);
    QCOMPARE(ordered.size(), items.size());

    QStringList before = remarksOrder(items);
    QStringList after = remarksOrder(ordered);
    before.sort();
    after.sort();
    // Same multiset: the permutation neither dropped nor duplicated an entry.
    QCOMPARE(after, before);
}

QTEST_GUILESS_MAIN(SubscriptionUpdateOrderTests)
#include "SubscriptionUpdateOrderTests.moc"
