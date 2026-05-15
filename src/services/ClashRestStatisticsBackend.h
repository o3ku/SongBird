#pragma once

#include <QObject>
#include <QString>

#include <atomic>

#include "common/OperationResult.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class ClashRestStatisticsBackend final : public QObject {
    Q_OBJECT

public:
    explicit ClashRestStatisticsBackend(QObject* parent = nullptr);
    ~ClashRestStatisticsBackend() override;

    bool isRunning() const { return running_.load(); }
    OperationResult start(const QString& host, int port, int refreshRateSeconds);
    void stop();

signals:
    void trafficDeltaReceived(quint64 up, quint64 down);
    void pollingAvailabilityChanged(bool available);

private:
    void poll();
    void handleReply(QNetworkReply* reply);
    void updateAvailability(bool available);

    QNetworkAccessManager* networkManager_ = nullptr;
    QTimer* timer_ = nullptr;
    QString endpoint_;
    std::atomic<bool> running_{false};
    std::atomic<bool> available_{false};
    std::atomic<bool> hasBaseline_{false};
    std::atomic<quint64> baselineUp_{0};
    std::atomic<quint64> baselineDown_{0};
};
