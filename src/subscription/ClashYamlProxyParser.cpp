#include "subscription/ClashYamlProxyParser.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

#include "subscription/ClashProxyItemParser.h"
#include "subscription/SubscriptionYamlParser.h"

namespace ClashYamlProxyParser {

QList<VmessItem> parseProxyItems(const QString& content)
{
    QList<VmessItem> items;
    const QStringList lines = content.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::KeepEmptyParts);

    bool inProxies = false;
    int proxiesIndent = -1;
    QJsonObject current;

    const auto flushCurrent = [&items, &current]() {
        if (current.isEmpty()) {
            return;
        }

        QJsonArray proxyArray;
        proxyArray.append(current);
        const QList<VmessItem> parsed = ClashProxyItemParser::parseProxyArray(proxyArray);
        for (const VmessItem& item : parsed) {
            items.append(item);
        }
        current = QJsonObject{};
    };

    for (const QString& rawLine : lines) {
        QString line = rawLine;
        const int commentIndex = line.indexOf(QStringLiteral(" #"));
        if (commentIndex >= 0) {
            line = line.left(commentIndex);
        }

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
