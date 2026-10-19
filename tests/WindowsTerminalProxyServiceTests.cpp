#include <QtTest>

#include <array>
#include <optional>

#include <QSettings>
#include <QString>

#include "platform/ITerminalProxyService.h"
#include "platform/windows/WindowsTerminalProxyService.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace {

// A scratch key, never the real HKCU\Environment key: a ctest run must not leave the developer's own
// terminal pointed at a port that is not listening, and the real key cannot be made to refuse a write
// anyway. The read and write paths are identical either way -- only the storage location differs.
const QString kScratchRoot = QStringLiteral("HKEY_CURRENT_USER\\Software\\SongBirdTestHarness");
const QString kScratchKey = kScratchRoot + QStringLiteral("\\TerminalProxyService");

// QSettings resolves the path through the registry under NativeFormat and reports AccessError when
// the string is not a registry path (measured: status=AccessError, no file is created in the working
// directory). That is the only deterministic way to make this service's writes fail without
// inventing a settings-store abstraction just for tests.
const QString kUnwritableKey = QStringLiteral("not-a-registry-path");

const QString kHttp = QStringLiteral("HTTP_PROXY");
const QString kHttps = QStringLiteral("HTTPS_PROXY");
const QString kAll = QStringLiteral("ALL_PROXY");
const QString kNo = QStringLiteral("NO_PROXY");

void removeScratchKey()
{
    QSettings software(QStringLiteral("HKEY_CURRENT_USER\\Software"), QSettings::NativeFormat);
    software.remove(QStringLiteral("SongBirdTestHarness"));
    software.sync();
}

// The scratch key is dropped before and after every case so an aborted run cannot leak state into the
// next one.
class ScopedRegistryKey
{
public:
    ScopedRegistryKey() { removeScratchKey(); }
    ~ScopedRegistryKey() { removeScratchKey(); }
};

void writeScratchValue(const QString& name, const QVariant& value)
{
    QSettings settings(kScratchKey, QSettings::NativeFormat);
    settings.setValue(name, value);
    settings.sync();
}

std::optional<QString> readScratchValue(const QString& name)
{
    QSettings settings(kScratchKey, QSettings::NativeFormat);
    const QVariant value = settings.value(name);
    if (!value.isValid()) {
        return std::nullopt;
    }
    return value.toString();
}

bool scratchContains(const QString& name)
{
    QSettings settings(kScratchKey, QSettings::NativeFormat);
    return settings.contains(name);
}

// The service mirrors every registry write into this process's own environment block, and that block
// is global -- it cannot be pointed at a scratch location. A test that let those writes stand would
// leave the ctest process (and anything it later launched) pointing at 127.0.0.1:2080, and worse, it
// would clobber a developer's real HTTP_PROXY. This saves the four values on construction and puts
// them back on destruction.
class ScopedProcessEnvironment
{
public:
    ScopedProcessEnvironment()
    {
        saved_ = {read(kHttp), read(kHttps), read(kAll), read(kNo)};
        clearAll();
    }

    ~ScopedProcessEnvironment()
    {
        put(kHttp, saved_[0]);
        put(kHttps, saved_[1]);
        put(kAll, saved_[2]);
        put(kNo, saved_[3]);
    }

private:
    static std::optional<QString> read(const QString& name)
    {
        const std::wstring wideName = name.toStdWString();
        const DWORD needed = GetEnvironmentVariableW(wideName.c_str(), nullptr, 0);
        if (needed == 0) {
            return std::nullopt;
        }
        std::wstring buffer(needed, L'\0');
        const DWORD written = GetEnvironmentVariableW(wideName.c_str(), buffer.data(), needed);
        if (written == 0 || written >= needed) {
            return std::nullopt;
        }
        buffer.resize(written);
        return QString::fromStdWString(buffer);
    }

    static void write(const QString& name, const std::optional<QString>& value)
    {
        const std::wstring wideName = name.toStdWString();
        if (value.has_value()) {
            const std::wstring wideValue = value->toStdWString();
            SetEnvironmentVariableW(wideName.c_str(), wideValue.c_str());
        } else {
            SetEnvironmentVariableW(wideName.c_str(), nullptr);
        }
    }

    static void clearAll()
    {
        for (const QString& name : {kHttp, kHttps, kAll, kNo}) {
            write(name, std::nullopt);
        }
    }

    static void put(const QString& name, const std::optional<QString>& value)
    {
        write(name, value);
    }

    std::array<std::optional<QString>, 4> saved_;
};

} // namespace

// Characterization tests for WindowsTerminalProxyService's registry contract.
//
// The service has two kinds of side effect: registry writes (testable, because the key is injectable)
// and writes to this process's own environment block plus a WM_SETTINGCHANGE broadcast (not testable,
// because neither can be pointed at a scratch location). These tests cover the first half
// exhaustively and say so rather than implying the second half is verified.
//
// The defect class they exist for is the same one SystemProxyServiceTests covers: the app must not
// destroy an environment the user set up before starting it. Concretely, a user with a corporate
// HTTP_PROXY must still have it after SongBird quits, and a user with their own NO_PROXY must not
// have it overwritten.
class WindowsTerminalProxyServiceTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // --- enable ---
    void enableWritesTheThreeProxyVariables();
    void enableWritesNoProxyWhenTheUserHasNone();
    void enableLeavesTheUsersNoProxyAlone();
    void enableTreatsAnEmptyNoProxyAsAbsent();
    void enableRejectsAnEmptyUrlWithoutTouchingTheRegistry();
    void enableReportsFailureWhenTheRegistryRefusesTheWrite();

    // --- disable ---
    void disableRemovesTheThreeProxyVariables();
    void disableLeavesNoProxyBehind();
    void disableReportsFailureWhenTheRegistryRefusesTheWrite();

    // --- isEnabledFor ---
    void isEnabledForMatchesTheWrittenUrl();
    void isEnabledForIgnoresCase();
    void isEnabledForIsFalseWhenNothingIsSet();
    void isEnabledForIsFalseForADifferentPort();
    void isEnabledForRejectsAnEmptyUrl();

    // --- capture / restore ---
    void captureRecordsWhichVariablesExisted();
    void captureOfAnEmptyRegistryIsValid();
    void restorePutsBackTheValuesThatExisted();
    void restoreDeletesOnlyTheVariablesThatDidNotExist();
    void restoreBringsBackAnEmptyButPresentVariable();
    void restoreReportsSuccessForASnapshotThatWasNeverCaptured();
    void restoreReportsFailureWhenTheRegistryRefusesTheWrite();
    void restoreIsIdempotent();

    // --- the round trip that matters ---
    void captureThenEnableThenRestoreLeavesTheUserEnvironmentIntact();
};

void WindowsTerminalProxyServiceTests::initTestCase()
{
#if !defined(Q_OS_WIN)
    QSKIP("WindowsTerminalProxyService writes to HKCU, which only exists on Windows.");
#endif
}

void WindowsTerminalProxyServiceTests::enableWritesTheThreeProxyVariables()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    QVERIFY(service.enable(QStringLiteral("http://127.0.0.1:2080"), QString()));

    QCOMPARE(readScratchValue(kHttp).value_or(QString()), QStringLiteral("http://127.0.0.1:2080"));
    QCOMPARE(readScratchValue(kHttps).value_or(QString()), QStringLiteral("http://127.0.0.1:2080"));
    QCOMPARE(readScratchValue(kAll).value_or(QString()), QStringLiteral("http://127.0.0.1:2080"));
}

void WindowsTerminalProxyServiceTests::enableWritesNoProxyWhenTheUserHasNone()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    const QString bypass = QStringLiteral("localhost,127.0.0.1");
    QVERIFY(service.enable(QStringLiteral("http://127.0.0.1:2080"), bypass));
    QCOMPARE(readScratchValue(kNo).value_or(QString()), bypass);
}

void WindowsTerminalProxyServiceTests::enableLeavesTheUsersNoProxyAlone()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    writeScratchValue(kNo, QStringLiteral("*.corp.example.com,10.1.2.0/24"));

    WindowsTerminalProxyService service(kScratchKey);
    QVERIFY(service.enable(QStringLiteral("http://127.0.0.1:2080"), QStringLiteral("localhost")));

    // The user's own bypass list survives: overwriting it would silently route their corporate hosts
    // through the proxy.
    QCOMPARE(readScratchValue(kNo).value_or(QString()),
             QStringLiteral("*.corp.example.com,10.1.2.0/24"));
}

void WindowsTerminalProxyServiceTests::enableTreatsAnEmptyNoProxyAsAbsent()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    // A variable that exists but is empty carries no bypass information, so it is safe to replace.
    writeScratchValue(kNo, QString());

    WindowsTerminalProxyService service(kScratchKey);
    const QString bypass = QStringLiteral("localhost");
    QVERIFY(service.enable(QStringLiteral("http://127.0.0.1:2080"), bypass));
    QCOMPARE(readScratchValue(kNo).value_or(QString()), bypass);
}

void WindowsTerminalProxyServiceTests::enableRejectsAnEmptyUrlWithoutTouchingTheRegistry()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    QVERIFY(!service.enable(QString(), QStringLiteral("localhost")));
    QVERIFY(!scratchContains(kHttp));
    QVERIFY(!scratchContains(kHttps));
    QVERIFY(!scratchContains(kAll));
}

void WindowsTerminalProxyServiceTests::enableReportsFailureWhenTheRegistryRefusesTheWrite()
{
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kUnwritableKey);
    QVERIFY(!service.enable(QStringLiteral("http://127.0.0.1:2080"), QString()));
}

void WindowsTerminalProxyServiceTests::disableRemovesTheThreeProxyVariables()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    QVERIFY(service.enable(QStringLiteral("http://127.0.0.1:2080"), QString()));
    QVERIFY(service.disable());

    QVERIFY(!scratchContains(kHttp));
    QVERIFY(!scratchContains(kHttps));
    QVERIFY(!scratchContains(kAll));
}

void WindowsTerminalProxyServiceTests::disableLeavesNoProxyBehind()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    QVERIFY(service.enable(QStringLiteral("http://127.0.0.1:2080"), QStringLiteral("localhost")));
    QVERIFY(service.disable());

    // This app cannot tell which NO_PROXY entries it added, so it must not delete the variable: an
    // extra bypass entry is harmless, deleting the user's list is not.
    QCOMPARE(readScratchValue(kNo).value_or(QString()), QStringLiteral("localhost"));
}

void WindowsTerminalProxyServiceTests::disableReportsFailureWhenTheRegistryRefusesTheWrite()
{
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kUnwritableKey);
    QVERIFY(!service.disable());
}

void WindowsTerminalProxyServiceTests::isEnabledForMatchesTheWrittenUrl()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    QVERIFY(service.enable(QStringLiteral("http://127.0.0.1:2080"), QString()));
    QVERIFY(service.isEnabledFor(QStringLiteral("http://127.0.0.1:2080")));
}

void WindowsTerminalProxyServiceTests::isEnabledForIgnoresCase()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    QVERIFY(service.enable(QStringLiteral("http://127.0.0.1:2080"), QString()));
    QVERIFY(service.isEnabledFor(QStringLiteral("HTTP://127.0.0.1:2080")));
}

void WindowsTerminalProxyServiceTests::isEnabledForIsFalseWhenNothingIsSet()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);
    QVERIFY(!service.isEnabledFor(QStringLiteral("http://127.0.0.1:2080")));
}

void WindowsTerminalProxyServiceTests::isEnabledForIsFalseForADifferentPort()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    QVERIFY(service.enable(QStringLiteral("http://127.0.0.1:2080"), QString()));
    QVERIFY(!service.isEnabledFor(QStringLiteral("http://127.0.0.1:9999")));
}

void WindowsTerminalProxyServiceTests::isEnabledForRejectsAnEmptyUrl()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);
    QVERIFY(!service.isEnabledFor(QString()));
}

void WindowsTerminalProxyServiceTests::captureRecordsWhichVariablesExisted()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    writeScratchValue(kHttp, QStringLiteral("http://corp:3128"));
    writeScratchValue(kNo, QStringLiteral("localhost"));

    WindowsTerminalProxyService service(kScratchKey);
    const TerminalProxySnapshot snapshot = service.capture();

    QVERIFY(snapshot.valid);
    QCOMPARE(snapshot.httpProxy.value_or(QString()), QStringLiteral("http://corp:3128"));
    QVERIFY(snapshot.httpsProxy == std::nullopt);
    QVERIFY(snapshot.allProxy == std::nullopt);
    QCOMPARE(snapshot.noProxy.value_or(QString()), QStringLiteral("localhost"));
}

void WindowsTerminalProxyServiceTests::captureOfAnEmptyRegistryIsValid()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    const TerminalProxySnapshot snapshot = service.capture();
    // Valid but all-absent: "there was nothing here" is a state worth restoring to.
    QVERIFY(snapshot.valid);
    QVERIFY(snapshot.httpProxy == std::nullopt);
    QVERIFY(snapshot.httpsProxy == std::nullopt);
    QVERIFY(snapshot.allProxy == std::nullopt);
    QVERIFY(snapshot.noProxy == std::nullopt);
}

void WindowsTerminalProxyServiceTests::restorePutsBackTheValuesThatExisted()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    TerminalProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.httpProxy = QStringLiteral("http://corp:3128");
    snapshot.httpsProxy = QStringLiteral("http://corp:3128");
    snapshot.allProxy = QStringLiteral("socks5://corp:1080");
    snapshot.noProxy = QStringLiteral("*.corp.example.com");

    QVERIFY(service.restore(snapshot));
    QCOMPARE(readScratchValue(kHttp).value_or(QString()), QStringLiteral("http://corp:3128"));
    QCOMPARE(readScratchValue(kHttps).value_or(QString()), QStringLiteral("http://corp:3128"));
    QCOMPARE(readScratchValue(kAll).value_or(QString()), QStringLiteral("socks5://corp:1080"));
    QCOMPARE(readScratchValue(kNo).value_or(QString()), QStringLiteral("*.corp.example.com"));
}

void WindowsTerminalProxyServiceTests::restoreDeletesOnlyTheVariablesThatDidNotExist()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    // Start with a value in place that the snapshot says was absent, to prove the restore deletes it
    // rather than leaving it.
    writeScratchValue(kHttp, QStringLiteral("http://127.0.0.1:2080"));
    writeScratchValue(kHttps, QStringLiteral("http://127.0.0.1:2080"));

    TerminalProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.httpProxy = std::nullopt;
    snapshot.httpsProxy = QStringLiteral("http://corp:3128");

    WindowsTerminalProxyService service(kScratchKey);
    QVERIFY(service.restore(snapshot));

    QVERIFY(!scratchContains(kHttp));
    QCOMPARE(readScratchValue(kHttps).value_or(QString()), QStringLiteral("http://corp:3128"));
}

void WindowsTerminalProxyServiceTests::restoreBringsBackAnEmptyButPresentVariable()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    // "Present but empty" is a real state a user can create, and it must come back as empty rather
    // than be deleted -- that is the whole reason the snapshot uses std::optional.
    TerminalProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.httpProxy = QString();

    QVERIFY(service.restore(snapshot));
    QVERIFY(scratchContains(kHttp));
    QCOMPARE(readScratchValue(kHttp).value_or(QStringLiteral("sentinel")), QString());
}

void WindowsTerminalProxyServiceTests::restoreReportsSuccessForASnapshotThatWasNeverCaptured()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    // Reporting failure here would turn every caller that never captured into a red case, and there
    // is nothing to undo anyway.
    QVERIFY(service.restore(TerminalProxySnapshot{}));
}

void WindowsTerminalProxyServiceTests::restoreReportsFailureWhenTheRegistryRefusesTheWrite()
{
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kUnwritableKey);

    TerminalProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.httpProxy = QStringLiteral("http://corp:3128");
    QVERIFY(!service.restore(snapshot));
}

void WindowsTerminalProxyServiceTests::restoreIsIdempotent()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;
    WindowsTerminalProxyService service(kScratchKey);

    TerminalProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.httpProxy = QStringLiteral("http://corp:3128");
    snapshot.noProxy = QStringLiteral("localhost");

    QVERIFY(service.restore(snapshot));
    QVERIFY(service.restore(snapshot));

    QCOMPARE(readScratchValue(kHttp).value_or(QString()), QStringLiteral("http://corp:3128"));
    QCOMPARE(readScratchValue(kNo).value_or(QString()), QStringLiteral("localhost"));
}

void WindowsTerminalProxyServiceTests::captureThenEnableThenRestoreLeavesTheUserEnvironmentIntact()
{
    ScopedRegistryKey scratch;
    ScopedProcessEnvironment environment;

    // The user's own environment before SongBird starts.
    writeScratchValue(kHttp, QStringLiteral("http://corp-proxy.example.com:3128"));
    writeScratchValue(kHttps, QStringLiteral("http://corp-proxy.example.com:3128"));
    writeScratchValue(kNo, QStringLiteral("*.corp.example.com"));

    WindowsTerminalProxyService service(kScratchKey);
    const TerminalProxySnapshot snapshot = service.capture();
    QVERIFY(snapshot.valid);

    // The app applies its own values.
    QVERIFY(service.enable(
        QStringLiteral("http://127.0.0.1:2080"), QStringLiteral("localhost,127.0.0.1")));
    QCOMPARE(readScratchValue(kHttp).value_or(QString()), QStringLiteral("http://127.0.0.1:2080"));
    // The user's NO_PROXY was not overwritten.
    QCOMPARE(readScratchValue(kNo).value_or(QString()), QStringLiteral("*.corp.example.com"));

    // The app quits and restores.
    QVERIFY(service.restore(snapshot));
    QCOMPARE(readScratchValue(kHttp).value_or(QString()),
             QStringLiteral("http://corp-proxy.example.com:3128"));
    QCOMPARE(readScratchValue(kHttps).value_or(QString()),
             QStringLiteral("http://corp-proxy.example.com:3128"));
    QCOMPARE(readScratchValue(kNo).value_or(QString()), QStringLiteral("*.corp.example.com"));
    // ALL_PROXY did not exist before and must not be left behind.
    QVERIFY(!scratchContains(kAll));
}

QTEST_GUILESS_MAIN(WindowsTerminalProxyServiceTests)
#include "WindowsTerminalProxyServiceTests.moc"
