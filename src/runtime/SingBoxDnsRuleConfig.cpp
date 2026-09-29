#include "runtime/SingBoxDnsRuleConfig.h"

#include <QJsonObject>

#include "runtime/DnsHosts.h"
#include "runtime/RoutingRuleJsonMapper.h"
#include "runtime/SingBoxDnsConfigSupport.h"

namespace {

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

// The "Direct Expected IPs" filter used to append `geoip` and `ip_cidr` to the direct DNS rule.
// Both are response match fields: sing-box applies them only together with `match_response`, which
// in turn requires a preceding `evaluate` action and does not exist before 1.14. So on every core
// this generator ever produced configs for, the two fields never took part in matching -- the rule
// matched on its domain conditions alone, and a non-matching `ip_cidr` still selected `direct_dns`.
// From 1.14 the fields are rejected outright unless the rule is rewritten around `evaluate`, and a
// rule set holding only IP entries (`geoip-cn`) is rejected the same way. Since the domain
// conditions already pick the direct DNS server, the filter is dropped rather than migrated: it
// removes nothing that used to work and keeps one rule shape across core versions.

QJsonArray stringArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }

    return array;
}

} // namespace

namespace SingBoxDnsRuleConfig {

void appendModeRules(
    QJsonArray& rules,
    const QStringList& predefinedHostDomains,
    bool hasRemoteDnsServer,
    bool hasDirectDnsServer)
{
    if (!predefinedHostDomains.isEmpty()) {
        // "Let the hosts table answer when it has an entry, otherwise keep going" needs a rule item
        // that is true for exactly those domains. Both candidates for that item are generation
        // specific -- `ip_accept_any` is a response match field that 1.14 rejects outright, and its
        // 1.14 replacement `preferred_by` is an unknown field to 1.13 -- so neither can be emitted
        // by a generator that has to serve both. A plain exact-domain list says the same thing:
        // every key in a hosts table is a concrete domain, and every core accepts `domain`.
        QJsonObject hostsRule;
        hostsRule.insert(QStringLiteral("domain"), stringArray(predefinedHostDomains));
        hostsRule.insert(QStringLiteral("server"), SingBoxDnsConfigSupport::hostsDnsTag());
        rules.append(hostsRule);
    }

    if (hasRemoteDnsServer) {
        QJsonObject proxyRule;
        proxyRule.insert(QStringLiteral("server"), SingBoxDnsConfigSupport::remoteDnsTag());
        proxyRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Global"));
        rules.append(proxyRule);
    }

    if (hasDirectDnsServer) {
        QJsonObject directRule;
        directRule.insert(QStringLiteral("server"), SingBoxDnsConfigSupport::directDnsTag());
        directRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Direct"));
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
    const QList<RoutingRule> effectiveRules = effectiveRoutingRules(config, selectedRouting);
    if (effectiveRules.isEmpty()) {
        return;
    }

    // The per-rule `strategy` these branches used to carry is gone: sing-box 1.14 dropped it as a
    // DNS rule action option. The two settings that fed it now collapse into the single global
    // `dns.strategy` the builder emits, so this function only picks a server tag.

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
