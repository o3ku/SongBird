#pragma once

#include <QModelIndex>
#include <QString>
#include <QStringList>

class ServerFilterProxyModel;
class ServerTableModel;
struct ServerTableRow;

namespace MainWindowServerSelectionSupport {

QString subscriptionSelectionIdForServer(const ServerTableRow& server);
QString subscriptionTabKeyForServer(const ServerTableRow& server);
QString restoredSubscriptionTabKey(
    const QString& persistedSelectionId,
    const QString& currentIndexId,
    const ServerTableModel* serverModel);
QStringList subscriptionServerIndexIds(
    const ServerTableModel* serverModel,
    const QString& subscriptionId);
QModelIndex visibleProxyIndexForServer(
    const ServerFilterProxyModel* serverFilterModel,
    const ServerTableModel* serverModel,
    const QString& indexId);

} // namespace MainWindowServerSelectionSupport
