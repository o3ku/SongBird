#include <QtTest>

#include "common/SystemProxyMode.h"

class SystemProxyModeTests : public QObject {
    Q_OBJECT

private slots:
    void resolveSystemProxyModeOnExitKeepsUnchangedMode();
    void resolveSystemProxyModeOnExitClearsManagedModes();
    void resolveSystemProxyModeOnExitClearsOnWindowsShutdown();
};

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
