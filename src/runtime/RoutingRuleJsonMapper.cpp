#include "runtime/RoutingRuleJsonMapper.h"

#include <QJsonObject>

#include "runtime/RoutingRuleJsonSupport.h"
#include "runtime/SingBoxRoutingRuleMapper.h"

namespace {

QJsonObject createLegacyRoutingRule(
    const RoutingRule& source,
    const QStringList& inboundTags,
    const QString& port,
    const QStringList& domains,
    const QStringList& ips,
    const QStringList& protocols,
    const QStringList& processes)
{
    QJsonObject rule;
    rule.insert(QStringLiteral("type"), QStringLiteral("field"));

    RoutingRuleJsonSupport::insertStringArray(rule, QStringLiteral("inboundTag"), inboundTags);

    if (!source.outboundTag.trimmed().isEmpty()) {
        rule.insert(QStringLiteral("outboundTag"), source.outboundTag.trimmed());
    }

    if (!port.isEmpty()) {
        rule.insert(QStringLiteral("port"), port);
    }

    if (!source.network.trimmed().isEmpty()) {
        rule.insert(QStringLiteral("network"), source.network.trimmed());
    }

    RoutingRuleJsonSupport::insertStringArray(rule, QStringLiteral("domain"), domains);
    RoutingRuleJsonSupport::insertStringArray(rule, QStringLiteral("ip"), ips);
    RoutingRuleJsonSupport::insertStringArray(rule, QStringLiteral("protocol"), protocols);
    RoutingRuleJsonSupport::insertStringArray(rule, QStringLiteral("process"), processes);

    return rule;
}

} // namespace

void RoutingRuleJsonMapper::appendLegacyRoutingRule(QJsonArray& rules, const RoutingRule& source)
{
    const QStringList domains = RoutingRuleJsonSupport::normalizeRuleValues(source.domain, true, true);
    const QStringList ips = RoutingRuleJsonSupport::normalizeRuleValues(source.ip);
    const QStringList protocols = RoutingRuleJsonSupport::normalizeRuleValues(source.protocol);
    const QStringList processes = RoutingRuleJsonSupport::normalizeRuleValues(source.process);
    const QStringList inboundTags = RoutingRuleJsonSupport::normalizeRuleValues(source.inboundTag);
    const QString port = source.port.trimmed().isEmpty() ? QString() : source.port.trimmed();
    const QString network = source.network.trimmed();
    const bool hasDomainOrIpOrProcess = !domains.isEmpty() || !ips.isEmpty() || !processes.isEmpty();

    if (!domains.isEmpty()) {
        rules.append(createLegacyRoutingRule(source, inboundTags, port, domains, {}, protocols, {}));
    }

    if (!ips.isEmpty()) {
        rules.append(createLegacyRoutingRule(source, inboundTags, port, {}, ips, protocols, {}));
    }

    if (!processes.isEmpty()) {
        rules.append(createLegacyRoutingRule(source, inboundTags, port, {}, {}, protocols, processes));
    }

    if (!hasDomainOrIpOrProcess
        && (!port.isEmpty() || !network.isEmpty() || !protocols.isEmpty() || !inboundTags.isEmpty())) {
        rules.append(createLegacyRoutingRule(source, inboundTags, port, {}, {}, protocols, {}));
    }
}

void RoutingRuleJsonMapper::appendSingBoxRoutingRule(QJsonArray& rules, const RoutingRule& source)
{
    SingBoxRoutingRuleMapper::appendRoutingRule(rules, source);
}

void RoutingRuleJsonMapper::appendSingBoxRoutingIpRule(QJsonArray& rules, const RoutingRule& source)
{
    SingBoxRoutingRuleMapper::appendIpRule(rules, source);
}

QStringList RoutingRuleJsonMapper::normalizeRuleValues(
    const QStringList& values,
    bool replaceCommaPlaceholder,
    bool removeCommentEntries)
{
    return RoutingRuleJsonSupport::normalizeRuleValues(values, replaceCommaPlaceholder, removeCommentEntries);
}
