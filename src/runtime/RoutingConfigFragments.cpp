#include "runtime/RoutingConfigFragments.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QSet>
#include <QStringList>

#include "common/GitHubUrls.h"
#include "domain/models/RoutingRule.h"
#include "runtime/DnsConfigFragments.h"
#include "runtime/RoutingRuleJsonMapper.h"
#include "runtime/core/singbox/SingBoxConfigFragments.h"

namespace {

const QString kSingBoxDirectDnsTag = QStringLiteral("direct_dns");
const QString kLocationProbeTag = QStringLiteral("location-probe");
const QString kLegacyDnsTag = QStringLiteral("dns-module");
const QString kLegacyDirectDnsTagPrefix = QStringLiteral("direct-dns");
const QString kSingBoxRuleSetDirectoryName = QStringLiteral("rule-set");
constexpr int kLocationProbePortOffset = 103;

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

QString resolveLocalSingBoxRuleSetPath(const QString& tag)
{
    const QString fileName = QStringLiteral("%1.srs").arg(tag);
    const QString path = QDir(QCoreApplication::applicationDirPath())
                             .filePath(QStringLiteral("%1/%2").arg(kSingBoxRuleSetDirectoryName, fileName));
    const QFileInfo fileInfo(path);
    return fileInfo.exists() && fileInfo.isFile()
        ? fileInfo.absoluteFilePath()
        : QString();
}

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

QJsonObject RoutingConfigFragments::buildSingBoxRoute(const Config& config, const RoutingItem* selectedRouting)
{
    QJsonObject route;
    route.insert(QStringLiteral("final"), QStringLiteral("proxy"));
    const bool customDnsObject = DnsConfigFragments::isCustomDnsObjectText(config.dns().remoteDns);
    const bool hasDirectDnsServer = !DnsConfigFragments::parseSingBoxDnsAddress(config.dns().directDns).isEmpty();
    if (!customDnsObject && hasDirectDnsServer) {
        QJsonObject defaultDomainResolver;
        defaultDomainResolver.insert(QStringLiteral("server"), kSingBoxDirectDnsTag);
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
    locationProbeRule.insert(QStringLiteral("inbound"), QJsonArray{kLocationProbeTag});
    rules.append(locationProbeRule);
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
        const QMap<QString, QString> systemHostsMap = config.dns().useSystemHosts ? DnsConfigFragments::loadSystemHosts() : QMap<QString, QString>{};
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

    QString singBoxDomainStrategy = config.dns().domainStrategy4Singbox.trimmed();
    if (selectedRouting != nullptr && !selectedRouting->domainStrategy4Singbox.trimmed().isEmpty()) {
        singBoxDomainStrategy = selectedRouting->domainStrategy4Singbox.trimmed();
    }

    auto appendResolveRule = [&rules, &singBoxDomainStrategy]() {
        QJsonObject resolveRule;
        resolveRule.insert(QStringLiteral("action"), QStringLiteral("resolve"));
        if (!singBoxDomainStrategy.isEmpty()) {
            resolveRule.insert(QStringLiteral("strategy"), singBoxDomainStrategy);
        }
        rules.append(resolveRule);
    };

    const QString legacyDomainStrategy = config.dns().domainStrategy.trimmed();
    const bool reapplyIpRulesAfterResolve = legacyDomainStrategy.compare(QStringLiteral("IPIfNonMatch"), Qt::CaseInsensitive) == 0;
    QList<RoutingRule> ipRulesAfterResolve;
    if (legacyDomainStrategy.compare(QStringLiteral("IPOnDemand"), Qt::CaseInsensitive) == 0) {
        appendResolveRule();
    }

    const QList<RoutingRule> effectiveRules = effectiveRoutingRules(config, selectedRouting);
    for (const RoutingRule& rule : effectiveRules) {
        RoutingRuleJsonMapper::appendSingBoxRoutingRule(rules, rule);
        if (reapplyIpRulesAfterResolve && !RoutingRuleJsonMapper::normalizeRuleValues(rule.ip).isEmpty()) {
            ipRulesAfterResolve.append(rule);
        }
    }

    if (reapplyIpRulesAfterResolve) {
        appendResolveRule();
        for (const RoutingRule& rule : ipRulesAfterResolve) {
            RoutingRuleJsonMapper::appendSingBoxRoutingIpRule(rules, rule);
        }
    }

    if (config.tun().tunModeItem.enableTun) {
        SingBoxConfigFragments::appendTunUdpRouteRule(rules, config.tun().tunModeItem);
    }

    if (!rules.isEmpty()) {
        route.insert(QStringLiteral("rules"), rules);
    }

    return route;
}

void RoutingConfigFragments::migrateGeoToRuleSet(QJsonObject& root)
{
    const QSet<QString> kUnavailableRuleSets = {
        QStringLiteral("geosite-gfw"),
        QStringLiteral("geoip-telegram"),
    };

    QSet<QString> allRuleSetTags;

    auto processRules = [&allRuleSetTags, &kUnavailableRuleSets](QJsonArray& rules) {
        for (int i = 0; i < rules.size(); ++i) {
            QJsonObject rule = rules[i].toObject();
            QJsonArray ruleSetTags = rule.take(QStringLiteral("rule_set")).toArray();
            bool modified = false;

            if (rule.contains(QStringLiteral("geosite"))) {
                QJsonArray values = rule.take(QStringLiteral("geosite")).toArray();
                for (const QJsonValue& v : values) {
                    const QString tag = QStringLiteral("geosite-%1").arg(v.toString());
                    if (kUnavailableRuleSets.contains(tag)) {
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
                    if (kUnavailableRuleSets.contains(tag)) {
                        continue;
                    }
                    ruleSetTags.append(tag);
                    allRuleSetTags.insert(tag);
                }
                modified = true;
            }

            if (modified) {
                rule.insert(QStringLiteral("rule_set"), ruleSetTags);
                rules[i] = rule;
            }
        }
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
