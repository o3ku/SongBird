#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <atomic>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "services/SpeedTestRequestItem.h"

class QThread;
class SpeedTestWorker;

class SpeedTestController final : public QObject {
    Q_OBJECT

public:
    explicit SpeedTestController(QString customConfigDirectory = {}, QObject* parent = nullptr);
    ~SpeedTestController() override;

    OperationResult start(const Config& config, const QList<SpeedTestRequestItem>& items);
    bool isRunning() const;
    void cancel();

signals:
    void runningChanged(bool running);
    void testResultReady(const QString& indexId, const QString& result);
    void logGenerated(const QString& message);
    void finished(const QString& summary);

private:
    QString customConfigDirectory_;
    SpeedTestWorker* worker_ = nullptr;
    QThread* workerThread_ = nullptr;
    std::atomic_bool running_{false};
    std::atomic_bool cancelled_{false};
};
