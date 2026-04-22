#pragma once

#include <QObject>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/ServerStatistics.h"
#include "domain/models/StatisticsSessionState.h"

class StatisticsService final : public QObject {
    Q_OBJECT

public:
    explicit StatisticsService(QString statisticsFilePath, QObject* parent = nullptr);

    void load();
    void save();
    void startSession(
        const Config& config,
        const QString& activeServerId,
        CoreType runtimeCore,
        int statePort,
        bool runtimeConfigReady,
        bool externallyManaged = false);
    void stopSession();
    void setPollingAvailable(bool available);
    void applyTrafficDelta(const QString& itemId, quint64 up, quint64 down);
    OperationResult clearAll();

    QList<ServerStatItem> statistics() const;
    ServerStatItem currentServerStat() const;
    StatisticsSessionState sessionState() const;

signals:
    void statisticsChanged();
    void sessionChanged();

private:
    void normalizeLoadedStatistics();
    ServerStatItem* ensureServerStat(const QString& itemId, bool* changed = nullptr);
    static bool normalizeServerStat(ServerStatItem& item, qint64 currentTicks);
    static qint64 currentDateTicks();

    QString statisticsFilePath_;
    ServerStatistics statistics_;
    StatisticsSessionState sessionState_;
};
