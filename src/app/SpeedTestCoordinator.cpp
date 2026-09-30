#include "app/SpeedTestCoordinator.h"

#include <algorithm>
#include <utility>

#include <QCoreApplication>

#include "app/SpeedTestCoordinatorLogic.h"
#include "common/ServerDisplayName.h"

namespace {

// How often the coordinator checks whether a checkpoint is due. Whether one actually happens is
// decided by the count gate in SpeedTestCoordinatorLogic::shouldSavePeriodically, so a tick that
// finds nothing new to write costs nothing.
constexpr int kPartialResultsSaveIntervalMs = 3000;

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

    // Parented to this coordinator, so it only fires while the coordinator is
    // alive and is torn down with it. Ticking while idle is harmless: the
    // callback is a no-op unless a batch is running with dirty results.
    partialResultsSaveTimer_.setInterval(kPartialResultsSaveIntervalMs);
    connect(&partialResultsSaveTimer_, &QTimer::timeout, this, [this]() {
        if (!deps_.backgroundTasks->isCurrent(speedTestTaskToken_)
            || !speedTestResultsDirty_
            || !SpeedTestCoordinatorLogic::shouldSavePeriodically(resultsSinceLastSave_)) {
            return;
        }
        saveDirtyResults();
    });
}

void SpeedTestCoordinator::startSpeedTest(const QStringList& indexIds)
{
    if (deps_.speedTestController == nullptr || deps_.backgroundTasks == nullptr || !deps_.mutableConfig) {
        if (deps_.appendResult) {
            deps_.appendResult(OperationResult::fail(QCoreApplication::translate(
                "AppBootstrap", "Speed test service is unavailable.")));
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
            server->indexId,
            serverDisplayName(*server),
            server->configType,
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
    resultsSinceLastSave_ = 0;
    partialResultsSaveTimer_.start();
    static const QString pending = QStringLiteral("...");
    QStringList pendingIds;
    for (const auto& item : items) {
        auto it = std::find_if(config.collection().servers.begin(), config.collection().servers.end(),
            [&item](const VmessItem& server) { return server.indexId == item.indexId; });
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
    partialResultsSaveTimer_.stop();
    // Flush whatever the batch already measured before dropping the token:
    // the proxy startup flow cancels the speed test, so this is the last
    // chance to persist partial results before they stop being tracked.
    if (SpeedTestCoordinatorLogic::shouldSaveOnCancel(speedTestResultsDirty_)
        && deps_.backgroundTasks->isCurrent(speedTestTaskToken_)) {
        saveDirtyResults();
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
        partialResultsSaveTimer_.stop();
        deps_.backgroundTasks->resetSpeedTestProgress();
        if (!deps_.backgroundTasks->isCurrent(speedTestTaskToken_)) {
            speedTestResultsDirty_ = false;
            speedTestTaskToken_ = {};
            return;
        }

        if (speedTestResultsDirty_ && deps_.mutableConfig && deps_.saveConfig) {
            const OperationResult saveResult = deps_.saveConfig(deps_.mutableConfig());
            if (!saveResult.success && deps_.appendResult) {
                deps_.appendResult(OperationResult::fail(QStringLiteral("%1 %2").arg(
                    QCoreApplication::translate(
                        "AppBootstrap", "Failed to save configuration after updating test results."),
                    saveResult.message)));
            }
        }
        speedTestResultsDirty_ = false;
        resultsSinceLastSave_ = 0;
        deps_.backgroundTasks->finish(speedTestTaskToken_);
        speedTestTaskToken_ = {};
        return;
    }

    speedTestResultsDirty_ = false;
    resultsSinceLastSave_ = 0;
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
}

void SpeedTestCoordinator::handleTestResultReady(const QString& indexId, const QString& result)
{
    if (deps_.backgroundTasks == nullptr || !deps_.backgroundTasks->isCurrent(speedTestTaskToken_)) {
        return;
    }

    const OperationResult updateResult = deps_.setTestResult
        ? deps_.setTestResult(indexId, result)
        : OperationResult::fail(QCoreApplication::translate(
              "AppBootstrap", "Speed test result service is unavailable."));
    if (!updateResult.success) {
        if (deps_.appendResult) {
            deps_.appendResult(updateResult);
        }
        return;
    }

    speedTestResultsDirty_ = true;
    ++resultsSinceLastSave_;
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

void SpeedTestCoordinator::saveDirtyResults()
{
    if (!speedTestResultsDirty_ || !deps_.mutableConfig || !deps_.saveConfig) {
        return;
    }

    const OperationResult saveResult = deps_.saveConfig(deps_.mutableConfig());
    if (!saveResult.success) {
        // Keep the dirty flag and the counter so the next tick retries; the
        // end-of-batch save also still sees the results as un-persisted.
        if (deps_.appendResult) {
            deps_.appendResult(OperationResult::fail(QStringLiteral("%1 %2").arg(
                QCoreApplication::translate(
                    "AppBootstrap", "Failed to save configuration after updating test results."),
                saveResult.message)));
        }
        return;
    }

    speedTestResultsDirty_ = false;
    resultsSinceLastSave_ = 0;
}

void SpeedTestCoordinator::handleFinished(const QString& summary)
{
    if (deps_.backgroundTasks == nullptr || !deps_.backgroundTasks->isCurrent(speedTestTaskToken_)) {
        return;
    }
    Q_UNUSED(summary);
}
