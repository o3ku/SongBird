#pragma once

#include <QList>
#include <QString>

#include <optional>

#include "domain/models/Config.h"
#include "domain/models/RoutingItem.h"

namespace RoutingProfiles {

QString defaultRoutingModeId();
QString customRoutingId(QString value, int index);
QList<RoutingItem> builtinRoutingItems();
QList<RoutingItem> routingItems(const CollectionConfigState& config);
QList<RoutingItem> customRoutingItemsFromRuntime(const QList<RoutingItem>& items);
void normalizeCustomRoutingItems(QList<RoutingItem>& items);
void normalizeRoutingConfig(CollectionConfigState& config);
int selectedRoutingIndex(const CollectionConfigState& config);
std::optional<RoutingItem> selectedRouting(const Config& config);
QString selectedRoutingName(const Config& config);

} // namespace RoutingProfiles
