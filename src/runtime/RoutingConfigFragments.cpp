#include "runtime/RoutingConfigFragments.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QStringList>

#include "domain/models/RoutingRule.h"
#include "runtime/DnsConfigFragments.h"
#include "runtime/RoutingRuleJsonMapper.h"

namespace {

const QString kLocationProbeTag = QStringLiteral("location-probe");
const QString kLegacyDnsTag = QStringLiteral("dns-module");
const QString kLegacyDirectDnsTagPrefix = QStringLiteral("direct-dns");
constexpr int kLocationProbePortOffset = 103;

} // namespace

QString RoutingConfigFragments::locationProbeTag()
{
    return kLocationProbeTag;
}

int RoutingConfigFragments::locationProbePortOffset()
{
    return kLocationProbePortOffset;
}

const RoutingItem* RoutingConfigFragments::resolveSelectedRouting(const Config& config)
{
    if (config.collection().enableRoutingAdvanced) {
        if (config.collection().routingIndex >= 0 && config.collection().routingIndex < config.collection().routingItems.size()) {
            return &config.collection().routingItems.at(config.collection().routingIndex);
        }
        return nullptr;
    }

    for (const RoutingItem& item : config.collection().routingItems) {
        if (item.locked) {
            return &item;
        }
    }

    return nullptr;
}

QList<RoutingRule> RoutingConfigFragments::effectiveRoutingRules(const Config& config, const RoutingItem* selectedRouting)
{
    QList<RoutingRule> rules;
    const QStringList customOrder{
        QStringLiteral("block"),
        QStringLiteral("direct"),
        QStringLiteral("proxy")};

    for (const QString& outboundTag : customOrder) {
        for (const RoutingRule& rule : config.collection().routingCustomRules) {
            if (!rule.enabled || rule.outboundTag.compare(outboundTag, Qt::CaseInsensitive) != 0) {
                continue;
            }
            rules.append(rule);
        }
    }

    if (selectedRouting != nullptr) {
        for (const RoutingRule& rule : selectedRouting->rules) {
            if (rule.enabled) {
                rules.append(rule);
            }
        }
    }

    return rules;
}

QJsonObject RoutingConfigFragments::buildLegacyRouting(const Config& config, const RoutingItem* selectedRouting)
{
    QJsonObject routing;

    if (!config.dns().domainStrategy.trimmed().isEmpty()) {
        routing.insert(QStringLiteral("domainStrategy"), config.dns().domainStrategy.trimmed());
    }

    if (!config.dns().domainMatcher.trimmed().isEmpty()) {
        routing.insert(QStringLiteral("domainMatcher"), config.dns().domainMatcher.trimmed());
    }

    QJsonArray rules;
    QJsonObject locationProbeRule;
    locationProbeRule.insert(QStringLiteral("type"), QStringLiteral("field"));
    locationProbeRule.insert(QStringLiteral("outboundTag"), QStringLiteral("proxy"));
    locationProbeRule.insert(QStringLiteral("inboundTag"), QJsonArray{kLocationProbeTag});
    rules.append(locationProbeRule);

    const QList<RoutingRule> effectiveRules = effectiveRoutingRules(config, selectedRouting);
    for (const RoutingRule& rule : effectiveRules) {
        RoutingRuleJsonMapper::appendLegacyRoutingRule(rules, rule);
    }

    if (!DnsConfigFragments::isCustomDnsObjectText(config.dns().remoteDns)) {
        const QStringList directDnsAddresses = DnsConfigFragments::splitDnsAddresses(config.dns().directDns);
        QStringList directDomains;
        for (const RoutingRule& rule : effectiveRules) {
            if (rule.outboundTag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) == 0) {
                directDomains.append(RoutingRuleJsonMapper::normalizeRuleValues(rule.domain, true, true));
            }
        }

        int directDnsTagIndex = 1;
        if (!directDomains.isEmpty()) {
            for (qsizetype index = 0; index < directDnsAddresses.size(); ++index) {
                QJsonObject rule;
                rule.insert(QStringLiteral("type"), QStringLiteral("field"));
                rule.insert(QStringLiteral("outboundTag"), QStringLiteral("direct"));
                rule.insert(
                    QStringLiteral("inboundTag"),
                    QJsonArray{QStringLiteral("%1-%2").arg(kLegacyDirectDnsTagPrefix).arg(directDnsTagIndex++)});
                rules.append(rule);
            }
        }
        if (DnsConfigFragments::usesDirectDnsAsFinalServer(selectedRouting)) {
            for (qsizetype index = 0; index < directDnsAddresses.size(); ++index) {
                QJsonObject rule;
                rule.insert(QStringLiteral("type"), QStringLiteral("field"));
                rule.insert(QStringLiteral("outboundTag"), QStringLiteral("direct"));
                rule.insert(
                    QStringLiteral("inboundTag"),
                    QJsonArray{QStringLiteral("%1-%2").arg(kLegacyDirectDnsTagPrefix).arg(directDnsTagIndex++)});
                rules.append(rule);
            }
        }

        if (!directDnsAddresses.isEmpty() || !DnsConfigFragments::splitDnsAddresses(config.dns().remoteDns).isEmpty()) {
            QJsonObject dnsFinalRule;
            dnsFinalRule.insert(QStringLiteral("type"), QStringLiteral("field"));
            dnsFinalRule.insert(
                QStringLiteral("outboundTag"),
                DnsConfigFragments::usesDirectDnsAsFinalServer(selectedRouting) ? QStringLiteral("direct") : QStringLiteral("proxy"));
            dnsFinalRule.insert(QStringLiteral("inboundTag"), QJsonArray{kLegacyDnsTag});
            rules.append(dnsFinalRule);
        }
    }

    if (!rules.isEmpty()) {
        routing.insert(QStringLiteral("rules"), rules);
    }

    return routing;
}
