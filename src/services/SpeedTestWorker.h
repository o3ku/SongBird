#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <atomic>

#include "domain/models/Config.h"
#include "services/SpeedTestRequestItem.h"

class SpeedTestWorker final : public QObject {
    Q_OBJECT

public:
    explicit SpeedTestWorker(QString customConfigDirectory, std::atomic_bool& cancelFlag, QObject* parent = nullptr);

    void runBatch(const Config& config, const QList<SpeedTestRequestItem>& items);

signals:
    void logGenerated(const QString& message);
    void testResultReady(const QString& indexId, const QString& result);
    void batchFinished(const QString& summary);

private:
    void runDirectProxyGroup(
        const QList<int>& indices,
        const QList<SpeedTestRequestItem>& items,
        const QString& urlTestUrl,
        int& completed);

    void runFallbackGroup(
        const QList<int>& indices,
        const QList<SpeedTestRequestItem>& items,
        const Config& probeConfigTemplate,
        const QString& urlTestUrl,
        int& completed);

    QString customConfigDirectory_;
    std::atomic_bool& cancelled_;
};
