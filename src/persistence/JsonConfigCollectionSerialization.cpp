#include "persistence/JsonConfigCollectionSerialization.h"

#include <QJsonArray>
#include <QJsonValue>

#include <utility>

namespace {

const QString kBuiltinRoutingWhitelistName = QStringLiteral("\u7ed5\u8fc7\u5927\u9646(Whitelist)");
const QString kBuiltinRoutingBlacklistName = QStringLiteral("\u9ed1\u540d\u5355(Blacklist)");
const QString kBuiltinRoutingGlobalName = QStringLiteral("\u5168\u5c40(Global)");

RoutingRule createRoutingRule(
    QString outboundTag,
    QStringList domain = {},
    QStringList ip = {},
    QStringList protocol = {},
    QString port = {})
{
    RoutingRule rule;
    rule.type = QStringLiteral("field");
    rule.port = std::move(port);
    rule.outboundTag = std::move(outboundTag);
    rule.ip = std::move(ip);
    rule.domain = std::move(domain);
    rule.protocol = std::move(protocol);
    rule.enabled = true;
    return rule;
}

RoutingItem createBuiltinRoutingItem(QString remarks, QList<RoutingRule> rules)
{
    RoutingItem item;
    item.remarks = std::move(remarks);
    item.rules = std::move(rules);
    item.enabled = true;
    item.locked = false;
    return item;
}

void ensureBuiltinRoutingItems(CollectionConfigState& config)
{
    for (const RoutingItem& item : config.routingItems) {
        if (!item.locked) {
            if (config.enableRoutingAdvanced
                && (config.routingIndex < 0 || config.routingIndex >= config.routingItems.size())) {
                config.routingIndex = 0;
            }
            return;
        }
    }

    config.routingItems.append(createBuiltinRoutingItem(
        kBuiltinRoutingWhitelistName,
        QList<RoutingRule>{
            createRoutingRule(
                QStringLiteral("direct"),
                QStringList{
                    QStringLiteral("domain:example-example.com"),
                    QStringLiteral("domain:example-example2.com")}),
            createRoutingRule(
                QStringLiteral("block"),
                QStringList{QStringLiteral("geosite:category-ads-all")}),
            createRoutingRule(
                QStringLiteral("direct"),
                QStringList{QStringLiteral("geosite:cn")}),
            createRoutingRule(
                QStringLiteral("direct"),
                {},
                QStringList{
                    QStringLiteral("geoip:private"),
                    QStringLiteral("geoip:cn")}),
            createRoutingRule(
                QStringLiteral("proxy"),
                {},
                {},
                {},
                QStringLiteral("0-65535"))}));

    config.routingItems.append(createBuiltinRoutingItem(
        kBuiltinRoutingBlacklistName,
        QList<RoutingRule>{
            createRoutingRule(
                QStringLiteral("direct"),
                {},
                {},
                QStringList{QStringLiteral("bittorrent")}),
            createRoutingRule(
                QStringLiteral("block"),
                QStringList{QStringLiteral("geosite:category-ads-all")}),
            createRoutingRule(
                QStringLiteral("proxy"),
                QStringList{
                    QStringLiteral("geosite:gfw"),
                    QStringLiteral("geosite:greatfire"),
                    QStringLiteral("geosite:tld-!cn")},
                QStringList{QStringLiteral("geoip:telegram")}),
            createRoutingRule(
                QStringLiteral("direct"),
                {},
                {},
                {},
                QStringLiteral("0-65535"))}));

    config.routingItems.append(createBuiltinRoutingItem(
        kBuiltinRoutingGlobalName,
        QList<RoutingRule>{
            createRoutingRule(
                QStringLiteral("proxy"),
                {},
                {},
                {},
                QStringLiteral("0-65535"))}));

    if (config.enableRoutingAdvanced
        && (config.routingIndex < 0 || config.routingIndex >= config.routingItems.size())) {
        config.routingIndex = 0;
    }
}

ConfigType parseConfigType(const QJsonValue& value)
{
    if (!value.isDouble()) {
        return ConfigType::Unknown;
    }

    switch (value.toInt()) {
    case 1:
        return ConfigType::VMess;
    case 2:
        return ConfigType::Custom;
    case 3:
        return ConfigType::Shadowsocks;
    case 4:
        return ConfigType::Socks;
    case 5:
        return ConfigType::VLESS;
    case 6:
        return ConfigType::Trojan;
    case 7:
        return ConfigType::Hysteria2;
    case 8:
        return ConfigType::TUIC;
    case 9:
        return ConfigType::WireGuard;
    case 11:
        return ConfigType::HTTP;
    case 13:
        return ConfigType::AnyTLS;
    case 14:
        return ConfigType::Naive;
    default:
        return ConfigType::Unknown;
    }
}

CoreType parseCoreType(const QJsonValue& value)
{
    if (value.isUndefined() || value.isNull()) {
        return CoreType::SingBox;
    }

    if (!value.isDouble()) {
        return CoreType::Unknown;
    }

    switch (value.toInt()) {
    case 1:
    case 2:
        return CoreType::Xray;
    case 3:
    case 4:
    case 11:
    case 12:
    case 21:
    case 22:
    case 24:
        return CoreType::SingBox;
    default:
        return CoreType::Unknown;
    }
}

int toConfigTypeValue(ConfigType type)
{
    return static_cast<int>(type);
}

int toCoreTypeValue(CoreType type)
{
    return static_cast<int>(type);
}

QList<RoutingRule> parseRoutingRules(const QJsonArray& array)
{
    QList<RoutingRule> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        RoutingRule item;
        item.type = object.value(QStringLiteral("type")).toString();
        item.port = object.value(QStringLiteral("port")).toString();
        item.network = object.value(QStringLiteral("network")).toString();
        item.outboundTag = object.value(QStringLiteral("outboundTag")).toString();
        item.enabled = object.value(QStringLiteral("enabled")).toBool(true);

        const QJsonArray inboundTagArray = object.value(QStringLiteral("inboundTag")).toArray();
        for (const QJsonValue& inboundTagValue : inboundTagArray) {
            item.inboundTag.append(inboundTagValue.toString());
        }

        const QJsonArray ipArray = object.value(QStringLiteral("ip")).toArray();
        for (const QJsonValue& ipValue : ipArray) {
            item.ip.append(ipValue.toString());
        }

        const QJsonArray domainArray = object.value(QStringLiteral("domain")).toArray();
        for (const QJsonValue& domainValue : domainArray) {
            item.domain.append(domainValue.toString());
        }

        const QJsonArray protocolArray = object.value(QStringLiteral("protocol")).toArray();
        for (const QJsonValue& protocolValue : protocolArray) {
            item.protocol.append(protocolValue.toString());
        }

        const QJsonArray processArray = object.value(QStringLiteral("process")).toArray();
        for (const QJsonValue& processValue : processArray) {
            item.process.append(processValue.toString());
        }

        items.append(item);
    }

    return items;
}

QJsonArray toRoutingRuleArray(const QList<RoutingRule>& items)
{
    QJsonArray array;
    for (const RoutingRule& item : items) {
        QJsonObject object;
        if (!item.type.trimmed().isEmpty()) {
            object.insert(QStringLiteral("type"), item.type);
        }
        if (!item.port.trimmed().isEmpty()) {
            object.insert(QStringLiteral("port"), item.port);
        }
        if (!item.network.trimmed().isEmpty()) {
            object.insert(QStringLiteral("network"), item.network);
        }
        if (!item.outboundTag.trimmed().isEmpty()) {
            object.insert(QStringLiteral("outboundTag"), item.outboundTag);
        }
        if (!item.enabled) {
            object.insert(QStringLiteral("enabled"), false);
        }

        QJsonArray inboundTagArray;
        for (const QString& value : item.inboundTag) {
            inboundTagArray.append(value);
        }
        if (!inboundTagArray.isEmpty()) {
            object.insert(QStringLiteral("inboundTag"), inboundTagArray);
        }

        QJsonArray ipArray;
        for (const QString& value : item.ip) {
            ipArray.append(value);
        }
        if (!ipArray.isEmpty()) {
            object.insert(QStringLiteral("ip"), ipArray);
        }

        QJsonArray domainArray;
        for (const QString& value : item.domain) {
            domainArray.append(value);
        }
        if (!domainArray.isEmpty()) {
            object.insert(QStringLiteral("domain"), domainArray);
        }

        QJsonArray protocolArray;
        for (const QString& value : item.protocol) {
            protocolArray.append(value);
        }
        if (!protocolArray.isEmpty()) {
            object.insert(QStringLiteral("protocol"), protocolArray);
        }

        QJsonArray processArray;
        for (const QString& value : item.process) {
            processArray.append(value);
        }
        if (!processArray.isEmpty()) {
            object.insert(QStringLiteral("process"), processArray);
        }

        array.append(object);
    }

    return array;
}

QList<VmessItem> parseServers(const QJsonArray& array)
{
    QList<VmessItem> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        VmessItem item;
        item.indexId = object.value(QStringLiteral("indexId")).toString();
        item.configType = parseConfigType(object.value(QStringLiteral("configType")));
        item.coreType = parseCoreType(object.value(QStringLiteral("coreType")));
        item.address = object.value(QStringLiteral("address")).toString();
        item.port = object.value(QStringLiteral("port")).toInt(0);
        item.id = object.value(QStringLiteral("id")).toString();
        item.alterId = object.value(QStringLiteral("alterId")).toInt(0);
        item.security = object.value(QStringLiteral("security")).toString();
        item.network = object.value(QStringLiteral("network")).toString();
        item.remarks = object.value(QStringLiteral("remarks")).toString();
        item.headerType = object.value(QStringLiteral("headerType")).toString();
        item.requestHost = object.value(QStringLiteral("requestHost")).toString();
        item.path = object.value(QStringLiteral("path")).toString();
        item.streamSecurity = object.value(QStringLiteral("streamSecurity")).toString();
        item.allowInsecure = object.value(QStringLiteral("allowInsecure")).toString();
        item.testResult = object.value(QStringLiteral("testResult")).toString();
        item.flow = object.value(QStringLiteral("flow")).toString();
        item.sni = object.value(QStringLiteral("sni")).toString();
        item.preSocksPort = object.value(QStringLiteral("preSocksPort")).toInt(0);
        item.fingerprint = object.value(QStringLiteral("fingerprint")).toString();
        item.publicKey = object.value(QStringLiteral("publicKey")).toString();
        item.shortId = object.value(QStringLiteral("shortId")).toString();
        item.spiderX = object.value(QStringLiteral("spiderX")).toString();
        item.mldsa65Verify = object.value(QStringLiteral("mldsa65Verify")).toString();
        item.echConfigList = object.value(QStringLiteral("echConfigList")).toString();
        item.echForceQuery = object.value(QStringLiteral("echForceQuery")).toString();
        item.cert = object.value(QStringLiteral("cert")).toString();
        item.certSha = object.value(QStringLiteral("certSha")).toString();
        if (object.contains(QStringLiteral("muxEnabled")) && object.value(QStringLiteral("muxEnabled")).isBool()) {
            item.muxEnabled = object.value(QStringLiteral("muxEnabled")).toBool();
        }
        item.extra = object.value(QStringLiteral("extra")).toString();
        item.userAgent = object.value(QStringLiteral("userAgent")).toString();
        item.finalmask = object.value(QStringLiteral("finalmask")).toString();
        item.obfsPassword = object.value(QStringLiteral("obfsPassword")).toString();
        item.upMbps = object.value(QStringLiteral("upMbps")).toString();
        item.downMbps = object.value(QStringLiteral("downMbps")).toString();
        item.congestionControl = object.value(QStringLiteral("congestionControl")).toString();
        item.udpRelayMode = object.value(QStringLiteral("udpRelayMode")).toString();
        item.zeroRttHandshake = object.value(QStringLiteral("zeroRttHandshake")).toBool(false);
        item.privateKey = object.value(QStringLiteral("privateKey")).toString();
        item.peerPublicKey = object.value(QStringLiteral("peerPublicKey")).toString();
        item.localAddress = object.value(QStringLiteral("localAddress")).toString();
        item.wireguardMtu = object.value(QStringLiteral("wireguardMtu")).toInt(0);
        item.reserved = object.value(QStringLiteral("reserved")).toString();
        item.username = object.value(QStringLiteral("username")).toString();
        item.naiveQuic = object.value(QStringLiteral("naiveQuic")).toBool(false);
        item.insecureConcurrency = object.value(QStringLiteral("insecureConcurrency")).toInt(0);
        item.subId = object.value(QStringLiteral("subscriptionId")).toString();

        const QJsonArray alpnArray = object.value(QStringLiteral("alpn")).toArray();
        for (const QJsonValue& alpnValue : alpnArray) {
            item.alpn.append(alpnValue.toString());
        }

        items.append(item);
    }

    return items;
}

QJsonArray toServerArray(const QList<VmessItem>& items)
{
    QJsonArray array;
    for (const VmessItem& item : items) {
        QJsonObject object;
        if (!item.indexId.trimmed().isEmpty()) {
            object.insert(QStringLiteral("indexId"), item.indexId);
        }
        if (item.configType != ConfigType::VMess) {
            object.insert(QStringLiteral("configType"), toConfigTypeValue(item.configType));
        }
        if (item.coreType != CoreType::SingBox) {
            object.insert(QStringLiteral("coreType"), toCoreTypeValue(item.coreType));
        }
        if (!item.address.trimmed().isEmpty()) {
            object.insert(QStringLiteral("address"), item.address);
        }
        if (item.port != 0) {
            object.insert(QStringLiteral("port"), item.port);
        }
        if (!item.id.trimmed().isEmpty()) {
            object.insert(QStringLiteral("id"), item.id);
        }
        if (item.alterId != 0) {
            object.insert(QStringLiteral("alterId"), item.alterId);
        }
        if (!item.security.trimmed().isEmpty()) {
            object.insert(QStringLiteral("security"), item.security);
        }
        if (!item.network.trimmed().isEmpty()) {
            object.insert(QStringLiteral("network"), item.network);
        }
        if (!item.remarks.trimmed().isEmpty()) {
            object.insert(QStringLiteral("remarks"), item.remarks);
        }
        if (!item.headerType.trimmed().isEmpty()) {
            object.insert(QStringLiteral("headerType"), item.headerType);
        }
        if (!item.requestHost.trimmed().isEmpty()) {
            object.insert(QStringLiteral("requestHost"), item.requestHost);
        }
        if (!item.path.trimmed().isEmpty()) {
            object.insert(QStringLiteral("path"), item.path);
        }
        if (!item.streamSecurity.trimmed().isEmpty()) {
            object.insert(QStringLiteral("streamSecurity"), item.streamSecurity);
        }
        if (!item.allowInsecure.trimmed().isEmpty()) {
            object.insert(QStringLiteral("allowInsecure"), item.allowInsecure);
        }
        if (!item.flow.trimmed().isEmpty()) {
            object.insert(QStringLiteral("flow"), item.flow);
        }
        if (!item.sni.trimmed().isEmpty()) {
            object.insert(QStringLiteral("sni"), item.sni);
        }
        if (item.preSocksPort != 0) {
            object.insert(QStringLiteral("preSocksPort"), item.preSocksPort);
        }
        if (!item.fingerprint.trimmed().isEmpty()) {
            object.insert(QStringLiteral("fingerprint"), item.fingerprint);
        }
        if (!item.publicKey.trimmed().isEmpty()) {
            object.insert(QStringLiteral("publicKey"), item.publicKey);
        }
        if (!item.shortId.trimmed().isEmpty()) {
            object.insert(QStringLiteral("shortId"), item.shortId);
        }
        if (!item.spiderX.trimmed().isEmpty()) {
            object.insert(QStringLiteral("spiderX"), item.spiderX);
        }
        if (!item.mldsa65Verify.trimmed().isEmpty()) {
            object.insert(QStringLiteral("mldsa65Verify"), item.mldsa65Verify);
        }
        if (!item.echConfigList.trimmed().isEmpty()) {
            object.insert(QStringLiteral("echConfigList"), item.echConfigList);
        }
        if (!item.echForceQuery.trimmed().isEmpty()) {
            object.insert(QStringLiteral("echForceQuery"), item.echForceQuery);
        }
        if (!item.cert.trimmed().isEmpty()) {
            object.insert(QStringLiteral("cert"), item.cert);
        }
        if (!item.certSha.trimmed().isEmpty()) {
            object.insert(QStringLiteral("certSha"), item.certSha);
        }
        if (item.muxEnabled.has_value()) {
            object.insert(QStringLiteral("muxEnabled"), item.muxEnabled.value());
        }
        if (!item.extra.trimmed().isEmpty()) {
            object.insert(QStringLiteral("extra"), item.extra);
        }
        if (!item.userAgent.trimmed().isEmpty()) {
            object.insert(QStringLiteral("userAgent"), item.userAgent);
        }
        if (!item.finalmask.trimmed().isEmpty()) {
            object.insert(QStringLiteral("finalmask"), item.finalmask);
        }
        if (!item.obfsPassword.trimmed().isEmpty()) {
            object.insert(QStringLiteral("obfsPassword"), item.obfsPassword);
        }
        if (!item.upMbps.trimmed().isEmpty()) {
            object.insert(QStringLiteral("upMbps"), item.upMbps);
        }
        if (!item.downMbps.trimmed().isEmpty()) {
            object.insert(QStringLiteral("downMbps"), item.downMbps);
        }
        if (!item.congestionControl.trimmed().isEmpty()) {
            object.insert(QStringLiteral("congestionControl"), item.congestionControl);
        }
        if (!item.udpRelayMode.trimmed().isEmpty()) {
            object.insert(QStringLiteral("udpRelayMode"), item.udpRelayMode);
        }
        if (item.zeroRttHandshake) {
            object.insert(QStringLiteral("zeroRttHandshake"), true);
        }
        if (!item.privateKey.trimmed().isEmpty()) {
            object.insert(QStringLiteral("privateKey"), item.privateKey);
        }
        if (!item.peerPublicKey.trimmed().isEmpty()) {
            object.insert(QStringLiteral("peerPublicKey"), item.peerPublicKey);
        }
        if (!item.localAddress.trimmed().isEmpty()) {
            object.insert(QStringLiteral("localAddress"), item.localAddress);
        }
        if (item.wireguardMtu != 0) {
            object.insert(QStringLiteral("wireguardMtu"), item.wireguardMtu);
        }
        if (!item.reserved.trimmed().isEmpty()) {
            object.insert(QStringLiteral("reserved"), item.reserved);
        }
        if (!item.username.trimmed().isEmpty()) {
            object.insert(QStringLiteral("username"), item.username);
        }
        if (item.naiveQuic) {
            object.insert(QStringLiteral("naiveQuic"), true);
        }
        if (item.insecureConcurrency != 0) {
            object.insert(QStringLiteral("insecureConcurrency"), item.insecureConcurrency);
        }
        if (!item.subId.trimmed().isEmpty()) {
            object.insert(QStringLiteral("subscriptionId"), item.subId);
        }

        QJsonArray alpnArray;
        for (const QString& value : item.alpn) {
            alpnArray.append(value);
        }
        if (!alpnArray.isEmpty()) {
            object.insert(QStringLiteral("alpn"), alpnArray);
        }

        array.append(object);
    }

    return array;
}

QList<SubItem> parseSubscriptions(const QJsonArray& array)
{
    QList<SubItem> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        SubItem item;
        item.id = object.value(QStringLiteral("id")).toString();
        item.remarks = object.value(QStringLiteral("remarks")).toString();
        item.url = object.value(QStringLiteral("url")).toString();
        item.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        item.userAgent = object.value(QStringLiteral("userAgent")).toString();
        items.append(item);
    }

    return items;
}

QJsonArray toSubscriptionArray(const QList<SubItem>& items)
{
    QJsonArray array;
    for (const SubItem& item : items) {
        QJsonObject object;
        if (!item.id.trimmed().isEmpty()) {
            object.insert(QStringLiteral("id"), item.id);
        }
        if (!item.remarks.trimmed().isEmpty()) {
            object.insert(QStringLiteral("remarks"), item.remarks);
        }
        if (!item.url.trimmed().isEmpty()) {
            object.insert(QStringLiteral("url"), item.url);
        }
        if (!item.enabled) {
            object.insert(QStringLiteral("enabled"), false);
        }
        if (!item.userAgent.trimmed().isEmpty()) {
            object.insert(QStringLiteral("userAgent"), item.userAgent);
        }
        array.append(object);
    }

    return array;
}

QList<GlobalHotkeyItem> parseGlobalHotkeys(const QJsonArray& array)
{
    QList<GlobalHotkeyItem> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        GlobalHotkeyItem item;
        item.action = static_cast<GlobalHotkeyAction>(
            object.value(QStringLiteral("EGlobalHotkey")).toInt(static_cast<int>(GlobalHotkeyAction::ShowForm)));
        item.alt = object.value(QStringLiteral("Alt")).toBool(false);
        item.control = object.value(QStringLiteral("Control")).toBool(false);
        item.shift = object.value(QStringLiteral("Shift")).toBool(false);
        if (object.contains(QStringLiteral("KeyCode")) && object.value(QStringLiteral("KeyCode")).isDouble()) {
            item.keyCode = object.value(QStringLiteral("KeyCode")).toInt();
        } else {
            item.keyCode.reset();
        }
        items.append(item);
    }

    return items;
}

QJsonArray toGlobalHotkeyArray(const QList<GlobalHotkeyItem>& items)
{
    QJsonArray array;
    for (const GlobalHotkeyItem& item : items) {
        QJsonObject object;
        if (item.action != GlobalHotkeyAction::ShowForm) {
            object.insert(QStringLiteral("EGlobalHotkey"), static_cast<int>(item.action));
        }
        if (item.alt) {
            object.insert(QStringLiteral("Alt"), true);
        }
        if (item.control) {
            object.insert(QStringLiteral("Control"), true);
        }
        if (item.shift) {
            object.insert(QStringLiteral("Shift"), true);
        }
        if (item.keyCode.has_value()) {
            object.insert(QStringLiteral("KeyCode"), item.keyCode.value());
        }
        array.append(object);
    }

    return array;
}

QList<RoutingItem> parseRoutingItems(const QJsonArray& array)
{
    QList<RoutingItem> items;
    items.reserve(array.size());

    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        RoutingItem item;
        item.remarks = object.value(QStringLiteral("remarks")).toString();
        item.url = object.value(QStringLiteral("url")).toString();
        item.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        item.locked = object.value(QStringLiteral("locked")).toBool(false);
        item.customIcon = object.value(QStringLiteral("customIcon")).toString();
        item.domainStrategy4Singbox = object.value(QStringLiteral("domainStrategyForSingbox")).toString();
        item.rules = parseRoutingRules(object.value(QStringLiteral("rules")).toArray());
        items.append(item);
    }

    return items;
}

QJsonArray toRoutingArray(const QList<RoutingItem>& items)
{
    QJsonArray array;
    for (const RoutingItem& item : items) {
        QJsonObject object;
        if (!item.remarks.trimmed().isEmpty()) {
            object.insert(QStringLiteral("remarks"), item.remarks);
        }
        if (!item.url.trimmed().isEmpty()) {
            object.insert(QStringLiteral("url"), item.url);
        }
        if (!item.enabled) {
            object.insert(QStringLiteral("enabled"), false);
        }
        if (item.locked) {
            object.insert(QStringLiteral("locked"), true);
        }
        if (!item.customIcon.trimmed().isEmpty()) {
            object.insert(QStringLiteral("customIcon"), item.customIcon);
        }
        if (!item.domainStrategy4Singbox.trimmed().isEmpty()) {
            object.insert(QStringLiteral("domainStrategyForSingbox"), item.domainStrategy4Singbox);
        }
        const QJsonArray rules = toRoutingRuleArray(item.rules);
        if (!rules.isEmpty()) {
            object.insert(QStringLiteral("rules"), rules);
        }
        array.append(object);
    }

    return array;
}

} // namespace

namespace JsonConfigCollectionSerialization {

void read(const QJsonObject& root, CollectionConfigState& config)
{
    config.servers = parseServers(root.value(QStringLiteral("servers")).toArray());
    config.subscriptions = parseSubscriptions(root.value(QStringLiteral("subscriptions")).toArray());
    config.globalHotkeys = parseGlobalHotkeys(root.value(QStringLiteral("globalHotkeys")).toArray());
    config.routingIndex = root.value(QStringLiteral("routingIndex")).toInt(0);
    config.enableRoutingAdvanced = root.value(QStringLiteral("enableRoutingAdvanced")).toBool(false);
    config.routingItems = parseRoutingItems(root.value(QStringLiteral("routingItems")).toArray());
    config.routingCustomRules = parseRoutingRules(root.value(QStringLiteral("routingCustomRules")).toArray());
    ensureBuiltinRoutingItems(config);
}

void write(QJsonObject& root, const CollectionConfigState& config)
{
    const QJsonArray servers = toServerArray(config.servers);
    if (!servers.isEmpty()) {
        root.insert(QStringLiteral("servers"), servers);
    }

    const QJsonArray subscriptions = toSubscriptionArray(config.subscriptions);
    if (!subscriptions.isEmpty()) {
        root.insert(QStringLiteral("subscriptions"), subscriptions);
    }

    const QJsonArray globalHotkeys = toGlobalHotkeyArray(config.globalHotkeys);
    if (!globalHotkeys.isEmpty()) {
        root.insert(QStringLiteral("globalHotkeys"), globalHotkeys);
    }

    if (config.routingIndex != 0) {
        root.insert(QStringLiteral("routingIndex"), config.routingIndex);
    }
    if (config.enableRoutingAdvanced) {
        root.insert(QStringLiteral("enableRoutingAdvanced"), true);
    }

    const QJsonArray routingItems = toRoutingArray(config.routingItems);
    if (!routingItems.isEmpty()) {
        root.insert(QStringLiteral("routingItems"), routingItems);
    }

    const QJsonArray routingCustomRules = toRoutingRuleArray(config.routingCustomRules);
    if (!routingCustomRules.isEmpty()) {
        root.insert(QStringLiteral("routingCustomRules"), routingCustomRules);
    }
}

} // namespace JsonConfigCollectionSerialization
