#include "subscription/ShareUrlParser.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include "common/PortValidator.h"
#include "subscription/SubscriptionImportTextParser.h"

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

QString decodedUserName(const QUrl& url)
{
    return QUrl::fromPercentEncoding(url.userName().toUtf8());
}

QString decodedPassword(const QUrl& url)
{
    return QUrl::fromPercentEncoding(url.password().toUtf8());
}

} // namespace

VmessItem ShareUrlParser::parse(const QString& shareUrl, bool* ok)
{
    if (ok != nullptr) {
        *ok = false;
    }

    const QString text = shareUrl.trimmed();
    if (text.isEmpty()) {
        return {};
    }

    if (text.startsWith(QStringLiteral("vmess://"), Qt::CaseInsensitive)) {
        return parseVmess(text, ok);
    }
    if (text.startsWith(QStringLiteral("ss://"), Qt::CaseInsensitive)) {
        return parseShadowsocks(text, ok);
    }
    if (text.startsWith(QStringLiteral("socks://"), Qt::CaseInsensitive)) {
        return parseSocks(text, ok);
    }
    if (text.startsWith(QStringLiteral("trojan://"), Qt::CaseInsensitive)) {
        return parseTrojan(text, ok);
    }
    if (text.startsWith(QStringLiteral("vless://"), Qt::CaseInsensitive)) {
        return parseVless(text, ok);
    }
    if (text.startsWith(QStringLiteral("hysteria2://"), Qt::CaseInsensitive)
        || text.startsWith(QStringLiteral("hy2://"), Qt::CaseInsensitive)) {
        return parseHysteria2(text, ok);
    }
    if (text.startsWith(QStringLiteral("tuic://"), Qt::CaseInsensitive)) {
        return parseTuic(text, ok);
    }
    if (text.startsWith(QStringLiteral("wg://"), Qt::CaseInsensitive)
        || text.startsWith(QStringLiteral("wireguard://"), Qt::CaseInsensitive)) {
        return parseWireguard(text, ok);
    }
    if (text.startsWith(QStringLiteral("anytls://"), Qt::CaseInsensitive)) {
        return parseAnytls(text, ok);
    }
    if (text.startsWith(QStringLiteral("naive+https://"), Qt::CaseInsensitive)
        || text.startsWith(QStringLiteral("naive+quic://"), Qt::CaseInsensitive)) {
        return parseNaive(text, ok);
    }

    return {};
}

QList<VmessItem> ShareUrlParser::parseMany(const QString& text)
{
    QList<VmessItem> items;
    const QStringList lines = SubscriptionImportTextParser::nonEmptyLines(text);
    for (const QString& line : lines) {
        bool ok = false;
        const VmessItem item = parse(line, &ok);
        if (ok) {
            items.append(item);
        }
    }

    return items;
}

VmessItem ShareUrlParser::parseVmess(const QString& shareUrl, bool* ok)
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

VmessItem ShareUrlParser::parseLegacyVmess(const QString& shareUrl)
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

VmessItem ShareUrlParser::parseStandardVmess(const QString& shareUrl)
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

VmessItem ShareUrlParser::parseKitsunebiVmess(const QString& shareUrl)
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

VmessItem ShareUrlParser::parseShadowsocks(const QString& shareUrl, bool* ok)
{
    VmessItem item;
    QUrl url(shareUrl);
    if (!url.isValid()) {
        item = parseLegacyShadowsocks(shareUrl);
        if (ok != nullptr) {
            *ok = !item.address.isEmpty() && isValidTcpPort(item.port) && !item.id.isEmpty();
        }
        return item;
    }

    QString encodedUserInfo = url.userName();
    if (!url.password().isEmpty()) {
        encodedUserInfo.append(QStringLiteral(":")).append(url.password());
    }

    QString credentials = encodedUserInfo;
    if (!credentials.contains(':')) {
        credentials = decodeBase64(credentials);
    }

    const QStringList parts = credentials.split(':');
    if (parts.size() < 2 || url.host().trimmed().isEmpty() || url.port() <= 0) {
        item = parseLegacyShadowsocks(shareUrl);
        if (ok != nullptr) {
            *ok = !item.address.isEmpty() && isValidTcpPort(item.port) && !item.id.isEmpty();
        }
        return item;
    }

    item.configType = ConfigType::Shadowsocks;
    item.remarks = url.fragment(QUrl::FullyDecoded);
    item.address = url.host();
    item.port = url.port();
    item.security = parts.at(0);
    item.id = parts.mid(1).join(QStringLiteral(":"));

    if (ok != nullptr) {
        *ok = !item.address.isEmpty() && isValidTcpPort(item.port) && !item.id.isEmpty();
    }

    return item;
}

VmessItem ShareUrlParser::parseSocks(const QString& shareUrl, bool* ok)
{
    VmessItem item;
    const QUrl url(shareUrl);
    if (!url.isValid()) {
        item = parseLegacySocks(shareUrl);
        if (ok != nullptr) {
            *ok = !item.address.isEmpty() && isValidTcpPort(item.port);
        }
        return item;
    }

    item.configType = ConfigType::Socks;
    item.remarks = url.fragment(QUrl::FullyDecoded);
    item.address = url.host();
    item.port = url.port();
    item.id = decodedUserName(url);
    item.security = decodedPassword(url);

    if (item.address.trimmed().isEmpty() || item.port <= 0) {
        item = parseLegacySocks(shareUrl);
    } else if (!url.userName().isEmpty() && url.password().isEmpty()) {
        const QString encodedUserInfo = url.userName();
        QString first;
        QString second;
        if (tryAssignUserInfo(encodedUserInfo, first, second, true)) {
            item.id = first;
            item.security = second;
        } else {
            item = parseLegacySocks(shareUrl);
        }
    }

    if (ok != nullptr) {
        *ok = !item.address.isEmpty() && isValidTcpPort(item.port);
    }

    return item;
}

VmessItem ShareUrlParser::parseTrojan(const QString& shareUrl, bool* ok)
{
    VmessItem item;
    const QUrl url(shareUrl);
    if (!url.isValid()) {
        return item;
    }

    item.configType = ConfigType::Trojan;
    item.remarks = url.fragment(QUrl::FullyDecoded);
    item.address = url.host();
    item.port = url.port();
    item.id = decodedUserName(url);

    resolveStandardTransport(QUrlQuery(url), item);

    if (ok != nullptr) {
        *ok = !item.address.isEmpty() && isValidTcpPort(item.port) && !item.id.isEmpty();
    }

    return item;
}

VmessItem ShareUrlParser::parseVless(const QString& shareUrl, bool* ok)
{
    VmessItem item;
    const QUrl url(shareUrl);
    if (!url.isValid()) {
        return item;
    }

    const QUrlQuery query(url);
    item.configType = ConfigType::VLESS;
    item.remarks = url.fragment(QUrl::FullyDecoded);
    item.address = url.host();
    item.port = url.port();
    item.id = decodedUserName(url);
    item.security = query.queryItemValue(QStringLiteral("encryption"));
    if (item.security.isEmpty()) {
        item.security = QStringLiteral("none");
    }

    resolveStandardTransport(query, item);

    if (ok != nullptr) {
        *ok = !item.address.isEmpty() && isValidTcpPort(item.port) && !item.id.isEmpty();
    }

    return item;
}

VmessItem ShareUrlParser::parseHysteria2(const QString& shareUrl, bool* ok)
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
    // hysteria2://password@host:port or hy2://password@host:port
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

VmessItem ShareUrlParser::parseTuic(const QString& shareUrl, bool* ok)
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
    // tuic://uuid:password@host:port
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

VmessItem ShareUrlParser::parseWireguard(const QString& shareUrl, bool* ok)
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

VmessItem ShareUrlParser::parseAnytls(const QString& shareUrl, bool* ok)
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
    // anytls://password@host:port
    item.id = decodedUserName(url);

    item.sni = query.queryItemValue(QStringLiteral("sni"));
    item.streamSecurity = QStringLiteral("tls");
    item.allowInsecure = query.queryItemValue(QStringLiteral("allowInsecure"));
    if (item.allowInsecure.isEmpty()) {
        item.allowInsecure = query.queryItemValue(QStringLiteral("insecure"));
    }
    item.fingerprint = query.queryItemValue(QStringLiteral("fp"));
    item.alpn = splitCsv(QUrl::fromPercentEncoding(query.queryItemValue(QStringLiteral("alpn")).toUtf8()));

    if (ok != nullptr) {
        *ok = !item.address.isEmpty() && isValidTcpPort(item.port) && !item.id.isEmpty();
    }

    return item;
}

VmessItem ShareUrlParser::parseNaive(const QString& shareUrl, bool* ok)
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
    // naive+https://user:pass@host:port  or naive+quic://user:pass@host:port
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

VmessItem ShareUrlParser::parseLegacyShadowsocks(const QString& shareUrl)
{
    VmessItem item;
    item.configType = ConfigType::Shadowsocks;

    QString payload = shareUrl.trimmed();
    if (!payload.startsWith(QStringLiteral("ss://"), Qt::CaseInsensitive)) {
        return {};
    }

    payload.remove(0, QStringLiteral("ss://").size());

    const int fragmentIndex = payload.indexOf(QChar('#'));
    if (fragmentIndex >= 0) {
        const QString tag = payload.mid(fragmentIndex + 1);
        item.remarks = QUrl::fromPercentEncoding(tag.toUtf8());
        payload = payload.left(fragmentIndex);
    }

    payload = payload.trimmed();
    if (payload.isEmpty()) {
        return {};
    }

    if (payload.endsWith(QChar('/'))) {
        payload.chop(1);
    }

    const QString decoded = decodeBase64(payload).trimmed();
    const int atIndex = decoded.lastIndexOf(QChar('@'));
    const int colonIndex = decoded.lastIndexOf(QChar(':'));
    const int methodSeparator = decoded.indexOf(QChar(':'));
    if (atIndex <= 0 || colonIndex <= atIndex || methodSeparator <= 0 || methodSeparator >= atIndex) {
        return {};
    }

    item.security = decoded.left(methodSeparator);
    item.id = decoded.mid(methodSeparator + 1, atIndex - methodSeparator - 1);
    item.address = decoded.mid(atIndex + 1, colonIndex - atIndex - 1);
    item.port = parseInt(decoded.mid(colonIndex + 1));
    return item;
}

VmessItem ShareUrlParser::parseLegacySocks(const QString& shareUrl)
{
    VmessItem item;
    item.configType = ConfigType::Socks;

    QString payload = shareUrl.trimmed();
    if (!payload.startsWith(QStringLiteral("socks://"), Qt::CaseInsensitive)) {
        return {};
    }

    payload.remove(0, QStringLiteral("socks://").size());

    const int fragmentIndex = payload.indexOf(QChar('#'));
    if (fragmentIndex >= 0) {
        const QString tag = payload.mid(fragmentIndex + 1);
        item.remarks = QUrl::fromPercentEncoding(tag.toUtf8());
        payload = payload.left(fragmentIndex);
    }

    payload = payload.trimmed();
    if (payload.isEmpty()) {
        return {};
    }

    if (!payload.contains(QChar('@'))) {
        payload = decodeBase64(payload).trimmed();
    }

    const int atIndex = payload.indexOf(QChar('@'));
    const int portIndex = payload.lastIndexOf(QChar(':'));
    if (atIndex <= 0 || portIndex <= atIndex) {
        return {};
    }

    QString first;
    QString second;
    if (!tryAssignUserInfo(payload.left(atIndex), first, second, true)) {
        return {};
    }

    item.id = first;
    item.security = second;
    item.address = payload.mid(atIndex + 1, portIndex - atIndex - 1);
    item.port = parseInt(payload.mid(portIndex + 1));
    return item;
}

bool ShareUrlParser::tryAssignUserInfo(QString credentials, QString& first, QString& second, bool allowEmpty)
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

bool ShareUrlParser::tryParseAddressAndPort(const QString& endpoint, QString& address, int& port)
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

void ShareUrlParser::resolveStandardTransport(const QUrlQuery& query, VmessItem& item)
{
    const auto decodedQueryValue = [&query](const QString& key) {
        return QUrl::fromPercentEncoding(query.queryItemValue(key).toUtf8());
    };
    item.flow = query.queryItemValue(QStringLiteral("flow"));
    item.streamSecurity = query.queryItemValue(QStringLiteral("security"));
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

QString ShareUrlParser::decodeBase64(const QString& value)
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

QStringList ShareUrlParser::splitCsv(const QString& value)
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

int ShareUrlParser::parseInt(const QString& value)
{
    bool ok = false;
    const int result = value.toInt(&ok);
    return ok ? result : 0;
}
