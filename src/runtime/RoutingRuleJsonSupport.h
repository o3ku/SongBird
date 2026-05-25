#pragma once

#include <QJsonObject>
#include <QStringList>

namespace RoutingRuleJsonSupport {

QStringList splitCsv(const QString& value);
QStringList normalizeRuleValues(
    const QStringList& values,
    bool replaceCommaPlaceholder = false,
    bool removeCommentEntries = false);
void insertStringArray(QJsonObject& object, const QString& key, const QStringList& values);

} // namespace RoutingRuleJsonSupport
