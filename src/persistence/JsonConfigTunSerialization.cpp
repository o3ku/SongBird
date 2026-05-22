#include "persistence/JsonConfigTunSerialization.h"

#include "persistence/JsonConfigUtils.h"

namespace {

using namespace JsonConfigUtils;

TunModeItem parseTunModeItem(const QJsonObject& object)
{
    TunModeItem item;
    item.enableTun = readBool(object, QStringLiteral("enableTun"), false);
    item.autoRoute = readBool(object, QStringLiteral("autoRoute"), true);
    item.strictRoute = readBool(object, QStringLiteral("strictRoute"), true);
    item.stack = readString(object, QStringLiteral("stack"), QStringLiteral("system"));
    item.mtu = readInt(object, QStringLiteral("mtu"), 9000);
    item.enableIPv6Address = readBool(object, QStringLiteral("enableIPv6Address"), false);
    item.icmpRouting = readString(object, QStringLiteral("icmpRouting"), QStringLiteral("rule")).trimmed();
    if (item.icmpRouting.isEmpty()) {
        item.icmpRouting = QStringLiteral("rule");
    }
    item.enableLegacyProtect = readBool(object, QStringLiteral("enableLegacyProtect"), false);
    return item;
}

QJsonObject toTunModeItem(const TunModeItem& item)
{
    QJsonObject object;
    writeIfTrue(object, QStringLiteral("enableTun"), item.enableTun);
    writeIfNotDefault(object, QStringLiteral("autoRoute"), item.autoRoute, true);
    writeIfNotDefault(object, QStringLiteral("strictRoute"), item.strictRoute, true);
    writeIfNotDefault(object, QStringLiteral("mtu"), item.mtu, 9000);
    writeIfTrue(object, QStringLiteral("enableIPv6Address"), item.enableIPv6Address);
    writeIfTrue(object, QStringLiteral("enableLegacyProtect"), item.enableLegacyProtect);
    if (item.stack != QStringLiteral("system")) {
        object.insert(QStringLiteral("stack"), item.stack);
    }
    if (item.icmpRouting.trimmed() != QStringLiteral("rule")) {
        object.insert(QStringLiteral("icmpRouting"), item.icmpRouting);
    }
    return object;
}

} // namespace

namespace JsonConfigTunSerialization {

void read(const QJsonObject& root, TunConfigState& config)
{
    config.tunModeItem = parseTunModeItem(root.value(QStringLiteral("tunModeItem")).toObject());
}

void write(QJsonObject& root, const TunConfigState& config)
{
    const QJsonObject tunModeItem = toTunModeItem(config.tunModeItem);
    if (!tunModeItem.isEmpty()) {
        root.insert(QStringLiteral("tunModeItem"), tunModeItem);
    }
}

} // namespace JsonConfigTunSerialization
