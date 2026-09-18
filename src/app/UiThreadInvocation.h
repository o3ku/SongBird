#pragma once

#include <utility>

#include <QMetaObject>
#include <QObject>
#include <QThread>

/// Runs `callback` on the thread that owns `context`.
///
/// This was previously copy-pasted verbatim into three translation units
/// (AppBootstrap, CoreUpdateCoordinator, GeoResourceUpdateCoordinator); keeping
/// one definition here avoids the copies drifting apart.
template <typename Callback>
void invokeOnUiThread(QObject* context, Callback&& callback)
{
    if (context == nullptr) {
        return;
    }

    if (QThread::currentThread() == context->thread()) {
        callback();
        return;
    }

    QMetaObject::invokeMethod(context, std::forward<Callback>(callback), Qt::QueuedConnection);
}
