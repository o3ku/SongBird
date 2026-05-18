#include <QtTest>

#include <algorithm>

#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "persistence/JsonConfigRepository.h"

class JsonConfigRepositoryTests : public QObject {
    Q_OBJECT

private slots:
    void loadDefaultsRouteOnlyToFalseWhenMissing();
    void loadReadsRouteOnlyFromInbound();
    void savePersistsRouteOnlyToInbound();
    void loadDefaultsEnableFragmentToFalseWhenMissing();
    void loadReadsEnableFragmentFromRoot();
    void savePersistsEnableFragmentToRoot();
    void loadDefaultsEnableCacheFile4SboxToTrueWhenMissing();
    void loadReadsEnableCacheFile4SboxFromRoot();
    void savePersistsEnableCacheFile4SboxToRoot();
    void loadDefaultsDefaultUserAgentToEmptyWhenMissing();
    void loadReadsDefaultUserAgentFromRoot();
    void savePersistsDefaultUserAgentToRoot();
    void loadDefaultsDefaultFingerprintToEmptyWhenMissing();
    void loadReadsDefaultFingerprintFromRoot();
    void savePersistsDefaultFingerprintToRoot();
    void loadReadsDomainStrategy4SingboxFromRoot();
    void savePersistsDomainStrategy4SingboxToRoot();
    void loadDefaultsSimpleDnsBaseFieldsWhenMissing();
    void loadReadsSimpleDnsBaseFieldsFromRoot();
    void savePersistsSimpleDnsBaseFieldsToRoot();
    void loadDefaultsSimpleDnsHostsFlagsWhenMissing();
    void loadReadsSimpleDnsHostsFlagsFromRoot();
    void savePersistsSimpleDnsHostsFlagsToRoot();
    void loadDefaultsUseSystemHostsToFalseWhenMissing();
    void loadReadsUseSystemHostsFromRoot();
    void savePersistsUseSystemHostsToRoot();
    void loadDefaultsServeStaleAndParallelQueryToFalseWhenMissing();
    void loadReadsServeStaleAndParallelQueryFromRoot();
    void savePersistsServeStaleAndParallelQueryToRoot();
    void loadDefaultsDirectExpectedIpsToEmptyWhenMissing();
    void loadReadsDirectExpectedIpsFromRoot();
    void savePersistsDirectExpectedIpsToRoot();
    void loadDefaultsMux4SboxProtocolToH2muxWhenMissing();
    void loadReadsMux4SboxProtocolFromRoot();
    void savePersistsMux4SboxProtocolToRoot();
    void loadDefaultsMux4SboxMaxConnectionsToEightWhenMissing();
    void loadLeavesMux4SboxPaddingUnsetWhenMissing();
    void loadReadsMux4SboxMaxConnectionsAndPaddingFromRoot();
    void savePersistsMux4SboxMaxConnectionsAndPaddingToRoot();
    void loadLeavesServerMuxOverrideUnsetWhenMissing();
    void loadReadsServerMuxOverrideWhenPresent();
    void savePersistsServerMuxOverrideToServerArray();
    void loadReadsServerFinalmask();
    void savePersistsServerFinalmask();
    void loadReadsServerCert();
    void savePersistsServerCert();
    void loadReadsServerCertSha();
    void savePersistsServerCertSha();
    void loadReadsServerMldsa65Verify();
    void savePersistsServerMldsa65Verify();
    void loadReadsServerEchFields();
    void savePersistsServerEchFields();
    void loadReadsRoutingRuleNetworkAndProcessFields();
    void savePersistsRoutingRuleNetworkAndProcessFields();
    void loadReadsRoutingCustomRules();
    void savePersistsRoutingCustomRules();
    void savePersistsSettingsRoutingRuleTabKey();
    void savePreservesUnknownRootFields();
    void saveRemovesDeprecatedDeadConfigKeys();
    void loadDefaultsMainRuntimeStateToOffWhenMissing();
    void loadReadsMainRuntimeStateFromUiItem();
    void loadReadsMainQrPreviewVisibleFromUiItem();
    void savePersistsMainQrPreviewVisible();
    void savePersistsMainRuntimeStateToUiItem();
    void loadReadsRoutingItemDomainStrategy4Singbox();
    void savePersistsRoutingItemDomainStrategy4Singbox();
    void loadReadsGlobalHotkeys();
    void savePersistsGlobalHotkeys();
    void saveRemovesDeprecatedTls13ConfigKey();
    void loadDefaultsTunIcmpRoutingToRuleWhenMissing();
    void loadDefaultsTunIcmpRoutingToRuleWhenEmptyObject();
    void loadDefaultsTunEnableIpv6AddressToFalseWhenMissing();
    void loadDefaultsTunEnableIpv6AddressToFalseWhenEmptyObject();
};

void JsonConfigRepositoryTests::loadDefaultsRouteOnlyToFalseWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(!config.routeOnly);
}

void JsonConfigRepositoryTests::loadReadsRouteOnlyFromInbound()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject inbound;
    inbound.insert(QStringLiteral("routeOnly"), true);

    QJsonObject root;
    root.insert(QStringLiteral("inbound"), QJsonArray{inbound});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QVERIFY(config.routeOnly);
}

void JsonConfigRepositoryTests::savePersistsRouteOnlyToInbound()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.routeOnly = true;

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QVERIFY(reloaded.routeOnly);
}

void JsonConfigRepositoryTests::loadDefaultsEnableFragmentToFalseWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(!config.enableFragment);
}

void JsonConfigRepositoryTests::loadReadsEnableFragmentFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("enableFragment"), true);

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QVERIFY(config.enableFragment);
}

void JsonConfigRepositoryTests::savePersistsEnableFragmentToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.enableFragment = true;

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QVERIFY(reloaded.enableFragment);
}

void JsonConfigRepositoryTests::loadDefaultsEnableCacheFile4SboxToTrueWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(config.enableCacheFile4Sbox);
}

void JsonConfigRepositoryTests::loadReadsEnableCacheFile4SboxFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("enableCacheFile4Sbox"), false);

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QVERIFY(!config.enableCacheFile4Sbox);
}

void JsonConfigRepositoryTests::savePersistsEnableCacheFile4SboxToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.enableCacheFile4Sbox = false;

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QVERIFY(!reloaded.enableCacheFile4Sbox);
}

void JsonConfigRepositoryTests::loadDefaultsDefaultUserAgentToEmptyWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(config.defaultUserAgent.isEmpty());
}

void JsonConfigRepositoryTests::loadReadsDefaultUserAgentFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("defUserAgent"), QStringLiteral("Mozilla/5.0"));

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.defaultUserAgent, QStringLiteral("Mozilla/5.0"));
}

void JsonConfigRepositoryTests::savePersistsDefaultUserAgentToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.defaultUserAgent = QStringLiteral("Mozilla/5.0");

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.defaultUserAgent, QStringLiteral("Mozilla/5.0"));
}

void JsonConfigRepositoryTests::loadDefaultsDefaultFingerprintToEmptyWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(config.defaultFingerprint.isEmpty());
}

void JsonConfigRepositoryTests::loadReadsDefaultFingerprintFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("defFingerprint"), QStringLiteral("firefox"));

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.defaultFingerprint, QStringLiteral("firefox"));
}

void JsonConfigRepositoryTests::savePersistsDefaultFingerprintToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.defaultFingerprint = QStringLiteral("firefox");

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.defaultFingerprint, QStringLiteral("firefox"));
}

void JsonConfigRepositoryTests::loadReadsDomainStrategy4SingboxFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("domainStrategy4Singbox"), QStringLiteral("prefer_ipv6"));

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.domainStrategy4Singbox, QStringLiteral("prefer_ipv6"));
}

void JsonConfigRepositoryTests::savePersistsDomainStrategy4SingboxToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.domainStrategy4Singbox = QStringLiteral("ipv4_only");

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.domainStrategy4Singbox, QStringLiteral("ipv4_only"));
}

void JsonConfigRepositoryTests::loadDefaultsSimpleDnsBaseFieldsWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QCOMPARE(config.directDns, QStringLiteral("https://dns.alidns.com/dns-query"));
    QCOMPARE(config.remoteDns, QStringLiteral("https://cloudflare-dns.com/dns-query"));
    QCOMPARE(config.bootstrapDns, QStringLiteral("223.5.5.5"));
    QVERIFY(!config.fakeIp);
    QVERIFY(config.globalFakeIp);
    QVERIFY(config.domainStrategyForFreedom.isEmpty());
    QVERIFY(config.domainStrategyForProxy.isEmpty());
    QVERIFY(config.dnsHosts.isEmpty());
}

void JsonConfigRepositoryTests::loadReadsSimpleDnsBaseFieldsFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("directDNS"), QStringLiteral("https://dns.alidns.com/dns-query,223.5.5.5"));
    root.insert(QStringLiteral("remoteDNS"), QStringLiteral("https://dns.google/dns-query,8.8.8.8"));
    root.insert(QStringLiteral("bootstrapDNS"), QStringLiteral("119.29.29.29"));
    root.insert(QStringLiteral("fakeIP"), true);
    root.insert(QStringLiteral("globalFakeIp"), false);
    root.insert(QStringLiteral("domainStrategy4Freedom"), QStringLiteral("UseIPv4"));
    root.insert(QStringLiteral("domainStrategy4Proxy"), QStringLiteral("UseIPv6"));
    root.insert(QStringLiteral("hosts"), QStringLiteral("example.com 1.2.3.4"));

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.directDns, QStringLiteral("https://dns.alidns.com/dns-query,223.5.5.5"));
    QCOMPARE(config.remoteDns, QStringLiteral("https://dns.google/dns-query,8.8.8.8"));
    QCOMPARE(config.bootstrapDns, QStringLiteral("119.29.29.29"));
    QVERIFY(config.fakeIp);
    QVERIFY(!config.globalFakeIp);
    QCOMPARE(config.domainStrategyForFreedom, QStringLiteral("UseIPv4"));
    QCOMPARE(config.domainStrategyForProxy, QStringLiteral("UseIPv6"));
    QCOMPARE(config.dnsHosts, QStringLiteral("example.com 1.2.3.4"));
}

void JsonConfigRepositoryTests::savePersistsSimpleDnsBaseFieldsToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.directDns = QStringLiteral("223.5.5.5");
    config.remoteDns = QStringLiteral("8.8.8.8");
    config.bootstrapDns = QStringLiteral("119.29.29.29");
    config.fakeIp = true;
    config.globalFakeIp = false;
    config.domainStrategyForFreedom = QStringLiteral("UseIPv4");
    config.domainStrategyForProxy = QStringLiteral("UseIPv6");
    config.dnsHosts = QStringLiteral("example.com 1.2.3.4");

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.directDns, QStringLiteral("223.5.5.5"));
    QCOMPARE(reloaded.remoteDns, QStringLiteral("8.8.8.8"));
    QCOMPARE(reloaded.bootstrapDns, QStringLiteral("119.29.29.29"));
    QVERIFY(reloaded.fakeIp);
    QVERIFY(!reloaded.globalFakeIp);
    QCOMPARE(reloaded.domainStrategyForFreedom, QStringLiteral("UseIPv4"));
    QCOMPARE(reloaded.domainStrategyForProxy, QStringLiteral("UseIPv6"));
    QCOMPARE(reloaded.dnsHosts, QStringLiteral("example.com 1.2.3.4"));
}

void JsonConfigRepositoryTests::loadDefaultsSimpleDnsHostsFlagsWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(config.addCommonHosts);
    QVERIFY(config.blockBindingQuery);
}

void JsonConfigRepositoryTests::loadReadsSimpleDnsHostsFlagsFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("addCommonHosts"), false);
    root.insert(QStringLiteral("blockBindingQuery"), false);

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QVERIFY(!config.addCommonHosts);
    QVERIFY(!config.blockBindingQuery);
}

void JsonConfigRepositoryTests::savePersistsSimpleDnsHostsFlagsToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.addCommonHosts = false;
    config.blockBindingQuery = false;

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QVERIFY(!reloaded.addCommonHosts);
    QVERIFY(!reloaded.blockBindingQuery);
}

void JsonConfigRepositoryTests::loadDefaultsUseSystemHostsToFalseWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(!config.useSystemHosts);
}

void JsonConfigRepositoryTests::loadReadsUseSystemHostsFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("useSystemHosts"), true);

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QVERIFY(config.useSystemHosts);
}

void JsonConfigRepositoryTests::savePersistsUseSystemHostsToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.useSystemHosts = true;

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QVERIFY(reloaded.useSystemHosts);
}

void JsonConfigRepositoryTests::loadDefaultsServeStaleAndParallelQueryToFalseWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(!config.serveStale);
    QVERIFY(!config.parallelQuery);
}

void JsonConfigRepositoryTests::loadReadsServeStaleAndParallelQueryFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("serveStale"), true);
    root.insert(QStringLiteral("enableParallelQuery"), true);

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QVERIFY(config.serveStale);
    QVERIFY(config.parallelQuery);
}

void JsonConfigRepositoryTests::savePersistsServeStaleAndParallelQueryToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.serveStale = true;
    config.parallelQuery = true;

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QVERIFY(reloaded.serveStale);
    QVERIFY(reloaded.parallelQuery);
}

void JsonConfigRepositoryTests::loadDefaultsDirectExpectedIpsToEmptyWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(config.directExpectedIps.isEmpty());
}

void JsonConfigRepositoryTests::loadReadsDirectExpectedIpsFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("directExpectedIPs"), QStringLiteral("geoip:cn,1.2.3.0/24"));

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.directExpectedIps, QStringLiteral("geoip:cn,1.2.3.0/24"));
}

void JsonConfigRepositoryTests::savePersistsDirectExpectedIpsToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.directExpectedIps = QStringLiteral("geoip:cn,1.2.3.0/24");

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.directExpectedIps, QStringLiteral("geoip:cn,1.2.3.0/24"));
}

void JsonConfigRepositoryTests::loadDefaultsMux4SboxProtocolToH2muxWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QCOMPARE(config.mux4SboxProtocol, QStringLiteral("h2mux"));
}

void JsonConfigRepositoryTests::loadReadsMux4SboxProtocolFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("mux4SboxProtocol"), QStringLiteral("yamux"));

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.mux4SboxProtocol, QStringLiteral("yamux"));
}

void JsonConfigRepositoryTests::savePersistsMux4SboxProtocolToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.mux4SboxProtocol = QStringLiteral("smux");

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.mux4SboxProtocol, QStringLiteral("smux"));
}

void JsonConfigRepositoryTests::loadDefaultsMux4SboxMaxConnectionsToEightWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QCOMPARE(config.mux4SboxMaxConnections, 8);
}

void JsonConfigRepositoryTests::loadLeavesMux4SboxPaddingUnsetWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(!config.mux4SboxPadding.has_value());
}

void JsonConfigRepositoryTests::loadReadsMux4SboxMaxConnectionsAndPaddingFromRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("mux4SboxMaxConnections"), 3);
    root.insert(QStringLiteral("mux4SboxPadding"), true);

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.mux4SboxMaxConnections, 3);
    QVERIFY(config.mux4SboxPadding.has_value());
    QCOMPARE(config.mux4SboxPadding.value(), true);
}

void JsonConfigRepositoryTests::savePersistsMux4SboxMaxConnectionsAndPaddingToRoot()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.mux4SboxMaxConnections = 16;
    config.mux4SboxPadding = false;

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.mux4SboxMaxConnections, 16);
    QVERIFY(reloaded.mux4SboxPadding.has_value());
    QCOMPARE(reloaded.mux4SboxPadding.value(), false);
}

void JsonConfigRepositoryTests::loadLeavesServerMuxOverrideUnsetWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject server;
    server.insert(QStringLiteral("address"), QStringLiteral("example.com"));
    server.insert(QStringLiteral("port"), 443);
    server.insert(QStringLiteral("id"), QStringLiteral("11111111-1111-1111-1111-111111111111"));

    QJsonObject root;
    root.insert(QStringLiteral("vmess"), QJsonArray{server});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.servers.size(), 1);
    QVERIFY(!config.servers.constFirst().muxEnabled.has_value());
}

void JsonConfigRepositoryTests::loadReadsServerMuxOverrideWhenPresent()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject server;
    server.insert(QStringLiteral("address"), QStringLiteral("example.com"));
    server.insert(QStringLiteral("port"), 443);
    server.insert(QStringLiteral("id"), QStringLiteral("11111111-1111-1111-1111-111111111111"));
    server.insert(QStringLiteral("muxEnabled"), false);

    QJsonObject root;
    root.insert(QStringLiteral("vmess"), QJsonArray{server});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.servers.size(), 1);
    QVERIFY(config.servers.constFirst().muxEnabled.has_value());
    QCOMPARE(config.servers.constFirst().muxEnabled.value(), false);
}

void JsonConfigRepositoryTests::savePersistsServerMuxOverrideToServerArray()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    VmessItem server;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.muxEnabled = true;
    config.servers.append(server);

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.servers.size(), 1);
    QVERIFY(reloaded.servers.constFirst().muxEnabled.has_value());
    QCOMPARE(reloaded.servers.constFirst().muxEnabled.value(), true);
}

void JsonConfigRepositoryTests::loadReadsServerFinalmask()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject server;
    server.insert(QStringLiteral("address"), QStringLiteral("example.com"));
    server.insert(QStringLiteral("port"), 443);
    server.insert(QStringLiteral("id"), QStringLiteral("11111111-1111-1111-1111-111111111111"));
    server.insert(QStringLiteral("finalmask"), QStringLiteral("{\"udp\":[{\"type\":\"mkcp-original\"}]}"));

    QJsonObject root;
    root.insert(QStringLiteral("vmess"), QJsonArray{server});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers.constFirst().finalmask, QStringLiteral("{\"udp\":[{\"type\":\"mkcp-original\"}]}"));
}

void JsonConfigRepositoryTests::savePersistsServerFinalmask()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    VmessItem server;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.finalmask = QStringLiteral("{\"udp\":[{\"type\":\"mkcp-original\"}]}");
    config.servers.append(server);

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.servers.size(), 1);
    QCOMPARE(reloaded.servers.constFirst().finalmask, QStringLiteral("{\"udp\":[{\"type\":\"mkcp-original\"}]}"));
}

void JsonConfigRepositoryTests::loadReadsServerCert()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject server;
    server.insert(QStringLiteral("address"), QStringLiteral("example.com"));
    server.insert(QStringLiteral("port"), 443);
    server.insert(QStringLiteral("id"), QStringLiteral("11111111-1111-1111-1111-111111111111"));
    server.insert(
        QStringLiteral("cert"),
        QStringLiteral(
            "-----BEGIN CERTIFICATE-----\n"
            "CERT-ONE\n"
            "-----END CERTIFICATE-----\n"
            "-----BEGIN CERTIFICATE-----\n"
            "CERT-TWO\n"
            "-----END CERTIFICATE-----"));

    QJsonObject root;
    root.insert(QStringLiteral("vmess"), QJsonArray{server});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(
        config.servers.constFirst().cert,
        QStringLiteral(
            "-----BEGIN CERTIFICATE-----\n"
            "CERT-ONE\n"
            "-----END CERTIFICATE-----\n"
            "-----BEGIN CERTIFICATE-----\n"
            "CERT-TWO\n"
            "-----END CERTIFICATE-----"));
}

void JsonConfigRepositoryTests::savePersistsServerCert()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    VmessItem server;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.cert = QStringLiteral(
        "-----BEGIN CERTIFICATE-----\n"
        "CERT-ONE\n"
        "-----END CERTIFICATE-----\n"
        "-----BEGIN CERTIFICATE-----\n"
        "CERT-TWO\n"
        "-----END CERTIFICATE-----");
    config.servers.append(server);

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.servers.size(), 1);
    QCOMPARE(
        reloaded.servers.constFirst().cert,
        QStringLiteral(
            "-----BEGIN CERTIFICATE-----\n"
            "CERT-ONE\n"
            "-----END CERTIFICATE-----\n"
            "-----BEGIN CERTIFICATE-----\n"
            "CERT-TWO\n"
            "-----END CERTIFICATE-----"));
}

void JsonConfigRepositoryTests::loadReadsServerCertSha()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject server;
    server.insert(QStringLiteral("address"), QStringLiteral("example.com"));
    server.insert(QStringLiteral("port"), 443);
    server.insert(QStringLiteral("id"), QStringLiteral("11111111-1111-1111-1111-111111111111"));
    server.insert(QStringLiteral("certSha"), QStringLiteral("sha256/base64-one,sha256/base64-two"));

    QJsonObject root;
    root.insert(QStringLiteral("vmess"), QJsonArray{server});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers.constFirst().certSha, QStringLiteral("sha256/base64-one,sha256/base64-two"));
}

void JsonConfigRepositoryTests::savePersistsServerCertSha()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    VmessItem server;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.certSha = QStringLiteral("sha256/base64-one,sha256/base64-two");
    config.servers.append(server);

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.servers.size(), 1);
    QCOMPARE(reloaded.servers.constFirst().certSha, QStringLiteral("sha256/base64-one,sha256/base64-two"));
}

void JsonConfigRepositoryTests::loadReadsServerMldsa65Verify()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject server;
    server.insert(QStringLiteral("address"), QStringLiteral("example.com"));
    server.insert(QStringLiteral("port"), 443);
    server.insert(QStringLiteral("id"), QStringLiteral("11111111-1111-1111-1111-111111111111"));
    server.insert(QStringLiteral("mldsa65Verify"), QStringLiteral("mldsa65-public-key"));

    QJsonObject root;
    root.insert(QStringLiteral("vmess"), QJsonArray{server});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers.constFirst().mldsa65Verify, QStringLiteral("mldsa65-public-key"));
}

void JsonConfigRepositoryTests::savePersistsServerMldsa65Verify()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    VmessItem server;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.mldsa65Verify = QStringLiteral("mldsa65-public-key");
    config.servers.append(server);

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.servers.size(), 1);
    QCOMPARE(reloaded.servers.constFirst().mldsa65Verify, QStringLiteral("mldsa65-public-key"));
}

void JsonConfigRepositoryTests::loadReadsServerEchFields()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject server;
    server.insert(QStringLiteral("address"), QStringLiteral("example.com"));
    server.insert(QStringLiteral("port"), 443);
    server.insert(QStringLiteral("id"), QStringLiteral("11111111-1111-1111-1111-111111111111"));
    server.insert(QStringLiteral("echConfigList"), QStringLiteral("ECHCONFIGBASE64"));
    server.insert(QStringLiteral("echForceQuery"), QStringLiteral("half"));

    QJsonObject root;
    root.insert(QStringLiteral("vmess"), QJsonArray{server});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.servers.size(), 1);
    QCOMPARE(config.servers.constFirst().echConfigList, QStringLiteral("ECHCONFIGBASE64"));
    QCOMPARE(config.servers.constFirst().echForceQuery, QStringLiteral("half"));
}

void JsonConfigRepositoryTests::savePersistsServerEchFields()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    VmessItem server;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.id = QStringLiteral("11111111-1111-1111-1111-111111111111");
    server.echConfigList = QStringLiteral("ECHCONFIGBASE64");
    server.echForceQuery = QStringLiteral("half");
    config.servers.append(server);

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.servers.size(), 1);
    QCOMPARE(reloaded.servers.constFirst().echConfigList, QStringLiteral("ECHCONFIGBASE64"));
    QCOMPARE(reloaded.servers.constFirst().echForceQuery, QStringLiteral("half"));
}

void JsonConfigRepositoryTests::loadReadsRoutingRuleNetworkAndProcessFields()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject rule;
    rule.insert(QStringLiteral("type"), QStringLiteral("field"));
    rule.insert(QStringLiteral("outboundTag"), QStringLiteral("proxy"));
    rule.insert(QStringLiteral("network"), QStringLiteral("tcp,udp"));
    rule.insert(QStringLiteral("process"), QJsonArray{
                                              QStringLiteral("self/"),
                                              QStringLiteral("C:/Program Files/Test/test.exe")});

    QJsonObject routing;
    routing.insert(QStringLiteral("remarks"), QStringLiteral("route"));
    routing.insert(QStringLiteral("rules"), QJsonArray{rule});

    QJsonObject root;
    root.insert(QStringLiteral("routings"), QJsonArray{routing});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.routingItems.size(), 1);
    QCOMPARE(config.routingItems.constFirst().rules.size(), 1);
    const RoutingRule loadedRule = config.routingItems.constFirst().rules.constFirst();
    QCOMPARE(loadedRule.network, QStringLiteral("tcp,udp"));
    QCOMPARE(
        loadedRule.process,
        QStringList({QStringLiteral("self/"), QStringLiteral("C:/Program Files/Test/test.exe")}));
}

void JsonConfigRepositoryTests::savePersistsRoutingRuleNetworkAndProcessFields()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    RoutingRule rule;
    rule.type = QStringLiteral("field");
    rule.outboundTag = QStringLiteral("proxy");
    rule.network = QStringLiteral("tcp,udp");
    rule.process = QStringList{QStringLiteral("self/"), QStringLiteral("test.exe")};

    RoutingItem item;
    item.remarks = QStringLiteral("route");
    item.rules = {rule};
    item.locked = true;
    config.routingItems = {item};

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QVERIFY(reloaded.routingItems.size() >= 1);
    const auto it = std::find_if(
        reloaded.routingItems.cbegin(),
        reloaded.routingItems.cend(),
        [](const RoutingItem& item) { return item.remarks == QStringLiteral("route"); });
    QVERIFY(it != reloaded.routingItems.cend());
    QCOMPARE(it->rules.size(), 1);
    const RoutingRule reloadedRule = it->rules.constFirst();
    QCOMPARE(reloadedRule.network, QStringLiteral("tcp,udp"));
    QCOMPARE(reloadedRule.process, QStringList({QStringLiteral("self/"), QStringLiteral("test.exe")}));
}

void JsonConfigRepositoryTests::loadReadsRoutingCustomRules()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject blockRule;
    blockRule.insert(QStringLiteral("type"), QStringLiteral("field"));
    blockRule.insert(QStringLiteral("outboundTag"), QStringLiteral("block"));
    blockRule.insert(QStringLiteral("domain"), QJsonArray{QStringLiteral("geosite:category-ads-all")});

    QJsonObject directRule;
    directRule.insert(QStringLiteral("type"), QStringLiteral("field"));
    directRule.insert(QStringLiteral("outboundTag"), QStringLiteral("direct"));
    directRule.insert(QStringLiteral("ip"), QJsonArray{QStringLiteral("geoip:private")});

    QJsonObject proxyRule;
    proxyRule.insert(QStringLiteral("type"), QStringLiteral("field"));
    proxyRule.insert(QStringLiteral("outboundTag"), QStringLiteral("proxy"));
    proxyRule.insert(QStringLiteral("port"), QStringLiteral("443"));
    proxyRule.insert(QStringLiteral("protocol"), QJsonArray{QStringLiteral("tls")});

    QJsonObject root;
    root.insert(QStringLiteral("routingCustomRules"), QJsonArray{blockRule, directRule, proxyRule});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.routingCustomRules.size(), 3);
    QCOMPARE(config.routingCustomRules.at(0).outboundTag, QStringLiteral("block"));
    QCOMPARE(config.routingCustomRules.at(0).domain, QStringList{QStringLiteral("geosite:category-ads-all")});
    QCOMPARE(config.routingCustomRules.at(1).outboundTag, QStringLiteral("direct"));
    QCOMPARE(config.routingCustomRules.at(1).ip, QStringList{QStringLiteral("geoip:private")});
    QCOMPARE(config.routingCustomRules.at(2).outboundTag, QStringLiteral("proxy"));
    QCOMPARE(config.routingCustomRules.at(2).port, QStringLiteral("443"));
    QCOMPARE(config.routingCustomRules.at(2).protocol, QStringList{QStringLiteral("tls")});
}

void JsonConfigRepositoryTests::savePersistsRoutingCustomRules()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    RoutingRule rule;
    rule.type = QStringLiteral("field");
    rule.outboundTag = QStringLiteral("proxy");
    rule.domain = QStringList{QStringLiteral("domain:openai.com")};
    config.routingCustomRules = {rule};

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.routingCustomRules.size(), 1);
    QCOMPARE(reloaded.routingCustomRules.constFirst().outboundTag, QStringLiteral("proxy"));
    QCOMPARE(reloaded.routingCustomRules.constFirst().domain, QStringList{QStringLiteral("domain:openai.com")});
}

void JsonConfigRepositoryTests::savePersistsSettingsRoutingRuleTabKey()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.settingsRoutingRuleTabKey = QStringLiteral("proxy");

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.settingsRoutingRuleTabKey, QStringLiteral("proxy"));
}

void JsonConfigRepositoryTests::savePreservesUnknownRootFields()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("customPreservedField"), QStringLiteral("keep-me"));
    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    Config config = repository.load();
    QVERIFY(repository.save(config));

    QFile reloadedFile(configPath);
    QVERIFY(reloadedFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonDocument document = QJsonDocument::fromJson(reloadedFile.readAll());
    QVERIFY(document.isObject());
    QCOMPARE(
        document.object().value(QStringLiteral("customPreservedField")).toString(),
        QStringLiteral("keep-me"));
}

void JsonConfigRepositoryTests::saveRemovesDeprecatedDeadConfigKeys()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject uiItem;
    uiItem.insert(QStringLiteral("mainQrPreviewVisible"), true);

    QJsonObject root;
    root.insert(QStringLiteral("keepOlderDedupl"), true);
    root.insert(QStringLiteral("uiItem"), uiItem);

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    Config config = repository.load();
    QVERIFY(repository.save(config));

    QFile reloadedFile(configPath);
    QVERIFY(reloadedFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonDocument document = QJsonDocument::fromJson(reloadedFile.readAll());
    QVERIFY(document.isObject());

    const QJsonObject savedRoot = document.object();
    QVERIFY(!savedRoot.contains(QStringLiteral("keepOlderDedupl")));
    QCOMPARE(savedRoot.value(QStringLiteral("uiItem")).toObject().value(QStringLiteral("mainQrPreviewVisible")).toBool(), true);
}

void JsonConfigRepositoryTests::loadDefaultsMainRuntimeStateToOffWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(!config.mainProxyEnabled);
}

void JsonConfigRepositoryTests::loadReadsMainRuntimeStateFromUiItem()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject uiItem;
    uiItem.insert(QStringLiteral("mainProxyEnabled"), true);

    QJsonObject root;
    root.insert(QStringLiteral("uiItem"), uiItem);

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QVERIFY(config.mainProxyEnabled);
}

void JsonConfigRepositoryTests::loadReadsMainQrPreviewVisibleFromUiItem()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject uiItem;
    uiItem.insert(QStringLiteral("mainQrPreviewVisible"), true);

    QJsonObject root;
    root.insert(QStringLiteral("uiItem"), uiItem);

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QVERIFY(config.mainQrPreviewVisible);
}

void JsonConfigRepositoryTests::savePersistsMainQrPreviewVisible()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config;
    config.mainQrPreviewVisible = true;
    QVERIFY(repository.save(config));

    const Config reloaded = repository.load();
    QVERIFY(reloaded.mainQrPreviewVisible);
}

void JsonConfigRepositoryTests::savePersistsMainRuntimeStateToUiItem()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    config.mainProxyEnabled = true;

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QVERIFY(reloaded.mainProxyEnabled);
}

void JsonConfigRepositoryTests::loadReadsRoutingItemDomainStrategy4Singbox()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject routing;
    routing.insert(QStringLiteral("remarks"), QStringLiteral("route"));
    routing.insert(QStringLiteral("domainStrategy4Singbox"), QStringLiteral("prefer_ipv4"));

    QJsonObject root;
    root.insert(QStringLiteral("routings"), QJsonArray{routing});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.routingItems.size(), 1);
    QCOMPARE(config.routingItems.constFirst().domainStrategy4Singbox, QStringLiteral("prefer_ipv4"));
}

void JsonConfigRepositoryTests::savePersistsRoutingItemDomainStrategy4Singbox()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    RoutingItem item;
    item.remarks = QStringLiteral("route");
    item.locked = true;
    item.domainStrategy4Singbox = QStringLiteral("ipv6_only");
    config.routingItems = {item};

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QVERIFY(reloaded.routingItems.size() >= 1);
    const auto it = std::find_if(
        reloaded.routingItems.cbegin(),
        reloaded.routingItems.cend(),
        [](const RoutingItem& item) { return item.remarks == QStringLiteral("route"); });
    QVERIFY(it != reloaded.routingItems.cend());
    QCOMPARE(it->domainStrategy4Singbox, QStringLiteral("ipv6_only"));
}

void JsonConfigRepositoryTests::loadReadsGlobalHotkeys()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject hotkey;
    hotkey.insert(QStringLiteral("EGlobalHotkey"), static_cast<int>(GlobalHotkeyAction::ShowForm));
    hotkey.insert(QStringLiteral("Control"), true);
    hotkey.insert(QStringLiteral("Alt"), true);
    hotkey.insert(QStringLiteral("Shift"), false);
    hotkey.insert(QStringLiteral("KeyCode"), 65);

    QJsonObject root;
    root.insert(QStringLiteral("GlobalHotkeys"), QJsonArray{hotkey});

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.globalHotkeys.size(), 1);
    const GlobalHotkeyItem item = config.globalHotkeys.constFirst();
    QCOMPARE(item.action, GlobalHotkeyAction::ShowForm);
    QVERIFY(item.control);
    QVERIFY(item.alt);
    QVERIFY(!item.shift);
    QVERIFY(item.keyCode.has_value());
    QCOMPARE(item.keyCode.value(), 65);
}

void JsonConfigRepositoryTests::savePersistsGlobalHotkeys()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    Config config = repository.load();
    GlobalHotkeyItem item;
    item.action = GlobalHotkeyAction::SystemProxySet;
    item.control = true;
    item.alt = false;
    item.shift = true;
    item.keyCode = 91;
    config.globalHotkeys = {item};

    QVERIFY(repository.save(config));

    JsonConfigRepository reloadedRepository(configPath);
    const Config reloaded = reloadedRepository.load();

    QCOMPARE(reloaded.globalHotkeys.size(), 1);
    const GlobalHotkeyItem reloadedItem = reloaded.globalHotkeys.constFirst();
    QCOMPARE(reloadedItem.action, GlobalHotkeyAction::SystemProxySet);
    QVERIFY(reloadedItem.control);
    QVERIFY(!reloadedItem.alt);
    QVERIFY(reloadedItem.shift);
    QVERIFY(reloadedItem.keyCode.has_value());
    QCOMPARE(reloadedItem.keyCode.value(), 91);
}

void JsonConfigRepositoryTests::saveRemovesDeprecatedTls13ConfigKey()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QJsonObject root;
    root.insert(QStringLiteral("enableSecurityProtocolTls13"), true);

    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();
    QVERIFY(repository.save(config));

    QFile reloadedFile(configPath);
    QVERIFY(reloadedFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonDocument document = QJsonDocument::fromJson(reloadedFile.readAll());
    QVERIFY(document.isObject());
    QVERIFY(!document.object().contains(QStringLiteral("enableSecurityProtocolTls13")));
}

void JsonConfigRepositoryTests::loadDefaultsTunIcmpRoutingToRuleWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QCOMPARE(config.tunModeItem.icmpRouting, QStringLiteral("rule"));
}

void JsonConfigRepositoryTests::loadDefaultsTunIcmpRoutingToRuleWhenEmptyObject()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QJsonObject root;
    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QCOMPARE(config.tunModeItem.icmpRouting, QStringLiteral("rule"));
}

void JsonConfigRepositoryTests::loadDefaultsTunEnableIpv6AddressToFalseWhenMissing()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    JsonConfigRepository repository(configPath);

    const Config config = repository.load();

    QVERIFY(!config.tunModeItem.enableIPv6Address);
}

void JsonConfigRepositoryTests::loadDefaultsTunEnableIpv6AddressToFalseWhenEmptyObject()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("guiNConfig.json"));
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QJsonObject root;
    QVERIFY(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) >= 0);
    file.close();

    JsonConfigRepository repository(configPath);
    const Config config = repository.load();

    QVERIFY(!config.tunModeItem.enableIPv6Address);
}

QTEST_MAIN(JsonConfigRepositoryTests)

#include "JsonConfigRepositoryTests.moc"
