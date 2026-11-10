#include <QtTest>

#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>

#include "services/ProxyAvailabilityCheckService.h"

// What used to be here, and why it is gone
// ----------------------------------------
// This file's only case was `checkRejectsTunMode`, which asserted that `check()` refuses when
// `ProxyAvailabilityCheckConfig::tunEnabled` is set. That guard was wrong, and both the guard and
// the field have been removed:
//
//   * every backend's *main* client config adds the HTTP inbound unconditionally --
//     SingBoxCoreBackend::buildInbounds, XrayCoreBackend::buildInbounds and
//     MihomoConfigFragments all append it with no TUN condition;
//   * the TUN sidecar core is built with AuxiliaryTunRouting::RelayToLocalProxy
//     (ClientConfigWriter::generateClientConfigs), i.e. it relays *into* that inbound, so the
//     inbound cannot be absent while TUN is on;
//   * A caller used to work around the guard by hardcoding `tunEnabled = false` -- a
//     caller lying to the service, which is what made the guard look load-bearing.
//
// So there is no longer any "TUN mode is rejected" behaviour to pin. Do not re-add that case: it
// would only be testable by re-adding the field.
//
// What is covered where
// ---------------------
// The success path is covered end to end by EndToEndSmokeTests, which starts a real sing-box core
// and asserts `availabilityResult.success`. The cases below are the fast, deterministic failure
// branches -- the ones that return before any socket work.
//
// Deliberately NOT covered here: the connect-then-wait path. `checkViaHttpProxy` retries
// `waitForConnected` for the full 30 s timeout, because it doubles as a "wait for the inbound to
// come up" readiness wait, so any test of it either takes 30 s or needs a responding server on a
// second thread. That cost is not worth paying for a branch the smoke test already reaches.
class ProxyAvailabilityCheckServiceTests : public QObject {
    Q_OBJECT

private slots:
    void failsImmediatelyForANonPositiveHttpPort();
    void failsImmediatelyWhenTheHttpPortOverflows();
    void reportsTheAvailabilityMessageWhenItFails();
    void injectedTimeoutBoundsTheWaitAgainstASilentProxy();
};

void ProxyAvailabilityCheckServiceTests::failsImmediatelyForANonPositiveHttpPort()
{
    ProxyAvailabilityCheckConfig config;
    config.httpPort = 0;

    ProxyAvailabilityCheckService service;

    QElapsedTimer timer;
    timer.start();
    const OperationResult result = service.check(config);
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY(!result.success);
    QVERIFY(!result.message.isEmpty());
    // The point of the case is that it short-circuits: an invalid port must be rejected before any
    // socket is opened, otherwise the caller waits out the 30 s timeout for a config that can
    // never work.
    QVERIFY2(elapsedMs < 5000, qPrintable(QStringLiteral("took %1 ms").arg(elapsedMs)));
}

void ProxyAvailabilityCheckServiceTests::failsImmediatelyWhenTheHttpPortOverflows()
{
    ProxyAvailabilityCheckConfig config;
    // The service no longer derives `localPort + 1` itself, so the overflow is asserted on the
    // value it is actually given rather than through a base port that used to overflow.
    config.httpPort = 70000;

    ProxyAvailabilityCheckService service;

    QElapsedTimer timer;
    timer.start();
    const OperationResult result = service.check(config);
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY(!result.success);
    QVERIFY(!result.message.isEmpty());
    QVERIFY2(elapsedMs < 5000, qPrintable(QStringLiteral("took %1 ms").arg(elapsedMs)));
}

void ProxyAvailabilityCheckServiceTests::reportsTheAvailabilityMessageWhenItFails()
{
    ProxyAvailabilityCheckConfig config;
    config.httpPort = 0;

    ProxyAvailabilityCheckService service;
    const OperationResult result = service.check(config);

    // The message is what the caller shows and logs, so it has to stay the availability sentence
    // and not drift into something a caller would have to parse.
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("Availability check")));
}


// The timeout field exists so a test (and the memory soak) can drive the
// wait-for-a-response path in milliseconds; the production default stays 30 s.
// A silent "proxy" -- one that accepts the TCP connection and then says
// nothing -- is the state the health watch sees against a dead node, and this
// case pins both that the wait is bounded by the injected value and that the
// result is the availability failure rather than an exception-shaped error.
void ProxyAvailabilityCheckServiceTests::injectedTimeoutBoundsTheWaitAgainstASilentProxy()
{
    QTcpServer silentProxy;
    QVERIFY(silentProxy.listen(QHostAddress::LocalHost, 0));
    QList<QTcpSocket*> swallowed;
    QObject::connect(&silentProxy, &QTcpServer::newConnection, &silentProxy, [&]() {
        while (silentProxy.hasPendingConnections()) {
            QTcpSocket* socket = silentProxy.nextPendingConnection();
            swallowed.append(socket);
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
                socket->readAll(); // accept and swallow, never answer
            });
        }
    });

    ProxyAvailabilityCheckConfig config;
    config.httpPort = silentProxy.serverPort();
    config.speedPingTestUrl = QStringLiteral("http://www.google.com/generate_204");
    config.timeoutMs = 250;

    const ProxyAvailabilityCheckService service;
    QElapsedTimer timer;
    timer.start();
    const OperationResult result = service.check(config);
    const qint64 elapsedMs = timer.elapsed();

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("Availability check")));
    QVERIFY2(elapsedMs >= 200 && elapsedMs < 5000,
             qPrintable(QStringLiteral("took %1 ms").arg(elapsedMs)));

    for (QTcpSocket* socket : swallowed) {
        socket->abort();
        delete socket;
    }
}
QTEST_MAIN(ProxyAvailabilityCheckServiceTests)

#include "ProxyAvailabilityCheckServiceTests.moc"
