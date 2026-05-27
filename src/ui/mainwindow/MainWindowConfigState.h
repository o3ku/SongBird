#pragma once

#include <QList>
#include <QString>

#include "domain/models/Config.h"

struct MainWindowConfigSnapshot {
    QString currentIndexId;
    QList<SubItem> subscriptions;
    QList<RoutingItem> routingItems;
    QString routingModeId;
    QList<CoreTypeItem> coreTypeItems;
};

MainWindowConfigSnapshot makeMainWindowConfigSnapshot(const Config& config);
QString currentCoreDisplayNameFromConfig(const Config& config, const QString& currentIndexId);
QString subscriptionIdFromTabKey(const QString& tabKey);
