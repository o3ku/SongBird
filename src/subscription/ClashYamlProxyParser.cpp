#include "subscription/ClashYamlProxyParser.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

#include "subscription/ClashProxyItemParser.h"
#include "subscription/SubscriptionYamlParser.h"

namespace ClashYamlProxyParser {

QList<VmessItem> parseProxyItems(const QString& content, QStringList* skippedTypes)
{
    QList<VmessItem> items;
    const QStringList lines = content.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::KeepEmptyParts);

    bool inProxies = false;
    int proxiesIndent = -1;
    QJsonObject current;

    // A node that lands in the array but yields no item is still a node we saw and
    // could not use, so its type belongs in the same skipped report as an
    // unsupported one. Nothing else records it: by the time the caller looks, the
    // node is indistinguishable from one that was never in the document.
    const auto flushCurrent = [&items, &current, skippedTypes]() {
        if (current.isEmpty()) {
            return;
        }

        QJsonArray proxyArray;
        proxyArray.append(current);
        QStringList nodeSkipped;
        const QList<VmessItem> parsed = ClashProxyItemParser::parseProxyArray(proxyArray, &nodeSkipped);
        if (skippedTypes != nullptr) {
            skippedTypes->append(nodeSkipped);
        }
        // parseProxyArray only records a type it recognises but cannot use (a bad
        // port, an empty address). A node it did not say anything about was dropped
        // for a reason it does not report -- a line the comment split or the key/value
        // split could not follow -- and that is precisely the silent loss worth naming.
        if (parsed.isEmpty() && nodeSkipped.isEmpty() && skippedTypes != nullptr) {
            const QString type = current.value(QStringLiteral("type")).toString().trimmed();
            skippedTypes->append(
                type.isEmpty()
                    ? QCoreApplication::translate("SubscriptionContentParser", "(no type)")
                    : type);
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
            const QString itemBody = trimmed.mid(2).trimmed();
            if (itemBody.startsWith(QChar('{'))) {
                const QJsonValue flowItem = SubscriptionYamlParser::parseValue(itemBody);
                if (flowItem.isObject()) {
                    current = flowItem.toObject();
                }
                continue;
            }

            QString key;
            QString value;
            if (SubscriptionYamlParser::splitKeyValue(itemBody, &key, &value)) {
                current.insert(key, SubscriptionYamlParser::parseValue(value));
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
