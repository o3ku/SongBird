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

// A node that yields no item is still a node we saw and could not use, so its
// type belongs in the same skipped report as an unsupported one. Nothing else
// records it: by the time the caller looks, the node is indistinguishable from
// one that was never in the document. A type alone cannot be acted on --
// "skipped 1 node(s): (no type)" names no node the user could go and fix -- so
// the name travels with it whenever the document gave one.

QString fieldText(const QList<QPair<QString, QJsonValue>>& fields, const QString& key)
{
    for (const auto& field : fields) {
        if (field.first == key) {
            return field.second.toString().trimmed();
        }
    }
    return {};
}

QString skippedEntryLabel(const QList<QPair<QString, QJsonValue>>& node)
{
    const QString type = fieldText(node, QStringLiteral("type"));
    const QString name = fieldText(node, QStringLiteral("name"));
    const QString typeLabel = type.isEmpty()
        ? QCoreApplication::translate("SubscriptionContentParser", "(no type)")
        : type;
    if (name.isEmpty()) {
        return typeLabel;
    }
    return QStringLiteral("%1 \"%2\"").arg(typeLabel, name);
}

// Read one flow-style list item into the node being built. Returns false when the
// item is not a well-formed object, which is the caller's signal that the entry is
// a fragment and the fields after it belong to nothing.
bool appendFlowFields(const QString& itemBody, QList<QPair<QString, QJsonValue>>* fields)
{
    const QJsonValue flowItem = SubscriptionYamlParser::parseValue(itemBody);
    if (!flowItem.isObject()) {
        return false;
    }

    const QJsonObject flowNode = flowItem.toObject();
    for (auto it = flowNode.begin(); it != flowNode.end(); ++it) {
        fields->append({it.key(), it.value()});
    }
    return true;
}

} // namespace

namespace ClashYamlProxyParser {

QList<VmessItem> parseProxyItems(const QString& content, QStringList* skippedTypes)
{
    QList<VmessItem> items;
    const QStringList lines = content.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::KeepEmptyParts);

    bool inProxies = false;
    int proxiesIndent = -1;
    // The node being read, as the key/value pairs the document actually wrote.
    //
    // Not a QJsonObject: QJsonObject::insert() drops a key equal to nullptr, so a
    // node that repeats a field or writes an empty one loses the key outright --
    // "type:" with no value is parsed to QJsonValue(QJsonValue::Null), which arrives
    // at insert() as a null string. A node whose type vanished that way used to keep
    // the *previous* node's type when the node was handed over as JSON, so it parsed
    // as a supported protocol carrying another node's address. Keeping the pairs in
    // order and converting once, per node, is what stops the leak.
    QList<QPair<QString, QJsonValue>> currentFields;
    // A node the document opened with "- " but whose fields never arrived. It has
    // no type to report, so it needs its own entry rather than falling through
    // the "unsupported node" path.
    bool nodeWithoutFields = false;
    // A flow-style item that is not a well-formed object. It is reported where it
    // is read, because the block-style fields that follow do not belong to it and
    // must not be added to a fragment of it.
    QString brokenFlowItemLabel;
    // The list item that opened this node was a malformed flow entry. Everything
    // after it is the wreckage of that entry, so it is read and discarded instead
    // of being added to a node stitched from two different list items.
    bool inBrokenFlowItem = false;

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

    // The node as a JSON object, built from this node's fields alone and discarded
    // with them.
    //
    // Object reuse was what leaked: `type:` with no value becomes a null QJsonValue,
    // QJsonObject::insert() drops a null key outright, and the key that survived in
    // the reused object was the *previous* node's -- so a node the document wrote
    // without a type arrived as a supported protocol carrying another node's address.
    // A fresh object per node means a dropped key is missing, not stale. Null itself
    // is handed on as an empty string for the same reason: "wrote nothing" must not
    // look like "wrote nothing there" once it reaches a field lookup.
    const auto nodeObject = [&currentFields]() {
        QJsonObject node;
        for (const auto& field : currentFields) {
            node.insert(field.first, field.second.isNull() ? QJsonValue(QString()) : field.second);
        }
        return node;
    };

    const auto flushCurrent = [&items, &currentFields, &nodeWithoutFields, &brokenFlowItemLabel,
                               &reportNodeWithoutFields, &nodeObject, skippedTypes]() {
        reportNodeWithoutFields();
        if (!brokenFlowItemLabel.isEmpty()) {
            if (skippedTypes != nullptr) {
                skippedTypes->append(brokenFlowItemLabel);
            }
            brokenFlowItemLabel.clear();
        }
        if (currentFields.isEmpty()) {
            return;
        }

        QJsonArray proxyArray;
        proxyArray.append(nodeObject());
        QStringList nodeSkipped;
        const QList<VmessItem> parsed = ClashProxyItemParser::parseProxyArray(proxyArray, &nodeSkipped);
        if (parsed.isEmpty() && skippedTypes != nullptr) {
            // parseProxyArray records the type it recognised but could not use. When
            // it recorded nothing the node was dropped for a reason it does not
            // report, which is precisely the silent loss worth naming -- and the
            // name is the only way to find that node in the document again.
            if (nodeSkipped.isEmpty()) {
                skippedTypes->append(skippedEntryLabel(currentFields));
            } else {
                // Keep the label the parser produced, but make it locatable: the
                // per-node report is where the user looks for the node, not the type.
                const QString name = fieldText(currentFields, QStringLiteral("name"));
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
        currentFields.clear();
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
            // Report first, then clear. flushCurrent() is what turns the node that
            // just ended into an item or a skipped entry, and clearing the fields
            // before calling it throws that node away: every node that was missing a
            // required field disappeared together with the one after it, and only the
            // last node in the document was ever reported.
            flushCurrent();
            nodeWithoutFields = false;
            inBrokenFlowItem = false;

            const QString itemBody = trimmed.mid(2).trimmed();
            if (itemBody.startsWith(QChar('{'))) {
                // A flow item that did not parse is a fragment, not a node. The
                // block-style fields that follow belong to no node at all -- they
                // are the remains of the entry that broke -- so they are dropped
                // rather than stitched onto the fragment.
                if (!appendFlowFields(itemBody, &currentFields)) {
                    inBrokenFlowItem = true;
                    brokenFlowItemLabel = QCoreApplication::translate(
                        "SubscriptionContentParser", "(malformed flow item) here");
                }
                continue;
            }
            inBrokenFlowItem = false;

            if (itemBody.isEmpty()) {
                nodeWithoutFields = true;
                continue;
            }

            QString key;
            QString value;
            if (SubscriptionYamlParser::splitKeyValue(itemBody, &key, &value)) {
                currentFields.append({key, SubscriptionYamlParser::parseValue(value)});
            } else {
                nodeWithoutFields = true;
            }
            continue;
        }

        QString key;
        QString value;
        if (inBrokenFlowItem) {
            continue;
        }
        if (!SubscriptionYamlParser::splitKeyValue(trimmed, &key, &value)) {
            continue;
        }
        currentFields.append({key, SubscriptionYamlParser::parseValue(value)});
    }

    flushCurrent();
    return items;
}

} // namespace ClashYamlProxyParser
