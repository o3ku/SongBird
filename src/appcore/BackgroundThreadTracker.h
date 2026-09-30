#pragma once

#include <QList>
#include <QPointer>

class QThread;

class BackgroundThreadTracker final {
public:
    void track(QThread* thread);
    void requestInterruptionAll();
    void waitForAll();

private:
    void pruneFinished();

    QList<QPointer<QThread>> threads_;
};
