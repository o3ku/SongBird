#include <QtTest>

#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>

#include "runtime/ClientConfigWriter.h"

class ClientConfigWriterTests : public QObject {
    Q_OBJECT

private slots:
    void validateServerRejectsUnsupportedSingBoxTransportForSocks();
    void validateServerRejectsUnsupportedSingBoxTransportForShadowsocks();
    void generateClientConfigsDoesNotEmitSingBoxSocksPasswordWithoutUsername();
    void generateClientConfigsBuildsSingBoxHttpOutbound();
    void generateClientConfigsDefaultsHttpAutoCoreToSingBox();
    void validateServerRejectsInvalidXhttpExtraJson();
    void validateServerRejectsInvalidFinalmaskJson();
    void generateClientConfigsSetsLegacySniffRouteOnlyWhenEnabled();
    void generateClientConfigsAddsLegacyFragmentOutboundWhenEnabled();
    void generateClientConfigsUsesDefaultAllowInsecureForLegacyTlsWhenServerValueMissing();
    void generateClientConfigsUsesDefaultUserAgentForLegacyWebSocketWhenServerValueMissing();
    void generateClientConfigsUsesDefaultUserAgentForLegacyHttpupgradeWhenServerValueMissing();
    void generateClientConfigsUsesServerUserAgentBeforeDefaultForLegacyHttpupgrade();
    void generateClientConfigsUsesDefaultUserAgentForLegacyTcpHttpWhenServerValueMissing();
    void generateClientConfigsUsesDefaultFingerprintForLegacyTlsWhenServerValueMissing();
    void generateClientConfigsAppliesLegacyFinalmaskOverride();
    void generateClientConfigsUsesLegacyTlsCertificates();
    void generateClientConfigsUsesLegacyPinnedPeerCertSha256();
    void generateClientConfigsBuildsLegacySimpleDnsServersAndHosts();
    void generateClientConfigsUsesLegacyDirectDnsAsFinalServerForCatchAllDirectRouting();
    void generateClientConfigsAddsCommonHostsToLegacyDnsHosts();
    void generateClientConfigsAddsSystemHostsToLegacyDnsHostsWhenEnabled();
    void generateClientConfigsAddsLegacyServeStaleAndParallelQueryWhenEnabled();
    void generateClientConfigsUsesLegacyRealityMldsa65Verify();
    void generateClientConfigsUsesLegacyTlsEchFields();
    void generateClientConfigsUsesDefaultAllowInsecureForSingBoxTlsWhenServerValueMissing();
    void generateClientConfigsUsesSingBoxTlsCertificates();
    void generateClientConfigsUsesSingBoxPinnedCertificatePublicKeyHashes();
    void generateClientConfigsUsesDefaultFingerprintForSingBoxTlsWhenServerValueMissing();
    void generateClientConfigsForcesSingBoxRealityTlsInsecureFalse();
    void generateClientConfigsUsesDefaultFingerprintForLegacyRealityWhenServerValueMissing();
    void generateClientConfigsAddsDefaultSingBoxWebSocketEarlyDataHeader();
    void generateClientConfigsOverridesSingBoxWebSocketEarlyDataHeaderFromEh();
    void generateClientConfigsUsesTcpNetworkForSingBoxWebSocketTransport();
    void generateClientConfigsCreatesTunCompatSingBoxRelayForXray();
    void generateClientConfigsCreatesTunCompatSingBoxRelayForXrayWithoutLegacyProtect();
    void generateClientConfigsUsesModernLocalDnsServerForTunCompatSingBoxRelay();
    void generateClientConfigsMovesTunCompatSniffToRouteAction();
    void generateClientConfigsAddsTunCompatRejectRules();
    void generateClientConfigsSplitsTunCompatDnsAndDirectProcessRules();
    void generateClientConfigsAddsTunRoutePackForNativeSingBoxCore();
    void generateClientConfigsUsesDirectIcmpRoutingForNativeSingBoxCore();
    void generateClientConfigsUsesRejectIcmpRoutingForNativeSingBoxCore();
    void generateClientConfigsIgnoresInvalidIcmpRoutingForNativeSingBoxCore();
    void generateClientConfigsDefaultsTunInboundToIpv4OnlyWhenIpv6AddressNotEnabled();
    void generateClientConfigsUsesServerMuxOverrideToDisableLegacyMux();
    void generateClientConfigsUsesServerMuxOverrideToEnableLegacyMux();
    void generateClientConfigsUsesConfiguredSingBoxMultiplexProtocol();
    void generateClientConfigsUsesConfiguredSingBoxMultiplexMaxConnectionsAndPadding();
    void generateClientConfigsUsesServerMuxOverrideToDisableSingBoxMultiplex();
    void generateClientConfigsUsesServerMuxOverrideToEnableSingBoxMultiplex();
    void generateClientConfigsSkipsSingBoxMultiplexWhenConfiguredProtocolEmpty();
    void generateClientConfigsAddsSingBoxCacheFileExperimentalWhenEnabled();
    void generateClientConfigsBuildsSimpleDnsPackForSingBoxCore();
    void generateClientConfigsUsesDirectDnsAsFinalServerForCatchAllDirectRouting();
    void generateClientConfigsAddsSingBoxFakeDnsServerAndCacheStorageWhenEnabled();
    void generateClientConfigsSkipsSimpleDnsRouteResolverForCustomSingBoxDnsObject();
    void generateClientConfigsKeepsDirectDnsAheadOfNonGlobalFakeDnsFallback();
    void generateClientConfigsUsesLogicalFakeIpFilterWhenGlobalFakeIpEnabled();
    void generateClientConfigsAddsCommonHostsWithoutRouteResolveRuleForSingBoxCore();
    void generateClientConfigsAddsSystemHostsForSingBoxDnsAndResolveRulesWhenEnabled();
    void generateClientConfigsUsesHostsDnsResolverForSingBoxDnsServerHostnames();
    void generateClientConfigsAddsLegacyDirectExpectedIpsForMatchingDirectGeosite();
    void generateClientConfigsAddsPredefinedAnswerForNonFullHostRule();
    void generateClientConfigsAddsSniffAndDnsHijackRulesForNativeSingBoxCore();
    void generateClientConfigsSetsSingBoxSniffOverrideDestinationWhenRouteOnlyFalse();
    void generateClientConfigsOmitsSingBoxSniffOverrideDestinationWhenRouteOnlyTrue();
    void generateClientConfigsAddsUdpDnsHijackRuleWhenSniffingDisabledForNativeSingBoxCore();
    void generateClientConfigsAddsSingBoxClashModeRouteRules();
    void generateClientConfigsMapsSingBoxBlockRoutingRuleToRejectAction();
    void generateClientConfigsTreatsPlainSingBoxRoutingDomainsAsKeywords();
    void generateClientConfigsSplitsSingBoxRoutingPortsIntoPortAndRangeFields();
    void generateClientConfigsAddsSingBoxResolveRuleBeforeUserRulesForIpOnDemand();
    void generateClientConfigsAddsSingBoxResolveRuleAfterUserRulesForIpIfNonMatch();
    void generateClientConfigsReappliesSingBoxIpRulesAfterResolveForIpIfNonMatch();
    void generateClientConfigsAddsSingBoxDirectExpectedIpsForMatchingDirectGeosite();
    void generateClientConfigsCarriesLegacyRoutingNetworkIntoProcessRules();
    void generateClientConfigsSplitsSingBoxRoutingProcessNameAndPathRules();
    void generateClientConfigsMergesCustomRulesBeforeSelectedBaseRoute();
    void generateClientConfigsUsesCustomDirectDomainsForSingBoxDnsRules();
    void generateClientConfigsDoesNotCreateTunCompatRelayForSingBoxCore();
    void generateClientConfigsDoesNotCreateTunCompatRelayForProtocolConfiguredSingBoxCore();
    void generateClientConfigsBuildsSingBoxHysteria2Outbound();
    void generateClientConfigsOmitsSingBoxDnsAndDefaultResolverWhenDnsInputsAreEmpty();
    void generateClientConfigsDoesNotEmitSingBoxNetworkForAnyTls();
};

namespace {

Config baseConfig()
{
    Config config;
    config.localPort = 10808;
    config.sniffingEnabled = true;
    config.tunModeItem.enableTun = true;
    config.tunModeItem.enableLegacyProtect = true;
    return config;
}

void setProtocolCore(Config& config, ConfigType configType, CoreType coreType)
{
    config.coreTypeItems.append(CoreTypeItem{
        static_cast<int>(configType),
        static_cast<int>(coreType)});
}

Config legacyConfig(ConfigType configType = ConfigType::VMess)
{
    Config config = baseConfig();
    setProtocolCore(config, configType, CoreType::Xray);
    return config;
}

VmessItem baseServer()
{
    VmessItem item;
    item.coreType = CoreType::Xray;
    item.configType = ConfigType::VMess;
    item.address = QStringLiteral("example.com");
    item.port = 443;
    item.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    item.security = QStringLiteral("auto");
    item.network = QStringLiteral("tcp");
    item.remarks = QStringLiteral("test");
    return item;
}

QJsonObject findObjectByTag(const QJsonArray& array, const QString& tag)
{
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("tag")).toString() == tag) {
            return object;
        }
    }
    return {};
}

QJsonObject findRouteRuleByAction(const QJsonArray& rules, const QString& action)
{
    for (const QJsonValue& value : rules) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("action")).toString() == action) {
            return object;
        }
    }
    return {};
}

QJsonObject findRouteRuleByOutbound(const QJsonArray& rules, const QString& outbound)
{
    for (const QJsonValue& value : rules) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("outbound")).toString() == outbound) {
            return object;
        }
    }
    return {};
}

QJsonObject findDnsRuleByServer(const QJsonArray& rules, const QString& server)
{
    for (const QJsonValue& value : rules) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("server")).toString() == server) {
            return object;
        }
    }
    return {};
}

QString testPemChain()
{
    return QStringLiteral(
        "-----BEGIN CERTIFICATE-----\n"
        "CERT-ONE\n"
        "-----END CERTIFICATE-----\n"
        "-----BEGIN CERTIFICATE-----\n"
        "CERT-TWO\n"
        "-----END CERTIFICATE-----");
}

bool jsonArrayContainsString(const QJsonArray& array, const QString& value)
{
    for (const QJsonValue& item : array) {
        if (item.toString() == value) {
            return true;
        }
    }
    return false;
}

bool ruleHasNetwork(const QJsonObject& rule, const QString& network)
{
    return jsonArrayContainsString(rule.value(QStringLiteral("network")).toArray(), network);
}

RoutingItem createRoutingItem(const QList<RoutingRule>& rules)
{
    RoutingItem item;
    item.remarks = QStringLiteral("route");
    item.rules = rules;
    item.locked = true;
    return item;
}

RoutingRule createRoutingRule(
    const QString& outboundTag,
    const QStringList& domains = {},
    const QStringList& ips = {},
    const QStringList& protocols = {},
    const QString& port = {},
    const QString& network = {},
    const QStringList& process = {})
{
    RoutingRule rule;
    rule.type = QStringLiteral("field");
    rule.outboundTag = outboundTag;
    rule.domain = domains;
    rule.ip = ips;
    rule.protocol = protocols;
    rule.port = port;
    rule.network = network;
    rule.process = process;
    rule.enabled = true;
    return rule;
}

} // namespace

void ClientConfigWriterTests::validateServerRejectsUnsupportedSingBoxTransportForSocks()
{
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.configType = ConfigType::Socks;
    server.network = QStringLiteral("ws");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    ClientConfigWriter writer;
    const OperationResult result = writer.writeClientConfig(
        baseConfig(),
        server,
        tempDir.filePath(QStringLiteral("config.json")));

    QVERIFY2(!result.success, qPrintable(result.message));
    QVERIFY(result.message.contains(QStringLiteral("Socks")));
    QVERIFY(result.message.contains(QStringLiteral("ws")));
}

void ClientConfigWriterTests::validateServerRejectsUnsupportedSingBoxTransportForShadowsocks()
{
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.configType = ConfigType::Shadowsocks;
    server.id = QStringLiteral("password");
    server.network = QStringLiteral("grpc");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    ClientConfigWriter writer;
    const OperationResult result = writer.writeClientConfig(
        baseConfig(),
        server,
        tempDir.filePath(QStringLiteral("config.json")));

    QVERIFY2(!result.success, qPrintable(result.message));
    QVERIFY(result.message.contains(QStringLiteral("Shadowsocks")));
    QVERIFY(result.message.contains(QStringLiteral("grpc")));
}

void ClientConfigWriterTests::generateClientConfigsDoesNotEmitSingBoxSocksPasswordWithoutUsername()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.configType = ConfigType::Socks;
    server.address = QStringLiteral("192.168.100.117");
    server.port = 1080;
    server.id.clear();
    server.security = QStringLiteral("auto");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));

    QCOMPARE(proxyOutbound.value(QStringLiteral("type")).toString(), QStringLiteral("socks"));
    QVERIFY(!proxyOutbound.contains(QStringLiteral("username")));
    QVERIFY(!proxyOutbound.contains(QStringLiteral("password")));
}

void ClientConfigWriterTests::generateClientConfigsBuildsSingBoxHttpOutbound()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.defaultAllowInsecure = true;

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.configType = ConfigType::HTTP;
    server.id = QStringLiteral("http-user");
    server.security = QStringLiteral("http-pass");
    server.streamSecurity = QStringLiteral("tls");
    server.sni = QStringLiteral("proxy.example.com");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));

    QCOMPARE(proxyOutbound.value(QStringLiteral("type")).toString(), QStringLiteral("http"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("server")).toString(), QStringLiteral("example.com"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("server_port")).toInt(), 443);
    QCOMPARE(proxyOutbound.value(QStringLiteral("username")).toString(), QStringLiteral("http-user"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("password")).toString(), QStringLiteral("http-pass"));
    QVERIFY(!proxyOutbound.contains(QStringLiteral("network")));
    QVERIFY(!proxyOutbound.contains(QStringLiteral("transport")));

    const QJsonObject tls = proxyOutbound.value(QStringLiteral("tls")).toObject();
    QCOMPARE(tls.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(tls.value(QStringLiteral("server_name")).toString(), QStringLiteral("proxy.example.com"));
    QCOMPARE(tls.value(QStringLiteral("insecure")).toBool(), true);
}

void ClientConfigWriterTests::generateClientConfigsDefaultsHttpAutoCoreToSingBox()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;

    VmessItem server = baseServer();
    server.coreType = CoreType::Auto;
    server.configType = ConfigType::HTTP;
    server.id = QStringLiteral("http-user");
    server.security = QStringLiteral("http-pass");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));

    QCOMPARE(proxyOutbound.value(QStringLiteral("type")).toString(), QStringLiteral("http"));
    QVERIFY(generated.auxiliary.isEmpty());
}

void ClientConfigWriterTests::validateServerRejectsInvalidXhttpExtraJson()
{
    VmessItem server = baseServer();
    server.network = QStringLiteral("xhttp");
    server.extra = QStringLiteral("[1,2,3]");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    ClientConfigWriter writer;
    const OperationResult result = writer.writeClientConfig(
        baseConfig(),
        server,
        tempDir.filePath(QStringLiteral("config.json")));

    QVERIFY2(!result.success, qPrintable(result.message));
    QVERIFY(result.message.contains(QStringLiteral("XHTTP")));
}

void ClientConfigWriterTests::validateServerRejectsInvalidFinalmaskJson()
{
    VmessItem server = baseServer();
    server.finalmask = QStringLiteral("[1,2,3]");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    ClientConfigWriter writer;
    const OperationResult result = writer.writeClientConfig(
        baseConfig(),
        server,
        tempDir.filePath(QStringLiteral("config.json")));

    QVERIFY2(!result.success, qPrintable(result.message));
    QVERIFY(result.message.contains(QStringLiteral("Finalmask")));
}

void ClientConfigWriterTests::generateClientConfigsSetsLegacySniffRouteOnlyWhenEnabled()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.routeOnly = true;
    VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject inbound = generated.primary.root.value(QStringLiteral("inbounds")).toArray().at(0).toObject();
    const QJsonObject sniffing = inbound.value(QStringLiteral("sniffing")).toObject();

    QCOMPARE(sniffing.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(sniffing.value(QStringLiteral("routeOnly")).toBool(), true);
}

void ClientConfigWriterTests::generateClientConfigsAddsLegacyFragmentOutboundWhenEnabled()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.enableFragment = true;
    VmessItem server = baseServer();
    server.streamSecurity = QStringLiteral("tls");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject fragmentOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("frag-proxy"));

    QCOMPARE(proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                 .value(QStringLiteral("sockopt")).toObject()
                 .value(QStringLiteral("dialerProxy")).toString(),
             QStringLiteral("frag-proxy"));
    QCOMPARE(fragmentOutbound.value(QStringLiteral("protocol")).toString(), QStringLiteral("freedom"));
    const QJsonObject fragmentSettings = fragmentOutbound.value(QStringLiteral("settings")).toObject()
                                             .value(QStringLiteral("fragment")).toObject();
    QCOMPARE(fragmentSettings.value(QStringLiteral("packets")).toString(), QStringLiteral("tlshello"));
    QCOMPARE(fragmentSettings.value(QStringLiteral("length")).toString(), QStringLiteral("100-200"));
    QCOMPARE(fragmentSettings.value(QStringLiteral("interval")).toString(), QStringLiteral("10-20"));
}

void ClientConfigWriterTests::generateClientConfigsUsesDefaultAllowInsecureForLegacyTlsWhenServerValueMissing()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.defaultAllowInsecure = true;
    VmessItem server = baseServer();
    server.streamSecurity = QStringLiteral("tls");
    server.allowInsecure.clear();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject streamSettings = proxyOutbound.value(QStringLiteral("streamSettings")).toObject();
    const QJsonObject tlsSettings = streamSettings.value(QStringLiteral("tlsSettings")).toObject();

    QCOMPARE(tlsSettings.value(QStringLiteral("allowInsecure")).toBool(), true);
}

void ClientConfigWriterTests::generateClientConfigsUsesDefaultUserAgentForLegacyWebSocketWhenServerValueMissing()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.defaultUserAgent = QStringLiteral("Mozilla/5.0");
    VmessItem server = baseServer();
    server.network = QStringLiteral("ws");
    server.path = QStringLiteral("/ws");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject wsSettings = proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                                      .value(QStringLiteral("wsSettings"))
                                      .toObject();
    const QJsonObject headers = wsSettings.value(QStringLiteral("headers")).toObject();

    QCOMPARE(headers.value(QStringLiteral("User-Agent")).toString(), QStringLiteral("Mozilla/5.0"));
}

void ClientConfigWriterTests::generateClientConfigsUsesDefaultUserAgentForLegacyHttpupgradeWhenServerValueMissing()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.defaultUserAgent = QStringLiteral("Mozilla/5.0");
    VmessItem server = baseServer();
    server.network = QStringLiteral("httpupgrade");
    server.path = QStringLiteral("/upgrade");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject httpupgradeSettings = proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                                               .value(QStringLiteral("httpupgradeSettings"))
                                               .toObject();
    const QJsonObject headers = httpupgradeSettings.value(QStringLiteral("headers")).toObject();

    QCOMPARE(headers.value(QStringLiteral("User-Agent")).toString(), QStringLiteral("Mozilla/5.0"));
}

void ClientConfigWriterTests::generateClientConfigsUsesServerUserAgentBeforeDefaultForLegacyHttpupgrade()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.defaultUserAgent = QStringLiteral("Mozilla/5.0");
    VmessItem server = baseServer();
    server.network = QStringLiteral("httpupgrade");
    server.userAgent = QStringLiteral("CustomUA/1.0");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject httpupgradeSettings = proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                                               .value(QStringLiteral("httpupgradeSettings"))
                                               .toObject();
    const QJsonObject headers = httpupgradeSettings.value(QStringLiteral("headers")).toObject();

    QCOMPARE(headers.value(QStringLiteral("User-Agent")).toString(), QStringLiteral("CustomUA/1.0"));
}

void ClientConfigWriterTests::generateClientConfigsUsesDefaultUserAgentForLegacyTcpHttpWhenServerValueMissing()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.defaultUserAgent = QStringLiteral("Mozilla/5.0");
    VmessItem server = baseServer();
    server.network = QStringLiteral("tcp");
    server.headerType = QStringLiteral("http");
    server.requestHost = QStringLiteral("cdn.example.com");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject requestHeaders = proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                                         .value(QStringLiteral("tcpSettings")).toObject()
                                         .value(QStringLiteral("header")).toObject()
                                         .value(QStringLiteral("request")).toObject()
                                         .value(QStringLiteral("headers")).toObject();
    const QJsonArray userAgents = requestHeaders.value(QStringLiteral("User-Agent")).toArray();

    QCOMPARE(userAgents.size(), 1);
    QCOMPARE(userAgents.at(0).toString(), QStringLiteral("Mozilla/5.0"));
}

void ClientConfigWriterTests::generateClientConfigsUsesDefaultFingerprintForLegacyTlsWhenServerValueMissing()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.defaultFingerprint = QStringLiteral("firefox");
    VmessItem server = baseServer();
    server.streamSecurity = QStringLiteral("tls");
    server.fingerprint.clear();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject tlsSettings = proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                                      .value(QStringLiteral("tlsSettings"))
                                      .toObject();

    QCOMPARE(tlsSettings.value(QStringLiteral("fingerprint")).toString(), QStringLiteral("firefox"));
}

void ClientConfigWriterTests::generateClientConfigsAppliesLegacyFinalmaskOverride()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    VmessItem server = baseServer();
    server.network = QStringLiteral("kcp");
    server.headerType = QStringLiteral("wireguard");
    server.path = QStringLiteral("seed-value");
    server.finalmask =
        QStringLiteral("{\"udp\":[{\"type\":\"mkcp-aes128gcm\",\"settings\":{\"password\":\"override\"}}]}");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject finalmask = proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                                      .value(QStringLiteral("finalmask"))
                                      .toObject();
    const QJsonArray udp = finalmask.value(QStringLiteral("udp")).toArray();

    QCOMPARE(udp.size(), 1);
    QCOMPARE(udp.at(0).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("mkcp-aes128gcm"));
    QCOMPARE(
        udp.at(0).toObject().value(QStringLiteral("settings")).toObject().value(QStringLiteral("password")).toString(),
        QStringLiteral("override"));
}

void ClientConfigWriterTests::generateClientConfigsUsesLegacyTlsCertificates()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.defaultAllowInsecure = true;
    VmessItem server = baseServer();
    server.streamSecurity = QStringLiteral("tls");
    server.cert = testPemChain();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject tlsSettings = proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                                      .value(QStringLiteral("tlsSettings"))
                                      .toObject();
    const QJsonArray certificates = tlsSettings.value(QStringLiteral("certificates")).toArray();

    QCOMPARE(tlsSettings.value(QStringLiteral("disableSystemRoot")).toBool(), true);
    QCOMPARE(tlsSettings.value(QStringLiteral("allowInsecure")).toBool(), false);
    QVERIFY(tlsSettings.value(QStringLiteral("pinnedPeerCertSha256")).isUndefined());
    QCOMPARE(certificates.size(), 2);
    QCOMPARE(
        certificates.at(0).toObject().value(QStringLiteral("certificate")).toArray().at(1).toString(),
        QStringLiteral("CERT-ONE"));
    QCOMPARE(
        certificates.at(1).toObject().value(QStringLiteral("certificate")).toArray().at(1).toString(),
        QStringLiteral("CERT-TWO"));
}

void ClientConfigWriterTests::generateClientConfigsUsesLegacyPinnedPeerCertSha256()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.defaultAllowInsecure = true;
    VmessItem server = baseServer();
    server.streamSecurity = QStringLiteral("tls");
    server.certSha = QStringLiteral("sha256/base64-one,sha256/base64-two");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject tlsSettings = proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                                      .value(QStringLiteral("tlsSettings"))
                                      .toObject();

    QCOMPARE(
        tlsSettings.value(QStringLiteral("pinnedPeerCertSha256")).toString(),
        QStringLiteral("sha256/base64-one,sha256/base64-two"));
    QCOMPARE(tlsSettings.value(QStringLiteral("allowInsecure")).toBool(), false);
}

void ClientConfigWriterTests::generateClientConfigsBuildsLegacySimpleDnsServersAndHosts()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.directDns = QStringLiteral("223.5.5.5");
    config.remoteDns = QStringLiteral("https://dns.google/dns-query");
    config.bootstrapDns = QStringLiteral("119.29.29.29");
    config.domainStrategyForFreedom = QStringLiteral("UseIPv4");
    config.domainStrategyForProxy = QStringLiteral("UseIPv6");
    config.dnsHosts = QStringLiteral("example.com 1.2.3.4");
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(QStringLiteral("direct"), QStringList{QStringLiteral("full:cn.example.com")}),
            createRoutingRule(QStringLiteral("proxy"), QStringList{QStringLiteral("full:google.com")})})};

    VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject dns = generated.primary.root.value(QStringLiteral("dns")).toObject();
    QCOMPARE(dns.value(QStringLiteral("tag")).toString(), QStringLiteral("dns-module"));

    const QJsonArray servers = dns.value(QStringLiteral("servers")).toArray();
    bool foundDirectServer = false;
    bool foundRemoteServer = false;
    bool foundBootstrapServer = false;
    for (const QJsonValue& value : servers) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject serverObject = value.toObject();
        const QJsonArray domains = serverObject.value(QStringLiteral("domains")).toArray();
        if (serverObject.value(QStringLiteral("tag")).toString() == QStringLiteral("direct-dns-1")) {
            QCOMPARE(serverObject.value(QStringLiteral("address")).toString(), QStringLiteral("223.5.5.5"));
            QVERIFY(jsonArrayContainsString(domains, QStringLiteral("full:cn.example.com")));
            QCOMPARE(serverObject.value(QStringLiteral("skipFallback")).toBool(), true);
            foundDirectServer = true;
        }
        if (serverObject.value(QStringLiteral("address")).toString() == QStringLiteral("https://dns.google/dns-query")
            && jsonArrayContainsString(domains, QStringLiteral("full:google.com"))) {
            QCOMPARE(serverObject.value(QStringLiteral("skipFallback")).toBool(), true);
            foundRemoteServer = true;
        }
        if (serverObject.value(QStringLiteral("address")).toString() == QStringLiteral("119.29.29.29")
            && jsonArrayContainsString(domains, QStringLiteral("full:dns.google"))) {
            QCOMPARE(serverObject.value(QStringLiteral("skipFallback")).toBool(), true);
            foundBootstrapServer = true;
        }
    }
    QVERIFY(foundDirectServer);
    QVERIFY(foundRemoteServer);
    QVERIFY(foundBootstrapServer);

    const QJsonObject hosts = dns.value(QStringLiteral("hosts")).toObject();
    QCOMPARE(hosts.value(QStringLiteral("example.com")).toArray().at(0).toString(), QStringLiteral("1.2.3.4"));

    const QJsonArray routingRules = generated.primary.root.value(QStringLiteral("routing")).toObject().value(QStringLiteral("rules")).toArray();
    bool foundDirectDnsRoute = false;
    bool foundDnsFinalRoute = false;
    for (const QJsonValue& value : routingRules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("outboundTag")).toString() == QStringLiteral("direct")
            && jsonArrayContainsString(rule.value(QStringLiteral("inboundTag")).toArray(), QStringLiteral("direct-dns-1"))) {
            foundDirectDnsRoute = true;
        }
        if (rule.value(QStringLiteral("outboundTag")).toString() == QStringLiteral("proxy")
            && jsonArrayContainsString(rule.value(QStringLiteral("inboundTag")).toArray(), QStringLiteral("dns-module"))) {
            foundDnsFinalRoute = true;
        }
    }
    QVERIFY(foundDirectDnsRoute);
    QVERIFY(foundDnsFinalRoute);

    const QJsonObject outboundsDirect = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("direct"));
    QCOMPARE(
        outboundsDirect.value(QStringLiteral("settings")).toObject().value(QStringLiteral("domainStrategy")).toString(),
        QStringLiteral("UseIPv4"));

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("targetStrategy")).toString(), QStringLiteral("UseIPv6"));
}

void ClientConfigWriterTests::generateClientConfigsUsesLegacyDirectDnsAsFinalServerForCatchAllDirectRouting()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.directDns = QStringLiteral("223.5.5.5");
    config.remoteDns = QStringLiteral("8.8.8.8");
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(
                QStringLiteral("direct"),
                {},
                QStringList{QStringLiteral("0.0.0.0/0")},
                {},
                QStringLiteral("0-65535"),
                QStringLiteral("tcp,udp"))})};

    VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray servers = generated.primary.root.value(QStringLiteral("dns")).toObject().value(QStringLiteral("servers")).toArray();
    bool foundDirectFinalServer = false;
    for (const QJsonValue& value : servers) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject serverObject = value.toObject();
        if (serverObject.value(QStringLiteral("tag")).toString() == QStringLiteral("direct-dns-1")
            && serverObject.value(QStringLiteral("skipFallback")).toBool() == false) {
            foundDirectFinalServer = true;
            break;
        }
    }
    QVERIFY(foundDirectFinalServer);

    const QJsonArray routingRules = generated.primary.root.value(QStringLiteral("routing")).toObject().value(QStringLiteral("rules")).toArray();
    bool foundDnsFinalDirectRoute = false;
    for (const QJsonValue& value : routingRules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("outboundTag")).toString() == QStringLiteral("direct")
            && jsonArrayContainsString(rule.value(QStringLiteral("inboundTag")).toArray(), QStringLiteral("dns-module"))) {
            foundDnsFinalDirectRoute = true;
            break;
        }
    }
    QVERIFY(foundDnsFinalDirectRoute);
}

void ClientConfigWriterTests::generateClientConfigsAddsCommonHostsToLegacyDnsHosts()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.addCommonHosts = true;
    config.dnsHosts.clear();

    VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject hosts = generated.primary.root.value(QStringLiteral("dns")).toObject().value(QStringLiteral("hosts")).toObject();
    QVERIFY(hosts.contains(QStringLiteral("dns.google")));
}

void ClientConfigWriterTests::generateClientConfigsAddsSystemHostsToLegacyDnsHostsWhenEnabled()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString hostsPath = tempDir.filePath(QStringLiteral("hosts"));
    QFile hostsFile(hostsPath);
    QVERIFY(hostsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(hostsFile.write("1.2.3.4 sys.example.com\n") >= 0);
    hostsFile.close();

    QVERIFY(qputenv("V2RAYQ_SYSTEM_HOSTS_PATH", hostsPath.toUtf8()));

    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.addCommonHosts = false;
    config.useSystemHosts = true;
    config.dnsHosts.clear();

    VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    qunsetenv("V2RAYQ_SYSTEM_HOSTS_PATH");

    const QJsonObject hosts = generated.primary.root.value(QStringLiteral("dns")).toObject().value(QStringLiteral("hosts")).toObject();
    QCOMPARE(hosts.value(QStringLiteral("sys.example.com")).toArray().at(0).toString(), QStringLiteral("1.2.3.4"));
}

void ClientConfigWriterTests::generateClientConfigsAddsLegacyServeStaleAndParallelQueryWhenEnabled()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.serveStale = true;
    config.parallelQuery = true;

    VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject dns = generated.primary.root.value(QStringLiteral("dns")).toObject();
    QCOMPARE(dns.value(QStringLiteral("serveStale")).toBool(), true);
    QCOMPARE(dns.value(QStringLiteral("enableParallelQuery")).toBool(), true);
}

void ClientConfigWriterTests::generateClientConfigsUsesLegacyRealityMldsa65Verify()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    VmessItem server = baseServer();
    server.streamSecurity = QStringLiteral("reality");
    server.publicKey = QStringLiteral("reality-public-key");
    server.mldsa65Verify = QStringLiteral("mldsa65-public-key");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject realitySettings = proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                                          .value(QStringLiteral("realitySettings"))
                                          .toObject();

    QCOMPARE(realitySettings.value(QStringLiteral("mldsa65Verify")).toString(), QStringLiteral("mldsa65-public-key"));
}

void ClientConfigWriterTests::generateClientConfigsUsesLegacyTlsEchFields()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    VmessItem server = baseServer();
    server.streamSecurity = QStringLiteral("tls");
    server.echConfigList = QStringLiteral("ECHCONFIGBASE64");
    server.echForceQuery = QStringLiteral("half");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject tlsSettings = proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                                      .value(QStringLiteral("tlsSettings"))
                                      .toObject();

    QCOMPARE(tlsSettings.value(QStringLiteral("echConfigList")).toString(), QStringLiteral("ECHCONFIGBASE64"));
    QCOMPARE(tlsSettings.value(QStringLiteral("echForceQuery")).toString(), QStringLiteral("half"));
}

void ClientConfigWriterTests::generateClientConfigsUsesDefaultAllowInsecureForSingBoxTlsWhenServerValueMissing()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.defaultAllowInsecure = true;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.streamSecurity = QStringLiteral("tls");
    server.allowInsecure.clear();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject tls = proxyOutbound.value(QStringLiteral("tls")).toObject();

    QCOMPARE(tls.value(QStringLiteral("insecure")).toBool(), true);
}

void ClientConfigWriterTests::generateClientConfigsUsesSingBoxTlsCertificates()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.defaultAllowInsecure = true;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.streamSecurity = QStringLiteral("tls");
    server.cert = testPemChain();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject tls = proxyOutbound.value(QStringLiteral("tls")).toObject();
    const QJsonArray certificates = tls.value(QStringLiteral("certificate")).toArray();

    QCOMPARE(tls.value(QStringLiteral("insecure")).toBool(), false);
    QCOMPARE(certificates.size(), 2);
    QCOMPARE(
        certificates.at(0).toString(),
        QStringLiteral("-----BEGIN CERTIFICATE-----\nCERT-ONE\n-----END CERTIFICATE-----"));
    QCOMPARE(
        certificates.at(1).toString(),
        QStringLiteral("-----BEGIN CERTIFICATE-----\nCERT-TWO\n-----END CERTIFICATE-----"));
}

void ClientConfigWriterTests::generateClientConfigsUsesSingBoxPinnedCertificatePublicKeyHashes()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.defaultAllowInsecure = true;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.streamSecurity = QStringLiteral("tls");
    server.certSha = QStringLiteral("sha256/base64-one,sha256/base64-two");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject tls = proxyOutbound.value(QStringLiteral("tls")).toObject();
    const QJsonArray pinnedHashes = tls.value(QStringLiteral("certificate_public_key_sha256")).toArray();

    QCOMPARE(tls.value(QStringLiteral("insecure")).toBool(), false);
    QCOMPARE(pinnedHashes.size(), 2);
    QCOMPARE(pinnedHashes.at(0).toString(), QStringLiteral("base64-one"));
    QCOMPARE(pinnedHashes.at(1).toString(), QStringLiteral("base64-two"));
}

void ClientConfigWriterTests::generateClientConfigsUsesDefaultFingerprintForSingBoxTlsWhenServerValueMissing()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.defaultFingerprint = QStringLiteral("firefox");
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.streamSecurity = QStringLiteral("tls");
    server.fingerprint.clear();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject utls = proxyOutbound.value(QStringLiteral("tls")).toObject()
                                  .value(QStringLiteral("utls"))
                                  .toObject();

    QCOMPARE(utls.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(utls.value(QStringLiteral("fingerprint")).toString(), QStringLiteral("firefox"));
}

void ClientConfigWriterTests::generateClientConfigsForcesSingBoxRealityTlsInsecureFalse()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.streamSecurity = QStringLiteral("reality");
    server.allowInsecure = QStringLiteral("true");
    server.publicKey = QStringLiteral("abcdefghijklmnopqrstuvwxyzABCDEFG");
    server.shortId = QStringLiteral("0123456789abcdef");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject tls = proxyOutbound.value(QStringLiteral("tls")).toObject();

    QCOMPARE(tls.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(tls.value(QStringLiteral("insecure")).toBool(), false);
}

void ClientConfigWriterTests::generateClientConfigsUsesDefaultFingerprintForLegacyRealityWhenServerValueMissing()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.defaultFingerprint = QStringLiteral("firefox");
    VmessItem server = baseServer();
    server.streamSecurity = QStringLiteral("reality");
    server.fingerprint.clear();
    server.publicKey = QStringLiteral("abcdefghijklmnopqrstuvwxyzABCDEFG");
    server.shortId = QStringLiteral("0123456789abcdef");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject realitySettings = proxyOutbound.value(QStringLiteral("streamSettings")).toObject()
                                          .value(QStringLiteral("realitySettings"))
                                          .toObject();

    QCOMPARE(realitySettings.value(QStringLiteral("fingerprint")).toString(), QStringLiteral("firefox"));
}

void ClientConfigWriterTests::generateClientConfigsAddsDefaultSingBoxWebSocketEarlyDataHeader()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.network = QStringLiteral("ws");
    server.path = QStringLiteral("/ws?ed=2048");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject transport = proxyOutbound.value(QStringLiteral("transport")).toObject();

    QCOMPARE(transport.value(QStringLiteral("type")).toString(), QStringLiteral("ws"));
    QCOMPARE(transport.value(QStringLiteral("path")).toString(), QStringLiteral("/ws"));
    QCOMPARE(transport.value(QStringLiteral("max_early_data")).toInt(), 2048);
    QCOMPARE(
        transport.value(QStringLiteral("early_data_header_name")).toString(),
        QStringLiteral("Sec-WebSocket-Protocol"));
}

void ClientConfigWriterTests::generateClientConfigsOverridesSingBoxWebSocketEarlyDataHeaderFromEh()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.network = QStringLiteral("ws");
    server.path = QStringLiteral("/ws?eh=X-Ed-Header&ed=1024");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject transport = proxyOutbound.value(QStringLiteral("transport")).toObject();

    QCOMPARE(transport.value(QStringLiteral("path")).toString(), QStringLiteral("/ws?eh=X-Ed-Header"));
    QCOMPARE(transport.value(QStringLiteral("max_early_data")).toInt(), 1024);
    QCOMPARE(
        transport.value(QStringLiteral("early_data_header_name")).toString(),
        QStringLiteral("X-Ed-Header"));
}

void ClientConfigWriterTests::generateClientConfigsUsesTcpNetworkForSingBoxWebSocketTransport()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.network = QStringLiteral("ws");
    server.path = QStringLiteral("/ws");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));

    QCOMPARE(proxyOutbound.value(QStringLiteral("network")).toString(), QStringLiteral("tcp"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("transport")).toObject().value(QStringLiteral("type")).toString(), QStringLiteral("ws"));
}

void ClientConfigWriterTests::generateClientConfigsCreatesTunCompatSingBoxRelayForXray()
{
    const Config config = baseConfig();
    const VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    QCOMPARE(generated.auxiliary.size(), 1);

    const QJsonObject compatRoot = generated.auxiliary.constFirst().root;
    const QJsonArray inbounds = compatRoot.value(QStringLiteral("inbounds")).toArray();
    const QJsonArray outbounds = compatRoot.value(QStringLiteral("outbounds")).toArray();
    const QJsonObject tunInbound = findObjectByTag(inbounds, QStringLiteral("tun-in"));
    const QJsonObject proxyOutbound = findObjectByTag(outbounds, QStringLiteral("proxy"));

    QCOMPARE(tunInbound.value(QStringLiteral("type")).toString(), QStringLiteral("tun"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("type")).toString(), QStringLiteral("socks"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("server")).toString(), QStringLiteral("127.0.0.1"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("server_port")).toInt(), config.localPort);
}

void ClientConfigWriterTests::generateClientConfigsCreatesTunCompatSingBoxRelayForXrayWithoutLegacyProtect()
{
    Config config = baseConfig();
    config.tunModeItem.enableLegacyProtect = false;
    const VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    QCOMPARE(generated.auxiliary.size(), 1);

    const QJsonObject compatRoot = generated.auxiliary.constFirst().root;
    const QJsonArray inbounds = compatRoot.value(QStringLiteral("inbounds")).toArray();
    const QJsonArray outbounds = compatRoot.value(QStringLiteral("outbounds")).toArray();
    const QJsonObject tunInbound = findObjectByTag(inbounds, QStringLiteral("tun-in"));
    const QJsonObject proxyOutbound = findObjectByTag(outbounds, QStringLiteral("proxy"));

    QCOMPARE(tunInbound.value(QStringLiteral("type")).toString(), QStringLiteral("tun"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("type")).toString(), QStringLiteral("socks"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("server")).toString(), QStringLiteral("127.0.0.1"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("server_port")).toInt(), config.localPort);
}

void ClientConfigWriterTests::generateClientConfigsUsesModernLocalDnsServerForTunCompatSingBoxRelay()
{
    const Config config = baseConfig();
    const VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject compatRoot = generated.auxiliary.constFirst().root;
    const QJsonObject dns = compatRoot.value(QStringLiteral("dns")).toObject();
    const QJsonArray servers = dns.value(QStringLiteral("servers")).toArray();

    QCOMPARE(servers.size(), 1);

    const QJsonObject localServer = servers.at(0).toObject();
    QCOMPARE(localServer.value(QStringLiteral("tag")).toString(), QStringLiteral("local"));
    QCOMPARE(localServer.value(QStringLiteral("type")).toString(), QStringLiteral("local"));
    QVERIFY(!localServer.contains(QStringLiteral("address")));
    QCOMPARE(dns.value(QStringLiteral("final")).toString(), QStringLiteral("local"));
}

void ClientConfigWriterTests::generateClientConfigsMovesTunCompatSniffToRouteAction()
{
    const Config config = baseConfig();
    const VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject compatRoot = generated.auxiliary.constFirst().root;
    const QJsonObject tunInbound = findObjectByTag(
        compatRoot.value(QStringLiteral("inbounds")).toArray(),
        QStringLiteral("tun-in"));
    const QJsonArray rules = compatRoot.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    QVERIFY(!tunInbound.contains(QStringLiteral("sniff")));

    bool foundSniffRule = false;
    for (const QJsonValue& value : rules) {
        if (value.toObject().value(QStringLiteral("action")).toString() == QStringLiteral("sniff")) {
            foundSniffRule = true;
            break;
        }
    }

    QVERIFY(foundSniffRule);
}

void ClientConfigWriterTests::generateClientConfigsAddsTunCompatRejectRules()
{
    const Config config = baseConfig();
    const VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject compatRoot = generated.auxiliary.constFirst().root;
    const QJsonArray rules = compatRoot.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundUdpRejectRule = false;
    bool foundMulticastRejectRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() != QStringLiteral("reject")) {
            continue;
        }

        const QJsonArray ports = rule.value(QStringLiteral("port")).toArray();
        if (!ports.isEmpty()) {
            if (ports.size() == 5
                && ports.at(0).toInt() == 135
                && ports.at(1).toInt() == 137
                && ports.at(2).toInt() == 138
                && ports.at(3).toInt() == 139
                && ports.at(4).toInt() == 5353) {
                foundUdpRejectRule = true;
            }
        }

        const QJsonArray ipCidrs = rule.value(QStringLiteral("ip_cidr")).toArray();
        if (jsonArrayContainsString(ipCidrs, QStringLiteral("224.0.0.0/3"))
            && jsonArrayContainsString(ipCidrs, QStringLiteral("ff00::/8"))) {
            foundMulticastRejectRule = true;
        }
    }

    QVERIFY(foundUdpRejectRule);
    QVERIFY(foundMulticastRejectRule);
}

void ClientConfigWriterTests::generateClientConfigsSplitsTunCompatDnsAndDirectProcessRules()
{
    const Config config = baseConfig();
    const VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject compatRoot = generated.auxiliary.constFirst().root;
    const QJsonArray rules = compatRoot.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    const QJsonObject dnsHijackRule = findRouteRuleByAction(rules, QStringLiteral("hijack-dns"));
    const QJsonArray dnsProcessNames = dnsHijackRule.value(QStringLiteral("process_name")).toArray();
    QVERIFY(jsonArrayContainsString(dnsProcessNames, QStringLiteral("xray.exe")));
    QVERIFY(jsonArrayContainsString(dnsProcessNames, QStringLiteral("v2ray.exe")));
    QVERIFY(jsonArrayContainsString(dnsProcessNames, QStringLiteral("SagerNet.exe")));
    QVERIFY(jsonArrayContainsString(dnsProcessNames, QStringLiteral("hysteria.exe")));
    QVERIFY(jsonArrayContainsString(dnsProcessNames, QStringLiteral("naiveproxy.exe")));
    QVERIFY(jsonArrayContainsString(dnsProcessNames, QStringLiteral("clash.exe")));
    QVERIFY(jsonArrayContainsString(dnsProcessNames, QStringLiteral("mihomo.exe")));
    QVERIFY(!jsonArrayContainsString(dnsProcessNames, QStringLiteral("sing-box.exe")));
    QVERIFY(!jsonArrayContainsString(dnsProcessNames, QStringLiteral("sing-box-client.exe")));

    const QJsonObject directProcessRule = findRouteRuleByOutbound(rules, QStringLiteral("direct"));
    const QJsonArray directProcessNames = directProcessRule.value(QStringLiteral("process_name")).toArray();
    QVERIFY(jsonArrayContainsString(directProcessNames, QStringLiteral("v2rayq.exe")));
    QVERIFY(jsonArrayContainsString(directProcessNames, QStringLiteral("xray.exe")));
    QVERIFY(jsonArrayContainsString(directProcessNames, QStringLiteral("v2ray.exe")));
    QVERIFY(jsonArrayContainsString(directProcessNames, QStringLiteral("SagerNet.exe")));
    QVERIFY(jsonArrayContainsString(directProcessNames, QStringLiteral("hysteria.exe")));
    QVERIFY(jsonArrayContainsString(directProcessNames, QStringLiteral("naiveproxy.exe")));
    QVERIFY(jsonArrayContainsString(directProcessNames, QStringLiteral("clash.exe")));
    QVERIFY(jsonArrayContainsString(directProcessNames, QStringLiteral("mihomo.exe")));
    QVERIFY(jsonArrayContainsString(directProcessNames, QStringLiteral("sing-box-client.exe")));
    QVERIFY(jsonArrayContainsString(directProcessNames, QStringLiteral("sing-box.exe")));
}

void ClientConfigWriterTests::generateClientConfigsAddsTunRoutePackForNativeSingBoxCore()
{
    const Config config = baseConfig();
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject route = generated.primary.root.value(QStringLiteral("route")).toObject();
    const QJsonArray rules = route.value(QStringLiteral("rules")).toArray();

    QCOMPARE(route.value(QStringLiteral("auto_detect_interface")).toBool(), true);

    bool foundUdpRejectRule = false;
    bool foundDnsHijackRule = false;
    bool foundDirectProcessRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("reject")
            && rule.value(QStringLiteral("port")).toArray().size() == 5) {
            foundUdpRejectRule = true;
        }
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("hijack-dns")
            && jsonArrayContainsString(rule.value(QStringLiteral("process_name")).toArray(), QStringLiteral("xray.exe"))) {
            foundDnsHijackRule = true;
        }
        if (rule.value(QStringLiteral("outbound")).toString() == QStringLiteral("direct")
            && jsonArrayContainsString(rule.value(QStringLiteral("process_name")).toArray(), QStringLiteral("sing-box.exe"))) {
            foundDirectProcessRule = true;
        }
    }

    QVERIFY(foundUdpRejectRule);
    QVERIFY(foundDnsHijackRule);
    QVERIFY(foundDirectProcessRule);
}

void ClientConfigWriterTests::generateClientConfigsUsesDirectIcmpRoutingForNativeSingBoxCore()
{
    Config config = baseConfig();
    config.tunModeItem.icmpRouting = QStringLiteral("direct");
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundDirectIcmpRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("outbound")).toString() == QStringLiteral("direct")
            && ruleHasNetwork(rule, QStringLiteral("icmp"))) {
            foundDirectIcmpRule = true;
            break;
        }
    }

    QVERIFY(foundDirectIcmpRule);
}

void ClientConfigWriterTests::generateClientConfigsUsesRejectIcmpRoutingForNativeSingBoxCore()
{
    Config config = baseConfig();
    config.tunModeItem.icmpRouting = QStringLiteral("drop");
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundRejectIcmpRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("reject")
            && rule.value(QStringLiteral("method")).toString() == QStringLiteral("drop")
            && ruleHasNetwork(rule, QStringLiteral("icmp"))) {
            foundRejectIcmpRule = true;
            break;
        }
    }

    QVERIFY(foundRejectIcmpRule);
}

void ClientConfigWriterTests::generateClientConfigsIgnoresInvalidIcmpRoutingForNativeSingBoxCore()
{
    Config config = baseConfig();
    config.tunModeItem.icmpRouting = QStringLiteral("bogus");
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundIcmpRule = false;
    for (const QJsonValue& value : rules) {
        if (ruleHasNetwork(value.toObject(), QStringLiteral("icmp"))) {
            foundIcmpRule = true;
            break;
        }
    }

    QVERIFY(!foundIcmpRule);
}

void ClientConfigWriterTests::generateClientConfigsDefaultsTunInboundToIpv4OnlyWhenIpv6AddressNotEnabled()
{
    const Config config = baseConfig();
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject tunInbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("inbounds")).toArray(),
        QStringLiteral("tun-in"));
    const QJsonArray addresses = tunInbound.value(QStringLiteral("address")).toArray();

    QCOMPARE(addresses.size(), 1);
    QCOMPARE(addresses.at(0).toString(), QStringLiteral("172.18.0.1/30"));
}

void ClientConfigWriterTests::generateClientConfigsUsesServerMuxOverrideToDisableLegacyMux()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.muxEnabled = true;
    VmessItem server = baseServer();
    server.muxEnabled = false;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject mux = proxyOutbound.value(QStringLiteral("mux")).toObject();

    QCOMPARE(mux.value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(mux.value(QStringLiteral("concurrency")).toInt(), -1);
}

void ClientConfigWriterTests::generateClientConfigsUsesServerMuxOverrideToEnableLegacyMux()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.muxEnabled = false;
    VmessItem server = baseServer();
    server.muxEnabled = true;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject mux = proxyOutbound.value(QStringLiteral("mux")).toObject();

    QCOMPARE(mux.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(mux.value(QStringLiteral("concurrency")).toInt(), 8);
}

void ClientConfigWriterTests::generateClientConfigsUsesConfiguredSingBoxMultiplexProtocol()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.muxEnabled = true;
    config.mux4SboxProtocol = QStringLiteral("yamux");
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject multiplex = proxyOutbound.value(QStringLiteral("multiplex")).toObject();

    QCOMPARE(multiplex.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(multiplex.value(QStringLiteral("protocol")).toString(), QStringLiteral("yamux"));
    QCOMPARE(multiplex.value(QStringLiteral("max_connections")).toInt(), 8);
}

void ClientConfigWriterTests::generateClientConfigsUsesConfiguredSingBoxMultiplexMaxConnectionsAndPadding()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.muxEnabled = true;
    config.mux4SboxProtocol = QStringLiteral("yamux");
    config.mux4SboxMaxConnections = 3;
    config.mux4SboxPadding = true;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject multiplex = proxyOutbound.value(QStringLiteral("multiplex")).toObject();

    QCOMPARE(multiplex.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(multiplex.value(QStringLiteral("protocol")).toString(), QStringLiteral("yamux"));
    QCOMPARE(multiplex.value(QStringLiteral("max_connections")).toInt(), 3);
    QCOMPARE(multiplex.value(QStringLiteral("padding")).toBool(), true);
}

void ClientConfigWriterTests::generateClientConfigsUsesServerMuxOverrideToDisableSingBoxMultiplex()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.muxEnabled = true;
    config.mux4SboxProtocol = QStringLiteral("yamux");
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.muxEnabled = false;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));

    QVERIFY(proxyOutbound.value(QStringLiteral("multiplex")).isUndefined());
}

void ClientConfigWriterTests::generateClientConfigsUsesServerMuxOverrideToEnableSingBoxMultiplex()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.muxEnabled = false;
    config.mux4SboxProtocol = QStringLiteral("yamux");
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.muxEnabled = true;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    const QJsonObject multiplex = proxyOutbound.value(QStringLiteral("multiplex")).toObject();

    QCOMPARE(multiplex.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(multiplex.value(QStringLiteral("protocol")).toString(), QStringLiteral("yamux"));
    QCOMPARE(multiplex.value(QStringLiteral("max_connections")).toInt(), 8);
}

void ClientConfigWriterTests::generateClientConfigsSkipsSingBoxMultiplexWhenConfiguredProtocolEmpty()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.muxEnabled = true;
    config.mux4SboxProtocol.clear();
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));

    QVERIFY(proxyOutbound.value(QStringLiteral("multiplex")).isUndefined());
}

void ClientConfigWriterTests::generateClientConfigsAddsSingBoxCacheFileExperimentalWhenEnabled()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.enableCacheFile4Sbox = true;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject experimental = generated.primary.root.value(QStringLiteral("experimental")).toObject();
    const QJsonObject cacheFile = experimental.value(QStringLiteral("cache_file")).toObject();

    QCOMPARE(cacheFile.value(QStringLiteral("enabled")).toBool(), true);
    QVERIFY(cacheFile.value(QStringLiteral("path")).toString().endsWith(QStringLiteral("cache.db")));
}

void ClientConfigWriterTests::generateClientConfigsBuildsSimpleDnsPackForSingBoxCore()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.directDns = QStringLiteral("https://dns.alidns.com/dns-query");
    config.remoteDns = QStringLiteral("https://dns.google/dns-query");
    config.bootstrapDns = QStringLiteral("223.5.5.5");
    config.domainStrategyForFreedom = QStringLiteral("UseIPv4");
    config.domainStrategyForProxy = QStringLiteral("UseIPv6");
    config.dnsHosts = QStringLiteral("example.com 1.2.3.4\nfull:full.example.com 5.6.7.8");

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject dns = generated.primary.root.value(QStringLiteral("dns")).toObject();
    const QJsonArray servers = dns.value(QStringLiteral("servers")).toArray();
    const QJsonObject localDns = findObjectByTag(servers, QStringLiteral("local_local"));
    const QJsonObject directDns = findObjectByTag(servers, QStringLiteral("direct_dns"));
    const QJsonObject remoteDns = findObjectByTag(servers, QStringLiteral("remote_dns"));
    const QJsonObject hostsDns = findObjectByTag(servers, QStringLiteral("hosts_dns"));

    QCOMPARE(localDns.value(QStringLiteral("type")).toString(), QStringLiteral("udp"));
    QCOMPARE(localDns.value(QStringLiteral("server")).toString(), QStringLiteral("223.5.5.5"));
    QCOMPARE(directDns.value(QStringLiteral("type")).toString(), QStringLiteral("https"));
    QCOMPARE(directDns.value(QStringLiteral("server")).toString(), QStringLiteral("dns.alidns.com"));
    QCOMPARE(directDns.value(QStringLiteral("path")).toString(), QStringLiteral("/dns-query"));
    QCOMPARE(directDns.value(QStringLiteral("domain_resolver")).toString(), QStringLiteral("hosts_dns"));
    QCOMPARE(remoteDns.value(QStringLiteral("type")).toString(), QStringLiteral("https"));
    QCOMPARE(remoteDns.value(QStringLiteral("server")).toString(), QStringLiteral("dns.google"));
    QCOMPARE(remoteDns.value(QStringLiteral("path")).toString(), QStringLiteral("/dns-query"));
    QCOMPARE(remoteDns.value(QStringLiteral("detour")).toString(), QStringLiteral("proxy"));
    QCOMPARE(remoteDns.value(QStringLiteral("domain_resolver")).toString(), QStringLiteral("hosts_dns"));
    QCOMPARE(hostsDns.value(QStringLiteral("type")).toString(), QStringLiteral("hosts"));
    const QJsonObject predefined = hostsDns.value(QStringLiteral("predefined")).toObject();
    QCOMPARE(predefined.value(QStringLiteral("example.com")).toArray().at(0).toString(), QStringLiteral("1.2.3.4"));
    QCOMPARE(predefined.value(QStringLiteral("full.example.com")).toArray().at(0).toString(), QStringLiteral("5.6.7.8"));

    const QJsonArray dnsRules = dns.value(QStringLiteral("rules")).toArray();
    const QJsonObject directRule = findDnsRuleByServer(dnsRules, QStringLiteral("direct_dns"));
    const QJsonObject remoteRule = findDnsRuleByServer(dnsRules, QStringLiteral("remote_dns"));
    QCOMPARE(directRule.value(QStringLiteral("clash_mode")).toString(), QStringLiteral("Direct"));
    QCOMPARE(directRule.value(QStringLiteral("strategy")).toString(), QStringLiteral("prefer_ipv4"));
    QCOMPARE(remoteRule.value(QStringLiteral("clash_mode")).toString(), QStringLiteral("Global"));
    QCOMPARE(remoteRule.value(QStringLiteral("strategy")).toString(), QStringLiteral("prefer_ipv6"));
    QCOMPARE(dns.value(QStringLiteral("final")).toString(), QStringLiteral("remote_dns"));

    const QJsonObject resolver = generated.primary.root.value(QStringLiteral("route")).toObject()
                                     .value(QStringLiteral("default_domain_resolver"))
                                     .toObject();
    QCOMPARE(resolver.value(QStringLiteral("server")).toString(), QStringLiteral("direct_dns"));
    QCOMPARE(resolver.value(QStringLiteral("strategy")).toString(), QStringLiteral("prefer_ipv4"));

    const QJsonArray routeRules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();
    bool foundHostsResolveRule = false;
    for (const QJsonValue& value : routeRules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("resolve")
            && jsonArrayContainsString(rule.value(QStringLiteral("domain")).toArray(), QStringLiteral("example.com"))
            && jsonArrayContainsString(rule.value(QStringLiteral("domain")).toArray(), QStringLiteral("full.example.com"))) {
            foundHostsResolveRule = true;
            break;
        }
    }

    QVERIFY(foundHostsResolveRule);
}

void ClientConfigWriterTests::generateClientConfigsUsesDirectDnsAsFinalServerForCatchAllDirectRouting()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(
                QStringLiteral("direct"),
                {},
                QStringList{QStringLiteral("0.0.0.0/0")},
                {},
                QStringLiteral("0-65535"),
                QStringLiteral("tcp,udp"))})};

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject dns = generated.primary.root.value(QStringLiteral("dns")).toObject();
    QCOMPARE(dns.value(QStringLiteral("final")).toString(), QStringLiteral("direct_dns"));
}

void ClientConfigWriterTests::generateClientConfigsAddsSingBoxFakeDnsServerAndCacheStorageWhenEnabled()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.enableCacheFile4Sbox = true;
    config.fakeIp = true;
    config.globalFakeIp = false;

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject dns = generated.primary.root.value(QStringLiteral("dns")).toObject();
    const QJsonObject fakeDns = findObjectByTag(dns.value(QStringLiteral("servers")).toArray(), QStringLiteral("fake_dns"));
    QCOMPARE(fakeDns.value(QStringLiteral("type")).toString(), QStringLiteral("fakeip"));

    bool foundFakeDnsRule = false;
    for (const QJsonValue& value : dns.value(QStringLiteral("rules")).toArray()) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("server")).toString() == QStringLiteral("fake_dns")
            && rule.value(QStringLiteral("query_type")).toArray().size() == 2
            && rule.value(QStringLiteral("rewrite_ttl")).toInt() == 1) {
            foundFakeDnsRule = true;
            break;
        }
    }
    QVERIFY(foundFakeDnsRule);

    const QJsonObject cacheFile = generated.primary.root.value(QStringLiteral("experimental"))
                                      .toObject()
                                      .value(QStringLiteral("cache_file"))
                                      .toObject();
    QCOMPARE(cacheFile.value(QStringLiteral("store_fakeip")).toBool(), true);
}

void ClientConfigWriterTests::generateClientConfigsSkipsSimpleDnsRouteResolverForCustomSingBoxDnsObject()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.remoteDns = QStringLiteral(
        "{\"servers\":[{\"tag\":\"custom_remote\",\"type\":\"udp\",\"server\":\"8.8.8.8\"}],\"final\":\"custom_remote\"}");
    config.dnsHosts = QStringLiteral("example.com 1.2.3.4");

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject dns = generated.primary.root.value(QStringLiteral("dns")).toObject();
    QCOMPARE(findObjectByTag(dns.value(QStringLiteral("servers")).toArray(), QStringLiteral("custom_remote"))
                 .value(QStringLiteral("server"))
                 .toString(),
             QStringLiteral("8.8.8.8"));

    const QJsonObject route = generated.primary.root.value(QStringLiteral("route")).toObject();
    QVERIFY(route.value(QStringLiteral("default_domain_resolver")).isUndefined());

    bool foundHostsResolveRule = false;
    for (const QJsonValue& value : route.value(QStringLiteral("rules")).toArray()) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("resolve")
            && jsonArrayContainsString(rule.value(QStringLiteral("domain")).toArray(), QStringLiteral("example.com"))) {
            foundHostsResolveRule = true;
            break;
        }
    }
    QVERIFY(!foundHostsResolveRule);
}

void ClientConfigWriterTests::generateClientConfigsKeepsDirectDnsAheadOfNonGlobalFakeDnsFallback()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.fakeIp = true;
    config.globalFakeIp = false;
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(QStringLiteral("direct"), QStringList{QStringLiteral("full:direct.example")}),
            createRoutingRule(QStringLiteral("proxy"), QStringList{QStringLiteral("full:proxy.example")})})};

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("dns")).toObject().value(QStringLiteral("rules")).toArray();

    int directRuleIndex = -1;
    int proxyFakeRuleIndex = -1;
    int proxyRemoteRuleIndex = -1;
    int catchAllFakeRuleIndex = -1;
    for (int i = 0; i < rules.size(); ++i) {
        const QJsonObject rule = rules.at(i).toObject();
        const QJsonArray domains = rule.value(QStringLiteral("domain")).toArray();
        if (jsonArrayContainsString(domains, QStringLiteral("direct.example"))
            && rule.value(QStringLiteral("server")).toString() == QStringLiteral("direct_dns")) {
            directRuleIndex = i;
        }
        if (jsonArrayContainsString(domains, QStringLiteral("proxy.example"))
            && rule.value(QStringLiteral("server")).toString() == QStringLiteral("fake_dns")
            && rule.value(QStringLiteral("query_type")).toArray().size() == 2
            && rule.value(QStringLiteral("rewrite_ttl")).toInt() == 1) {
            proxyFakeRuleIndex = i;
        }
        if (jsonArrayContainsString(domains, QStringLiteral("proxy.example"))
            && rule.value(QStringLiteral("server")).toString() == QStringLiteral("remote_dns")) {
            proxyRemoteRuleIndex = i;
        }
        if (!rule.contains(QStringLiteral("domain"))
            && rule.value(QStringLiteral("server")).toString() == QStringLiteral("fake_dns")
            && rule.value(QStringLiteral("query_type")).toArray().size() == 2
            && rule.value(QStringLiteral("rewrite_ttl")).toInt() == 1) {
            catchAllFakeRuleIndex = i;
        }
    }

    QVERIFY(directRuleIndex >= 0);
    QVERIFY(proxyFakeRuleIndex >= 0);
    QVERIFY(proxyRemoteRuleIndex >= 0);
    QVERIFY(catchAllFakeRuleIndex >= 0);
    QVERIFY(proxyFakeRuleIndex < proxyRemoteRuleIndex);
    QVERIFY(directRuleIndex < catchAllFakeRuleIndex);
    QVERIFY(proxyRemoteRuleIndex < catchAllFakeRuleIndex);
}

void ClientConfigWriterTests::generateClientConfigsUsesLogicalFakeIpFilterWhenGlobalFakeIpEnabled()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.fakeIp = true;
    config.globalFakeIp = true;

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("dns")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundLogicalFakeRule = false;
    bool foundPlainCatchAllFakeRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("server")).toString() == QStringLiteral("fake_dns")
            && rule.value(QStringLiteral("type")).toString() == QStringLiteral("logical")
            && rule.value(QStringLiteral("mode")).toString() == QStringLiteral("and")
            && rule.value(QStringLiteral("rewrite_ttl")).toInt() == 1) {
            const QJsonArray nestedRules = rule.value(QStringLiteral("rules")).toArray();
            QCOMPARE(nestedRules.size(), 2);
            QCOMPARE(nestedRules.at(0).toObject().value(QStringLiteral("query_type")).toArray().size(), 2);
            const QJsonObject filterRule = nestedRules.at(1).toObject();
            QCOMPARE(filterRule.value(QStringLiteral("invert")).toBool(), true);
            QVERIFY(jsonArrayContainsString(filterRule.value(QStringLiteral("domain_suffix")).toArray(), QStringLiteral("localhost")));
            QVERIFY(jsonArrayContainsString(filterRule.value(QStringLiteral("domain_keyword")).toArray(), QStringLiteral("stun")));
            foundLogicalFakeRule = true;
        }
        if (rule.value(QStringLiteral("server")).toString() == QStringLiteral("fake_dns")
            && rule.value(QStringLiteral("query_type")).toArray().size() == 2
            && !rule.contains(QStringLiteral("type"))) {
            foundPlainCatchAllFakeRule = true;
        }
    }

    QVERIFY(foundLogicalFakeRule);
    QVERIFY(!foundPlainCatchAllFakeRule);
}

void ClientConfigWriterTests::generateClientConfigsAddsCommonHostsWithoutRouteResolveRuleForSingBoxCore()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.addCommonHosts = true;
    config.blockBindingQuery = true;

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject dns = generated.primary.root.value(QStringLiteral("dns")).toObject();
    const QJsonObject hostsDns = findObjectByTag(dns.value(QStringLiteral("servers")).toArray(), QStringLiteral("hosts_dns"));
    const QJsonObject predefined = hostsDns.value(QStringLiteral("predefined")).toObject();
    QVERIFY(predefined.contains(QStringLiteral("dns.google")));

    bool foundBlockBindingRule = false;
    for (const QJsonValue& value : dns.value(QStringLiteral("rules")).toArray()) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("predefined")
            && rule.value(QStringLiteral("rcode")).toString() == QStringLiteral("NOERROR")
            && rule.value(QStringLiteral("query_type")).toArray().size() == 2
            && rule.value(QStringLiteral("query_type")).toArray().at(0).toInt() == 64
            && rule.value(QStringLiteral("query_type")).toArray().at(1).toInt() == 65) {
            foundBlockBindingRule = true;
            break;
        }
    }
    QVERIFY(foundBlockBindingRule);

    const QJsonArray routeRules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();
    bool foundPredefinedHostResolveRule = false;
    for (const QJsonValue& value : routeRules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("resolve")
            && jsonArrayContainsString(rule.value(QStringLiteral("domain")).toArray(), QStringLiteral("dns.google"))) {
            foundPredefinedHostResolveRule = true;
            break;
        }
    }
    const QByteArray routeJson = QJsonDocument(generated.primary.root.value(QStringLiteral("route")).toObject())
                                     .toJson(QJsonDocument::Compact);
    QVERIFY2(!foundPredefinedHostResolveRule, routeJson.constData());
}

void ClientConfigWriterTests::generateClientConfigsAddsSystemHostsForSingBoxDnsAndResolveRulesWhenEnabled()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString hostsPath = tempDir.filePath(QStringLiteral("hosts"));
    QFile hostsFile(hostsPath);
    QVERIFY(hostsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(hostsFile.write("5.6.7.8 system-host.example\n") >= 0);
    hostsFile.close();

    QVERIFY(qputenv("V2RAYQ_SYSTEM_HOSTS_PATH", hostsPath.toUtf8()));

    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.addCommonHosts = false;
    config.useSystemHosts = true;
    config.dnsHosts.clear();

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    qunsetenv("V2RAYQ_SYSTEM_HOSTS_PATH");

    const QJsonObject dns = generated.primary.root.value(QStringLiteral("dns")).toObject();
    const QJsonObject hostsDns = findObjectByTag(dns.value(QStringLiteral("servers")).toArray(), QStringLiteral("hosts_dns"));
    const QJsonObject predefined = hostsDns.value(QStringLiteral("predefined")).toObject();
    QCOMPARE(predefined.value(QStringLiteral("system-host.example")).toArray().at(0).toString(), QStringLiteral("5.6.7.8"));

    const QJsonArray routeRules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();
    bool foundSystemHostResolveRule = false;
    for (const QJsonValue& value : routeRules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("resolve")
            && jsonArrayContainsString(rule.value(QStringLiteral("domain")).toArray(), QStringLiteral("system-host.example"))) {
            foundSystemHostResolveRule = true;
            break;
        }
    }
    QVERIFY(foundSystemHostResolveRule);
}

void ClientConfigWriterTests::generateClientConfigsUsesHostsDnsResolverForSingBoxDnsServerHostnames()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.addCommonHosts = true;
    config.useSystemHosts = false;
    config.bootstrapDns = QStringLiteral("https://bootstrap-host.example/dns-query");
    config.directDns = QStringLiteral("https://direct-host.example/dns-query");
    config.remoteDns = QStringLiteral("https://dns.google/dns-query");
    config.dnsHosts =
        QStringLiteral("bootstrap-host.example 1.1.1.1\n"
                       "direct-host.example 2.2.2.2");

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray servers = generated.primary.root.value(QStringLiteral("dns")).toObject().value(QStringLiteral("servers")).toArray();
    const QJsonObject localDns = findObjectByTag(servers, QStringLiteral("local_local"));
    const QJsonObject remoteDns = findObjectByTag(servers, QStringLiteral("remote_dns"));
    const QJsonObject directDns = findObjectByTag(servers, QStringLiteral("direct_dns"));

    QCOMPARE(localDns.value(QStringLiteral("domain_resolver")).toString(), QStringLiteral("hosts_dns"));
    QCOMPARE(remoteDns.value(QStringLiteral("domain_resolver")).toString(), QStringLiteral("hosts_dns"));
    QCOMPARE(directDns.value(QStringLiteral("domain_resolver")).toString(), QStringLiteral("hosts_dns"));
}

void ClientConfigWriterTests::generateClientConfigsAddsLegacyDirectExpectedIpsForMatchingDirectGeosite()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.directDns = QStringLiteral("223.5.5.5");
    config.remoteDns = QStringLiteral("8.8.8.8");
    config.directExpectedIps = QStringLiteral("geoip:cn,1.2.3.0/24");
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(QStringLiteral("direct"), QStringList{QStringLiteral("geosite:cn")})})};

    VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray servers = generated.primary.root.value(QStringLiteral("dns")).toObject().value(QStringLiteral("servers")).toArray();
    bool foundExpectedIpServer = false;
    for (const QJsonValue& value : servers) {
        const QJsonObject serverObject = value.toObject();
        if (serverObject.value(QStringLiteral("tag")).toString() != QStringLiteral("direct-dns-1")) {
            continue;
        }

        if (!jsonArrayContainsString(serverObject.value(QStringLiteral("domains")).toArray(), QStringLiteral("geosite:cn"))) {
            continue;
        }

        const QJsonArray expectedIps = serverObject.value(QStringLiteral("expectedIPs")).toArray();
        QVERIFY(jsonArrayContainsString(expectedIps, QStringLiteral("geoip:cn")));
        QVERIFY(jsonArrayContainsString(expectedIps, QStringLiteral("1.2.3.0/24")));
        foundExpectedIpServer = true;
        break;
    }

    QVERIFY(foundExpectedIpServer);
}

void ClientConfigWriterTests::generateClientConfigsAddsPredefinedAnswerForNonFullHostRule()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.addCommonHosts = false;
    config.blockBindingQuery = false;
    config.dnsHosts = QStringLiteral("domain:example.com 1.2.3.4");

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray dnsRules = generated.primary.root.value(QStringLiteral("dns")).toObject().value(QStringLiteral("rules")).toArray();
    bool foundPredefinedAnswer = false;
    for (const QJsonValue& value : dnsRules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("predefined")
            && jsonArrayContainsString(rule.value(QStringLiteral("domain_suffix")).toArray(), QStringLiteral("example.com"))
            && jsonArrayContainsString(rule.value(QStringLiteral("answer")).toArray(), QStringLiteral("*. IN A 1.2.3.4"))) {
            foundPredefinedAnswer = true;
            break;
        }
    }
    QVERIFY(foundPredefinedAnswer);
}

void ClientConfigWriterTests::generateClientConfigsAddsSniffAndDnsHijackRulesForNativeSingBoxCore()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = true;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundSniffRule = false;
    bool foundDnsProtocolHijackRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("sniff")) {
            foundSniffRule = true;
        }
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("hijack-dns")
            && jsonArrayContainsString(rule.value(QStringLiteral("protocol")).toArray(), QStringLiteral("dns"))) {
            foundDnsProtocolHijackRule = true;
        }
    }

    QVERIFY(foundSniffRule);
    QVERIFY(foundDnsProtocolHijackRule);
}

void ClientConfigWriterTests::generateClientConfigsSetsSingBoxSniffOverrideDestinationWhenRouteOnlyFalse()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = true;
    config.routeOnly = false;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundSniffRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("sniff")) {
            foundSniffRule = true;
            QVERIFY(!rule.contains(QStringLiteral("override_destination")));
        }
    }

    QVERIFY(foundSniffRule);
}

void ClientConfigWriterTests::generateClientConfigsOmitsSingBoxSniffOverrideDestinationWhenRouteOnlyTrue()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = true;
    config.routeOnly = true;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundSniffRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("sniff")) {
            foundSniffRule = true;
            QVERIFY(!rule.contains(QStringLiteral("override_destination")));
        }
    }

    QVERIFY(foundSniffRule);
}

void ClientConfigWriterTests::generateClientConfigsAddsUdpDnsHijackRuleWhenSniffingDisabledForNativeSingBoxCore()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = false;
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundUdpDnsHijackRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("hijack-dns")
            && rule.value(QStringLiteral("port")).toArray().size() == 1
            && rule.value(QStringLiteral("port")).toArray().at(0).toInt() == 53
            && ruleHasNetwork(rule, QStringLiteral("udp"))) {
            foundUdpDnsHijackRule = true;
            break;
        }
    }

    QVERIFY(foundUdpDnsHijackRule);
}

void ClientConfigWriterTests::generateClientConfigsAddsSingBoxClashModeRouteRules()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = false;
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(QStringLiteral("proxy"), QStringList{QStringLiteral("geosite:google")})})};
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    int directIndex = -1;
    int globalIndex = -1;
    int userRuleIndex = -1;
    for (int i = 0; i < rules.size(); ++i) {
        const QJsonObject rule = rules.at(i).toObject();
        if (rule.value(QStringLiteral("clash_mode")).toString() == QStringLiteral("Direct")
            && rule.value(QStringLiteral("outbound")).toString() == QStringLiteral("direct")) {
            directIndex = i;
        }
        if (rule.value(QStringLiteral("clash_mode")).toString() == QStringLiteral("Global")
            && rule.value(QStringLiteral("outbound")).toString() == QStringLiteral("proxy")) {
            globalIndex = i;
        }
        if (jsonArrayContainsString(rule.value(QStringLiteral("rule_set")).toArray(), QStringLiteral("geosite-google"))) {
            userRuleIndex = i;
        }
    }

    QVERIFY(directIndex >= 0);
    QVERIFY(globalIndex >= 0);
    QVERIFY(userRuleIndex >= 0);
    QVERIFY(directIndex < userRuleIndex);
    QVERIFY(globalIndex < userRuleIndex);
}

void ClientConfigWriterTests::generateClientConfigsMapsSingBoxBlockRoutingRuleToRejectAction()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = false;
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(QStringLiteral("block"), QStringList{QStringLiteral("geosite:category-ads-all")})})};
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundRejectRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("reject")
            && jsonArrayContainsString(rule.value(QStringLiteral("rule_set")).toArray(), QStringLiteral("geosite-category-ads-all"))) {
            QVERIFY(rule.value(QStringLiteral("outbound")).isUndefined());
            foundRejectRule = true;
            break;
        }
    }

    QVERIFY(foundRejectRule);
}

void ClientConfigWriterTests::generateClientConfigsTreatsPlainSingBoxRoutingDomainsAsKeywords()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = false;
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(QStringLiteral("proxy"), QStringList{QStringLiteral("example.com")})})};
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundKeywordRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (jsonArrayContainsString(rule.value(QStringLiteral("domain_keyword")).toArray(), QStringLiteral("example.com"))) {
            QVERIFY(rule.value(QStringLiteral("domain")).isUndefined());
            foundKeywordRule = true;
            break;
        }
    }

    QVERIFY(foundKeywordRule);
}

void ClientConfigWriterTests::generateClientConfigsSplitsSingBoxRoutingPortsIntoPortAndRangeFields()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = false;
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(QStringLiteral("proxy"), {}, {}, {}, QStringLiteral("80,443,1000-2000"))})};
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundPortRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        const QJsonArray ports = rule.value(QStringLiteral("port")).toArray();
        const QJsonArray portRanges = rule.value(QStringLiteral("port_range")).toArray();
        if (ports.size() == 2
            && ports.at(0).toInt() == 80
            && ports.at(1).toInt() == 443
            && portRanges.size() == 1
            && portRanges.at(0).toString() == QStringLiteral("1000:2000")) {
            foundPortRule = true;
            break;
        }
    }

    QVERIFY(foundPortRule);
}

void ClientConfigWriterTests::generateClientConfigsAddsSingBoxResolveRuleBeforeUserRulesForIpOnDemand()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = false;
    config.domainStrategy = QStringLiteral("IPOnDemand");
    config.domainStrategy4Singbox = QStringLiteral("prefer_ipv6");
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(QStringLiteral("proxy"), QStringList{QStringLiteral("geosite:google")})})};
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    int resolveRuleIndex = -1;
    int ruleSetRuleIndex = -1;
    for (int i = 0; i < rules.size(); ++i) {
        const QJsonObject rule = rules.at(i).toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("resolve")
            && rule.value(QStringLiteral("strategy")).toString() == QStringLiteral("prefer_ipv6")) {
            resolveRuleIndex = i;
        }
        if (jsonArrayContainsString(rule.value(QStringLiteral("rule_set")).toArray(), QStringLiteral("geosite-google"))) {
            ruleSetRuleIndex = i;
        }
    }

    QVERIFY(resolveRuleIndex >= 0);
    QVERIFY(ruleSetRuleIndex >= 0);
    QVERIFY(resolveRuleIndex < ruleSetRuleIndex);
}

void ClientConfigWriterTests::generateClientConfigsAddsSingBoxResolveRuleAfterUserRulesForIpIfNonMatch()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = false;
    config.domainStrategy = QStringLiteral("IPIfNonMatch");
    config.domainStrategy4Singbox = QStringLiteral("prefer_ipv4");
    RoutingItem route = createRoutingItem({
        createRoutingRule(QStringLiteral("proxy"), {}, QStringList{QStringLiteral("geoip:telegram")})});
    route.domainStrategy4Singbox = QStringLiteral("ipv6_only");
    config.routingItems = {route};
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    int resolveRuleIndex = -1;
    int firstGeoipRuleIndex = -1;
    for (int i = 0; i < rules.size(); ++i) {
        const QJsonObject rule = rules.at(i).toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("resolve")
            && rule.value(QStringLiteral("strategy")).toString() == QStringLiteral("ipv6_only")) {
            resolveRuleIndex = i;
        }
        if (firstGeoipRuleIndex < 0
            && jsonArrayContainsString(rule.value(QStringLiteral("rule_set")).toArray(), QStringLiteral("geoip-telegram"))) {
            firstGeoipRuleIndex = i;
        }
    }

    QVERIFY(resolveRuleIndex >= 0);
    QVERIFY(firstGeoipRuleIndex >= 0);
    QVERIFY(resolveRuleIndex > firstGeoipRuleIndex);
}

void ClientConfigWriterTests::generateClientConfigsReappliesSingBoxIpRulesAfterResolveForIpIfNonMatch()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = false;
    config.domainStrategy = QStringLiteral("IPIfNonMatch");
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(QStringLiteral("proxy"), {}, QStringList{QStringLiteral("geoip:telegram")})})};
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    int resolveRuleIndex = -1;
    int geoipRuleCount = 0;
    bool foundGeoipRuleAfterResolve = false;
    for (int i = 0; i < rules.size(); ++i) {
        const QJsonObject rule = rules.at(i).toObject();
        if (rule.value(QStringLiteral("action")).toString() == QStringLiteral("resolve")) {
            resolveRuleIndex = i;
        }
        if (jsonArrayContainsString(rule.value(QStringLiteral("rule_set")).toArray(), QStringLiteral("geoip-telegram"))
            && rule.value(QStringLiteral("outbound")).toString() == QStringLiteral("proxy")) {
            ++geoipRuleCount;
            if (resolveRuleIndex >= 0 && i > resolveRuleIndex) {
                foundGeoipRuleAfterResolve = true;
            }
        }
    }

    QVERIFY(resolveRuleIndex >= 0);
    QCOMPARE(geoipRuleCount, 2);
    QVERIFY(foundGeoipRuleAfterResolve);
}

void ClientConfigWriterTests::generateClientConfigsAddsSingBoxDirectExpectedIpsForMatchingDirectGeosite()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = false;
    config.directExpectedIps = QStringLiteral("geoip:cn,1.2.3.0/24");
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(QStringLiteral("direct"), QStringList{QStringLiteral("geosite:cn")})})};

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("dns")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundExpectedIpRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("server")).toString() != QStringLiteral("direct_dns")) {
            continue;
        }
        if (!jsonArrayContainsString(rule.value(QStringLiteral("rule_set")).toArray(), QStringLiteral("geosite-cn"))) {
            continue;
        }
        QVERIFY(jsonArrayContainsString(rule.value(QStringLiteral("rule_set")).toArray(), QStringLiteral("geoip-cn")));
        QVERIFY(jsonArrayContainsString(rule.value(QStringLiteral("ip_cidr")).toArray(), QStringLiteral("1.2.3.0/24")));
        foundExpectedIpRule = true;
        break;
    }

    QVERIFY(foundExpectedIpRule);
}

void ClientConfigWriterTests::generateClientConfigsCarriesLegacyRoutingNetworkIntoProcessRules()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = false;
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(
                QStringLiteral("proxy"),
                {},
                {},
                {},
                {},
                QStringLiteral("tcp,udp"),
                QStringList{QStringLiteral("v2rayq.exe")})})};
    VmessItem server = baseServer();

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("routing")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundProcessRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (jsonArrayContainsString(rule.value(QStringLiteral("process")).toArray(), QStringLiteral("v2rayq.exe"))) {
            QCOMPARE(rule.value(QStringLiteral("network")).toString(), QStringLiteral("tcp,udp"));
            foundProcessRule = true;
            break;
        }
    }

    QVERIFY(foundProcessRule);
}

void ClientConfigWriterTests::generateClientConfigsSplitsSingBoxRoutingProcessNameAndPathRules()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.sniffingEnabled = false;
    config.routingItems = {
        createRoutingItem({
            createRoutingRule(
                QStringLiteral("proxy"),
                {},
                {},
                {},
                {},
                QStringLiteral("tcp,udp"),
                QStringList{
                    QStringLiteral("self/"),
                    QStringLiteral("plainproc"),
                    QStringLiteral("C:/Program Files/Test/test.exe")})})};
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonArray rules = generated.primary.root.value(QStringLiteral("route")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundProcessNameRule = false;
    bool foundProcessPathRule = false;
    for (const QJsonValue& value : rules) {
        const QJsonObject rule = value.toObject();
        if (jsonArrayContainsString(rule.value(QStringLiteral("process_name")).toArray(), QStringLiteral("sing-box.exe"))
            && jsonArrayContainsString(rule.value(QStringLiteral("process_name")).toArray(), QStringLiteral("plainproc.exe"))) {
            QVERIFY(ruleHasNetwork(rule, QStringLiteral("tcp")));
            QVERIFY(ruleHasNetwork(rule, QStringLiteral("udp")));
            foundProcessNameRule = true;
        }
        if (jsonArrayContainsString(rule.value(QStringLiteral("process_path")).toArray(), QStringLiteral("C:\\Program Files\\Test\\test.exe"))) {
            QVERIFY(ruleHasNetwork(rule, QStringLiteral("tcp")));
            QVERIFY(ruleHasNetwork(rule, QStringLiteral("udp")));
            foundProcessPathRule = true;
        }
    }

    QVERIFY(foundProcessNameRule);
    QVERIFY(foundProcessPathRule);
}

void ClientConfigWriterTests::generateClientConfigsMergesCustomRulesBeforeSelectedBaseRoute()
{
    Config config = legacyConfig();
    config.tunModeItem.enableTun = false;
    config.enableRoutingAdvanced = true;
    config.routingIndex = 0;
    config.routingCustomRules = {
        createRoutingRule(QStringLiteral("proxy"), QStringList{QStringLiteral("domain:openai.com")}),
        createRoutingRule(QStringLiteral("direct"), {}, QStringList{QStringLiteral("geoip:private")}),
        createRoutingRule(QStringLiteral("block"), QStringList{QStringLiteral("geosite:category-ads-all")})};
    config.routingItems = {
        createRoutingItem(QList<RoutingRule>{
            createRoutingRule(QStringLiteral("direct"), QStringList{QStringLiteral("geosite:cn")})})};

    VmessItem server = baseServer();
    server.coreType = CoreType::Xray;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);
    const QJsonArray rules = generated.primary.root.value(QStringLiteral("routing")).toObject().value(QStringLiteral("rules")).toArray();

    QVERIFY(rules.size() >= 4);
    QCOMPARE(rules.at(0).toObject().value(QStringLiteral("outboundTag")).toString(), QStringLiteral("block"));
    QVERIFY(jsonArrayContainsString(
        rules.at(0).toObject().value(QStringLiteral("domain")).toArray(),
        QStringLiteral("geosite:category-ads-all")));

    QCOMPARE(rules.at(1).toObject().value(QStringLiteral("outboundTag")).toString(), QStringLiteral("direct"));
    QVERIFY(jsonArrayContainsString(
        rules.at(1).toObject().value(QStringLiteral("ip")).toArray(),
        QStringLiteral("geoip:private")));

    QCOMPARE(rules.at(2).toObject().value(QStringLiteral("outboundTag")).toString(), QStringLiteral("proxy"));
    QVERIFY(jsonArrayContainsString(
        rules.at(2).toObject().value(QStringLiteral("domain")).toArray(),
        QStringLiteral("domain:openai.com")));

    QCOMPARE(rules.at(3).toObject().value(QStringLiteral("outboundTag")).toString(), QStringLiteral("direct"));
    QVERIFY(jsonArrayContainsString(
        rules.at(3).toObject().value(QStringLiteral("domain")).toArray(),
        QStringLiteral("geosite:cn")));
}

void ClientConfigWriterTests::generateClientConfigsUsesCustomDirectDomainsForSingBoxDnsRules()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.remoteDns = QStringLiteral("1.1.1.1");
    config.directDns = QStringLiteral("223.5.5.5");
    config.routingCustomRules = {
        createRoutingRule(QStringLiteral("direct"), QStringList{QStringLiteral("domain:ftp.sh")})};

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);
    const QJsonArray dnsRules = generated.primary.root.value(QStringLiteral("dns")).toObject().value(QStringLiteral("rules")).toArray();

    bool foundDirectDnsRule = false;
    for (const QJsonValue& value : dnsRules) {
        const QJsonObject rule = value.toObject();
        if (rule.value(QStringLiteral("server")).toString() != QStringLiteral("direct_dns")) {
            continue;
        }

        if (jsonArrayContainsString(rule.value(QStringLiteral("domain_suffix")).toArray(), QStringLiteral("ftp.sh"))) {
            foundDirectDnsRule = true;
            break;
        }
    }

    QVERIFY(foundDirectDnsRule);
}

void ClientConfigWriterTests::generateClientConfigsDoesNotCreateTunCompatRelayForSingBoxCore()
{
    const Config config = baseConfig();
    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    QVERIFY(generated.auxiliary.isEmpty());
}

void ClientConfigWriterTests::generateClientConfigsBuildsSingBoxHysteria2Outbound()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.configType = ConfigType::Hysteria2;
    server.address = QStringLiteral("69.12.75.229");
    server.port = 18962;
    server.id = QStringLiteral("83e96db9-34ec-4d85-ac13-e37e44006aa8");
    server.obfsPassword = QStringLiteral("A60kExOWbVc9FHqS");
    server.headerType = QStringLiteral("salamander");
    server.streamSecurity = QStringLiteral("tls");
    server.sni = QStringLiteral("drus1.ipl.cc.cd");
    server.allowInsecure = QStringLiteral("1");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("type")).toString(), QStringLiteral("hysteria2"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("server")).toString(), QStringLiteral("69.12.75.229"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("server_port")).toInt(), 18962);
    QCOMPARE(proxyOutbound.value(QStringLiteral("password")).toString(), QStringLiteral("83e96db9-34ec-4d85-ac13-e37e44006aa8"));

    const QJsonObject obfs = proxyOutbound.value(QStringLiteral("obfs")).toObject();
    QCOMPARE(obfs.value(QStringLiteral("type")).toString(), QStringLiteral("salamander"));
    QCOMPARE(obfs.value(QStringLiteral("password")).toString(), QStringLiteral("A60kExOWbVc9FHqS"));

    const QJsonObject tls = proxyOutbound.value(QStringLiteral("tls")).toObject();
    QCOMPARE(tls.value(QStringLiteral("enabled")).toBool(), true);
    QCOMPARE(tls.value(QStringLiteral("server_name")).toString(), QStringLiteral("drus1.ipl.cc.cd"));
    QCOMPARE(tls.value(QStringLiteral("insecure")).toBool(), true);
}

void ClientConfigWriterTests::generateClientConfigsOmitsSingBoxDnsAndDefaultResolverWhenDnsInputsAreEmpty()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;
    config.enableRoutingAdvanced = false;
    config.routingItems.clear();
    config.routingCustomRules.clear();
    config.remoteDns.clear();
    config.directDns.clear();
    config.bootstrapDns.clear();
    config.dnsHosts.clear();
    config.addCommonHosts = false;
    config.useSystemHosts = false;
    config.fakeIp = false;

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    QVERIFY(generated.primary.root.value(QStringLiteral("dns")).isUndefined());

    const QJsonObject route = generated.primary.root.value(QStringLiteral("route")).toObject();
    QVERIFY(route.value(QStringLiteral("default_domain_resolver")).isUndefined());
}

void ClientConfigWriterTests::generateClientConfigsDoesNotCreateTunCompatRelayForProtocolConfiguredSingBoxCore()
{
    Config config = baseConfig();
    setProtocolCore(config, ConfigType::VMess, CoreType::SingBox);

    VmessItem server = baseServer();
    server.coreType = CoreType::Auto;

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    QVERIFY(generated.auxiliary.isEmpty());
}

void ClientConfigWriterTests::generateClientConfigsDoesNotEmitSingBoxNetworkForAnyTls()
{
    Config config = baseConfig();
    config.tunModeItem.enableTun = false;

    VmessItem server = baseServer();
    server.coreType = CoreType::SingBox;
    server.configType = ConfigType::AnyTLS;
    server.address = QStringLiteral("69.12.75.229");
    server.port = 18963;
    server.id = QStringLiteral("83e96db9-34ec-4d85-ac13-e37e44006aa8");
    server.streamSecurity = QStringLiteral("tls");
    server.sni = QStringLiteral("drus1.ipl.cc.cd");
    server.allowInsecure = QStringLiteral("1");

    ClientConfigWriter writer;
    const ClientConfigWriter::GeneratedConfigSet generated = writer.generateClientConfigs(config, server, 0);

    const QJsonObject proxyOutbound = findObjectByTag(
        generated.primary.root.value(QStringLiteral("outbounds")).toArray(),
        QStringLiteral("proxy"));
    QCOMPARE(proxyOutbound.value(QStringLiteral("type")).toString(), QStringLiteral("anytls"));
    QVERIFY(proxyOutbound.value(QStringLiteral("network")).isUndefined());
}

QTEST_MAIN(ClientConfigWriterTests)

#include "ClientConfigWriterTests.moc"
