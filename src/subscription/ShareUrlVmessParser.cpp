#include "subscription/ShareUrlVmessParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include "common/PortValidator.h"
#include "subscription/ShareUrlParserSupport.h"

using ShareUrlParserSupport::decodeBase64;
using ShareUrlParserSupport::normalizeJsonText;
using ShareUrlParserSupport::parseInt;
using ShareUrlParserSupport::splitCsv;
using ShareUrlParserSupport::tryAssignUserInfo;
using ShareUrlParserSupport::tryParseAddressAndPort;

namespace {

VmessItem parseLegacyVmess(const QString& shareUrl)
{
    VmessItem item;
    const QString payload = shareUrl.mid(QStringLiteral("vmess://").size());
    const QString decoded = decodeBase64(payload);
    const QJsonDocument document = QJsonDocument::fromJson(decoded.toUtf8());
    if (!document.isObject()) {
        return {};
    }

    const QJsonObject object = document.object();
    item.configType = ConfigType::VMess;
    item.remarks = object.value(QStringLiteral("ps")).toString();
    item.address = object.value(QStringLiteral("add")).toString();
    item.port = parseInt(object.value(QStringLiteral("port")).toString());
    item.id = object.value(QStringLiteral("id")).toString();
    item.alterId = parseInt(object.value(QStringLiteral("aid")).toString());
    item.security = object.value(QStringLiteral("scy")).toString(QStringLiteral("auto"));
    item.network = object.value(QStringLiteral("net")).toString(QStringLiteral("tcp"));
    item.headerType = object.value(QStringLiteral("type")).toString(QStringLiteral("none"));
    item.requestHost = object.value(QStringLiteral("host")).toString();
    item.path = object.value(QStringLiteral("path")).toString();
    item.streamSecurity = object.value(QStringLiteral("tls")).toString();
    item.sni = object.value(QStringLiteral("sni")).toString();
    item.alpn = splitCsv(object.value(QStringLiteral("alpn")).toString());
    item.echConfigList = object.value(QStringLiteral("ech")).toString();
    item.fingerprint = object.value(QStringLiteral("fp")).toString();
    item.publicKey = object.value(QStringLiteral("pbk")).toString();
    item.shortId = object.value(QStringLiteral("sid")).toString();
    item.spiderX = object.value(QStringLiteral("spx")).toString();
    item.mldsa65Verify = object.value(QStringLiteral("pqv")).toString();
    item.finalmask = normalizeJsonText(object.value(QStringLiteral("fm")).toString());
    return item;
}

VmessItem parseStandardVmess(const QString& shareUrl)
{
    static const QRegularExpression userInfoPattern(
        QStringLiteral(
            R"(^(?<network>[a-z]+)(?:\+(?<streamSecurity>[a-z]+))?:(?<id>[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})$)"));

    QString payload = shareUrl.trimmed();
    if (!payload.startsWith(QStringLiteral("vmess://"), Qt::CaseInsensitive)) {
        return {};
    }

    payload.remove(0, QStringLiteral("vmess://").size());

    QString remarks;
    const int fragmentIndex = payload.indexOf(QChar('#'));
    if (fragmentIndex >= 0) {
        remarks = QUrl::fromPercentEncoding(payload.mid(fragmentIndex + 1).toUtf8());
        payload = payload.left(fragmentIndex);
    }

    QString queryString;
    const int queryIndex = payload.indexOf(QChar('?'));
    if (queryIndex >= 0) {
        queryString = payload.mid(queryIndex + 1);
        payload = payload.left(queryIndex);
    }

    if (payload.endsWith(QChar('/'))) {
        payload.chop(1);
    }

    const int atIndex = payload.lastIndexOf(QChar('@'));
    if (atIndex <= 0) {
        return {};
    }

    const QString userInfo = payload.left(atIndex);
    QString address;
    int port = 0;
    if (!tryParseAddressAndPort(payload.mid(atIndex + 1), address, port)) {
        return {};
    }

    const QRegularExpressionMatch match = userInfoPattern.match(userInfo);
    if (!match.hasMatch()) {
        return {};
    }

    VmessItem item;
    item.configType = ConfigType::VMess;
    item.security = QStringLiteral("auto");
    item.address = address;
    item.port = port;
    item.remarks = remarks;
    item.id = match.captured(QStringLiteral("id"));
    item.network = match.captured(QStringLiteral("network")).toLower();
    item.streamSecurity = match.captured(QStringLiteral("streamSecurity")).toLower();

    QUrlQuery query;
    query.setQuery(queryString);
    const auto decodedQueryValue = [&query](const QString& key) {
        return QUrl::fromPercentEncoding(query.queryItemValue(key).toUtf8());
    };
    if (item.network.compare(QStringLiteral("tcp"), Qt::CaseInsensitive) == 0) {
        item.headerType = query.queryItemValue(QStringLiteral("type"));
        if (item.headerType.isEmpty()) {
            item.headerType = QStringLiteral("none");
        }
        item.requestHost = decodedQueryValue(QStringLiteral("host"));
        item.path = decodedQueryValue(QStringLiteral("path"));
    } else if (item.network.compare(QStringLiteral("kcp"), Qt::CaseInsensitive) == 0) {
        item.headerType = query.queryItemValue(QStringLiteral("type"));
        if (item.headerType.isEmpty()) {
            item.headerType = QStringLiteral("none");
        }
    } else if (item.network.compare(QStringLiteral("ws"), Qt::CaseInsensitive) == 0) {
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
    } else if (item.network.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
        || item.network.compare(QStringLiteral("h2"), Qt::CaseInsensitive) == 0) {
        item.network = QStringLiteral("h2");
        item.requestHost = decodedQueryValue(QStringLiteral("host"));
        item.path = decodedQueryValue(QStringLiteral("path"));
    } else if (item.network.compare(QStringLiteral("quic"), Qt::CaseInsensitive) == 0) {
        item.headerType = query.queryItemValue(QStringLiteral("type"));
        if (item.headerType.isEmpty()) {
            item.headerType = QStringLiteral("none");
        }
        item.requestHost = decodedQueryValue(QStringLiteral("security"));
        item.path = decodedQueryValue(QStringLiteral("key"));
    } else {
        return {};
    }

    return item;
}

VmessItem parseKitsunebiVmess(const QString& shareUrl)
{
    QString payload = shareUrl.trimmed();
    if (!payload.startsWith(QStringLiteral("vmess://"), Qt::CaseInsensitive)) {
        return {};
    }

    payload.remove(0, QStringLiteral("vmess://").size());

    const int queryIndex = payload.indexOf(QChar('?'));
    if (queryIndex >= 0) {
        payload = payload.left(queryIndex);
    }

    const QString decoded = decodeBase64(payload).trimmed();
    const int atIndex = decoded.indexOf(QChar('@'));
    if (atIndex <= 0) {
        return {};
    }

    QString first;
    QString second;
    if (!tryAssignUserInfo(decoded.left(atIndex), first, second)) {
        return {};
    }

    VmessItem item;
    item.configType = ConfigType::VMess;
    item.security = first;
    item.id = second;
    if (!tryParseAddressAndPort(decoded.mid(atIndex + 1), item.address, item.port)) {
        return {};
    }
    item.network = QStringLiteral("tcp");
    item.headerType = QStringLiteral("none");
    item.remarks = QStringLiteral("Alien");
    return item;
}

} // namespace

namespace ShareUrlVmessParser {

VmessItem parseVmess(const QString& shareUrl, bool* ok)
{
    VmessItem item = parseLegacyVmess(shareUrl);
    if (item.address.isEmpty() || item.port <= 0 || item.id.isEmpty()) {
        item = parseStandardVmess(shareUrl);
    }
    if (item.address.isEmpty() || item.port <= 0 || item.id.isEmpty()) {
        item = parseKitsunebiVmess(shareUrl);
    }

    if (ok != nullptr) {
        *ok = !item.address.isEmpty() && isValidTcpPort(item.port) && !item.id.isEmpty();
    }

    return item;
}

} // namespace ShareUrlVmessParser
