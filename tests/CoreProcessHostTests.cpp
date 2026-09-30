#include <QtTest>

#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include "runtime/QtCoreProcessHost.h"

class CoreProcessHostTests : public QObject
{
    Q_OBJECT

private slots:
    void stopWithoutGracePeriodStopsTheConsoleProcessPromptly();
};

// The other suites drive ICoreProcessHost through a stand-in, which is the right shape for testing
// ProxySession. This one is the opposite case: the behaviour under test *is* how Qt drives a real
// child process on Windows, so it needs the real host and a real process.
void CoreProcessHostTests::stopWithoutGracePeriodStopsTheConsoleProcessPromptly()
{
#ifdef Q_OS_WIN
    // A core engine is a console application, and QProcess::terminate() on Windows only posts
    // WM_CLOSE to the child's top-level windows and to its main thread
    // (qtbase/src/corelib/io/qprocess_win.cpp) -- neither of which a console process has or pumps.
    // The graceful request never reaches the child, and the forced kill is what actually stops it,
    // so waiting out the grace period first was pure latency. It was paid on every stop: on every
    // server switch, where the new core cannot start until the old one has released its ports, and
    // on shutdown, which stops the core synchronously.
    //
    // `ping` has the same shape as a core here -- console subsystem, no windows, no message loop --
    // so it stands in for one without needing a core binary to be present.
    const QString pingPath = QDir(qEnvironmentVariable("SystemRoot") + QStringLiteral("/System32"))
                                 .filePath(QStringLiteral("ping.exe"));
    if (!QFileInfo::exists(pingPath)) {
        QSKIP("ping.exe is not available; skipping the console process stop probe.");
    }

    QtCoreProcessHost host;
    CoreInfo coreInfo;
    coreInfo.program = pingPath;
    coreInfo.arguments = QStringList{
        QStringLiteral("-n"), QStringLiteral("60"), QStringLiteral("127.0.0.1")};
    coreInfo.appendConfigArgument = false;
    coreInfo.captureOutput = true;

    bool exited = false;
    const OperationResult startResult = host.start(
        coreInfo,
        QString(),
        [](const QString&) {},
        {},
        {},
        [&exited](int, QProcess::ExitStatus, bool) { exited = true; });
    QVERIFY2(startResult.success, qPrintable(startResult.message));
    QTRY_VERIFY(host.isRunning());

    const OperationResult stopResult = host.stop(false);
    QVERIFY2(stopResult.success, qPrintable(stopResult.message));

    // 500 ms sits far above what a kill plus process teardown needs and far below the 1500 ms grace
    // period the host used to wait out before killing, so the two behaviours cannot be confused.
    QTRY_VERIFY_WITH_TIMEOUT(exited, 500);
    QVERIFY(!host.isRunning());
#else
    QSKIP("The console process stop probe is Windows specific.");
#endif
}

QTEST_MAIN(CoreProcessHostTests)

#include "CoreProcessHostTests.moc"
