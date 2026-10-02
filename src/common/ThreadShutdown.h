#pragma once

#include <QString>

class QThread;

/// Result of one bounded wait for a worker thread to stop.
enum class ThreadShutdownOutcome
{
    /// The worker stopped within the budget. The normal case.
    Finished,
    /// The worker ignored interruption for the whole budget and is still running. The caller
    /// must not free anything the worker can still reach.
    Abandoned,
};

/// How long one worker thread is given to stop during shutdown.
struct ThreadShutdownBudget
{
    /// A worker that polls QThread::isInterruptionRequested() at its iteration boundaries exits
    /// inside this window, so nothing is logged and nothing is recorded.
    unsigned long cooperativeMs = 5000;
    /// Granted only once the worker has proven it is not honoring interruption -- a worker
    /// blocked in a syscall, or draining a batch it cannot abandon, lands here.
    unsigned long tolerantMs = 5000;
    /// Upper bound on the total wait, measured from entry. This is the point of the helper: the
    /// loops it replaces waited forever.
    unsigned long hardCapMs = 30000;
    /// Where the record is appended. Empty means defaultShutdownRecordPath().
    QString recordPath;
};

/// Stops `thread` within `budget` instead of waiting for it forever.
///
/// The ladder, in order:
///   1. requestInterruption() + quit() -- what every worker in this codebase polls.
///   2. wait(cooperativeMs)            -- the normal case: silent and fast.
///   3. one qWarning + one durable record, then wait(tolerantMs) until hardCapMs.
///   4. return Abandoned, having appended a second record.
///
/// Step 4 deliberately does not kill the thread. QThread::terminate() is TerminateThread, and it
/// leaves the QThread object's own state stuck: QThreadPrivate::finish() never runs, so wait()
/// never succeeds again and destroying the object trips QThread's "Destroyed while thread is
/// still running" qFatal. It can also leave the worker holding the CRT heap lock, in which case
/// the caller deadlocks on its next allocation instead of exiting -- a worse hang than the one
/// being fixed. Abandoning instead hands the decision back to the caller, which is the only place
/// that knows whether it can leak rather than free.
///
/// `nullptr` returns Finished.
ThreadShutdownOutcome stopThreadForShutdown(QThread* thread, const ThreadShutdownBudget& budget = {});

/// Appends a timestamped `message` to the shutdown record and mirrors it to the log stream.
/// Returns the file path, or an empty string when nothing could be written.
///
/// The file exists because a GUI process's stderr is not visible to anyone. This line is the only
/// clue left behind by a shutdown that had to give up on a worker, so it is written before the
/// process may die.
QString appendShutdownRecord(const QString& message, const QString& recordPath = {});

/// `shutdown-hang.log` under QStandardPaths::AppDataLocation.
QString defaultShutdownRecordPath();

/// Ends the process immediately, without running destructors, and never returns.
///
/// Called once stopThreadForShutdown() reports Abandoned and the caller has established that it
/// cannot free what the worker can still reach. Skipping the remaining teardown is the point:
/// unwinding would free exactly those objects. The record is appended first so it survives even
/// if the abandoned worker is holding a lock the process would otherwise deadlock on.
[[noreturn]] void abandonProcessAfterStuckThread(const QString& context, const QString& recordPath = {});
