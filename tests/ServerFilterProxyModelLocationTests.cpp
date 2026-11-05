#include <QtTest>

#include <QRegularExpression>

#include "ui/models/ServerFilterProxyModel.h"
#include "ui/models/ServerTableModel.h"

namespace {

VmessItem makeServer(const QString& indexId, const QString& remarks, const QString& locationOverride = {}, bool favorite = false)
{
    VmessItem server;
    server.indexId = indexId;
    server.configType = ConfigType::VMess;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.remarks = remarks;
    server.locationOverride = locationOverride;
    server.favorite = favorite;
    return server;
}

QStringList visibleIds(ServerFilterProxyModel& filter)
{
    QStringList ids;
    for (int row = 0; row < filter.rowCount(); ++row) {
        const QModelIndex sourceIndex = filter.mapToSource(filter.index(row, 0));
        const auto* model = qobject_cast<const ServerTableModel*>(filter.sourceModel());
        const ServerTableRow* item = model == nullptr ? nullptr : model->itemAt(sourceIndex.row());
        if (item != nullptr) {
            ids.append(item->indexId);
        }
    }
    return ids;
}

} // namespace

// The location filter is what the location-grouped view shows. Three properties are pinned:
//
//   * Favorite shows favorited nodes across subscriptions -- favorite is a flag on the node, not a\n//     location, so it must not consult the location resolution at all,
//   * Location resolves each node through the catalog (override first, name inference second), so\n//     a manually moved node appears where the user put it, and
//   * the location modes never leak subscription semantics: a subscription filter set before the\n//     switch must not keep filtering once the location mode is active, and clearing the location\n//     mode puts the subscription filter back in charge unchanged.
class ServerFilterProxyModelLocationTests : public QObject {
    Q_OBJECT

private slots:
    void favoriteModeShowsFavoritedNodes();
    void locationModeResolvesOverrideBeforeName();
    void clearingLocationModeRestoresSubscriptionFilter();
    void textFilterStillAppliesOnTop();
};

void ServerFilterProxyModelLocationTests::favoriteModeShowsFavoritedNodes()
{
    ServerTableModel model;
    QList<VmessItem> servers;
    servers.append(makeServer(QStringLiteral("us-fav"), QStringLiteral("🇺🇸 US 01"), {}, true));
    servers.append(makeServer(QStringLiteral("jp-plain"), QStringLiteral("🇯🇵 JP 01")));
    servers.append(makeServer(QStringLiteral("hk-fav"), QStringLiteral("香港 02"), QStringLiteral("JP"), true));
    model.setItems(servers);

    ServerFilterProxyModel filter;
    filter.setSourceModel(&model);
    filter.setSubscriptionFilterMode(ServerFilterProxyModel::SubscriptionFilterMode::Ungrouped);
    filter.setLocationFilterMode(ServerFilterProxyModel::LocationFilterMode::Favorite);

    QCOMPARE(visibleIds(filter), QStringList({QStringLiteral("us-fav"), QStringLiteral("hk-fav")}));
}

void ServerFilterProxyModelLocationTests::locationModeResolvesOverrideBeforeName()
{
    ServerTableModel model;
    QList<VmessItem> servers;
    // Name says US, override says JP: the override is the user's hand, and it wins.
    servers.append(makeServer(QStringLiteral("moved"), QStringLiteral("🇺🇸 US 01"), QStringLiteral("JP")));
    // HK folds into the merged TW item.
    servers.append(makeServer(QStringLiteral("hk-node"), QStringLiteral("香港 IEPL")));
    // A stale override is ignored; the name decides.
    servers.append(makeServer(QStringLiteral("stale"), QStringLiteral("🇯🇵 JP 02"), QStringLiteral("XX")));
    servers.append(makeServer(QStringLiteral("other-node"), QStringLiteral("Random Relay")));
    model.setItems(servers);

    ServerFilterProxyModel filter;
    filter.setSourceModel(&model);

    filter.setLocationFilterMode(ServerFilterProxyModel::LocationFilterMode::Location, QStringLiteral("JP"));
    QCOMPARE(visibleIds(filter), QStringList({QStringLiteral("moved"), QStringLiteral("stale")}));

    filter.setLocationFilterMode(ServerFilterProxyModel::LocationFilterMode::Location, QStringLiteral("TW"));
    QCOMPARE(visibleIds(filter), QStringList({QStringLiteral("hk-node")}));

    filter.setLocationFilterMode(ServerFilterProxyModel::LocationFilterMode::Location, QStringLiteral("OTHER"));
    QCOMPARE(visibleIds(filter), QStringList({QStringLiteral("other-node")}));
}

void ServerFilterProxyModelLocationTests::clearingLocationModeRestoresSubscriptionFilter()
{
    ServerTableModel model;
    QList<VmessItem> servers;
    VmessItem subscribed = makeServer(QStringLiteral("sub-node"), QStringLiteral("🇺🇸 US 01"));
    subscribed.subId = QStringLiteral("sub-1");
    servers.append(subscribed);
    servers.append(makeServer(QStringLiteral("free-node"), QStringLiteral("🇯🇵 JP 01")));
    model.setItems(servers);

    ServerFilterProxyModel filter;
    filter.setSourceModel(&model);
    filter.setKnownSubscriptionIds({QStringLiteral("sub-1")});
    filter.setSubscriptionFilterMode(
        ServerFilterProxyModel::SubscriptionFilterMode::Subscription,
        QStringLiteral("sub-1"));

    // Switching to a location mode replaces the subscription filter for as long as it is active...
    filter.setLocationFilterMode(ServerFilterProxyModel::LocationFilterMode::Location, QStringLiteral("US"));
    QCOMPARE(visibleIds(filter), QStringList({QStringLiteral("sub-node")}));

    // ...and a node the subscription filter would have hidden is visible in location mode for as
    // long as it matches the location.
    filter.setLocationFilterMode(ServerFilterProxyModel::LocationFilterMode::Location, QStringLiteral("JP"));
    QCOMPARE(visibleIds(filter), QStringList({QStringLiteral("free-node")}));

    // Back on subscription view: the previous subscription selection still applies, unchanged.
    filter.clearLocationFilterMode();
    QCOMPARE(visibleIds(filter), QStringList({QStringLiteral("sub-node")}));
}

void ServerFilterProxyModelLocationTests::textFilterStillAppliesOnTop()
{
    ServerTableModel model;
    QList<VmessItem> servers;
    servers.append(makeServer(QStringLiteral("us-1"), QStringLiteral("🇺🇸 Los Angeles 01")));
    servers.append(makeServer(QStringLiteral("us-2"), QStringLiteral("🇺🇸 New York 02")));
    model.setItems(servers);

    ServerFilterProxyModel filter;
    filter.setSourceModel(&model);
    filter.setLocationFilterMode(ServerFilterProxyModel::LocationFilterMode::Location, QStringLiteral("US"));
    // The production filter always sets a regular expression (SubscriptionViewController::
    // applyFilter), never setFilterFixedString: on Qt5 the fixed-string mode leaves
    // filterRegularExpression() empty while still filtering, so a pattern-emptiness check would
    // misread it. Staying on the production path keeps the test honest about what is exercised.
    filter.setFilterRegularExpression(
        QRegularExpression(QStringLiteral("york"), QRegularExpression::CaseInsensitiveOption));

    QCOMPARE(visibleIds(filter), QStringList({QStringLiteral("us-2")}));
}

QTEST_MAIN(ServerFilterProxyModelLocationTests)
#include "ServerFilterProxyModelLocationTests.moc"
