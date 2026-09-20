#pragma once

#include <functional>
#include <utility>

#include <QObject>
#include <QThread>

/// Creates a worker thread that runs `task`, hands it to `track` when one is supplied, and
/// starts it. Returns the thread so callers that need the handle can keep it.
///
/// This sequence used to be written out by hand at every call site and the copies drifted
/// apart: some tracked the thread unconditionally, some only when a tracker had been
/// injected, and the ones with no fallback leaked the QThread object whenever no tracker
/// was present. Every tracker in this codebase already deletes the thread on `finished`
/// (BackgroundThreadTracker::track, ProxySession::trackBackgroundThread,
/// SongBirdAutoCoordinator::trackThread), so routing the decision through one place only
/// adds the missing branch -- it does not change what happens when a tracker exists.
template <typename Task>
QThread* launchBackgroundThread(Task&& task, const std::function<void(QThread*)>& track)
{
    QThread* thread = QThread::create(std::forward<Task>(task));
    if (track) {
        track(thread);
    } else {
        QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }
    return thread;
}

/// Overload for call sites that manage the thread's lifetime themselves.
template <typename Task>
QThread* launchBackgroundThread(Task&& task)
{
    QThread* thread = QThread::create(std::forward<Task>(task));
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    return thread;
}
