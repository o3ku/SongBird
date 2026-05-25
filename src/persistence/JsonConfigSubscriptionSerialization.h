#pragma once

#include <QJsonArray>

#include "domain/models/Config.h"

namespace JsonConfigSubscriptionSerialization {

QList<SubItem> readSubscriptions(const QJsonArray& array);
QJsonArray writeSubscriptions(const QList<SubItem>& items);

} // namespace JsonConfigSubscriptionSerialization
