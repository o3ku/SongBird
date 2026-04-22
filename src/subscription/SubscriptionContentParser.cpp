#include "subscription/SubscriptionContentParser.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "subscription/ShareUrlParser.h"

QList<VmessItem> SubscriptionContentParser::parseMany(const QString& content)
{
    QList<VmessItem> parsed = tryParse(content);
    if (!parsed.isEmpty()) {
        return parsed;
    }

    return tryParse(tryDecodeBase64(content));
}

QList<VmessItem> SubscriptionContentParser::tryParse(const QString& content)
{
    QList<VmessItem> items = ShareUrlParser::parseMany(content);
    if (!items.isEmpty()) {
        return items;
    }

    return tryParseSip008(content);
}

QList<VmessItem> SubscriptionContentParser::tryParseSip008(const QString& content)
{
    QList<VmessItem> items;
    const QJsonDocument document = QJsonDocument::fromJson(content.trimmed().toUtf8());
    if (!document.isObject() && !document.isArray()) {
        return items;
    }

    QJsonArray servers;
    if (document.isArray()) {
        servers = document.array();
    } else {
        servers = document.object().value(QStringLiteral("servers")).toArray();
    }

    for (const QJsonValue& value : servers) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        VmessItem item;
        item.configType = ConfigType::Shadowsocks;
        item.remarks = object.value(QStringLiteral("remarks")).toString();
        item.address = object.value(QStringLiteral("server")).toString();
        item.port = object.value(QStringLiteral("server_port")).toInt(0);
        item.security = object.value(QStringLiteral("method")).toString();
        item.id = object.value(QStringLiteral("password")).toString();

        if (!item.address.isEmpty() && item.port > 0) {
            items.append(item);
        }
    }

    return items;
}

QString SubscriptionContentParser::tryDecodeBase64(const QString& content)
{
    QByteArray normalized = content.trimmed().toUtf8();
    normalized.replace('-', '+');
    normalized.replace('_', '/');
    normalized.replace("\r", "");
    normalized.replace("\n", "");

    const int padding = normalized.size() % 4;
    if (padding > 0) {
        normalized.append(QByteArray(4 - padding, '='));
    }

    return QByteArray::fromBase64(normalized);
}
