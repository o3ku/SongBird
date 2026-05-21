#include "persistence/JsonConfigPolicySerialization.h"

#include <QJsonArray>
#include <QJsonValue>

#include "runtime/ProtocolCoreCompat.h"

namespace {

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
        item.id = object.value(QStringLiteral("id")).toString();
        item.name = object.value(QStringLiteral("name")).toString();
        item.strategy = static_cast<PolicyGroupItem::Strategy>(
            object.value(QStringLiteral("strategy")).toInt(0));
        item.urlTestUrl = object.value(QStringLiteral("urlTestUrl")).toString();
        item.toleranceMs = object.value(QStringLiteral("toleranceMs")).toInt(0);

        const QJsonArray membersArray = object.value(QStringLiteral("memberServerIds")).toArray();
        for (const QJsonValue& memberValue : membersArray) {
            item.memberServerIds.append(memberValue.toString());
        }

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

        QJsonArray membersArray;
        for (const QString& memberId : item.memberServerIds) {
            membersArray.append(memberId);
        }
        object.insert(QStringLiteral("memberServerIds"), membersArray);

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
        item.configType = object.value(QStringLiteral("configType")).toInt(0);
        item.coreType = object.value(QStringLiteral("coreType")).toInt(0);
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
    if (!policyGroups.isEmpty()) {
        root.insert(QStringLiteral("policyGroups"), policyGroups);
    }

    if (!isDefaultCoreTypeItems(config.coreTypeItems)) {
        root.insert(QStringLiteral("coreTypeItems"), toCoreTypeItemArray(config.coreTypeItems));
    }
}

} // namespace JsonConfigPolicySerialization
