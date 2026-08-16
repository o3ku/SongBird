#include <QtTest/QtTest>

#include "auto/AutoRuntimeDefaults.h"
#include "app/OutboundLocationProbeService.h"
#include "common/SystemProxyMode.h"

class AutoRuntimeDefaultsTests : public QObject
{
    Q_OBJECT

private slots:
    void forcesAutoProxyRuntimeMode();
    void preservesTunPreference();
    void normalizesPortBase_data();
    void normalizesPortBase();
};

void AutoRuntimeDefaultsTests::forcesAutoProxyRuntimeMode()
{
    Config config;
    config.sysProxyType = toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear);
    config.ui().mainProxyEnabled = false;
    config.tun().tunModeItem.enableTun = true;

    applyAutoRuntimeDefaults(config);

    QCOMPARE(normalizeSystemProxyMode(config.sysProxyType), SystemProxyMode::ForcedChange);
    QVERIFY(config.ui().mainProxyEnabled);
    QVERIFY(config.tun().tunModeItem.enableTun);
}

void AutoRuntimeDefaultsTests::preservesTunPreference()
{
    Config config;
    config.tun().tunModeItem.enableTun = false;

    applyAutoRuntimeDefaults(config);

    QVERIFY(!config.tun().tunModeItem.enableTun);
}

void AutoRuntimeDefaultsTests::normalizesPortBase_data()
{
    QTest::addColumn<int>("inputPort");
    QTest::addColumn<int>("expectedBasePort");

    QTest::newRow("valid-default") << 10808 << 10808;
    QTest::newRow("zero") << 0 << 10808;
    QTest::newRow("negative") << -1 << 10808;
    QTest::newRow("last-location-probe-safe-port") << 65432 << 65432;
    QTest::newRow("too-high-for-location-probe") << 65433 << 10808;
    QTest::newRow("above-tcp-range") << 70000 << 10808;
}

void AutoRuntimeDefaultsTests::normalizesPortBase()
{
    QFETCH(int, inputPort);
    QFETCH(int, expectedBasePort);

    Config config;
    config.localPort = inputPort;
    config.localHttpPort = 1;
    config.localLocationProbePort = 2;

    applyAutoRuntimeDefaults(config);

    QCOMPARE(config.localPort, expectedBasePort);
    QCOMPARE(config.localHttpPort, expectedBasePort + 1);
    QCOMPARE(
        config.localLocationProbePort,
        expectedBasePort + OutboundLocationProbeService::LocationProbePortOffset);
}

QTEST_MAIN(AutoRuntimeDefaultsTests)

#include "AutoRuntimeDefaultsTests.moc"
