#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QStringList>

#include "app/BackgroundTaskCoordinator.h"
#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"
#include "services/SpeedTestController.h"

class SpeedTestCoordinator final : public QObject
{
    Q_OBJECT

public:
    struct Dependencies {
        BackgroundTaskCoordinator* backgroundTasks = nullptr;
        SpeedTestController* speedTestController = nullptr;
        std::function<Config&()> mutableConfig;
        std::function<bool(Config&)> saveConfig;
        std::function<const VmessItem*(const QString&)> findServerById;
        std::function<CoreType(const VmessItem&)> resolveLaunchCoreType;
        std::function<CoreInfo(const VmessItem&)> resolveCoreInfo;
        std::function<OperationResult(const QString&, const QString&)> setTestResult;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void(const QString&)> appendLog;
        std::function<void(const QString&, const QString&)> updateServerTestResult;
        std::function<void(const QStringList&, const QString&)> updateServerTestResults;
        std::function<void()> refreshTrayServers;
    };

    explicit SpeedTestCoordinator(Dependencies dependencies, QObject* parent = nullptr);

    void startSpeedTest(const QStringList& indexIds);
    void cancelActiveSpeedTest();

private:
    void handleRunningChanged(bool running);
    void handleLogGenerated(const QString& message);
    void handleTestResultReady(const QString& indexId, const QString& result);
    void handleFinished(const QString& summary);

    Dependencies deps_;
    BackgroundTaskCoordinator::Token speedTestTaskToken_;
    bool speedTestResultsDirty_ = false;
};
