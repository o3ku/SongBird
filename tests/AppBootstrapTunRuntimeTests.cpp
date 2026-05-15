#include <QtTest>

#include "app/TunRuntimeState.h"

class AppBootstrapTunRuntimeTests : public QObject {
    Q_OBJECT

private slots:
    void cleanupRequiredWhenCoreStartedWithTun();
    void cleanupSkippedWhenCoreStartedWithoutTun();
    void startAfterTunCleanupRequiresSuccessfulCleanup();
    void postStopActionAfterTunCleanupRequiresSuccessfulCleanup();
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

void AppBootstrapTunRuntimeTests::startAfterTunCleanupRequiresSuccessfulCleanup()
{
    QVERIFY(shouldResumeCoreStartAfterTunCleanup(true, true, false, false));
    QVERIFY(!shouldResumeCoreStartAfterTunCleanup(false, true, false, false));
    QVERIFY(!shouldResumeCoreStartAfterTunCleanup(true, true, true, false));
    QVERIFY(!shouldResumeCoreStartAfterTunCleanup(true, true, false, true));
}

void AppBootstrapTunRuntimeTests::postStopActionAfterTunCleanupRequiresSuccessfulCleanup()
{
    QVERIFY(shouldRunPostStopActionAfterTunCleanup(true, true, false));
    QVERIFY(!shouldRunPostStopActionAfterTunCleanup(false, true, false));
    QVERIFY(!shouldRunPostStopActionAfterTunCleanup(true, false, false));
    QVERIFY(!shouldRunPostStopActionAfterTunCleanup(true, true, true));
}

QTEST_MAIN(AppBootstrapTunRuntimeTests)

#include "AppBootstrapTunRuntimeTests.moc"
