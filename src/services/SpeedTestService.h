#pragma once

#include <QObject>
#include <QList>
#include <QStringList>
#include <atomic>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"
class QThread;
class QObject;

enum class SpeedTestMode {
    Ping = 0,
    TcpPing,
    RealPing,
    DownloadSpeed
};

struct SpeedTestRequestItem {
    VmessItem server;
    CoreInfo coreInfo;
};

class SpeedTestService final : public QObject {
    Q_OBJECT

public:
    explicit SpeedTestService(QString customConfigDirectory = {}, QObject* parent = nullptr);
    ~SpeedTestService() override;

    OperationResult start(SpeedTestMode mode, const Config& config, const QList<SpeedTestRequestItem>& items);
    bool isRunning() const;
    void cancel();

signals:
    void runningChanged(bool running);
    void testResultReady(const QString& indexId, const QString& result);
    void logGenerated(const QString& message);
    void finished(const QString& summary);

private:
    QString customConfigDirectory_;
    QObject* worker_ = nullptr;
    QThread* workerThread_ = nullptr;
    std::atomic_bool running_{false};
    std::atomic_bool cancelled_{false};
};
