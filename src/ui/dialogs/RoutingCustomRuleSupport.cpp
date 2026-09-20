#include "ui/dialogs/RoutingCustomRuleSupport.h"

#include <QRegularExpression>

namespace {

bool containsNonEmptyValue(const QStringList& values)
{
    for (const QString& value : values) {
        if (!value.trimmed().isEmpty()) {
            return true;
        }
    }
    return false;
}

bool hasEditableCustomRoutingFields(const RoutingRule& rule)
{
    return !rule.protocol.isEmpty()
        || !rule.port.trimmed().isEmpty()
        || !rule.ip.isEmpty()
        || !rule.domain.isEmpty()
        || !rule.process.isEmpty();
}

// The editor is a flat grid -- one list per (action, field kind) -- so it cannot express a
// rule that combines kinds. Accepting one would let collectRules split it apart, turning
// `{domain: X, process: Y}` (every condition must match) into `{domain: X}` OR
// `{process: Y}` and silently widening what gets routed. Rules that span kinds are
// therefore preserved verbatim, the same treatment `network`/`inboundTag` already get.
//
// protocol and port count as a single group because collectRules emits them as one rule.
int editableFieldGroupCount(const RoutingRule& rule)
{
    int groups = 0;
    if (!rule.protocol.isEmpty() || !rule.port.trimmed().isEmpty()) {
        ++groups;
    }
    if (!rule.ip.isEmpty()) {
        ++groups;
    }
    if (!rule.domain.isEmpty()) {
        ++groups;
    }
    if (!rule.process.isEmpty()) {
        ++groups;
    }
    return groups;
}

bool isEditableCustomRoutingRule(const RoutingRule& rule, bool supportedAction)
{
    if (!supportedAction || !rule.enabled) {
        return false;
    }

    const QString type = rule.type.trimmed();
    if (!type.isEmpty() && type.compare(QStringLiteral("field"), Qt::CaseInsensitive) != 0) {
        return false;
    }

    // `network` and `inboundTag` have no editor field, so any rule carrying them is
    // preserved verbatim instead. `process` does have one now, so it is editable -- provided
    // it is the rule's only field kind, so the round-trip cannot change its meaning.
    return hasEditableCustomRoutingFields(rule)
        && editableFieldGroupCount(rule) == 1
        && rule.network.trimmed().isEmpty()
        && !containsNonEmptyValue(rule.inboundTag);
}

void appendUnique(QStringList& target, const QStringList& values)
{
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !target.contains(trimmed)) {
            target.append(trimmed);
        }
    }
}

RoutingRule makeRule(const QString& action)
{
    RoutingRule rule;
    rule.type = QStringLiteral("field");
    rule.enabled = true;
    rule.outboundTag = action;
    return rule;
}

} // namespace

QList<QPair<QString, QString>> RoutingCustomRuleSupport::customRuleTabs()
{
    return {
        {QStringLiteral("block"), QStringLiteral("BLOCK")},
        {QStringLiteral("direct"), QStringLiteral("DIRECT")},
        {QStringLiteral("proxy"), QStringLiteral("PROXY")}};
}

QStringList RoutingCustomRuleSupport::actionOrder()
{
    return {
        QStringLiteral("block"),
        QStringLiteral("direct"),
        QStringLiteral("proxy")};
}

QString RoutingCustomRuleSupport::normalizedTabKey(QString key)
{
    key = key.trimmed().toLower();
    return key == QStringLiteral("block") || key == QStringLiteral("proxy")
        ? key
        : QStringLiteral("direct");
}

QString RoutingCustomRuleSupport::joinValues(const QStringList& values)
{
    QStringList normalized;
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            normalized.append(trimmed);
        }
    }
    return normalized.join(QStringLiteral(", "));
}

QStringList RoutingCustomRuleSupport::splitValues(const QString& value)
{
    QStringList values;
    const QStringList parts = value.split(QRegularExpression(QStringLiteral("[,\\n]")), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            values.append(trimmed);
        }
    }
    return values;
}

RoutingCustomRuleSupport::PartitionedRules RoutingCustomRuleSupport::partitionEditableRules(
    const QList<RoutingRule>& rules,
    const QStringList& supportedActions)
{
    PartitionedRules result;
    for (const RoutingRule& rule : rules) {
        const QString action = rule.outboundTag.trimmed().toLower();
        if (!isEditableCustomRoutingRule(rule, supportedActions.contains(action))) {
            result.preservedRules.append(rule);
            continue;
        }

        // Merging by action is only safe because every rule that reaches here is confined to
        // a single field group: within a group the values are OR'd either way, and
        // collectRules re-emits one rule per group, so the output matches the input.
        RuleValues& values = result.valuesByAction[action];
        appendUnique(values.protocols, rule.protocol);
        appendUnique(values.ips, rule.ip);
        appendUnique(values.domains, rule.domain);
        appendUnique(values.processes, rule.process);
        const QString port = rule.port.trimmed();
        if (!port.isEmpty() && !values.ports.contains(port)) {
            values.ports.append(port);
        }
    }
    return result;
}

QList<RoutingRule> RoutingCustomRuleSupport::collectRules(
    const QList<RoutingRule>& preservedRules,
    const QMap<QString, RuleValues>& valuesByAction)
{
    QList<RoutingRule> rules = preservedRules;
    for (const QString& action : actionOrder()) {
        const RuleValues values = valuesByAction.value(action);
        const QString portText = joinValues(values.ports);
        if (!values.protocols.isEmpty() || !portText.isEmpty()) {
            RoutingRule rule = makeRule(action);
            rule.protocol = values.protocols;
            rule.port = portText;
            rules.append(rule);
        }

        if (!values.ips.isEmpty()) {
            RoutingRule rule = makeRule(action);
            rule.ip = values.ips;
            rules.append(rule);
        }

        if (!values.domains.isEmpty()) {
            RoutingRule rule = makeRule(action);
            rule.domain = values.domains;
            rules.append(rule);
        }

        if (!values.processes.isEmpty()) {
            RoutingRule rule = makeRule(action);
            rule.process = values.processes;
            rules.append(rule);
        }
    }
    return rules;
}
