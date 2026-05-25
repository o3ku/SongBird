#pragma once

#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>

#include "domain/models/RoutingRule.h"

namespace RoutingCustomRuleSupport {

struct RuleValues {
    QStringList protocols;
    QStringList ports;
    QStringList ips;
    QStringList domains;
};

struct PartitionedRules {
    QMap<QString, RuleValues> valuesByAction;
    QList<RoutingRule> preservedRules;
};

QList<QPair<QString, QString>> customRuleTabs();
QStringList actionOrder();
QString normalizedTabKey(QString key);
QString joinValues(const QStringList& values);
QStringList splitValues(const QString& value);
PartitionedRules partitionEditableRules(
    const QList<RoutingRule>& rules,
    const QStringList& supportedActions);
QList<RoutingRule> collectRules(
    const QList<RoutingRule>& preservedRules,
    const QMap<QString, RuleValues>& valuesByAction);

} // namespace RoutingCustomRuleSupport
