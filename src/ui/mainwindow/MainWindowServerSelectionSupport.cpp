#include "ui/mainwindow/MainWindowServerSelectionSupport.h"

#include "ui/mainwindow/SubscriptionViewSupport.h"
#include "ui/models/ServerFilterProxyModel.h"
#include "ui/models/ServerTableModel.h"

namespace {

constexpr int ServerLookupColumn = 0;

} // namespace

QString MainWindowServerSelectionSupport::subscriptionSelectionIdForServer(const ServerTableRow& server)
{
    const QString subscriptionId = server.subId.trimmed();
    return subscriptionId.isEmpty()
        ? QStringLiteral("__unsubscribed__")
        : subscriptionId;
}

QString MainWindowServerSelectionSupport::subscriptionTabKeyForServer(const ServerTableRow& server)
{
    const QString subscriptionId = server.subId.trimmed();
    return subscriptionId.isEmpty()
        ? QStringLiteral("ungrouped")
        : QStringLiteral("sub:%1").arg(subscriptionId);
}

QString MainWindowServerSelectionSupport::restoredSubscriptionTabKey(
    const QString& persistedSelectionId,
    const QString& currentIndexId,
    const ServerTableModel* serverModel)
{
    QString preferredTabKey = SubscriptionViewSupport::tabKeyFromSelectionId(persistedSelectionId);
    if (serverModel == nullptr || currentIndexId.trimmed().isEmpty()) {
        return preferredTabKey;
    }

    const ServerTableRow* currentServer = serverModel->itemByIndexId(currentIndexId);
    if (currentServer == nullptr) {
        return preferredTabKey;
    }

    const QString serverTabKey = subscriptionTabKeyForServer(*currentServer);
    return preferredTabKey == serverTabKey ? preferredTabKey : serverTabKey;
}

QStringList MainWindowServerSelectionSupport::subscriptionServerIndexIds(
    const ServerTableModel* serverModel,
    const QString& subscriptionId)
{
    const QString normalizedId = subscriptionId.trimmed();
    if (serverModel == nullptr || normalizedId.isEmpty()) {
        return {};
    }

    QStringList indexIds;
    for (int row = 0; row < serverModel->rowCount(); ++row) {
        const ServerTableRow* item = serverModel->itemAt(row);
        if (item == nullptr || item->subId.trimmed() != normalizedId || item->indexId.trimmed().isEmpty()) {
            continue;
        }

        indexIds.append(item->indexId);
    }

    return indexIds;
}

QModelIndex MainWindowServerSelectionSupport::visibleProxyIndexForServer(
    const ServerFilterProxyModel* serverFilterModel,
    const ServerTableModel* serverModel,
    const QString& indexId)
{
    if (serverFilterModel == nullptr || serverModel == nullptr || indexId.trimmed().isEmpty()) {
        return {};
    }

    for (int proxyRow = 0; proxyRow < serverFilterModel->rowCount(); ++proxyRow) {
        const QModelIndex proxyIndex = serverFilterModel->index(proxyRow, ServerLookupColumn);
        const QModelIndex sourceIndex = serverFilterModel->mapToSource(proxyIndex);
        const ServerTableRow* item = serverModel->itemAt(sourceIndex.row());
        if (item != nullptr && item->indexId == indexId) {
            return proxyIndex;
        }
    }

    return {};
}
