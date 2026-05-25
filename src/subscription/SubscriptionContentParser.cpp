#include "subscription/SubscriptionContentParser.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include "common/PortValidator.h"
#include "subscription/ClashSubscriptionParser.h"
#include "subscription/ShareUrlParser.h"
#include "subscription/SubscriptionJsonSupport.h"
#include "subscription/SubscriptionSingBoxParser.h"

using SubscriptionJsonSupport::parsePortValue;

QList<VmessItem> SubscriptionContentParser::parseMany(const QString& content)
{
    return parseManyWithReport(content).items;
}

SubscriptionParseReport SubscriptionContentParser::parseManyWithReport(const QString& content)
{
    SubscriptionParseReport report;
    QString current = content.trimmed();
    report.notes.append(QStringLiteral("Subscription parse: input length=%1").arg(current.size()));

    for (int iteration = 0; iteration < 8 && !current.isEmpty(); ++iteration) {
        const QList<VmessItem> shareItems = ShareUrlParser::parseMany(current);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 share-url count=%2").arg(iteration).arg(shareItems.size()));
        if (!shareItems.isEmpty()) {
            report.items = shareItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=share-url"));
            return report;
        }

        const QList<VmessItem> sipItems = tryParseSip008(current);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 sip008 count=%2").arg(iteration).arg(sipItems.size()));
        if (!sipItems.isEmpty()) {
            report.items = sipItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=sip008"));
            return report;
        }

        const QList<VmessItem> singBoxItems = tryParseSingBox(current);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 sing-box count=%2").arg(iteration).arg(singBoxItems.size()));
        if (!singBoxItems.isEmpty()) {
            report.items = singBoxItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=sing-box"));
            return report;
        }

        const QList<VmessItem> clashItems = tryParseClash(current);
        report.notes.append(QStringLiteral("Subscription parse: pass %1 clash count=%2").arg(iteration).arg(clashItems.size()));
        if (!clashItems.isEmpty()) {
            report.items = clashItems;
            report.notes.append(QStringLiteral("Subscription parse: selected parser=clash"));
            return report;
        }

        const QString decoded = tryDecodeBase64(current).trimmed();
        if (decoded.isEmpty() || decoded == current) {
            report.notes.append(QStringLiteral("Subscription parse: no further base64 decode on pass %1").arg(iteration));
            break;
        }

        report.notes.append(
            QStringLiteral("Subscription parse: base64 decode pass %1 produced length=%2 prefix=%3")
                .arg(iteration)
                .arg(decoded.size())
                .arg(decoded.left(80).replace(QChar('\n'), QChar(' '))));
        current = decoded;
    }

    report.notes.append(QStringLiteral("Subscription parse: no parser matched"));
    return report;
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
        parsePortValue(object.value(QStringLiteral("server_port")), &item.port);
        item.security = object.value(QStringLiteral("method")).toString();
        item.id = object.value(QStringLiteral("password")).toString();

        if (!item.address.isEmpty() && isValidTcpPort(item.port)) {
            items.append(item);
        }
    }

    return items;
}

QList<VmessItem> SubscriptionContentParser::tryParseJsonArray(const QJsonArray& array)
{
    QList<VmessItem> items;

    bool allStrings = !array.isEmpty();
    for (const QJsonValue& value : array) {
        if (!value.isString()) {
            allStrings = false;
            break;
        }
    }

    if (allStrings) {
        QStringList lines;
        lines.reserve(array.size());
        for (const QJsonValue& value : array) {
            const QString text = value.toString().trimmed();
            if (!text.isEmpty()) {
                lines.append(text);
            }
        }
        items = parseMany(lines.join(QChar('\n')));
        if (!items.isEmpty()) {
            return items;
        }
    }

    return tryParseClashProxyArray(array);
}

QList<VmessItem> SubscriptionContentParser::tryParseSingBox(const QString& content)
{
    const QJsonDocument document = QJsonDocument::fromJson(content.trimmed().toUtf8());
    if (document.isArray()) {
        return tryParseJsonArray(document.array());
    }

    if (!document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    if (root.contains(QStringLiteral("proxies")) && root.value(QStringLiteral("proxies")).isArray()) {
        QList<VmessItem> clashItems = tryParseClashProxyArray(root.value(QStringLiteral("proxies")).toArray());
        if (!clashItems.isEmpty()) {
            return clashItems;
        }
    }

    QJsonArray items;
    const QJsonArray outbounds = root.value(QStringLiteral("outbounds")).toArray();
    const QJsonArray endpoints = root.value(QStringLiteral("endpoints")).toArray();
    for (const QJsonValue& value : outbounds) {
        if (value.isObject()) {
            items.append(value.toObject());
        }
    }
    for (const QJsonValue& value : endpoints) {
        if (value.isObject()) {
            items.append(value.toObject());
        }
    }

    QList<VmessItem> parsed;
    for (const QJsonValue& value : items) {
        QList<VmessItem> current = SubscriptionSingBoxParser::parseOutboundObject(value.toObject());
        for (const VmessItem& item : current) {
            parsed.append(item);
        }
    }

    return parsed;
}

QList<VmessItem> SubscriptionContentParser::tryParseClash(const QString& content)
{
    return ClashSubscriptionParser::parseContent(content);
}

QList<VmessItem> SubscriptionContentParser::tryParseClashProxyArray(const QJsonArray& proxies)
{
    return ClashSubscriptionParser::parseProxyArray(proxies);
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
