#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

class SubscriptionImportTextParser {
public:
    static QStringList nonEmptyLines(const QString& text);
    static bool isSubscriptionUrl(const QString& text);
    static QString extractHostOrIpFromUrl(const QString& url);
    // The hosts plain http is allowed for: a subscription fetched over http sends its
    // content -- node credentials included -- in clear text, so http is only ever a
    // loopback convenience, never a way to fetch from the network.
    static bool isLoopbackHost(const QString& host);
    // Whether a subscription URL may be fetched: https to any host, or http to a loopback
    // host only. Anything else (another scheme, http to a non-loopback host, no host at
    // all) must be refused before any network I/O happens.
    static bool isAllowedSubscriptionUrl(const QString& url);
    static bool isAllowedSubscriptionUrl(const QUrl& parsedUrl);
};
