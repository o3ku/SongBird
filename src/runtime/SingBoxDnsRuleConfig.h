#pragma once

#include <QJsonArray>
#include <QMap>
#include <QStringList>

#include "domain/models/Config.h"
#include "domain/models/RoutingItem.h"

namespace SingBoxDnsRuleConfig {

void appendModeRules(
    QJsonArray& rules,
    const Config& config,
    bool hasPredefinedHosts,
    bool hasRemoteDnsServer,
    bool hasDirectDnsServer);
void appendHostRules(QJsonArray& rules, const QMap<QString, QStringList>& hostsMap);
void appendBlockBindingRule(QJsonArray& rules);
void appendGlobalFakeIpRule(QJsonArray& rules);
void appendRoutingRules(QJsonArray& rules, const Config& config, const RoutingItem* selectedRouting);
void appendScopedFakeIpFallbackRule(QJsonArray& rules);

} // namespace SingBoxDnsRuleConfig
