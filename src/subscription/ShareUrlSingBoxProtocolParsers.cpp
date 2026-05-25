#include "subscription/ShareUrlProtocolParsers.h"

#include <QUrl>
#include <QUrlQuery>

#include "common/PortValidator.h"
#include "subscription/ShareUrlParserSupport.h"

using ShareUrlParserSupport::decodedPassword;
using ShareUrlParserSupport::decodedUserName;
using ShareUrlParserSupport::splitCsv;

namespace ShareUrlProtocolParsers {

VmessItem parseHysteria2(const QString& shareUrl, bool* ok)
{
    VmessItem item;
    const QUrl url(shareUrl);
    if (!url.isValid()) {
        return item;
    }

    const QUrlQuery query(url);
    item.configType = ConfigType::Hysteria2;
    item.remarks = url.fragment(QUrl::FullyDecoded);
    item.address = url.host();
    item.port = url.port();
    item.id = decodedUserName(url);

    item.sni = query.queryItemValue(QStringLiteral("sni"));
    item.streamSecurity = QStringLiteral("tls");
    item.allowInsecure = query.queryItemValue(QStringLiteral("insecure"));
    item.obfsPassword = query.queryItemValue(QStringLiteral("obfs-password"));
    if (!query.queryItemValue(QStringLiteral("obfs")).isEmpty()) {
        item.headerType = query.queryItemValue(QStringLiteral("obfs"));
    }
    item.upMbps = query.queryItemValue(QStringLiteral("upmbps"));
    item.downMbps = query.queryItemValue(QStringLiteral("downmbps"));
    item.certSha = QUrl::fromPercentEncoding(query.queryItemValue(QStringLiteral("pinSHA256")).toUtf8());

    const QString alpn = query.queryItemValue(QStringLiteral("alpn"));
    if (!alpn.isEmpty()) {
        item.alpn = alpn.split(QChar(','), Qt::SkipEmptyParts);
    }

    if (ok != nullptr) {
        *ok = !item.address.isEmpty() && isValidTcpPort(item.port);
    }

    return item;
}

VmessItem parseTuic(const QString& shareUrl, bool* ok)
{
    VmessItem item;
    const QUrl url(shareUrl);
    if (!url.isValid()) {
        return item;
    }

    const QUrlQuery query(url);
    item.configType = ConfigType::TUIC;
    item.remarks = url.fragment(QUrl::FullyDecoded);
    item.address = url.host();
    item.port = url.port();
    item.id = decodedUserName(url);
    item.security = decodedPassword(url);

    item.sni = query.queryItemValue(QStringLiteral("sni"));
    item.streamSecurity = QStringLiteral("tls");
    item.allowInsecure = query.queryItemValue(QStringLiteral("allowInsecure"));
    item.congestionControl = query.queryItemValue(QStringLiteral("congestion_control"));
    if (item.congestionControl.isEmpty()) {
        item.congestionControl = QStringLiteral("cubic");
    }
    item.udpRelayMode = query.queryItemValue(QStringLiteral("udp_relay_mode"));
    if (item.udpRelayMode.isEmpty()) {
        item.udpRelayMode = QStringLiteral("native");
    }
    item.zeroRttHandshake = query.queryItemValue(QStringLiteral("zero_rtt_handshake")) == QStringLiteral("true");
    item.alpn = splitCsv(query.queryItemValue(QStringLiteral("alpn")));

    if (ok != nullptr) {
        *ok = !item.address.isEmpty() && isValidTcpPort(item.port) && !item.id.isEmpty();
    }

    return item;
}

VmessItem parseWireguard(const QString& shareUrl, bool* ok)
{
    VmessItem item;
    const QUrl url(shareUrl);
    if (!url.isValid()) {
        return item;
    }

    const QUrlQuery query(url);
    item.configType = ConfigType::WireGuard;
    item.remarks = url.fragment(QUrl::FullyDecoded);
    item.address = url.host();
    item.port = url.port();
    item.privateKey = decodedUserName(url);

    item.peerPublicKey = query.queryItemValue(QStringLiteral("publickey"));
    item.localAddress = query.queryItemValue(QStringLiteral("address"));
    item.reserved = query.queryItemValue(QStringLiteral("reserved"));
    const QString mtuStr = query.queryItemValue(QStringLiteral("mtu"));
    if (!mtuStr.isEmpty()) {
        bool okMtu = false;
        const int mtu = mtuStr.toInt(&okMtu);
        if (okMtu && mtu > 0) {
            item.wireguardMtu = mtu;
        }
    }

    if (ok != nullptr) {
        *ok = !item.address.isEmpty() && isValidTcpPort(item.port) && !item.privateKey.isEmpty();
    }

    return item;
}

VmessItem parseAnytls(const QString& shareUrl, bool* ok)
{
    VmessItem item;
    const QUrl url(shareUrl);
    if (!url.isValid()) {
        return item;
    }

    const QUrlQuery query(url);
    item.configType = ConfigType::AnyTLS;
    item.coreType = CoreType::SingBox;
    item.remarks = url.fragment(QUrl::FullyDecoded);
    item.address = url.host();
    item.port = url.port();
    item.id = decodedUserName(url);

    item.sni = query.queryItemValue(QStringLiteral("sni"));
    item.streamSecurity = QStringLiteral("tls");
    item.allowInsecure = query.queryItemValue(QStringLiteral("allowInsecure"));
    if (item.allowInsecure.isEmpty()) {
        item.allowInsecure = query.queryItemValue(QStringLiteral("insecure"));
    }
    item.fingerprint = query.queryItemValue(QStringLiteral("fp"));
    item.alpn = splitCsv(QUrl::fromPercentEncoding(query.queryItemValue(QStringLiteral("alpn")).toUtf8()));
    item.idleSessionCheckInterval = query.queryItemValue(QStringLiteral("idle_session_check_interval"));
    item.idleSessionTimeout = query.queryItemValue(QStringLiteral("idle_session_timeout"));
    item.minIdleSession = query.queryItemValue(QStringLiteral("min_idle_session"));

    if (ok != nullptr) {
        *ok = !item.address.isEmpty() && isValidTcpPort(item.port) && !item.id.isEmpty();
    }

    return item;
}

VmessItem parseNaive(const QString& shareUrl, bool* ok)
{
    VmessItem item;
    const QUrl url(shareUrl);
    if (!url.isValid()) {
        return item;
    }

    const QUrlQuery query(url);
    item.configType = ConfigType::Naive;
    item.coreType = CoreType::SingBox;
    item.remarks = url.fragment(QUrl::FullyDecoded);
    item.address = url.host();
    item.port = url.port();
    item.username = decodedUserName(url);
    item.id = decodedPassword(url);
    item.naiveQuic = url.scheme().compare(QStringLiteral("naive+quic"), Qt::CaseInsensitive) == 0;

    item.sni = query.queryItemValue(QStringLiteral("sni"));
    item.streamSecurity = QStringLiteral("tls");
    item.allowInsecure = query.queryItemValue(QStringLiteral("allowInsecure"));
    if (item.allowInsecure.isEmpty()) {
        item.allowInsecure = query.queryItemValue(QStringLiteral("insecure"));
    }
    item.alpn = splitCsv(query.queryItemValue(QStringLiteral("alpn")));
    item.congestionControl = query.queryItemValue(QStringLiteral("congestion_control"));
    const QString concurrency = query.queryItemValue(QStringLiteral("insecure-concurrency"));
    if (!concurrency.isEmpty()) {
        bool okNum = false;
        const int value = concurrency.toInt(&okNum);
        if (okNum && value > 0) {
            item.insecureConcurrency = value;
        }
    }

    if (ok != nullptr) {
        *ok = !item.address.isEmpty() && isValidTcpPort(item.port) && !item.id.isEmpty();
    }

    return item;
}

} // namespace ShareUrlProtocolParsers
