#include <atomic>
#include <memory>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include "appcore/BackgroundThreadTracker.h"
#include "common/ThreadShutdown.h"

namespace {

int outcomeValue(ThreadShutdownOutcome outcome)
{
    return static_cast<int>(outcome);
}

QStringList recordLines(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

/// A worker that never looks at QThread::isInterruptionRequested(). It runs until `stopFlag` is
/// cleared, so a test can decide when -- and whether -- it stops. This is what a worker blocked in
/// a syscall looks like from the tracker's side.
QThread* launchWorkerThatIgnoresInterruption(const std::shared_ptr<std::atomic_bool>& stopFlag)
{
    return QThread::create([stopFlag]() {
        while (stopFlag->load()) {
            QThread::msleep(5);
        }
    });
}

ThreadShutdownBudget shortBudget(const QString& recordPath, unsigned long hardCapMs)
{
    ThreadShutdownBudget budget;
    budget.cooperativeMs = 100;
    budget.tolerantMs = 100;
    budget.hardCapMs = hardCapMs;
    budget.recordPath = recordPath;
    return budget;
}

} // namespace

class ThreadShutdownTests : public QObject
{
    Q_OBJECT

private slots:
    void nullThreadIsReportedAsFinished();
    void cooperativeWorkerStopsWithoutWritingARecord();
    void workerThatStopsLateIsStillReportedAsFinished();
    void workerThatIgnoresInterruptionIsAbandonedAtTheHardCap();
    void zeroBudgetAbandonsWithoutWaiting();
    void recordIsAppendedRatherThanOverwritten();
    void recordReportsAnEmptyPathWhenNothingCouldBeWritten();
    void defaultRecordPathIsAnAbsoluteLogFile();
    void trackerReportsAbandonmentAndKeepsTheWorkerTracked();
    void exitProbeEndsTheProcessWithoutUnwinding();
};

void ThreadShutdownTests::nullThreadIsReportedAsFinished()
{
    QCOMPARE(outcomeValue(stopThreadForShutdown(nullptr)), outcomeValue(ThreadShutdownOutcome::Finished));
}

void ThreadShutdownTests::cooperativeWorkerStopsWithoutWritingARecord()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString recordPath = tempDir.filePath(QStringLiteral("shutdown.log"));

    QThread* worker = QThread::create([]() {
        while (!QThread::currentThread()->isInterruptionRequested()) {
            QThread::msleep(5);
        }
    });
    worker->start();

    QElapsedTimer elapsed;
    elapsed.start();
    const ThreadShutdownOutcome outcome = stopThreadForShutdown(worker, shortBudget(recordPath, 5000));

    QCOMPARE(outcomeValue(outcome), outcomeValue(ThreadShutdownOutcome::Finished));
    QVERIFY2(elapsed.elapsed() < 1000, qPrintable(QString::number(elapsed.elapsed())));
    // A worker that honors interruption is the normal case, so it must not leave a record behind.
    QVERIFY(!QFile::exists(recordPath));

    QVERIFY(worker->wait(5000));
    delete worker;
}

void ThreadShutdownTests::workerThatStopsLateIsStillReportedAsFinished()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString recordPath = tempDir.filePath(QStringLiteral("shutdown.log"));

    // Ignores interruption and stops on its own after 300 ms: long enough to outlast the 100 ms
    // cooperative window, short enough to land inside the hard cap.
    QThread* worker = QThread::create([]() { QThread::msleep(300); });
    worker->start();

    ThreadShutdownBudget budget = shortBudget(recordPath, 5000);
    budget.tolerantMs = 1000;
    const ThreadShutdownOutcome outcome = stopThreadForShutdown(worker, budget);

    QCOMPARE(outcomeValue(outcome), outcomeValue(ThreadShutdownOutcome::Finished));
    const QStringList lines = recordLines(recordPath);
    QCOMPARE(lines.size(), 1);
    QVERIFY(lines.first().contains(QStringLiteral("did not honor interruption")));

    QVERIFY(worker->wait(5000));
    delete worker;
}

void ThreadShutdownTests::workerThatIgnoresInterruptionIsAbandonedAtTheHardCap()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString recordPath = tempDir.filePath(QStringLiteral("shutdown.log"));

    auto stopFlag = std::make_shared<std::atomic_bool>(true);
    QThread* worker = launchWorkerThatIgnoresInterruption(stopFlag);
    worker->start();

    QElapsedTimer elapsed;
    elapsed.start();
    const ThreadShutdownOutcome outcome = stopThreadForShutdown(worker, shortBudget(recordPath, 400));
    const qint64 spent = elapsed.elapsed();

    QCOMPARE(outcomeValue(outcome), outcomeValue(ThreadShutdownOutcome::Abandoned));
    QVERIFY2(spent >= 350, qPrintable(QString::number(spent)));
    // The worker is still running at this point, so anything close to the cap proves the wait was
    // bounded rather than "long enough for this worker".
    QVERIFY2(spent < 1500, qPrintable(QString::number(spent)));

    const QStringList lines = recordLines(recordPath);
    QCOMPARE(lines.size(), 2);
    QVERIFY(lines.first().contains(QStringLiteral("did not honor interruption")));
    QVERIFY(lines.last().contains(QStringLiteral("giving up")));

    stopFlag->store(false);
    QVERIFY(worker->wait(5000));
    delete worker;
}

void ThreadShutdownTests::zeroBudgetAbandonsWithoutWaiting()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString recordPath = tempDir.filePath(QStringLiteral("shutdown.log"));

    ThreadShutdownBudget budget;
    budget.cooperativeMs = 0;
    budget.tolerantMs = 0;
    budget.hardCapMs = 0;
    budget.recordPath = recordPath;

    auto stopFlag = std::make_shared<std::atomic_bool>(true);
    QThread* worker = launchWorkerThatIgnoresInterruption(stopFlag);
    worker->start();

    QElapsedTimer elapsed;
    elapsed.start();
    const ThreadShutdownOutcome outcome = stopThreadForShutdown(worker, budget);

    QCOMPARE(outcomeValue(outcome), outcomeValue(ThreadShutdownOutcome::Abandoned));
    QVERIFY2(elapsed.elapsed() < 500, qPrintable(QString::number(elapsed.elapsed())));
    QCOMPARE(recordLines(recordPath).size(), 2);

    stopFlag->store(false);
    QVERIFY(worker->wait(5000));
    delete worker;
}

void ThreadShutdownTests::recordIsAppendedRatherThanOverwritten()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString recordPath = tempDir.filePath(QStringLiteral("nested/shutdown.log"));

    QCOMPARE(appendShutdownRecord(QStringLiteral("first"), recordPath), recordPath);
    QCOMPARE(appendShutdownRecord(QStringLiteral("second"), recordPath), recordPath);

    const QStringList lines = recordLines(recordPath);
    QCOMPARE(lines.size(), 2);
    QVERIFY(lines.at(0).endsWith(QStringLiteral("first")));
    QVERIFY(lines.at(1).endsWith(QStringLiteral("second")));
}

void ThreadShutdownTests::recordReportsAnEmptyPathWhenNothingCouldBeWritten()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // The parent "directory" is a regular file, so neither mkpath nor the open can succeed.
    const QString blocker = tempDir.filePath(QStringLiteral("blocker"));
    QFile blockerFile(blocker);
    QVERIFY(blockerFile.open(QIODevice::WriteOnly));
    blockerFile.write("not a directory");
    blockerFile.close();

    QVERIFY(appendShutdownRecord(QStringLiteral("nowhere"), blocker + QStringLiteral("/shutdown.log")).isEmpty());
}

void ThreadShutdownTests::defaultRecordPathIsAnAbsoluteLogFile()
{
    const QString path = defaultShutdownRecordPath();
    QVERIFY(path.endsWith(QStringLiteral("shutdown-hang.log")));
    QVERIFY(QDir::isAbsolutePath(path));
}

void ThreadShutdownTests::trackerReportsAbandonmentAndKeepsTheWorkerTracked()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString recordPath = tempDir.filePath(QStringLiteral("tracker.log"));

    BackgroundThreadTracker tracker(shortBudget(recordPath, 400));

    auto stopFlag = std::make_shared<std::atomic_bool>(true);
    QThread* worker = launchWorkerThatIgnoresInterruption(stopFlag);
    QPointer<QThread> guard(worker);
    tracker.track(worker);
    worker->start();

    // The named defect: this used to be a loop with no upper bound, so this call could not return.
    QVERIFY(!tracker.waitForAll());
    QVERIFY(QFile::exists(recordPath));

    stopFlag->store(false);
    QVERIFY(guard.isNull() || guard->wait(5000));

    // The abandoned worker was kept in the list, so once it exits the tracker can report success.
    QVERIFY(tracker.waitForAll());
}

void ThreadShutdownTests::exitProbeEndsTheProcessWithoutUnwinding()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString recordPath = tempDir.filePath(QStringLiteral("probe.log"));

    // Windows logs "QProcess: CreateFile failed. (All pipe instances are busy.)" during this start
    // on every run -- including the runs below that show the child starting, exiting with the
    // expected code and writing the record. Redirecting the child's streams does not change it, so
    // it is Qt's own named-pipe setup being refused rather than the child failing to run.
    QProcess process;
    process.setProgram(QCoreApplication::applicationFilePath());
    process.setArguments({QStringLiteral("--abandon-probe"), recordPath});
    process.start();
    QVERIFY(process.waitForStarted(5000));
    QVERIFY(process.waitForFinished(10000));

    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), static_cast<int>(EXIT_FAILURE));

    const QStringList lines = recordLines(recordPath);
    QCOMPARE(lines.size(), 1);
    QVERIFY(lines.first().contains(QStringLiteral("ending the process without unwinding")));
}

int main(int argc, char** argv)
{
    // abandonProcessAfterStuckThread() cannot be observed from inside the process it ends, so the
    // suite re-runs itself as a subprocess and checks the exit code and the record from outside.
    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--abandon-probe")) {
        abandonProcessAfterStuckThread(QStringLiteral("test probe"), QString::fromLocal8Bit(argv[2]));
        return 0;
    }

    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SongBird"));
    QCoreApplication::setApplicationName(QStringLiteral("SongBirdTests"));

    ThreadShutdownTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "ThreadShutdownTests.moc"
