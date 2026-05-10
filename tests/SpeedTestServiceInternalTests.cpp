#include <QtTest>

#include "services/SpeedTestServiceInternal.h"

class SpeedTestServiceInternalTests : public QObject {
    Q_OBJECT

private slots:
    void detectReadyProxyPrefersSocksWhenBothPortsAreReady();
    void detectReadyProxyAcceptsHttpWhenSocksIsNotReady();
    void detectReadyProxyReturnsNulloptWhenNoPortIsReady();
    void makeUrlTestRuntimeConfigDisablesRoutingAndTun();
};

void SpeedTestServiceInternalTests::detectReadyProxyPrefersSocksWhenBothPortsAreReady()
{
    const auto readyProxy = SpeedTestServiceInternal::detectReadyProxy(
        1080,
        1081,
        [](int) { return true; });

    QVERIFY(readyProxy.has_value());
    QCOMPARE(readyProxy->type, QNetworkProxy::Socks5Proxy);
    QCOMPARE(readyProxy->port, 1080);
    QCOMPARE(readyProxy->name, QStringLiteral("socks"));
}

void SpeedTestServiceInternalTests::detectReadyProxyAcceptsHttpWhenSocksIsNotReady()
{
    const auto readyProxy = SpeedTestServiceInternal::detectReadyProxy(
        1080,
        1081,
        [](int port) { return port == 1081; });

    QVERIFY(readyProxy.has_value());
    QCOMPARE(readyProxy->type, QNetworkProxy::HttpProxy);
    QCOMPARE(readyProxy->port, 1081);
    QCOMPARE(readyProxy->name, QStringLiteral("http"));
}

void SpeedTestServiceInternalTests::detectReadyProxyReturnsNulloptWhenNoPortIsReady()
{
    const auto readyProxy = SpeedTestServiceInternal::detectReadyProxy(
        1080,
        1081,
        [](int) { return false; });

    QVERIFY(!readyProxy.has_value());
}

void SpeedTestServiceInternalTests::makeUrlTestRuntimeConfigDisablesRoutingAndTun()
{
    Config config;
    config.allowLanConnection = true;
    config.enableStatistics = true;
    config.logEnabled = true;
    config.tunModeItem.enableTun = true;
    config.enableRoutingAdvanced = true;
    config.routingIndex = 3;
    config.routingItems = {RoutingItem{}};
    config.routingCustomRules = {RoutingRule{}};

    const Config runtimeConfig = SpeedTestServiceInternal::makeUrlTestRuntimeConfig(config);

    QVERIFY(!runtimeConfig.allowLanConnection);
    QVERIFY(!runtimeConfig.enableStatistics);
    QVERIFY(!runtimeConfig.logEnabled);
    QVERIFY(!runtimeConfig.tunModeItem.enableTun);
    QVERIFY(!runtimeConfig.enableCacheFile4Sbox);
    QVERIFY(!runtimeConfig.sniffingEnabled);
    QVERIFY(!runtimeConfig.routeOnly);
    QVERIFY(!runtimeConfig.enableRoutingAdvanced);
    QCOMPARE(runtimeConfig.routingIndex, 0);
    QVERIFY(runtimeConfig.routingItems.isEmpty());
    QVERIFY(runtimeConfig.routingCustomRules.isEmpty());
    QVERIFY(runtimeConfig.remoteDns.isEmpty());
    QVERIFY(runtimeConfig.directDns.isEmpty());
    QVERIFY(runtimeConfig.bootstrapDns.isEmpty());
    QVERIFY(runtimeConfig.dnsHosts.isEmpty());
    QVERIFY(!runtimeConfig.addCommonHosts);
    QVERIFY(!runtimeConfig.useSystemHosts);
    QVERIFY(!runtimeConfig.fakeIp);
    QVERIFY(!runtimeConfig.globalFakeIp);
    QVERIFY(!runtimeConfig.blockBindingQuery);
    QVERIFY(!runtimeConfig.parallelQuery);
    QVERIFY(runtimeConfig.directExpectedIps.isEmpty());
}

QTEST_MAIN(SpeedTestServiceInternalTests)

#include "SpeedTestServiceInternalTests.moc"
