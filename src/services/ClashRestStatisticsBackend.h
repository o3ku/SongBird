#pragma once

#include <QObject>
#include <QString>

#include "common/OperationResult.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class ClashRestStatisticsBackend final : public QObject {
    Q_OBJECT

public:
    explicit ClashRestStatisticsBackend(QObject* parent = nullptr);
    ~ClashRestStatisticsBackend() override;

    bool isRunning() const { return running_; }
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
    bool running_ = false;
    bool available_ = false;
    bool hasBaseline_ = false;
    quint64 baselineUp_ = 0;
    quint64 baselineDown_ = 0;
};
