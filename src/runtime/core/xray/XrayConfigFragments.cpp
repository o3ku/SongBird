#include "runtime/core/xray/XrayConfigFragments.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include "common/UserAgent.h"
#include "runtime/ProtocolConfigMapper.h"

namespace {

constexpr int kLocationProbePortOffset = 103;
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

QString resolveLegacyUserAgent(const Config& config, const VmessItem& server)
{
    const QString serverUserAgent = server.userAgent.trimmed();
    if (!serverUserAgent.isEmpty()) {
        return serverUserAgent;
    }

    const QString defaultUserAgent = config.dns().defaultUserAgent.trimmed();
    return defaultUserAgent.isEmpty() ? fallbackUserAgent() : defaultUserAgent;
}

bool resolveMuxEnabled(const Config& config, const VmessItem& server)
{
    return server.muxEnabled.value_or(config.muxEnabled);
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

QJsonArray buildLegacyTlsCertificates(const QStringList& certificates)
{
    QJsonArray result;
    for (const QString& certificate : certificates) {
        QJsonArray certificateLines;
        const QStringList lines = certificate.split(QChar('\n'));
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                certificateLines.append(trimmed);
            }
        }
        if (certificateLines.isEmpty()) {
            continue;
        }

        QJsonObject item;
        item.insert(QStringLiteral("certificate"), certificateLines);
        item.insert(QStringLiteral("usage"), QStringLiteral("verify"));
        result.append(item);
    }
    return result;
}

} // namespace

QJsonObject XrayConfigFragments::buildSocksInbound(const Config& config, bool allowLan, int offset)
{
    QJsonObject inbound;
    inbound.insert(QStringLiteral("tag"), allowLan ? QStringLiteral("socks-lan") : QStringLiteral("socks"));
    inbound.insert(QStringLiteral("port"), config.localPort + offset);
    inbound.insert(QStringLiteral("listen"), allowLan ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1"));
    inbound.insert(QStringLiteral("protocol"), QStringLiteral("socks"));

    QJsonObject settings;
    settings.insert(QStringLiteral("auth"), QStringLiteral("noauth"));
    settings.insert(QStringLiteral("udp"), config.udpEnabled);
    settings.insert(QStringLiteral("allowTransparent"), false);
    if (allowLan && !config.inboundUser.trimmed().isEmpty() && !config.inboundPassword.trimmed().isEmpty()) {
        settings.insert(QStringLiteral("auth"), QStringLiteral("password"));
        QJsonArray accounts;
        QJsonObject account;
        account.insert(QStringLiteral("user"), config.inboundUser);
        account.insert(QStringLiteral("pass"), config.inboundPassword);
        accounts.append(account);
        settings.insert(QStringLiteral("accounts"), accounts);
        settings.insert(QStringLiteral("auth"), QStringLiteral("password"));
    }
    inbound.insert(QStringLiteral("settings"), settings);
    inbound.insert(QStringLiteral("sniffing"), buildSniffing(config));
    return inbound;
}

QJsonObject XrayConfigFragments::buildHttpInbound(const Config& config, bool allowLan, int offset)
{
    return buildHttpInboundWithTag(
        config,
        allowLan ? QStringLiteral("http-lan") : QStringLiteral("http"),
        offset);
}

QJsonObject XrayConfigFragments::buildHttpInboundWithTag(const Config& config, const QString& tag, int offset)
{
    QJsonObject inbound;
    const bool allowLan = tag.endsWith(QStringLiteral("-lan"));
    int port = config.localPort + offset;
    if (!allowLan && offset == 1 && config.localHttpPort > 0) {
        port = config.localHttpPort;
    } else if (tag == kLocationProbeTag && config.localLocationProbePort > 0) {
        port = config.localLocationProbePort;
    }
    inbound.insert(QStringLiteral("tag"), tag);
    inbound.insert(QStringLiteral("port"), port);
    inbound.insert(QStringLiteral("listen"), allowLan ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1"));
    inbound.insert(QStringLiteral("protocol"), QStringLiteral("http"));

    QJsonObject settings;
    if (allowLan && !config.inboundUser.trimmed().isEmpty() && !config.inboundPassword.trimmed().isEmpty()) {
        settings.insert(QStringLiteral("auth"), QStringLiteral("password"));
        QJsonArray accounts;
        QJsonObject account;
        account.insert(QStringLiteral("user"), config.inboundUser);
        account.insert(QStringLiteral("pass"), config.inboundPassword);
        accounts.append(account);
        settings.insert(QStringLiteral("accounts"), accounts);
    }
    inbound.insert(QStringLiteral("settings"), settings);
    inbound.insert(QStringLiteral("sniffing"), buildSniffing(config));
    return inbound;
}

QJsonObject XrayConfigFragments::buildSniffing(const Config& config)
{
    QJsonObject sniffing;
    sniffing.insert(QStringLiteral("enabled"), config.sniffingEnabled);
    sniffing.insert(QStringLiteral("routeOnly"), config.routeOnly);
    QJsonArray destOverride;
    destOverride.append(QStringLiteral("http"));
    destOverride.append(QStringLiteral("tls"));
    sniffing.insert(QStringLiteral("destOverride"), destOverride);
    return sniffing;
}

QJsonObject XrayConfigFragments::buildStreamSettings(const Config& config, const VmessItem& server)
{
    QJsonObject streamSettings;
    if (server.configType == ConfigType::Hysteria2) {
        streamSettings.insert(QStringLiteral("network"), QStringLiteral("hysteria"));
        QJsonObject hysteriaSettings;
        hysteriaSettings.insert(QStringLiteral("version"), 2);
        hysteriaSettings.insert(QStringLiteral("auth"), server.id);
        streamSettings.insert(QStringLiteral("hysteriaSettings"), hysteriaSettings);
        return streamSettings;
    }

    const QString network = server.network.trimmed().isEmpty() ? QStringLiteral("tcp") : server.network.trimmed();
    const QString transportSecurity = server.streamSecurity.trimmed();
    const QString userAgent = resolveLegacyUserAgent(config, server);
    const QString fingerprint = ProtocolConfigMapper::resolveFingerprint(config, server);
    streamSettings.insert(QStringLiteral("network"), network);

    if (!transportSecurity.isEmpty()) {
        streamSettings.insert(QStringLiteral("security"), transportSecurity);
    }

    if (network.compare(QStringLiteral("kcp"), Qt::CaseInsensitive) == 0) {
        QJsonObject kcpSettings;
        QJsonObject header;
        header.insert(
            QStringLiteral("type"),
            server.headerType.trimmed().isEmpty() ? QStringLiteral("none") : server.headerType.trimmed());
        kcpSettings.insert(QStringLiteral("header"), header);
        if (!server.path.trimmed().isEmpty()) {
            kcpSettings.insert(QStringLiteral("seed"), server.path.trimmed());
        }
        streamSettings.insert(QStringLiteral("kcpSettings"), kcpSettings);
    } else if (network.compare(QStringLiteral("quic"), Qt::CaseInsensitive) == 0) {
        QJsonObject quicSettings;
        quicSettings.insert(
            QStringLiteral("security"),
            server.requestHost.trimmed().isEmpty() ? QStringLiteral("none") : server.requestHost.trimmed());
        quicSettings.insert(QStringLiteral("key"), server.path.trimmed());
        QJsonObject header;
        header.insert(
            QStringLiteral("type"),
            server.headerType.trimmed().isEmpty() ? QStringLiteral("none") : server.headerType.trimmed());
        quicSettings.insert(QStringLiteral("header"), header);
        streamSettings.insert(QStringLiteral("quicSettings"), quicSettings);
    } else if (network.compare(QStringLiteral("ws"), Qt::CaseInsensitive) == 0) {
        QJsonObject wsSettings;
        if (!server.path.trimmed().isEmpty()) {
            wsSettings.insert(QStringLiteral("path"), server.path);
        }
        QJsonObject headers;
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            headers.insert(QStringLiteral("Host"), hosts.constFirst());
        }
        if (!userAgent.isEmpty()) {
            headers.insert(QStringLiteral("User-Agent"), userAgent);
        }
        if (!headers.isEmpty()) {
            wsSettings.insert(QStringLiteral("headers"), headers);
        }
        streamSettings.insert(QStringLiteral("wsSettings"), wsSettings);
    } else if (network.compare(QStringLiteral("grpc"), Qt::CaseInsensitive) == 0) {
        QJsonObject grpcSettings;
        grpcSettings.insert(QStringLiteral("serviceName"), server.path);
        grpcSettings.insert(
            QStringLiteral("multiMode"),
            server.headerType.compare(QStringLiteral("multi"), Qt::CaseInsensitive) == 0);
        streamSettings.insert(QStringLiteral("grpcSettings"), grpcSettings);
    } else if (network.compare(QStringLiteral("h2"), Qt::CaseInsensitive) == 0) {
        QJsonObject httpSettings;
        httpSettings.insert(QStringLiteral("path"), server.path);
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonArray hostArray;
            for (const QString& host : hosts) {
                hostArray.append(host);
            }
            httpSettings.insert(QStringLiteral("host"), hostArray);
        }
        streamSettings.insert(QStringLiteral("httpSettings"), httpSettings);
    } else if (network.compare(QStringLiteral("httpupgrade"), Qt::CaseInsensitive) == 0) {
        QJsonObject httpupgradeSettings;
        if (!server.path.trimmed().isEmpty()) {
            httpupgradeSettings.insert(QStringLiteral("path"), server.path);
        }
        if (!server.requestHost.trimmed().isEmpty()) {
            httpupgradeSettings.insert(QStringLiteral("host"), server.requestHost);
        }
        if (!userAgent.isEmpty()) {
            QJsonObject headers;
            headers.insert(QStringLiteral("User-Agent"), userAgent);
            httpupgradeSettings.insert(QStringLiteral("headers"), headers);
        }
        streamSettings.insert(QStringLiteral("httpupgradeSettings"), httpupgradeSettings);
    } else if (network.compare(QStringLiteral("xhttp"), Qt::CaseInsensitive) == 0) {
        streamSettings.insert(QStringLiteral("network"), QStringLiteral("xhttp"));
        QJsonObject xhttpSettings;
        if (!server.path.trimmed().isEmpty()) {
            xhttpSettings.insert(QStringLiteral("path"), server.path);
        }
        if (!server.requestHost.trimmed().isEmpty()) {
            xhttpSettings.insert(QStringLiteral("host"), server.requestHost);
        }
        if (!server.headerType.trimmed().isEmpty()) {
            xhttpSettings.insert(QStringLiteral("mode"), server.headerType);
        }
        if (!server.extra.trimmed().isEmpty()) {
            QJsonParseError parseError;
            const QJsonDocument extraDoc = QJsonDocument::fromJson(server.extra.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError && extraDoc.isObject()) {
                xhttpSettings.insert(QStringLiteral("extra"), extraDoc.object());
            }
        }
        streamSettings.insert(QStringLiteral("xhttpSettings"), xhttpSettings);
    } else if (network.compare(QStringLiteral("tcp"), Qt::CaseInsensitive) == 0
        && server.headerType.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0) {
        QJsonObject tcpSettings;
        QJsonObject header;
        header.insert(QStringLiteral("type"), QStringLiteral("http"));

        QJsonObject request;
        QJsonArray paths;
        const QStringList rawPaths = splitCsv(server.path);
        if (rawPaths.isEmpty()) {
            paths.append(QStringLiteral("/"));
        } else {
            for (const QString& path : rawPaths) {
                paths.append(path);
            }
        }
        request.insert(QStringLiteral("path"), paths);

        QJsonObject headers;
        const QStringList hosts = splitCsv(server.requestHost);
        if (!hosts.isEmpty()) {
            QJsonArray hostArray;
            for (const QString& host : hosts) {
                hostArray.append(host);
            }
            headers.insert(QStringLiteral("Host"), hostArray);
        }
        if (!userAgent.isEmpty()) {
            headers.insert(QStringLiteral("User-Agent"), QJsonArray{userAgent});
        }
        if (!headers.isEmpty()) {
            request.insert(QStringLiteral("headers"), headers);
        }

        header.insert(QStringLiteral("request"), request);
        tcpSettings.insert(QStringLiteral("header"), header);
        streamSettings.insert(QStringLiteral("tcpSettings"), tcpSettings);
    }

    if (!server.finalmask.trimmed().isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument finalmaskDocument = QJsonDocument::fromJson(server.finalmask.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && finalmaskDocument.isObject()) {
            streamSettings.insert(QStringLiteral("finalmask"), finalmaskDocument.object());
        }
    }

    if (ProtocolConfigMapper::isRealityTransport(transportSecurity)) {
        QJsonObject realitySettings;
        realitySettings.insert(QStringLiteral("serverName"), ProtocolConfigMapper::resolveServerName(server));
        realitySettings.insert(QStringLiteral("fingerprint"), ProtocolConfigMapper::resolveRealityFingerprint(config, server));
        realitySettings.insert(QStringLiteral("publicKey"), server.publicKey.trimmed());
        realitySettings.insert(QStringLiteral("shortId"), server.shortId.trimmed());
        realitySettings.insert(QStringLiteral("spiderX"), server.spiderX.trimmed());
        if (!server.mldsa65Verify.trimmed().isEmpty()) {
            realitySettings.insert(QStringLiteral("mldsa65Verify"), server.mldsa65Verify.trimmed());
        }
        streamSettings.insert(QStringLiteral("realitySettings"), realitySettings);
    } else if (transportSecurity.compare(QStringLiteral("tls"), Qt::CaseInsensitive) == 0
        || transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) == 0) {
        QJsonObject tlsSettings;
        const QStringList certificates = splitPemCertificates(server.cert);
        tlsSettings.insert(QStringLiteral("serverName"), ProtocolConfigMapper::resolveServerName(server));
        tlsSettings.insert(
            QStringLiteral("allowInsecure"),
            ProtocolConfigMapper::resolveAllowInsecure(server.allowInsecure, config.dns().defaultAllowInsecure));
        if (!fingerprint.isEmpty()) {
            tlsSettings.insert(QStringLiteral("fingerprint"), fingerprint);
        }
        if (!server.echConfigList.trimmed().isEmpty()) {
            tlsSettings.insert(QStringLiteral("echConfigList"), server.echConfigList.trimmed());
        }
        if (!server.echForceQuery.trimmed().isEmpty()) {
            tlsSettings.insert(QStringLiteral("echForceQuery"), server.echForceQuery.trimmed());
        }
        if (!certificates.isEmpty()) {
            tlsSettings.insert(QStringLiteral("certificates"), buildLegacyTlsCertificates(certificates));
            tlsSettings.insert(QStringLiteral("disableSystemRoot"), true);
            tlsSettings.insert(QStringLiteral("allowInsecure"), false);
        } else if (!server.certSha.trimmed().isEmpty()) {
            tlsSettings.insert(QStringLiteral("pinnedPeerCertSha256"), server.certSha.trimmed());
            tlsSettings.insert(QStringLiteral("allowInsecure"), false);
        }

        if (!server.alpn.isEmpty()) {
            QJsonArray alpnArray;
            for (const QString& item : server.alpn) {
                alpnArray.append(item);
            }
            tlsSettings.insert(QStringLiteral("alpn"), alpnArray);
        }

        if (transportSecurity.compare(QStringLiteral("xtls"), Qt::CaseInsensitive) == 0) {
            streamSettings.insert(QStringLiteral("xtlsSettings"), tlsSettings);
        } else {
            streamSettings.insert(QStringLiteral("tlsSettings"), tlsSettings);
        }
    }

    return streamSettings;
}

QJsonObject XrayConfigFragments::buildPrimaryOutbound(const Config& config, const VmessItem& server)
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("proxy"));
    outbound.insert(QStringLiteral("streamSettings"), buildStreamSettings(config, server));
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

QJsonObject XrayConfigFragments::buildDirectOutbound(const Config& config)
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

QJsonObject XrayConfigFragments::buildBlackholeOutbound()
{
    QJsonObject outbound;
    outbound.insert(QStringLiteral("tag"), QStringLiteral("block"));
    outbound.insert(QStringLiteral("protocol"), QStringLiteral("blackhole"));
    outbound.insert(QStringLiteral("settings"), QJsonObject());
    return outbound;
}

QJsonObject XrayConfigFragments::buildFragmentOutbound()
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
