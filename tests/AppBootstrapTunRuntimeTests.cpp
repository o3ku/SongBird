#include <QtTest>

#include "app/TunRuntimeState.h"

class AppBootstrapTunRuntimeTests : public QObject {
    Q_OBJECT

private slots:
    void cleanupRequiredWhenCoreStartedWithTun();
    void cleanupSkippedWhenCoreStartedWithoutTun();
};

void AppBootstrapTunRuntimeTests::cleanupRequiredWhenCoreStartedWithTun()
{
    QVERIFY(shouldCleanupTunAfterCoreStop(true, true));
    QVERIFY(!shouldCleanupTunAfterCoreStop(false, true));
}

void AppBootstrapTunRuntimeTests::cleanupSkippedWhenCoreStartedWithoutTun()
{
    QVERIFY(!shouldCleanupTunAfterCoreStop(true, false));
    QVERIFY(!shouldCleanupTunAfterCoreStop(false, false));
}

QTEST_MAIN(AppBootstrapTunRuntimeTests)

#include "AppBootstrapTunRuntimeTests.moc"
