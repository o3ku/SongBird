#include "services/SpeedTestController.h"

#include <QMetaObject>
#include <QThread>

#include <utility>

#include "services/SpeedTestWorker.h"

SpeedTestController::SpeedTestController(QString customConfigDirectory, QObject* parent)
    : QObject(parent)
    , customConfigDirectory_(std::move(customConfigDirectory))
{
    workerThread_ = new QThread(this);
    auto* worker = new SpeedTestWorker(customConfigDirectory_, cancelled_);
    worker_ = worker;
    worker->moveToThread(workerThread_);

    QObject::connect(workerThread_, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(worker, &SpeedTestWorker::logGenerated, this, &SpeedTestController::logGenerated);
    QObject::connect(worker, &SpeedTestWorker::testResultReady, this, &SpeedTestController::testResultReady);
    QObject::connect(worker, &SpeedTestWorker::batchFinished, this, [this](const QString& summary) {
        running_ = false;
        emit logGenerated(summary);
        emit finished(summary);
        emit runningChanged(false);
    });

    workerThread_->start();
}

OperationResult SpeedTestController::start(const Config& config, const QList<SpeedTestRequestItem>& items)
{
    if (running_) {
        return OperationResult::fail(QStringLiteral("Another speed test batch is already running."));
    }

    if (items.isEmpty()) {
        return OperationResult::fail(QStringLiteral("No servers were selected for speed testing."));
    }

    if (worker_ == nullptr || workerThread_ == nullptr || !workerThread_->isRunning()) {
        return OperationResult::fail(QStringLiteral("Speed test worker thread is unavailable."));
    }

    cancelled_ = false;
    running_ = true;
    const QString message = QStringLiteral("URL Test started for %1 server(s).")
                                .arg(items.size());
    emit runningChanged(true);
    emit logGenerated(message);

    auto* worker = worker_;
    const bool invoked = QMetaObject::invokeMethod(
        worker,
        [worker, config, items]() {
            worker->runBatch(config, items);
        },
        Qt::QueuedConnection);
    if (!invoked) {
        running_ = false;
        emit runningChanged(false);
        return OperationResult::fail(QStringLiteral("Failed to queue the speed test batch."));
    }

    return OperationResult::ok(message);
}

bool SpeedTestController::isRunning() const
{
    return running_;
}

void SpeedTestController::cancel()
{
    if (running_) {
        cancelled_ = true;
    }
}

SpeedTestController::~SpeedTestController()
{
    if (workerThread_ == nullptr) {
        return;
    }

    // The worker checks cancelled_ at every batch-loop iteration. Without this
    // signal it keeps spawning per-server probes, and the wait() below can
    // block until a large pending batch drains.
    cancelled_ = true;

    workerThread_->quit();
    while (!workerThread_->wait(5000)) {
        workerThread_->requestInterruption();
        workerThread_->quit();
        qWarning("SpeedTestController: worker thread did not exit before shutdown; still waiting");
    }
}
