#include "backends/singbox/SingBoxOutboundConfigFragments.h"

#include <QJsonArray>

#include "backends/singbox/SingBoxOutboundConfigSupport.h"
#include "backends/singbox/SingBoxTlsConfigFragments.h"
#include "backends/singbox/SingBoxTransportConfigFragments.h"
#include "runtime/ProtocolConfigMapper.h"

namespace OutboundSupport = SingBoxOutboundConfigSupport;

QJsonObject SingBoxOutboundConfigFragments::buildPrimaryOutbound(const Config& config, const VmessItem& server)
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
        const QStringList hosts = OutboundSupport::splitCsv(server.requestHost);
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
        OutboundSupport::insertPositiveIntOrString(
            outbound,
            QStringLiteral("idle_session_check_interval"),
            server.idleSessionCheckInterval);
        OutboundSupport::insertPositiveIntOrString(outbound, QStringLiteral("idle_session_timeout"), server.idleSessionTimeout);
        OutboundSupport::insertPositiveIntOrString(outbound, QStringLiteral("min_idle_session"), server.minIdleSession);
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
    if (OutboundSupport::resolveMuxEnabled(config, server) && !singBoxMuxProtocol.isEmpty()) {
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

QJsonObject SingBoxOutboundConfigFragments::buildTls(const Config& config, const VmessItem& server)
{
    return SingBoxTlsConfigFragments::buildTls(config, server);
}

QJsonObject SingBoxOutboundConfigFragments::buildTransport(const VmessItem& server)
{
    return SingBoxTransportConfigFragments::buildTransport(server);
}

QJsonObject SingBoxOutboundConfigFragments::buildSocksOutbound(
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
