#include <QTest>

#include <QList>

#include "domain/models/SubItem.h"
#include "ui/mainwindow/ServerEmptyStatePolicy.h"

class ServerEmptyStatePolicyTests : public QObject {
    Q_OBJECT

private slots:
    void noSubscriptionIsTheOnlyFirstRun();
    void enabledSubscriptionWithoutNodesAsksForARefresh();
    void disabledSubscriptionDoesNotCountAsImported();
    void nodesHiddenByTheTabAreNotAFirstRun();
    void visibleNodesAreNeverAFirstRun();
    void textFilterWinsOverEverythingElse();
};

namespace {

SubItem makeSubscription(const QString& id, bool enabled = true)
{
    return SubItem{id, id, QStringLiteral("https://example.com/") + id, enabled, QStringLiteral("ua")};
}

} // namespace

void ServerEmptyStatePolicyTests::noSubscriptionIsTheOnlyFirstRun()
{
    using State = ServerEmptyStatePolicy::State;

    // Nothing imported, nothing held: the one case the onboarding page is for.
    QCOMPARE(ServerEmptyStatePolicy::decide(false, false, 0, {}), State::FirstRun);

    // A server on file but no subscription at all is also a first run in the sense
    // that matters -- the panel's job is still "here is how to get servers in" --
    // but only because no subscription exists; the row count is not the signal.
    QCOMPARE(ServerEmptyStatePolicy::decide(false, false, 3, {}), State::FirstRun);
}

void ServerEmptyStatePolicyTests::enabledSubscriptionWithoutNodesAsksForARefresh()
{
    using State = ServerEmptyStatePolicy::State;
    const QList<SubItem> subscriptions{makeSubscription(QStringLiteral("sub-1"))};

    // The reported bug: an enabled subscription holding zero nodes looked exactly like
    // a fresh install, so the app showed the onboarding page to a user whose import
    // had already been recorded.
    QCOMPARE(ServerEmptyStatePolicy::decide(false, false, 0, subscriptions), State::SubscriptionWithoutNodes);

    // Several subscriptions with nothing downloaded from any of them is the same
    // situation, not a different one.
    QCOMPARE(
        ServerEmptyStatePolicy::decide(
            false, false, 0,
            {makeSubscription(QStringLiteral("sub-1")), makeSubscription(QStringLiteral("sub-2"))}),
        State::SubscriptionWithoutNodes);
}

void ServerEmptyStatePolicyTests::disabledSubscriptionDoesNotCountAsImported()
{
    // A disabled subscription is not in any view, and its lack of nodes is a choice
    // rather than a failed download. Telling the user to refresh it would be advice
    // about a list they turned off.
    QCOMPARE(
        ServerEmptyStatePolicy::decide(false, false, 0, {makeSubscription(QStringLiteral("sub-1"), false)}),
        ServerEmptyStatePolicy::State::FirstRun);
}

void ServerEmptyStatePolicyTests::nodesHiddenByTheTabAreNotAFirstRun()
{
    // Servers exist, this tab shows none of them (the "Ungrouped" tab of a grouped
    // config). Importing again is the wrong advice, so this is a separate state.
    QCOMPARE(
        ServerEmptyStatePolicy::decide(false, false, 4, {makeSubscription(QStringLiteral("sub-1"))}),
        ServerEmptyStatePolicy::State::WithheldSubscription);
}

void ServerEmptyStatePolicyTests::visibleNodesAreNeverAFirstRun()
{
    // A visible row is never a decision the panel has to make -- it is not shown at
    // all -- but the function still has to answer something honest.
    QCOMPARE(ServerEmptyStatePolicy::decide(true, false, 0, {}), ServerEmptyStatePolicy::State::NodesElsewhere);
    QCOMPARE(ServerEmptyStatePolicy::decide(true, true, 2, {}), ServerEmptyStatePolicy::State::NodesElsewhere);
}

void ServerEmptyStatePolicyTests::textFilterWinsOverEverythingElse()
{
    // The user typed something that matched nothing. Even with no subscription on file
    // the first-run page would be wrong: the servers are there, the filter hid them,
    // and the fix is in the box above.
    QCOMPARE(ServerEmptyStatePolicy::decide(false, true, 0, {}), ServerEmptyStatePolicy::State::NodesElsewhere);
    QCOMPARE(
        ServerEmptyStatePolicy::decide(false, true, 5, {makeSubscription(QStringLiteral("sub-1"))}),
        ServerEmptyStatePolicy::State::NodesElsewhere);
}

QTEST_MAIN(ServerEmptyStatePolicyTests)

#include "ServerEmptyStatePolicyTests.moc"
