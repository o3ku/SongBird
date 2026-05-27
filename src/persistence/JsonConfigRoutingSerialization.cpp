#include "persistence/JsonConfigRoutingSerialization.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "domain/models/RoutingProfiles.h"
#include "persistence/JsonConfigUtils.h"

namespace {

using namespace JsonConfigUtils;

QList<RoutingRule> parseRoutingRules(const QJsonArray& array)
{
    QList<RoutingRule> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        RoutingRule item;
        item.type = readString(object, QStringLiteral("type"));
        item.port = readString(object, QStringLiteral("port"));
        item.network = readString(object, QStringLiteral("network"));
        item.outboundTag = readString(object, QStringLiteral("outboundTag"));
        item.enabled = readBool(object, QStringLiteral("enabled"), true);
        item.inboundTag = readStringList(object, QStringLiteral("inboundTag"));
        item.ip = readStringList(object, QStringLiteral("ip"));
        item.domain = readStringList(object, QStringLiteral("domain"));
        item.protocol = readStringList(object, QStringLiteral("protocol"));
        item.process = readStringList(object, QStringLiteral("process"));
        items.append(item);
    }

    return items;
}

QJsonArray toRoutingRuleArray(const QList<RoutingRule>& items)
{
    QJsonArray array;
    for (const RoutingRule& item : items) {
        QJsonObject object;
        writeIfNotEmpty(object, QStringLiteral("type"), item.type);
        writeIfNotEmpty(object, QStringLiteral("port"), item.port);
        writeIfNotEmpty(object, QStringLiteral("network"), item.network);
        writeIfNotEmpty(object, QStringLiteral("outboundTag"), item.outboundTag);
        writeIfNotDefault(object, QStringLiteral("enabled"), item.enabled, true);
        writeStringListIfNotEmpty(object, QStringLiteral("inboundTag"), item.inboundTag);
        writeStringListIfNotEmpty(object, QStringLiteral("ip"), item.ip);
        writeStringListIfNotEmpty(object, QStringLiteral("domain"), item.domain);
        writeStringListIfNotEmpty(object, QStringLiteral("protocol"), item.protocol);
        writeStringListIfNotEmpty(object, QStringLiteral("process"), item.process);
        array.append(object);
    }

    return array;
}

QList<RoutingItem> parseCustomRoutingItems(const QJsonArray& array)
{
    QList<RoutingItem> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        RoutingItem item;
        item.id = RoutingProfiles::customRoutingId(readString(object, QStringLiteral("id")), items.size());
        item.remarks = readString(object, QStringLiteral("name"));
        item.url = readString(object, QStringLiteral("url"));
        item.enabled = readBool(object, QStringLiteral("enabled"), true);
        item.locked = false;
        item.builtin = false;
        item.customIcon = readString(object, QStringLiteral("customIcon"));
        item.domainStrategy4Singbox = readString(object, QStringLiteral("domainStrategyForSingbox"));
        item.rules = parseRoutingRules(object.value(QStringLiteral("rules")).toArray());
        items.append(item);
    }

    return items;
}

QJsonArray toCustomRoutingArray(const QList<RoutingItem>& items)
{
    QJsonArray array;
    for (const RoutingItem& item : items) {
        QJsonObject object;
        const QString id = item.id.startsWith(QStringLiteral("custom:"))
            ? item.id.mid(QStringLiteral("custom:").size())
            : item.id;
        writeIfNotEmpty(object, QStringLiteral("id"), id);
        writeIfNotEmpty(object, QStringLiteral("name"), item.remarks);
        writeIfNotEmpty(object, QStringLiteral("url"), item.url);
        writeIfNotDefault(object, QStringLiteral("enabled"), item.enabled, true);
        writeIfNotEmpty(object, QStringLiteral("customIcon"), item.customIcon);
        writeIfNotEmpty(object, QStringLiteral("domainStrategyForSingbox"), item.domainStrategy4Singbox);
        const QJsonArray rules = toRoutingRuleArray(item.rules);
        writeArrayIfNotEmpty(object, QStringLiteral("rules"), rules);
        array.append(object);
    }

    return array;
}

} // namespace

namespace JsonConfigRoutingSerialization {

void readRouting(const QJsonObject& root, CollectionConfigState& config)
{
    config.routingModeId = readString(root, QStringLiteral("routingModeId"), RoutingProfiles::defaultRoutingModeId());
    config.customRoutingItems = parseCustomRoutingItems(root.value(QStringLiteral("customRoutingItems")).toArray());
    config.routingCustomRules = parseRoutingRules(root.value(QStringLiteral("routingCustomRules")).toArray());
    RoutingProfiles::normalizeRoutingConfig(config);
}

void writeRouting(QJsonObject& root, const CollectionConfigState& config)
{
    writeIfNotDefault(root, QStringLiteral("routingModeId"), config.routingModeId, RoutingProfiles::defaultRoutingModeId());

    const QJsonArray customRoutingItems = toCustomRoutingArray(config.customRoutingItems);
    writeArrayIfNotEmpty(root, QStringLiteral("customRoutingItems"), customRoutingItems);

    const QJsonArray routingCustomRules = toRoutingRuleArray(config.routingCustomRules);
    writeArrayIfNotEmpty(root, QStringLiteral("routingCustomRules"), routingCustomRules);
}

} // namespace JsonConfigRoutingSerialization
