#include "subscription/ShareUrlBuilder.h"

#include <QByteArray>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPair>
#include <QUrl>

namespace {

QString normalizeJsonText(const QString& value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || (!document.isObject() && !document.isArray())) {
        return trimmed;
    }

    return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
}

} // namespace

QString ShareUrlBuilder::build(const VmessItem& item)
{
    switch (item.configType) {
    case ConfigType::VMess:
        return buildVmess(item);
    case ConfigType::Shadowsocks:
        return buildShadowsocks(item);
    case ConfigType::Socks:
        return buildSocks(item);
    case ConfigType::Trojan:
        return buildTrojan(item);
    case ConfigType::VLESS:
        return buildVless(item);
    case ConfigType::Hysteria2:
        return buildHysteria2(item);
    case ConfigType::TUIC:
        return buildTuic(item);
    case ConfigType::WireGuard:
        return buildWireguard(item);
    case ConfigType::AnyTLS:
        return buildAnytls(item);
    case ConfigType::Naive:
        return buildNaive(item);
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        return {};
    }
}

QString ShareUrlBuilder::buildVmess(const VmessItem& item)
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

QString ShareUrlBuilder::buildShadowsocks(const VmessItem& item)
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

QString ShareUrlBuilder::buildSocks(const VmessItem& item)
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

QString ShareUrlBuilder::buildTrojan(const VmessItem& item)
{
    return QStringLiteral("trojan://")
        + item.id
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(buildStandardTransportEntries(item, {}))
        + buildRemark(item.remarks);
}

QString ShareUrlBuilder::buildVless(const VmessItem& item)
{
    QList<QPair<QString, QString>> entries{
        qMakePair(QStringLiteral("encryption"), item.security.trimmed().isEmpty() ? QStringLiteral("none") : item.security)
    };
    const QList<QPair<QString, QString>> transportEntries = buildStandardTransportEntries(item, QStringLiteral("none"));
    for (const auto& entry : transportEntries) {
        entries.append(entry);
    }

    return QStringLiteral("vless://")
        + item.id
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(entries)
        + buildRemark(item.remarks);
}

QString ShareUrlBuilder::buildHysteria2(const VmessItem& item)
{
    QList<QPair<QString, QString>> entries;
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
        + QString::fromUtf8(QUrl::toPercentEncoding(item.id))
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(entries)
        + buildRemark(item.remarks);
}

QString ShareUrlBuilder::buildTuic(const VmessItem& item)
{
    QList<QPair<QString, QString>> entries;
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

    QString credentials = item.id;
    if (!item.security.trimmed().isEmpty()) {
        credentials += QStringLiteral(":") + item.security;
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

QString ShareUrlBuilder::buildWireguard(const VmessItem& item)
{
    QList<QPair<QString, QString>> entries;
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
        + QString::fromUtf8(QUrl::toPercentEncoding(item.privateKey))
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(entries)
        + buildRemark(item.remarks);
}

QString ShareUrlBuilder::buildAnytls(const VmessItem& item)
{
    QList<QPair<QString, QString>> entries;
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

    return QStringLiteral("anytls://")
        + QString::fromUtf8(QUrl::toPercentEncoding(item.id))
        + QStringLiteral("@")
        + wrapIpv6(item.address)
        + QStringLiteral(":")
        + QString::number(item.port)
        + buildQuery(entries)
        + buildRemark(item.remarks);
}

QString ShareUrlBuilder::buildNaive(const VmessItem& item)
{
    QList<QPair<QString, QString>> entries;
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
        userInfo = QString::fromUtf8(QUrl::toPercentEncoding(item.username))
            + QStringLiteral(":")
            + QString::fromUtf8(QUrl::toPercentEncoding(item.id));
    } else {
        userInfo = QString::fromUtf8(QUrl::toPercentEncoding(item.id));
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

QList<QPair<QString, QString>> ShareUrlBuilder::buildStandardTransportEntries(const VmessItem& item, const QString& defaultSecurity)
{
    QList<QPair<QString, QString>> entries;
    if (!item.flow.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("flow"), item.flow));
    }

    if (!item.streamSecurity.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("security"), item.streamSecurity));
    } else if (!defaultSecurity.isNull()) {
        entries.append(qMakePair(QStringLiteral("security"), defaultSecurity));
    }

    if (!item.sni.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("sni"), item.sni));
    }

    if (!item.alpn.isEmpty()) {
        entries.append(qMakePair(QStringLiteral("alpn"), joinList(item.alpn)));
    }
    if (!item.echConfigList.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("ech"), item.echConfigList));
    }
    if (!item.certSha.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("pcs"), item.certSha));
    }
    if (!item.finalmask.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("fm"), normalizeJsonText(item.finalmask)));
    }

    if (item.streamSecurity.compare(QStringLiteral("reality"), Qt::CaseInsensitive) == 0) {
        if (!item.fingerprint.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("fp"), item.fingerprint));
        }
        if (!item.publicKey.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("pbk"), item.publicKey));
        }
        if (!item.shortId.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("sid"), item.shortId));
        }
        if (!item.spiderX.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("spx"), item.spiderX));
        }
        if (!item.mldsa65Verify.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("pqv"), item.mldsa65Verify));
        }
    }

    const QString network = item.network.trimmed().isEmpty() ? QStringLiteral("tcp") : item.network;
    entries.append(qMakePair(QStringLiteral("type"), network));

    if (network.compare(QStringLiteral("ws"), Qt::CaseInsensitive) == 0) {
        if (!item.requestHost.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("host"), item.requestHost));
        }
        if (!item.path.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("path"), item.path));
        }
    } else if (network.compare(QStringLiteral("httpupgrade"), Qt::CaseInsensitive) == 0) {
        if (!item.requestHost.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("host"), item.requestHost));
        }
        if (!item.path.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("path"), item.path));
        }
    } else if (network.compare(QStringLiteral("xhttp"), Qt::CaseInsensitive) == 0) {
        if (!item.requestHost.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("host"), item.requestHost));
        }
        if (!item.path.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("path"), item.path));
        }
        if (!item.headerType.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("mode"), item.headerType));
        }
        if (!item.extra.trimmed().isEmpty()) {
            QString extra = item.extra;
            QJsonParseError parseError;
            const QJsonDocument extraDocument = QJsonDocument::fromJson(item.extra.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError
                && (extraDocument.isObject() || extraDocument.isArray())) {
                extra = QString::fromUtf8(extraDocument.toJson(QJsonDocument::Compact));
            }
            entries.append(qMakePair(QStringLiteral("extra"), extra));
        }
    } else if (network.compare(QStringLiteral("kcp"), Qt::CaseInsensitive) == 0) {
        entries.append(qMakePair(
            QStringLiteral("headerType"),
            item.headerType.trimmed().isEmpty() ? QStringLiteral("none") : item.headerType));
        if (!item.path.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("seed"), item.path));
        }
    } else if (network.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
        || network.compare(QStringLiteral("h2"), Qt::CaseInsensitive) == 0) {
        entries.removeLast();
        entries.append(qMakePair(QStringLiteral("type"), QStringLiteral("http")));
        if (!item.requestHost.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("host"), item.requestHost));
        }
        if (!item.path.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("path"), item.path));
        }
    } else if (network.compare(QStringLiteral("quic"), Qt::CaseInsensitive) == 0) {
        entries.append(qMakePair(
            QStringLiteral("headerType"),
            item.headerType.trimmed().isEmpty() ? QStringLiteral("none") : item.headerType));
        entries.append(qMakePair(
            QStringLiteral("quicSecurity"),
            item.requestHost.trimmed().isEmpty() ? QStringLiteral("none") : item.requestHost));
        if (!item.path.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("key"), item.path));
        }
    } else if (network.compare(QStringLiteral("grpc"), Qt::CaseInsensitive) == 0) {
        if (!item.path.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("serviceName"), item.path));
        }
        if (!item.headerType.trimmed().isEmpty()
            && (item.headerType.compare(QStringLiteral("gun"), Qt::CaseInsensitive) == 0
                || item.headerType.compare(QStringLiteral("multi"), Qt::CaseInsensitive) == 0)) {
            entries.append(qMakePair(QStringLiteral("mode"), item.headerType));
        }
    } else {
        entries.append(qMakePair(
            QStringLiteral("headerType"),
            item.headerType.trimmed().isEmpty() ? QStringLiteral("none") : item.headerType));
        if (!item.requestHost.trimmed().isEmpty()) {
            entries.append(qMakePair(QStringLiteral("host"), item.requestHost));
        }
    }

    return entries;
}

QString ShareUrlBuilder::buildQuery(const QList<QPair<QString, QString>>& entries)
{
    QStringList parts;
    for (const auto& entry : entries) {
        if (!entry.first.isEmpty() && !entry.second.isNull()) {
            parts.append(entry.first + QStringLiteral("=") + QString::fromUtf8(QUrl::toPercentEncoding(entry.second)));
        }
    }

    return parts.isEmpty() ? QString() : QStringLiteral("?") + parts.join(QStringLiteral("&"));
}

QString ShareUrlBuilder::buildRemark(const QString& remarks)
{
    return remarks.trimmed().isEmpty()
        ? QString()
        : QStringLiteral("#") + QString::fromUtf8(QUrl::toPercentEncoding(remarks));
}

QString ShareUrlBuilder::joinList(const QStringList& values)
{
    return values.join(QStringLiteral(","));
}

QString ShareUrlBuilder::encodeBase64(const QString& value)
{
    return QString::fromUtf8(value.toUtf8().toBase64());
}

QString ShareUrlBuilder::wrapIpv6(const QString& address)
{
    QHostAddress parsed;
    if (parsed.setAddress(address) && parsed.protocol() == QAbstractSocket::IPv6Protocol) {
        return QStringLiteral("[%1]").arg(address);
    }

    return address;
}
