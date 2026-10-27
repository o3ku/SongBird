#pragma once

#include <QCoreApplication>
#include <QString>

namespace SubscriptionSkippedNode {

// One entry per node the parser could not turn into a server, rendered for the
// user: the declared type, plus the node's name when the parser had one.
//
// The name is the whole point. A type alone says what went wrong, never which node
// it happened to, and that is unusable on the subscriptions that actually fail --
// the ones with two thousand nodes. Every reader (Clash JSON, Clash YAML, sing-box)
// goes through this, so the same airport does not change granularity with the
// format it happens to be served in.
inline QString label(const QString& type, const QString& name)
{
    QString typeLabel = type.trimmed();
    if (typeLabel.isEmpty()) {
        typeLabel = QCoreApplication::translate("SubscriptionContentParser", "(no type)");
    }
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return typeLabel;
    }
    return QStringLiteral("%1 \"%2\"").arg(typeLabel, trimmedName);
}

// The type half of an entry, for the prose line that summarises them. Only the
// prose: the count beside it is the entry count, because an entry is a node. Reading
// the type back out of the label is why the label is a fixed shape.
inline QString typeName(const QString& entry)
{
    const int quote = entry.indexOf(QLatin1String(" \""));
    return quote > 0 && entry.endsWith(QChar('"')) ? entry.left(quote) : entry;
}

} // namespace SubscriptionSkippedNode
