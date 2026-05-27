#include <QtTest/QtTest>

#include "app/BackgroundTaskCoordinator.h"
#include "app/CoreUpdateCoordinator.h"

class CoreUpdateCoordinatorTests : public QObject {
    Q_OBJECT

private slots:
    void canceledConfirmationFinishesTokenAndNotifiesCompletion();
    void runningCoreUpdateIsStoredPendingAndContinuesAfterStop();
    void expiredLifetimeGuardDropsWorkerCallbacks();
};

namespace {

struct Harness {
    QObject uiContext;
    BackgroundTaskCoordinator backgroundTasks;
    std::shared_ptr<char> lifetimeGuard = std::make_shared<char>();
    bool confirm = true;
    bool shouldStopRunningCore = false;
    OperationResult updateResult = OperationResult::ok(QStringLiteral("updated"), true);
    int stopCount = 0;
    int runnerCount = 0;
    int completionCount = 0;
    int refreshCount = 0;
    int clearStoppedCount = 0;
    int syncCount = 0;
    QList<bool> enableSystemProxyCalls;
    QStringList logs;
    QList<OperationResult> appendedResults;
    QList<OperationResult> completions;
    QString installDirectory;
    CoreType runnerCoreType = CoreType::Unknown;
    bool runnerSkipLocalVersionCheck = false;
    bool runnerCheckPreRelease = false;
    bool runnerIgnoreGeoCore = false;
    QStringList progressMessages = {QStringLiteral("download")};

    CoreUpdateCoordinator::Dependencies dependencies()
    {
        CoreUpdateCoordinator::Dependencies deps;
        deps.backgroundTasks = &backgroundTasks;
        deps.lifetimeGuard = lifetimeGuard;
        deps.isShuttingDown = []() { return false; };
        deps.defaultProgressContext = [this]() -> QObject* { return &uiContext; };
        deps.defaultDialogParent = []() -> QWidget* { return nullptr; };
        deps.uiContext = [this]() -> QObject* { return &uiContext; };
        deps.resolveRuntimeCoreType = [](CoreType coreType) { return ::resolveRuntimeCoreType(coreType); };
        deps.resolveCoreInstallDirectory = [this](CoreType coreType) {
            installDirectory = QStringLiteral("install-%1").arg(coreTypeDisplayName(coreType));
            return installDirectory;
        };
        deps.makeWorkerConfig = []() {
            CoreUpdateConfig config;
            config.checkPreReleaseUpdate = true;
            config.ignoreGeoUpdateCore = true;
            return config;
        };
        deps.shouldStopRunningCoreForUpdate = [this](CoreType) { return shouldStopRunningCore; };
        deps.confirmUpdate = [this](QWidget*, const QString&, const QString&) { return confirm; };
        deps.appendResult = [this](const OperationResult& result) { appendedResults.append(result); };
        deps.appendLog = [this](const QString& message) { logs.append(message); };
        deps.showOperationMessage = [](const QString&, const OperationResult&, QWidget*) {};
        deps.stopForCoreUpdate = [this]() { ++stopCount; };
        deps.enableSystemProxy = [this](bool showOverlay) { enableSystemProxyCalls.append(showOverlay); };
        deps.clearProxyStateAfterCoreStopped = [this]() { ++clearStoppedCount; };
        deps.syncStatusIndicators = [this]() { ++syncCount; };
        deps.refreshExistingCoreTypes = [this]() { ++refreshCount; };
        deps.updateRunner = [this](
                                CoreType coreType,
                                const CoreUpdateConfig& config,
                                const QString&,
                                const CoreUpdateService::UpdateOptions& options) {
            ++runnerCount;
            runnerCoreType = coreType;
            runnerSkipLocalVersionCheck = options.skipLocalVersionCheck;
            runnerCheckPreRelease = config.checkPreReleaseUpdate;
            runnerIgnoreGeoCore = config.ignoreGeoUpdateCore;
            for (const QString& message : progressMessages) {
                if (options.progressHandler) {
                    options.progressHandler(message);
                }
            }
            return updateResult;
        };
        deps.startBackgroundTask = [](std::function<void()> task) { task(); };
        return deps;
    }

    CoreUpdateCoordinator::Request request()
    {
        CoreUpdateCoordinator::Request request;
        request.coreTypeValue = static_cast<int>(CoreType::Xray);
        request.completionObserver = [this](const OperationResult& result) {
            ++completionCount;
            completions.append(result);
        };
        return request;
    }
};

} // namespace

void CoreUpdateCoordinatorTests::canceledConfirmationFinishesTokenAndNotifiesCompletion()
{
    Harness harness;
    harness.confirm = false;
    CoreUpdateCoordinator coordinator(harness.dependencies());

    coordinator.updateCore(harness.request());

    QCOMPARE(harness.backgroundTasks.isActive(), false);
    QCOMPARE(harness.runnerCount, 0);
    QCOMPARE(harness.stopCount, 0);
    QCOMPARE(harness.completionCount, 1);
    QCOMPARE(harness.appendedResults.size(), 1);
    QCOMPARE(harness.appendedResults.first().success, true);
    QVERIFY(harness.appendedResults.first().message.contains(QStringLiteral("canceled")));
    QCOMPARE(coordinator.hasPendingCoreUpdate(), false);
}

void CoreUpdateCoordinatorTests::runningCoreUpdateIsStoredPendingAndContinuesAfterStop()
{
    Harness harness;
    harness.shouldStopRunningCore = true;
    CoreUpdateCoordinator coordinator(harness.dependencies());
    CoreUpdateCoordinator::Request request = harness.request();
    request.startAfterSuccess = true;
    request.skipLocalVersionCheck = true;

    coordinator.updateCore(request);

    QCOMPARE(harness.backgroundTasks.isKindActive(BackgroundTaskCoordinator::Kind::CoreUpdate), true);
    QCOMPARE(coordinator.hasPendingCoreUpdate(), true);
    QCOMPARE(harness.stopCount, 1);
    QCOMPARE(harness.runnerCount, 0);

    coordinator.continuePendingCoreUpdate();

    QCOMPARE(coordinator.hasPendingCoreUpdate(), false);
    QCOMPARE(harness.backgroundTasks.isActive(), false);
    QCOMPARE(harness.runnerCount, 1);
    QCOMPARE(harness.runnerCoreType, CoreType::Xray);
    QCOMPARE(harness.runnerSkipLocalVersionCheck, true);
    QCOMPARE(harness.runnerCheckPreRelease, true);
    QCOMPARE(harness.runnerIgnoreGeoCore, true);
    QCOMPARE(harness.completionCount, 1);
    QCOMPARE(harness.refreshCount, 1);
    QCOMPARE(harness.enableSystemProxyCalls, QList<bool>({true}));
    QVERIFY(harness.logs.contains(QStringLiteral("Starting Xray update...")));
    QVERIFY(harness.logs.contains(QStringLiteral("download")));
    QVERIFY(harness.logs.contains(QStringLiteral("Restarting the updated core...")));
}

void CoreUpdateCoordinatorTests::expiredLifetimeGuardDropsWorkerCallbacks()
{
    Harness harness;
    CoreUpdateCoordinator coordinator(harness.dependencies());
    harness.lifetimeGuard.reset();

    CoreUpdateCoordinator::Request request = harness.request();
    request.skipConfirmation = true;
    coordinator.updateCore(request);

    QCOMPARE(harness.runnerCount, 0);
    QCOMPARE(harness.completionCount, 0);
    QCOMPARE(harness.backgroundTasks.isKindActive(BackgroundTaskCoordinator::Kind::CoreUpdate), true);
}

QTEST_MAIN(CoreUpdateCoordinatorTests)

#include "CoreUpdateCoordinatorTests.moc"
