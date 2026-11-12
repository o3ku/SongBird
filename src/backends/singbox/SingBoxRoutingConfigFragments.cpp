#include "backends/singbox/SingBoxRoutingConfigFragments.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QSet>
#include <QStringList>

#include "backends/singbox/SingBoxConfigFragments.h"
#include "backends/singbox/SingBoxRuleSetAvailability.h"
#include "common/AppPaths.h"
#include "common/GitHubUrls.h"
#include "common/SingBoxPaths.h"
#include "domain/models/RoutingRule.h"
#include "runtime/DnsConfigFragments.h"
#include "runtime/RoutingConfigFragments.h"
#include "runtime/RoutingRuleJsonMapper.h"

namespace {

QString resolveLocalSingBoxRuleSetPath(const QString& tag)
{
    const QString fileName = QStringLiteral("%1.srs").arg(tag);
    const QString path = QDir(AppPaths::applicationDirectory())
                             .filePath(QStringLiteral("%1/%2").arg(SingBoxPaths::ruleSetDirectoryName(), fileName));
    const QFileInfo fileInfo(path);
    return fileInfo.exists() && fileInfo.isFile()
        ? fileInfo.absoluteFilePath()
        : QString();
}

// Whether a rule still carries a field that can match traffic. Only the action keys are not
// conditions: everything else the mapper emits -- domain/domain_*/ip_cidr/ip_is_private/port/
// network/protocol/inbound/process_*/clash_mode/rule_set -- selects what the rule applies to.
bool ruleHasMatchCondition(const QJsonObject& rule)
{
    static const QSet<QString> nonConditionKeys = {
        QStringLiteral("action"),
        QStringLiteral("outbound"),
        QStringLiteral("type"),
    };
    for (auto it = rule.constBegin(); it != rule.constEnd(); ++it) {
        if (!nonConditionKeys.contains(it.key())) {
            return true;
        }
    }
    return false;
}

} // namespace

QJsonObject SingBoxRoutingConfigFragments::buildRoute(
    const Config& config,
    const RoutingItem* selectedRouting,
    const QJsonArray& dnsServers)
{
    QJsonObject route;
    route.insert(QStringLiteral("final"), QStringLiteral("proxy"));

    // A custom `dns` object carries its own resolver rules, so the app must not inject a
    // `resolve` rule built from the hosts maps on top of it.
    const bool customDnsObject = DnsConfigFragments::isCustomDnsObjectText(config.dns().remoteDns);

    // Naming a default resolver is not optional once the config declares more than one DNS
    // server: sing-box refuses to start without it (FATAL, and only the deprecated
    // ENABLE_DEPRECATED_MISSING_DOMAIN_RESOLVER escape hatch silences it). So the tag comes from
    // the servers the config really carries rather than being assumed to be `direct_dns`, which
    // used to leave the field out whenever the direct DNS was cleared or replaced by a custom
    // `dns` object -- exactly the two states a user can reach from the settings page.
    const QString defaultResolverTag = DnsConfigFragments::defaultDomainResolverTag(dnsServers);
    if (!defaultResolverTag.isEmpty()) {
        QJsonObject defaultDomainResolver;
        defaultDomainResolver.insert(QStringLiteral("server"), defaultResolverTag);
        const QString directDnsStrategy = DnsConfigFragments::mapDomainStrategyToSingBox(config.dns().domainStrategyForFreedom);
        if (!directDnsStrategy.isEmpty()) {
            defaultDomainResolver.insert(QStringLiteral("strategy"), directDnsStrategy);
        }
        route.insert(QStringLiteral("default_domain_resolver"), defaultDomainResolver);
    }

    QJsonArray rules;
    QJsonObject locationProbeRule;
    locationProbeRule.insert(QStringLiteral("action"), QStringLiteral("route"));
    locationProbeRule.insert(QStringLiteral("outbound"), QStringLiteral("proxy"));
    locationProbeRule.insert(QStringLiteral("inbound"), QJsonArray{RoutingConfigFragments::locationProbeTag()});
    rules.append(locationProbeRule);

    rules.append(SingBoxConfigFragments::buildTunCompatPrivateAddressDirectRule());

    if (config.tun().tunModeItem.enableTun) {
        route.insert(QStringLiteral("auto_detect_interface"), true);
        QJsonArray tunRules = SingBoxConfigFragments::buildTunCompatRejectRules();
        for (const QJsonValue& rule : tunRules) {
            rules.append(rule);
        }
        SingBoxConfigFragments::appendTunCompatProcessRules(rules);
        SingBoxConfigFragments::appendTunIcmpRouteRule(rules, config.tun().tunModeItem);
    }
    SingBoxConfigFragments::appendSniffRules(rules, config);

    if (!customDnsObject) {
        QJsonObject hostsResolveRule;
        hostsResolveRule.insert(QStringLiteral("action"), QStringLiteral("resolve"));
        int hostsMatchCount = 0;
        const QMap<QString, QStringList> hostsMap = DnsConfigFragments::parseHostsToDictionary(config.dns().dnsHosts);
        const QMap<QString, QString> systemHostsMap =
            config.dns().useSystemHosts ? DnsConfigFragments::loadSystemHosts() : QMap<QString, QString>{};
        for (auto it = systemHostsMap.constBegin(); it != systemHostsMap.constEnd(); ++it) {
            if (DnsConfigFragments::appendSingBoxDomainField(hostsResolveRule, it.key(), true)) {
                ++hostsMatchCount;
            }
        }
        for (auto it = hostsMap.constBegin(); it != hostsMap.constEnd(); ++it) {
            if (DnsConfigFragments::appendSingBoxDomainField(hostsResolveRule, it.key(), true)) {
                ++hostsMatchCount;
            }
        }
        if (hostsMatchCount > 0) {
            rules.append(hostsResolveRule);
        }
    }

    QJsonObject directModeRule;
    directModeRule.insert(QStringLiteral("outbound"), QStringLiteral("direct"));
    directModeRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Direct"));
    rules.append(directModeRule);

    QJsonObject globalModeRule;
    globalModeRule.insert(QStringLiteral("outbound"), QStringLiteral("proxy"));
    globalModeRule.insert(QStringLiteral("clash_mode"), QStringLiteral("Global"));
    rules.append(globalModeRule);

    if (config.tun().tunModeItem.enableTun) {
        SingBoxConfigFragments::appendTunUdpRouteRule(rules, config.tun().tunModeItem);
    }

    const QList<RoutingRule> effectiveRules = RoutingConfigFragments::effectiveRoutingRules(config, selectedRouting);
    for (const RoutingRule& rule : effectiveRules) {
        RoutingRuleJsonMapper::appendSingBoxRoutingRule(rules, rule);
    }

    if (!rules.isEmpty()) {
        route.insert(QStringLiteral("rules"), rules);
    }

    return route;
}

void SingBoxRoutingConfigFragments::migrateGeoToRuleSet(QJsonObject& root)
{
    QSet<QString> allRuleSetTags;

    auto processRules = [&allRuleSetTags](QJsonArray& rules) {
        QJsonArray processed;
        for (const QJsonValue& value : rules) {
            QJsonObject rule = value.toObject();
            QJsonArray ruleSetTags = rule.take(QStringLiteral("rule_set")).toArray();
            bool modified = false;

            if (rule.contains(QStringLiteral("geosite"))) {
                QJsonArray values = rule.take(QStringLiteral("geosite")).toArray();
                for (const QJsonValue& v : values) {
                const QString tag = QStringLiteral("geosite-%1").arg(v.toString());
                if (!isSingBoxRuleSetPublishedUpstream(tag)) {
                    continue;
                }
                    ruleSetTags.append(tag);
                    allRuleSetTags.insert(tag);
                }
                modified = true;
            }

            if (rule.contains(QStringLiteral("geoip"))) {
                QJsonArray values = rule.take(QStringLiteral("geoip")).toArray();
                for (const QJsonValue& v : values) {
                    const QString tag = QStringLiteral("geoip-%1").arg(v.toString());
                    if (!isSingBoxRuleSetPublishedUpstream(tag)) {
                        continue;
                    }
                    ruleSetTags.append(tag);
                    allRuleSetTags.insert(tag);
                }
                modified = true;
            }

            if (!modified) {
                // No geosite/geoip: the rule is untouched (its own rule_set, if any, is intact in
                // `value`, which is what gets appended).
                processed.append(value);
                continue;
            }

            if (!ruleSetTags.isEmpty()) {
                rule.insert(QStringLiteral("rule_set"), ruleSetTags);
            } else if (!ruleHasMatchCondition(rule)) {
                // Every condition was an unpublished rule-set, so nothing is left to match. Kept
                // as-is the rule would be unconditional and shadow every rule after it, so it is
                // dropped rather than allowed to become a catch-all.
                continue;
            }
            processed.append(rule);
        }
        rules = processed;
    };

    if (root.contains(QStringLiteral("dns"))) {
        QJsonObject dns = root.value(QStringLiteral("dns")).toObject();
        if (dns.contains(QStringLiteral("rules"))) {
            QJsonArray dnsRules = dns.take(QStringLiteral("rules")).toArray();
            processRules(dnsRules);
            dns.insert(QStringLiteral("rules"), dnsRules);
            root.insert(QStringLiteral("dns"), dns);
        }
    }

    QJsonObject route = root.take(QStringLiteral("route")).toObject();
    if (route.contains(QStringLiteral("rules"))) {
        QJsonArray routeRules = route.take(QStringLiteral("rules")).toArray();
        processRules(routeRules);
        route.insert(QStringLiteral("rules"), routeRules);
    }

    if (!allRuleSetTags.isEmpty()) {
        QJsonArray ruleSetDefinitions;
        for (const QString& tag : allRuleSetTags) {
            QJsonObject def;
            def.insert(QStringLiteral("tag"), tag);
            def.insert(QStringLiteral("format"), QStringLiteral("binary"));
            const QString localPath = resolveLocalSingBoxRuleSetPath(tag);
            if (!localPath.isEmpty()) {
                def.insert(QStringLiteral("type"), QStringLiteral("local"));
                def.insert(QStringLiteral("path"), localPath);
            } else {
                def.insert(QStringLiteral("type"), QStringLiteral("remote"));
                def.insert(QStringLiteral("download_detour"), QStringLiteral("proxy"));
                if (tag.startsWith(QStringLiteral("geosite-"))) {
                    def.insert(QStringLiteral("url"),
                        singRuleSetDownloadUrl(singGeositeRepositoryPath(), tag).toString());
                } else if (tag.startsWith(QStringLiteral("geoip-"))) {
                    def.insert(QStringLiteral("url"),
                        singRuleSetDownloadUrl(singGeoipRepositoryPath(), tag).toString());
                }
            }
            ruleSetDefinitions.append(def);
        }
        route.insert(QStringLiteral("rule_set"), ruleSetDefinitions);
    }

    root.insert(QStringLiteral("route"), route);
}
