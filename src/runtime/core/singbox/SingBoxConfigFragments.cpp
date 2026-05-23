#include "runtime/core/singbox/SingBoxConfigFragments.h"

#include <QJsonValue>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

#include "runtime/ProtocolConfigMapper.h"

namespace {

const QString kLocationProbeTag = QStringLiteral("location-probe");
const QString kPemBeginMarker = QStringLiteral("-----BEGIN CERTIFICATE-----");
const QString kPemEndMarker = QStringLiteral("-----END CERTIFICATE-----");

QStringList splitCsv(const QString& value)
{
    QStringList result;
    const QStringList parts = value.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }

    return result;
}

QString resolvePrimaryPath(const QString& value)
{
    const QStringList paths = splitCsv(value);
    return paths.isEmpty() ? QStringLiteral("/") : paths.constFirst();
}

bool resolveMuxEnabled(const Config& config, const VmessItem& server)
{
    return server.muxEnabled.value_or(config.muxEnabled);
}

void insertPositiveIntOrString(QJsonObject& object, const QString& key, const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    bool ok = false;
    const int number = trimmed.toInt(&ok);
    object.insert(key, ok && number > 0 ? QJsonValue(number) : QJsonValue(trimmed));
}

QStringList splitPemCertificates(const QString& value)
{
    QString text = value;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar('\r'), QChar('\n'));

    QStringList certificates;
    int searchFrom = 0;
    while (true) {
        const int begin = text.indexOf(kPemBeginMarker, searchFrom);
        if (begin < 0) {
            break;
        }

        const int end = text.indexOf(kPemEndMarker, begin);
        if (end < 0) {
            break;
        }

        const int afterEnd = end + kPemEndMarker.size();
        const QString certificate = text.mid(begin, afterEnd - begin).trimmed();
        if (!certificate.isEmpty()) {
            certificates.append(certificate);
        }
        searchFrom = afterEnd;
    }

    return certificates;
}

QStringList splitPinnedCertificateHashes(const QString& value)
{
    QStringList hashes;
    const QStringList parts = value.split(QChar(','), Qt::SkipEmptyParts);
    for (QString part : parts) {
        part = part.trimmed();
        if (part.startsWith(QStringLiteral("sha256/"), Qt::CaseInsensitive)) {
            part = part.mid(QStringLiteral("sha256/").size());
        }
        if (!part.isEmpty()) {
            hashes.append(part);
        }
    }
    return hashes;
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

    if (allowLan && !config.inboundUser.trimmed().isEmpty() && !config.inboundPassword.trimmed().isEmpty()) {
        QJsonArray users;
        QJsonObject user;
        user.insert(QStringLiteral("username"), config.inboundUser);
        user.insert(QStringLiteral("password"), config.inboundPassword);
        users.append(user);
        inbound.insert(QStringLiteral("users"), users);
    }

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

    if (allowLan && !config.inboundUser.trimmed().isEmpty() && !config.inboundPassword.trimmed().isEmpty()) {
        QJsonArray users;
        QJsonObject user;
        user.insert(QStringLiteral("username"), config.inboundUser);
        user.insert(QStringLiteral("password"), config.inboundPassword);
        users.append(user);
        inbound.insert(QStringLiteral("users"), users);
    }

    return inbound;
}

QJsonObject SingBoxConfigFragments::buildPrimaryOutbound(const Config& config, const VmessItem& server)
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("proxy"));
    outbound.insert(QStringLiteral("type"), ProtocolConfigMapper::resolveSingBoxOutboundType(server.configType));
    outbound.insert(QStringLiteral("server"), server.address);
    outbound.insert(QStringLiteral("server_port"), server.port);

    const bool needsNetworkField = server.configType != ConfigType::Hysteria2
        && server.configType != ConfigType::TUIC
        && server.configType != ConfigType::WireGuard
        && server.configType != ConfigType::AnyTLS
        && server.configType != ConfigType::HTTP;
    if (needsNetworkField) {
        outbound.insert(QStringLiteral("network"), ProtocolConfigMapper::resolveSingBoxNetwork(server));
    }

    switch (server.configType) {
    case ConfigType::VMess:
        outbound.insert(QStringLiteral("uuid"), server.id);
        outbound.insert(QStringLiteral("security"), server.security.trimmed().isEmpty() ? QStringLiteral("auto") : server.security);
        break;
    case ConfigType::VLESS:
        outbound.insert(QStringLiteral("uuid"), server.id);
        if (!server.flow.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("flow"), server.flow);
        }
        if (!server.packetEncoding.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("packet_encoding"), server.packetEncoding.trimmed());
        }
        break;
    case ConfigType::Trojan:
        outbound.insert(QStringLiteral("password"), server.id);
        break;
    case ConfigType::Shadowsocks:
        outbound.insert(QStringLiteral("method"), server.security.trimmed().isEmpty() ? QStringLiteral("aes-128-gcm") : server.security);
        outbound.insert(QStringLiteral("password"), server.id);
        break;
    case ConfigType::Socks:
        if (!server.id.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("username"), server.id);
        }
        if (!server.id.trimmed().isEmpty() && !server.security.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("password"), server.security);
        }
        break;
    case ConfigType::HTTP: {
        if (!server.id.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("username"), server.id);
        }
        if (!server.security.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("password"), server.security);
        }
        const QString path = server.path.trimmed();
        if (!path.isEmpty()) {
            outbound.insert(QStringLiteral("path"), path);
        }
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonObject headers;
            headers.insert(QStringLiteral("Host"), hosts.constFirst());
            outbound.insert(QStringLiteral("headers"), headers);
        }
        break;
    }
    case ConfigType::Hysteria2:
        outbound.insert(QStringLiteral("password"), server.id);
        if (!server.obfsPassword.trimmed().isEmpty()) {
            QJsonObject obfs;
            obfs.insert(QStringLiteral("type"), QStringLiteral("salamander"));
            obfs.insert(QStringLiteral("password"), server.obfsPassword);
            outbound.insert(QStringLiteral("obfs"), obfs);
        }
        if (!server.upMbps.trimmed().isEmpty()) {
            bool okUp = false;
            const int up = server.upMbps.toInt(&okUp);
            if (okUp && up > 0) {
                outbound.insert(QStringLiteral("up_mbps"), up);
            }
        }
        if (!server.downMbps.trimmed().isEmpty()) {
            bool okDown = false;
            const int down = server.downMbps.toInt(&okDown);
            if (okDown && down > 0) {
                outbound.insert(QStringLiteral("down_mbps"), down);
            }
        }
        break;
    case ConfigType::TUIC:
        outbound.insert(QStringLiteral("uuid"), server.id);
        if (!server.security.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("password"), server.security);
        }
        if (!server.congestionControl.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("congestion_control"), server.congestionControl);
        }
        if (!server.udpRelayMode.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("udp_relay_mode"), server.udpRelayMode);
        }
        if (server.zeroRttHandshake) {
            outbound.insert(QStringLiteral("zero_rtt_handshake"), true);
        }
        break;
    case ConfigType::WireGuard: {
        if (!server.privateKey.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("private_key"), server.privateKey);
        }
        if (!server.peerPublicKey.trimmed().isEmpty()) {
            QJsonArray peersArr;
            QJsonObject peerObj;
            peerObj.insert(QStringLiteral("public_key"), server.peerPublicKey);
            if (!server.reserved.trimmed().isEmpty()) {
                QJsonArray reservedArr;
                const QStringList parts = server.reserved.split(QChar(','), Qt::SkipEmptyParts);
                for (const QString& part : parts) {
                    bool okR = false;
                    const int val = part.trimmed().toInt(&okR);
                    if (okR) {
                        reservedArr.append(val);
                    }
                }
                if (!reservedArr.isEmpty()) {
                    peerObj.insert(QStringLiteral("reserved"), reservedArr);
                }
            }
            peersArr.append(peerObj);
            outbound.insert(QStringLiteral("peers"), peersArr);
        }
        if (!server.localAddress.trimmed().isEmpty()) {
            const QStringList addresses = server.localAddress.split(QChar(','), Qt::SkipEmptyParts);
            QJsonArray addrArr;
            for (const QString& addr : addresses) {
                addrArr.append(addr.trimmed());
            }
            outbound.insert(QStringLiteral("local_address"), addrArr);
        }
        if (server.wireguardMtu > 0) {
            outbound.insert(QStringLiteral("mtu"), server.wireguardMtu);
        }
        break;
    }
    case ConfigType::AnyTLS:
        outbound.insert(QStringLiteral("password"), server.id);
        insertPositiveIntOrString(
            outbound,
            QStringLiteral("idle_session_check_interval"),
            server.idleSessionCheckInterval);
        insertPositiveIntOrString(outbound, QStringLiteral("idle_session_timeout"), server.idleSessionTimeout);
        insertPositiveIntOrString(outbound, QStringLiteral("min_idle_session"), server.minIdleSession);
        break;
    case ConfigType::Naive:
        if (!server.username.trimmed().isEmpty()) {
            outbound.insert(QStringLiteral("username"), server.username);
        }
        outbound.insert(QStringLiteral("password"), server.id);
        if (server.naiveQuic) {
            outbound.insert(QStringLiteral("quic"), true);
            if (!server.congestionControl.trimmed().isEmpty()) {
                outbound.insert(QStringLiteral("quic_congestion_control"), server.congestionControl);
            }
        }
        if (server.insecureConcurrency > 0) {
            outbound.insert(QStringLiteral("insecure_concurrency"), server.insecureConcurrency);
        }
        break;
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        break;
    }

    const QString singBoxMuxProtocol = config.mux4SboxProtocol.trimmed();
    if (resolveMuxEnabled(config, server) && !singBoxMuxProtocol.isEmpty()) {
        const bool supportsMultiplex = server.configType == ConfigType::VMess
            || server.configType == ConfigType::Trojan
            || server.configType == ConfigType::Shadowsocks
            || (server.configType == ConfigType::VLESS && server.flow.trimmed().isEmpty());
        if (supportsMultiplex) {
            QJsonObject multiplex;
            multiplex.insert(QStringLiteral("enabled"), true);
            multiplex.insert(QStringLiteral("protocol"), singBoxMuxProtocol);
            multiplex.insert(
                QStringLiteral("max_connections"),
                config.mux4SboxMaxConnections > 0 ? config.mux4SboxMaxConnections : 8);
            if (config.mux4SboxPadding.has_value()) {
                multiplex.insert(QStringLiteral("padding"), config.mux4SboxPadding.value());
            }
            outbound.insert(QStringLiteral("multiplex"), multiplex);
        }
    }

    const bool needsTls = server.configType != ConfigType::WireGuard;
    if (needsTls) {
        const QJsonObject tls = buildTls(config, server);
        if (!tls.isEmpty()) {
            outbound.insert(QStringLiteral("tls"), tls);
        }
    }

    const bool needsTransport = server.configType != ConfigType::Hysteria2
        && server.configType != ConfigType::TUIC
        && server.configType != ConfigType::WireGuard
        && server.configType != ConfigType::HTTP;
    if (needsTransport) {
        const QJsonObject transport = buildTransport(server);
        if (!transport.isEmpty()) {
            outbound.insert(QStringLiteral("transport"), transport);
        }
    }

    return outbound;
}

QJsonObject SingBoxConfigFragments::buildTls(const Config& config, const VmessItem& server)
{
    const QString transportSecurity = server.streamSecurity.trimmed();
    if (transportSecurity.isEmpty()
        || (transportSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) != 0
            && transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) != 0
            && !ProtocolConfigMapper::isRealityTransport(transportSecurity))) {
        return {};
    }

    QJsonObject tls;
    tls.insert(QStringLiteral("enabled"), true);
    tls.insert(
        QStringLiteral("insecure"),
        ProtocolConfigMapper::resolveAllowInsecure(server.allowInsecure, config.dns().defaultAllowInsecure));

    const QString serverName = ProtocolConfigMapper::resolveServerName(server).trimmed();
    if (!serverName.isEmpty()) {
        tls.insert(QStringLiteral("server_name"), serverName);
    }

    if (!server.alpn.isEmpty()) {
        QJsonArray alpnArray;
        for (const QString& item : server.alpn) {
            if (!item.trimmed().isEmpty()) {
                alpnArray.append(item.trimmed());
            }
        }
        if (!alpnArray.isEmpty()) {
            tls.insert(QStringLiteral("alpn"), alpnArray);
        }
    }

    const QStringList certificates = splitPemCertificates(server.cert);
    if (!certificates.isEmpty() && !ProtocolConfigMapper::isRealityTransport(transportSecurity)) {
        QJsonArray certificateArray;
        for (const QString& certificate : certificates) {
            certificateArray.append(certificate);
        }
        tls.insert(QStringLiteral("certificate"), certificateArray);
        tls.insert(QStringLiteral("insecure"), false);
    } else if (!ProtocolConfigMapper::isRealityTransport(transportSecurity)) {
        const QStringList pinnedHashes = splitPinnedCertificateHashes(server.certSha);
        if (!pinnedHashes.isEmpty()) {
            QJsonArray pinnedHashArray;
            for (const QString& hash : pinnedHashes) {
                pinnedHashArray.append(hash);
            }
            tls.insert(QStringLiteral("certificate_public_key_sha256"), pinnedHashArray);
            tls.insert(QStringLiteral("insecure"), false);
        }
    }

    if (ProtocolConfigMapper::isRealityTransport(transportSecurity)) {
        QJsonObject utls;
        utls.insert(QStringLiteral("enabled"), true);
        utls.insert(QStringLiteral("fingerprint"), ProtocolConfigMapper::resolveRealityFingerprint(config, server));
        tls.insert(QStringLiteral("utls"), utls);

        QJsonObject reality;
        reality.insert(QStringLiteral("enabled"), true);
        reality.insert(QStringLiteral("public_key"), server.publicKey.trimmed());
        reality.insert(QStringLiteral("short_id"), server.shortId.trimmed());
        tls.insert(QStringLiteral("reality"), reality);
        tls.insert(QStringLiteral("insecure"), false);
    } else {
        const QString fingerprint = ProtocolConfigMapper::resolveFingerprint(config, server);
        if (!fingerprint.isEmpty()) {
            QJsonObject utls;
            utls.insert(QStringLiteral("enabled"), true);
            utls.insert(QStringLiteral("fingerprint"), fingerprint);
            tls.insert(QStringLiteral("utls"), utls);
        }
    }

    return tls;
}

QJsonObject SingBoxConfigFragments::buildTransport(const VmessItem& server)
{
    const QString network = server.network.trimmed().isEmpty() ? QStringLiteral("tcp") : server.network.trimmed();
    QJsonObject transport;

    if (network.compare(QStringLiteral("ws"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("ws"));
        QString wsPath = server.path.trimmed();
        if (!wsPath.isEmpty()) {
            const int querySeparator = wsPath.indexOf(QLatin1Char('?'));
            if (querySeparator >= 0) {
                const QString basePath = wsPath.left(querySeparator);
                QUrlQuery query(wsPath.mid(querySeparator + 1));

                bool hasEarlyData = false;
                const int maxEarlyData = query.queryItemValue(QStringLiteral("ed")).toInt(&hasEarlyData);
                if (hasEarlyData) {
                    transport.insert(QStringLiteral("max_early_data"), maxEarlyData);
                    transport.insert(QStringLiteral("early_data_header_name"), QStringLiteral("Sec-WebSocket-Protocol"));
                    query.removeAllQueryItems(QStringLiteral("ed"));
                }

                const QString earlyDataHeader = query.queryItemValue(QStringLiteral("eh"));
                if (!earlyDataHeader.isEmpty()) {
                    transport.insert(QStringLiteral("early_data_header_name"), earlyDataHeader);
                }

                wsPath = basePath;
                const QString encodedQuery = query.toString(QUrl::FullyEncoded);
                if (!encodedQuery.isEmpty()) {
                    wsPath += QStringLiteral("?") + encodedQuery;
                }
            }
        }

        if (!wsPath.isEmpty()) {
            transport.insert(QStringLiteral("path"), wsPath);
        }

        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonObject headers;
            headers.insert(QStringLiteral("Host"), hosts.constFirst());
            transport.insert(QStringLiteral("headers"), headers);
        }

        return transport;
    }

    if (network.compare(QStringLiteral("kcp"), Qt::CaseInsensitive) == 0) {
        return {};
    }

    if (network.compare(QStringLiteral("quic"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("quic"));
        return transport;
    }

    if (network.compare(QStringLiteral("grpc"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("grpc"));
        if (!server.path.trimmed().isEmpty()) {
            transport.insert(QStringLiteral("service_name"), server.path.trimmed());
        }
        return transport;
    }

    if (network.compare(QStringLiteral("httpupgrade"), Qt::CaseInsensitive) == 0) {
        transport.insert(QStringLiteral("type"), QStringLiteral("httpupgrade"));
        if (!server.path.trimmed().isEmpty()) {
            transport.insert(QStringLiteral("path"), server.path.trimmed());
        }
        if (!server.requestHost.trimmed().isEmpty()) {
            transport.insert(QStringLiteral("host"), server.requestHost.trimmed());
        }
        return transport;
    }

    if (network.compare(QStringLiteral("h2"), Qt::CaseInsensitive) == 0
        || (network.compare(QStringLiteral("tcp"), Qt::CaseInsensitive) == 0
            && server.headerType.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0)) {
        transport.insert(QStringLiteral("type"), QStringLiteral("http"));

        const QString path = resolvePrimaryPath(server.path);
        if (!path.isEmpty()) {
            transport.insert(QStringLiteral("path"), path);
        }

        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonArray hostArray;
            for (const QString& host : hosts) {
                hostArray.append(host);
            }
            transport.insert(QStringLiteral("host"), hostArray);
        }

        return transport;
    }

    return {};
}

QJsonObject SingBoxConfigFragments::buildSocksOutbound(
    const QString& tag,
    const QString& server,
    int port,
    bool udpOverTcp)
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), tag);
    outbound.insert(QStringLiteral("type"), QStringLiteral("socks"));
    outbound.insert(QStringLiteral("server"), server);
    outbound.insert(QStringLiteral("server_port"), port);
    if (udpOverTcp) {
        outbound.insert(QStringLiteral("udp_over_tcp"), true);
    }
    return outbound;
}

QJsonObject SingBoxConfigFragments::buildTunCompatDns()
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

QJsonObject SingBoxConfigFragments::buildTunCompatRoute(const Config& config)
{
    QJsonObject route;
    route.insert(QStringLiteral("final"), QStringLiteral("proxy"));
    route.insert(QStringLiteral("auto_detect_interface"), true);

    QJsonArray rules = buildTunCompatRejectRules();
    appendTunCompatProcessRules(rules);
    appendTunIcmpRouteRule(rules, config.tun().tunModeItem);
    appendSniffRules(rules, config);
    appendTunUdpRouteRule(rules, config.tun().tunModeItem);

    route.insert(QStringLiteral("rules"), rules);
    return route;
}

QJsonArray SingBoxConfigFragments::buildTunCompatOutbounds(const Config& config)
{
    QJsonArray outbounds;
    outbounds.append(buildSocksOutbound(
        QStringLiteral("proxy"),
        QStringLiteral("127.0.0.1"),
        config.localPort));
    outbounds.append(buildDirectOutbound());
    outbounds.append(buildBlockOutbound());
    return outbounds;
}

QJsonArray SingBoxConfigFragments::buildTunCompatRejectRules()
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

void SingBoxConfigFragments::appendTunCompatProcessRules(QJsonArray& rules)
{
    QJsonObject dnsHijackRule;
    dnsHijackRule.insert(QStringLiteral("port"), QJsonArray{53});
    dnsHijackRule.insert(QStringLiteral("action"), QStringLiteral("hijack-dns"));
    QJsonArray dnsProcessNames;
    for (const QString& processName : buildTunCompatDnsProcessNames()) {
        dnsProcessNames.append(processName);
    }
    dnsHijackRule.insert(QStringLiteral("process_name"), dnsProcessNames);
    rules.append(dnsHijackRule);

    QJsonObject directProcessRule;
    directProcessRule.insert(QStringLiteral("outbound"), QStringLiteral("direct"));
    QJsonArray processNames;
    for (const QString& processName : buildTunCompatDirectProcessNames()) {
        processNames.append(processName);
    }
    directProcessRule.insert(QStringLiteral("process_name"), processNames);
    rules.append(directProcessRule);
}

void SingBoxConfigFragments::appendTunIcmpRouteRule(QJsonArray& rules, const TunModeItem& tun)
{
    const QString policy = tun.icmpRouting.trimmed().toLower();
    const QStringList supportedPolicies{
        QStringLiteral("rule"),
        QStringLiteral("direct"),
        QStringLiteral("unreachable"),
        QStringLiteral("drop"),
        QStringLiteral("reply")};
    if (policy.isEmpty()
        || !supportedPolicies.contains(policy)
        || policy == QStringLiteral("rule")) {
        return;
    }

    QJsonObject rule;
    rule.insert(QStringLiteral("network"), QJsonArray{QStringLiteral("icmp")});

    if (policy == QStringLiteral("direct")) {
        rule.insert(QStringLiteral("outbound"), QStringLiteral("direct"));
        rules.append(rule);
        return;
    }

    rule.insert(QStringLiteral("action"), QStringLiteral("reject"));
    rule.insert(
        QStringLiteral("method"),
        policy == QStringLiteral("drop")
            ? QStringLiteral("drop")
            : policy == QStringLiteral("unreachable")
                ? QStringLiteral("default")
                : QStringLiteral("reply"));
    rules.append(rule);
}

void SingBoxConfigFragments::appendTunUdpRouteRule(QJsonArray& rules, const TunModeItem& tun)
{
    const QString policy = tun.udpRouting.trimmed().toLower();
    const QStringList supportedPolicies{
        QStringLiteral("rule"),
        QStringLiteral("direct"),
        QStringLiteral("unreachable"),
        QStringLiteral("drop"),
        QStringLiteral("reply")};
    if (policy.isEmpty()
        || !supportedPolicies.contains(policy)
        || policy == QStringLiteral("rule")) {
        return;
    }

    QJsonObject rule;
    rule.insert(QStringLiteral("network"), QJsonArray{QStringLiteral("udp")});

    if (policy == QStringLiteral("direct")) {
        rule.insert(QStringLiteral("outbound"), QStringLiteral("direct"));
        rules.append(rule);
        return;
    }

    rule.insert(QStringLiteral("action"), QStringLiteral("reject"));
    rule.insert(
        QStringLiteral("method"),
        policy == QStringLiteral("drop")
            ? QStringLiteral("drop")
            : policy == QStringLiteral("unreachable")
                ? QStringLiteral("default")
                : QStringLiteral("reply"));
    rules.append(rule);
}

void SingBoxConfigFragments::appendSniffRules(QJsonArray& rules, const Config& config)
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

QStringList SingBoxConfigFragments::buildTunCompatDnsProcessNames()
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

QStringList SingBoxConfigFragments::buildTunCompatDirectProcessNames()
{
    QStringList processNames{QStringLiteral("SongBird.exe")};
    processNames.append(buildTunCompatDnsProcessNames());
    processNames.append(QStringLiteral("sing-box-client.exe"));
    processNames.append(QStringLiteral("sing-box.exe"));
    return processNames;
}
