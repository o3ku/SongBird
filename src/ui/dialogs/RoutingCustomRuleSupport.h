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
    QStringList processes;
};

// A rule the editor cannot express, kept verbatim along with the position it held in the list it
// was read from. The position is what lets collectRules put it back where it was: route rules are
// evaluated in order, so re-emitting it somewhere else changes which rule wins.
struct PreservedRule {
    int originalIndex = 0;
    RoutingRule rule;
};

struct PartitionedRules {
    QMap<QString, RuleValues> valuesByAction;
    QList<PreservedRule> preservedRules;
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
    const QList<PreservedRule>& preservedRules,
    const QMap<QString, RuleValues>& valuesByAction);

} // namespace RoutingCustomRuleSupport
