#include "backends/singbox/SingBoxConfigFragments.h"

#include "backends/singbox/SingBoxOutboundConfigFragments.h"
#include "backends/singbox/SingBoxTunCompatConfigFragments.h"

namespace TunCompatConfig = SingBoxTunCompatConfigFragments;

namespace {

const QString kLocationProbeTag = QStringLiteral("location-probe");

void insertAllowLanUsers(QJsonObject& inbound, const Config& config, bool allowLan)
{
    if (!allowLan || config.inboundUser.trimmed().isEmpty() || config.inboundPassword.trimmed().isEmpty()) {
        return;
    }

    QJsonArray users;
    QJsonObject user;
    user.insert(QStringLiteral("username"), config.inboundUser);
    user.insert(QStringLiteral("password"), config.inboundPassword);
    users.append(user);
    inbound.insert(QStringLiteral("users"), users);
}

} // namespace

QJsonObject SingBoxConfigFragments::buildDirectOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("direct"));
    outbound.insert(QStringLiteral("type"), QStringLiteral("direct"));
    return outbound;
}

QJsonObject SingBoxConfigFragments::buildBlockOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("block"));
    outbound.insert(QStringLiteral("type"), QStringLiteral("block"));
    return outbound;
}

QJsonObject SingBoxConfigFragments::buildTunInbound(const Config& config)
{
    const TunModeItem& tun = config.tun().tunModeItem;
    QJsonObject inbound;
    inbound.insert(QStringLiteral("type"), QStringLiteral("tun"));
    inbound.insert(QStringLiteral("tag"), QStringLiteral("tun-in"));
    inbound.insert(QStringLiteral("interface_name"), QStringLiteral("singbox_tun"));

    QJsonArray addresses;
    if (tun.enableIPv6Address) {
        addresses.append(QStringLiteral("fdfe:dcba:9876::1/126"));
    }
    addresses.append(QStringLiteral("172.18.0.1/30"));
    inbound.insert(QStringLiteral("address"), addresses);

    inbound.insert(QStringLiteral("mtu"), tun.mtu > 0 ? tun.mtu : 9000);
    inbound.insert(QStringLiteral("auto_route"), tun.autoRoute);
    inbound.insert(QStringLiteral("strict_route"), tun.strictRoute);
    inbound.insert(QStringLiteral("stack"), tun.stack.isEmpty() ? QStringLiteral("system") : tun.stack);

    return inbound;
}

QJsonObject SingBoxConfigFragments::buildSocksInbound(const Config& config, bool allowLan, int offset)
{
    QJsonObject inbound;
    inbound.insert(QStringLiteral("type"), QStringLiteral("socks"));
    inbound.insert(QStringLiteral("tag"), allowLan ? QStringLiteral("socks-lan") : QStringLiteral("socks"));
    inbound.insert(QStringLiteral("listen"), allowLan ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1"));
    inbound.insert(QStringLiteral("listen_port"), config.localPort + offset);

    insertAllowLanUsers(inbound, config, allowLan);

    return inbound;
}

QJsonObject SingBoxConfigFragments::buildHttpInbound(const Config& config, bool allowLan, int offset)
{
    return buildHttpInboundWithTag(
        config,
        allowLan ? QStringLiteral("http-lan") : QStringLiteral("http"),
        offset);
}

QJsonObject SingBoxConfigFragments::buildHttpInboundWithTag(const Config& config, const QString& tag, int offset)
{
    QJsonObject inbound;
    const bool allowLan = tag.endsWith(QStringLiteral("-lan"));
    int port = config.localPort + offset;
    if (!allowLan && offset == 1 && config.localHttpPort > 0) {
        port = config.localHttpPort;
    } else if (tag == kLocationProbeTag && config.localLocationProbePort > 0) {
        port = config.localLocationProbePort;
    }
    inbound.insert(QStringLiteral("type"), QStringLiteral("http"));
    inbound.insert(QStringLiteral("tag"), tag);
    inbound.insert(QStringLiteral("listen"), allowLan ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1"));
    inbound.insert(QStringLiteral("listen_port"), port);

    insertAllowLanUsers(inbound, config, allowLan);

    return inbound;
}

QJsonObject SingBoxConfigFragments::buildPrimaryOutbound(const Config& config, const VmessItem& server)
{
    return SingBoxOutboundConfigFragments::buildPrimaryOutbound(config, server);
}

QJsonObject SingBoxConfigFragments::buildTls(const Config& config, const VmessItem& server)
{
    return SingBoxOutboundConfigFragments::buildTls(config, server);
}

QJsonObject SingBoxConfigFragments::buildTransport(const VmessItem& server)
{
    return SingBoxOutboundConfigFragments::buildTransport(server);
}

QJsonObject SingBoxConfigFragments::buildSocksOutbound(
    const QString& tag,
    const QString& server,
    int port,
    bool udpOverTcp)
{
    return SingBoxOutboundConfigFragments::buildSocksOutbound(tag, server, port, udpOverTcp);
}

QJsonObject SingBoxConfigFragments::buildTunCompatDns()
{
    return TunCompatConfig::buildDns();
}

QJsonObject SingBoxConfigFragments::buildTunCompatRoute(const Config& config)
{
    return TunCompatConfig::buildRoute(config);
}

QJsonArray SingBoxConfigFragments::buildTunCompatOutbounds(const Config& config)
{
    return TunCompatConfig::buildOutbounds(config);
}

QJsonArray SingBoxConfigFragments::buildTunCompatRejectRules()
{
    return TunCompatConfig::buildRejectRules();
}

void SingBoxConfigFragments::appendTunCompatProcessRules(QJsonArray& rules)
{
    TunCompatConfig::appendProcessRules(rules);
}

void SingBoxConfigFragments::appendTunIcmpRouteRule(QJsonArray& rules, const TunModeItem& tun)
{
    TunCompatConfig::appendIcmpRouteRule(rules, tun);
}

void SingBoxConfigFragments::appendTunUdpRouteRule(QJsonArray& rules, const TunModeItem& tun)
{
    TunCompatConfig::appendUdpRouteRule(rules, tun);
}

void SingBoxConfigFragments::appendSniffRules(QJsonArray& rules, const Config& config)
{
    TunCompatConfig::appendSniffRules(rules, config);
}

QStringList SingBoxConfigFragments::buildTunCompatDnsProcessNames()
{
    return TunCompatConfig::buildDnsProcessNames();
}

QStringList SingBoxConfigFragments::buildTunCompatDirectProcessNames()
{
    return TunCompatConfig::buildDirectProcessNames();
}
