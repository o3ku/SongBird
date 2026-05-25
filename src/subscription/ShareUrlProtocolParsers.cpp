#include "subscription/ShareUrlProtocolParsers.h"

#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include "common/PortValidator.h"
#include "subscription/ShareUrlParserSupport.h"

using ShareUrlParserSupport::decodeBase64;
using ShareUrlParserSupport::decodedPassword;
using ShareUrlParserSupport::decodedUserName;
using ShareUrlParserSupport::parseInt;
using ShareUrlParserSupport::resolveStandardTransport;
using ShareUrlParserSupport::tryAssignUserInfo;

namespace {

VmessItem parseLegacyShadowsocks(const QString& shareUrl)
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

VmessItem parseLegacySocks(const QString& shareUrl)
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

} // namespace

namespace ShareUrlProtocolParsers {

VmessItem parseShadowsocks(const QString& shareUrl, bool* ok)
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

VmessItem parseSocks(const QString& shareUrl, bool* ok)
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

VmessItem parseTrojan(const QString& shareUrl, bool* ok)
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

VmessItem parseVless(const QString& shareUrl, bool* ok)
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

} // namespace ShareUrlProtocolParsers
