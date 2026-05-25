#pragma once

#include <QJsonArray>

#include "domain/models/RoutingRule.h"

namespace SingBoxRoutingRuleMapper {

void appendRoutingRule(QJsonArray& rules, const RoutingRule& source);
void appendIpRule(QJsonArray& rules, const RoutingRule& source);

} // namespace SingBoxRoutingRuleMapper
