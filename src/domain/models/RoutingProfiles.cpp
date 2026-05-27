#include "domain/models/RoutingProfiles.h"

#include <QCoreApplication>

#include <utility>

namespace {

const QString kBuiltinRoutingWhitelistId = QStringLiteral("builtin:whitelist");
const QString kBuiltinRoutingBlacklistId = QStringLiteral("builtin:blacklist");
const QString kBuiltinRoutingGlobalId = QStringLiteral("builtin:global");

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

RoutingItem createBuiltinRoutingItem(QString id, const char* sourceName, QList<RoutingRule> rules)
{
    RoutingItem item;
    item.id = std::move(id);
    item.remarks = QCoreApplication::translate("RoutingProfiles", sourceName);
    item.rules = std::move(rules);
    item.enabled = true;
    item.locked = false;
    item.builtin = true;
    return item;
}

} // namespace

QString RoutingProfiles::defaultRoutingModeId()
{
    return kBuiltinRoutingWhitelistId;
}

QString RoutingProfiles::customRoutingId(QString value, int index)
{
    value = value.trimmed();
    if (value.startsWith(QStringLiteral("custom:"))) {
        value = value.mid(QStringLiteral("custom:").size()).trimmed();
    }
    return value.isEmpty()
        ? QStringLiteral("custom:%1").arg(index + 1)
        : QStringLiteral("custom:%1").arg(value);
}

QList<RoutingItem> RoutingProfiles::builtinRoutingItems()
{
    QList<RoutingItem> items;
    items.reserve(3);

    items.append(createBuiltinRoutingItem(
        kBuiltinRoutingWhitelistId,
        QT_TRANSLATE_NOOP("RoutingProfiles", "Bypass CN"),
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

    items.append(createBuiltinRoutingItem(
        kBuiltinRoutingBlacklistId,
        QT_TRANSLATE_NOOP("RoutingProfiles", "Blacklist"),
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

    items.append(createBuiltinRoutingItem(
        kBuiltinRoutingGlobalId,
        QT_TRANSLATE_NOOP("RoutingProfiles", "Global"),
        QList<RoutingRule>{
            createRoutingRule(
                QStringLiteral("proxy"),
                {},
                {},
                {},
                QStringLiteral("0-65535"))}));

    return items;
}

QList<RoutingItem> RoutingProfiles::customRoutingItemsFromRuntime(const QList<RoutingItem>& items)
{
    QList<RoutingItem> customItems;
    for (RoutingItem item : items) {
        if (item.builtin || item.id.startsWith(QStringLiteral("builtin:"))) {
            continue;
        }
        item.builtin = false;
        item.locked = false;
        customItems.append(item);
    }
    normalizeCustomRoutingItems(customItems);
    return customItems;
}

QList<RoutingItem> RoutingProfiles::routingItems(const CollectionConfigState& config)
{
    QList<RoutingItem> items = builtinRoutingItems();
    QList<RoutingItem> customItems = config.customRoutingItems;
    normalizeCustomRoutingItems(customItems);
    for (const RoutingItem& item : std::as_const(customItems)) {
        items.append(item);
    }

    const QString routingModeId = config.routingModeId.trimmed().isEmpty()
        ? defaultRoutingModeId()
        : config.routingModeId.trimmed();
    int selectedIndex = -1;
    for (int index = 0; index < items.size(); ++index) {
        RoutingItem& item = items[index];
        item.locked = item.id == routingModeId;
        if (item.locked) {
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0 && !items.isEmpty()) {
        items[0].locked = true;
    }

    return items;
}

void RoutingProfiles::normalizeCustomRoutingItems(QList<RoutingItem>& items)
{
    for (int index = 0; index < items.size(); ++index) {
        RoutingItem& item = items[index];
        item.id = customRoutingId(item.id, index);
        item.builtin = false;
        item.enabled = true;
        item.locked = false;
    }
}

void RoutingProfiles::normalizeRoutingConfig(CollectionConfigState& config)
{
    normalizeCustomRoutingItems(config.customRoutingItems);
    if (config.routingModeId.trimmed().isEmpty()) {
        config.routingModeId = defaultRoutingModeId();
    }

    for (const RoutingItem& item : routingItems(config)) {
        if (item.id == config.routingModeId) {
            return;
        }
    }
    config.routingModeId = defaultRoutingModeId();
}

int RoutingProfiles::selectedRoutingIndex(const CollectionConfigState& config)
{
    const QList<RoutingItem> items = routingItems(config);
    for (int index = 0; index < items.size(); ++index) {
        if (items.at(index).id == config.routingModeId || items.at(index).locked) {
            return index;
        }
    }
    return items.isEmpty() ? -1 : 0;
}

std::optional<RoutingItem> RoutingProfiles::selectedRouting(const Config& config)
{
    const QList<RoutingItem> items = routingItems(config.collection());
    const int index = selectedRoutingIndex(config.collection());
    if (index < 0 || index >= items.size()) {
        return std::nullopt;
    }
    return items.at(index);
}

QString RoutingProfiles::selectedRoutingName(const Config& config)
{
    const std::optional<RoutingItem> routing = selectedRouting(config);
    return routing.has_value() ? routing->remarks.trimmed() : QString();
}
