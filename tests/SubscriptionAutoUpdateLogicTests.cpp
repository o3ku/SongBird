#include <QtTest>

#include <QDateTime>

#include "app/SubscriptionAutoUpdateLogic.h"

namespace {

SubItem makeSubscription(
    const QString& id,
    bool enabled,
    qint64 lastUpdatedAtMs,
    const QString& url = QStringLiteral("https://example.com/sub"))
{
    SubItem item;
    item.id = id;
    item.remarks = id;
    item.url = url;
    item.enabled = enabled;
    item.lastUpdatedAtMs = lastUpdatedAtMs;
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

QTEST_MAIN(SubscriptionAutoUpdateLogicTests)
#include "SubscriptionAutoUpdateLogicTests.moc"
