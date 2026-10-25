#pragma once

#include <QDateTime>
#include <QFontMetrics>
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
QString subscriptionTabLabel(const SubItem& item, const QFontMetrics& metrics);
QString subscriptionDetails(const SubItem& item, const QDateTime& now = QDateTime::currentDateTime());
QString subscriptionToolTip(const SubItem& item);
bool quotaWarning(const SubItem& item);

} // namespace SubscriptionViewSupport
