#include <QtTest>

#include "services/SpeedTestServiceInternal.h"

class SpeedTestServiceInternalTests : public QObject {
    Q_OBJECT

private slots:
    void detectReadyProxyPrefersHttpWhenBothPortsAreReady();
    void detectReadyProxyAcceptsSocksWhenHttpIsNotReady();
    void detectReadyProxyReturnsNulloptWhenNoPortIsReady();
    void reserveProxyPortsRejectsOverlapUntilReleased();
    void makeUrlTestRuntimeConfigKeepsRoutingAndDnsBehavior();
    void classifyUrlProbeResultFormatsAccessibleLatency();
    void classifyUrlProbeResultFormatsTimeout();
    void classifyUrlProbeResultFormatsBlockedFailure();
    void tryParseUrlProbeLatencyAcceptsLegacyAndAccessibleFormats();
};

void SpeedTestServiceInternalTests::detectReadyProxyPrefersHttpWhenBothPortsAreReady()
{
    const auto readyProxy = SpeedTestServiceInternal::detectReadyProxy(
        1080,
        1081,
        [](int) { return true; });

    QVERIFY(readyProxy.has_value());
    QCOMPARE(readyProxy->type, QNetworkProxy::HttpProxy);
    QCOMPARE(readyProxy->port, 1081);
    QCOMPARE(readyProxy->name, QStringLiteral("http"));
}

void SpeedTestServiceInternalTests::detectReadyProxyAcceptsSocksWhenHttpIsNotReady()
{
    const auto readyProxy = SpeedTestServiceInternal::detectReadyProxy(
        1080,
        1081,
        [](int port) { return port == 1080; });

    QVERIFY(readyProxy.has_value());
    QCOMPARE(readyProxy->type, QNetworkProxy::Socks5Proxy);
    QCOMPARE(readyProxy->port, 1080);
    QCOMPARE(readyProxy->name, QStringLiteral("socks"));
}

void SpeedTestServiceInternalTests::detectReadyProxyReturnsNulloptWhenNoPortIsReady()
{
    const auto readyProxy = SpeedTestServiceInternal::detectReadyProxy(
        1080,
        1081,
        [](int) { return false; });

    QVERIFY(!readyProxy.has_value());
}

void SpeedTestServiceInternalTests::reserveProxyPortsRejectsOverlapUntilReleased()
{
    SpeedTestServiceInternal::resetGlobalState();

    QVERIFY(SpeedTestServiceInternal::reserveProxyPorts(54910, 54911));
    QVERIFY(!SpeedTestServiceInternal::reserveProxyPorts(54911, 54912));

    SpeedTestServiceInternal::releaseProxyPorts(54910, 54911);

    QVERIFY(SpeedTestServiceInternal::reserveProxyPorts(54911, 54912));

    SpeedTestServiceInternal::releaseProxyPorts(54911, 54912);
}

void SpeedTestServiceInternalTests::makeUrlTestRuntimeConfigKeepsRoutingAndDnsBehavior()
{
    Config config;
    config.allowLanConnection = true;
    config.logEnabled = true;
    config.tun().tunModeItem.enableTun = true;
    config.dns().enableCacheFile4Sbox = true;
    config.sniffingEnabled = true;
    config.routeOnly = true;
    config.collection().enableRoutingAdvanced = true;
    config.collection().routingIndex = 3;
    config.collection().routingItems = {RoutingItem{}};
    config.collection().routingCustomRules = {RoutingRule{}};
    config.dns().remoteDns = QStringLiteral("https://dns.google/dns-query");
    config.dns().directDns = QStringLiteral("https://dns.alidns.com/dns-query");
    config.dns().bootstrapDns = QStringLiteral("223.5.5.5");
    config.dns().dnsHosts = QStringLiteral("example.com 1.1.1.1");
    config.dns().addCommonHosts = true;
    config.dns().useSystemHosts = true;
    config.dns().fakeIp = true;
    config.dns().globalFakeIp = true;
    config.dns().blockBindingQuery = true;
    config.dns().parallelQuery = true;
    config.dns().directExpectedIps = QStringLiteral("geoip:private");
    config.collection().servers = {VmessItem{}};
    config.collection().subscriptions = {SubItem{}};
    config.policy().coreTypeItems = {CoreTypeItem{}};

    const Config runtimeConfig = SpeedTestServiceInternal::makeUrlTestRuntimeConfig(config);

    QVERIFY(!runtimeConfig.allowLanConnection);
    QVERIFY(!runtimeConfig.logEnabled);
    QVERIFY(!runtimeConfig.tun().tunModeItem.enableTun);
    QVERIFY(!runtimeConfig.dns().enableCacheFile4Sbox);
    QVERIFY(runtimeConfig.sniffingEnabled);
    QVERIFY(runtimeConfig.routeOnly);
    QVERIFY(runtimeConfig.collection().enableRoutingAdvanced);
    QCOMPARE(runtimeConfig.collection().routingIndex, 3);
    QCOMPARE(runtimeConfig.collection().routingItems.size(), 1);
    QCOMPARE(runtimeConfig.collection().routingCustomRules.size(), 1);
    QCOMPARE(runtimeConfig.dns().remoteDns, QStringLiteral("https://dns.google/dns-query"));
    QCOMPARE(runtimeConfig.dns().directDns, QStringLiteral("https://dns.alidns.com/dns-query"));
    QCOMPARE(runtimeConfig.dns().bootstrapDns, QStringLiteral("223.5.5.5"));
    QCOMPARE(runtimeConfig.dns().dnsHosts, QStringLiteral("example.com 1.1.1.1"));
    QVERIFY(runtimeConfig.dns().addCommonHosts);
    QVERIFY(runtimeConfig.dns().useSystemHosts);
    QVERIFY(runtimeConfig.dns().fakeIp);
    QVERIFY(runtimeConfig.dns().globalFakeIp);
    QVERIFY(runtimeConfig.dns().blockBindingQuery);
    QVERIFY(runtimeConfig.dns().parallelQuery);
    QCOMPARE(runtimeConfig.dns().directExpectedIps, QStringLiteral("geoip:private"));
    QVERIFY(runtimeConfig.collection().servers.isEmpty());
    QVERIFY(runtimeConfig.collection().subscriptions.isEmpty());
    QVERIFY(runtimeConfig.policy().coreTypeItems.isEmpty());
}

void SpeedTestServiceInternalTests::classifyUrlProbeResultFormatsAccessibleLatency()
{
    const auto result = SpeedTestServiceInternal::classifyUrlProbeResult(true, false, 287, QString{});
    QCOMPARE(result.status, SpeedTestServiceInternal::UrlProbeStatus::Accessible);
    QCOMPARE(result.latencyMs, 287);
    QCOMPARE(SpeedTestServiceInternal::formatUrlProbeResult(result), QStringLiteral("287 ms"));
}

void SpeedTestServiceInternalTests::classifyUrlProbeResultFormatsTimeout()
{
    const auto result = SpeedTestServiceInternal::classifyUrlProbeResult(false, true, -1, QStringLiteral("Operation timed out"));
    QCOMPARE(result.status, SpeedTestServiceInternal::UrlProbeStatus::Timeout);
    QCOMPARE(SpeedTestServiceInternal::formatUrlProbeResult(result), QStringLiteral("Timeout"));
}

void SpeedTestServiceInternalTests::classifyUrlProbeResultFormatsBlockedFailure()
{
    const auto result = SpeedTestServiceInternal::classifyUrlProbeResult(false, false, -1, QStringLiteral("Connection refused"));
    QCOMPARE(result.status, SpeedTestServiceInternal::UrlProbeStatus::Failed);
    QCOMPARE(SpeedTestServiceInternal::formatUrlProbeResult(result), QStringLiteral("Connection refused"));
}

void SpeedTestServiceInternalTests::tryParseUrlProbeLatencyAcceptsLegacyAndAccessibleFormats()
{
    double latencyMs = 0.0;
    QVERIFY(SpeedTestServiceInternal::tryParseUrlProbeLatency(QStringLiteral("12 ms"), latencyMs));
    QCOMPARE(latencyMs, 12.0);

    QVERIFY(SpeedTestServiceInternal::tryParseUrlProbeLatency(QStringLiteral("Accessible 34 ms"), latencyMs));
    QCOMPARE(latencyMs, 34.0);

    QVERIFY(!SpeedTestServiceInternal::tryParseUrlProbeLatency(QStringLiteral("Blocked: reset"), latencyMs));
    QVERIFY(!SpeedTestServiceInternal::tryParseUrlProbeLatency(QStringLiteral("Timeout"), latencyMs));
}

QTEST_MAIN(SpeedTestServiceInternalTests)

#include "SpeedTestServiceInternalTests.moc"
