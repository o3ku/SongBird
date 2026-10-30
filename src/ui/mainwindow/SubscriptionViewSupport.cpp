#include "ui/mainwindow/SubscriptionViewSupport.h"

#include <QObject>

namespace {

const QString UngroupedTabKey = QStringLiteral("ungrouped");
const QString UnsubscribedSelectionId = QStringLiteral("__unsubscribed__");
const QString SubscriptionTabPrefix = QStringLiteral("sub:");
// Location items share the tab-key namespace with subscriptions ("sub:") and ungrouped, so
// every consumer that switches on tabData() -- the context menu, the filter, the persisted
// selection -- can tell the three apart by prefix alone.
const QString LocationTabPrefix = QStringLiteral("loc:");

} // namespace

QSet<QString> SubscriptionViewSupport::enabledSubscriptionIds(const QList<SubItem>& subscriptions)
{
    QSet<QString> ids;
    for (const SubItem& item : subscriptions) {
        if (!item.enabled) {
            continue;
        }

        const QString subscriptionId = item.id.trimmed();
        if (!subscriptionId.isEmpty()) {
            ids.insert(subscriptionId);
        }
    }
    return ids;
}

QString SubscriptionViewSupport::tabKeyFromSelectionId(const QString& selectionId)
{
    const QString normalized = selectionId.trimmed();
    if (normalized == UnsubscribedSelectionId
        || normalized.compare(UngroupedTabKey, Qt::CaseInsensitive) == 0) {
        return UngroupedTabKey;
    }

    return normalized.isEmpty()
        ? UngroupedTabKey
        : SubscriptionTabPrefix + normalized;
}

QString SubscriptionViewSupport::persistedSelectionIdFromTabKey(const QString& tabKey)
{
    if (tabKey == UngroupedTabKey) {
        return UnsubscribedSelectionId;
    }

    if (tabKey.startsWith(SubscriptionTabPrefix)) {
        return tabKey.mid(SubscriptionTabPrefix.size());
    }

    return {};
}

QString SubscriptionViewSupport::subscriptionIdFromTabKey(const QString& tabKey)
{
    return tabKey.startsWith(SubscriptionTabPrefix)
        ? tabKey.mid(SubscriptionTabPrefix.size())
        : QString();
}

QString SubscriptionViewSupport::tabKeyFromLocationId(const QString& locationId)
{
    const QString normalized = locationId.trimmed();
    return normalized.isEmpty() ? QString() : LocationTabPrefix + normalized;
}

QString SubscriptionViewSupport::locationIdFromTabKey(const QString& tabKey)
{
    return tabKey.startsWith(LocationTabPrefix)
        ? tabKey.mid(LocationTabPrefix.size())
        : QString();
}

QString SubscriptionViewSupport::subscriptionDisplayName(const SubItem& item)
{
    const QString remarks = item.remarks.trimmed();
    if (!remarks.isEmpty()) {
        return remarks;
    }

    const QString url = item.url.trimmed();
    if (!url.isEmpty()) {
        return url;
    }

    return item.id.trimmed().isEmpty()
        ? QObject::tr("Subscription")
        : item.id.trimmed();
}
