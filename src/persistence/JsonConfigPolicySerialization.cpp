#include "persistence/JsonConfigPolicySerialization.h"

#include <QJsonArray>
#include <QJsonValue>

#include "persistence/JsonConfigUtils.h"
#include "runtime/ProtocolCoreCompat.h"

namespace {

using namespace JsonConfigUtils;

QList<CoreTypeItem> parseCoreTypeItems(const QJsonArray& array)
{
    QList<CoreTypeItem> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        CoreTypeItem item;
        item.configType = readInt(object, QStringLiteral("configType"), 0);
        item.coreType = readInt(object, QStringLiteral("coreType"), 0);
        items.append(item);
    }

    return items;
}

QJsonArray toCoreTypeItemArray(const QList<CoreTypeItem>& items)
{
    QJsonArray array;
    for (const CoreTypeItem& item : items) {
        QJsonObject object;
        object.insert(QStringLiteral("configType"), item.configType);
        object.insert(QStringLiteral("coreType"), item.coreType);
        array.append(object);
    }

    return array;
}

bool isDefaultCoreTypeItems(const QList<CoreTypeItem>& items)
{
    return items == defaultCoreTypeItems();
}

} // namespace

namespace JsonConfigPolicySerialization {

void read(const QJsonObject& root, PolicyConfigState& config)
{
    config.coreTypeItems = parseCoreTypeItems(readArray(root, QStringLiteral("coreTypeItems")));
    if (config.coreTypeItems.isEmpty()) {
        config.coreTypeItems = defaultCoreTypeItems();
    }
}

void write(QJsonObject& root, const PolicyConfigState& config)
{
    if (!isDefaultCoreTypeItems(config.coreTypeItems)) {
        root.insert(QStringLiteral("coreTypeItems"), toCoreTypeItemArray(config.coreTypeItems));
    }
}

} // namespace JsonConfigPolicySerialization
