#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/RoutingItem.h"

namespace SingBoxDnsConfigBuilder {

QJsonObject build(const Config& config, const RoutingItem* selectedRouting);

} // namespace SingBoxDnsConfigBuilder
