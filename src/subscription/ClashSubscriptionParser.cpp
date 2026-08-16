#include "subscription/ClashSubscriptionParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "subscription/ClashProxyItemParser.h"
#include "subscription/ClashYamlProxyParser.h"

namespace ClashSubscriptionParser {

QList<VmessItem> parseContent(const QString& content, QStringList* skippedTypes)
{
    QList<VmessItem> items = ClashYamlProxyParser::parseProxyItems(content, skippedTypes);
    if (!items.isEmpty()) {
        return items;
    }

    const QJsonDocument document = QJsonDocument::fromJson(content.trimmed().toUtf8());
    if (document.isObject()) {
        const QJsonValue proxies = document.object().value(QStringLiteral("proxies"));
        if (proxies.isArray()) {
            return parseProxyArray(proxies.toArray(), skippedTypes);
        }
    }

    return {};
}

QList<VmessItem> parseProxyArray(const QJsonArray& proxies, QStringList* skippedTypes)
{
    return ClashProxyItemParser::parseProxyArray(proxies, skippedTypes);
}

} // namespace ClashSubscriptionParser
