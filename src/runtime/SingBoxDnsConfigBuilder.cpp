#include "runtime/SingBoxDnsConfigBuilder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include "runtime/DnsHosts.h"
#include "runtime/SingBoxDnsConfigSupport.h"
#include "runtime/SingBoxDnsRuleConfig.h"

namespace DnsSupport = SingBoxDnsConfigSupport;
namespace DnsRules = SingBoxDnsRuleConfig;

namespace SingBoxDnsConfigBuilder {

QJsonObject build(const Config& config, const RoutingItem* selectedRouting)
{
    const QString remoteDns = config.dns().remoteDns.trimmed();
    if (!remoteDns.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(remoteDns.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError
            && document.isObject()
            && document.object().contains(QStringLiteral("servers"))) {
            return document.object();
        }
    }

    const bool useDirectFinal = DnsSupport::usesDirectDnsAsFinalServer(selectedRouting);

    QJsonObject dns;
    dns.insert(QStringLiteral("independent_cache"), true);

    QJsonArray servers;
    QJsonObject localDns = DnsSupport::parseDnsAddress(config.dns().bootstrapDns);
    if (!localDns.isEmpty()) {
        localDns.insert(QStringLiteral("tag"), DnsSupport::localDnsTag());
        servers.append(localDns);
    }

    QJsonObject remoteDnsServer = DnsSupport::parseDnsAddress(config.dns().remoteDns);
    if (!remoteDnsServer.isEmpty()) {
        remoteDnsServer.insert(QStringLiteral("tag"), DnsSupport::remoteDnsTag());
        remoteDnsServer.insert(QStringLiteral("detour"), QStringLiteral("proxy"));
        remoteDnsServer.insert(QStringLiteral("domain_resolver"), DnsSupport::localDnsTag());
        servers.append(remoteDnsServer);
    }

    QJsonObject directDnsServer = DnsSupport::parseDnsAddress(config.dns().directDns);
    if (!directDnsServer.isEmpty()) {
        directDnsServer.insert(QStringLiteral("tag"), DnsSupport::directDnsTag());
        directDnsServer.insert(QStringLiteral("domain_resolver"), DnsSupport::localDnsTag());
        servers.append(directDnsServer);
    }

    const QMap<QString, QStringList> hostsMap = DnsHosts::parseConfigured(config.dns().dnsHosts);
    const QJsonObject predefinedHosts = DnsSupport::predefinedHosts(config, hostsMap);
    const bool hasPredefinedHosts = !predefinedHosts.isEmpty();
    const QJsonObject hostsDnsServer = DnsSupport::hostsDnsServer(predefinedHosts);
    DnsSupport::applyHostsResolver(localDns, predefinedHosts);
    DnsSupport::applyHostsResolver(remoteDnsServer, predefinedHosts);
    DnsSupport::applyHostsResolver(directDnsServer, predefinedHosts);
    for (qsizetype index = 0; index < servers.size(); ++index) {
        const QString tag = servers.at(index).toObject().value(QStringLiteral("tag")).toString();
        if (tag == DnsSupport::localDnsTag()) {
            servers[index] = localDns;
        } else if (tag == DnsSupport::remoteDnsTag()) {
            servers[index] = remoteDnsServer;
        } else if (tag == DnsSupport::directDnsTag()) {
            servers[index] = directDnsServer;
        }
    }
    if (hasPredefinedHosts) {
        servers.append(hostsDnsServer);
    }

    if (config.dns().fakeIp) {
        QJsonObject fakeDnsServer;
        fakeDnsServer.insert(QStringLiteral("tag"), DnsSupport::fakeDnsTag());
        fakeDnsServer.insert(QStringLiteral("type"), QStringLiteral("fakeip"));
        fakeDnsServer.insert(QStringLiteral("inet4_range"), QStringLiteral("198.18.0.0/15"));
        fakeDnsServer.insert(QStringLiteral("inet6_range"), QStringLiteral("fc00::/18"));
        servers.append(fakeDnsServer);
    }

    if (servers.isEmpty()) {
        return {};
    }

    dns.insert(QStringLiteral("servers"), servers);
    const QString finalServerTag = DnsSupport::finalDnsServerTag(
        !remoteDnsServer.isEmpty(),
        !directDnsServer.isEmpty(),
        hasPredefinedHosts,
        useDirectFinal);
    if (!finalServerTag.isEmpty()) {
        dns.insert(QStringLiteral("final"), finalServerTag);
    }

    QJsonArray rules;
    DnsRules::appendModeRules(
        rules,
        config,
        hasPredefinedHosts,
        !remoteDnsServer.isEmpty(),
        !directDnsServer.isEmpty());
    DnsRules::appendHostRules(rules, hostsMap);

    if (config.dns().blockBindingQuery) {
        DnsRules::appendBlockBindingRule(rules);
    }

    if (config.dns().fakeIp && config.dns().globalFakeIp) {
        DnsRules::appendGlobalFakeIpRule(rules);
    }
    DnsRules::appendRoutingRules(rules, config, selectedRouting);

    if (config.dns().fakeIp && !config.dns().globalFakeIp && !useDirectFinal) {
        DnsRules::appendScopedFakeIpFallbackRule(rules);
    }

    if (!rules.isEmpty()) {
        dns.insert(QStringLiteral("rules"), rules);
    }

    return dns;
}

} // namespace SingBoxDnsConfigBuilder
