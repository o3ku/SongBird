#include <QtTest>

#include <QDateTime>

#include "app/SubscriptionAutoUpdateLogic.h"

namespace {

SubItem makeSubscription(
    const QString& id,
    bool enabled,
    qint64 lastUpdatedAtMs,
    const QString& url = QStringLiteral("https://example.com/sub"),
    int failStreak = 0)
{
    SubItem item;
    item.id = id;
    item.remarks = id;
    item.url = url;
    item.enabled = enabled;
    item.lastUpdatedAtMs = lastUpdatedAtMs;
    item.failStreak = failStreak;
    return item;
}

} // namespace

// What is pinned: the feature is off by default; a tick names exactly the enabled, url-bearing
// subscriptions whose last successful refresh is older than the interval (never-updated counts
// as due, so enabling baselines once); the interval is clamped so no stored value can make the
// feature hammer a provider; and disabled/blank/duplicate subscriptions never make the list.
class SubscriptionAutoUpdateLogicTests : public QObject {
    Q_OBJECT

private slots:
    void offByDefaultNamesNothing();
    void neverUpdatedSubscriptionsAreDueImmediately();
    void onlyExpiredSubscriptionsAreNamed();
    void disabledAndUrllessSubscriptionsAreSkipped();
    void intervalIsClamped();
    void failingSubscriptionsBackOffExponentially();
    void backoffStreakIsCapped();
};

void SubscriptionAutoUpdateLogicTests::offByDefaultNamesNothing()
{
    Config config;
    config.collection().subscriptions = {makeSubscription(QStringLiteral("sub-1"), true, 0)};
    QVERIFY(!config.subscriptionAutoUpdateEnabled);
    QCOMPARE(SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, QDateTime::currentMSecsSinceEpoch()).size(), 0);
}

void SubscriptionAutoUpdateLogicTests::neverUpdatedSubscriptionsAreDueImmediately()
{
    Config config;
    config.subscriptionAutoUpdateEnabled = true;
    config.collection().subscriptions = {
        makeSubscription(QStringLiteral("fresh"), true, QDateTime::currentMSecsSinceEpoch()),
        makeSubscription(QStringLiteral("never"), true, 0)};
    QCOMPARE(
        SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, QDateTime::currentMSecsSinceEpoch()),
        QStringList({QStringLiteral("never")}));
}

void SubscriptionAutoUpdateLogicTests::onlyExpiredSubscriptionsAreNamed()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 sixHours = 6 * 60 * 60 * 1000;
    Config config;
    config.subscriptionAutoUpdateEnabled = true;
    config.subscriptionAutoUpdateIntervalHours = 6;
    config.collection().subscriptions = {
        makeSubscription(QStringLiteral("edge-old"), true, now - sixHours),
        makeSubscription(QStringLiteral("just-inside"), true, now - sixHours + 60 * 1000),
        makeSubscription(QStringLiteral("recent"), true, now - 60 * 1000)};
    // The boundary is inclusive: exactly one interval old is due.
    QCOMPARE(
        SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, now),
        QStringList({QStringLiteral("edge-old")}));
}

void SubscriptionAutoUpdateLogicTests::disabledAndUrllessSubscriptionsAreSkipped()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    Config config;
    config.subscriptionAutoUpdateEnabled = true;
    config.collection().subscriptions = {
        makeSubscription(QStringLiteral("disabled"), false, 0),
        makeSubscription(QStringLiteral("no-url"), true, 0, QString()),
        makeSubscription(QStringLiteral("blank-url"), true, 0, QStringLiteral("   ")),
        makeSubscription(QStringLiteral("due"), true, 0)};
    QCOMPARE(
        SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, now),
        QStringList({QStringLiteral("due")}));
}

void SubscriptionAutoUpdateLogicTests::intervalIsClamped()
{
    QCOMPARE(SubscriptionAutoUpdateLogic::normalizedIntervalHours(0), 1);
    QCOMPARE(SubscriptionAutoUpdateLogic::normalizedIntervalHours(-5), 1);
    QCOMPARE(SubscriptionAutoUpdateLogic::normalizedIntervalHours(6), 6);
    QCOMPARE(SubscriptionAutoUpdateLogic::normalizedIntervalHours(24 * 30), 24 * 7);

    // A fat-fingered 0 in the stored config means "at least an hour", not "every tick": a
    // never-updated subscription becomes due on the first tick and fresh on the next one.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    Config config;
    config.subscriptionAutoUpdateEnabled = true;
    config.subscriptionAutoUpdateIntervalHours = 0;
    config.collection().subscriptions = {makeSubscription(QStringLiteral("sub"), true, now - 30 * 60 * 1000)};
    QCOMPARE(SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, now).size(), 0);
}

void SubscriptionAutoUpdateLogicTests::failingSubscriptionsBackOffExponentially()
{
    using SubscriptionAutoUpdateLogic::kCheckIntervalMs;
    // An even tick index: streak 1 (period 2), 2 (period 4) and 3 (period 8) all pass here;
    // on the neighbouring tick they must all be held back.
    const qint64 evenTick = 1000 * kCheckIntervalMs;
    const qint64 oddTick = evenTick + kCheckIntervalMs;
    const qint64 longAgo = 0;
    Config config;
    config.subscriptionAutoUpdateEnabled = true;
    config.subscriptionAutoUpdateIntervalHours = 1;
    config.collection().subscriptions = {
        makeSubscription(QStringLiteral("healthy"), true, longAgo, QStringLiteral("https://example.com/sub"), 0),
        makeSubscription(QStringLiteral("streak1"), true, longAgo, QStringLiteral("https://example.com/sub"), 1),
        makeSubscription(QStringLiteral("streak2"), true, longAgo, QStringLiteral("https://example.com/sub"), 2),
        makeSubscription(QStringLiteral("streak3"), true, longAgo, QStringLiteral("https://example.com/sub"), 3)};

    // A healthy subscription is due on every tick once its interval elapsed.
    QVERIFY(SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, evenTick)
                .contains(QStringLiteral("healthy")));
    QVERIFY(SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, oddTick)
                .contains(QStringLiteral("healthy")));

    // On the aligned tick every streak is retried; on the next tick none of them is.
    QCOMPARE(SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, evenTick).size(), 4);
    const QStringList heldBack = SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, oddTick);
    QCOMPARE(heldBack, QStringList({QStringLiteral("healthy")}));

    // Tick 1002 is divisible by 2 but not by 4: only streak 1 joins the healthy one.
    const qint64 twoTicksLater = evenTick + 2 * kCheckIntervalMs;
    QCOMPARE(
        SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, twoTicksLater),
        QStringList({QStringLiteral("healthy"), QStringLiteral("streak1")}));
    // Tick 1004 is divisible by 4 but not by 8: streaks 1 and 2 join.
    const qint64 fourTicksLater = evenTick + 4 * kCheckIntervalMs;
    QCOMPARE(
        SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, fourTicksLater),
        QStringList({QStringLiteral("healthy"), QStringLiteral("streak1"), QStringLiteral("streak2")}));
}

void SubscriptionAutoUpdateLogicTests::backoffStreakIsCapped()
{
    using SubscriptionAutoUpdateLogic::kCheckIntervalMs;
    using SubscriptionAutoUpdateLogic::kMaxBackoffStreak;
    const qint64 alignedTick = 1000 * kCheckIntervalMs;
    Config config;
    config.subscriptionAutoUpdateEnabled = true;
    config.subscriptionAutoUpdateIntervalHours = 1;
    // A streak past the cap must not shift by more than 2^kMaxBackoffStreak ticks, and a
    // negative (hand-edited) streak must not break the modulo.
    config.collection().subscriptions = {
        makeSubscription(QStringLiteral("huge"), true, 0, QStringLiteral("https://example.com/sub"), kMaxBackoffStreak + 10),
        makeSubscription(QStringLiteral("negative"), true, 0, QStringLiteral("https://example.com/sub"), -3)};

    QCOMPARE(SubscriptionAutoUpdateLogic::backoffTickPeriod(kMaxBackoffStreak + 10),
             qint64(1) << kMaxBackoffStreak);
    QCOMPARE(SubscriptionAutoUpdateLogic::backoffTickPeriod(-3), qint64(1));
    // The huge streak is due on a tick divisible by the capped period, and the negative streak
    // (treated as zero) is due on every tick.
    const QStringList onAligned = SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, alignedTick);
    QVERIFY(onAligned.contains(QStringLiteral("huge")));
    QVERIFY(onAligned.contains(QStringLiteral("negative")));
    const QStringList offAligned = SubscriptionAutoUpdateLogic::dueSubscriptionIds(config, alignedTick + kCheckIntervalMs);
    QVERIFY(!offAligned.contains(QStringLiteral("huge")));
    QVERIFY(offAligned.contains(QStringLiteral("negative")));
}

QTEST_MAIN(SubscriptionAutoUpdateLogicTests)
#include "SubscriptionAutoUpdateLogicTests.moc"
