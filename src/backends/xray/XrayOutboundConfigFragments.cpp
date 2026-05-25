#include "backends/xray/XrayOutboundConfigFragments.h"

#include <QJsonArray>

#include "backends/xray/XrayStreamConfigFragments.h"

namespace {

bool resolveMuxEnabled(const Config& config, const VmessItem& server)
{
    return server.muxEnabled.value_or(config.muxEnabled);
}

} // namespace

namespace XrayOutboundConfigFragments {

QJsonObject buildPrimaryOutbound(const Config& config, const VmessItem& server)
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("proxy"));
    outbound.insert(QStringLiteral("streamSettings"), XrayStreamConfigFragments::buildStreamSettings(config, server));
    if (!config.dns().domainStrategyForProxy.trimmed().isEmpty()
        && server.configType != ConfigType::Socks
        && server.configType != ConfigType::HTTP
        && server.configType != ConfigType::Custom
        && server.configType != ConfigType::Unknown
        && server.configType != ConfigType::Hysteria2
        && server.configType != ConfigType::TUIC
        && server.configType != ConfigType::WireGuard) {
        outbound.insert(QStringLiteral("targetStrategy"), config.dns().domainStrategyForProxy.trimmed());
    }

    if (server.configType == ConfigType::Shadowsocks) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("shadowsocks"));
        QJsonObject settings;
        QJsonArray servers;
        QJsonObject item;
        item.insert(QStringLiteral("address"), server.address);
        item.insert(QStringLiteral("port"), server.port);
        item.insert(QStringLiteral("method"), server.security.trimmed().isEmpty() ? QStringLiteral("aes-128-gcm") : server.security);
        item.insert(QStringLiteral("password"), server.id);
        servers.append(item);
        settings.insert(QStringLiteral("servers"), servers);
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    if (server.configType == ConfigType::Socks) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("socks"));
        QJsonObject settings;
        QJsonArray servers;
        QJsonObject item;
        item.insert(QStringLiteral("address"), server.address);
        item.insert(QStringLiteral("port"), server.port);
        if (!server.id.trimmed().isEmpty()) {
            QJsonArray users;
            QJsonObject user;
            user.insert(QStringLiteral("user"), server.id);
            user.insert(QStringLiteral("pass"), server.security);
            users.append(user);
            item.insert(QStringLiteral("users"), users);
        }
        servers.append(item);
        settings.insert(QStringLiteral("servers"), servers);
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    if (server.configType == ConfigType::HTTP) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("http"));
        QJsonObject settings;
        QJsonArray servers;
        QJsonObject item;
        item.insert(QStringLiteral("address"), server.address);
        item.insert(QStringLiteral("port"), server.port);
        if (!server.id.trimmed().isEmpty()) {
            QJsonArray users;
            QJsonObject user;
            user.insert(QStringLiteral("user"), server.id);
            user.insert(QStringLiteral("pass"), server.security);
            users.append(user);
            item.insert(QStringLiteral("users"), users);
        }
        servers.append(item);
        settings.insert(QStringLiteral("servers"), servers);
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    if (server.configType == ConfigType::Trojan) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("trojan"));
        QJsonObject settings;
        QJsonArray servers;
        QJsonObject item;
        item.insert(QStringLiteral("address"), server.address);
        item.insert(QStringLiteral("port"), server.port);
        item.insert(QStringLiteral("password"), server.id);
        if (!server.flow.trimmed().isEmpty()) {
            item.insert(QStringLiteral("flow"), server.flow);
        }
        servers.append(item);
        settings.insert(QStringLiteral("servers"), servers);
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    if (server.configType == ConfigType::Hysteria2) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("hysteria"));
        QJsonObject settings;
        settings.insert(QStringLiteral("version"), 2);
        settings.insert(QStringLiteral("address"), server.address);
        settings.insert(QStringLiteral("port"), server.port);
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    if (server.configType == ConfigType::WireGuard) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("wireguard"));
        QJsonObject settings;
        QJsonArray peers;
        QJsonObject peer;
        if (!server.peerPublicKey.trimmed().isEmpty()) {
            peer.insert(QStringLiteral("publicKey"), server.peerPublicKey);
        }
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
                peer.insert(QStringLiteral("reserved"), reservedArr);
            }
        }
        peer.insert(QStringLiteral("endpoint"), server.address + QStringLiteral(":") + QString::number(server.port));
        peers.append(peer);
        settings.insert(QStringLiteral("peers"), peers);
        if (!server.localAddress.trimmed().isEmpty()) {
            const QJsonArray addresses = QJsonArray::fromStringList(
                server.localAddress.split(QChar(','), Qt::SkipEmptyParts));
            settings.insert(QStringLiteral("address"), addresses);
        }
        if (!server.privateKey.trimmed().isEmpty()) {
            settings.insert(QStringLiteral("secretKey"), server.privateKey);
        }
        if (server.wireguardMtu > 0) {
            settings.insert(QStringLiteral("mtu"), server.wireguardMtu);
        }
        outbound.insert(QStringLiteral("settings"), settings);
        return outbound;
    }

    QJsonObject settings;
    QJsonArray vnext;
    QJsonObject remote;
    remote.insert(QStringLiteral("address"), server.address);
    remote.insert(QStringLiteral("port"), server.port);

    QJsonArray users;
    QJsonObject user;
    user.insert(QStringLiteral("id"), server.id);

    if (server.configType == ConfigType::VLESS) {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("vless"));
        user.insert(QStringLiteral("encryption"), QStringLiteral("none"));
        if (!server.flow.trimmed().isEmpty()) {
            user.insert(QStringLiteral("flow"), server.flow);
        }
        if (!server.packetEncoding.trimmed().isEmpty()) {
            user.insert(QStringLiteral("packetEncoding"), server.packetEncoding.trimmed());
        }
    } else {
        outbound.insert(QStringLiteral("protocol"), QStringLiteral("vmess"));
        user.insert(QStringLiteral("alterId"), server.alterId);
        user.insert(QStringLiteral("security"), server.security.trimmed().isEmpty() ? QStringLiteral("auto") : server.security);
        const bool muxEnabled = resolveMuxEnabled(config, server);
        QJsonObject mux;
        mux.insert(QStringLiteral("enabled"), muxEnabled);
        mux.insert(QStringLiteral("concurrency"), muxEnabled ? 8 : -1);
        outbound.insert(QStringLiteral("mux"), mux);
    }

    users.append(user);
    remote.insert(QStringLiteral("users"), users);
    vnext.append(remote);
    settings.insert(QStringLiteral("vnext"), vnext);
    outbound.insert(QStringLiteral("settings"), settings);
    return outbound;
}

QJsonObject buildDirectOutbound(const Config& config)
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("direct"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("freedom"));

    QJsonObject settings;
    if (!config.dns().domainStrategyForFreedom.trimmed().isEmpty()) {
        settings.insert(QStringLiteral("domainStrategy"), config.dns().domainStrategyForFreedom.trimmed());
        settings.insert(QStringLiteral("userLevel"), 0);
    }

    outbound.insert(QStringLiteral("settings"), settings);
    return outbound;
}

QJsonObject buildBlackholeOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("block"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("blackhole"));
    outbound.insert(QStringLiteral("settings"), QJsonObject());
    return outbound;
}

QJsonObject buildFragmentOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("frag-proxy"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("freedom"));

    QJsonObject settings;
    QJsonObject fragment;
    fragment.insert(QStringLiteral("packets"), QStringLiteral("tlshello"));
    fragment.insert(QStringLiteral("length"), QStringLiteral("100-200"));
    fragment.insert(QStringLiteral("interval"), QStringLiteral("10-20"));
    settings.insert(QStringLiteral("fragment"), fragment);
    outbound.insert(QStringLiteral("settings"), settings);
    return outbound;
}

} // namespace XrayOutboundConfigFragments
