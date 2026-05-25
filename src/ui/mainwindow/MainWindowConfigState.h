#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include "domain/models/Config.h"

struct MainWindowConfigSnapshot {
    QString currentIndexId;
    QList<SubItem> subscriptions;
    QList<RoutingItem> routingItems;
    int routingIndex = 0;
    QList<CoreTypeItem> coreTypeItems;
};

MainWindowConfigSnapshot makeMainWindowConfigSnapshot(const Config& config);
QHash<QString, QString> buildMainWindowShareUrlCache(const QList<VmessItem>& servers);
QString currentCoreDisplayNameFromConfig(const Config& config, const QString& currentIndexId);
QString subscriptionIdFromTabKey(const QString& tabKey);
