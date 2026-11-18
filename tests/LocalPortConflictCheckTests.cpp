#include <QtTest>

#include <QHostAddress>
#include <QTcpServer>

#include "common/LocalPortConflictCheck.h"

class LocalPortConflictCheckTests : public QObject {
    Q_OBJECT

private slots:
    void reportsFirstOccupiedPortInOrder();
    void derivesPortsFromOverrides();
    void skipsInvalidPortsInsteadOfReportingThem();
    void bindProbeSeesListeningPort();
    void bindProbeRejectsInvalidPorts();
};

void LocalPortConflictCheckTests::reportsFirstOccupiedPortInOrder()
{
    // An unset localPort still resolves to the ports the core would actually bind.
    Config config;
    config.localPort = 0;
    const QList<LocalPortConflictCheck::Port> ports = LocalPortConflictCheck::localInboundPorts(config);
    QCOMPARE(ports.size(), 3);
    QCOMPARE(ports.at(0).port, 10808);
    QCOMPARE(ports.at(0).role, LocalPortConflictCheck::Role::Socks);
    QCOMPARE(ports.at(1).port, 10809);
    QCOMPARE(ports.at(1).role, LocalPortConflictCheck::Role::Http);
    QCOMPARE(ports.at(2).port, 10911);
    QCOMPARE(ports.at(2).role, LocalPortConflictCheck::Role::LocationProbe);

    // The SOCKS port is reported first even when a later port is also taken.
    const LocalPortConflictCheck::Port occupied = LocalPortConflictCheck::firstOccupiedPort(
        ports, [](int port) { return port != 10808 && port != 10809; });
    QCOMPARE(occupied.port, 10808);
    QCOMPARE(occupied.role, LocalPortConflictCheck::Role::Socks);

    // With the SOCKS port free the HTTP port is next, not the probe.
    const LocalPortConflictCheck::Port httpOccupied = LocalPortConflictCheck::firstOccupiedPort(
        ports, [](int port) { return port != 10809; });
    QCOMPARE(httpOccupied.port, 10809);
    QCOMPARE(httpOccupied.role, LocalPortConflictCheck::Role::Http);

    // All free reports port 0, never a role.
    const LocalPortConflictCheck::Port none = LocalPortConflictCheck::firstOccupiedPort(
        ports, [](int) { return true; });
    QCOMPARE(none.port, 0);
}

void LocalPortConflictCheckTests::derivesPortsFromOverrides()
{
    Config config;
    config.localPort = 10808;
    config.localHttpPort = 18080;
    config.localLocationProbePort = 18081;
    const QList<LocalPortConflictCheck::Port> ports = LocalPortConflictCheck::localInboundPorts(config);
    QCOMPARE(ports.at(0).port, 10808);
    QCOMPARE(ports.at(1).port, 18080);
    QCOMPARE(ports.at(2).port, 18081);

    const LocalPortConflictCheck::Port occupied = LocalPortConflictCheck::firstOccupiedPort(
        ports, [](int port) { return port != 18081; });
    QCOMPARE(occupied.port, 18081);
    QCOMPARE(occupied.role, LocalPortConflictCheck::Role::LocationProbe);
}

void LocalPortConflictCheckTests::skipsInvalidPortsInsteadOfReportingThem()
{
    // A hand-built list can hold ports the derivation never produces (0, out of range). They
    // are skipped, never reported as a conflict for a port that does not exist.
    const QList<LocalPortConflictCheck::Port> ports{
        {0, LocalPortConflictCheck::Role::Socks},
        {70000, LocalPortConflictCheck::Role::Http},
        {10809, LocalPortConflictCheck::Role::LocationProbe},
    };
    const LocalPortConflictCheck::Port occupied = LocalPortConflictCheck::firstOccupiedPort(
        ports, [](int) { return false; });
    QCOMPARE(occupied.port, 10809);
    QCOMPARE(occupied.role, LocalPortConflictCheck::Role::LocationProbe);
}

void LocalPortConflictCheckTests::bindProbeSeesListeningPort()
{
    QTcpServer listener;
    QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
    const int occupiedPort = listener.serverPort();
    QVERIFY(occupiedPort > 0);

    QVERIFY(!LocalPortConflictCheck::isLocalTcpPortFree(occupiedPort));

    listener.close();
    QVERIFY(LocalPortConflictCheck::isLocalTcpPortFree(occupiedPort));
}

void LocalPortConflictCheckTests::bindProbeRejectsInvalidPorts()
{
    QVERIFY(!LocalPortConflictCheck::isLocalTcpPortFree(0));
    QVERIFY(!LocalPortConflictCheck::isLocalTcpPortFree(-1));
    QVERIFY(!LocalPortConflictCheck::isLocalTcpPortFree(65536));
}

QTEST_MAIN(LocalPortConflictCheckTests)

#include "LocalPortConflictCheckTests.moc"
