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
    config.enableStatistics = true;
    config.logEnabled = true;
    config.tunModeItem.enableTun = true;
    config.enableCacheFile4Sbox = true;
    config.sniffingEnabled = true;
    config.routeOnly = true;
    config.enableRoutingAdvanced = true;
    config.routingIndex = 3;
    config.routingItems = {RoutingItem{}};
    config.routingCustomRules = {RoutingRule{}};
    config.remoteDns = QStringLiteral("https://dns.google/dns-query");
    config.directDns = QStringLiteral("https://dns.alidns.com/dns-query");
    config.bootstrapDns = QStringLiteral("223.5.5.5");
    config.dnsHosts = QStringLiteral("example.com 1.1.1.1");
    config.addCommonHosts = true;
    config.useSystemHosts = true;
    config.fakeIp = true;
    config.globalFakeIp = true;
    config.blockBindingQuery = true;
    config.parallelQuery = true;
    config.directExpectedIps = QStringLiteral("geoip:private");

    const Config runtimeConfig = SpeedTestServiceInternal::makeUrlTestRuntimeConfig(config);

    QVERIFY(!runtimeConfig.allowLanConnection);
    QVERIFY(!runtimeConfig.enableStatistics);
    QVERIFY(!runtimeConfig.logEnabled);
    QVERIFY(!runtimeConfig.tunModeItem.enableTun);
    QVERIFY(!runtimeConfig.enableCacheFile4Sbox);
    QVERIFY(runtimeConfig.sniffingEnabled);
    QVERIFY(runtimeConfig.routeOnly);
    QVERIFY(runtimeConfig.enableRoutingAdvanced);
    QCOMPARE(runtimeConfig.routingIndex, 3);
    QCOMPARE(runtimeConfig.routingItems.size(), 1);
    QCOMPARE(runtimeConfig.routingCustomRules.size(), 1);
    QCOMPARE(runtimeConfig.remoteDns, QStringLiteral("https://dns.google/dns-query"));
    QCOMPARE(runtimeConfig.directDns, QStringLiteral("https://dns.alidns.com/dns-query"));
    QCOMPARE(runtimeConfig.bootstrapDns, QStringLiteral("223.5.5.5"));
    QCOMPARE(runtimeConfig.dnsHosts, QStringLiteral("example.com 1.1.1.1"));
    QVERIFY(runtimeConfig.addCommonHosts);
    QVERIFY(runtimeConfig.useSystemHosts);
    QVERIFY(runtimeConfig.fakeIp);
    QVERIFY(runtimeConfig.globalFakeIp);
    QVERIFY(runtimeConfig.blockBindingQuery);
    QVERIFY(runtimeConfig.parallelQuery);
    QCOMPARE(runtimeConfig.directExpectedIps, QStringLiteral("geoip:private"));
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
