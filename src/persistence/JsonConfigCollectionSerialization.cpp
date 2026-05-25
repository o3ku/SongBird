#include "persistence/JsonConfigCollectionSerialization.h"

#include <QJsonArray>

#include "persistence/JsonConfigRoutingSerialization.h"
#include "persistence/JsonConfigServerSerialization.h"
#include "persistence/JsonConfigSubscriptionSerialization.h"
#include "persistence/JsonConfigUtils.h"

namespace JsonConfigCollectionSerialization {

void read(const QJsonObject& root, CollectionConfigState& config)
{
    config.servers = JsonConfigServerSerialization::readServers(root.value(QStringLiteral("servers")).toArray());
    config.subscriptions = JsonConfigSubscriptionSerialization::readSubscriptions(
        root.value(QStringLiteral("subscriptions")).toArray());
    JsonConfigRoutingSerialization::readRouting(root, config);
}

void write(QJsonObject& root, const CollectionConfigState& config)
{
    const QJsonArray servers = JsonConfigServerSerialization::writeServers(config.servers);
    JsonConfigUtils::writeArrayIfNotEmpty(root, QStringLiteral("servers"), servers);

    const QJsonArray subscriptions = JsonConfigSubscriptionSerialization::writeSubscriptions(config.subscriptions);
    JsonConfigUtils::writeArrayIfNotEmpty(root, QStringLiteral("subscriptions"), subscriptions);

    JsonConfigRoutingSerialization::writeRouting(root, config);
}

} // namespace JsonConfigCollectionSerialization
