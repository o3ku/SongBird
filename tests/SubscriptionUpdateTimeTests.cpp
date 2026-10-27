#include <QtTest>

#include "domain/models/Config.h"
#include "services/SubscriptionUpdateFailureSummary.h"
#include "services/SubscriptionUpdateTime.h"

namespace {

constexpr qint64 kNow = 1770000000000LL; // an arbitrary fixed instant
constexpr qint64 kMinuteMs = 60LL * 1000LL;
constexpr qint64 kHourMs = 60LL * kMinuteMs;
constexpr qint64 kDayMs = 24LL * kHourMs;

SubItem makeSub(const QString& id, const QString& remarks)
{
    SubItem item;
    item.id = id;
    item.remarks = remarks;
    item.url = QStringLiteral("https://example.com/") + id;
    item.enabled = true;
    return item;
}

} // namespace

// The two labels the subscription surfaces show are a pure function of two timestamps, which is
// what makes the boundaries testable at all: "2 minutes ago" is not "0 hours ago", and a config
// written by a clock that is ahead must not read as a negative age.
class SubscriptionUpdateTimeTests : public QObject
{
    Q_OBJECT

private slots:
    void neverUpdatedIsStatedRatherThanDated();
    void recentRefreshReadsAsJustNow();
    void minutesHoursAndDaysCoarsen();
    void futureTimestampReadsAsJustNow();
    void tooltipIsEmptyWhenNothingHasHappenedYet();
    void tooltipLeadsWithTheFailure();
    void cellTextMarksNever();
    void failureSummaryAttributesLinesByDisplayName();
    void failureSummaryDoesNotPutOneFailureOnTwoSubscriptions();
    void failureSummaryIgnoresNonFailureLines();
    void failureSummaryFollowsTheServicesBatchOrder();
    void failureSummaryPrefersTheServiceAttribution();
    void failureSummarySkipsChecksThatOnlyConfirmedTheCache();
};

void SubscriptionUpdateTimeTests::neverUpdatedIsStatedRatherThanDated()
{
    QCOMPARE(SubscriptionUpdateTime::relativeLabel(0, kNow), QStringLiteral("Never updated"));
    QCOMPARE(SubscriptionUpdateTime::absoluteLabel(0), QStringLiteral("Never updated"));
    QCOMPARE(SubscriptionUpdateTime::ageMinutes(0, kNow), qint64(0));
}

void SubscriptionUpdateTimeTests::recentRefreshReadsAsJustNow()
{
    QCOMPARE(SubscriptionUpdateTime::relativeLabel(kNow - 1000, kNow), QStringLiteral("Just now"));
    // Exactly one minute old is a minute, not "just now": rounding a refresh down to "now" for
    // the first 90 seconds would answer a different question than the one asked.
    QCOMPARE(SubscriptionUpdateTime::relativeLabel(kNow - kMinuteMs, kNow), QStringLiteral("1 min ago"));
}

void SubscriptionUpdateTimeTests::minutesHoursAndDaysCoarsen()
{
    QCOMPARE(SubscriptionUpdateTime::relativeLabel(kNow - 59 * kMinuteMs, kNow), QStringLiteral("59 min ago"));
    QCOMPARE(SubscriptionUpdateTime::relativeLabel(kNow - kHourMs, kNow), QStringLiteral("1 h ago"));
    QCOMPARE(SubscriptionUpdateTime::relativeLabel(kNow - 23 * kHourMs, kNow), QStringLiteral("23 h ago"));
    QCOMPARE(SubscriptionUpdateTime::relativeLabel(kNow - kDayMs, kNow), QStringLiteral("1 d ago"));
    QCOMPARE(SubscriptionUpdateTime::relativeLabel(kNow - 5 * kDayMs, kNow), QStringLiteral("5 d ago"));
}

void SubscriptionUpdateTimeTests::futureTimestampReadsAsJustNow()
{
    // A config file travels between machines, so a timestamp in the future is an ordinary
    // outcome of a clock that is ahead. The unsigned difference would print about 584942417 years.
    QCOMPARE(SubscriptionUpdateTime::relativeLabel(kNow + 3 * kDayMs, kNow), QStringLiteral("Just now"));
    QCOMPARE(SubscriptionUpdateTime::ageMinutes(kNow + kDayMs, kNow), qint64(0));
}

void SubscriptionUpdateTimeTests::tooltipIsEmptyWhenNothingHasHappenedYet()
{
    // A fresh install has nothing to report, and a tab that says "Never updated" on launch is an
    // alarm about something that has not happened.
    QVERIFY(SubscriptionUpdateTime::tooltipText(QStringLiteral("Airport"), 0, kNow, QString()).isEmpty());
}

void SubscriptionUpdateTimeTests::tooltipLeadsWithTheFailure()
{
    const QString tooltip = SubscriptionUpdateTime::tooltipText(
        QStringLiteral("Airport"),
        kNow - 3 * kHourMs,
        kNow,
        QStringLiteral("Airport download failed: timeout"));

    QVERIFY(tooltip.startsWith(QStringLiteral("Airport\n")));
    QVERIFY(tooltip.contains(QStringLiteral("3 h ago")));
    // The reason is the service's line, shown as written: it already leads with the subscription
    // name, and the name is the header. Prefixing it again put the name twice in one tooltip.
    QVERIFY2(tooltip.contains(QStringLiteral("Airport download failed: timeout")), qPrintable(tooltip));
    // Once as the header, once inside the reason the service wrote -- never a third time, which is
    // what the old "Last attempt failed: <name> ..." wrapper produced.
    QCOMPARE(tooltip.count(QStringLiteral("Airport")), 2);
    QCOMPARE(tooltip.section(QChar('\n'), 2), QStringLiteral("Airport download failed: timeout"));

    // A failure with no successful refresh at all still produces a tooltip: that subscription has
    // nothing but the reason, and hiding it would leave the user with an empty hover.
    const QString onlyFailure = SubscriptionUpdateTime::tooltipText(
        QStringLiteral("Airport"), 0, kNow, QStringLiteral("Airport download failed: timeout"));
    QVERIFY(!onlyFailure.isEmpty());
    QCOMPARE(onlyFailure, QStringLiteral("Airport\nAirport download failed: timeout"));
}

void SubscriptionUpdateTimeTests::cellTextMarksNever()
{
    QCOMPARE(SubscriptionUpdateTime::cellText(0, kNow), QStringLiteral("-"));
    QCOMPARE(SubscriptionUpdateTime::cellText(kNow - 2 * kHourMs, kNow), QStringLiteral("2 h ago"));
}

// The service reports a batch as one multi-line message; the hover text needs one reason per
// subscription. Only the display name identifies a line, so the mapping is built from the config.
void SubscriptionUpdateTimeTests::failureSummaryAttributesLinesByDisplayName()
{
    Config config;
    config.collection().subscriptions = {
        makeSub(QStringLiteral("sub-1"), QStringLiteral("Airport")),
        makeSub(QStringLiteral("sub-2"), QStringLiteral("Backup"))};

    const QString message = QStringLiteral(
        "Updated 0 of 2 subscription(s), imported 0 server(s).\n"
        "[FAIL] Airport download failed: timeout\n"
        "[FAIL] Backup returned no supported servers.");

    const QHash<QString, QString> failures =
        SubscriptionUpdateFailureSummary::perSubscription(config, {}, message);

    QCOMPARE(failures.size(), 2);
    QCOMPARE(failures.value(QStringLiteral("sub-1")), QStringLiteral("Airport download failed: timeout"));
    QCOMPARE(failures.value(QStringLiteral("sub-2")), QStringLiteral("Backup returned no supported servers."));
}

void SubscriptionUpdateTimeTests::failureSummaryDoesNotPutOneFailureOnTwoSubscriptions()
{
    // Two subscriptions with the same remarks are two entries, and a name match must be spent
    // once: without that, one failure would be reported on both tabs and the second would never
    // show its own reason.
    Config config;
    config.collection().subscriptions = {
        makeSub(QStringLiteral("sub-1"), QStringLiteral("Airport")),
        makeSub(QStringLiteral("sub-2"), QStringLiteral("Airport"))};

    const QString message = QStringLiteral(
        "Updated 0 of 1 subscription(s), imported 0 server(s).\n"
        "[FAIL] Airport download failed: timeout");

    // Only sub-2 was asked for, so only sub-2 may be blamed.
    const QHash<QString, QString> failures =
        SubscriptionUpdateFailureSummary::perSubscription(config, {QStringLiteral("sub-2")}, message);

    QCOMPARE(failures.size(), 1);
    QVERIFY(!failures.contains(QStringLiteral("sub-1")));
    QCOMPARE(failures.value(QStringLiteral("sub-2")), QStringLiteral("Airport download failed: timeout"));
}

void SubscriptionUpdateTimeTests::failureSummaryIgnoresNonFailureLines()
{
    Config config;
    config.collection().subscriptions = {makeSub(QStringLiteral("sub-1"), QStringLiteral("Airport"))};

    const QString successMessage = QStringLiteral(
        "Updated 1 of 1 subscription(s), imported 4 server(s).\n"
        "[OK] Airport imported 4 server(s).");

    QVERIFY(SubscriptionUpdateFailureSummary::perSubscription(config, {}, successMessage).isEmpty());

    // A name that only appears as a substring of another name is not a match: "Air" must not
    // claim the line written for "Airport".
    Config nearMiss;
    nearMiss.collection().subscriptions = {makeSub(QStringLiteral("sub-1"), QStringLiteral("Air"))};
    const QString otherMessage = QStringLiteral(
        "Updated 0 of 1 subscription(s), imported 0 server(s).\n"
        "[FAIL] Airport download failed: timeout");
    QVERIFY(SubscriptionUpdateFailureSummary::perSubscription(nearMiss, {}, otherMessage).isEmpty());
}

// The service writes its [FAIL] lines in SubscriptionUpdateOrder order, not in config order, so a
// first-wins match has to walk the same permutation. Two same-named subscriptions on different
// hosts are exactly the case where the two orders disagree: the service updates the healthy host
// first, while the config lists the failing one first.
void SubscriptionUpdateTimeTests::failureSummaryFollowsTheServicesBatchOrder()
{
    Config config;
    SubItem broken = makeSub(QStringLiteral("sub-1"), QStringLiteral("Airport"));
    broken.url = QStringLiteral("https://first.example/sub");
    broken.failStreak = 3;
    SubItem healthy = makeSub(QStringLiteral("sub-2"), QStringLiteral("Airport"));
    healthy.url = QStringLiteral("https://second.example/sub");
    config.collection().subscriptions = {broken, healthy};

    // orderForUpdate() defers the failing host, so sub-2 is walked before sub-1 and the single
    // failure line is sub-2's.
    const QString message = QStringLiteral(
        "Updated 0 of 1 subscription(s), imported 0 server(s).\n"
        "[FAIL] Airport download failed: timeout");

    const QHash<QString, QString> failures =
        SubscriptionUpdateFailureSummary::perSubscription(config, {}, message);

    QCOMPARE(failures.size(), 1);
    QVERIFY2(failures.contains(QStringLiteral("sub-2")), qPrintable(failures.keys().join(QStringLiteral(","))));
    QVERIFY(!failures.contains(QStringLiteral("sub-1")));
}

// The attribution the service records while it walks the batch is the fact; the message is the
// prose. Reading the fact means two subscriptions sharing a display name can never be confused,
// and the failure reason arrives untranslated-by-accident.
void SubscriptionUpdateTimeTests::failureSummaryPrefersTheServiceAttribution()
{
    Config config;
    config.collection().subscriptions = {
        makeSub(QStringLiteral("sub-1"), QStringLiteral("Airport")),
        makeSub(QStringLiteral("sub-2"), QStringLiteral("Airport"))};

    OperationResult result = OperationResult::fail(QStringLiteral(
        "Updated 0 of 2 subscription(s), imported 0 server(s).\n"
        "[FAIL] Airport download failed: timeout\n"
        "[FAIL] Airport returned no supported servers."));
    result.subjectFailures = {
        {QStringLiteral("sub-2"), QStringLiteral("Airport download failed: timeout")},
        {QStringLiteral("sub-1"), QStringLiteral("Airport returned no supported servers.")}};

    const QHash<QString, QString> failures =
        SubscriptionUpdateFailureSummary::perSubscriptionFromMessage(config, {}, result);

    QCOMPARE(failures.size(), 2);
    QCOMPARE(failures.value(QStringLiteral("sub-1")), QStringLiteral("Airport returned no supported servers."));
    QCOMPARE(failures.value(QStringLiteral("sub-2")), QStringLiteral("Airport download failed: timeout"));
}

// Two service outcomes read as [FAIL] lines but are not reasons the user can act on: a 304 means
// the check succeeded and there was nothing new, and a subscription cancelled by shutdown is not
// a problem with the subscription. Showing either on a tab would be an alarm about nothing.
void SubscriptionUpdateTimeTests::failureSummarySkipsChecksThatOnlyConfirmedTheCache()
{
    Config config;
    config.collection().subscriptions = {
        makeSub(QStringLiteral("sub-1"), QStringLiteral("Airport")),
        makeSub(QStringLiteral("sub-2"), QStringLiteral("Backup"))};

    OperationResult result = OperationResult::fail(QStringLiteral(
        "Updated 1 of 2 subscription(s), imported 0 server(s).\n"
        "[FAIL] Airport is up to date (not modified).\n"
        "[FAIL] Backup update cancelled."));
    result.subjectFailures = {
        {QStringLiteral("sub-1"), QStringLiteral("Airport is up to date (not modified).")},
        {QStringLiteral("sub-2"), QStringLiteral("Backup update cancelled.")}};

    QVERIFY(SubscriptionUpdateFailureSummary::perSubscriptionFromMessage(config, {}, result).isEmpty());
}

QTEST_MAIN(SubscriptionUpdateTimeTests)
#include "SubscriptionUpdateTimeTests.moc"
