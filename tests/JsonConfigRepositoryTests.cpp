#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "persistence/JsonConfigRepository.h"

namespace {

QString makeConfigPath(QTemporaryDir& tempDir)
{
    return tempDir.filePath(QStringLiteral("songbird.json"));
}

QString makeStatePath(QTemporaryDir& tempDir)
{
    return tempDir.filePath(QStringLiteral("SongBird.state.json"));
}

bool writeJsonFile(const QString& path, const QJsonObject& root)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    return file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0;
}

QJsonObject readJsonFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

VmessItem makeServer()
{
    VmessItem item;
    item.indexId = QStringLiteral("server-1");
    item.configType = ConfigType::Trojan;
    item.coreType = CoreType::SingBox;
    item.address = QStringLiteral("1.2.3.4");
    item.port = 443;
    item.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    item.remarks = QStringLiteral("primary");
    item.streamSecurity = QStringLiteral("tls");
    item.muxEnabled = false;
    item.finalmask = QStringLiteral("{\"udp\":[{\"type\":\"mkcp-original\"}]}");
    item.cert = QStringLiteral("-----BEGIN CERTIFICATE-----");
    item.certSha = QStringLiteral("sha256/example");
    item.mldsa65Verify = QStringLiteral("mldsa-key");
    item.echConfigList = QStringLiteral("ECHCONFIGBASE64");
    item.echForceQuery = QStringLiteral("half");
    item.subId = QStringLiteral("sub-1");
    item.alpn = QStringList{QStringLiteral("h2"), QStringLiteral("http/1.1")};
    return item;
}

RoutingRule makeRoutingRule()
{
    RoutingRule item;
    item.type = QStringLiteral("field");
    item.port = QStringLiteral("443");
    item.network = QStringLiteral("tcp,udp");
    item.outboundTag = QStringLiteral("proxy");
    item.inboundTag = QStringList{QStringLiteral("socks"), QStringLiteral("http")};
    item.ip = QStringList{QStringLiteral("geoip:private")};
    item.domain = QStringList{QStringLiteral("geosite:cn")};
    item.protocol = QStringList{QStringLiteral("bittorrent")};
    item.process = QStringList{QStringLiteral("chrome.exe")};
    item.enabled = true;
    return item;
}

RoutingItem makeRoutingItem()
{
    RoutingItem item;
    item.remarks = QStringLiteral("custom-route");
    item.url = QStringLiteral("https://example.test/route.json");
    item.rules = {makeRoutingRule()};
    item.enabled = true;
    item.locked = true;
    item.customIcon = QStringLiteral("route.svg");
    item.domainStrategy4Singbox = QStringLiteral("prefer_ipv4");
    return item;
}

SubItem makeSubscription()
{
    SubItem item;
    item.id = QStringLiteral("sub-1");
    item.remarks = QStringLiteral("feed");
    item.url = QStringLiteral("https://example.test/sub");
    item.enabled = true;
    item.userAgent = QStringLiteral("SongBird/1.0");
    return item;
}

PolicyGroupItem makePolicyGroup()
{
    PolicyGroupItem item;
    item.id = QStringLiteral("group-1");
    item.name = QStringLiteral("auto");
    item.strategy = PolicyGroupItem::Strategy::UrlTest;
    item.memberServerIds = QStringList{QStringLiteral("server-1"), QStringLiteral("server-2")};
    item.urlTestUrl = QStringLiteral("https://connectivitycheck.gstatic.com/generate_204");
    item.toleranceMs = 150;
    return item;
}

CoreTypeItem makeCoreTypeItem()
{
    return CoreTypeItem{
        static_cast<int>(ConfigType::Trojan),
        static_cast<int>(CoreType::SingBox)};
}

QJsonObject makeCanonicalRoot()
{
    const VmessItem server = makeServer();
    const RoutingItem routingItem = makeRoutingItem();
    const RoutingRule routeRule = routingItem.rules.constFirst();
    const SubItem subscription = makeSubscription();
    const PolicyGroupItem policyGroup = makePolicyGroup();
    const CoreTypeItem coreTypeItem = makeCoreTypeItem();

    QJsonObject defaults;
    defaults.insert(QStringLiteral("speedPingTestUrl"), QStringLiteral("https://probe.example/test"));
    defaults.insert(QStringLiteral("ieProxyExceptions"), QStringLiteral("localhost;127.*"));

    QJsonObject inbound;
    inbound.insert(QStringLiteral("localPort"), 10809);
    inbound.insert(QStringLiteral("protocol"), QStringLiteral("http"));
    inbound.insert(QStringLiteral("udpEnabled"), false);
    inbound.insert(QStringLiteral("sniffingEnabled"), false);
    inbound.insert(QStringLiteral("routeOnly"), true);
    inbound.insert(QStringLiteral("allowLANConn"), true);
    inbound.insert(QStringLiteral("user"), QStringLiteral("tester"));
    inbound.insert(QStringLiteral("pass"), QStringLiteral("secret"));

    QJsonObject mainColumnWidths;
    mainColumnWidths.insert(QStringLiteral("address"), 240);

    QJsonObject ui;
    ui.insert(QStringLiteral("showMainOnStartup"), false);
    ui.insert(QStringLiteral("autoRunEnabled"), true);
    ui.insert(QStringLiteral("languageCode"), QStringLiteral("en-US"));
    ui.insert(QStringLiteral("themeName"), QStringLiteral("Dark"));
    ui.insert(QStringLiteral("mainLocationX"), 10);
    ui.insert(QStringLiteral("mainLocationY"), 20);
    ui.insert(QStringLiteral("mainSizeWidth"), 1280);
    ui.insert(QStringLiteral("mainSizeHeight"), 720);
    ui.insert(QStringLiteral("mainSelectedSubscriptionId"), QStringLiteral("sub-1"));
    ui.insert(QStringLiteral("settingsRoutingRuleTabKey"), QStringLiteral("basic"));
    ui.insert(QStringLiteral("mainServerLogSplitPercent"), 55);
    ui.insert(QStringLiteral("mainServerQrSplitPercent"), 80);
    ui.insert(QStringLiteral("mainQrPreviewVisible"), true);
    ui.insert(QStringLiteral("mainProxyEnabled"), true);
    ui.insert(QStringLiteral("mainColumnWidths"), mainColumnWidths);

    QJsonObject tunModeItem;
    tunModeItem.insert(QStringLiteral("enableTun"), true);
    tunModeItem.insert(QStringLiteral("autoRoute"), false);
    tunModeItem.insert(QStringLiteral("strictRoute"), false);
    tunModeItem.insert(QStringLiteral("stack"), QStringLiteral("mixed"));
    tunModeItem.insert(QStringLiteral("mtu"), 1400);
    tunModeItem.insert(QStringLiteral("enableIPv6Address"), true);
    tunModeItem.insert(QStringLiteral("icmpRouting"), QStringLiteral("direct"));
    tunModeItem.insert(QStringLiteral("enableLegacyProtect"), true);

    QJsonObject serverObject;
    serverObject.insert(QStringLiteral("indexId"), server.indexId);
    serverObject.insert(QStringLiteral("configType"), static_cast<int>(server.configType));
    serverObject.insert(QStringLiteral("coreType"), static_cast<int>(server.coreType));
    serverObject.insert(QStringLiteral("address"), server.address);
    serverObject.insert(QStringLiteral("port"), server.port);
    serverObject.insert(QStringLiteral("id"), server.id);
    serverObject.insert(QStringLiteral("remarks"), server.remarks);
    serverObject.insert(QStringLiteral("streamSecurity"), server.streamSecurity);
    serverObject.insert(QStringLiteral("muxEnabled"), server.muxEnabled.value());
    serverObject.insert(QStringLiteral("finalmask"), server.finalmask);
    serverObject.insert(QStringLiteral("cert"), server.cert);
    serverObject.insert(QStringLiteral("certSha"), server.certSha);
    serverObject.insert(QStringLiteral("mldsa65Verify"), server.mldsa65Verify);
    serverObject.insert(QStringLiteral("echConfigList"), server.echConfigList);
    serverObject.insert(QStringLiteral("echForceQuery"), server.echForceQuery);
    serverObject.insert(QStringLiteral("subscriptionId"), server.subId);
    serverObject.insert(QStringLiteral("alpn"), QJsonArray{server.alpn.at(0), server.alpn.at(1)});

    QJsonObject subscriptionObject;
    subscriptionObject.insert(QStringLiteral("id"), subscription.id);
    subscriptionObject.insert(QStringLiteral("remarks"), subscription.remarks);
    subscriptionObject.insert(QStringLiteral("url"), subscription.url);
    subscriptionObject.insert(QStringLiteral("enabled"), subscription.enabled);
    subscriptionObject.insert(QStringLiteral("userAgent"), subscription.userAgent);

    QJsonObject ruleObject;
    ruleObject.insert(QStringLiteral("type"), routeRule.type);
    ruleObject.insert(QStringLiteral("port"), routeRule.port);
    ruleObject.insert(QStringLiteral("network"), routeRule.network);
    ruleObject.insert(QStringLiteral("outboundTag"), routeRule.outboundTag);
    ruleObject.insert(QStringLiteral("enabled"), routeRule.enabled);
    ruleObject.insert(QStringLiteral("inboundTag"), QJsonArray{routeRule.inboundTag.at(0), routeRule.inboundTag.at(1)});
    ruleObject.insert(QStringLiteral("ip"), QJsonArray{routeRule.ip.at(0)});
    ruleObject.insert(QStringLiteral("domain"), QJsonArray{routeRule.domain.at(0)});
    ruleObject.insert(QStringLiteral("protocol"), QJsonArray{routeRule.protocol.at(0)});
    ruleObject.insert(QStringLiteral("process"), QJsonArray{routeRule.process.at(0)});

    QJsonObject routingObject;
    routingObject.insert(QStringLiteral("remarks"), routingItem.remarks);
    routingObject.insert(QStringLiteral("url"), routingItem.url);
    routingObject.insert(QStringLiteral("enabled"), routingItem.enabled);
    routingObject.insert(QStringLiteral("locked"), routingItem.locked);
    routingObject.insert(QStringLiteral("customIcon"), routingItem.customIcon);
    routingObject.insert(QStringLiteral("domainStrategyForSingbox"), routingItem.domainStrategy4Singbox);
    routingObject.insert(QStringLiteral("rules"), QJsonArray{ruleObject});

    QJsonObject policyGroupObject;
    policyGroupObject.insert(QStringLiteral("id"), policyGroup.id);
    policyGroupObject.insert(QStringLiteral("name"), policyGroup.name);
    policyGroupObject.insert(QStringLiteral("strategy"), static_cast<int>(policyGroup.strategy));
    policyGroupObject.insert(QStringLiteral("memberServerIds"), QJsonArray{policyGroup.memberServerIds.at(0), policyGroup.memberServerIds.at(1)});
    policyGroupObject.insert(QStringLiteral("urlTestUrl"), policyGroup.urlTestUrl);
    policyGroupObject.insert(QStringLiteral("toleranceMs"), policyGroup.toleranceMs);

    QJsonObject coreTypeObject;
    coreTypeObject.insert(QStringLiteral("configType"), coreTypeItem.configType);
    coreTypeObject.insert(QStringLiteral("coreType"), coreTypeItem.coreType);

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 7);
    root.insert(QStringLiteral("logEnabled"), true);
    root.insert(QStringLiteral("logLevel"), QStringLiteral("warning"));
    root.insert(QStringLiteral("indexId"), QStringLiteral("server-1"));
    root.insert(QStringLiteral("inbound"), QJsonArray{inbound});
    root.insert(QStringLiteral("defaults"), defaults);
    root.insert(QStringLiteral("muxEnabled"), true);
    root.insert(QStringLiteral("mux4SboxProtocol"), QStringLiteral("smux"));
    root.insert(QStringLiteral("mux4SboxMaxConnections"), 16);
    root.insert(QStringLiteral("mux4SboxPadding"), true);
    root.insert(QStringLiteral("sysProxyType"), 2);
    root.insert(QStringLiteral("enableFragment"), true);
    root.insert(QStringLiteral("enableCacheFile4Sbox"), false);
    root.insert(QStringLiteral("defaultFingerprint"), QStringLiteral("chrome"));
    root.insert(QStringLiteral("defaultUserAgent"), QStringLiteral("Mozilla/5.0"));
    root.insert(QStringLiteral("directDns"), QStringLiteral("223.5.5.5"));
    root.insert(QStringLiteral("remoteDns"), QStringLiteral("8.8.8.8"));
    root.insert(QStringLiteral("bootstrapDns"), QStringLiteral("119.29.29.29"));
    root.insert(QStringLiteral("fakeIp"), true);
    root.insert(QStringLiteral("globalFakeIp"), false);
    root.insert(QStringLiteral("serveStale"), true);
    root.insert(QStringLiteral("parallelQuery"), true);
    root.insert(QStringLiteral("directExpectedIps"), QStringLiteral("geoip:cn"));
    root.insert(QStringLiteral("useSystemHosts"), true);
    root.insert(QStringLiteral("addCommonHosts"), false);
    root.insert(QStringLiteral("blockBindingQuery"), false);
    root.insert(QStringLiteral("domainStrategyForFreedom"), QStringLiteral("UseIPv4"));
    root.insert(QStringLiteral("domainStrategyForProxy"), QStringLiteral("UseIPv6"));
    root.insert(QStringLiteral("dnsHosts"), QStringLiteral("example.com 1.2.3.4"));
    root.insert(QStringLiteral("defaultAllowInsecure"), true);
    root.insert(QStringLiteral("domainStrategy"), QStringLiteral("prefer_ipv4"));
    root.insert(QStringLiteral("domainStrategyForSingbox"), QStringLiteral("prefer_ipv6"));
    root.insert(QStringLiteral("domainMatcher"), QStringLiteral("mph"));
    root.insert(QStringLiteral("ignoreGeoUpdateCore"), true);
    root.insert(QStringLiteral("systemProxyAdvancedProtocol"), QStringLiteral("http"));
    root.insert(QStringLiteral("checkPreReleaseUpdate"), true);
    root.insert(QStringLiteral("ui"), ui);
    root.insert(QStringLiteral("tunModeItem"), tunModeItem);
    root.insert(QStringLiteral("servers"), QJsonArray{serverObject});
    root.insert(QStringLiteral("subscriptions"), QJsonArray{subscriptionObject});
    root.insert(QStringLiteral("routingIndex"), 1);
    root.insert(QStringLiteral("enableRoutingAdvanced"), true);
    root.insert(QStringLiteral("routingItems"), QJsonArray{routingObject});
    root.insert(QStringLiteral("routingCustomRules"), QJsonArray{ruleObject});
    root.insert(QStringLiteral("policyGroups"), QJsonArray{policyGroupObject});
    root.insert(QStringLiteral("coreTypeItems"), QJsonArray{coreTypeObject});
    return root;
}

} // namespace

class JsonConfigRepositoryTests : public QObject {
    Q_OBJECT

private slots:
    void loadMissingFileBuildsDefaultSongBirdConfig();
    void loadReadsCanonicalSongBirdStructure();
    void loadTreatsMissingServerConfigTypeAsVmess();
    void loadMergesStateFileWithoutBlockingOnMissingOrInvalidState();
    void loadIgnoresLegacyOnlyFields();
    void saveWritesCanonicalSongBirdStructure();
    void saveRemovesEmptyStateFile();
    void saveReplacesExistingRootInsteadOfMerging();
};

void JsonConfigRepositoryTests::loadMissingFileBuildsDefaultSongBirdConfig()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    JsonConfigRepository repository(makeConfigPath(tempDir));
    const Config config = repository.load();

    QVERIFY(repository.lastLoadError().isEmpty());
    QVERIFY(!config.routeOnly);
    QCOMPARE(config.localPort, 10808);
    QCOMPARE(config.dns().directDns, QStringLiteral("https://dns.alidns.com/dns-query"));
    QCOMPARE(config.dns().remoteDns, QStringLiteral("https://cloudflare-dns.com/dns-query"));
    QCOMPARE(config.dns().bootstrapDns, QStringLiteral("223.5.5.5"));
    QVERIFY(config.dns().enableCacheFile4Sbox);
    QVERIFY(!config.ui().mainProxyEnabled);
    QCOMPARE(config.tun().tunModeItem.icmpRouting, QStringLiteral("rule"));
    QVERIFY(!config.tun().tunModeItem.enableIPv6Address);
    QVERIFY(config.policy().coreTypeItems.size() > 0);
    QVERIFY(config.collection().routingItems.size() >= 3);
    QVERIFY(config.collection().servers.isEmpty());
    QVERIFY(config.collection().subscriptions.isEmpty());
}

void JsonConfigRepositoryTests::loadReadsCanonicalSongBirdStructure()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = makeConfigPath(tempDir);
    QVERIFY(writeJsonFile(configPath, makeCanonicalRoot()));

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QVERIFY(repository.lastLoadError().isEmpty());
    QVERIFY(config.logEnabled);
    QCOMPARE(config.logLevel, QStringLiteral("warning"));
    QCOMPARE(config.currentIndexId, QStringLiteral("server-1"));
    QCOMPARE(config.localPort, 10809);
    QCOMPARE(config.localProtocol, QStringLiteral("http"));
    QVERIFY(!config.udpEnabled);
    QVERIFY(!config.sniffingEnabled);
    QVERIFY(config.routeOnly);
    QVERIFY(config.allowLanConnection);
    QCOMPARE(config.inboundUser, QStringLiteral("tester"));
    QCOMPARE(config.inboundPassword, QStringLiteral("secret"));
    QCOMPARE(config.defaults().speedPingTestUrl, QStringLiteral("https://probe.example/test"));
    QCOMPARE(config.defaults().defIeProxyExceptions, QStringLiteral("localhost;127.*"));
    QVERIFY(config.muxEnabled);
    QCOMPARE(config.mux4SboxProtocol, QStringLiteral("smux"));
    QCOMPARE(config.mux4SboxMaxConnections, 16);
    QVERIFY(config.mux4SboxPadding.has_value());
    QVERIFY(config.mux4SboxPadding.value());
    QCOMPARE(config.sysProxyType, 2);
    QVERIFY(config.dns().enableFragment);
    QVERIFY(!config.dns().enableCacheFile4Sbox);
    QCOMPARE(config.dns().defaultFingerprint, QStringLiteral("chrome"));
    QCOMPARE(config.dns().defaultUserAgent, QStringLiteral("Mozilla/5.0"));
    QCOMPARE(config.dns().directDns, QStringLiteral("223.5.5.5"));
    QCOMPARE(config.dns().remoteDns, QStringLiteral("8.8.8.8"));
    QCOMPARE(config.dns().bootstrapDns, QStringLiteral("119.29.29.29"));
    QVERIFY(config.dns().fakeIp);
    QVERIFY(!config.dns().globalFakeIp);
    QVERIFY(config.dns().serveStale);
    QVERIFY(config.dns().parallelQuery);
    QCOMPARE(config.dns().directExpectedIps, QStringLiteral("geoip:cn"));
    QVERIFY(config.dns().useSystemHosts);
    QVERIFY(!config.dns().addCommonHosts);
    QVERIFY(!config.dns().blockBindingQuery);
    QCOMPARE(config.dns().domainStrategyForFreedom, QStringLiteral("UseIPv4"));
    QCOMPARE(config.dns().domainStrategyForProxy, QStringLiteral("UseIPv6"));
    QCOMPARE(config.dns().dnsHosts, QStringLiteral("example.com 1.2.3.4"));
    QVERIFY(config.dns().defaultAllowInsecure);
    QCOMPARE(config.dns().domainStrategy, QStringLiteral("prefer_ipv4"));
    QCOMPARE(config.dns().domainStrategy4Singbox, QStringLiteral("prefer_ipv6"));
    QCOMPARE(config.dns().domainMatcher, QStringLiteral("mph"));
    QVERIFY(config.ignoreGeoUpdateCore);
    QCOMPARE(config.systemProxyAdvancedProtocol, QStringLiteral("http"));
    QVERIFY(config.checkPreReleaseUpdate);
    QVERIFY(!config.ui().showMainOnStartup);
    QVERIFY(config.ui().autoRunEnabled);
    QCOMPARE(config.ui().languageCode, QStringLiteral("en-US"));
    QCOMPARE(config.ui().themeName, QStringLiteral("Dark"));
    QCOMPARE(config.ui().mainLocationX, 10);
    QCOMPARE(config.ui().mainLocationY, 20);
    QCOMPARE(config.ui().mainSizeWidth, 1280);
    QCOMPARE(config.ui().mainSizeHeight, 720);
    QCOMPARE(config.ui().mainSelectedSubId, QStringLiteral("sub-1"));
    QCOMPARE(config.ui().settingsRoutingRuleTabKey, QStringLiteral("basic"));
    QCOMPARE(config.ui().mainServerLogSplitPercent, 55);
    QCOMPARE(config.ui().mainServerQrSplitPercent, 80);
    QVERIFY(config.ui().mainQrPreviewVisible);
    QVERIFY(config.ui().mainProxyEnabled);
    QCOMPARE(config.ui().mainColumnWidths.value(QStringLiteral("address")), 240);
    QVERIFY(config.tun().tunModeItem.enableTun);
    QVERIFY(!config.tun().tunModeItem.autoRoute);
    QVERIFY(!config.tun().tunModeItem.strictRoute);
    QCOMPARE(config.tun().tunModeItem.stack, QStringLiteral("mixed"));
    QCOMPARE(config.tun().tunModeItem.mtu, 1400);
    QVERIFY(config.tun().tunModeItem.enableIPv6Address);
    QCOMPARE(config.tun().tunModeItem.icmpRouting, QStringLiteral("direct"));
    QVERIFY(config.tun().tunModeItem.enableLegacyProtect);
    QCOMPARE(config.collection().servers.size(), 1);
    QCOMPARE(config.collection().servers.constFirst().configType, ConfigType::Trojan);
    QCOMPARE(config.collection().servers.constFirst().coreType, CoreType::SingBox);
    QCOMPARE(config.collection().servers.constFirst().subId, QStringLiteral("sub-1"));
    QVERIFY(config.collection().servers.constFirst().muxEnabled.has_value());
    QVERIFY(!config.collection().servers.constFirst().muxEnabled.value());
    QCOMPARE(config.collection().servers.constFirst().finalmask, QStringLiteral("{\"udp\":[{\"type\":\"mkcp-original\"}]}"));
    QCOMPARE(config.collection().servers.constFirst().cert, QStringLiteral("-----BEGIN CERTIFICATE-----"));
    QCOMPARE(config.collection().servers.constFirst().certSha, QStringLiteral("sha256/example"));
    QCOMPARE(config.collection().servers.constFirst().mldsa65Verify, QStringLiteral("mldsa-key"));
    QCOMPARE(config.collection().servers.constFirst().echConfigList, QStringLiteral("ECHCONFIGBASE64"));
    QCOMPARE(config.collection().servers.constFirst().echForceQuery, QStringLiteral("half"));
    QCOMPARE(config.collection().subscriptions.size(), 1);
    QCOMPARE(config.collection().subscriptions.constFirst().remarks, QStringLiteral("feed"));
    QCOMPARE(config.collection().routingIndex, 1);
    QVERIFY(config.collection().enableRoutingAdvanced);
    QVERIFY(config.collection().routingItems.size() >= 1);
    QCOMPARE(config.collection().routingItems.constFirst().domainStrategy4Singbox, QStringLiteral("prefer_ipv4"));
    QCOMPARE(config.collection().routingItems.constFirst().rules.constFirst().network, QStringLiteral("tcp,udp"));
    const QStringList expectedRuleProcess{QStringLiteral("chrome.exe")};
    QCOMPARE(config.collection().routingItems.constFirst().rules.constFirst().process, expectedRuleProcess);
    QCOMPARE(config.collection().routingCustomRules.size(), 1);
    QCOMPARE(config.collection().routingCustomRules.constFirst().network, QStringLiteral("tcp,udp"));
    QCOMPARE(config.collection().routingCustomRules.constFirst().process, expectedRuleProcess);
    QCOMPARE(config.policy().policyGroups.size(), 1);
    QCOMPARE(config.policy().policyGroups.constFirst().name, QStringLiteral("auto"));
    QCOMPARE(config.policy().policyGroups.constFirst().strategy, PolicyGroupItem::Strategy::UrlTest);
    const QStringList expectedMemberServerIds{
        QStringLiteral("server-1"),
        QStringLiteral("server-2")
    };
    QCOMPARE(config.policy().policyGroups.constFirst().memberServerIds, expectedMemberServerIds);
    QCOMPARE(config.policy().coreTypeItems.size(), 1);
    QCOMPARE(config.policy().coreTypeItems.constFirst().configType, static_cast<int>(ConfigType::Trojan));
    QCOMPARE(config.policy().coreTypeItems.constFirst().coreType, static_cast<int>(CoreType::SingBox));
}

void JsonConfigRepositoryTests::loadTreatsMissingServerConfigTypeAsVmess()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = makeConfigPath(tempDir);

    QJsonObject server;
    server.insert(QStringLiteral("indexId"), QStringLiteral("vmess-server"));
    server.insert(QStringLiteral("address"), QStringLiteral("159.13.48.30"));
    server.insert(QStringLiteral("port"), 13668);
    server.insert(QStringLiteral("id"), QStringLiteral("7011399b-5745-4395-82c3-cfafe486e863"));

    QJsonObject root;
    root.insert(QStringLiteral("servers"), QJsonArray{server});
    QVERIFY(writeJsonFile(configPath, root));

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.collection().servers.size(), 1);
    QCOMPARE(config.collection().servers.constFirst().configType, ConfigType::VMess);
}

void JsonConfigRepositoryTests::loadMergesStateFileWithoutBlockingOnMissingOrInvalidState()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = makeConfigPath(tempDir);
    QVERIFY(writeJsonFile(configPath, makeCanonicalRoot()));

    QJsonObject stateUi;
    stateUi.insert(QStringLiteral("mainLocationX"), 99);
    stateUi.insert(QStringLiteral("mainSelectedSubscriptionId"), QStringLiteral("override-sub"));
    stateUi.insert(QStringLiteral("settingsRoutingRuleTabKey"), QStringLiteral("override-tab"));
    stateUi.insert(QStringLiteral("mainProxyEnabled"), true);

    QJsonObject serverState;
    serverState.insert(QStringLiteral("indexId"), QStringLiteral("server-1"));
    serverState.insert(QStringLiteral("testResult"), QStringLiteral("42 ms"));

    QJsonObject stateRoot;
    stateRoot.insert(QStringLiteral("ui"), stateUi);
    stateRoot.insert(QStringLiteral("serverStates"), QJsonArray{serverState});
    QVERIFY(writeJsonFile(makeStatePath(tempDir), stateRoot));

    JsonConfigRepository repository(configPath);
    Config config = repository.load();

    QVERIFY(repository.lastLoadError().isEmpty());
    QCOMPARE(config.ui().mainLocationX, 99);
    QCOMPARE(config.ui().mainSelectedSubId, QStringLiteral("override-sub"));
    QCOMPARE(config.ui().settingsRoutingRuleTabKey, QStringLiteral("override-tab"));
    QVERIFY(config.ui().mainProxyEnabled);
    QCOMPARE(config.collection().servers.constFirst().testResult, QStringLiteral("42 ms"));

    QFile invalidState(makeStatePath(tempDir));
    QVERIFY(invalidState.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    QVERIFY(invalidState.write("{invalid-json") >= 0);
    invalidState.close();

    config = repository.load();
    QVERIFY(repository.lastLoadError().isEmpty());
    QCOMPARE(config.currentIndexId, QStringLiteral("server-1"));
    QVERIFY(config.collection().servers.size() == 1);
}

void JsonConfigRepositoryTests::loadIgnoresLegacyOnlyFields()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = makeConfigPath(tempDir);

    QJsonObject uiItem;
    uiItem.insert(QStringLiteral("mainProxyEnabled"), true);

    QJsonObject server;
    server.insert(QStringLiteral("indexId"), QStringLiteral("legacy-server"));
    server.insert(QStringLiteral("configType"), 1);
    server.insert(QStringLiteral("address"), QStringLiteral("9.9.9.9"));
    server.insert(QStringLiteral("port"), 443);
    server.insert(QStringLiteral("subid"), QStringLiteral("legacy-sub"));

    QJsonObject root;
    root.insert(QStringLiteral("defUserAgent"), QStringLiteral("LegacyAgent"));
    root.insert(QStringLiteral("domainStrategy4Singbox"), QStringLiteral("prefer_ipv4"));
    root.insert(QStringLiteral("uiItem"), uiItem);
    root.insert(QStringLiteral("vmess"), QJsonArray{server});
    root.insert(QStringLiteral("subItem"), QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("legacy-sub")}}});
    root.insert(QStringLiteral("routings"), QJsonArray{QJsonObject{{QStringLiteral("remarks"), QStringLiteral("legacy-route")}}});
    QVERIFY(writeJsonFile(configPath, root));

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QVERIFY(repository.lastLoadError().isEmpty());
    QVERIFY(config.dns().defaultUserAgent.isEmpty());
    QVERIFY(config.dns().domainStrategy4Singbox.isEmpty());
    QVERIFY(!config.ui().mainProxyEnabled);
    QVERIFY(config.collection().servers.isEmpty());
    QVERIFY(config.collection().subscriptions.isEmpty());
    QVERIFY(config.collection().routingItems.size() >= 3);
}

void JsonConfigRepositoryTests::saveWritesCanonicalSongBirdStructure()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = makeConfigPath(tempDir);
    JsonConfigRepository repository(configPath);

    Config config;
    config.logEnabled = true;
    config.logLevel = QStringLiteral("debug");
    config.currentIndexId = QStringLiteral("server-1");
    config.localPort = 2080;
    config.localProtocol = QStringLiteral("http");
    config.udpEnabled = false;
    config.sniffingEnabled = false;
    config.routeOnly = true;
    config.allowLanConnection = true;
    config.inboundUser = QStringLiteral("alice");
    config.inboundPassword = QStringLiteral("pw");
    config.defaults().speedPingTestUrl = QStringLiteral("https://probe.example/test");
    config.defaults().defIeProxyExceptions = QStringLiteral("localhost;127.*");
    config.muxEnabled = true;
    config.mux4SboxProtocol = QStringLiteral("smux");
    config.mux4SboxMaxConnections = 32;
    config.mux4SboxPadding = true;
    config.sysProxyType = 1;
    config.dns().enableFragment = true;
    config.dns().enableCacheFile4Sbox = false;
    config.dns().defaultFingerprint = QStringLiteral("firefox");
    config.dns().defaultUserAgent = QStringLiteral("Agent/2.0");
    config.dns().directDns = QStringLiteral("1.1.1.1");
    config.dns().remoteDns = QStringLiteral("8.8.4.4");
    config.dns().bootstrapDns = QStringLiteral("9.9.9.9");
    config.dns().fakeIp = true;
    config.dns().globalFakeIp = false;
    config.dns().serveStale = true;
    config.dns().parallelQuery = true;
    config.dns().directExpectedIps = QStringLiteral("geoip:private");
    config.dns().useSystemHosts = true;
    config.dns().addCommonHosts = false;
    config.dns().blockBindingQuery = false;
    config.dns().domainStrategyForFreedom = QStringLiteral("UseIPv4");
    config.dns().domainStrategyForProxy = QStringLiteral("UseIPv6");
    config.dns().dnsHosts = QStringLiteral("example.com 127.0.0.1");
    config.dns().defaultAllowInsecure = true;
    config.dns().domainStrategy = QStringLiteral("ipv4_only");
    config.dns().domainStrategy4Singbox = QStringLiteral("prefer_ipv4");
    config.dns().domainMatcher = QStringLiteral("hybrid");
    config.ignoreGeoUpdateCore = true;
    config.systemProxyAdvancedProtocol = QStringLiteral("socks");
    config.checkPreReleaseUpdate = true;
    config.ui().showMainOnStartup = false;
    config.ui().autoRunEnabled = true;
    config.ui().languageCode = QStringLiteral("zh-CN");
    config.ui().themeName = QStringLiteral("Dark");
    config.ui().mainLocationX = 100;
    config.ui().mainLocationY = 200;
    config.ui().mainSizeWidth = 1440;
    config.ui().mainSizeHeight = 900;
    config.ui().mainSelectedSubId = QStringLiteral("sub-1");
    config.ui().settingsRoutingRuleTabKey = QStringLiteral("advanced");
    config.ui().mainServerLogSplitPercent = 45;
    config.ui().mainServerQrSplitPercent = 75;
    config.ui().mainQrPreviewVisible = true;
    config.ui().mainProxyEnabled = true;
    config.ui().mainColumnWidths.insert(QStringLiteral("address"), 260);
    config.tun().tunModeItem.enableTun = true;
    config.tun().tunModeItem.autoRoute = false;
    config.tun().tunModeItem.strictRoute = false;
    config.tun().tunModeItem.stack = QStringLiteral("gvisor");
    config.tun().tunModeItem.mtu = 1500;
    config.tun().tunModeItem.enableIPv6Address = true;
    config.tun().tunModeItem.icmpRouting = QStringLiteral("direct");
    config.tun().tunModeItem.enableLegacyProtect = true;
    config.collection().servers = {makeServer()};
    config.collection().servers[0].testResult = QStringLiteral("123 ms");
    config.collection().subscriptions = {makeSubscription()};
    config.collection().routingIndex = 2;
    config.collection().enableRoutingAdvanced = true;
    config.collection().routingItems = {makeRoutingItem()};
    config.collection().routingCustomRules = {makeRoutingRule()};
    config.policy().policyGroups = {makePolicyGroup()};
    config.policy().coreTypeItems = {makeCoreTypeItem()};

    QVERIFY(repository.save(config));

    const QJsonObject savedRoot = readJsonFile(configPath);
    const QJsonObject savedState = readJsonFile(makeStatePath(tempDir));
    QVERIFY(!savedRoot.isEmpty());
    QVERIFY(!savedState.isEmpty());
    QCOMPARE(savedRoot.value(QStringLiteral("schemaVersion")).toInt(), 7);
    QVERIFY(savedRoot.contains(QStringLiteral("defaults")));
    QVERIFY(savedRoot.contains(QStringLiteral("ui")));
    QVERIFY(savedRoot.contains(QStringLiteral("tunModeItem")));
    QVERIFY(savedRoot.contains(QStringLiteral("servers")));
    QVERIFY(savedRoot.contains(QStringLiteral("subscriptions")));
    QVERIFY(savedRoot.contains(QStringLiteral("routingItems")));
    QVERIFY(savedRoot.contains(QStringLiteral("routingCustomRules")));
    QVERIFY(savedRoot.contains(QStringLiteral("policyGroups")));
    QVERIFY(savedRoot.contains(QStringLiteral("coreTypeItems")));
    QVERIFY(!savedRoot.contains(QStringLiteral("uiItem")));
    QVERIFY(!savedRoot.contains(QStringLiteral("constItem")));
    QVERIFY(!savedRoot.contains(QStringLiteral("vmess")));
    QVERIFY(!savedRoot.contains(QStringLiteral("subItem")));
    QVERIFY(!savedRoot.contains(QStringLiteral("routings")));
    QVERIFY(!savedRoot.contains(QStringLiteral("loglevel")));
    QVERIFY(!savedRoot.contains(QStringLiteral("defUserAgent")));
    QVERIFY(!savedRoot.contains(QStringLiteral("defFingerprint")));
    QVERIFY(!savedRoot.contains(QStringLiteral("directDNS")));
    QVERIFY(!savedRoot.contains(QStringLiteral("remoteDNS")));
    QVERIFY(!savedRoot.contains(QStringLiteral("bootstrapDNS")));
    QVERIFY(!savedRoot.contains(QStringLiteral("fakeIP")));
    QVERIFY(!savedRoot.contains(QStringLiteral("enableParallelQuery")));
    QVERIFY(!savedRoot.contains(QStringLiteral("directExpectedIPs")));
    QVERIFY(!savedRoot.contains(QStringLiteral("domainStrategy4Freedom")));
    QVERIFY(!savedRoot.contains(QStringLiteral("domainStrategy4Proxy")));
    QVERIFY(!savedRoot.contains(QStringLiteral("hosts")));
    QVERIFY(!savedRoot.contains(QStringLiteral("defAllowInsecure")));
    QVERIFY(!savedRoot.contains(QStringLiteral("domainStrategy4Singbox")));
    QVERIFY(!savedRoot.contains(QStringLiteral("languageCode")));

    const QJsonObject savedUi = savedRoot.value(QStringLiteral("ui")).toObject();
    QCOMPARE(savedUi.value(QStringLiteral("languageCode")).toString(), QStringLiteral("zh-CN"));
    QCOMPARE(savedUi.value(QStringLiteral("themeName")).toString(), QStringLiteral("Dark"));
    QVERIFY(!savedUi.contains(QStringLiteral("settingsRoutingRuleTabKey")));
    QVERIFY(!savedUi.contains(QStringLiteral("mainSelectedSubscriptionId")));
    QVERIFY(!savedUi.contains(QStringLiteral("mainProxyEnabled")));
    QVERIFY(!savedUi.contains(QStringLiteral("mainColumnWidths")));

    const QJsonObject savedStateUi = savedState.value(QStringLiteral("ui")).toObject();
    QCOMPARE(savedStateUi.value(QStringLiteral("mainSelectedSubscriptionId")).toString(), QStringLiteral("sub-1"));
    QCOMPARE(savedStateUi.value(QStringLiteral("settingsRoutingRuleTabKey")).toString(), QStringLiteral("advanced"));
    QCOMPARE(savedStateUi.value(QStringLiteral("mainColumnWidths")).toObject().value(QStringLiteral("address")).toInt(), 260);
    QCOMPARE(savedStateUi.value(QStringLiteral("mainLocationX")).toInt(), 100);
    QCOMPARE(savedStateUi.value(QStringLiteral("mainSizeWidth")).toInt(), 1440);
    QVERIFY(savedStateUi.value(QStringLiteral("mainProxyEnabled")).toBool());
    QVERIFY(!savedStateUi.contains(QStringLiteral("themeName")));

    const QJsonObject savedDefaults = savedRoot.value(QStringLiteral("defaults")).toObject();
    QCOMPARE(savedDefaults.value(QStringLiteral("ieProxyExceptions")).toString(), QStringLiteral("localhost;127.*"));

    const QJsonObject savedServer = savedRoot.value(QStringLiteral("servers")).toArray().at(0).toObject();
    QCOMPARE(savedServer.value(QStringLiteral("configType")).toInt(), static_cast<int>(ConfigType::Trojan));
    QVERIFY(!savedServer.contains(QStringLiteral("coreType")));
    QCOMPARE(savedServer.value(QStringLiteral("subscriptionId")).toString(), QStringLiteral("sub-1"));
    QVERIFY(!savedServer.contains(QStringLiteral("sort")));
    QVERIFY(!savedServer.contains(QStringLiteral("testResult")));

    const QJsonObject savedServerState = savedState.value(QStringLiteral("serverStates")).toArray().at(0).toObject();
    QCOMPARE(savedServerState.value(QStringLiteral("indexId")).toString(), QStringLiteral("server-1"));
    QCOMPARE(savedServerState.value(QStringLiteral("testResult")).toString(), QStringLiteral("123 ms"));

    const QJsonObject savedRouting = savedRoot.value(QStringLiteral("routingItems")).toArray().at(0).toObject();
    QCOMPARE(savedRouting.value(QStringLiteral("domainStrategyForSingbox")).toString(), QStringLiteral("prefer_ipv4"));
    QCOMPARE(savedRouting.value(QStringLiteral("rules")).toArray().at(0).toObject().value(QStringLiteral("network")).toString(), QStringLiteral("tcp,udp"));
    QCOMPARE(savedRouting.value(QStringLiteral("rules")).toArray().at(0).toObject().value(QStringLiteral("process")).toArray().at(0).toString(), QStringLiteral("chrome.exe"));

    const Config reloaded = repository.load();
    QVERIFY(reloaded.routeOnly);
    QCOMPARE(reloaded.ui().mainSelectedSubId, QStringLiteral("sub-1"));
    QCOMPARE(reloaded.collection().servers.size(), 1);
    QCOMPARE(reloaded.collection().servers.constFirst().subId, QStringLiteral("sub-1"));
    QCOMPARE(reloaded.collection().routingItems.constFirst().domainStrategy4Singbox, QStringLiteral("prefer_ipv4"));
    QCOMPARE(reloaded.policy().policyGroups.size(), 1);
}

void JsonConfigRepositoryTests::saveRemovesEmptyStateFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    JsonConfigRepository repository(makeConfigPath(tempDir));
    QJsonObject staleState;
    staleState.insert(QStringLiteral("serverStates"), QJsonArray{
        QJsonObject{
            {QStringLiteral("indexId"), QStringLiteral("server-1")},
            {QStringLiteral("testResult"), QStringLiteral("999 ms")}
        }
    });
    QVERIFY(writeJsonFile(makeStatePath(tempDir), staleState));
    QVERIFY(QFileInfo::exists(makeStatePath(tempDir)));

    Config config;
    config.collection().servers = {makeServer()};
    config.collection().servers[0].testResult.clear();

    QVERIFY(repository.save(config));
    QVERIFY(!QFileInfo::exists(makeStatePath(tempDir)));
}

void JsonConfigRepositoryTests::saveReplacesExistingRootInsteadOfMerging()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = makeConfigPath(tempDir);

    QJsonObject legacyRoot;
    legacyRoot.insert(QStringLiteral("customPreservedField"), QStringLiteral("keep-me"));
    legacyRoot.insert(QStringLiteral("uiItem"), QJsonObject{{QStringLiteral("mainProxyEnabled"), true}});
    QVERIFY(writeJsonFile(configPath, legacyRoot));

    JsonConfigRepository repository(configPath);
    Config config;
    config.ui().languageCode = QStringLiteral("en-US");
    config.ui().mainProxyEnabled = true;
    QVERIFY(repository.save(config));

    const QJsonObject savedRoot = readJsonFile(configPath);
    QVERIFY(!savedRoot.isEmpty());
    QVERIFY(!savedRoot.contains(QStringLiteral("customPreservedField")));
    QVERIFY(!savedRoot.contains(QStringLiteral("uiItem")));
    QCOMPARE(savedRoot.value(QStringLiteral("ui")).toObject().value(QStringLiteral("languageCode")).toString(), QStringLiteral("en-US"));
    QVERIFY(!savedRoot.value(QStringLiteral("ui")).toObject().contains(QStringLiteral("mainProxyEnabled")));

    const QJsonObject savedState = readJsonFile(makeStatePath(tempDir));
    QCOMPARE(savedState.value(QStringLiteral("ui")).toObject().value(QStringLiteral("mainProxyEnabled")).toBool(), true);
}

QTEST_MAIN(JsonConfigRepositoryTests)

#include "JsonConfigRepositoryTests.moc"

