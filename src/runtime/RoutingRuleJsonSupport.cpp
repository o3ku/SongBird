#include "runtime/RoutingRuleJsonSupport.h"

#include <QJsonArray>

namespace RoutingRuleJsonSupport {

QStringList splitCsv(const QString& value)
{
    QStringList items;
    const QStringList rawItems = value.split(',', Qt::SkipEmptyParts);
    for (const QString& rawItem : rawItems) {
        const QString item = rawItem.trimmed();
        if (!item.isEmpty()) {
            items.append(item);
        }
    }

    return items;
}

QStringList normalizeRuleValues(
    const QStringList& values,
    bool replaceCommaPlaceholder,
    bool removeCommentEntries)
{
    QStringList result;
    for (const QString& value : values) {
        QString normalized = value.trimmed();
        if (normalized.isEmpty()) {
            continue;
        }

        if (removeCommentEntries && normalized.startsWith(QChar('#'))) {
            continue;
        }

        if (replaceCommaPlaceholder) {
            normalized.replace(QStringLiteral("<COMMA>"), QStringLiteral(","));
        }

        result.append(normalized);
    }

    return result;
}

void insertStringArray(QJsonObject& object, const QString& key, const QStringList& values)
{
    if (values.isEmpty()) {
        return;
    }

    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }
    object.insert(key, array);
}

} // namespace RoutingRuleJsonSupport
