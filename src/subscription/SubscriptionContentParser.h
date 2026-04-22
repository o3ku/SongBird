#pragma once

#include <QList>
#include <QString>

#include "domain/models/VmessItem.h"

class SubscriptionContentParser {
public:
    static QList<VmessItem> parseMany(const QString& content);

private:
    static QList<VmessItem> tryParse(const QString& content);
    static QList<VmessItem> tryParseSip008(const QString& content);
    static QString tryDecodeBase64(const QString& content);
};
