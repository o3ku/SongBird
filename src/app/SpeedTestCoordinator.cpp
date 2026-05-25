#include "app/SpeedTestCoordinator.h"

#include <algorithm>
#include <utility>

#include "common/ServerDisplayName.h"

namespace {

VmessItem runtimeServerForLaunchCore(const VmessItem& server, CoreType launchCore)
{
    VmessItem runtimeServer = server;
    runtimeServer.coreType = launchCore;
    return runtimeServer;
}

} // namespace

SpeedTestCoordinator::SpeedTestCoordinator(Dependencies dependencies, QObject* parent)
    : QObject(parent)
    , deps_(std::move(dependencies))
{
    if (deps_.speedTestController != nullptr) {
        QObject::connect(
            deps_.speedTestController,
            &SpeedTestController::runningChanged,
            this,
            &SpeedTestCoordinator::handleRunningChanged);
        QObject::connect(
            deps_.speedTestController,
            &SpeedTestController::logGenerated,
            this,
            &SpeedTestCoordinator::handleLogGenerated);
        QObject::connect(
            deps_.speedTestController,
            &SpeedTestController::testResultReady,
            this,
            &SpeedTestCoordinator::handleTestResultReady);
        QObject::connect(
            deps_.speedTestController,
            &SpeedTestController::finished,
            this,
            &SpeedTestCoordinator::handleFinished);
    }
}

void SpeedTestCoordinator::startSpeedTest(const QStringList& indexIds)
{
    if (deps_.speedTestController == nullptr || deps_.backgroundTasks == nullptr || !deps_.mutableConfig) {
        if (deps_.appendResult) {
            deps_.appendResult(OperationResult::fail(QStringLiteral("Speed test service is unavailable.")));
        }
        return;
    }

    deps_.backgroundTasks->resetSpeedTestProgress();
    const BackgroundTaskCoordinator::Token token =
        deps_.backgroundTasks->tryBeginUserTask(BackgroundTaskCoordinator::Kind::SpeedTest);
    if (!token.isValid()) {
        return;
    }
    speedTestTaskToken_ = token;

    QStringList uniqueIds;
    for (const QString& indexId : indexIds) {
        const QString trimmed = indexId.trimmed();
        if (!trimmed.isEmpty() && !uniqueIds.contains(trimmed)) {
            uniqueIds.append(trimmed);
        }
    }

    QList<SpeedTestRequestItem> items;
    items.reserve(uniqueIds.size());
    for (const QString& indexId : uniqueIds) {
        const VmessItem* server = deps_.findServerById ? deps_.findServerById(indexId) : nullptr;
        if (server == nullptr) {
            continue;
        }

        const CoreType launchCore = deps_.resolveLaunchCoreType
            ? deps_.resolveLaunchCoreType(*server)
            : server->coreType;
        const VmessItem runtimeServer = runtimeServerForLaunchCore(*server, launchCore);

        items.append(SpeedTestRequestItem{
            *server,
            runtimeServer,
            deps_.resolveCoreInfo ? deps_.resolveCoreInfo(runtimeServer) : CoreInfo{}});
    }

    Config& config = deps_.mutableConfig();
    const OperationResult startResult = deps_.speedTestController->start(config, items);
    if (!startResult.success && deps_.appendResult) {
        deps_.appendResult(startResult);
    }

    if (!startResult.success) {
        deps_.backgroundTasks->resetSpeedTestProgress();
        deps_.backgroundTasks->finish(speedTestTaskToken_);
        speedTestTaskToken_ = {};
        return;
    }

    deps_.backgroundTasks->setSpeedTestTotalCount(items.size());
    deps_.backgroundTasks->syncState();
    static const QString pending = QStringLiteral("...");
    QStringList pendingIds;
    for (const auto& item : items) {
        auto it = std::find_if(config.collection().servers.begin(), config.collection().servers.end(),
            [&item](const VmessItem& server) { return server.indexId == item.server.indexId; });
        if (it != config.collection().servers.end()) {
            it->testResult = pending;
            pendingIds.append(it->indexId);
        }
    }
    if (deps_.updateServerTestResults) {
        deps_.updateServerTestResults(pendingIds, pending);
    }
    if (deps_.refreshTrayServers) {
        deps_.refreshTrayServers();
    }
}

void SpeedTestCoordinator::cancelActiveSpeedTest()
{
    if (deps_.speedTestController != nullptr) {
        deps_.speedTestController->cancel();
    }
    speedTestResultsDirty_ = false;
    speedTestTaskToken_ = {};
}

void SpeedTestCoordinator::handleRunningChanged(bool running)
{
    if (deps_.backgroundTasks == nullptr) {
        return;
    }

    if (!running) {
        deps_.backgroundTasks->resetSpeedTestProgress();
        if (!deps_.backgroundTasks->isCurrent(speedTestTaskToken_)) {
            speedTestResultsDirty_ = false;
            speedTestTaskToken_ = {};
            return;
        }

        if (speedTestResultsDirty_ && deps_.mutableConfig && deps_.saveConfig
            && !deps_.saveConfig(deps_.mutableConfig())) {
            if (deps_.appendResult) {
                deps_.appendResult(OperationResult::fail(
                    QStringLiteral("Failed to save configuration after updating test results.")));
            }
        }
        speedTestResultsDirty_ = false;
        deps_.backgroundTasks->finish(speedTestTaskToken_);
        speedTestTaskToken_ = {};
        return;
    }

    speedTestResultsDirty_ = false;
    deps_.backgroundTasks->syncState();
}

void SpeedTestCoordinator::handleLogGenerated(const QString& message)
{
    if (deps_.backgroundTasks == nullptr || !deps_.backgroundTasks->isCurrent(speedTestTaskToken_)) {
        return;
    }

    if (deps_.appendLog) {
        deps_.appendLog(message);
    }
    if (deps_.showTransientStatus) {
        deps_.showTransientStatus(message, 3000);
    }
}

void SpeedTestCoordinator::handleTestResultReady(const QString& indexId, const QString& result)
{
    if (deps_.backgroundTasks == nullptr || !deps_.backgroundTasks->isCurrent(speedTestTaskToken_)) {
        return;
    }

    const OperationResult updateResult = deps_.setTestResult
        ? deps_.setTestResult(indexId, result)
        : OperationResult::fail(QStringLiteral("Speed test result service is unavailable."));
    if (!updateResult.success) {
        if (deps_.appendResult) {
            deps_.appendResult(updateResult);
        }
        return;
    }

    speedTestResultsDirty_ = true;
    const VmessItem* speedTestServer = deps_.findServerById ? deps_.findServerById(indexId) : nullptr;
    const QString serverName = speedTestServer == nullptr ? QString() : serverDisplayName(*speedTestServer);
    deps_.backgroundTasks->recordSpeedTestResult(serverName);
    deps_.backgroundTasks->syncState();

    if (deps_.updateServerTestResult) {
        deps_.updateServerTestResult(indexId, result);
    }
    if (deps_.refreshTrayServers) {
        deps_.refreshTrayServers();
    }
}

void SpeedTestCoordinator::handleFinished(const QString& summary)
{
    if (deps_.backgroundTasks == nullptr || !deps_.backgroundTasks->isCurrent(speedTestTaskToken_)) {
        return;
    }
    if (deps_.showTransientStatus) {
        deps_.showTransientStatus(summary, 4000);
    }
}
