#include "persistence/JsonConfigRoutingSerialization.h"

#include <QJsonArray>
#include <QJsonValue>

#include <utility>

#include "persistence/JsonConfigUtils.h"

namespace {

using namespace JsonConfigUtils;

const QString kBuiltinRoutingWhitelistName = QStringLiteral("\u7ed5\u8fc7\u5927\u9646(Whitelist)");
const QString kBuiltinRoutingBlacklistName = QStringLiteral("\u9ed1\u540d\u5355(Blacklist)");
const QString kBuiltinRoutingGlobalName = QStringLiteral("\u5168\u5c40(Global)");

RoutingRule createRoutingRule(
    QString outboundTag,
    QStringList domain = {},
    QStringList ip = {},
    QStringList protocol = {},
    QString port = {})
{
    RoutingRule rule;
    rule.type = QStringLiteral("field");
    rule.port = std::move(port);
    rule.outboundTag = std::move(outboundTag);
    rule.ip = std::move(ip);
    rule.domain = std::move(domain);
    rule.protocol = std::move(protocol);
    rule.enabled = true;
    return rule;
}

RoutingItem createBuiltinRoutingItem(QString remarks, QList<RoutingRule> rules)
{
    RoutingItem item;
    item.remarks = std::move(remarks);
    item.rules = std::move(rules);
    item.enabled = true;
    item.locked = false;
    return item;
}

void ensureBuiltinRoutingItems(CollectionConfigState& config)
{
    for (const RoutingItem& item : config.routingItems) {
        if (!item.locked) {
            if (config.enableRoutingAdvanced
                && (config.routingIndex < 0 || config.routingIndex >= config.routingItems.size())) {
                config.routingIndex = 0;
            }
            return;
        }
    }

    config.routingItems.append(createBuiltinRoutingItem(
        kBuiltinRoutingWhitelistName,
        QList<RoutingRule>{
            createRoutingRule(
                QStringLiteral("direct"),
                QStringList{
                    QStringLiteral("domain:example-example.com"),
                    QStringLiteral("domain:example-example2.com")}),
            createRoutingRule(
                QStringLiteral("block"),
                QStringList{QStringLiteral("geosite:category-ads-all")}),
            createRoutingRule(
                QStringLiteral("direct"),
                QStringList{QStringLiteral("geosite:cn")}),
            createRoutingRule(
                QStringLiteral("direct"),
                {},
                QStringList{
                    QStringLiteral("geoip:private"),
                    QStringLiteral("geoip:cn")}),
            createRoutingRule(
                QStringLiteral("proxy"),
                {},
                {},
                {},
                QStringLiteral("0-65535"))}));

    config.routingItems.append(createBuiltinRoutingItem(
        kBuiltinRoutingBlacklistName,
        QList<RoutingRule>{
            createRoutingRule(
                QStringLiteral("direct"),
                {},
                {},
                QStringList{QStringLiteral("bittorrent")}),
            createRoutingRule(
                QStringLiteral("block"),
                QStringList{QStringLiteral("geosite:category-ads-all")}),
            createRoutingRule(
                QStringLiteral("proxy"),
                QStringList{
                    QStringLiteral("geosite:gfw"),
                    QStringLiteral("geosite:greatfire"),
                    QStringLiteral("geosite:tld-!cn")},
                QStringList{QStringLiteral("geoip:telegram")}),
            createRoutingRule(
                QStringLiteral("direct"),
                {},
                {},
                {},
                QStringLiteral("0-65535"))}));

    config.routingItems.append(createBuiltinRoutingItem(
        kBuiltinRoutingGlobalName,
        QList<RoutingRule>{
            createRoutingRule(
                QStringLiteral("proxy"),
                {},
                {},
                {},
                QStringLiteral("0-65535"))}));

    if (config.enableRoutingAdvanced
        && (config.routingIndex < 0 || config.routingIndex >= config.routingItems.size())) {
        config.routingIndex = 0;
    }
}

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

QList<RoutingItem> parseRoutingItems(const QJsonArray& array)
{
    QList<RoutingItem> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        RoutingItem item;
        item.remarks = readString(object, QStringLiteral("remarks"));
        item.url = readString(object, QStringLiteral("url"));
        item.enabled = readBool(object, QStringLiteral("enabled"), true);
        item.locked = readBool(object, QStringLiteral("locked"));
        item.customIcon = readString(object, QStringLiteral("customIcon"));
        item.domainStrategy4Singbox = readString(object, QStringLiteral("domainStrategyForSingbox"));
        item.rules = parseRoutingRules(object.value(QStringLiteral("rules")).toArray());
        items.append(item);
    }

    return items;
}

QJsonArray toRoutingArray(const QList<RoutingItem>& items)
{
    QJsonArray array;
    for (const RoutingItem& item : items) {
        QJsonObject object;
        writeIfNotEmpty(object, QStringLiteral("remarks"), item.remarks);
        writeIfNotEmpty(object, QStringLiteral("url"), item.url);
        writeIfNotDefault(object, QStringLiteral("enabled"), item.enabled, true);
        writeIfTrue(object, QStringLiteral("locked"), item.locked);
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
    config.routingIndex = readInt(root, QStringLiteral("routingIndex"));
    config.enableRoutingAdvanced = readBool(root, QStringLiteral("enableRoutingAdvanced"));
    config.routingItems = parseRoutingItems(root.value(QStringLiteral("routingItems")).toArray());
    config.routingCustomRules = parseRoutingRules(root.value(QStringLiteral("routingCustomRules")).toArray());
    ensureBuiltinRoutingItems(config);
}

void writeRouting(QJsonObject& root, const CollectionConfigState& config)
{
    writeIfNotDefault(root, QStringLiteral("routingIndex"), config.routingIndex, 0);
    writeIfTrue(root, QStringLiteral("enableRoutingAdvanced"), config.enableRoutingAdvanced);

    const QJsonArray routingItems = toRoutingArray(config.routingItems);
    writeArrayIfNotEmpty(root, QStringLiteral("routingItems"), routingItems);

    const QJsonArray routingCustomRules = toRoutingRuleArray(config.routingCustomRules);
    writeArrayIfNotEmpty(root, QStringLiteral("routingCustomRules"), routingCustomRules);
}

} // namespace JsonConfigRoutingSerialization
