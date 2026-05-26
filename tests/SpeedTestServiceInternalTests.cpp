#include <QtTest>

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>

#include "services/SpeedTestBatchConfig.h"
#include "services/SpeedTestPortReservation.h"
#include "services/SpeedTestRuntimeProcess.h"
#include "services/SpeedTestServiceInternal.h"
#include "services/SpeedTestUrlProbe.h"

class SpeedTestServiceInternalTests : public QObject {
    Q_OBJECT

private slots:
    void detectReadyProxyPrefersHttpWhenBothPortsAreReady();
    void detectReadyProxyAcceptsSocksWhenHttpIsNotReady();
    void detectReadyProxyReturnsNulloptWhenNoPortIsReady();
    void reserveProxyPortsRejectsOverlapUntilReleased();
    void takeAvailableProxyPortsReservesDistinctPortsUntilReleased();
    void makeUrlTestRuntimeConfigKeepsRoutingAndDnsBehavior();
    void extractBatchPrimaryOutboundDetectsLegacyAndSingBox();
    void assembleBatchConfigsBuildExpectedInboundAndRoutingShape();
    void classifyUrlProbeResultFormatsAccessibleLatency();
    void classifyUrlProbeResultFormatsTimeout();
    void classifyUrlProbeResultFormatsBlockedFailure();
    void tryParseUrlProbeLatencyAcceptsLegacyAndAccessibleFormats();
    void normalizeUrlProbeErrorUsesStableShortMessages();
    void upstreamSocksIpLiteralDoesNotReportHostNotFound();
    void retryUrlProbeOnlyForTransientFailures();
    void summarizeProcessOutputKeepsStableShortPreview();
    void buildCoreArgumentsReplacesPlaceholderAndAppendsConfigPath();
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

void SpeedTestServiceInternalTests::takeAvailableProxyPortsReservesDistinctPortsUntilReleased()
{
    SpeedTestServiceInternal::resetGlobalState();

    const SpeedTestPortReservation::Ports ports = SpeedTestPortReservation::takeAvailable();
    QVERIFY(ports.socksPort > 0);
    QVERIFY(ports.httpPort > 0);
    QVERIFY(ports.locationProbePort > 0);
    QVERIFY(ports.socksPort != ports.httpPort);
    QVERIFY(ports.socksPort != ports.locationProbePort);
    QVERIFY(ports.httpPort != ports.locationProbePort);
    QVERIFY(!SpeedTestServiceInternal::reserveProxyPorts(
        ports.socksPort,
        ports.httpPort,
        ports.locationProbePort));

    SpeedTestPortReservation::release(ports);
    QVERIFY(SpeedTestServiceInternal::reserveProxyPorts(
        ports.socksPort,
        ports.httpPort,
        ports.locationProbePort));
    SpeedTestPortReservation::release(ports);
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

void SpeedTestServiceInternalTests::extractBatchPrimaryOutboundDetectsLegacyAndSingBox()
{
    QJsonObject legacyOutbound;
    legacyOutbound.insert(QStringLiteral("protocol"), QStringLiteral("vmess"));
    QJsonArray legacyOutbounds;
    legacyOutbounds.append(legacyOutbound);
    QJsonObject legacyRoot;
    legacyRoot.insert(QStringLiteral("outbounds"), legacyOutbounds);

    const auto legacyExtracted = SpeedTestBatchConfig::extractPrimaryOutbound(legacyRoot);
    QVERIFY(legacyExtracted.has_value());
    QVERIFY(legacyExtracted->first == SpeedTestBatchConfig::BatchFlavor::Legacy);
    QCOMPARE(legacyExtracted->second.value(QStringLiteral("protocol")).toString(), QStringLiteral("vmess"));

    QJsonObject singBoxOutbound;
    singBoxOutbound.insert(QStringLiteral("type"), QStringLiteral("vless"));
    QJsonArray singBoxOutbounds;
    singBoxOutbounds.append(singBoxOutbound);
    QJsonObject singBoxRoot;
    singBoxRoot.insert(QStringLiteral("outbounds"), singBoxOutbounds);

    const auto singBoxExtracted = SpeedTestBatchConfig::extractPrimaryOutbound(singBoxRoot);
    QVERIFY(singBoxExtracted.has_value());
    QVERIFY(singBoxExtracted->first == SpeedTestBatchConfig::BatchFlavor::SingBox);
    QCOMPARE(singBoxExtracted->second.value(QStringLiteral("type")).toString(), QStringLiteral("vless"));
}

void SpeedTestServiceInternalTests::assembleBatchConfigsBuildExpectedInboundAndRoutingShape()
{
    SpeedTestBatchConfig::ProbeEntry entry;
    entry.socksPort = 55080;
    entry.inboundTag = QStringLiteral("socks_in_0");
    entry.outboundTag = QStringLiteral("proxy_0");
    entry.outbound.insert(QStringLiteral("tag"), entry.outboundTag);
    entry.outbound.insert(QStringLiteral("protocol"), QStringLiteral("vmess"));

    const QJsonObject legacyRoot = SpeedTestBatchConfig::assembleLegacyConfig({entry});
    const QJsonObject legacyInbound =
        legacyRoot.value(QStringLiteral("inbounds")).toArray().first().toObject();
    QCOMPARE(legacyInbound.value(QStringLiteral("protocol")).toString(), QStringLiteral("socks"));
    QCOMPARE(legacyInbound.value(QStringLiteral("listen")).toString(), QStringLiteral("127.0.0.1"));
    QCOMPARE(legacyInbound.value(QStringLiteral("port")).toInt(), 55080);
    QCOMPARE(legacyRoot.value(QStringLiteral("outbounds")).toArray().size(), 3);

    const QJsonObject legacyRule =
        legacyRoot.value(QStringLiteral("routing")).toObject()
            .value(QStringLiteral("rules")).toArray().first().toObject();
    QCOMPARE(
        legacyRule.value(QStringLiteral("inboundTag")).toArray().first().toString(),
        QStringLiteral("socks_in_0"));
    QCOMPARE(legacyRule.value(QStringLiteral("outboundTag")).toString(), QStringLiteral("proxy_0"));

    entry.outbound = QJsonObject{};
    entry.outbound.insert(QStringLiteral("tag"), entry.outboundTag);
    entry.outbound.insert(QStringLiteral("type"), QStringLiteral("vless"));

    const QJsonObject singBoxRoot = SpeedTestBatchConfig::assembleSingBoxConfig({entry});
    const QJsonObject singBoxInbound =
        singBoxRoot.value(QStringLiteral("inbounds")).toArray().first().toObject();
    QCOMPARE(singBoxInbound.value(QStringLiteral("type")).toString(), QStringLiteral("socks"));
    QCOMPARE(singBoxInbound.value(QStringLiteral("listen_port")).toInt(), 55080);
    QCOMPARE(singBoxRoot.value(QStringLiteral("outbounds")).toArray().size(), 2);

    const QJsonObject singBoxRule =
        singBoxRoot.value(QStringLiteral("route")).toObject()
            .value(QStringLiteral("rules")).toArray().first().toObject();
    QCOMPARE(
        singBoxRule.value(QStringLiteral("inbound")).toArray().first().toString(),
        QStringLiteral("socks_in_0"));
    QCOMPARE(singBoxRule.value(QStringLiteral("outbound")).toString(), QStringLiteral("proxy_0"));
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

void SpeedTestServiceInternalTests::normalizeUrlProbeErrorUsesStableShortMessages()
{
    QCOMPARE(SpeedTestUrlProbe::normalizeErrorText(QString{}), QStringLiteral("Failed"));
    QCOMPARE(SpeedTestUrlProbe::normalizeErrorText(QStringLiteral("Operation timed out")), QStringLiteral("Timeout"));
    QCOMPARE(
        SpeedTestUrlProbe::normalizeErrorText(QStringLiteral("Connection reset by peer\nsecond line")),
        QStringLiteral("Connection reset by peer"));

    const QString longError(140, QLatin1Char('x'));
    QCOMPARE(SpeedTestUrlProbe::normalizeErrorText(longError).size(), 96);
}

void SpeedTestServiceInternalTests::upstreamSocksIpLiteralDoesNotReportHostNotFound()
{
    std::atomic_bool cancelled{false};
    VmessItem server;
    server.configType = ConfigType::Socks;
    server.address = QStringLiteral("192.0.2.1");
    server.port = 9;

    const SpeedTestServiceInternal::UrlProbeResult result =
        SpeedTestUrlProbe::probeUpstreamProxyWithRetry(
            server,
            QStringLiteral("https://example.com/"),
            10,
            cancelled);

    if (result.status == SpeedTestServiceInternal::UrlProbeStatus::Failed) {
        QVERIFY(!result.errorText.contains(QStringLiteral("host not found"), Qt::CaseInsensitive));
    }
}

void SpeedTestServiceInternalTests::retryUrlProbeOnlyForTransientFailures()
{
    QVERIFY(SpeedTestUrlProbe::shouldRetry(
        SpeedTestServiceInternal::UrlProbeResult{SpeedTestServiceInternal::UrlProbeStatus::Timeout, -1, QStringLiteral("Timeout")}));
    QVERIFY(SpeedTestUrlProbe::shouldRetry(
        SpeedTestServiceInternal::UrlProbeResult{SpeedTestServiceInternal::UrlProbeStatus::Failed, 120, QStringLiteral("Connection reset")}));
    QVERIFY(!SpeedTestUrlProbe::shouldRetry(
        SpeedTestServiceInternal::UrlProbeResult{SpeedTestServiceInternal::UrlProbeStatus::Failed, 900, QStringLiteral("Connection reset")}));
    QVERIFY(!SpeedTestUrlProbe::shouldRetry(
        SpeedTestServiceInternal::UrlProbeResult{SpeedTestServiceInternal::UrlProbeStatus::Accessible, 50, {}}));
}

void SpeedTestServiceInternalTests::summarizeProcessOutputKeepsStableShortPreview()
{
    QCOMPARE(SpeedTestRuntimeProcess::summarizeProcessOutput(QString{}), QStringLiteral("<no output>"));
    QCOMPARE(
        SpeedTestRuntimeProcess::summarizeProcessOutput(QStringLiteral(" first \n\n second \r\n third \n fourth ")),
        QStringLiteral("first | second | third"));

    const QString longLine(300, QLatin1Char('x'));
    QCOMPARE(SpeedTestRuntimeProcess::summarizeProcessOutput(longLine).size(), 240);
}

void SpeedTestServiceInternalTests::buildCoreArgumentsReplacesPlaceholderAndAppendsConfigPath()
{
    CoreInfo coreInfo;
    coreInfo.arguments = QStringList{
        QStringLiteral("run"),
        QStringLiteral("CONFIG"),
        QStringLiteral("--verbose")
    };
    coreInfo.configPlaceholder = QStringLiteral("CONFIG");
    coreInfo.appendConfigArgument = true;

    const QString normalizedPath = QDir::toNativeSeparators(QStringLiteral("tmp/runtime.json"));
    const QStringList arguments = SpeedTestRuntimeProcess::buildCoreArguments(coreInfo, QStringLiteral("tmp/runtime.json"));

    QCOMPARE(arguments.at(0), QStringLiteral("run"));
    QCOMPARE(arguments.at(1), normalizedPath);
    QCOMPARE(arguments.at(2), QStringLiteral("--verbose"));
    QCOMPARE(arguments.at(3), QStringLiteral("-config"));
    QCOMPARE(arguments.at(4), normalizedPath);
}

QTEST_MAIN(SpeedTestServiceInternalTests)

#include "SpeedTestServiceInternalTests.moc"
