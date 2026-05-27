#pragma once

#include <QList>
#include <QJsonObject>
#include <QString>

#include <optional>

#include "domain/models/Config.h"
#include "domain/models/RoutingItem.h"
#include "domain/models/RoutingRule.h"

class RoutingConfigFragments {
public:
    static QString locationProbeTag();
    static int locationProbePortOffset();

    static std::optional<RoutingItem> resolveSelectedRouting(const Config& config);
    static QList<RoutingRule> effectiveRoutingRules(const Config& config, const RoutingItem* selectedRouting);
    static QJsonObject buildLegacyRouting(const Config& config, const RoutingItem* selectedRouting);
};
