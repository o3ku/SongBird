#include <QtTest>

#include <QSettings>
#include <QString>

#include <utility>

#include "common/SystemProxyMode.h"
#include "platform/windows/WindowsSystemProxyService.h"

namespace {

// A scratch key, never the real Internet Settings key: a ctest run must not touch the user's
// system proxy, and the real key cannot be made to refuse a write anyway.
const QString kScratchRoot = QStringLiteral("HKEY_CURRENT_USER\\Software\\SongBirdTestHarness");
const QString kScratchKey = kScratchRoot + QStringLiteral("\\SystemProxyService");

// QSettings resolves the path through the registry under NativeFormat and reports AccessError
// when the string is not a registry path (measured: status=AccessError, no file is created in
// the working directory). That is the only deterministic way to make this service's writes
// fail without inventing a settings-store abstraction just for tests.
const QString kUnwritableKey = QStringLiteral("not-a-registry-path");

void removeScratchKey()
{
    QSettings root(kScratchRoot, QSettings::NativeFormat);
    root.remove(QStringLiteral("SystemProxyService"));
    root.sync();
    QSettings software(QStringLiteral("HKEY_CURRENT_USER\\Software"), QSettings::NativeFormat);
    software.remove(QStringLiteral("SongBirdTestHarness"));
    software.sync();
}

// The scratch key is dropped before and after every case so an aborted run cannot leak state
// into the next one.
class ScopedRegistryKey
{
public:
    ScopedRegistryKey()
    {
        removeScratchKey();
    }

    ~ScopedRegistryKey()
    {
        removeScratchKey();
    }

    static QString path()
    {
        return kScratchKey;
    }
};

void writeScratchValue(const QString& key, const QVariant& value)
{
    QSettings settings(kScratchKey, QSettings::NativeFormat);
    settings.setValue(key, value);
    settings.sync();
}

QString readScratchValue(const QString& key)
{
    QSettings settings(kScratchKey, QSettings::NativeFormat);
    return settings.value(key).toString();
}

bool scratchContains(const QString& key)
{
    QSettings settings(kScratchKey, QSettings::NativeFormat);
    return settings.contains(key);
}

} // namespace

class SystemProxyServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void clearReportsFailureWhenTheRegistryRefusesTheWrite();
    void enableReportsFailureWhenTheRegistryRefusesTheWrite();
    void enableRejectsAnInvalidPortWithoutTouchingTheRegistry();

    void clearDisablesTheProxyAndDropsThePacScript();
    void enableWritesTheProxyAndDropsThePacScript();
    void enableExpandsTheAdvancedProtocolPlaceholders();
    void shutdownClearsTheProxy();
};

void SystemProxyServiceTests::initTestCase()
{
#if !defined(Q_OS_WIN)
    QSKIP("WindowsSystemProxyService writes to HKCU, which only exists on Windows.");
#endif
}

void SystemProxyServiceTests::clearReportsFailureWhenTheRegistryRefusesTheWrite()
{
    WindowsSystemProxyService service(kUnwritableKey);

    // The disable path used to call setProxy() and then return true unconditionally, so a
    // failed write was reported as a successful cleanup: the caller dropped its managed-proxy
    // flag while the system was still pointed at the core that had just stopped.
    //
    // update() makes two writes against one key -- remove AutoConfigURL, then setProxy() -- and
    // they share a failure mode, so this assertion is satisfied by *either* result being
    // checked. Verified by mutation: restoring the pre-fix disable branch (which checks
    // neither) fails here; dropping just one of the two checks does not. The granularity is
    // the whole operation on purpose: what callers need to know is whether the mode applied.
    QVERIFY(!service.update(SystemProxyMode::ForcedClear, 0, 0, QString(), QString()));
}

void SystemProxyServiceTests::enableReportsFailureWhenTheRegistryRefusesTheWrite()
{
    WindowsSystemProxyService service(kUnwritableKey);

    // Already correct before the fix. Kept as the contrast case: the two directions must not
    // diverge again, and this is the assertion that pins the pair.
    QVERIFY(!service.update(SystemProxyMode::ForcedChange, 10809, 10808, QString(), QString()));
}

void SystemProxyServiceTests::enableRejectsAnInvalidPortWithoutTouchingTheRegistry()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    writeScratchValue(QStringLiteral("AutoConfigURL"), QStringLiteral("http://pac.invalid/proxy.pac"));
    writeScratchValue(QStringLiteral("ProxyEnable"), 0);

    QVERIFY(!service.update(SystemProxyMode::ForcedChange, 0, 0, QString(), QString()));

    // Removing the PAC script is a write too, so it must not happen before the request is
    // known to be actionable -- otherwise a rejected call still strips the user's PAC config.
    QCOMPARE(readScratchValue(QStringLiteral("AutoConfigURL")),
        QStringLiteral("http://pac.invalid/proxy.pac"));
}

void SystemProxyServiceTests::clearDisablesTheProxyAndDropsThePacScript()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    QVERIFY(service.update(SystemProxyMode::ForcedChange, 10809, 10808,
        QStringLiteral("localhost"), QString()));
    QVERIFY(service.isEnabled());
    QCOMPARE(readScratchValue(QStringLiteral("ProxyServer")), QStringLiteral("127.0.0.1:10809"));

    writeScratchValue(QStringLiteral("AutoConfigURL"), QStringLiteral("http://pac.invalid/proxy.pac"));

    QVERIFY(service.update(SystemProxyMode::ForcedClear, 0, 0, QString(), QString()));

    QVERIFY(!service.isEnabled());
    QCOMPARE(readScratchValue(QStringLiteral("ProxyEnable")), QStringLiteral("0"));
    QVERIFY(!scratchContains(QStringLiteral("ProxyServer")));
    QVERIFY(!scratchContains(QStringLiteral("ProxyOverride")));
    // A PAC script overrides ProxyEnable, so leaving it behind would keep the system pointed
    // at the PAC URL even though the service reports the proxy as cleared.
    QVERIFY(!scratchContains(QStringLiteral("AutoConfigURL")));
}

void SystemProxyServiceTests::enableWritesTheProxyAndDropsThePacScript()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    writeScratchValue(QStringLiteral("AutoConfigURL"), QStringLiteral("http://pac.invalid/proxy.pac"));

    QVERIFY(service.update(SystemProxyMode::ForcedChange, 10809, 10808,
        QStringLiteral("localhost;<local>"), QString()));

    QVERIFY(service.isEnabled());
    QCOMPARE(readScratchValue(QStringLiteral("ProxyServer")), QStringLiteral("127.0.0.1:10809"));
    QCOMPARE(readScratchValue(QStringLiteral("ProxyOverride")), QStringLiteral("localhost;<local>"));
    QVERIFY(!scratchContains(QStringLiteral("AutoConfigURL")));
}

void SystemProxyServiceTests::enableExpandsTheAdvancedProtocolPlaceholders()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    QVERIFY(service.update(SystemProxyMode::ForcedChange, 10809, 10808, QString(),
        QStringLiteral("http={ip}:{http_port};socks={ip}:{socks_port}")));

    QCOMPARE(readScratchValue(QStringLiteral("ProxyServer")),
        QStringLiteral("http=127.0.0.1:10809;socks=127.0.0.1:10808"));
}

void SystemProxyServiceTests::shutdownClearsTheProxy()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    QVERIFY(service.update(SystemProxyMode::ForcedChange, 10809, 10808, QString(), QString()));
    QVERIFY(service.isEnabled());

    service.resetOnShutdown();

    QVERIFY(!service.isEnabled());
    QVERIFY(!scratchContains(QStringLiteral("ProxyServer")));
}

QTEST_MAIN(SystemProxyServiceTests)

#include "SystemProxyServiceTests.moc"
