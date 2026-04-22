#pragma once

#include <QList>
#include <QString>
#include <QUrlQuery>

#include "domain/models/VmessItem.h"

class ShareUrlParser {
public:
    static VmessItem parse(const QString& shareUrl, bool* ok = nullptr);
    static QList<VmessItem> parseMany(const QString& text);

private:
    static VmessItem parseVmess(const QString& shareUrl, bool* ok);
    static VmessItem parseShadowsocks(const QString& shareUrl, bool* ok);
    static VmessItem parseSocks(const QString& shareUrl, bool* ok);
    static VmessItem parseTrojan(const QString& shareUrl, bool* ok);
    static VmessItem parseVless(const QString& shareUrl, bool* ok);
    static VmessItem parseHysteria2(const QString& shareUrl, bool* ok);
    static VmessItem parseTuic(const QString& shareUrl, bool* ok);
    static VmessItem parseWireguard(const QString& shareUrl, bool* ok);
    static VmessItem parseAnytls(const QString& shareUrl, bool* ok);
    static VmessItem parseNaive(const QString& shareUrl, bool* ok);
    static VmessItem parseLegacyVmess(const QString& shareUrl);
    static VmessItem parseStandardVmess(const QString& shareUrl);
    static VmessItem parseKitsunebiVmess(const QString& shareUrl);
    static VmessItem parseLegacyShadowsocks(const QString& shareUrl);
    static VmessItem parseLegacySocks(const QString& shareUrl);
    static bool tryAssignUserInfo(QString credentials, QString& first, QString& second);
    static bool tryParseAddressAndPort(const QString& endpoint, QString& address, int& port);
    static void resolveStandardTransport(const QUrlQuery& query, VmessItem& item);
    static QString decodeBase64(const QString& value);
    static QStringList splitCsv(const QString& value);
    static int parseInt(const QString& value);
};
