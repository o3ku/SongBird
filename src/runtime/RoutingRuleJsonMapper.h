#pragma once

#include <QJsonArray>
#include <QStringList>

#include "domain/models/RoutingRule.h"

class RoutingRuleJsonMapper {
public:
    static void appendLegacyRoutingRule(QJsonArray& rules, const RoutingRule& source);
    static void appendSingBoxRoutingRule(QJsonArray& rules, const RoutingRule& source);
    static void appendSingBoxRoutingIpRule(QJsonArray& rules, const RoutingRule& source);
    static QStringList normalizeRuleValues(
        const QStringList& values,
        bool replaceCommaPlaceholder = false,
        bool removeCommentEntries = false);
};
