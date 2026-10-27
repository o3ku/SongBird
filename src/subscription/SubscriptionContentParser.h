#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

struct SubscriptionParseReport {
    QList<VmessItem> items;
    QStringList notes;
    // One entry per node the parser could not turn into a server, holding that
    // node's declared type (and its name where the parser had one). Covers every
    // pass, not only the one that produced items, so a run where nothing at all
    // was imported still explains itself. Identical entries are collapsed, so
    // size() is the skipped node count rather than the parse-attempt count.
    QStringList skippedTypes;

    bool hasSkippedNodes() const { return !skippedTypes.isEmpty(); }

    // Human readable one-liner, e.g. "skipped 2 node(s): anytls, wireguard".
    // Empty when nothing was skipped.
    static QString skippedSummary(const QStringList& types);
    QString skippedSummary() const;
};

class SubscriptionContentParser {
public:
    static QList<VmessItem> parseMany(const QString& content);
    static SubscriptionParseReport parseManyWithReport(const QString& content);

private:
    static QList<VmessItem> tryParseClash(const QString& content, QStringList* skippedTypes);
    static QList<VmessItem> tryParseJsonArray(const QJsonArray& array, QStringList* skippedTypes);
    static QList<VmessItem> tryParseSingBox(const QString& content, QStringList* skippedTypes);
    static QList<VmessItem> tryParseSip008(const QString& content);
    static QString tryDecodeBase64(const QString& content);
};
