#include "subscription/ShareUrlParserSupport.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include "common/PortValidator.h"

namespace ShareUrlParserSupport {

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

QString decodedUserName(const QUrl& url)
{
    return QUrl::fromPercentEncoding(url.userName().toUtf8());
}

QString decodedPassword(const QUrl& url)
{
    return QUrl::fromPercentEncoding(url.password().toUtf8());
}

QString firstNonEmptyQueryValue(const QUrlQuery& query, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString value = query.queryItemValue(key);
        if (!value.isEmpty()) {
            return value;
        }
    }

    return {};
}

QString decodeBase64(const QString& value)
{
    QByteArray normalized = value.trimmed().toUtf8();
    normalized.replace('-', '+');
    normalized.replace('_', '/');

    const int padding = normalized.size() % 4;
    if (padding > 0) {
        normalized.append(QByteArray(4 - padding, '='));
    }

    return QByteArray::fromBase64(normalized);
}

QStringList splitCsv(const QString& value)
{
    QStringList items;
    const QStringList rawItems = value.split(',', Qt::SkipEmptyParts);
    for (const QString& rawItem : rawItems) {
        const QString item = rawItem.trimmed();
        if (!item.isEmpty()) {
            items.append(item);
        }
    }

    return items;
}

int parseInt(const QString& value)
{
    bool ok = false;
    const int result = value.toInt(&ok);
    return ok ? result : 0;
}

bool tryAssignUserInfo(QString credentials, QString& first, QString& second, bool allowEmpty)
{
    credentials = credentials.trimmed();
    if (credentials.isEmpty()) {
        return false;
    }

    const QString decoded = decodeBase64(credentials).trimmed();
    if (decoded.contains(QChar(':'))) {
        credentials = decoded;
    }

    const int separatorIndex = credentials.indexOf(QChar(':'));
    if (separatorIndex < 0) {
        return false;
    }

    first = credentials.left(separatorIndex);
    second = credentials.mid(separatorIndex + 1);
    return allowEmpty || !first.isEmpty() || !second.isEmpty();
}

bool tryParseAddressAndPort(const QString& endpoint, QString& address, int& port)
{
    const QString trimmed = endpoint.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    if (trimmed.startsWith(QStringLiteral("["))) {
        const int closingIndex = trimmed.indexOf(QStringLiteral("]:"));
        if (closingIndex <= 0) {
            return false;
        }

        address = trimmed.mid(1, closingIndex - 1);
        port = parseInt(trimmed.mid(closingIndex + 2));
        return !address.isEmpty() && isValidTcpPort(port);
    }

    const int separatorIndex = trimmed.lastIndexOf(QChar(':'));
    if (separatorIndex <= 0) {
        return false;
    }

    address = trimmed.left(separatorIndex);
    port = parseInt(trimmed.mid(separatorIndex + 1));
    return !address.isEmpty() && isValidTcpPort(port);
}

void resolveStandardTransport(const QUrlQuery& query, VmessItem& item)
{
    const auto decodedQueryValue = [&query](const QString& key) {
        return QUrl::fromPercentEncoding(query.queryItemValue(key).toUtf8());
    };
    item.flow = query.queryItemValue(QStringLiteral("flow"));
    item.streamSecurity = query.queryItemValue(QStringLiteral("security"));
    item.allowInsecure = firstNonEmptyQueryValue(
        query,
        {QStringLiteral("allowInsecure"), QStringLiteral("allow_insecure"), QStringLiteral("insecure")});
    item.sni = decodedQueryValue(QStringLiteral("sni"));
    item.alpn = splitCsv(decodedQueryValue(QStringLiteral("alpn")));
    item.echConfigList = decodedQueryValue(QStringLiteral("ech"));
    item.fingerprint = decodedQueryValue(QStringLiteral("fp"));
    item.certSha = decodedQueryValue(QStringLiteral("pcs"));
    item.publicKey = decodedQueryValue(QStringLiteral("pbk"));
    item.shortId = decodedQueryValue(QStringLiteral("sid"));
    item.spiderX = decodedQueryValue(QStringLiteral("spx"));
    item.mldsa65Verify = decodedQueryValue(QStringLiteral("pqv"));
    item.finalmask = normalizeJsonText(decodedQueryValue(QStringLiteral("fm")));
    item.packetEncoding = firstNonEmptyQueryValue(
        query,
        {QStringLiteral("packetEncoding"), QStringLiteral("packet_encoding")});
    item.network = query.queryItemValue(QStringLiteral("type"));
    if (item.network.isEmpty()) {
        item.network = QStringLiteral("tcp");
    }

    if (item.network.compare(QStringLiteral("ws"), Qt::CaseInsensitive) == 0) {
        item.requestHost = decodedQueryValue(QStringLiteral("host"));
        item.path = decodedQueryValue(QStringLiteral("path"));
    } else if (item.network.compare(QStringLiteral("httpupgrade"), Qt::CaseInsensitive) == 0) {
        item.requestHost = decodedQueryValue(QStringLiteral("host"));
        item.path = decodedQueryValue(QStringLiteral("path"));
    } else if (item.network.compare(QStringLiteral("xhttp"), Qt::CaseInsensitive) == 0) {
        item.requestHost = decodedQueryValue(QStringLiteral("host"));
        item.path = decodedQueryValue(QStringLiteral("path"));
        item.headerType = decodedQueryValue(QStringLiteral("mode"));
        item.extra = normalizeJsonText(decodedQueryValue(QStringLiteral("extra")));
    } else if (item.network.compare(QStringLiteral("kcp"), Qt::CaseInsensitive) == 0) {
        item.headerType = query.queryItemValue(QStringLiteral("headerType"));
        if (item.headerType.isEmpty()) {
            item.headerType = QStringLiteral("none");
        }
        item.path = decodedQueryValue(QStringLiteral("seed"));
    } else if (item.network.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
        || item.network.compare(QStringLiteral("h2"), Qt::CaseInsensitive) == 0) {
        item.network = QStringLiteral("h2");
        item.requestHost = decodedQueryValue(QStringLiteral("host"));
        item.path = decodedQueryValue(QStringLiteral("path"));
    } else if (item.network.compare(QStringLiteral("quic"), Qt::CaseInsensitive) == 0) {
        item.headerType = query.queryItemValue(QStringLiteral("headerType"));
        if (item.headerType.isEmpty()) {
            item.headerType = QStringLiteral("none");
        }
        item.requestHost = decodedQueryValue(QStringLiteral("quicSecurity"));
        if (item.requestHost.isEmpty()) {
            item.requestHost = QStringLiteral("none");
        }
        item.path = decodedQueryValue(QStringLiteral("key"));
    } else if (item.network.compare(QStringLiteral("grpc"), Qt::CaseInsensitive) == 0) {
        item.requestHost = decodedQueryValue(QStringLiteral("authority"));
        item.path = decodedQueryValue(QStringLiteral("serviceName"));
        item.headerType = decodedQueryValue(QStringLiteral("mode"));
    } else {
        item.headerType = query.queryItemValue(QStringLiteral("headerType"));
        if (item.headerType.isEmpty()) {
            item.headerType = QStringLiteral("none");
        }
        item.requestHost = decodedQueryValue(QStringLiteral("host"));
    }
}

} // namespace ShareUrlParserSupport
