#include <QtTest>

#include "common/SystemProxyMode.h"

class SystemProxyModeTests : public QObject {
    Q_OBJECT

private slots:
    void shouldAdoptManagedSystemProxyOnStartupOnlyForPersistedManagedProxy();
    void shouldClearManagedSystemProxyOnlyWhenOwned();
    void resolveSystemProxyModeOnExitKeepsUnchangedMode();
    void resolveSystemProxyModeOnExitClearsManagedModes();
    void resolveSystemProxyModeOnExitClearsOnWindowsShutdown();
};

void SystemProxyModeTests::shouldAdoptManagedSystemProxyOnStartupOnlyForPersistedManagedProxy()
{
    QVERIFY(shouldAdoptManagedSystemProxyOnStartup(
        SystemProxyMode::ForcedChange,
        true,
        true));
    QVERIFY(!shouldAdoptManagedSystemProxyOnStartup(
        SystemProxyMode::ForcedChange,
        false,
        true));
    QVERIFY(!shouldAdoptManagedSystemProxyOnStartup(
        SystemProxyMode::Unchanged,
        true,
        true));
}

void SystemProxyModeTests::shouldClearManagedSystemProxyOnlyWhenOwned()
{
    QVERIFY(shouldClearManagedSystemProxy(true, true));
    QVERIFY(!shouldClearManagedSystemProxy(false, true));
    QVERIFY(!shouldClearManagedSystemProxy(true, false));
}

void SystemProxyModeTests::resolveSystemProxyModeOnExitKeepsUnchangedMode()
{
    QCOMPARE(
        resolveSystemProxyModeOnExit(SystemProxyMode::Unchanged, false),
        SystemProxyMode::Unchanged);
}

void SystemProxyModeTests::resolveSystemProxyModeOnExitClearsManagedModes()
{
    QCOMPARE(
        resolveSystemProxyModeOnExit(SystemProxyMode::ForcedClear, false),
        SystemProxyMode::ForcedClear);
    QCOMPARE(
        resolveSystemProxyModeOnExit(SystemProxyMode::ForcedChange, false),
        SystemProxyMode::ForcedClear);
    QCOMPARE(
        resolveSystemProxyModeOnExit(SystemProxyMode::Pac, false),
        SystemProxyMode::ForcedClear);
}

void SystemProxyModeTests::resolveSystemProxyModeOnExitClearsOnWindowsShutdown()
{
    QCOMPARE(
        resolveSystemProxyModeOnExit(SystemProxyMode::Unchanged, true),
        SystemProxyMode::ForcedClear);
    QCOMPARE(
        resolveSystemProxyModeOnExit(SystemProxyMode::Pac, true),
        SystemProxyMode::ForcedClear);
}

QTEST_MAIN(SystemProxyModeTests)
#include "SystemProxyModeTests.moc"
