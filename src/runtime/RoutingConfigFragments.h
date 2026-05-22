#pragma once

#include <QJsonObject>
#include <QString>

#include "domain/models/Config.h"
#include "domain/models/RoutingItem.h"

class RoutingConfigFragments {
public:
    static QString locationProbeTag();
    static int locationProbePortOffset();

    static const RoutingItem* resolveSelectedRouting(const Config& config);
    static QJsonObject buildLegacyRouting(const Config& config, const RoutingItem* selectedRouting);
    static QJsonObject buildSingBoxRoute(const Config& config, const RoutingItem* selectedRouting);
    static void migrateGeoToRuleSet(QJsonObject& root);
};
