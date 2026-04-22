#include "services/RoutingService.h"

#include <algorithm>

RoutingService::RoutingService(IConfigRepository& repository)
    : repository_(repository)
{
}

OperationResult RoutingService::saveRouting(
    Config& config,
    QList<RoutingItem> items,
    bool enableAdvanced,
    int selectedIndex,
    const QString& domainStrategy,
    const QString& domainMatcher)
{
    normalizeRoutingItems(items);

    const int effectiveIndex = resolveSelectedIndex(selectedIndex, items.size());
    for (int index = 0; index < items.size(); ++index) {
        items[index].locked = index == effectiveIndex;
    }

    config.domainStrategy = domainStrategy.trimmed();
    config.domainMatcher = domainMatcher.trimmed();
    config.enableRoutingAdvanced = enableAdvanced;
    config.routingIndex = effectiveIndex < 0 ? 0 : effectiveIndex;
    config.routingItems = std::move(items);

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save routing settings."));
    }

    return OperationResult::ok(QStringLiteral("Routing settings saved."));
}

OperationResult RoutingService::setRoutingMode(Config& config, bool enableAdvanced, int selectedIndex)
{
    if (!enableAdvanced || config.routingItems.isEmpty()) {
        for (RoutingItem& item : config.routingItems) {
            item.locked = false;
        }
        config.enableRoutingAdvanced = false;
        config.routingIndex = 0;

        if (!repository_.save(config)) {
            return OperationResult::fail(QStringLiteral("Failed to save the selected routing mode."));
        }

        return OperationResult::ok(QStringLiteral("Routing mode switched to Basic."));
    }

    const int effectiveIndex = resolveSelectedIndex(selectedIndex, config.routingItems.size());
    if (effectiveIndex < 0) {
        return OperationResult::fail(QStringLiteral("No routing entry is available."));
    }

    for (int index = 0; index < config.routingItems.size(); ++index) {
        config.routingItems[index].locked = index == effectiveIndex;
    }
    config.enableRoutingAdvanced = true;
    config.routingIndex = effectiveIndex;

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save the selected routing mode."));
    }

    return OperationResult::ok(QStringLiteral("Routing mode switched."));
}

OperationResult RoutingService::selectRouting(Config& config, int selectedIndex)
{
    const int effectiveIndex = resolveSelectedIndex(selectedIndex, config.routingItems.size());
    if (effectiveIndex < 0) {
        return OperationResult::fail(QStringLiteral("No routing entry is available."));
    }

    for (int index = 0; index < config.routingItems.size(); ++index) {
        config.routingItems[index].locked = index == effectiveIndex;
    }
    config.routingIndex = effectiveIndex;

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save the selected routing entry."));
    }

    return OperationResult::ok(QStringLiteral("Routing entry switched."));
}

void RoutingService::normalizeRoutingItems(QList<RoutingItem>& items)
{
    for (RoutingItem& item : items) {
        normalizeRoutingItem(item);
    }
}

void RoutingService::normalizeRoutingItem(RoutingItem& item)
{
    item.remarks = item.remarks.trimmed();
    item.url = item.url.trimmed();
    item.customIcon = item.customIcon.trimmed();

    QList<RoutingRule> normalizedRules;
    normalizedRules.reserve(item.rules.size());
    for (RoutingRule rule : item.rules) {
        normalizeRoutingRule(rule);
        if (isMeaningfulRule(rule)) {
            normalizedRules.append(rule);
        }
    }

    item.rules = normalizedRules;
}

void RoutingService::normalizeRoutingRule(RoutingRule& rule)
{
    rule.type = rule.type.trimmed().isEmpty() ? QStringLiteral("field") : rule.type.trimmed();
    rule.port = rule.port.trimmed();
    rule.network = rule.network.trimmed();
    rule.outboundTag = rule.outboundTag.trimmed();
    rule.inboundTag = normalizeValues(rule.inboundTag);
    rule.ip = normalizeValues(rule.ip);
    rule.domain = normalizeValues(rule.domain);
    rule.protocol = normalizeValues(rule.protocol);
    rule.process = normalizeValues(rule.process);
}

QStringList RoutingService::normalizeValues(const QStringList& values)
{
    QStringList normalized;
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            normalized.append(trimmed);
        }
    }

    normalized.removeDuplicates();
    return normalized;
}

bool RoutingService::isMeaningfulRule(const RoutingRule& rule)
{
    return !rule.outboundTag.isEmpty()
        || !rule.port.isEmpty()
        || !rule.network.isEmpty()
        || !rule.inboundTag.isEmpty()
        || !rule.ip.isEmpty()
        || !rule.domain.isEmpty()
        || !rule.protocol.isEmpty()
        || !rule.process.isEmpty();
}

int RoutingService::resolveSelectedIndex(int selectedIndex, int itemCount)
{
    if (itemCount <= 0) {
        return -1;
    }

    if (selectedIndex < 0 || selectedIndex >= itemCount) {
        return 0;
    }

    return selectedIndex;
}
