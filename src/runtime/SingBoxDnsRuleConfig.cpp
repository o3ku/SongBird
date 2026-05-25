#include "runtime/SingBoxDnsRuleConfig.h"

#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include "runtime/DnsHosts.h"
#include "runtime/RoutingRuleJsonMapper.h"
#include "runtime/SingBoxDnsConfigSupport.h"

namespace {

struct ExpectedIps {
    QStringList cidrs;
    QStringList geoips;
    QSet<QString> regionNames;
};

QList<RoutingRule> effectiveRoutingRules(const Config& config, const RoutingItem* selectedRouting)
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

ExpectedIps parseExpectedIps(const QString& value)
{
    ExpectedIps expectedIps;
    for (const QString& item : value.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts)) {
        const QString trimmed = item.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (trimmed.startsWith(QStringLiteral("geoip:"), Qt::CaseInsensitive)) {
            const QString region = trimmed.mid(QStringLiteral("geoip:").size()).trimmed();
            if (region.isEmpty()) {
                continue;
            }
            expectedIps.geoips.append(region);
            expectedIps.regionNames.insert(region);
            expectedIps.regionNames.insert(QStringLiteral("geolocation-%1").arg(region));
            expectedIps.regionNames.insert(QStringLiteral("tld-%1").arg(region));
        } else {
            expectedIps.cidrs.append(trimmed);
        }
    }

    return expectedIps;
}

QJsonArray stringArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }

    return array;
}

void appendExpectedIpFilters(QJsonObject& rule, const ExpectedIps& expectedIps)
{
    if (expectedIps.geoips.isEmpty()) {
        return;
    }

    QSet<QString> geositeValues;
    for (const QJsonValue& geositeValue : rule.value(QStringLiteral("geosite")).toArray()) {
        geositeValues.insert(geositeValue.toString());
    }

    bool matchedExpectedRegion = false;
    for (const QString& geositeValue : geositeValues) {
        if (expectedIps.regionNames.contains(geositeValue)) {
            matchedExpectedRegion = true;
            break;
        }
    }
    if (!matchedExpectedRegion) {
        return;
    }

    rule.insert(QStringLiteral("geoip"), stringArray(expectedIps.geoips));
    if (!expectedIps.cidrs.isEmpty()) {
        rule.insert(QStringLiteral("ip_cidr"), stringArray(expectedIps.cidrs));
    }
}

} // namespace

namespace SingBoxDnsRuleConfig {

void appendModeRules(
    QJsonArray& rules,
    const Config& config,
    bool hasPredefinedHosts,
    bool hasRemoteDnsServer,
    bool hasDirectDnsServer)
{
    if (hasPredefinedHosts) {
        QJsonObject hostsRule;
        hostsRule.insert(QStringLiteral("ip_accept_any"), true);
        hostsRule.insert(QStringLiteral("server"), SingBoxDnsConfigSupport::hostsDnsTag());
        rules.append(hostsRule);
    }

    if (hasRemoteDnsServer) {
        QJsonObject proxyRule;
        proxyRule.insert(QStringLiteral("server"), SingBoxDnsConfigSupport::remoteDnsTag());
        proxyRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Global"));
        const QString proxyStrategy = SingBoxDnsConfigSupport::mapDomainStrategy(config.dns().domainStrategyForProxy);
        if (!proxyStrategy.isEmpty()) {
            proxyRule.insert(QStringLiteral("strategy"), proxyStrategy);
        }
        rules.append(proxyRule);
    }

    if (hasDirectDnsServer) {
        QJsonObject directRule;
        directRule.insert(QStringLiteral("server"), SingBoxDnsConfigSupport::directDnsTag());
        directRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Direct"));
        const QString directStrategy = SingBoxDnsConfigSupport::mapDomainStrategy(config.dns().domainStrategyForFreedom);
        if (!directStrategy.isEmpty()) {
            directRule.insert(QStringLiteral("strategy"), directStrategy);
        }
        rules.append(directRule);
    }
}

void appendHostRules(QJsonArray& rules, const QMap<QString, QStringList>& hostsMap)
{
    for (auto it = hostsMap.constBegin(); it != hostsMap.constEnd(); ++it) {
        if (it.value().isEmpty()) {
            continue;
        }

        const QString predefined = it.value().constFirst().trimmed();
        if (predefined.isEmpty()) {
            continue;
        }

        QJsonObject rule;
        rule.insert(QStringLiteral("query_type"), QJsonArray{1, 5, 28});
        rule.insert(QStringLiteral("action"), QStringLiteral("predefined"));
        rule.insert(QStringLiteral("rcode"), QStringLiteral("NOERROR"));
        if (!SingBoxDnsConfigSupport::appendDomainField(rule, it.key(), true)) {
            continue;
        }

        if (predefined.startsWith(QChar('#'))) {
            bool ok = false;
            const int rcode = predefined.mid(1).toInt(&ok);
            rule.insert(QStringLiteral("rcode"), SingBoxDnsConfigSupport::mapRcode(ok ? rcode : 0));
        } else if (DnsHosts::isDomainName(predefined)) {
            rule.insert(QStringLiteral("answer"), QJsonArray{QStringLiteral("*. IN CNAME %1.").arg(predefined)});
        } else if (DnsHosts::isIpAddress(predefined) && rule.value(QStringLiteral("domain")).toArray().isEmpty()) {
            rule.insert(
                QStringLiteral("answer"),
                QJsonArray{
                    QStringLiteral("*. IN %1 %2").arg(predefined.contains(QChar(':')) ? QStringLiteral("AAAA") : QStringLiteral("A"), predefined)});
        } else {
            continue;
        }

        rules.append(rule);
    }
}

void appendBlockBindingRule(QJsonArray& rules)
{
    QJsonObject blockBindingRule;
    blockBindingRule.insert(QStringLiteral("query_type"), QJsonArray{64, 65});
    blockBindingRule.insert(QStringLiteral("action"), QStringLiteral("predefined"));
    blockBindingRule.insert(QStringLiteral("rcode"), QStringLiteral("NOERROR"));
    rules.append(blockBindingRule);
}

void appendGlobalFakeIpRule(QJsonArray& rules)
{
    QJsonObject filterRule = SingBoxDnsConfigSupport::fakeIpFilterRule();
    if (filterRule.isEmpty()) {
        return;
    }

    filterRule.insert(QStringLiteral("invert"), true);

    QJsonObject fakeDnsRule;
    fakeDnsRule.insert(QStringLiteral("server"), SingBoxDnsConfigSupport::fakeDnsTag());
    fakeDnsRule.insert(QStringLiteral("type"), QStringLiteral("logical"));
    fakeDnsRule.insert(QStringLiteral("mode"), QStringLiteral("and"));
    fakeDnsRule.insert(QStringLiteral("rewrite_ttl"), 1);
    fakeDnsRule.insert(
        QStringLiteral("rules"),
        QJsonArray{
            QJsonObject{{QStringLiteral("query_type"), QJsonArray{1, 28}}},
            filterRule});
    rules.append(fakeDnsRule);
}

void appendRoutingRules(QJsonArray& rules, const Config& config, const RoutingItem* selectedRouting)
{
    const ExpectedIps expectedIps = parseExpectedIps(config.dns().directExpectedIps);
    const QList<RoutingRule> effectiveRules = effectiveRoutingRules(config, selectedRouting);
    if (effectiveRules.isEmpty()) {
        return;
    }

    const QString directStrategy = SingBoxDnsConfigSupport::mapDomainStrategy(config.dns().domainStrategyForFreedom);
    const QString proxyStrategy = SingBoxDnsConfigSupport::mapDomainStrategy(config.dns().domainStrategyForProxy);

    for (const RoutingRule& sourceRule : effectiveRules) {
        if (!sourceRule.enabled) {
            continue;
        }

        const QStringList domains = RoutingRuleJsonMapper::normalizeRuleValues(sourceRule.domain, true, true);
        if (domains.isEmpty()) {
            continue;
        }

        QJsonObject rule;
        int validDomainCount = 0;
        for (const QString& domain : domains) {
            if (SingBoxDnsConfigSupport::appendDomainField(rule, domain, true)) {
                ++validDomainCount;
            }
        }
        if (validDomainCount <= 0) {
            continue;
        }

        const QString outboundTag = sourceRule.outboundTag.trimmed();
        if (outboundTag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) == 0) {
            rule.insert(QStringLiteral("server"), SingBoxDnsConfigSupport::directDnsTag());
            if (!directStrategy.isEmpty()) {
                rule.insert(QStringLiteral("strategy"), directStrategy);
            }
            appendExpectedIpFilters(rule, expectedIps);
        } else if (outboundTag.compare(QStringLiteral("block"), Qt::CaseInsensitive) == 0) {
            rule.insert(QStringLiteral("action"), QStringLiteral("predefined"));
            rule.insert(QStringLiteral("rcode"), QStringLiteral("NXDOMAIN"));
        } else {
            if (config.dns().fakeIp && !config.dns().globalFakeIp) {
                QJsonObject fakeRule = rule;
                fakeRule.insert(QStringLiteral("server"), SingBoxDnsConfigSupport::fakeDnsTag());
                fakeRule.insert(QStringLiteral("query_type"), QJsonArray{1, 28});
                fakeRule.insert(QStringLiteral("rewrite_ttl"), 1);
                rules.append(fakeRule);
            }
            rule.insert(QStringLiteral("server"), SingBoxDnsConfigSupport::remoteDnsTag());
            if (!proxyStrategy.isEmpty()) {
                rule.insert(QStringLiteral("strategy"), proxyStrategy);
            }
        }

        rules.append(rule);
    }
}

void appendScopedFakeIpFallbackRule(QJsonArray& rules)
{
    QJsonObject fakeDnsRule;
    fakeDnsRule.insert(QStringLiteral("server"), SingBoxDnsConfigSupport::fakeDnsTag());
    fakeDnsRule.insert(QStringLiteral("query_type"), QJsonArray{1, 28});
    fakeDnsRule.insert(QStringLiteral("rewrite_ttl"), 1);
    rules.append(fakeDnsRule);
}

} // namespace SingBoxDnsRuleConfig
