#pragma once

#include <QList>
#include <QSet>
#include <QString>

#include "domain/models/SubItem.h"

namespace SubscriptionViewSupport {

QSet<QString> enabledSubscriptionIds(const QList<SubItem>& subscriptions);
QString tabKeyFromSelectionId(const QString& selectionId);
QString persistedSelectionIdFromTabKey(const QString& tabKey);
QString subscriptionIdFromTabKey(const QString& tabKey);
QString subscriptionDisplayName(const SubItem& item);

} // namespace SubscriptionViewSupport
