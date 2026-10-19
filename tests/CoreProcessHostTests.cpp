#include <QtTest>

#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include "runtime/QtCoreProcessHost.h"

#ifdef Q_OS_WIN
#include "platform/windows/WindowsJobObject.h"

#include <windows.h>
#endif

class CoreProcessHostTests : public QObject
{
    Q_OBJECT

private slots:
    void stopWithoutGracePeriodStopsTheConsoleProcessPromptly();
    void coreChildIsAssignedToTheAppJobObject();
    void coreHostStartMessageHasNoJobFallbackNoteWhenAssignmentWorks();

private:
    // Whether the direct-assignment probe already proved this environment can nest jobs.
    // Slots run in declaration order, so the host-message probe below can consult it: a
    // fallback note is only environmental when the direct probe fails in an outer job too.
    bool directAssignWorks_ = false;
};

#ifdef Q_OS_WIN
namespace {

// True when this test process already lives inside a job object, which makes assigning our own
// children to a second job fail for environmental reasons rather than product ones. Callers skip
// the live-child assertion in that case instead of failing it.
bool currentProcessIsAlreadyInAJob()
{
    BOOL inJob = FALSE;
    return IsProcessInJob(GetCurrentProcess(), nullptr, &inJob) && inJob;
}

QString systemPingPath()
{
    const QString pingPath = QDir(qEnvironmentVariable("SystemRoot") + QStringLiteral("/System32"))
                                 .filePath(QStringLiteral("ping.exe"));
    return QFileInfo::exists(pingPath) ? pingPath : QString();
}

} // namespace
#endif

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

// The job object is what keeps a killed or crashed app from leaving a core behind that still
// holds the ports: every core child joins one job with KILL_ON_JOB_CLOSE, so the OS reaps the
// members when our last handle closes no matter how we died. The PID file stays as the fallback
// for children that escaped assignment, not as the mechanism.
void CoreProcessHostTests::coreChildIsAssignedToTheAppJobObject()
{
#ifdef Q_OS_WIN
    // Creating a job object never depends on the environment; assigning a live child does -- a
    // runner that already placed this process in a restrictive job makes nesting fail. The two
    // halves are asserted separately so an environment limitation cannot masquerade as a product
    // regression, and a product regression cannot hide behind the skip.
    QVERIFY(WindowsJobObject::ensureAppJobInitialized());
    QVERIFY(WindowsJobObject::ensureAppJobInitialized());
    QVERIFY(WindowsJobObject::appJobHandleForTest() != nullptr);
    QVERIFY(!WindowsJobObject::assignProcessToAppJob(-1));
    QVERIFY(!WindowsJobObject::assignProcessToAppJob(0));

    const QString pingPath = QDir(qEnvironmentVariable("SystemRoot") + QStringLiteral("/System32"))
                                 .filePath(QStringLiteral("ping.exe"));
    if (!QFileInfo::exists(pingPath)) {
        QSKIP("ping.exe is not available; skipping the job assignment probe.");
    }

    QProcess child;
    child.setProgram(pingPath);
    child.setArguments(
        QStringList{QStringLiteral("-n"), QStringLiteral("60"), QStringLiteral("127.0.0.1")});
    child.start();
    QVERIFY2(child.waitForStarted(5000), qPrintable(child.errorString()));

    const qint64 pid = child.processId();
    const bool assigned = WindowsJobObject::assignProcessToAppJob(pid);
    child.kill();
    child.waitForFinished(5000);
    if (!assigned && currentProcessIsAlreadyInAJob()) {
        QSKIP("This test process already runs inside a job object, so nesting is refused; "
              "the live-child assignment cannot be exercised here.");
    }
    QVERIFY2(assigned, "A live child of this process must join the application job object.");
    directAssignWorks_ = true;
#else
    QSKIP("The job object probe is Windows specific.");
#endif
}

// Pins the production path, not just the helper: QtCoreProcessHost assigns each core child on
// `started`, and folds an assignment failure into the started message so the log panel shows it
// instead of it vanishing. When the assignment works -- the case this asserts -- the message is
// exactly the historical one, with no fallback note.
void CoreProcessHostTests::coreHostStartMessageHasNoJobFallbackNoteWhenAssignmentWorks()
{
#ifdef Q_OS_WIN
    const QString pingPath = systemPingPath();
    if (pingPath.isEmpty()) {
        QSKIP("ping.exe is not available; skipping the host start message probe.");
    }

    QtCoreProcessHost host;
    CoreInfo coreInfo;
    coreInfo.program = pingPath;
    coreInfo.arguments = QStringList{
        QStringLiteral("-n"), QStringLiteral("60"), QStringLiteral("127.0.0.1")};
    coreInfo.appendConfigArgument = false;
    coreInfo.captureOutput = true;

    QString startedMessage;
    bool notified = false;
    const OperationResult startResult = host.start(
        coreInfo,
        QString(),
        [](const QString&) {},
        [&startedMessage, &notified](const QString& message) {
            startedMessage = message;
            notified = true;
        });
    QVERIFY2(startResult.success, qPrintable(startResult.message));
    QTRY_VERIFY(notified);
    QVERIFY(host.stop(true).success);

    // Outcome-based: a fallback note is environmental only when this environment cannot nest
    // jobs at all, which the helper probe above has already measured. Here the direct probe
    // passed (directAssignWorks_), so nesting works and a fallback note from the host means the
    // host did not even try -- a wiring regression, not the weather.
    if (startedMessage != QStringLiteral("Core process started: ping.exe")
        && !directAssignWorks_ && currentProcessIsAlreadyInAJob()) {
        QSKIP("The host took the job fallback branch while this test process itself runs inside "
              "an outer job object that refuses nesting; the refusal is environmental.");
    }
    QCOMPARE(startedMessage, QStringLiteral("Core process started: ping.exe"));
#else
    QSKIP("The host start message probe is Windows specific.");
#endif
}

QTEST_MAIN(CoreProcessHostTests)

#include "CoreProcessHostTests.moc"
