#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/RoutingItem.h"

class SingBoxRoutingConfigFragments {
public:
    static QJsonObject buildRoute(const Config& config, const RoutingItem* selectedRouting);
    static void migrateGeoToRuleSet(QJsonObject& root);
};
