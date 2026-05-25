#include "subscription/ShareUrlBuilderSupport.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStringList>
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

namespace ShareUrlBuilderSupport {

QString encodeUserInfoPart(const QString& value)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(value));
}

QueryEntries buildStandardTransportEntries(const VmessItem& item, const QString& defaultSecurity)
{
    QueryEntries entries;
    if (!item.flow.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("flow"), item.flow));
    }
    if (!item.packetEncoding.trimmed().isEmpty()) {
        entries.append(qMakePair(QStringLiteral("packetEncoding"), item.packetEncoding));
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
            entries.append(qMakePair(QStringLiteral("extra"), normalizeJsonText(item.extra)));
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

QString buildQuery(const QueryEntries& entries)
{
    QStringList parts;
    for (const auto& entry : entries) {
        if (!entry.first.isEmpty() && !entry.second.isNull()) {
            parts.append(entry.first + QStringLiteral("=") + QString::fromUtf8(QUrl::toPercentEncoding(entry.second)));
        }
    }

    return parts.isEmpty() ? QString() : QStringLiteral("?") + parts.join(QStringLiteral("&"));
}

QString buildRemark(const QString& remarks)
{
    return remarks.trimmed().isEmpty()
        ? QString()
        : QStringLiteral("#") + QString::fromUtf8(QUrl::toPercentEncoding(remarks));
}

QString joinList(const QStringList& values)
{
    return values.join(QStringLiteral(","));
}

QString encodeBase64(const QString& value)
{
    return QString::fromUtf8(value.toUtf8().toBase64());
}

QString wrapIpv6(const QString& address)
{
    QHostAddress parsed;
    if (parsed.setAddress(address) && parsed.protocol() == QAbstractSocket::IPv6Protocol) {
        return QStringLiteral("[%1]").arg(address);
    }

    return address;
}

} // namespace ShareUrlBuilderSupport
