#include "backends/mihomo/MihomoConfigFragments.h"

#include <optional>

#include <QJsonArray>

#include "backends/singbox/SingBoxOutboundConfigSupport.h"
#include "runtime/ProtocolConfigMapper.h"
#include "runtime/RoutingConfigFragments.h"
#include "runtime/TunAdapterNames.h"

namespace OutboundSupport = SingBoxOutboundConfigSupport;

namespace {

const QString kProxyGroupName = QStringLiteral("proxy");
const QString kPrimaryProxyName = QStringLiteral("server");

QString normalizedLogLevel(const QString& level)
{
    const QString normalized = level.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("warn")) {
        return QStringLiteral("warning");
    }
    return normalized;
}

int httpPort(const Config& config)
{
    return config.localHttpPort > 0 ? config.localHttpPort : config.localPort + 1;
}

int locationProbePort(const Config& config)
{
    return config.localLocationProbePort > 0
        ? config.localLocationProbePort
        : config.localPort + RoutingConfigFragments::locationProbePortOffset();
}

QString clashPolicyName(const QString& outboundTag)
{
    if (outboundTag.compare(QStringLiteral("direct"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("DIRECT");
    }
    if (outboundTag.compare(QStringLiteral("block"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("REJECT");
    }
    return kProxyGroupName;
}

void appendStringArray(QJsonArray& target, const QStringList& values)
{
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            target.append(trimmed);
        }
    }
}

QString normalizeClashType(ConfigType type)
{
    switch (type) {
    case ConfigType::VMess:
        return QStringLiteral("vmess");
    case ConfigType::VLESS:
        return QStringLiteral("vless");
    case ConfigType::Trojan:
        return QStringLiteral("trojan");
    case ConfigType::Shadowsocks:
        return QStringLiteral("ss");
    case ConfigType::Socks:
        return QStringLiteral("socks5");
    case ConfigType::HTTP:
        return QStringLiteral("http");
    case ConfigType::Hysteria2:
        return QStringLiteral("hysteria2");
    case ConfigType::Custom:
    case ConfigType::TUIC:
    case ConfigType::WireGuard:
    case ConfigType::AnyTLS:
    case ConfigType::Naive:
    case ConfigType::Unknown:
    default:
        return {};
    }
}

void insertTlsOptions(QJsonObject& proxy, const Config& config, const VmessItem& server)
{
    const QString transportSecurity = server.streamSecurity.trimmed();
    if (transportSecurity.isEmpty()
        || (transportSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) != 0
            && transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) != 0
            && !ProtocolConfigMapper::isRealityTransport(transportSecurity))) {
        return;
    }

    proxy.insert(QStringLiteral("tls"), true);
    proxy.insert(
        QStringLiteral("skip-cert-verify"),
        ProtocolConfigMapper::resolveAllowInsecure(server.allowInsecure, config.dns().defaultAllowInsecure));

    const QString serverName = ProtocolConfigMapper::resolveServerName(server).trimmed();
    if (!serverName.isEmpty()) {
        proxy.insert(QStringLiteral("servername"), serverName);
        if (server.configType == ConfigType::Trojan || server.configType == ConfigType::Hysteria2) {
            proxy.insert(QStringLiteral("sni"), serverName);
        }
    }

    const QString fingerprint = ProtocolConfigMapper::isRealityTransport(transportSecurity)
        ? ProtocolConfigMapper::resolveRealityFingerprint(config, server)
        : ProtocolConfigMapper::resolveFingerprint(config, server);
    if (!fingerprint.isEmpty()) {
        proxy.insert(QStringLiteral("client-fingerprint"), fingerprint);
    }

    if (!server.alpn.isEmpty()) {
        QJsonArray alpn;
        appendStringArray(alpn, server.alpn);
        if (!alpn.isEmpty()) {
            proxy.insert(QStringLiteral("alpn"), alpn);
        }
    }

    if (ProtocolConfigMapper::isRealityTransport(transportSecurity)) {
        QJsonObject reality;
        reality.insert(QStringLiteral("public-key"), server.publicKey.trimmed());
        reality.insert(QStringLiteral("short-id"), server.shortId.trimmed());
        proxy.insert(QStringLiteral("reality-opts"), reality);
        proxy.insert(QStringLiteral("skip-cert-verify"), false);
    }
}

void insertTransportOptions(QJsonObject& proxy, const VmessItem& server)
{
    const QString network = server.network.trimmed().isEmpty()
        ? QStringLiteral("tcp")
        : server.network.trimmed().toLower();
    if (network == QStringLiteral("tcp")) {
        return;
    }

    if (network == QStringLiteral("ws")) {
        proxy.insert(QStringLiteral("network"), QStringLiteral("ws"));
        QJsonObject wsOptions;
        if (!server.path.trimmed().isEmpty()) {
            wsOptions.insert(QStringLiteral("path"), server.path.trimmed());
        }
        const QStringList hosts = OutboundSupport::splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonObject headers;
            headers.insert(QStringLiteral("Host"), hosts.constFirst());
            wsOptions.insert(QStringLiteral("headers"), headers);
        }
        if (!wsOptions.isEmpty()) {
            proxy.insert(QStringLiteral("ws-opts"), wsOptions);
        }
        return;
    }

    if (network == QStringLiteral("grpc")) {
        proxy.insert(QStringLiteral("network"), QStringLiteral("grpc"));
        if (!server.path.trimmed().isEmpty()) {
            QJsonObject grpcOptions;
            grpcOptions.insert(QStringLiteral("grpc-service-name"), server.path.trimmed());
            proxy.insert(QStringLiteral("grpc-opts"), grpcOptions);
        }
        return;
    }

    if (network == QStringLiteral("h2")) {
        proxy.insert(QStringLiteral("network"), QStringLiteral("h2"));
        QJsonObject h2Options;
        const QString path = OutboundSupport::resolvePrimaryPath(server.path);
        if (!path.isEmpty()) {
            h2Options.insert(QStringLiteral("path"), path);
        }
        const QStringList hosts = OutboundSupport::splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonArray hostArray;
            appendStringArray(hostArray, hosts);
            h2Options.insert(QStringLiteral("host"), hostArray);
        }
        if (!h2Options.isEmpty()) {
            proxy.insert(QStringLiteral("h2-opts"), h2Options);
        }
    }
}

QStringList dnsServers(const Config& config)
{
    QStringList servers;
    servers.append(OutboundSupport::splitCsv(config.dns().remoteDns));
    servers.append(OutboundSupport::splitCsv(config.dns().directDns));
    servers.append(OutboundSupport::splitCsv(config.dns().bootstrapDns));
    if (servers.isEmpty()) {
        servers.append(QStringLiteral("1.1.1.1"));
    }
    servers.removeDuplicates();
    return servers;
}

QJsonObject buildDns(const Config& config)
{
    QJsonObject dns;
    dns.insert(QStringLiteral("enable"), true);
    dns.insert(QStringLiteral("listen"), QStringLiteral("127.0.0.1:0"));

    QJsonArray nameservers;
    appendStringArray(nameservers, dnsServers(config));
    dns.insert(QStringLiteral("nameserver"), nameservers);

    if (config.dns().fakeIp) {
        dns.insert(QStringLiteral("enhanced-mode"), QStringLiteral("fake-ip"));
    }
    return dns;
}

QJsonObject buildTun(const Config& config)
{
    const TunModeItem& tun = config.tun().tunModeItem;
    QJsonObject tunConfig;
    tunConfig.insert(QStringLiteral("enable"), true);
    tunConfig.insert(QStringLiteral("device"), songbirdTunAdapterName());
    tunConfig.insert(QStringLiteral("stack"), tun.stack.trimmed().isEmpty() ? QStringLiteral("system") : tun.stack.trimmed());
    tunConfig.insert(QStringLiteral("auto-route"), tun.autoRoute);
    tunConfig.insert(QStringLiteral("strict-route"), tun.strictRoute);
    tunConfig.insert(QStringLiteral("mtu"), tun.mtu > 0 ? tun.mtu : 9000);
    tunConfig.insert(QStringLiteral("dns-hijack"), QJsonArray{QStringLiteral("any:53")});
    return tunConfig;
}

QJsonObject buildProxyGroup()
{
    QJsonObject group;
    group.insert(QStringLiteral("name"), kProxyGroupName);
    group.insert(QStringLiteral("type"), QStringLiteral("select"));
    group.insert(QStringLiteral("proxies"), QJsonArray{kPrimaryProxyName});
    return group;
}

void appendDomainRule(QJsonArray& rules, const QString& value, const QString& policy)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (trimmed.startsWith(QStringLiteral("geosite:"), Qt::CaseInsensitive)) {
        rules.append(QStringLiteral("GEOSITE,%1,%2").arg(trimmed.mid(8), policy));
    } else if (trimmed.startsWith(QStringLiteral("domain:"), Qt::CaseInsensitive)) {
        rules.append(QStringLiteral("DOMAIN,%1,%2").arg(trimmed.mid(7), policy));
    } else if (trimmed.startsWith(QStringLiteral("full:"), Qt::CaseInsensitive)) {
        rules.append(QStringLiteral("DOMAIN,%1,%2").arg(trimmed.mid(5), policy));
    } else if (trimmed.startsWith(QStringLiteral("regexp:"), Qt::CaseInsensitive)) {
        rules.append(QStringLiteral("DOMAIN-REGEX,%1,%2").arg(trimmed.mid(7), policy));
    } else if (trimmed.startsWith(QChar('.'))) {
        rules.append(QStringLiteral("DOMAIN-SUFFIX,%1,%2").arg(trimmed.mid(1), policy));
    } else {
        rules.append(QStringLiteral("DOMAIN-KEYWORD,%1,%2").arg(trimmed, policy));
    }
}

void appendIpRule(QJsonArray& rules, const QString& value, const QString& policy)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (trimmed.startsWith(QStringLiteral("geoip:"), Qt::CaseInsensitive)) {
        rules.append(QStringLiteral("GEOIP,%1,%2").arg(trimmed.mid(6).toUpper(), policy));
        return;
    }

    const QString ruleType = trimmed.contains(QChar(':')) ? QStringLiteral("IP-CIDR6") : QStringLiteral("IP-CIDR");
    rules.append(QStringLiteral("%1,%2,%3,no-resolve").arg(ruleType, trimmed, policy));
}

void appendRoutingRules(QJsonArray& rules, const Config& config)
{
    const std::optional<RoutingItem> selectedRouting = RoutingConfigFragments::resolveSelectedRouting(config);
    const RoutingItem* selectedRoutingPtr = selectedRouting.has_value() ? &*selectedRouting : nullptr;
    const QList<RoutingRule> effectiveRules = RoutingConfigFragments::effectiveRoutingRules(config, selectedRoutingPtr);

    for (const RoutingRule& rule : effectiveRules) {
        const QString policy = clashPolicyName(rule.outboundTag);
        for (const QString& domain : rule.domain) {
            appendDomainRule(rules, domain, policy);
        }
        for (const QString& ip : rule.ip) {
            appendIpRule(rules, ip, policy);
        }
        for (const QString& process : rule.process) {
            const QString trimmed = process.trimmed();
            if (!trimmed.isEmpty() && !trimmed.contains(QChar('/')) && !trimmed.contains(QChar('\\'))) {
                rules.append(QStringLiteral("PROCESS-NAME,%1,%2").arg(trimmed, policy));
            }
        }
        for (const QString& port : OutboundSupport::splitCsv(rule.port)) {
            rules.append(QStringLiteral("DST-PORT,%1,%2").arg(port, policy));
        }
    }

    rules.append(QStringLiteral("MATCH,%1").arg(kProxyGroupName));
}

} // namespace

namespace MihomoConfigFragments {

QJsonObject buildClientRoot(const Config& config, const VmessItem& server)
{
    QJsonObject root;
    root.insert(QStringLiteral("log-level"), normalizedLogLevel(config.logLevel));
    root.insert(QStringLiteral("mode"), QStringLiteral("rule"));
    root.insert(QStringLiteral("allow-lan"), config.allowLanConnection);
    root.insert(QStringLiteral("bind-address"), config.allowLanConnection ? QStringLiteral("*") : QStringLiteral("127.0.0.1"));
    root.insert(QStringLiteral("socks-port"), config.localPort);
    root.insert(QStringLiteral("port"), httpPort(config));

    if (!config.inboundUser.trimmed().isEmpty() && !config.inboundPassword.trimmed().isEmpty()) {
        root.insert(
            QStringLiteral("authentication"),
            QJsonArray{QStringLiteral("%1:%2").arg(config.inboundUser, config.inboundPassword)});
    }

    const int probePort = locationProbePort(config);
    if (probePort != httpPort(config)) {
        QJsonObject listener;
        listener.insert(QStringLiteral("name"), QStringLiteral("location-probe"));
        listener.insert(QStringLiteral("type"), QStringLiteral("http"));
        listener.insert(QStringLiteral("listen"), QStringLiteral("127.0.0.1"));
        listener.insert(QStringLiteral("port"), probePort);
        root.insert(QStringLiteral("listeners"), QJsonArray{listener});
    }

    if (config.tun().tunModeItem.enableTun) {
        root.insert(QStringLiteral("tun"), buildTun(config));
        root.insert(QStringLiteral("dns"), buildDns(config));
    }

    root.insert(QStringLiteral("proxies"), QJsonArray{buildPrimaryProxy(config, server, kPrimaryProxyName)});
    root.insert(QStringLiteral("proxy-groups"), QJsonArray{buildProxyGroup()});

    QJsonArray rules;
    appendRoutingRules(rules, config);
    root.insert(QStringLiteral("rules"), rules);
    return root;
}

QJsonObject buildPrimaryProxy(const Config& config, const VmessItem& server, const QString& name)
{
    QJsonObject proxy;
    proxy.insert(QStringLiteral("name"), name);
    proxy.insert(QStringLiteral("type"), normalizeClashType(server.configType));
    proxy.insert(QStringLiteral("server"), server.address);
    proxy.insert(QStringLiteral("port"), server.port);
    proxy.insert(QStringLiteral("udp"), config.udpEnabled);

    switch (server.configType) {
    case ConfigType::VMess:
        proxy.insert(QStringLiteral("uuid"), server.id);
        proxy.insert(QStringLiteral("alterId"), server.alterId);
        proxy.insert(QStringLiteral("cipher"), server.security.trimmed().isEmpty() ? QStringLiteral("auto") : server.security.trimmed());
        break;
    case ConfigType::VLESS:
        proxy.insert(QStringLiteral("uuid"), server.id);
        if (!server.flow.trimmed().isEmpty()) {
            proxy.insert(QStringLiteral("flow"), server.flow.trimmed());
        }
        break;
    case ConfigType::Trojan:
        proxy.insert(QStringLiteral("password"), server.id);
        break;
    case ConfigType::Shadowsocks:
        proxy.insert(QStringLiteral("cipher"), server.security.trimmed().isEmpty() ? QStringLiteral("aes-128-gcm") : server.security.trimmed());
        proxy.insert(QStringLiteral("password"), server.id);
        break;
    case ConfigType::Socks:
        if (!server.id.trimmed().isEmpty()) {
            proxy.insert(QStringLiteral("username"), server.id.trimmed());
        }
        if (!server.security.trimmed().isEmpty()) {
            proxy.insert(QStringLiteral("password"), server.security.trimmed());
        }
        break;
    case ConfigType::HTTP:
        if (!server.id.trimmed().isEmpty()) {
            proxy.insert(QStringLiteral("username"), server.id.trimmed());
        }
        if (!server.security.trimmed().isEmpty()) {
            proxy.insert(QStringLiteral("password"), server.security.trimmed());
        }
        break;
    case ConfigType::Hysteria2:
        proxy.insert(QStringLiteral("password"), server.id);
        if (!server.obfsPassword.trimmed().isEmpty()) {
            proxy.insert(QStringLiteral("obfs"), QStringLiteral("salamander"));
            proxy.insert(QStringLiteral("obfs-password"), server.obfsPassword.trimmed());
        }
        break;
    case ConfigType::Custom:
    case ConfigType::TUIC:
    case ConfigType::WireGuard:
    case ConfigType::AnyTLS:
    case ConfigType::Naive:
    case ConfigType::Unknown:
    default:
        break;
    }

    insertTlsOptions(proxy, config, server);
    insertTransportOptions(proxy, server);
    return proxy;
}

} // namespace MihomoConfigFragments
