#include "appcore/BackgroundThreadTracker.h"

#include <utility>

#include <QThread>

#include "common/ThreadShutdown.h"

BackgroundThreadTracker::BackgroundThreadTracker(ThreadShutdownBudget budget)
    : budget_(std::move(budget))
{
}

void BackgroundThreadTracker::track(QThread* thread)
{
    if (thread == nullptr) {
        return;
    }

    pruneFinished();
    threads_.append(thread);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
}

void BackgroundThreadTracker::requestInterruptionAll()
{
    pruneFinished();
    const QList<QPointer<QThread>> threads = threads_;
    for (const QPointer<QThread>& threadGuard : threads) {
        QThread* thread = threadGuard.data();
        if (thread != nullptr) {
            thread->requestInterruption();
        }
    }
}

bool BackgroundThreadTracker::waitForAll()
{
    // Workers cooperatively check QThread::isInterruptionRequested() at their
    // iteration boundaries or through service-level cancel flags. Keep waiting
    // during destruction because tracked workers may still capture owner state --
    // but only up to ThreadShutdownBudget's hard cap, because an unbounded wait
    // is a shutdown that can never complete.
    bool allStopped = true;
    const QList<QPointer<QThread>> threads = threads_;
    for (const QPointer<QThread>& threadGuard : threads) {
        QThread* thread = threadGuard.data();
        if (thread == nullptr) {
            threads_.removeOne(threadGuard);
            continue;
        }

        if (stopThreadForShutdown(thread, budget_) == ThreadShutdownOutcome::Abandoned) {
            // Still running, and it may finish later: keep it tracked so a later
            // call can try again.
            allStopped = false;
            continue;
        }

        threads_.removeOne(threadGuard);
    }

    return allStopped;
}

void BackgroundThreadTracker::pruneFinished()
{
    for (int index = threads_.size() - 1; index >= 0; --index) {
        const QPointer<QThread>& thread = threads_.at(index);
        if (thread.isNull() || !thread->isRunning()) {
            threads_.removeAt(index);
        }
    }
}
