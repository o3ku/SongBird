#pragma once

#include <QString>
#include <QStringList>

class SubscriptionImportTextParser {
public:
    static QStringList nonEmptyLines(const QString& text);
    static bool isSubscriptionUrl(const QString& text);
    static bool isKnownServerShareUrl(const QString& text);
    static QString extractHostOrIpFromUrl(const QString& url);
};
