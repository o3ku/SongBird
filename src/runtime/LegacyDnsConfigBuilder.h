#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/RoutingItem.h"

namespace LegacyDnsConfigBuilder {

QJsonObject build(const Config& config, const RoutingItem* selectedRouting);

} // namespace LegacyDnsConfigBuilder
