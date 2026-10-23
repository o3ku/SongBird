#include "common/ThreadShutdown.h"

#include <cstdlib>

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QStringList>

namespace {

QString describeThread(const QThread* thread)
{
    const QString objectName = thread->objectName();
    const QString label = objectName.trimmed().isEmpty() ? QStringLiteral("(unnamed)") : objectName;
    return QStringLiteral("%1 [%2]")
        .arg(label, QString::number(reinterpret_cast<quintptr>(thread), 16));
}

} // namespace

QString defaultShutdownRecordPath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.trimmed().isEmpty()) {
        return {};
    }

    return QDir(base).filePath(QStringLiteral("shutdown-hang.log"));
}

QString appendShutdownRecord(const QString& message, const QString& recordPath)
{
    qWarning("ThreadShutdown: %s", qUtf8Printable(message));

    const QString path = recordPath.trimmed().isEmpty() ? defaultShutdownRecordPath() : recordPath;
    if (path.trimmed().isEmpty()) {
        return {};
    }

    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODate) << ' ' << message << '\n';
    stream.flush();
    file.close();

    return path;
}

ThreadShutdownOutcome stopThreadForShutdown(QThread* thread, const ThreadShutdownBudget& budget)
{
    if (thread == nullptr) {
        return ThreadShutdownOutcome::Finished;
    }

    thread->requestInterruption();
    thread->quit();

    QElapsedTimer elapsed;
    elapsed.start();

    // Time left before the hard cap, or 0 once it is spent. Every wait below is clamped to it, so
    // the total can never exceed the cap no matter how the worker behaves.
    const auto remainingMs = [&elapsed, &budget]() -> unsigned long {
        const qint64 used = elapsed.elapsed();
        if (used >= static_cast<qint64>(budget.hardCapMs)) {
            return 0;
        }

        return static_cast<unsigned long>(static_cast<qint64>(budget.hardCapMs) - used);
    };

    // Step 2: the normal case. Every worker in this codebase polls interruption, so this wait
    // returning true is the entire cost of a clean shutdown.
    if (thread->wait(qMin(budget.cooperativeMs, remainingMs()))) {
        return ThreadShutdownOutcome::Finished;
    }

    // Step 3: the worker is not honoring interruption. Say so once, durably, then keep trying --
    // still bounded, because a worker that never stops must not be able to pin the process open.
    appendShutdownRecord(
        QStringLiteral("background thread did not honor interruption within %1ms; shutdown budget is %2ms: %3")
            .arg(QString::number(budget.cooperativeMs), QString::number(budget.hardCapMs), describeThread(thread)),
        budget.recordPath);

    while (remainingMs() > 0) {
        const unsigned long chunkMs = qMin(budget.tolerantMs, remainingMs());
        if (chunkMs == 0) {
            break;
        }

        thread->requestInterruption();
        thread->quit();
        if (thread->wait(chunkMs)) {
            return ThreadShutdownOutcome::Finished;
        }
    }

    // Step 4: give up rather than wait forever. Nothing is killed; the caller decides.
    appendShutdownRecord(
        QStringLiteral("giving up on background thread after %1ms: %2")
            .arg(QString::number(budget.hardCapMs), describeThread(thread)),
        budget.recordPath);

    return ThreadShutdownOutcome::Abandoned;
}

ThreadShutdownOutcome collectThreadForShutdown(
    QThread* thread,
    const ThreadShutdownBudget& budget,
    QStringList* abandonedObjectNames)
{
    if (thread == nullptr) {
        return ThreadShutdownOutcome::Finished;
    }

    // Deliberately no helper thread. An earlier version handed the wait to a QThread created here and
    // invoked the ladder on it with Qt::BlockingQueuedConnection -- which queues the call on the
    // receiver's thread. The collector lives in the calling thread, so the caller was asking its own
    // event loop for a slot while blocking that loop: Qt logged "Dead lock detected", dropped the
    // call, and the function went on to wait for a collector that had never run its body. It hung on
    // exactly the shutdown path this file exists to bound.
    //
    // The ladder does not need one. Its waits all carry a timeout (`wait(ms)`), and the one call Qt
    // refuses from a thread's own event loop -- waiting on itself for longer than the event loop
    // will ever process -- is answered before it happens: the caller may already have interrupted
    // this very thread, which isFinished() reports, and the waits below are clamped to a budget that
    // ends whether or not the worker cooperates.
    const QString label = describeThread(thread);
    const ThreadShutdownOutcome outcome = stopThreadForShutdown(thread, budget);

    if (outcome == ThreadShutdownOutcome::Abandoned && abandonedObjectNames != nullptr) {
        abandonedObjectNames->append(label);
    }

    return outcome;
}

void abandonProcessAfterStuckThread(const QString& context, const QString& recordPath)
{
    appendShutdownRecord(
        QStringLiteral("ending the process without unwinding because %1").arg(context),
        recordPath);

    std::_Exit(EXIT_FAILURE);
}
