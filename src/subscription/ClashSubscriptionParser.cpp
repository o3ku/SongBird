#include "subscription/ClashSubscriptionParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "subscription/ClashProxyItemParser.h"
#include "subscription/ClashYamlProxyParser.h"

namespace ClashSubscriptionParser {

QList<VmessItem> parseContent(const QString& content)
{
    QList<VmessItem> items = ClashYamlProxyParser::parseProxyItems(content);
    if (!items.isEmpty()) {
        return items;
    }

    const QJsonDocument document = QJsonDocument::fromJson(content.trimmed().toUtf8());
    if (document.isObject()) {
        const QJsonValue proxies = document.object().value(QStringLiteral("proxies"));
        if (proxies.isArray()) {
            return parseProxyArray(proxies.toArray());
        }
    }

    return {};
}

QList<VmessItem> parseProxyArray(const QJsonArray& proxies)
{
    return ClashProxyItemParser::parseProxyArray(proxies);
}

} // namespace ClashSubscriptionParser
