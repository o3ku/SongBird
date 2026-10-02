#pragma once

#include <QList>
#include <QPointer>

#include "common/ThreadShutdown.h"

class QThread;

class BackgroundThreadTracker final {
public:
    /// `budget` bounds how long waitForAll() may spend on one worker. It is a constructor
    /// argument because otherwise the give-up branch below has no reachable coverage: a test
    /// would have to wait out the production cap to see it.
    explicit BackgroundThreadTracker(ThreadShutdownBudget budget = {});

    void track(QThread* thread);
    void requestInterruptionAll();

    /// Asks every tracked worker to stop and waits for it, with an upper bound (see
    /// common/ThreadShutdown.h for the ladder and why it is bounded).
    ///
    /// Returns false when a worker ignored interruption for the whole budget and is still
    /// running. The caller must then not free anything that worker can reach -- the tracked
    /// workers call QMetaObject::invokeMethod() on their owner, which dereferences it. A worker
    /// that has not stopped stays in the list, so a later call can try again.
    bool waitForAll();

private:
    void pruneFinished();

    ThreadShutdownBudget budget_;
    QList<QPointer<QThread>> threads_;
};
