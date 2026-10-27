#include "subscription/ClashYamlProxyParser.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

#include "subscription/ClashProxyItemParser.h"
#include "subscription/SubscriptionYamlParser.h"

namespace {

// A node that lands in the array but yields no item is still a node we saw and
// could not use, so its type belongs in the same skipped report as an
// unsupported one. Nothing else records it: by the time the caller looks, the
// node is indistinguishable from one that was never in the document. A type
// alone cannot be acted on -- "skipped 1 node(s): (no type)" names no node the
// user could go and fix -- so the name travels with it whenever the document
// gave one.
QString skippedEntryLabel(const QJsonObject& node)
{
    const QString type = node.value(QStringLiteral("type")).toString().trimmed();
    const QString name = node.value(QStringLiteral("name")).toString().trimmed();
    const QString typeLabel = type.isEmpty()
        ? QCoreApplication::translate("SubscriptionContentParser", "(no type)")
        : type;
    if (name.isEmpty()) {
        return typeLabel;
    }
    return QStringLiteral("%1 \"%2\"").arg(typeLabel, name);
}

} // namespace

namespace ClashYamlProxyParser {

QList<VmessItem> parseProxyItems(const QString& content, QStringList* skippedTypes)
{
    QList<VmessItem> items;
    const QStringList lines = content.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::KeepEmptyParts);

    bool inProxies = false;
    int proxiesIndent = -1;
    QJsonObject current;
    // A node the document opened with "- " but whose fields never arrived. It has
    // no type to report, so it needs its own entry rather than falling through
    // the "unsupported node" path.
    bool nodeWithoutFields = false;
    // A flow-style item that is not a well-formed object. It is reported where it
    // is read, because the block-style fields that follow do not belong to it and
    // must not be added to a fragment of it.
    QString brokenFlowItemLabel;

    const auto reportNodeWithoutFields = [&nodeWithoutFields, skippedTypes]() {
        if (!nodeWithoutFields) {
            return;
        }
        nodeWithoutFields = false;
        if (skippedTypes != nullptr) {
            skippedTypes->append(QCoreApplication::translate(
                "SubscriptionContentParser", "(no fields) here"));
        }
    };

    const auto flushCurrent = [&items, &current, &nodeWithoutFields, &brokenFlowItemLabel,
                               &reportNodeWithoutFields, skippedTypes]() {
        reportNodeWithoutFields();
        if (!brokenFlowItemLabel.isEmpty()) {
            if (skippedTypes != nullptr) {
                skippedTypes->append(brokenFlowItemLabel);
            }
            brokenFlowItemLabel.clear();
        }
        if (current.isEmpty()) {
            return;
        }

        QJsonArray proxyArray;
        proxyArray.append(current);
        QStringList nodeSkipped;
        const QList<VmessItem> parsed = ClashProxyItemParser::parseProxyArray(proxyArray, &nodeSkipped);
        if (parsed.isEmpty() && skippedTypes != nullptr) {
            // parseProxyArray records the type it recognised but could not use. When
            // it recorded nothing the node was dropped for a reason it does not
            // report, which is precisely the silent loss worth naming -- and the
            // name is the only way to find that node in the document again.
            if (nodeSkipped.isEmpty()) {
                skippedTypes->append(skippedEntryLabel(current));
            } else {
                // Keep the label the parser produced, but make it locatable: the
                // per-node report is where the user looks for the node, not the type.
                const QString name = current.value(QStringLiteral("name")).toString().trimmed();
                if (!name.isEmpty()) {
                    for (QString& entry : nodeSkipped) {
                        entry = QStringLiteral("%1 \"%2\"").arg(entry, name);
                    }
                }
                skippedTypes->append(nodeSkipped);
            }
        } else if (skippedTypes != nullptr) {
            skippedTypes->append(nodeSkipped);
        }
        for (const VmessItem& item : parsed) {
            items.append(item);
        }
        current = QJsonObject{};
    };

    for (const QString& rawLine : lines) {
        const QString line = SubscriptionYamlParser::stripComment(rawLine);

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        const int indent = SubscriptionYamlParser::indent(line);
        if (!inProxies) {
            if (trimmed == QStringLiteral("proxies:")) {
                inProxies = true;
                proxiesIndent = indent;
            }
            continue;
        }

        if (indent <= proxiesIndent && !SubscriptionYamlParser::isListItem(line)) {
            break;
        }

        if (SubscriptionYamlParser::isListItem(line)) {
            flushCurrent();
            // Every "- " starts a node, so the fields of the previous one must not
            // be visible from here on. Clearing before reading rather than at the
            // end of the previous node is what keeps a field that could not be
            // parsed from attaching itself to the next node.
            current = QJsonObject{};
            nodeWithoutFields = false;
            brokenFlowItemLabel.clear();

            const QString itemBody = trimmed.mid(2).trimmed();
            if (itemBody.startsWith(QChar('{'))) {
                const QJsonValue flowItem = SubscriptionYamlParser::parseValue(itemBody);
                if (flowItem.isObject()) {
                    current = flowItem.toObject();
                } else {
                    brokenFlowItemLabel = QCoreApplication::translate(
                        "SubscriptionContentParser", "(malformed flow item) here");
                }
                continue;
            }

            if (itemBody.isEmpty()) {
                nodeWithoutFields = true;
                continue;
            }

            QString key;
            QString value;
            if (SubscriptionYamlParser::splitKeyValue(itemBody, &key, &value)) {
                current.insert(key, SubscriptionYamlParser::parseValue(value));
            } else {
                nodeWithoutFields = true;
            }
            continue;
        }

        QString key;
        QString value;
        if (!SubscriptionYamlParser::splitKeyValue(trimmed, &key, &value)) {
            continue;
        }
        current.insert(key, SubscriptionYamlParser::parseValue(value));
    }

    flushCurrent();
    return items;
}

} // namespace ClashYamlProxyParser
