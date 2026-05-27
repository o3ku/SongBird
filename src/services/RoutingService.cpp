#include "services/RoutingService.h"

#include <algorithm>

#include "domain/models/RoutingProfiles.h"

namespace {

bool containsRoutingModeId(const CollectionConfigState& config, const QString& routingModeId)
{
    for (const RoutingItem& item : RoutingProfiles::routingItems(config)) {
        if (item.id == routingModeId) {
            return true;
        }
    }
    return false;
}

} // namespace

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

    QString selectedRoutingModeId;
    if (effectiveIndex >= 0 && effectiveIndex < items.size()) {
        const RoutingItem& selectedItem = items.at(effectiveIndex);
        if (selectedItem.id.startsWith(QStringLiteral("builtin:"))) {
            selectedRoutingModeId = selectedItem.id;
        } else {
            int customIndex = -1;
            for (int index = 0; index <= effectiveIndex; ++index) {
                const RoutingItem& item = items.at(index);
                if (!item.builtin && !item.id.startsWith(QStringLiteral("builtin:"))) {
                    ++customIndex;
                }
            }
            selectedRoutingModeId = RoutingProfiles::customRoutingId(selectedItem.id, customIndex);
        }
    }

    config.dns().domainStrategy = domainStrategy.trimmed();
    config.dns().domainMatcher = domainMatcher.trimmed();
    Q_UNUSED(enableAdvanced);
    config.collection().customRoutingItems = RoutingProfiles::customRoutingItemsFromRuntime(items);
    if (!selectedRoutingModeId.isEmpty()) {
        config.collection().routingModeId = selectedRoutingModeId;
    }
    RoutingProfiles::normalizeRoutingConfig(config.collection());

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save routing settings."));
    }

    return OperationResult::ok(QStringLiteral("Routing settings saved."));
}

OperationResult RoutingService::setRoutingMode(Config& config, const QString& routingModeId)
{
    const QString normalizedId = routingModeId.trimmed();
    if (normalizedId.isEmpty()) {
        return OperationResult::fail(QStringLiteral("No routing entry is available."));
    }

    if (!containsRoutingModeId(config.collection(), normalizedId)) {
        return OperationResult::fail(QStringLiteral("No routing entry is available."));
    }
    config.collection().routingModeId = normalizedId;
    RoutingProfiles::normalizeRoutingConfig(config.collection());

    if (!repository_.save(config)) {
        return OperationResult::fail(QStringLiteral("Failed to save the selected routing mode."));
    }

    return OperationResult::ok(QStringLiteral("Routing mode switched."));
}

OperationResult RoutingService::selectRouting(Config& config, const QString& routingModeId)
{
    return setRoutingMode(config, routingModeId);
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
