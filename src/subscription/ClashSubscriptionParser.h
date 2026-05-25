#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>

#include "domain/models/VmessItem.h"

namespace ClashSubscriptionParser {

QList<VmessItem> parseContent(const QString& content);
QList<VmessItem> parseProxyArray(const QJsonArray& proxies);

} // namespace ClashSubscriptionParser
