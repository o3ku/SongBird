#include "subscription/ShareUrlProtocolBuilders.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "subscription/ShareUrlBuilderSupport.h"

using ShareUrlBuilderSupport::QueryEntries;
using ShareUrlBuilderSupport::buildQuery;
using ShareUrlBuilderSupport::buildRemark;
using ShareUrlBuilderSupport::buildStandardTransportEntries;
using ShareUrlBuilderSupport::encodeBase64;
using ShareUrlBuilderSupport::encodeUserInfoPart;
using ShareUrlBuilderSupport::joinList;
using ShareUrlBuilderSupport::wrapIpv6;

namespace ShareUrlProtocolBuilders {

QString buildVmess(const VmessItem& item)
{
    QJsonObject object;
    object.insert(QStringLiteral("v"), QStringLiteral("2"));
    object.insert(QStringLiteral("ps"), item.remarks.trimmed());
    object.insert(QStringLiteral("add"), item.address);
    object.insert(QStringLiteral("port"), QString::number(item.port));
    object.insert(QStringLiteral("id"), item.id);
    object.insert(QStringLiteral("aid"), QString::number(item.alterId));
    object.insert(QStringLiteral("scy"), item.security);
    object.insert(QStringLiteral("net"), item.network);
    object.insert(QStringLiteral("type"), item.headerType);
    object.insert(QStringLiteral("host"), item.requestHost);
    object.insert(QStringLiteral("path"), item.path);
    object.insert(QStringLiteral("tls"), item.streamSecurity);
    object.insert(QStringLiteral("sni"), item.sni);
    object.insert(QStringLiteral("alpn"), joinList(item.alpn));
    return QStringLiteral("vmess://") + encodeBase64(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
}

QString buildShadowsocks(const VmessItem& item)
{
    const QString credentials = encodeBase64(QStringLiteral("%1:%2").arg(item.security, item.id));
    return QStringLiteral("ss://")
        + credentials
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildRemark(item.remarks);
}

QString buildSocks(const VmessItem& item)
{
    const QString credentials = encodeBase64(QStringLiteral("%1:%2").arg(item.id, item.security));
    return QStringLiteral("socks://")
        + credentials
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildRemark(item.remarks);
}

QString buildTrojan(const VmessItem& item)
{
    return QStringLiteral("trojan://")
        + encodeUserInfoPart(item.id)
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(buildStandardTransportEntries(item, {}))
        + buildRemark(item.remarks);
}

QString buildVless(const VmessItem& item)
{
    QueryEntries entries{
        qMakePair(QStringLiteral("encryption"), item.security.trimmed().isEmpty() ? QStringLiteral("none") : item.security)
    };
    const QueryEntries transportEntries = buildStandardTransportEntries(item, QStringLiteral("none"));
    for (const auto& entry : transportEntries) {
        entries.append(entry);
    }

    return QStringLiteral("vless://")
        + encodeUserInfoPart(item.id)
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(entries)
        + buildRemark(item.remarks);
}

QString buildHysteria2(const VmessItem& item)
{
    QueryEntries entries;
    if (!item.sni.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("sni"), item.sni));
    }
    if (!item.obfsPassword.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("obfs"), QStringLiteral("salamander")));
        entries.append(qMakePair(QStringLiteral("obfs-password"), item.obfsPassword));
    }
    if (!item.upMbps.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("upmbps"), item.upMbps));
    }
    if (!item.downMbps.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("downmbps"), item.downMbps));
    }
    if (item.allowInsecure.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
        entries.append(qMakePair(QStringLiteral("insecure"), QStringLiteral("1")));
    }
    if (!item.alpn.isEmpty()) {
        entries.append(qMakePair(QStringLiteral("alpn"), joinList(item.alpn)));
    }

    return QStringLiteral("hysteria2://")
        + encodeUserInfoPart(item.id)
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(entries)
        + buildRemark(item.remarks);
}

QString buildTuic(const VmessItem& item)
{
    QueryEntries entries;
    if (!item.sni.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("sni"), item.sni));
    }
    if (!item.congestionControl.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("congestion_control"), item.congestionControl));
    }
    if (!item.udpRelayMode.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("udp_relay_mode"), item.udpRelayMode));
    }
    if (item.zeroRttHandshake) {
        entries.append(qMakePair(QStringLiteral("zero_rtt_handshake"), QStringLiteral("true")));
    }
    if (item.allowInsecure.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
        entries.append(qMakePair(QStringLiteral("allowInsecure"), QStringLiteral("1")));
    }
    if (!item.alpn.isEmpty()) {
        entries.append(qMakePair(QStringLiteral("alpn"), joinList(item.alpn)));
    }

    QString credentials = encodeUserInfoPart(item.id);
    if (!item.security.trimmed().isEmpty()) {
        credentials += QStringLiteral(":") + encodeUserInfoPart(item.security);
    }

    return QStringLiteral("tuic://")
        + credentials
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(entries)
        + buildRemark(item.remarks);
}

QString buildWireguard(const VmessItem& item)
{
    QueryEntries entries;
    if (!item.peerPublicKey.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("publickey"), item.peerPublicKey));
    }
    if (!item.localAddress.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("address"), item.localAddress));
    }
    if (!item.reserved.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("reserved"), item.reserved));
    }
    if (item.wireguardMtu > 0) {
        entries.append(qMakePair(QStringLiteral("mtu"), QString::number(item.wireguardMtu)));
    }

    return QStringLiteral("wg://")
        + encodeUserInfoPart(item.privateKey)
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(entries)
        + buildRemark(item.remarks);
}

QString buildAnytls(const VmessItem& item)
{
    QueryEntries entries;
    if (!item.sni.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("sni"), item.sni));
    }
    if (item.allowInsecure.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
        || item.allowInsecure == QStringLiteral("1")) {
        entries.append(qMakePair(QStringLiteral("insecure"), QStringLiteral("1")));
    }
    if (!item.alpn.isEmpty()) {
        entries.append(qMakePair(QStringLiteral("alpn"), joinList(item.alpn)));
    }
    if (!item.fingerprint.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("fp"), item.fingerprint));
    }
    if (!item.idleSessionCheckInterval.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("idle_session_check_interval"), item.idleSessionCheckInterval));
    }
    if (!item.idleSessionTimeout.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("idle_session_timeout"), item.idleSessionTimeout));
    }
    if (!item.minIdleSession.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("min_idle_session"), item.minIdleSession));
    }

    return QStringLiteral("anytls://")
        + encodeUserInfoPart(item.id)
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(entries)
        + buildRemark(item.remarks);
}

QString buildNaive(const VmessItem& item)
{
    QueryEntries entries;
    if (!item.sni.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("sni"), item.sni));
    }
    if (item.allowInsecure.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
        || item.allowInsecure == QStringLiteral("1")) {
        entries.append(qMakePair(QStringLiteral("insecure"), QStringLiteral("1")));
    }
    if (!item.alpn.isEmpty()) {
        entries.append(qMakePair(QStringLiteral("alpn"), joinList(item.alpn)));
    }
    if (!item.congestionControl.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("congestion_control"), item.congestionControl));
    }
    if (item.insecureConcurrency > 0) {
        entries.append(qMakePair(QStringLiteral("insecure-concurrency"), QString::number(item.insecureConcurrency)));
    }

    const QString scheme = item.naiveQuic
        ? QStringLiteral("naive+quic://")
        : QStringLiteral("naive+https://");

    QString userInfo;
    if (!item.username.trimmed().isEmpty()) {
        userInfo = encodeUserInfoPart(item.username)
            + QStringLiteral(":")
            + encodeUserInfoPart(item.id);
    } else {
        userInfo = encodeUserInfoPart(item.id);
    }

    return scheme
        + userInfo
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(entries)
        + buildRemark(item.remarks);
}

} // namespace ShareUrlProtocolBuilders
