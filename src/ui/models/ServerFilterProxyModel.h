#pragma once

#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>

class ServerFilterProxyModel final : public QSortFilterProxyModel {
    Q_OBJECT

public:
    enum class SubscriptionFilterMode {
        Ungrouped = 0,
        Subscription
    };

    // Location-grouped view (the "group by location" toggle): Favorite shows every node whose
    // favorite flag is set, across subscriptions; Location shows the nodes whose resolved
    // location id (manual override first, then name inference -- LocationCatalog) matches. These
    // modes coexist with SubscriptionFilterMode rather than replacing it: the subscription view
    // still exists and still uses its own mode, only one mode is active at a time, and the text
    // filter keeps applying on top of either one.
    enum class LocationFilterMode {
        Favorite = 0,
        Location
    };

    explicit ServerFilterProxyModel(QObject* parent = nullptr);

    void setKnownSubscriptionIds(const QSet<QString>& subscriptionIds);
    void setSubscriptionFilterMode(SubscriptionFilterMode mode, QString subscriptionId = {});
    QString subscriptionId() const;
    void setLocationFilterMode(LocationFilterMode mode, QString locationId = {});
    // Back to the subscription filter: the toggle is off. Explicit rather than a magic location
    // id so a caller cannot accidentally leave a stale location filter behind by passing "".
    void clearLocationFilterMode();
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override;

private:
    bool matchesSubscriptionFilter(const QString& subscriptionId) const;
    bool matchesLocationFilter(const QString& locationOverride, const QString& displayName, bool favorite) const;
    bool matchesTextFilter(int sourceRow, const QModelIndex& sourceParent) const;

    QSet<QString> knownSubscriptionIds_;
    SubscriptionFilterMode subscriptionFilterMode_ = SubscriptionFilterMode::Ungrouped;
    QString subscriptionId_;
    // True while the tab bar is in location mode; when false the location fields are ignored and
    // the subscription filter decides, so a caller that never opted in sees the old behaviour.
    bool locationModeActive_ = false;
    LocationFilterMode locationFilterMode_ = LocationFilterMode::Favorite;
    QString locationId_;
};
