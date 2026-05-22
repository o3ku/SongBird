#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include "app/CoreStartupCheckpoint.h"
#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"

struct SettingsDialogApplyPlan;

class CoreLifecycleService;
class ClientConfigWriter;
class ServerConfigWriter;
class MainWindow;
class SettingsDialog;
class JsonConfigRepository;
class QNetworkAccessManager;
class QObject;
class QTcpServer;
class QThread;
class QWidget;
class SingleInstanceGuard;
class QtCoreProcessHost;
class QTimer;
class RoutingService;
class ConfigBackupService;
class GeoResourceUpdateService;
struct CoreUpdateConfig;
class ServerService;
class SpeedTestService;
class SubscriptionService;
class SubscriptionUpdateService;
class TrayController;
class WindowsAutoRunService;
class WindowsSystemProxyService;

class AppBootstrap {
public:
    enum class BackgroundTaskKind {
        None,
        SpeedTest,
        SubscriptionUpdate,
        ProxyAvailabilityCheck,
        CoreUpdate,
        GeoUpdate
    };

    explicit AppBootstrap(
        QString configPath = {},
        bool startHidden = false,
        bool skipCoreChecks = false);
    ~AppBootstrap();

    bool run();

private:
    void wireMainWindow();
    void syncWindow();
    void syncStatusIndicators();
    bool reloadConfig();
    bool shouldStartCoreOnStartup() const;
    void startCoreOnStartup();
    void appendStartupResourceCheckResults();
    void appendResult(const OperationResult& result);
    void resetCoreStartupChecklist(const QStringList& steps, bool showOverlay);
    void prepareCoreStartupChecklist(bool showOverlay);
    void setCoreStartupCheckpointStatus(CoreStartupCheckpointStatus status, const QString& step, const QString& detail = {});
    void syncCoreStartupChecklistOverlay();
    void clearCoreStartupChecklist();
    void keepCoreStartupChecklistForUserDismissal(const QString& step, const QString& detail);
    void failCoreStartup(const OperationResult& result);
    void cleanupAfterFailedCoreStartup();
    void downloadMissingGeoFilesAndResumeStartup(
        const CoreInfo& coreInfo,
        const QString& geoValidationMessage,
        const QString& checkGeoStep,
        bool skipTunCleanup,
        std::optional<bool> startupProxyEnabled,
        bool showStartupOverlay);
    void downloadMissingSingBoxRuleSetsAndResumeStartup(
        const QStringList& missingRuleSetTags,
        const QString& checkGeoStep,
        bool skipTunCleanup,
        std::optional<bool> startupProxyEnabled,
        bool showStartupOverlay);
    void downloadMissingCoreAndResumeStartup(
        CoreType coreType,
        const OperationResult& missingCoreResult,
        const QString& downloadCoreStep,
        bool skipTunCleanup,
        std::optional<bool> startupProxyEnabled,
        bool showStartupOverlay);
    void showOperationMessage(
        const QString& title,
        const OperationResult& result,
        QWidget* parent = nullptr,
        bool showDialog = true);
    void persistUiState();
    void startCore(bool showStartupOverlay = false);
    void startCore(bool skipTunCleanup, bool showStartupOverlay);
    void startCoreInternal(
        bool skipTunCleanup,
        std::optional<bool> startupProxyEnabled,
        bool environmentAlreadyChecked = false,
        bool showStartupOverlay = false);
    void stopCore(bool immediate = false);
    void stopCoreInternal(bool immediate, bool clearRestartAfterStop);
    void handleCoreStarted(
        const QString& serverIndexId,
        CoreType runtimeCore,
        bool customServer,
        std::optional<bool> startupProxyEnabled);
    void validateCoreListeningAndHandleStarted(
        const QString& serverIndexId,
        CoreType runtimeCore,
        bool customServer,
        std::optional<bool> startupProxyEnabled);
    void handleCoreStartFailed(const QString& message);
    void disconnectPendingCoreStartConnection();
    void restartCoreIfRunning(const QString& reason, bool showStartupOverlay = false);
    void scheduleCoreStartAfterCoreStopped(bool showStartupOverlay);
    void clearCurrentServerLocation();
    void queryCurrentServerLocation(
        const QString& serverIndexId,
        CoreType runtimeCore,
        std::optional<bool> startupProxyEnabled);
    bool applySystemProxyAfterOutboundLocation(std::optional<bool> startupProxyEnabled);
    void clearCoreStartupChecklistAfterStableRun(const QString& serverIndexId, qint64 coreStartTime);
    bool saveSystemProxyMode(SystemProxyMode mode);
    void setSystemProxyMode(
        SystemProxyMode mode,
        bool skipTunCleanup = false,
        bool immediateStop = false,
        bool showStartupOverlay = false);
    void clearAppliedSystemProxyAfterCoreStopped();
    void clearProxyStateAfterCoreStopped();
    void applySystemProxyModeOnExit(bool windowsShutdown);
    void cleanupRuntimeForExit(bool windowsShutdown);
    void enableSystemProxy(bool showStartupOverlay = false);
    void disableSystemProxy();
    void setTunEnabled(bool enabled);
    void toggleAutoRun();
    void importFromClipboard();
    void importClipboardTextAsync(const QString& text);
    void importSubscriptionUrlsFromTextAsync(const QString& text);
    void exportClientConfig(const QString& indexId);
    void exportServerConfig(const QString& indexId);
    void importClientConfigFromFile();
    void importServerConfigFromFile();
    OperationResult importCustomConfigText(const QString& text);
    void updateAllSubscriptions();
    void updateCurrentSubscription(const QString& subscriptionId);
    void updateCurrentSubscriptionViaProxy(const QString& subscriptionId);
    void runProxyAvailabilityCheck();
    void updateCore(int coreTypeValue);
    void updateCore(int coreTypeValue, bool startAfterSuccess);
    void updateCore(
        int coreTypeValue,
        bool startAfterSuccess,
        QObject* progressContext,
        QWidget* dialogParent,
        bool skipConfirmation,
        bool skipLocalVersionCheck,
        const std::function<void(const QString&)>& progressObserver,
        const std::function<void(const OperationResult&)>& completionObserver);
    void runCoreUpdateTask(
        CoreType coreType,
        CoreUpdateConfig workerConfig,
        QString installDirectory,
        QPointer<QObject> progressContextGuard,
        bool skipLocalVersionCheck,
        std::function<void(const QString&)> progressObserver,
        std::function<void(const OperationResult&)> completionObserver);
    void finalizeCoreUpdate(
        const QString& title,
        const OperationResult& result,
        bool stoppedForInstall,
        bool startAfterSuccess,
        QPointer<QWidget> dialogParentGuard,
        const std::function<void(const OperationResult&)>& completionObserver);
    void continuePendingCoreUpdate();
    void updateGeoResources();
    void updateSelectedSubscriptions(const QList<int>& rowIndexes);
    void updateSubscriptionsByIds(const QStringList& subscriptionIds, bool useProxy, const QString& startupMessage);
    void hideSubscription(const QString& subscriptionId);
    void deleteSubscription(const QString& subscriptionId);
    void openCustomConfigFile(const QString& indexId);
    void handleRoutingSelectionResult(
        const OperationResult& result,
        bool previousAdvancedEnabled,
        int previousRoutingIndex);
    void handleDefaultServerSelectionResult(const OperationResult& result, const QString& previousIndexId);
    bool requestDefaultServerSwitchAfterCoreStop(const QString& indexId, bool enableTun);
    void scheduleDefaultServerSwitchAfterCoreStopped();
    void switchDefaultServerAfterCoreStopped();
    void setDefaultServerWithTun(const QString& indexId);
    void autoBackupCurrentConfig();
    void restoreConfigFromBackup();
    void openSettingsDialog(int initialTab = 0);
    void applySettingsDialogResult(const Config& updatedConfig);
    void openAboutDialog();
    void openExternalUrl(const QString& url);
    void openLoopbackTool();
    bool askRestartAsAdministratorForTun() const;
    bool promptRestartAsAdministratorForTun();
    void promptRestartForLanguageChange();
    bool restartApplication(bool requireAdministrator);
    void startSpeedTest(const QStringList& indexIds);
    bool isCoreRunning() const;
    bool isCoreReady() const;
    const VmessItem* findServerById(const QString& indexId) const;
    const VmessItem* resolveActiveServer() const;
    std::optional<VmessItem> findServerSnapshotById(const QString& indexId) const;
    std::optional<VmessItem> resolveActiveServerSnapshot() const;
    QString resolveConfigPath() const;
    QString resolveCustomConfigDirectory() const;
    QString resolveCustomConfigPath(const QString& address) const;
    QString resolveRuntimeConfigPath(const VmessItem& server) const;
    CoreType resolveLaunchCoreType(const VmessItem& server) const;
    CoreInfo resolveCoreInfo(const VmessItem& server) const;
    CoreType resolveEffectiveCoreType(const VmessItem& server) const;
    QStringList resolveCoreCandidates(CoreType coreType) const;
    QString locateFirstExistingFile(const QStringList& candidates) const;
    void refreshExistingCoreTypes();
    QList<CoreType> detectExistingCoreTypes() const;
    QString detectCoreVersion(CoreType coreType) const;
    QString resolveCoreInstallDirectory(CoreType coreType) const;
    QString buildRoutingSummaryText() const;
    QString buildListenSummaryText() const;
    QString buildSystemProxyExceptions() const;
    bool updateSystemProxyMode(SystemProxyMode mode) const;
    QString backgroundTaskDescription(BackgroundTaskKind kind) const;
    void resetSpeedTestProgressState();
    bool tryBeginBackgroundTask(BackgroundTaskKind kind);
    bool isCoreStartupBlockingBackgroundTask() const;
    void finishBackgroundTask(BackgroundTaskKind kind);
    void syncBackgroundTaskState();
    void trackBackgroundThread(QThread* thread);
    void waitForBackgroundThreads();
    void stopAuxiliaryCore(bool immediate = false);
    OperationResult removeStaleTunAdapterIfPresent() const;
    void removeStaleTunAdapterAsync(const std::function<void(const OperationResult&)>& completion);
    void removeStaleTunAdapter();
    void removeStaleSingBoxCache();
    void cleanupOrphanCoreProcesses();
    void cleanupCoreProcessesUsingConfiguredPorts();
    void handleCoreExited(int exitCode, int status, bool stopRequested, bool auxiliary);
    void scheduleCoreRestart(const QString& reason, bool auxiliary, int delayMs = 3000);
    void cancelPendingCoreRestarts();

    QString configPath_;
    bool startHidden_ = false;
    bool skipCoreChecks_ = false;
    Config config_;
    std::unique_ptr<JsonConfigRepository> repository_;
    std::unique_ptr<ServerService> serverService_;
    std::unique_ptr<ConfigBackupService> configBackupService_;
    std::unique_ptr<RoutingService> routingService_;
    std::unique_ptr<SpeedTestService> speedTestService_;
    std::unique_ptr<SubscriptionService> subscriptionService_;
    std::unique_ptr<GeoResourceUpdateService> geoResourceUpdateService_;
    std::unique_ptr<ClientConfigWriter> clientConfigWriter_;
    std::unique_ptr<ServerConfigWriter> serverConfigWriter_;
    std::unique_ptr<QtCoreProcessHost> coreProcessHost_;
    std::unique_ptr<CoreLifecycleService> coreLifecycleService_;
    std::unique_ptr<QtCoreProcessHost> auxiliaryCoreProcessHost_;
    std::unique_ptr<CoreLifecycleService> auxiliaryCoreLifecycleService_;
    std::unique_ptr<WindowsAutoRunService> autoRunService_;
    std::unique_ptr<WindowsSystemProxyService> systemProxyService_;
    std::unique_ptr<TrayController> trayController_;
    std::unique_ptr<MainWindow> mainWindow_;
    QList<CoreType> existingCoreTypes_;
    SystemProxyMode systemProxyMode_ = SystemProxyMode::ForcedClear;
    bool uiReady_ = false;
    bool uiStateRestored_ = false;
    bool shutdownUiStatePersisted_ = false;
    bool exitProxyStateApplied_ = false;
    bool restartHandoffPending_ = false;
    bool windowsShutdownRequested_ = false;
    bool subscriptionUpdateRunning_ = false;
    bool proxyAvailabilityCheckRunning_ = false;
    bool coreUpdateRunning_ = false;
    bool geoUpdateRunning_ = false;
    bool speedTestResultsDirty_ = false;
    bool coreStartPending_ = false;
    bool coreStopPending_ = false;
    bool coreReady_ = false;
    QString currentServerLocation_;
    QStringList coreStartupSteps_;
    QHash<QString, CoreStartupCheckpointStatus> coreStartupStepStatus_;
    QHash<QString, QString> coreStartupStepDetails_;
    bool coreStartupOverlayRequested_ = false;
    bool coreStartupOverlayShown_ = false;
    qint64 coreStartupOverlayShownAtMs_ = 0;
    int coreStartupOverlayClearGeneration_ = 0;
    int coreStartupStableRunGeneration_ = 0;
    bool cancelCoreStartAfterGeoUpdate_ = false;
    bool coreTunEnabledAtStart_ = false;
    bool managedSystemProxyActive_ = false;
    bool tunCleanupActive_ = false;
    bool skipTunCleanupOnNextCoreExit_ = false;
    bool resumeCoreStartAfterTunCleanup_ = false;
    bool restartAfterStopPending_ = false;
    bool restartAfterStopShowStartupOverlay_ = false;
    QString pendingDefaultServerAfterStopId_;
    bool pendingDefaultServerAfterStopEnableTun_ = false;
    bool pendingDefaultServerAfterStopShowStartupOverlay_ = false;
    bool keepCoreStartupChecklistOnNextStop_ = false;
    bool setCurrentActivationPending_ = false;
    bool coreUpdatePendingAfterStop_ = false;
    BackgroundTaskKind activeBackgroundTask_ = BackgroundTaskKind::None;
    int speedTestTotalCount_ = 0;
    int speedTestCompletedCount_ = 0;
    QString speedTestProgressServerName_;
    CoreType pendingCoreUpdateType_ = CoreType::Unknown;
    bool pendingCoreUpdateStartAfterSuccess_ = false;
    bool pendingCoreUpdateSkipLocalVersionCheck_ = false;
    QPointer<QObject> pendingCoreUpdateProgressContext_;
    QPointer<QWidget> pendingCoreUpdateDialogParent_;
    std::function<void(const QString&)> pendingCoreUpdateProgressObserver_;
    std::function<void(const OperationResult&)> pendingCoreUpdateCompletionObserver_;
    QMetaObject::Connection coreStartedConnection_;
    QList<QPointer<QThread>> backgroundThreads_;
    std::shared_ptr<char> lifetimeGuard_ = std::make_shared<char>();
    std::atomic_bool shuttingDown_{false};
    QTimer* coreRestartTimer_ = nullptr;
    QTimer* auxiliaryRestartTimer_ = nullptr;
    int coreCrashRestartCount_ = 0;
    int auxiliaryCrashRestartCount_ = 0;
    qint64 coreStartTime_ = 0;
};
