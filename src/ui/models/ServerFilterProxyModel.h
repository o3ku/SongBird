#pragma once

#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>

class ServerFilterProxyModel final : public QSortFilterProxyModel {
    Q_OBJECT

public:
    enum class SubscriptionFilterMode {
        All = 0,
        Ungrouped,
        Subscription
    };

    explicit ServerFilterProxyModel(QObject* parent = nullptr);

    void setKnownSubscriptionIds(const QSet<QString>& subscriptionIds);
    void setSubscriptionFilterMode(SubscriptionFilterMode mode, QString subscriptionId = {});
    SubscriptionFilterMode subscriptionFilterMode() const;
    QString subscriptionId() const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const override;

private:
    bool matchesSubscriptionFilter(const QString& subscriptionId) const;
    bool matchesTextFilter(int sourceRow, const QModelIndex& sourceParent) const;

    QSet<QString> knownSubscriptionIds_;
    SubscriptionFilterMode subscriptionFilterMode_ = SubscriptionFilterMode::All;
    QString subscriptionId_;
};
