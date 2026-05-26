#include "backends/singbox/SingBoxTunCompatConfigFragments.h"

#include <QSet>

#include "backends/singbox/SingBoxOutboundConfigFragments.h"
#include "domain/models/Config.h"

namespace {

QJsonObject buildDirectOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("direct"));
    outbound.insert(QStringLiteral("type"), QStringLiteral("direct"));
    return outbound;
}

QJsonObject buildBlockOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("block"));
    outbound.insert(QStringLiteral("type"), QStringLiteral("block"));
    return outbound;
}

bool isSupportedTunNetworkPolicy(const QString& policy)
{
    const QStringList supportedPolicies{
        QStringLiteral("rule"),
        QStringLiteral("direct"),
        QStringLiteral("unreachable"),
        QStringLiteral("drop"),
        QStringLiteral("reply")};
    return supportedPolicies.contains(policy);
}

QString tunRejectMethodForPolicy(const QString& policy)
{
    if (policy == QStringLiteral("drop")) {
        return QStringLiteral("drop");
    }

    if (policy == QStringLiteral("unreachable")) {
        return QStringLiteral("default");
    }

    return QStringLiteral("reply");
}

void appendTunNetworkPolicyRouteRule(QJsonArray& rules, const QString& network, const QString& policy)
{
    if (policy.isEmpty()
        || !isSupportedTunNetworkPolicy(policy)
        || policy == QStringLiteral("rule")) {
        return;
    }

    QJsonObject rule;
    rule.insert(QStringLiteral("network"), QJsonArray{network});

    if (policy == QStringLiteral("direct")) {
        rule.insert(QStringLiteral("outbound"), QStringLiteral("direct"));
        rules.append(rule);
        return;
    }

    rule.insert(QStringLiteral("action"), QStringLiteral("reject"));
    rule.insert(QStringLiteral("method"), tunRejectMethodForPolicy(policy));
    rules.append(rule);
}

} // namespace

namespace SingBoxTunCompatConfigFragments {

QJsonObject buildDns()
{
    QJsonObject server;
    server.insert(QStringLiteral("tag"), QStringLiteral("local"));
    server.insert(QStringLiteral("type"), QStringLiteral("local"));

    QJsonArray servers;
    servers.append(server);

    QJsonObject dns;
    dns.insert(QStringLiteral("servers"), servers);
    dns.insert(QStringLiteral("final"), QStringLiteral("local"));
    return dns;
}

QJsonObject buildRoute(const Config& config)
{
    QJsonObject route;
    route.insert(QStringLiteral("final"), QStringLiteral("proxy"));
    route.insert(QStringLiteral("auto_detect_interface"), true);

    QJsonArray rules = buildRejectRules();
    appendProcessRules(rules);
    rules.append(buildPrivateAddressDirectRule());
    appendIcmpRouteRule(rules, config.tun().tunModeItem);
    appendSniffRules(rules, config);
    appendUdpRouteRule(rules, config.tun().tunModeItem);

    route.insert(QStringLiteral("rules"), rules);
    return route;
}

QJsonArray buildOutbounds(const Config& config)
{
    QJsonArray outbounds;
    outbounds.append(SingBoxOutboundConfigFragments::buildSocksOutbound(
        QStringLiteral("proxy"),
        QStringLiteral("127.0.0.1"),
        config.localPort));
    outbounds.append(buildDirectOutbound());
    outbounds.append(buildBlockOutbound());
    return outbounds;
}

QJsonObject buildPrivateAddressDirectRule()
{
    QJsonObject rule;
    rule.insert(QStringLiteral("action"), QStringLiteral("route"));
    rule.insert(QStringLiteral("outbound"), QStringLiteral("direct"));
    rule.insert(QStringLiteral("ip_is_private"), true);
    return rule;
}

QJsonArray buildRejectRules()
{
    QJsonArray rules;

    QJsonObject udpRejectRule;
    udpRejectRule.insert(QStringLiteral("network"), QJsonArray{QStringLiteral("udp")});
    udpRejectRule.insert(QStringLiteral("port"), QJsonArray{135, 137, 138, 139, 5353});
    udpRejectRule.insert(QStringLiteral("action"), QStringLiteral("reject"));
    rules.append(udpRejectRule);

    QJsonObject multicastRejectRule;
    multicastRejectRule.insert(QStringLiteral("ip_cidr"), QJsonArray{
                                                          QStringLiteral("224.0.0.0/3"),
                                                          QStringLiteral("ff00::/8")});
    multicastRejectRule.insert(QStringLiteral("action"), QStringLiteral("reject"));
    rules.append(multicastRejectRule);

    return rules;
}

void appendProcessRules(QJsonArray& rules)
{
    QJsonObject dnsHijackRule;
    dnsHijackRule.insert(QStringLiteral("port"), QJsonArray{53});
    dnsHijackRule.insert(QStringLiteral("action"), QStringLiteral("hijack-dns"));
    QJsonArray dnsProcessNames;
    for (const QString& processName : buildDnsProcessNames()) {
        dnsProcessNames.append(processName);
    }
    dnsHijackRule.insert(QStringLiteral("process_name"), dnsProcessNames);
    rules.append(dnsHijackRule);

    QJsonObject directProcessRule;
    directProcessRule.insert(QStringLiteral("outbound"), QStringLiteral("direct"));
    QJsonArray processNames;
    for (const QString& processName : buildDirectProcessNames()) {
        processNames.append(processName);
    }
    directProcessRule.insert(QStringLiteral("process_name"), processNames);
    rules.append(directProcessRule);
}

void appendIcmpRouteRule(QJsonArray& rules, const TunModeItem& tun)
{
    const QString policy = tun.icmpRouting.trimmed().toLower();
    appendTunNetworkPolicyRouteRule(rules, QStringLiteral("icmp"), policy);
}

void appendUdpRouteRule(QJsonArray& rules, const TunModeItem& tun)
{
    const QString policy = tun.udpRouting.trimmed().toLower();
    appendTunNetworkPolicyRouteRule(rules, QStringLiteral("udp"), policy);
}

void appendSniffRules(QJsonArray& rules, const Config& config)
{
    if (config.sniffingEnabled) {
        QJsonObject sniffRule;
        sniffRule.insert(QStringLiteral("action"), QStringLiteral("sniff"));
        rules.append(sniffRule);

        QJsonObject dnsHijackRule;
        dnsHijackRule.insert(QStringLiteral("action"), QStringLiteral("hijack-dns"));
        dnsHijackRule.insert(QStringLiteral("protocol"), QJsonArray{QStringLiteral("dns")});
        rules.append(dnsHijackRule);
        return;
    }

    QJsonObject dnsHijackRule;
    dnsHijackRule.insert(QStringLiteral("port"), QJsonArray{53});
    dnsHijackRule.insert(QStringLiteral("network"), QJsonArray{QStringLiteral("udp")});
    dnsHijackRule.insert(QStringLiteral("action"), QStringLiteral("hijack-dns"));
    rules.append(dnsHijackRule);
}

QStringList buildDnsProcessNames()
{
    QStringList processNames{
        QStringLiteral("xray.exe"),
    };

    QSet<QString> seen;
    QStringList deduplicated;
    for (const QString& processName : processNames) {
        if (seen.contains(processName)) {
            continue;
        }
        seen.insert(processName);
        deduplicated.append(processName);
    }
    return deduplicated;
}

QStringList buildDirectProcessNames()
{
    QStringList processNames{QStringLiteral("SongBird.exe")};
    processNames.append(buildDnsProcessNames());
    processNames.append(QStringLiteral("sing-box-client.exe"));
    processNames.append(QStringLiteral("sing-box.exe"));
    return processNames;
}

} // namespace SingBoxTunCompatConfigFragments
