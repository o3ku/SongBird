#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

struct SubscriptionParseReport {
    QList<VmessItem> items;
    QStringList notes;
    // One entry per node the parser could not turn into a server: the declared
    // type, plus the node's name, which is the only way to find it again in a
    // document with thousands of them.
    //
    // Covers every pass, not only the one that produced items, so a run where
    // nothing at all was imported still explains itself. Entries are never
    // collapsed into one another, so size() is the skipped node count: two nodes
    // that share a name are two nodes. What is not counted twice is a document
    // read by two readers in one pass -- that is one reading, not two.
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
