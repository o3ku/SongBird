#include <QtTest>

#include <QElapsedTimer>

#include "services/ProxyAvailabilityCheckService.h"

// What used to be here, and why it is gone
// ----------------------------------------
// This file's only case was `checkRejectsTunMode`, which asserted that `check()` refuses when
// `ProxyAvailabilityCheckConfig::tunEnabled` is set. That guard was wrong, and both the guard and
// the field have been removed:
//
//   * every backend's *main* client config adds the HTTP inbound unconditionally at
//     `localPort + 1` -- SingBoxCoreBackend::buildInbounds, XrayCoreBackend::buildInbounds and
//     MihomoConfigFragments all append it with no TUN condition;
//   * the TUN sidecar core is built with AuxiliaryTunRouting::RelayToLocalProxy
//     (ClientConfigWriter::generateClientConfigs), i.e. it relays *into* that inbound, so the
//     inbound cannot be absent while TUN is on;
//   * SongBirdAutoCoordinator worked around the guard by hardcoding `tunEnabled = false` -- a
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
    void failsImmediatelyForANonPositiveLocalPort();
    void failsImmediatelyWhenTheDerivedHttpPortOverflows();
    void reportsTheAvailabilityMessageWhenItFails();
};

void ProxyAvailabilityCheckServiceTests::failsImmediatelyForANonPositiveLocalPort()
{
    ProxyAvailabilityCheckConfig config;
    config.localPort = 0;

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

void ProxyAvailabilityCheckServiceTests::failsImmediatelyWhenTheDerivedHttpPortOverflows()
{
    ProxyAvailabilityCheckConfig config;
    config.localPort = 65535; // localPort + 1 is out of range

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
    config.localPort = 0;

    ProxyAvailabilityCheckService service;
    const OperationResult result = service.check(config);

    // The message is what the caller shows and logs, so it has to stay the availability sentence
    // and not drift into something a caller would have to parse.
    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("Availability check")));
}

QTEST_MAIN(ProxyAvailabilityCheckServiceTests)

#include "ProxyAvailabilityCheckServiceTests.moc"
