#include "persistence/JsonConfigTunSerialization.h"

namespace {

TunModeItem parseTunModeItem(const QJsonObject& object)
{
    TunModeItem item;
    item.enableTun = object.value(QStringLiteral("enableTun")).toBool(false);
    item.autoRoute = object.value(QStringLiteral("autoRoute")).toBool(true);
    item.strictRoute = object.value(QStringLiteral("strictRoute")).toBool(true);
    item.stack = object.value(QStringLiteral("stack")).toString(QStringLiteral("system"));
    item.mtu = object.value(QStringLiteral("mtu")).toInt(9000);
    item.enableIPv6Address = object.value(QStringLiteral("enableIPv6Address")).toBool(false);
    item.icmpRouting = object.value(QStringLiteral("icmpRouting")).toString(QStringLiteral("rule")).trimmed();
    if (item.icmpRouting.isEmpty()) {
        item.icmpRouting = QStringLiteral("rule");
    }
    item.enableLegacyProtect = object.value(QStringLiteral("enableLegacyProtect")).toBool(false);
    return item;
}

QJsonObject toTunModeItem(const TunModeItem& item)
{
    QJsonObject object;
    if (item.enableTun) {
        object.insert(QStringLiteral("enableTun"), true);
    }
    if (!item.autoRoute) {
        object.insert(QStringLiteral("autoRoute"), false);
    }
    if (!item.strictRoute) {
        object.insert(QStringLiteral("strictRoute"), false);
    }
    if (item.stack != QStringLiteral("system")) {
        object.insert(QStringLiteral("stack"), item.stack);
    }
    if (item.mtu != 9000) {
        object.insert(QStringLiteral("mtu"), item.mtu);
    }
    if (item.enableIPv6Address) {
        object.insert(QStringLiteral("enableIPv6Address"), true);
    }
    if (item.icmpRouting.trimmed() != QStringLiteral("rule")) {
        object.insert(QStringLiteral("icmpRouting"), item.icmpRouting);
    }
    if (item.enableLegacyProtect) {
        object.insert(QStringLiteral("enableLegacyProtect"), true);
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
