#include "subscription/SubscriptionSingBoxParser.h"

#include "common/PortValidator.h"
#include "subscription/SubscriptionJsonSupport.h"

using SubscriptionJsonSupport::compactJson;
using SubscriptionJsonSupport::firstNonEmpty;
using SubscriptionJsonSupport::joinStringList;
using SubscriptionJsonSupport::parsePortValue;
using SubscriptionJsonSupport::stringListValue;

QList<VmessItem> SubscriptionSingBoxParser::parseOutboundObject(const QJsonObject& object)
{
    QList<VmessItem> items;
    const QString type = object.value(QStringLiteral("type")).toString().trimmed().toLower();
    if (type.isEmpty()) {
        return items;
    }

    VmessItem item;
    item.coreType = CoreType::SingBox;
    item.remarks = firstNonEmpty(object, {"tag", "name"});
    item.address = firstNonEmpty(object, {"server"});
    parsePortValue(object.value(QStringLiteral("server_port")), &item.port);
    if (item.port <= 0) {
        parsePortValue(object.value(QStringLiteral("server-port")), &item.port);
    }
    if (item.port <= 0) {
        parsePortValue(object.value(QStringLiteral("port")), &item.port);
    }

    if (type == QStringLiteral("vmess")) {
        item.configType = ConfigType::VMess;
        item.id = firstNonEmpty(object, {"uuid", "id"});
        item.security = firstNonEmpty(object, {"security"});
        if (item.security.isEmpty()) {
            item.security = QStringLiteral("auto");
        }
        item.alterId = firstNonEmpty(object, {"alter_id", "alterId"}).toInt();
    } else if (type == QStringLiteral("vless")) {
        item.configType = ConfigType::VLESS;
        item.id = firstNonEmpty(object, {"uuid"});
        item.flow = firstNonEmpty(object, {"flow"});
        item.security = firstNonEmpty(object, {"encryption"});
        if (item.security.isEmpty()) {
            item.security = QStringLiteral("none");
        }
    } else if (type == QStringLiteral("trojan")) {
        item.configType = ConfigType::Trojan;
        item.id = firstNonEmpty(object, {"password"});
    } else if (type == QStringLiteral("shadowsocks")) {
        item.configType = ConfigType::Shadowsocks;
        item.id = firstNonEmpty(object, {"password"});
        item.security = firstNonEmpty(object, {"method"});
    } else if (type == QStringLiteral("socks")) {
        item.configType = ConfigType::Socks;
        item.id = firstNonEmpty(object, {"username"});
        item.security = firstNonEmpty(object, {"password"});
    } else if (type == QStringLiteral("http")) {
        item.configType = ConfigType::HTTP;
        item.id = firstNonEmpty(object, {"username"});
        item.security = firstNonEmpty(object, {"password"});
    } else if (type == QStringLiteral("hysteria2")) {
        item.configType = ConfigType::Hysteria2;
        item.id = firstNonEmpty(object, {"password"});
        item.streamSecurity = QStringLiteral("tls");
        item.sni = firstNonEmpty(object, {"server_name", "sni"});
        item.obfsPassword = firstNonEmpty(object, {"obfs-password", "obfs_password"});
        item.upMbps = firstNonEmpty(object, {"up_mbps", "upmbps"});
        item.downMbps = firstNonEmpty(object, {"down_mbps", "downmbps"});
    } else if (type == QStringLiteral("tuic")) {
        item.configType = ConfigType::TUIC;
        item.id = firstNonEmpty(object, {"uuid"});
        item.security = firstNonEmpty(object, {"password"});
        item.streamSecurity = QStringLiteral("tls");
        item.sni = firstNonEmpty(object, {"server_name", "sni"});
        item.alpn = stringListValue(object.value(QStringLiteral("alpn")));
        item.congestionControl = firstNonEmpty(object, {"congestion_control"});
        item.udpRelayMode = firstNonEmpty(object, {"udp_relay_mode"});
        item.zeroRttHandshake = firstNonEmpty(object, {"zero_rtt_handshake"}) == QStringLiteral("true");
    } else if (type == QStringLiteral("wireguard")) {
        item.configType = ConfigType::WireGuard;
        item.privateKey = firstNonEmpty(object, {"private_key"});
        item.peerPublicKey = firstNonEmpty(object, {"peer_public_key"});
        item.localAddress = joinStringList(stringListValue(object.value(QStringLiteral("local_address"))));
        item.reserved = joinStringList(stringListValue(object.value(QStringLiteral("reserved"))));
    } else if (type == QStringLiteral("anytls")) {
        item.configType = ConfigType::AnyTLS;
        item.id = firstNonEmpty(object, {"password"});
        item.streamSecurity = QStringLiteral("tls");
        item.sni = firstNonEmpty(object, {"server_name", "sni"});
        item.fingerprint = firstNonEmpty(object, {"utls", "fingerprint"});
        item.alpn = stringListValue(object.value(QStringLiteral("alpn")));
    } else if (type == QStringLiteral("naive")) {
        item.configType = ConfigType::Naive;
        item.username = firstNonEmpty(object, {"username"});
        item.id = firstNonEmpty(object, {"password"});
        item.streamSecurity = QStringLiteral("tls");
    } else {
        return items;
    }

    item.network = firstNonEmpty(object, {"network"});
    if (item.network.isEmpty()) {
        item.network = firstNonEmpty(object, {"transport"});
    }
    if (item.network.isEmpty()) {
        item.network = QStringLiteral("tcp");
    }
    item.headerType = firstNonEmpty(object, {"packet_encoding", "headerType"});
    if (item.headerType.isEmpty()) {
        item.headerType = QStringLiteral("none");
    }
    if (item.streamSecurity.isEmpty()) {
        item.streamSecurity = firstNonEmpty(object, {"tls", "security"});
    }
    item.sni = item.sni.isEmpty() ? firstNonEmpty(object, {"tls_server_name", "servername"}) : item.sni;
    item.requestHost = firstNonEmpty(object, {"host"});
    item.path = firstNonEmpty(object, {"path", "service_name"});
    item.allowInsecure = firstNonEmpty(object, {"insecure", "allow_insecure"});
    item.fingerprint = item.fingerprint.isEmpty() ? firstNonEmpty(object, {"fingerprint", "client_fingerprint"}) : item.fingerprint;
    item.publicKey = firstNonEmpty(object, {"reality_public_key", "public_key"});
    item.shortId = firstNonEmpty(object, {"reality_short_id", "short_id"});
    item.extra = compactJson(object.value(QStringLiteral("transport")));

    if (!item.address.isEmpty() && isValidTcpPort(item.port)) {
        items.append(item);
    }
    return items;
}
