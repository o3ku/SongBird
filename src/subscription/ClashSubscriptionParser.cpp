#include "subscription/ClashSubscriptionParser.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonValue>

#include "subscription/ClashProxyItemParser.h"
#include "subscription/ClashYamlProxyParser.h"

namespace ClashSubscriptionParser {

QList<VmessItem> parseContent(const QString& content, QStringList* skippedTypes)
{
    // JSON first, and the order is the whole point. A subscription served as a
    // JSON document is valid YAML too, so the line walker used to read it -- and
    // read it partly: `proxies:` splits into an empty value, the flow-style items
    // that follow are not list items, and the first `"server": "..."` inside them
    // becomes a top-level key of a node that has no type. The walker then handed
    // back an empty list, the JSON fallback ran, and whenever the document also
    // carried `proxy-groups:` or `rules:` the fallback failed as a whole --
    // "returned no supported servers" for a subscription that parsed perfectly.
    //
    // A document that is JSON is JSON; the YAML walker is for everything else. The
    // JSON reading only wins when it produced nodes: hand-written YAML is not JSON,
    // but a one-node document like `name: "Gemini #1"` can parse as a JSON object
    // whose `"proxies"` key is a quoted scalar rather than an array, and returning
    // that empty result would end the walk on a document the line parser reads fine.
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(content.trimmed().toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        const QJsonValue proxies = document.object().value(QStringLiteral("proxies"));
        if (proxies.isArray()) {
            QStringList jsonSkipped;
            const QList<VmessItem> items = parseProxyArray(proxies.toArray(), &jsonSkipped);
            if (!items.isEmpty() || !jsonSkipped.isEmpty()) {
                if (skippedTypes != nullptr) {
                    *skippedTypes = jsonSkipped;
                }
                return items;
            }
        }
    }

    return ClashYamlProxyParser::parseProxyItems(content, skippedTypes);
}

QList<VmessItem> parseProxyArray(const QJsonArray& proxies, QStringList* skippedTypes)
{
    return ClashProxyItemParser::parseProxyArray(proxies, skippedTypes);
}

} // namespace ClashSubscriptionParser
