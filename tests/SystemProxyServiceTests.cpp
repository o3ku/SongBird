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

    // Snapshot / restore. The defect these cover: quitting used to delete ProxyServer and
    // ProxyOverride, so a user who was running their own proxy when the app started lost it.
    void captureRecordsWhichKeysExisted();
    void restorePutsBackTheValuesThatExisted();
    void restoreDeletesOnlyTheKeysThatDidNotExist();
    void restoreBringsBackThePacScriptThatEnableCleared();
    void restoreLeavesThePacScriptRemovedWhenThereWasNone();
    void restoreReportsSuccessForASnapshotThatWasNeverCaptured();
    void captureOfAnEmptyRegistryIsValidAndRestorable();
    void restoreReportsFailureWhenTheRegistryRefusesTheWrite();
    void captureIsNotDisturbedByOurOwnWrite();
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

void SystemProxyServiceTests::captureRecordsWhichKeysExisted()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    // A machine with the proxy on and an override list, but no PAC script. The absent PAC is the
    // case a QString cannot express: it has to come back as nullopt, not as "".
    writeScratchValue(QStringLiteral("ProxyEnable"), 1);
    writeScratchValue(QStringLiteral("ProxyServer"), QStringLiteral("10.0.0.1:3128"));
    writeScratchValue(QStringLiteral("ProxyOverride"), QStringLiteral("<local>"));

    const SystemProxySnapshot snapshot = service.capture();

    QVERIFY(snapshot.valid);
    QVERIFY(snapshot.enabled);
    QCOMPARE(snapshot.server, std::optional<QString>(QStringLiteral("10.0.0.1:3128")));
    QCOMPARE(snapshot.overrideList, std::optional<QString>(QStringLiteral("<local>")));
    QVERIFY(!snapshot.autoConfigUrl.has_value());
}

void SystemProxyServiceTests::restorePutsBackTheValuesThatExisted()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    writeScratchValue(QStringLiteral("ProxyEnable"), 1);
    writeScratchValue(QStringLiteral("ProxyServer"), QStringLiteral("10.0.0.1:3128"));
    writeScratchValue(QStringLiteral("ProxyOverride"), QStringLiteral("<local>"));
    const SystemProxySnapshot snapshot = service.capture();

    // We take the proxy over, then put it back.
    QVERIFY(service.update(SystemProxyMode::ForcedChange, 10809, 10808, QStringLiteral("localhost"), QString()));
    QCOMPARE(readScratchValue(QStringLiteral("ProxyServer")), QStringLiteral("127.0.0.1:10809"));

    QVERIFY(service.restore(snapshot));

    QVERIFY(service.isEnabled());
    QCOMPARE(readScratchValue(QStringLiteral("ProxyServer")), QStringLiteral("10.0.0.1:3128"));
    QCOMPARE(readScratchValue(QStringLiteral("ProxyOverride")), QStringLiteral("<local>"));
}

void SystemProxyServiceTests::restoreDeletesOnlyTheKeysThatDidNotExist()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    // Nothing was configured: the proxy was off and neither key existed.
    writeScratchValue(QStringLiteral("ProxyEnable"), 0);
    const SystemProxySnapshot snapshot = service.capture();
    QVERIFY(snapshot.valid);
    QVERIFY(!snapshot.enabled);

    QVERIFY(service.update(SystemProxyMode::ForcedChange, 10809, 10808, QStringLiteral("localhost"), QString()));
    QVERIFY(scratchContains(QStringLiteral("ProxyServer")));

    QVERIFY(service.restore(snapshot));

    QVERIFY(!service.isEnabled());
    // Both keys are gone because neither existed before us -- not because the restore clears.
    QVERIFY(!scratchContains(QStringLiteral("ProxyServer")));
    QVERIFY(!scratchContains(QStringLiteral("ProxyOverride")));
}

void SystemProxyServiceTests::restoreBringsBackThePacScriptThatEnableCleared()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    writeScratchValue(QStringLiteral("AutoConfigURL"), QStringLiteral("http://pac.invalid/proxy.pac"));
    writeScratchValue(QStringLiteral("ProxyEnable"), 0);
    const SystemProxySnapshot snapshot = service.capture();

    // enable() has to clear the PAC or it would keep winning over our ProxyServer.
    QVERIFY(service.update(SystemProxyMode::ForcedChange, 10809, 10808, QString(), QString()));
    QVERIFY(!scratchContains(QStringLiteral("AutoConfigURL")));

    QVERIFY(service.restore(snapshot));

    // The user's PAC script is theirs, and a restore that dropped it would silently change how
    // their browser resolves proxies -- a state they never asked us to touch.
    QCOMPARE(readScratchValue(QStringLiteral("AutoConfigURL")),
        QStringLiteral("http://pac.invalid/proxy.pac"));
}

void SystemProxyServiceTests::restoreLeavesThePacScriptRemovedWhenThereWasNone()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    writeScratchValue(QStringLiteral("ProxyEnable"), 0);
    const SystemProxySnapshot snapshot = service.capture();
    QVERIFY(!snapshot.autoConfigUrl.has_value());

    QVERIFY(service.update(SystemProxyMode::ForcedChange, 10809, 10808, QString(), QString()));
    QVERIFY(service.restore(snapshot));

    QVERIFY(!scratchContains(QStringLiteral("AutoConfigURL")));
}

void SystemProxyServiceTests::restoreReportsSuccessForASnapshotThatWasNeverCaptured()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    writeScratchValue(QStringLiteral("ProxyServer"), QStringLiteral("10.0.0.1:3128"));

    // A default-constructed snapshot must be a no-op, not "delete everything": restoring it
    // would wipe a proxy that was never ours to begin with.
    QVERIFY(service.restore(SystemProxySnapshot{}));
    QCOMPARE(readScratchValue(QStringLiteral("ProxyServer")), QStringLiteral("10.0.0.1:3128"));
}

void SystemProxyServiceTests::captureOfAnEmptyRegistryIsValidAndRestorable()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    // An existing key with no proxy values is a *capture*, not a failure: it describes "the proxy
    // was off and none of these keys were set", which is exactly the state a restore should put
    // back (proxy off, our leftovers deleted). QSettings cannot tell this apart from a registry
    // path that does not exist -- both read as NoError with no values -- so capture() does not
    // pretend to; it reports what it can see.
    const SystemProxySnapshot snapshot = service.capture();

    QVERIFY(snapshot.valid);
    QVERIFY(!snapshot.enabled);
    QVERIFY(!snapshot.server.has_value());
    QVERIFY(!snapshot.overrideList.has_value());
    QVERIFY(!snapshot.autoConfigUrl.has_value());
    QVERIFY(isRestorableSystemProxySnapshot(snapshot));
}

void SystemProxyServiceTests::restoreReportsFailureWhenTheRegistryRefusesTheWrite()
{
    WindowsSystemProxyService service(kUnwritableKey);

    SystemProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.enabled = true;
    snapshot.server = QStringLiteral("10.0.0.1:3128");

    // A restore that cannot write must say so; silently reporting success is how the caller ends
    // up believing the user's proxy is back when the registry still points at a dead port.
    QVERIFY(!service.restore(snapshot));
}

void SystemProxyServiceTests::captureIsNotDisturbedByOurOwnWrite()
{
    ScopedRegistryKey scratch;
    WindowsSystemProxyService service(ScopedRegistryKey::path());

    writeScratchValue(QStringLiteral("ProxyEnable"), 1);
    writeScratchValue(QStringLiteral("ProxyServer"), QStringLiteral("10.0.0.1:3128"));

    const SystemProxySnapshot before = service.capture();
    QVERIFY(service.update(SystemProxyMode::ForcedChange, 10809, 10808, QString(), QString()));
    const SystemProxySnapshot after = service.capture();

    // A second capture sees our own port -- which is exactly why the callers must capture once,
    // before the first write, and never again. This pins the hazard rather than the fix.
    QCOMPARE(after.server, std::optional<QString>(QStringLiteral("127.0.0.1:10809")));
    QCOMPARE(before.server, std::optional<QString>(QStringLiteral("10.0.0.1:3128")));
}

QTEST_MAIN(SystemProxyServiceTests)

#include "SystemProxyServiceTests.moc"
