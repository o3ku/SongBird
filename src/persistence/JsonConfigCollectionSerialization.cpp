#include "persistence/JsonConfigCollectionSerialization.h"

#include <QJsonArray>
#include <QJsonValue>

#include <utility>

#include "persistence/JsonConfigUtils.h"

namespace {

using namespace JsonConfigUtils;

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
    if (value.isUndefined() || value.isNull()) {
        return ConfigType::VMess;
    }

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
        item.type = readString(object, QStringLiteral("type"));
        item.port = readString(object, QStringLiteral("port"));
        item.network = readString(object, QStringLiteral("network"));
        item.outboundTag = readString(object, QStringLiteral("outboundTag"));
        item.enabled = readBool(object, QStringLiteral("enabled"), true);
        item.inboundTag = readStringList(object, QStringLiteral("inboundTag"));
        item.ip = readStringList(object, QStringLiteral("ip"));
        item.domain = readStringList(object, QStringLiteral("domain"));
        item.protocol = readStringList(object, QStringLiteral("protocol"));
        item.process = readStringList(object, QStringLiteral("process"));
        items.append(item);
    }

    return items;
}

QJsonArray toRoutingRuleArray(const QList<RoutingRule>& items)
{
    QJsonArray array;
    for (const RoutingRule& item : items) {
        QJsonObject object;
        writeIfNotEmpty(object, QStringLiteral("type"), item.type);
        writeIfNotEmpty(object, QStringLiteral("port"), item.port);
        writeIfNotEmpty(object, QStringLiteral("network"), item.network);
        writeIfNotEmpty(object, QStringLiteral("outboundTag"), item.outboundTag);
        writeIfNotDefault(object, QStringLiteral("enabled"), item.enabled, true);
        writeStringListIfNotEmpty(object, QStringLiteral("inboundTag"), item.inboundTag);
        writeStringListIfNotEmpty(object, QStringLiteral("ip"), item.ip);
        writeStringListIfNotEmpty(object, QStringLiteral("domain"), item.domain);
        writeStringListIfNotEmpty(object, QStringLiteral("protocol"), item.protocol);
        writeStringListIfNotEmpty(object, QStringLiteral("process"), item.process);
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
        writeIfNotEmpty(object, QStringLiteral("indexId"), item.indexId);
        if (item.configType != ConfigType::VMess) {
            object.insert(QStringLiteral("configType"), toConfigTypeValue(item.configType));
        }
        if (item.coreType != CoreType::SingBox) {
            object.insert(QStringLiteral("coreType"), toCoreTypeValue(item.coreType));
        }
        writeIfNotEmpty(object, QStringLiteral("address"), item.address);
        writeIfNotDefault(object, QStringLiteral("port"), item.port, 0);
        writeIfNotEmpty(object, QStringLiteral("id"), item.id);
        writeIfNotDefault(object, QStringLiteral("alterId"), item.alterId, 0);
        writeIfNotEmpty(object, QStringLiteral("security"), item.security);
        writeIfNotEmpty(object, QStringLiteral("network"), item.network);
        writeIfNotEmpty(object, QStringLiteral("remarks"), item.remarks);
        writeIfNotEmpty(object, QStringLiteral("headerType"), item.headerType);
        writeIfNotEmpty(object, QStringLiteral("requestHost"), item.requestHost);
        writeIfNotEmpty(object, QStringLiteral("path"), item.path);
        writeIfNotEmpty(object, QStringLiteral("streamSecurity"), item.streamSecurity);
        writeIfNotEmpty(object, QStringLiteral("allowInsecure"), item.allowInsecure);
        writeIfNotEmpty(object, QStringLiteral("flow"), item.flow);
        writeIfNotEmpty(object, QStringLiteral("sni"), item.sni);
        writeIfNotDefault(object, QStringLiteral("preSocksPort"), item.preSocksPort, 0);
        writeIfNotEmpty(object, QStringLiteral("fingerprint"), item.fingerprint);
        writeIfNotEmpty(object, QStringLiteral("publicKey"), item.publicKey);
        writeIfNotEmpty(object, QStringLiteral("shortId"), item.shortId);
        writeIfNotEmpty(object, QStringLiteral("spiderX"), item.spiderX);
        writeIfNotEmpty(object, QStringLiteral("mldsa65Verify"), item.mldsa65Verify);
        writeIfNotEmpty(object, QStringLiteral("echConfigList"), item.echConfigList);
        writeIfNotEmpty(object, QStringLiteral("echForceQuery"), item.echForceQuery);
        writeIfNotEmpty(object, QStringLiteral("cert"), item.cert);
        writeIfNotEmpty(object, QStringLiteral("certSha"), item.certSha);
        if (item.muxEnabled.has_value()) {
            object.insert(QStringLiteral("muxEnabled"), item.muxEnabled.value());
        }
        writeIfNotEmpty(object, QStringLiteral("extra"), item.extra);
        writeIfNotEmpty(object, QStringLiteral("userAgent"), item.userAgent);
        writeIfNotEmpty(object, QStringLiteral("finalmask"), item.finalmask);
        writeIfNotEmpty(object, QStringLiteral("obfsPassword"), item.obfsPassword);
        writeIfNotEmpty(object, QStringLiteral("upMbps"), item.upMbps);
        writeIfNotEmpty(object, QStringLiteral("downMbps"), item.downMbps);
        writeIfNotEmpty(object, QStringLiteral("congestionControl"), item.congestionControl);
        writeIfNotEmpty(object, QStringLiteral("udpRelayMode"), item.udpRelayMode);
        writeIfTrue(object, QStringLiteral("zeroRttHandshake"), item.zeroRttHandshake);
        writeIfNotEmpty(object, QStringLiteral("privateKey"), item.privateKey);
        writeIfNotEmpty(object, QStringLiteral("peerPublicKey"), item.peerPublicKey);
        writeIfNotEmpty(object, QStringLiteral("localAddress"), item.localAddress);
        writeIfNotDefault(object, QStringLiteral("wireguardMtu"), item.wireguardMtu, 0);
        writeIfNotEmpty(object, QStringLiteral("reserved"), item.reserved);
        writeIfNotEmpty(object, QStringLiteral("username"), item.username);
        writeIfTrue(object, QStringLiteral("naiveQuic"), item.naiveQuic);
        writeIfNotDefault(object, QStringLiteral("insecureConcurrency"), item.insecureConcurrency, 0);
        writeIfNotEmpty(object, QStringLiteral("subscriptionId"), item.subId);
        writeStringListIfNotEmpty(object, QStringLiteral("alpn"), item.alpn);

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
        writeIfNotEmpty(object, QStringLiteral("id"), item.id);
        writeIfNotEmpty(object, QStringLiteral("remarks"), item.remarks);
        writeIfNotEmpty(object, QStringLiteral("url"), item.url);
        writeIfNotDefault(object, QStringLiteral("enabled"), item.enabled, true);
        writeIfNotEmpty(object, QStringLiteral("userAgent"), item.userAgent);
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
        writeIfNotEmpty(object, QStringLiteral("remarks"), item.remarks);
        writeIfNotEmpty(object, QStringLiteral("url"), item.url);
        writeIfNotDefault(object, QStringLiteral("enabled"), item.enabled, true);
        writeIfTrue(object, QStringLiteral("locked"), item.locked);
        writeIfNotEmpty(object, QStringLiteral("customIcon"), item.customIcon);
        writeIfNotEmpty(object, QStringLiteral("domainStrategyForSingbox"), item.domainStrategy4Singbox);
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

    writeIfNotDefault(root, QStringLiteral("routingIndex"), config.routingIndex, 0);
    writeIfTrue(root, QStringLiteral("enableRoutingAdvanced"), config.enableRoutingAdvanced);

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
