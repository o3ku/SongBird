#include "persistence/JsonConfigSubscriptionSerialization.h"

#include <QJsonValue>

#include "persistence/JsonConfigUtils.h"

namespace JsonConfigSubscriptionSerialization {

QList<SubItem> readSubscriptions(const QJsonArray& array)
{
    QList<SubItem> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        SubItem item;
        item.id = JsonConfigUtils::readString(object, QStringLiteral("id"));
        item.remarks = JsonConfigUtils::readString(object, QStringLiteral("remarks"));
        item.url = JsonConfigUtils::readString(object, QStringLiteral("url"));
        item.enabled = JsonConfigUtils::readBool(object, QStringLiteral("enabled"), true);
        item.userAgent = JsonConfigUtils::readString(object, QStringLiteral("userAgent"));
        item.etag = JsonConfigUtils::readString(object, QStringLiteral("etag"));
        item.lastModified = JsonConfigUtils::readString(object, QStringLiteral("lastModified"));
        item.failStreak = JsonConfigUtils::readInt(object, QStringLiteral("failStreak"), 0);
        item.lastUpdatedAtMs = JsonConfigUtils::readInt64(object, QStringLiteral("lastUpdatedAtMs"), 0);
        items.append(item);
    }

    return items;
}

QJsonArray writeSubscriptions(const QList<SubItem>& items)
{
    QJsonArray array;
    for (const SubItem& item : items) {
        QJsonObject object;
        JsonConfigUtils::writeIfNotEmpty(object, QStringLiteral("id"), item.id);
        JsonConfigUtils::writeIfNotEmpty(object, QStringLiteral("remarks"), item.remarks);
        JsonConfigUtils::writeIfNotEmpty(object, QStringLiteral("url"), item.url);
        JsonConfigUtils::writeIfNotDefault(object, QStringLiteral("enabled"), item.enabled, true);
        JsonConfigUtils::writeIfNotEmpty(object, QStringLiteral("userAgent"), item.userAgent);
        JsonConfigUtils::writeIfNotEmpty(object, QStringLiteral("etag"), item.etag);
        JsonConfigUtils::writeIfNotEmpty(object, QStringLiteral("lastModified"), item.lastModified);
        JsonConfigUtils::writeIfNotDefault(object, QStringLiteral("failStreak"), item.failStreak, 0);
        JsonConfigUtils::writeIfNotDefault(object, QStringLiteral("lastUpdatedAtMs"), item.lastUpdatedAtMs, qint64(0));
        array.append(object);
    }

    return array;
}

} // namespace JsonConfigSubscriptionSerialization
