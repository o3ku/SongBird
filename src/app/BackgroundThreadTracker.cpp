#include "app/BackgroundThreadTracker.h"

#include <QThread>

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

void BackgroundThreadTracker::waitForAll()
{
    // Workers cooperatively check QThread::isInterruptionRequested() at their
    // iteration boundaries or through service-level cancel flags. Keep waiting
    // during destruction because tracked workers may still capture owner state.
    constexpr unsigned long kShutdownWaitMs = 15000;
    const QList<QPointer<QThread>> threads = threads_;
    for (const QPointer<QThread>& threadGuard : threads) {
        QThread* thread = threadGuard.data();
        if (thread == nullptr) {
            continue;
        }

        thread->requestInterruption();
        while (!thread->wait(kShutdownWaitMs)) {
            qWarning("BackgroundThreadTracker: background thread did not honor interruption within %lums; still waiting",
                kShutdownWaitMs);
            thread->requestInterruption();
        }
    }
    threads_.clear();
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
