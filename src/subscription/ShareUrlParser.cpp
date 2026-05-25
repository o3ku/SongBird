#include "subscription/ShareUrlParser.h"

#include "subscription/ShareUrlProtocolParsers.h"
#include "subscription/ShareUrlVmessParser.h"
#include "subscription/SubscriptionImportTextParser.h"

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
        return ShareUrlVmessParser::parseVmess(text, ok);
    }
    if (text.startsWith(QStringLiteral("ss://"), Qt::CaseInsensitive)) {
        return ShareUrlProtocolParsers::parseShadowsocks(text, ok);
    }
    if (text.startsWith(QStringLiteral("socks://"), Qt::CaseInsensitive)) {
        return ShareUrlProtocolParsers::parseSocks(text, ok);
    }
    if (text.startsWith(QStringLiteral("trojan://"), Qt::CaseInsensitive)) {
        return ShareUrlProtocolParsers::parseTrojan(text, ok);
    }
    if (text.startsWith(QStringLiteral("vless://"), Qt::CaseInsensitive)) {
        return ShareUrlProtocolParsers::parseVless(text, ok);
    }
    if (text.startsWith(QStringLiteral("hysteria2://"), Qt::CaseInsensitive)
        || text.startsWith(QStringLiteral("hy2://"), Qt::CaseInsensitive)) {
        return ShareUrlProtocolParsers::parseHysteria2(text, ok);
    }
    if (text.startsWith(QStringLiteral("tuic://"), Qt::CaseInsensitive)) {
        return ShareUrlProtocolParsers::parseTuic(text, ok);
    }
    if (text.startsWith(QStringLiteral("wg://"), Qt::CaseInsensitive)
        || text.startsWith(QStringLiteral("wireguard://"), Qt::CaseInsensitive)) {
        return ShareUrlProtocolParsers::parseWireguard(text, ok);
    }
    if (text.startsWith(QStringLiteral("anytls://"), Qt::CaseInsensitive)) {
        return ShareUrlProtocolParsers::parseAnytls(text, ok);
    }
    if (text.startsWith(QStringLiteral("naive+https://"), Qt::CaseInsensitive)
        || text.startsWith(QStringLiteral("naive+quic://"), Qt::CaseInsensitive)) {
        return ShareUrlProtocolParsers::parseNaive(text, ok);
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
