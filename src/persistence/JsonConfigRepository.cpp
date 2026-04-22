#include "persistence/JsonConfigRepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>

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

void ensureBuiltinRoutingItems(Config& config)
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

} // namespace

JsonConfigRepository::JsonConfigRepository(QString configPath)
    : configPath_(std::move(configPath))
{
}

Config JsonConfigRepository::load()
{
    lastLoadError_.clear();
    QFile file(configPath_);
    if (!file.exists()) {
        rawRoot_ = QJsonObject();
        return parseConfig(rawRoot_);
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        rawRoot_ = QJsonObject();
        lastLoadError_ = QStringLiteral("Failed to open configuration file: %1")
                             .arg(QDir::toNativeSeparators(configPath_));
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        rawRoot_ = QJsonObject();
        lastLoadError_ = parseError.error != QJsonParseError::NoError
            ? QStringLiteral("Failed to parse configuration file: %1 (offset %2: %3).")
                  .arg(QDir::toNativeSeparators(configPath_))
                  .arg(parseError.offset)
                  .arg(parseError.errorString())
            : QStringLiteral("Configuration file root must be a JSON object: %1")
                  .arg(QDir::toNativeSeparators(configPath_));
        return {};
    }

    rawRoot_ = document.object();
    return parseConfig(rawRoot_);
}

bool JsonConfigRepository::save(const Config& config)
{
    QJsonObject root = rawRoot_;
    applyConfig(root, config);

    const QFileInfo fileInfo(configPath_);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return false;
    }

    QSaveFile file(configPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    const QJsonDocument document(root);
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0) {
        return false;
    }

    if (!file.commit()) {
        return false;
    }

    rawRoot_ = root;
    return true;
}

QString JsonConfigRepository::configPath() const
{
    return configPath_;
}

QString JsonConfigRepository::lastLoadError() const
{
    return lastLoadError_;
}

Config JsonConfigRepository::parseConfig(const QJsonObject& root)
{
    Config config;
    config.logEnabled = root.value(QStringLiteral("logEnabled")).toBool(false);
    config.logLevel = root.value(QStringLiteral("loglevel")).toString();
    config.currentIndexId = root.value(QStringLiteral("indexId")).toString();
    const QJsonArray inbounds = root.value(QStringLiteral("inbound")).toArray();
    if (!inbounds.isEmpty() && inbounds.at(0).isObject()) {
        const QJsonObject inbound = inbounds.at(0).toObject();
        config.localPort = inbound.value(QStringLiteral("localPort")).toInt(10808);
        config.localProtocol = inbound.value(QStringLiteral("protocol")).toString(QStringLiteral("socks"));
        config.udpEnabled = inbound.value(QStringLiteral("udpEnabled")).toBool(true);
        config.sniffingEnabled = inbound.value(QStringLiteral("sniffingEnabled")).toBool(true);
        config.routeOnly = inbound.value(QStringLiteral("routeOnly")).toBool(false);
        config.allowLanConnection = inbound.value(QStringLiteral("allowLANConn")).toBool(false);
        config.inboundUser = inbound.value(QStringLiteral("user")).toString();
        config.inboundPassword = inbound.value(QStringLiteral("pass")).toString();
    }
    config.muxEnabled = root.value(QStringLiteral("muxEnabled")).toBool(false);
    config.mux4SboxProtocol = root.contains(QStringLiteral("mux4SboxProtocol"))
        ? root.value(QStringLiteral("mux4SboxProtocol")).toString()
        : QStringLiteral("h2mux");
    config.mux4SboxMaxConnections = root.contains(QStringLiteral("mux4SboxMaxConnections"))
        ? root.value(QStringLiteral("mux4SboxMaxConnections")).toInt(8)
        : 8;
    if (config.mux4SboxMaxConnections <= 0) {
        config.mux4SboxMaxConnections = 8;
    }
    if (root.contains(QStringLiteral("mux4SboxPadding")) && root.value(QStringLiteral("mux4SboxPadding")).isBool()) {
        config.mux4SboxPadding = root.value(QStringLiteral("mux4SboxPadding")).toBool();
    } else {
        config.mux4SboxPadding.reset();
    }
    config.sysProxyType = root.value(QStringLiteral("sysProxyType")).toInt(0);
    config.enableStatistics = root.value(QStringLiteral("enableStatistics")).toBool(false);
    config.keepOlderDedup = root.value(QStringLiteral("keepOlderDedupl")).toBool(false);
    config.statisticsFreshRate = root.value(QStringLiteral("statisticsFreshRate")).toInt(1);
    const QJsonObject uiItem = root.value(QStringLiteral("uiItem")).toObject();
    config.showMainOnStartup = uiItem.contains(QStringLiteral("showMainOnStartup"))
        ? uiItem.value(QStringLiteral("showMainOnStartup")).toBool(true)
        : true;
    config.autoRunEnabled = uiItem.value(QStringLiteral("autoRunEnabled")).toBool(false);
    config.languageCode = uiItem.value(QStringLiteral("languageCode")).toString();
    if (config.languageCode.trimmed().isEmpty()) {
        config.languageCode = root.value(QStringLiteral("languageCode")).toString();
    }
    config.mainLocationX = uiItem.value(QStringLiteral("mainLocationX")).toInt(0);
    config.mainLocationY = uiItem.value(QStringLiteral("mainLocationY")).toInt(0);
    config.mainSizeWidth = uiItem.value(QStringLiteral("mainSizeWidth")).toInt(0);
    config.mainSizeHeight = uiItem.value(QStringLiteral("mainSizeHeight")).toInt(0);
    config.mainSelectedSubId = uiItem.value(QStringLiteral("mainSelectedSubId")).toString();
    config.mainServerLogSplitPercent =
        normalizeSplitPercent(uiItem.value(QStringLiteral("mainServerLogSplitPercent")).toInt(60), 60);
    config.mainServerQrSplitPercent =
        normalizeSplitPercent(uiItem.value(QStringLiteral("mainServerQrSplitPercent")).toInt(78), 78);
    config.mainQrPreviewVisible = uiItem.contains(QStringLiteral("mainQrPreviewVisible"))
        ? uiItem.value(QStringLiteral("mainQrPreviewVisible")).toBool(false)
        : false;
    const QJsonObject mainLvColWidth = uiItem.value(QStringLiteral("mainLvColWidth")).toObject();
    for (auto it = mainLvColWidth.constBegin(); it != mainLvColWidth.constEnd(); ++it) {
        if (!it.value().isDouble()) {
            continue;
        }

        const int width = it.value().toInt(0);
        if (width > 0) {
            config.mainColumnWidths.insert(it.key(), width);
        }
    }
    const QJsonObject constItem = root.value(QStringLiteral("constItem")).toObject();
    config.speedTestUrl = constItem.value(QStringLiteral("speedTestUrl")).toString();
    config.speedPingTestUrl = constItem.value(QStringLiteral("speedPingTestUrl")).toString();
    config.defIeProxyExceptions = constItem.value(QStringLiteral("defIEProxyExceptions")).toString();
    config.enableFragment = root.value(QStringLiteral("enableFragment")).toBool(false);
    config.enableCacheFile4Sbox = root.contains(QStringLiteral("enableCacheFile4Sbox"))
        ? root.value(QStringLiteral("enableCacheFile4Sbox")).toBool(true)
        : true;
    config.defaultFingerprint = root.value(QStringLiteral("defFingerprint")).toString();
    config.defaultUserAgent = root.value(QStringLiteral("defUserAgent")).toString();
    if (root.contains(QStringLiteral("directDNS"))) {
        config.directDns = root.value(QStringLiteral("directDNS")).toString();
    }
    if (root.contains(QStringLiteral("remoteDNS"))) {
        config.remoteDns = root.value(QStringLiteral("remoteDNS")).toString();
    }
    if (root.contains(QStringLiteral("bootstrapDNS"))) {
        config.bootstrapDns = root.value(QStringLiteral("bootstrapDNS")).toString();
    }
    config.fakeIp = root.contains(QStringLiteral("fakeIP"))
        ? root.value(QStringLiteral("fakeIP")).toBool(false)
        : false;
    config.globalFakeIp = root.contains(QStringLiteral("globalFakeIp"))
        ? root.value(QStringLiteral("globalFakeIp")).toBool(true)
        : true;
    config.serveStale = root.contains(QStringLiteral("serveStale"))
        ? root.value(QStringLiteral("serveStale")).toBool(false)
        : false;
    config.parallelQuery = root.contains(QStringLiteral("enableParallelQuery"))
        ? root.value(QStringLiteral("enableParallelQuery")).toBool(false)
        : false;
    config.directExpectedIps = root.value(QStringLiteral("directExpectedIPs")).toString();
    config.useSystemHosts = root.contains(QStringLiteral("useSystemHosts"))
        ? root.value(QStringLiteral("useSystemHosts")).toBool(false)
        : false;
    config.addCommonHosts = root.contains(QStringLiteral("addCommonHosts"))
        ? root.value(QStringLiteral("addCommonHosts")).toBool(true)
        : true;
    config.blockBindingQuery = root.contains(QStringLiteral("blockBindingQuery"))
        ? root.value(QStringLiteral("blockBindingQuery")).toBool(true)
        : true;
    config.domainStrategyForFreedom = root.value(QStringLiteral("domainStrategy4Freedom")).toString();
    config.domainStrategyForProxy = root.value(QStringLiteral("domainStrategy4Proxy")).toString();
    config.dnsHosts = root.value(QStringLiteral("hosts")).toString();
    config.defaultAllowInsecure = root.value(QStringLiteral("defAllowInsecure")).toBool(false);
    config.domainStrategy = root.value(QStringLiteral("domainStrategy")).toString();
    config.domainStrategy4Singbox = root.value(QStringLiteral("domainStrategy4Singbox")).toString();
    config.domainMatcher = root.value(QStringLiteral("domainMatcher")).toString();
    config.routingIndex = root.value(QStringLiteral("routingIndex")).toInt(0);
    config.enableRoutingAdvanced = root.value(QStringLiteral("enableRoutingAdvanced")).toBool(false);
    config.ignoreGeoUpdateCore = root.value(QStringLiteral("ignoreGeoUpdateCore")).toBool(false);
    config.systemProxyExceptions = root.value(QStringLiteral("systemProxyExceptions")).toString();
    config.systemProxyAdvancedProtocol = root.value(QStringLiteral("systemProxyAdvancedProtocol")).toString();
    config.pacUrl = root.value(QStringLiteral("pacUrl")).toString();
    config.autoUpdateInterval = root.value(QStringLiteral("autoUpdateInterval")).toInt(0);
    config.autoUpdateSubInterval = root.value(QStringLiteral("autoUpdateSubInterval")).toInt(0);
    config.checkPreReleaseUpdate = root.value(QStringLiteral("checkPreReleaseUpdate")).toBool(false);
    config.enableSecurityProtocolTls13 = root.value(QStringLiteral("enableSecurityProtocolTls13")).toBool(false);
    config.trayMenuServersLimit = root.value(QStringLiteral("trayMenuServersLimit")).toInt(0);
    config.servers = parseServers(root.value(QStringLiteral("vmess")).toArray());
    config.subscriptions = parseSubscriptions(root.value(QStringLiteral("subItem")).toArray());
    QJsonArray globalHotkeys = root.value(QStringLiteral("GlobalHotkeys")).toArray();
    if (globalHotkeys.isEmpty()) {
        globalHotkeys = root.value(QStringLiteral("globalHotkeys")).toArray();
    }
    config.globalHotkeys = parseGlobalHotkeys(globalHotkeys);
    QJsonArray routings = root.value(QStringLiteral("routings")).toArray();
    if (routings.isEmpty()) {
        routings = root.value(QStringLiteral("routingItems")).toArray();
    }
    config.routingItems = parseRoutingItems(routings);
    ensureBuiltinRoutingItems(config);
    config.tunModeItem = parseTunModeItem(root.value(QStringLiteral("tunModeItem")).toObject());
    config.policyGroups = parsePolicyGroups(root.value(QStringLiteral("policyGroups")).toArray());
    config.coreTypeItems = parseCoreTypeItems(root.value(QStringLiteral("coreTypeItems")).toArray());
    return config;
}

void JsonConfigRepository::applyConfig(QJsonObject& root, const Config& config)
{
    root.insert(QStringLiteral("logEnabled"), config.logEnabled);
    root.insert(QStringLiteral("loglevel"), config.logLevel);
    root.insert(QStringLiteral("indexId"), config.currentIndexId);
    QJsonArray inbounds = root.value(QStringLiteral("inbound")).toArray();
    QJsonObject inbound = (!inbounds.isEmpty() && inbounds.at(0).isObject())
        ? inbounds.at(0).toObject()
        : QJsonObject();
    inbound.insert(QStringLiteral("localPort"), config.localPort);
    inbound.insert(QStringLiteral("protocol"), config.localProtocol);
    inbound.insert(QStringLiteral("udpEnabled"), config.udpEnabled);
    inbound.insert(QStringLiteral("sniffingEnabled"), config.sniffingEnabled);
    inbound.insert(QStringLiteral("routeOnly"), config.routeOnly);
    inbound.insert(QStringLiteral("allowLANConn"), config.allowLanConnection);
    inbound.insert(QStringLiteral("user"), config.inboundUser);
    inbound.insert(QStringLiteral("pass"), config.inboundPassword);
    if (inbounds.isEmpty()) {
        inbounds.append(inbound);
    } else {
        inbounds.replace(0, inbound);
    }
    root.insert(QStringLiteral("inbound"), inbounds);
    root.insert(QStringLiteral("muxEnabled"), config.muxEnabled);
    root.insert(QStringLiteral("mux4SboxProtocol"), config.mux4SboxProtocol);
    root.insert(QStringLiteral("mux4SboxMaxConnections"), config.mux4SboxMaxConnections > 0 ? config.mux4SboxMaxConnections : 8);
    root.insert(
        QStringLiteral("mux4SboxPadding"),
        config.mux4SboxPadding.has_value() ? QJsonValue(config.mux4SboxPadding.value()) : QJsonValue(QJsonValue::Null));
    root.insert(QStringLiteral("sysProxyType"), config.sysProxyType);
    root.insert(QStringLiteral("enableStatistics"), config.enableStatistics);
    root.insert(QStringLiteral("keepOlderDedupl"), config.keepOlderDedup);
    root.insert(QStringLiteral("statisticsFreshRate"), config.statisticsFreshRate);
    QJsonObject uiItem = root.value(QStringLiteral("uiItem")).toObject();
    uiItem.insert(QStringLiteral("showMainOnStartup"), config.showMainOnStartup);
    uiItem.insert(QStringLiteral("autoRunEnabled"), config.autoRunEnabled);
    uiItem.insert(QStringLiteral("languageCode"), config.languageCode);
    uiItem.insert(QStringLiteral("mainLocationX"), config.mainLocationX);
    uiItem.insert(QStringLiteral("mainLocationY"), config.mainLocationY);
    uiItem.insert(QStringLiteral("mainSizeWidth"), config.mainSizeWidth);
    uiItem.insert(QStringLiteral("mainSizeHeight"), config.mainSizeHeight);
    uiItem.insert(QStringLiteral("mainSelectedSubId"), config.mainSelectedSubId);
    uiItem.insert(QStringLiteral("mainServerLogSplitPercent"), normalizeSplitPercent(config.mainServerLogSplitPercent, 60));
    uiItem.insert(QStringLiteral("mainServerQrSplitPercent"), normalizeSplitPercent(config.mainServerQrSplitPercent, 78));
    uiItem.insert(QStringLiteral("mainQrPreviewVisible"), config.mainQrPreviewVisible);
    QJsonObject mainLvColWidth;
    for (auto it = config.mainColumnWidths.constBegin(); it != config.mainColumnWidths.constEnd(); ++it) {
        if (it.value() > 0) {
            mainLvColWidth.insert(it.key(), it.value());
        }
    }
    uiItem.insert(QStringLiteral("mainLvColWidth"), mainLvColWidth);
    root.insert(QStringLiteral("uiItem"), uiItem);
    root.remove(QStringLiteral("languageCode"));
    QJsonObject constItem = root.value(QStringLiteral("constItem")).toObject();
    constItem.insert(QStringLiteral("speedTestUrl"), config.speedTestUrl);
    constItem.insert(QStringLiteral("speedPingTestUrl"), config.speedPingTestUrl);
    constItem.insert(QStringLiteral("defIEProxyExceptions"), config.defIeProxyExceptions);
    root.insert(QStringLiteral("constItem"), constItem);
    root.insert(QStringLiteral("enableFragment"), config.enableFragment);
    root.insert(QStringLiteral("enableCacheFile4Sbox"), config.enableCacheFile4Sbox);
    root.insert(QStringLiteral("directDNS"), config.directDns);
    root.insert(QStringLiteral("remoteDNS"), config.remoteDns);
    root.insert(QStringLiteral("bootstrapDNS"), config.bootstrapDns);
    root.insert(QStringLiteral("fakeIP"), config.fakeIp);
    root.insert(QStringLiteral("globalFakeIp"), config.globalFakeIp);
    root.insert(QStringLiteral("serveStale"), config.serveStale);
    root.insert(QStringLiteral("enableParallelQuery"), config.parallelQuery);
    root.insert(QStringLiteral("directExpectedIPs"), config.directExpectedIps);
    root.insert(QStringLiteral("useSystemHosts"), config.useSystemHosts);
    root.insert(QStringLiteral("addCommonHosts"), config.addCommonHosts);
    root.insert(QStringLiteral("blockBindingQuery"), config.blockBindingQuery);
    root.insert(QStringLiteral("domainStrategy4Freedom"), config.domainStrategyForFreedom);
    root.insert(QStringLiteral("domainStrategy4Proxy"), config.domainStrategyForProxy);
    root.insert(QStringLiteral("hosts"), config.dnsHosts);
    root.insert(QStringLiteral("defAllowInsecure"), config.defaultAllowInsecure);
    root.insert(QStringLiteral("defFingerprint"), config.defaultFingerprint);
    root.insert(QStringLiteral("defUserAgent"), config.defaultUserAgent);
    root.insert(QStringLiteral("domainStrategy"), config.domainStrategy);
    root.insert(QStringLiteral("domainStrategy4Singbox"), config.domainStrategy4Singbox);
    root.insert(QStringLiteral("domainMatcher"), config.domainMatcher);
    root.insert(QStringLiteral("routingIndex"), config.routingIndex);
    root.insert(QStringLiteral("enableRoutingAdvanced"), config.enableRoutingAdvanced);
    root.insert(QStringLiteral("ignoreGeoUpdateCore"), config.ignoreGeoUpdateCore);
    root.insert(QStringLiteral("systemProxyExceptions"), config.systemProxyExceptions);
    root.insert(QStringLiteral("systemProxyAdvancedProtocol"), config.systemProxyAdvancedProtocol);
    root.insert(QStringLiteral("pacUrl"), config.pacUrl);
    root.insert(QStringLiteral("autoUpdateInterval"), config.autoUpdateInterval);
    root.insert(QStringLiteral("autoUpdateSubInterval"), config.autoUpdateSubInterval);
    root.insert(QStringLiteral("checkPreReleaseUpdate"), config.checkPreReleaseUpdate);
    root.insert(QStringLiteral("enableSecurityProtocolTls13"), config.enableSecurityProtocolTls13);
    root.insert(QStringLiteral("trayMenuServersLimit"), config.trayMenuServersLimit);
    root.insert(QStringLiteral("vmess"), toServerArray(config.servers));
    root.insert(QStringLiteral("subItem"), toSubscriptionArray(config.subscriptions));
    root.insert(QStringLiteral("GlobalHotkeys"), toGlobalHotkeyArray(config.globalHotkeys));
    root.remove(QStringLiteral("globalHotkeys"));
    root.insert(QStringLiteral("routings"), toRoutingArray(config.routingItems));
    root.remove(QStringLiteral("routingItems"));
    root.insert(QStringLiteral("tunModeItem"), toTunModeItem(config.tunModeItem));
    root.insert(QStringLiteral("policyGroups"), toPolicyGroupArray(config.policyGroups));
    root.insert(QStringLiteral("coreTypeItems"), toCoreTypeItemArray(config.coreTypeItems));
}

TunModeItem JsonConfigRepository::parseTunModeItem(const QJsonObject& object)
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

QJsonObject JsonConfigRepository::toTunModeItem(const TunModeItem& item)
{
    QJsonObject object;
    object.insert(QStringLiteral("enableTun"), item.enableTun);
    object.insert(QStringLiteral("autoRoute"), item.autoRoute);
    object.insert(QStringLiteral("strictRoute"), item.strictRoute);
    object.insert(QStringLiteral("stack"), item.stack);
    object.insert(QStringLiteral("mtu"), item.mtu);
    object.insert(QStringLiteral("enableIPv6Address"), item.enableIPv6Address);
    object.insert(QStringLiteral("icmpRouting"), item.icmpRouting);
    object.insert(QStringLiteral("enableLegacyProtect"), item.enableLegacyProtect);
    return object;
}

QList<PolicyGroupItem> JsonConfigRepository::parsePolicyGroups(const QJsonArray& array)
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

QJsonArray JsonConfigRepository::toPolicyGroupArray(const QList<PolicyGroupItem>& items)
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

QList<CoreTypeItem> JsonConfigRepository::parseCoreTypeItems(const QJsonArray& array)
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

QJsonArray JsonConfigRepository::toCoreTypeItemArray(const QList<CoreTypeItem>& items)
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

int JsonConfigRepository::normalizeSplitPercent(int value, int fallback)
{
    return value < 10 || value > 90 ? fallback : value;
}

QList<VmessItem> JsonConfigRepository::parseServers(const QJsonArray& array)
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
        item.subId = object.value(QStringLiteral("subid")).toString();
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
        item.sort = object.value(QStringLiteral("sort")).toInt(0);
        item.extra = object.value(QStringLiteral("extra")).toString();
        item.userAgent = object.value(QStringLiteral("userAgent")).toString();
        item.finalmask = object.value(QStringLiteral("finalmask")).toString();
        // Hysteria2
        item.obfsPassword = object.value(QStringLiteral("obfsPassword")).toString();
        item.upMbps = object.value(QStringLiteral("upMbps")).toString();
        item.downMbps = object.value(QStringLiteral("downMbps")).toString();
        // TUIC
        item.congestionControl = object.value(QStringLiteral("congestionControl")).toString();
        item.udpRelayMode = object.value(QStringLiteral("udpRelayMode")).toString();
        item.zeroRttHandshake = object.value(QStringLiteral("zeroRttHandshake")).toBool(false);
        // WireGuard
        item.privateKey = object.value(QStringLiteral("privateKey")).toString();
        item.peerPublicKey = object.value(QStringLiteral("peerPublicKey")).toString();
        item.localAddress = object.value(QStringLiteral("localAddress")).toString();
        item.wireguardMtu = object.value(QStringLiteral("wireguardMtu")).toInt(0);
        item.reserved = object.value(QStringLiteral("reserved")).toString();
        // Naive
        item.username = object.value(QStringLiteral("username")).toString();
        item.naiveQuic = object.value(QStringLiteral("naiveQuic")).toBool(false);
        item.insecureConcurrency = object.value(QStringLiteral("insecureConcurrency")).toInt(0);

        const QJsonArray alpnArray = object.value(QStringLiteral("alpn")).toArray();
        for (const QJsonValue& alpnValue : alpnArray) {
            item.alpn.append(alpnValue.toString());
        }

        items.append(item);
    }

    return items;
}

QList<SubItem> JsonConfigRepository::parseSubscriptions(const QJsonArray& array)
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

QList<GlobalHotkeyItem> JsonConfigRepository::parseGlobalHotkeys(const QJsonArray& array)
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

QList<RoutingItem> JsonConfigRepository::parseRoutingItems(const QJsonArray& array)
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
        item.domainStrategy4Singbox = object.value(QStringLiteral("domainStrategy4Singbox")).toString();
        item.rules = parseRoutingRules(object.value(QStringLiteral("rules")).toArray());
        items.append(item);
    }

    return items;
}

QList<RoutingRule> JsonConfigRepository::parseRoutingRules(const QJsonArray& array)
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

QJsonArray JsonConfigRepository::toServerArray(const QList<VmessItem>& items)
{
    QJsonArray array;
    for (const VmessItem& item : items) {
        QJsonObject object;
        object.insert(QStringLiteral("indexId"), item.indexId);
        object.insert(QStringLiteral("configType"), toLegacyConfigTypeValue(item.configType));
        object.insert(QStringLiteral("coreType"), toLegacyCoreTypeValue(item.coreType));
        object.insert(QStringLiteral("address"), item.address);
        object.insert(QStringLiteral("port"), item.port);
        object.insert(QStringLiteral("id"), item.id);
        object.insert(QStringLiteral("alterId"), item.alterId);
        object.insert(QStringLiteral("security"), item.security);
        object.insert(QStringLiteral("network"), item.network);
        object.insert(QStringLiteral("remarks"), item.remarks);
        object.insert(QStringLiteral("headerType"), item.headerType);
        object.insert(QStringLiteral("requestHost"), item.requestHost);
        object.insert(QStringLiteral("path"), item.path);
        object.insert(QStringLiteral("streamSecurity"), item.streamSecurity);
        object.insert(QStringLiteral("allowInsecure"), item.allowInsecure);
        object.insert(QStringLiteral("testResult"), item.testResult);
        object.insert(QStringLiteral("subid"), item.subId);
        object.insert(QStringLiteral("flow"), item.flow);
        object.insert(QStringLiteral("sni"), item.sni);
        object.insert(QStringLiteral("preSocksPort"), item.preSocksPort);
        object.insert(QStringLiteral("fingerprint"), item.fingerprint);
        object.insert(QStringLiteral("publicKey"), item.publicKey);
        object.insert(QStringLiteral("shortId"), item.shortId);
        object.insert(QStringLiteral("spiderX"), item.spiderX);
        object.insert(QStringLiteral("mldsa65Verify"), item.mldsa65Verify);
        object.insert(QStringLiteral("echConfigList"), item.echConfigList);
        object.insert(QStringLiteral("echForceQuery"), item.echForceQuery);
        object.insert(QStringLiteral("cert"), item.cert);
        object.insert(QStringLiteral("certSha"), item.certSha);
        object.insert(
            QStringLiteral("muxEnabled"),
            item.muxEnabled.has_value() ? QJsonValue(item.muxEnabled.value()) : QJsonValue(QJsonValue::Null));
        object.insert(QStringLiteral("sort"), item.sort);
        object.insert(QStringLiteral("extra"), item.extra);
        object.insert(QStringLiteral("userAgent"), item.userAgent);
        object.insert(QStringLiteral("finalmask"), item.finalmask);
        // Hysteria2
        object.insert(QStringLiteral("obfsPassword"), item.obfsPassword);
        object.insert(QStringLiteral("upMbps"), item.upMbps);
        object.insert(QStringLiteral("downMbps"), item.downMbps);
        // TUIC
        object.insert(QStringLiteral("congestionControl"), item.congestionControl);
        object.insert(QStringLiteral("udpRelayMode"), item.udpRelayMode);
        object.insert(QStringLiteral("zeroRttHandshake"), item.zeroRttHandshake);
        // WireGuard
        object.insert(QStringLiteral("privateKey"), item.privateKey);
        object.insert(QStringLiteral("peerPublicKey"), item.peerPublicKey);
        object.insert(QStringLiteral("localAddress"), item.localAddress);
        object.insert(QStringLiteral("wireguardMtu"), item.wireguardMtu);
        object.insert(QStringLiteral("reserved"), item.reserved);
        // Naive
        object.insert(QStringLiteral("username"), item.username);
        object.insert(QStringLiteral("naiveQuic"), item.naiveQuic);
        object.insert(QStringLiteral("insecureConcurrency"), item.insecureConcurrency);

        QJsonArray alpnArray;
        for (const QString& value : item.alpn) {
            alpnArray.append(value);
        }
        object.insert(QStringLiteral("alpn"), alpnArray);

        array.append(object);
    }

    return array;
}

QJsonArray JsonConfigRepository::toSubscriptionArray(const QList<SubItem>& items)
{
    QJsonArray array;
    for (const SubItem& item : items) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), item.id);
        object.insert(QStringLiteral("remarks"), item.remarks);
        object.insert(QStringLiteral("url"), item.url);
        object.insert(QStringLiteral("enabled"), item.enabled);
        object.insert(QStringLiteral("userAgent"), item.userAgent);
        array.append(object);
    }

    return array;
}

QJsonArray JsonConfigRepository::toGlobalHotkeyArray(const QList<GlobalHotkeyItem>& items)
{
    QJsonArray array;
    for (const GlobalHotkeyItem& item : items) {
        QJsonObject object;
        object.insert(QStringLiteral("EGlobalHotkey"), static_cast<int>(item.action));
        object.insert(QStringLiteral("Alt"), item.alt);
        object.insert(QStringLiteral("Control"), item.control);
        object.insert(QStringLiteral("Shift"), item.shift);
        object.insert(
            QStringLiteral("KeyCode"),
            item.keyCode.has_value() ? QJsonValue(item.keyCode.value()) : QJsonValue(QJsonValue::Null));
        array.append(object);
    }

    return array;
}

QJsonArray JsonConfigRepository::toRoutingArray(const QList<RoutingItem>& items)
{
    QJsonArray array;
    for (const RoutingItem& item : items) {
        QJsonObject object;
        object.insert(QStringLiteral("remarks"), item.remarks);
        object.insert(QStringLiteral("url"), item.url);
        object.insert(QStringLiteral("enabled"), item.enabled);
        object.insert(QStringLiteral("locked"), item.locked);
        object.insert(QStringLiteral("customIcon"), item.customIcon);
        object.insert(QStringLiteral("domainStrategy4Singbox"), item.domainStrategy4Singbox);
        object.insert(QStringLiteral("rules"), toRoutingRuleArray(item.rules));
        array.append(object);
    }

    return array;
}

QJsonArray JsonConfigRepository::toRoutingRuleArray(const QList<RoutingRule>& items)
{
    QJsonArray array;
    for (const RoutingRule& item : items) {
        QJsonObject object;
        object.insert(QStringLiteral("type"), item.type);
        object.insert(QStringLiteral("port"), item.port);
        object.insert(QStringLiteral("network"), item.network);
        object.insert(QStringLiteral("outboundTag"), item.outboundTag);
        object.insert(QStringLiteral("enabled"), item.enabled);

        QJsonArray inboundTagArray;
        for (const QString& value : item.inboundTag) {
            inboundTagArray.append(value);
        }
        object.insert(QStringLiteral("inboundTag"), inboundTagArray);

        QJsonArray ipArray;
        for (const QString& value : item.ip) {
            ipArray.append(value);
        }
        object.insert(QStringLiteral("ip"), ipArray);

        QJsonArray domainArray;
        for (const QString& value : item.domain) {
            domainArray.append(value);
        }
        object.insert(QStringLiteral("domain"), domainArray);

        QJsonArray protocolArray;
        for (const QString& value : item.protocol) {
            protocolArray.append(value);
        }
        object.insert(QStringLiteral("protocol"), protocolArray);

        QJsonArray processArray;
        for (const QString& value : item.process) {
            processArray.append(value);
        }
        object.insert(QStringLiteral("process"), processArray);

        array.append(object);
    }

    return array;
}

ConfigType JsonConfigRepository::parseConfigType(const QJsonValue& value)
{
    if (value.isDouble()) {
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

    const QString text = value.toString().trimmed();
    if (text.compare(QStringLiteral("VMess"), Qt::CaseInsensitive) == 0) {
        return ConfigType::VMess;
    }
    if (text.compare(QStringLiteral("Custom"), Qt::CaseInsensitive) == 0) {
        return ConfigType::Custom;
    }
    if (text.compare(QStringLiteral("Shadowsocks"), Qt::CaseInsensitive) == 0) {
        return ConfigType::Shadowsocks;
    }
    if (text.compare(QStringLiteral("Socks"), Qt::CaseInsensitive) == 0) {
        return ConfigType::Socks;
    }
    if (text.compare(QStringLiteral("VLESS"), Qt::CaseInsensitive) == 0) {
        return ConfigType::VLESS;
    }
    if (text.compare(QStringLiteral("Trojan"), Qt::CaseInsensitive) == 0) {
        return ConfigType::Trojan;
    }
    if (text.compare(QStringLiteral("Hysteria2"), Qt::CaseInsensitive) == 0) {
        return ConfigType::Hysteria2;
    }
    if (text.compare(QStringLiteral("TUIC"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("Tuic"), Qt::CaseInsensitive) == 0) {
        return ConfigType::TUIC;
    }
    if (text.compare(QStringLiteral("WireGuard"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("Wireguard"), Qt::CaseInsensitive) == 0) {
        return ConfigType::WireGuard;
    }
    if (text.compare(QStringLiteral("HTTP"), Qt::CaseInsensitive) == 0) {
        return ConfigType::HTTP;
    }
    if (text.compare(QStringLiteral("AnyTLS"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("Anytls"), Qt::CaseInsensitive) == 0) {
        return ConfigType::AnyTLS;
    }
    if (text.compare(QStringLiteral("Naive"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("NaiveProxy"), Qt::CaseInsensitive) == 0) {
        return ConfigType::Naive;
    }

    return ConfigType::Unknown;
}

CoreType JsonConfigRepository::parseCoreType(const QJsonValue& value)
{
    if (value.isUndefined() || value.isNull()) {
        return CoreType::Auto;
    }

    if (value.isDouble()) {
        switch (value.toInt()) {
        case 1:
            return CoreType::V2Fly;
        case 2:
            return CoreType::Xray;
        case 3:
            return CoreType::SagerNet;
        case 4:
            return CoreType::V2FlyV5;
        case 11:
            return CoreType::Clash;
        case 12:
            return CoreType::ClashMeta;
        case 21:
            return CoreType::Hysteria;
        case 22:
            return CoreType::NaiveProxy;
        case 24:
            return CoreType::SingBox;
        default:
            return CoreType::Unknown;
        }
    }

    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        return CoreType::Auto;
    }
    if (text.compare(QStringLiteral("v2fly"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("v2ray"), Qt::CaseInsensitive) == 0) {
        return CoreType::V2Fly;
    }
    if (text.compare(QStringLiteral("xray"), Qt::CaseInsensitive) == 0) {
        return CoreType::Xray;
    }
    if (text.compare(QStringLiteral("sagernet"), Qt::CaseInsensitive) == 0) {
        return CoreType::SagerNet;
    }
    if (text.compare(QStringLiteral("v2fly_v5"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("v2ray_v5"), Qt::CaseInsensitive) == 0) {
        return CoreType::V2FlyV5;
    }
    if (text.compare(QStringLiteral("clash"), Qt::CaseInsensitive) == 0) {
        return CoreType::Clash;
    }
    if (text.compare(QStringLiteral("clash_meta"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("clash-meta"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("clash.meta"), Qt::CaseInsensitive) == 0) {
        return CoreType::ClashMeta;
    }
    if (text.compare(QStringLiteral("hysteria"), Qt::CaseInsensitive) == 0) {
        return CoreType::Hysteria;
    }
    if (text.compare(QStringLiteral("naiveproxy"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("naive"), Qt::CaseInsensitive) == 0) {
        return CoreType::NaiveProxy;
    }
    if (text.compare(QStringLiteral("sing_box"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("sing-box"), Qt::CaseInsensitive) == 0) {
        return CoreType::SingBox;
    }

    return CoreType::Unknown;
}

int JsonConfigRepository::toLegacyConfigTypeValue(ConfigType type)
{
    switch (type) {
    case ConfigType::VMess:
        return 1;
    case ConfigType::Custom:
        return 2;
    case ConfigType::Shadowsocks:
        return 3;
    case ConfigType::Socks:
        return 4;
    case ConfigType::VLESS:
        return 5;
    case ConfigType::Trojan:
        return 6;
    case ConfigType::Hysteria2:
        return 7;
    case ConfigType::TUIC:
        return 8;
    case ConfigType::WireGuard:
        return 9;
    case ConfigType::HTTP:
        return 11;
    case ConfigType::AnyTLS:
        return 13;
    case ConfigType::Naive:
        return 14;
    case ConfigType::Unknown:
    default:
        return 1;
    }
}

QJsonValue JsonConfigRepository::toLegacyCoreTypeValue(CoreType type)
{
    switch (type) {
    case CoreType::Auto:
        return QJsonValue(QJsonValue::Null);
    case CoreType::V2Fly:
        return 1;
    case CoreType::Xray:
        return 2;
    case CoreType::SagerNet:
        return 3;
    case CoreType::V2FlyV5:
        return 4;
    case CoreType::Clash:
        return 11;
    case CoreType::ClashMeta:
        return 12;
    case CoreType::Hysteria:
        return 21;
    case CoreType::NaiveProxy:
        return 22;
    case CoreType::SingBox:
        return 24;
    case CoreType::Unknown:
    default:
        return QJsonValue(QJsonValue::Null);
    }
}
