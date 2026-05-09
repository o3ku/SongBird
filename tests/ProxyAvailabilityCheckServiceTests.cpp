#include <QtTest>

#include "services/ProxyAvailabilityCheckService.h"

class ProxyAvailabilityCheckServiceTests : public QObject {
    Q_OBJECT

private slots:
    void checkRejectsTunMode();
};

void ProxyAvailabilityCheckServiceTests::checkRejectsTunMode()
{
    Config config;
    config.localPort = 10808;
    config.tunModeItem.enableTun = true;

    ProxyAvailabilityCheckService service;
    const OperationResult result = service.check(config);

    QVERIFY(!result.success);
    QVERIFY(result.message.contains(QStringLiteral("TUN mode")));
}

QTEST_MAIN(ProxyAvailabilityCheckServiceTests)

#include "ProxyAvailabilityCheckServiceTests.moc"
