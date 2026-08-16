#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

namespace ClashSubscriptionParser {

QList<VmessItem> parseContent(const QString& content, QStringList* skippedTypes = nullptr);
QList<VmessItem> parseProxyArray(const QJsonArray& proxies, QStringList* skippedTypes = nullptr);

} // namespace ClashSubscriptionParser
