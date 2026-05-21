#include <QtTest>

#include "common/SystemProxyMode.h"

class SystemProxyModeTests : public QObject {
    Q_OBJECT

private slots:
    void shouldAdoptManagedSystemProxyOnStartupOnlyForPersistedManagedProxy();
    void shouldClearManagedSystemProxyOnlyWhenOwned();
    void resolveSystemProxyModeOnExitAlwaysClears();
    void normalizeSystemProxyModeMapsOldValuesToForcedClear();
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
        SystemProxyMode::ForcedClear,
        true,
        true));
}

void SystemProxyModeTests::shouldClearManagedSystemProxyOnlyWhenOwned()
{
    QVERIFY(shouldClearManagedSystemProxy(true, true));
    QVERIFY(!shouldClearManagedSystemProxy(false, true));
    QVERIFY(!shouldClearManagedSystemProxy(true, false));
}

void SystemProxyModeTests::resolveSystemProxyModeOnExitAlwaysClears()
{
    QCOMPARE(
        resolveSystemProxyModeOnExit(SystemProxyMode::ForcedClear, false),
        SystemProxyMode::ForcedClear);
    QCOMPARE(
        resolveSystemProxyModeOnExit(SystemProxyMode::ForcedChange, false),
        SystemProxyMode::ForcedClear);
    QCOMPARE(
        resolveSystemProxyModeOnExit(SystemProxyMode::ForcedClear, true),
        SystemProxyMode::ForcedClear);
    QCOMPARE(
        resolveSystemProxyModeOnExit(SystemProxyMode::ForcedChange, true),
        SystemProxyMode::ForcedClear);
}

void SystemProxyModeTests::normalizeSystemProxyModeMapsOldValuesToForcedClear()
{
    QCOMPARE(normalizeSystemProxyMode(0), SystemProxyMode::ForcedClear);
    QCOMPARE(normalizeSystemProxyMode(1), SystemProxyMode::ForcedChange);
    QCOMPARE(normalizeSystemProxyMode(2), SystemProxyMode::ForcedClear);
    QCOMPARE(normalizeSystemProxyMode(3), SystemProxyMode::ForcedClear);
    QCOMPARE(normalizeSystemProxyMode(99), SystemProxyMode::ForcedClear);
}

QTEST_MAIN(SystemProxyModeTests)
#include "SystemProxyModeTests.moc"
