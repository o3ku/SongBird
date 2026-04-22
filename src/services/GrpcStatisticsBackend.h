#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "common/OperationResult.h"

class GrpcStatisticsBackend final : public QObject {
    Q_OBJECT

public:
    explicit GrpcStatisticsBackend(QObject* parent = nullptr);
    ~GrpcStatisticsBackend() override;

    bool isSupported() const;
    bool isRunning() const;
    OperationResult start(const QString& host, int port, int refreshRateSeconds);
    void stop();

signals:
    void trafficDeltaReceived(quint64 up, quint64 down);
    void pollingAvailabilityChanged(bool available);

private:
    void runWorker(QString endpoint, int refreshRateSeconds);
    void updateAvailability(bool available);

    std::atomic_bool running_ = false;
    std::atomic_bool stopRequested_ = false;
    std::atomic_bool pollingAvailable_ = false;
    std::condition_variable stopCondition_;
    std::mutex stopMutex_;
    std::thread worker_;
};
