#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

struct SubscriptionParseReport {
    QList<VmessItem> items;
    QStringList notes;
};

class SubscriptionContentParser {
public:
    static QList<VmessItem> parseMany(const QString& content);
    static SubscriptionParseReport parseManyWithReport(const QString& content);
    static QList<VmessItem> tryParseClashProxyArray(const QJsonArray& proxies);

private:
    static QList<VmessItem> tryParseClash(const QString& content);
    static QList<VmessItem> tryParseJsonArray(const QJsonArray& array);
    static QList<VmessItem> tryParse(const QString& content);
    static QList<VmessItem> tryParseSingBox(const QString& content);
    static QList<VmessItem> tryParseSingBoxOutboundObject(const QJsonObject& object);
    static QList<VmessItem> tryParseSip008(const QString& content);
    static QString tryDecodeBase64(const QString& content);
};
