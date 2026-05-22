#include "persistence/JsonConfigPolicySerialization.h"

#include <QJsonArray>
#include <QJsonValue>

#include "persistence/JsonConfigUtils.h"
#include "runtime/ProtocolCoreCompat.h"

namespace {

using namespace JsonConfigUtils;

PolicyGroupItem::Strategy parsePolicyGroupStrategy(const QJsonObject& object)
{
    const int value = readInt(object, QStringLiteral("strategy"), 0);
    switch (value) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        return static_cast<PolicyGroupItem::Strategy>(value);
    default:
        return PolicyGroupItem::Strategy::LeastPing;
    }
}

QList<PolicyGroupItem> parsePolicyGroups(const QJsonArray& array)
{
    QList<PolicyGroupItem> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        PolicyGroupItem item;
        item.id = readString(object, QStringLiteral("id"));
        item.name = readString(object, QStringLiteral("name"));
        item.strategy = parsePolicyGroupStrategy(object);
        item.urlTestUrl = readString(object, QStringLiteral("urlTestUrl"));
        item.toleranceMs = readInt(object, QStringLiteral("toleranceMs"), 0);
        item.memberServerIds = readStringList(object, QStringLiteral("memberServerIds"));
        items.append(item);
    }

    return items;
}

QJsonArray toPolicyGroupArray(const QList<PolicyGroupItem>& items)
{
    QJsonArray array;
    for (const PolicyGroupItem& item : items) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), item.id);
        object.insert(QStringLiteral("name"), item.name);
        object.insert(QStringLiteral("strategy"), static_cast<int>(item.strategy));
        object.insert(QStringLiteral("urlTestUrl"), item.urlTestUrl);
        object.insert(QStringLiteral("toleranceMs"), item.toleranceMs);
        object.insert(QStringLiteral("memberServerIds"), toStringArray(item.memberServerIds));
        array.append(object);
    }

    return array;
}

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
    config.policyGroups = parsePolicyGroups(root.value(QStringLiteral("policyGroups")).toArray());
    config.coreTypeItems = parseCoreTypeItems(root.value(QStringLiteral("coreTypeItems")).toArray());
    if (config.coreTypeItems.isEmpty()) {
        config.coreTypeItems = defaultCoreTypeItems();
    }
}

void write(QJsonObject& root, const PolicyConfigState& config)
{
    const QJsonArray policyGroups = toPolicyGroupArray(config.policyGroups);
    writeArrayIfNotEmpty(root, QStringLiteral("policyGroups"), policyGroups);

    if (!isDefaultCoreTypeItems(config.coreTypeItems)) {
        root.insert(QStringLiteral("coreTypeItems"), toCoreTypeItemArray(config.coreTypeItems));
    }
}

} // namespace JsonConfigPolicySerialization
