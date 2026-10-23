#include <QtTest>

#include "common/LocalInboundPorts.h"

// The local inbound ports used to be settled independently by every reader, and a config that
// carried a `localHttpPort` override (the Auto front end persists one) had the core bound to one
// port while the system proxy, the health check and the subscription updater all dialled another.
// These cases pin the resolution and the reconciliation that keeps a stale override from surviving
// a `localPort` change.
class LocalInboundPortsTests : public QObject {
    Q_OBJECT

private slots:
    void resolvesHttpPortOneAboveTheSocksPortByDefault();
    void honoursAnHttpPortOverride();
    void resolvesLocationProbePortFromTheSharedOffset();
    void rejectsAnOverrideThatDisagreesWithTheBasePort();
    void keepsAnOverrideThatAgreesWithTheBasePort();
    void derivesBothPortsFromTheEffectiveBasePort();
    void rejectsAnOverrideDerivedFromTheWrongDefault();
};

void LocalInboundPortsTests::resolvesHttpPortOneAboveTheSocksPortByDefault()
{
    Config config;
    config.localPort = 20808;

    QCOMPARE(LocalInboundPorts::httpPort(config), 20809);
}

void LocalInboundPortsTests::honoursAnHttpPortOverride()
{
    Config config;
    config.localPort = 20808;
    config.localHttpPort = 18080;

    QCOMPARE(LocalInboundPorts::httpPort(config), 18080);
}

void LocalInboundPortsTests::resolvesLocationProbePortFromTheSharedOffset()
{
    Config config;
    config.localPort = 10808;
    QCOMPARE(LocalInboundPorts::locationProbePort(config), 10808 + LocalInboundPorts::kLocationProbeOffset);

    config.localLocationProbePort = 12000;
    QCOMPARE(LocalInboundPorts::locationProbePort(config), 12000);
}

void LocalInboundPortsTests::rejectsAnOverrideThatDisagreesWithTheBasePort()
{
    Config config;
    config.localPort = 20808;
    config.localHttpPort = 10809;
    config.localLocationProbePort = 10911;

    LocalInboundPorts::clearStaleOverrides(config);

    QCOMPARE(config.localHttpPort, 0);
    QCOMPARE(config.localLocationProbePort, 0);
    QCOMPARE(LocalInboundPorts::httpPort(config), 20809);
    QCOMPARE(LocalInboundPorts::locationProbePort(config), 20808 + LocalInboundPorts::kLocationProbeOffset);
}

void LocalInboundPortsTests::keepsAnOverrideThatAgreesWithTheBasePort()
{
    Config config;
    config.localPort = 20808;
    config.localHttpPort = 20809;
    config.localLocationProbePort = 20808 + LocalInboundPorts::kLocationProbeOffset;

    LocalInboundPorts::clearStaleOverrides(config);

    QCOMPARE(config.localHttpPort, 20809);
    QCOMPARE(config.localLocationProbePort, 20808 + LocalInboundPorts::kLocationProbeOffset);
}

void LocalInboundPortsTests::derivesBothPortsFromTheEffectiveBasePort()
{
    // An unset port is an unset preference, not a zero to add offsets to. Deriving from the stored
    // value gave `0 + 1` here while the settings dialog and the core config both meant 10808.
    Config config;
    QCOMPARE(LocalInboundPorts::httpPort(config), kDefaultLocalPort + 1);
    QCOMPARE(LocalInboundPorts::locationProbePort(config),
             kDefaultLocalPort + LocalInboundPorts::kLocationProbeOffset);

    config.localPort = -5;
    QCOMPARE(LocalInboundPorts::httpPort(config), kDefaultLocalPort + 1);
}

void LocalInboundPortsTests::rejectsAnOverrideDerivedFromTheWrongDefault()
{
    // The Auto front end used to persist `10808 + 1` next to a `localPort` of 0 -- the same number,
    // written before the base port was normalised. It is stale for the same reason any other
    // mismatch is: it was derived, not chosen.
    Config config;
    config.localPort = 0;
    config.localHttpPort = 18080;

    LocalInboundPorts::clearStaleOverrides(config);

    QCOMPARE(config.localHttpPort, 0);
    QCOMPARE(LocalInboundPorts::httpPort(config), kDefaultLocalPort + 1);
}

QTEST_MAIN(LocalInboundPortsTests)

#include "LocalInboundPortsTests.moc"
