#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>

#include "app/BackgroundTaskCoordinator.h"
#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"
#include "app/RuntimeState.h"
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"
#include "services/AppUpdateService.h"

struct SettingsDialogApplyPlan;

class ClientConfigWriter;
class ProxySession;
class MainWindow;
class SettingsDialog;
class UwpLoopbackDialog;
class JsonConfigRepository;
class QObject;
class QThread;
class QWidget;
class QtCoreProcessHost;
class RoutingService;
class ConfigBackupService;
class GeoResourceUpdateService;
class AppUpdateInstallService;
class CoreProcessCleanupService;
class CoreDiscoveryService;
class OutboundLocationProbeService;
class TunRuntimeService;
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
    explicit AppBootstrap(
        QString configPath = {},
        bool startHidden = false,
        bool skipCoreChecks = false);
    ~AppBootstrap();

    bool run();

private:
    enum class RuntimePhase {
        Stopped,
        EnvironmentCleanup,
        ValidateCoreApplication,
        ValidateRuntimeResources,
        ValidateCoreConfig,
        StartTunRuntime,
        StartCoreProcess,
        CheckOutboundLocation,
        ApplySystemProxy,
        Proxying,
        Stopping
    };

    void wireMainWindow();
    void syncWindow();
    void syncStatusIndicators();
    void logProxySyncDiagnostic(const RuntimeStateSnapshot& snapshot);
    ProxyUiState computeProxyUiState() const;
    void setRuntimePhase(RuntimePhase phase);
    bool reloadConfig();
    void applyStartupSystemProxyPreference();
    void appendStartupResourceCheckResults();
    void appendResult(const OperationResult& result);
    void showOperationMessage(
        const QString& title,
        const OperationResult& result,
        QWidget* parent = nullptr,
        bool showDialog = true);
    void persistUiState();
    bool isProxyActivationInProgress() const;
    void cancelBackgroundTasksForProxyStartup();
    void startManagedProxyCoreInternal(
        bool skipTunCleanup,
        bool showStartupOverlay = false);
    void stopCore(bool immediate = false);
    void restartCoreIfRunning(const QString& reason, bool showStartupOverlay = false);
    bool saveSystemProxyMode(SystemProxyMode mode);
    void setSystemProxyMode(
        SystemProxyMode mode,
        bool skipTunCleanup = false,
        bool immediateStop = false,
        bool showStartupOverlay = false);
    void clearProxyStateAfterCoreStopped();
    void applySystemProxyModeOnExit(bool windowsShutdown);
    void cleanupRuntimeForExit(bool windowsShutdown);
    void enableSystemProxy(bool showStartupOverlay = false);
    void retryCoreStartup(bool showStartupOverlay = true);
    void disableSystemProxy();
    void setTunEnabled(bool enabled);
    void importFromClipboard();
    void importClipboardTextAsync(const QString& text);
    void importSubscriptionUrlsFromTextAsync(const QString& text);
    void updateAllSubscriptions();
    void updateCurrentSubscription(const QString& subscriptionId);
    void updateCurrentSubscriptionViaProxy(const QString& subscriptionId);
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
        BackgroundTaskCoordinator::Token token,
        CoreType coreType,
        CoreUpdateConfig workerConfig,
        QString installDirectory,
        QPointer<QObject> progressContextGuard,
        bool skipLocalVersionCheck,
        std::function<void(const QString&)> progressObserver,
        std::function<void(const OperationResult&)> completionObserver);
    void finalizeCoreUpdate(
        BackgroundTaskCoordinator::Token token,
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
    void handleRoutingSelectionResult(
        const OperationResult& result,
        bool previousAdvancedEnabled,
        int previousRoutingIndex);
    void handleDefaultServerSelectionResult(const OperationResult& result, const QString& previousIndexId);
    bool requestDefaultServerSwitchAfterCoreStop(const QString& indexId, bool enableTun);
    void scheduleDefaultServerSwitchAfterCoreStopped(
        const QString& indexId,
        bool enableTun,
        bool showStartupOverlay);
    void switchDefaultServerAfterCoreStopped(
        const QString& indexId,
        bool enableTun,
        bool showStartupOverlay);
    void setDefaultServerWithTun(const QString& indexId);
    void autoBackupCurrentConfig();
    void restoreConfigFromBackup();
    void checkAppUpdates(bool manual);
    void downloadAppUpdate(const AppUpdateCheckResult& updateResult, QPointer<QWidget> dialogParent);
    void openSettingsDialog(int initialTab = 0);
    void applySettingsDialogResult(const Config& updatedConfig);
    void openAboutDialog();
    void openUwpLoopbackDialog();
    void openExternalUrl(const QString& url);
    bool askRestartAsAdministratorForTun() const;
    bool promptRestartAsAdministratorForTun();
    void promptRestartForLanguageChange();
    bool promptRestartForDownloadedAppUpdate(const QString& newExecutablePath, QWidget* parent);
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
    void trackBackgroundThread(QThread* thread);
    void waitForBackgroundThreads();
    OperationResult removeStaleTunAdapterIfPresent() const;
    void removeStaleSingBoxCache();
    void cleanupOrphanCoreProcesses();
    void cleanupCoreProcessesUsingConfiguredPorts();

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
    std::unique_ptr<QtCoreProcessHost> coreProcessHost_;
    std::unique_ptr<AppUpdateInstallService> appUpdateInstallService_;
    std::unique_ptr<CoreProcessCleanupService> coreProcessCleanupService_;
    std::unique_ptr<CoreDiscoveryService> coreDiscoveryService_;
    std::unique_ptr<OutboundLocationProbeService> outboundLocationProbeService_;
    std::unique_ptr<TunRuntimeService> tunRuntimeService_;
    std::unique_ptr<RuntimeState> runtimeState_;
    std::unique_ptr<QtCoreProcessHost> auxiliaryCoreProcessHost_;
    std::unique_ptr<WindowsAutoRunService> autoRunService_;
    std::unique_ptr<WindowsSystemProxyService> systemProxyService_;
    std::unique_ptr<BackgroundTaskCoordinator> backgroundTasks_;
    std::unique_ptr<ProxySession> proxySession_;
    std::unique_ptr<TrayController> trayController_;
    std::unique_ptr<MainWindow> mainWindow_;
    QList<CoreType> existingCoreTypes_;
    SystemProxyMode systemProxyMode_ = SystemProxyMode::ForcedClear;
    bool uiReady_ = false;
    bool uiStateRestored_ = false;
    bool shutdownUiStatePersisted_ = false;
    bool exitProxyStateApplied_ = false;
    bool windowsShutdownRequested_ = false;
    bool speedTestResultsDirty_ = false;
    RuntimePhase runtimePhase_ = RuntimePhase::Stopped;
    QString lastProxySyncDiagnostic_;
    std::optional<ProxyUiState> lastProxyUiStateLogged_;
    bool setCurrentActivationPending_ = false;
    bool appUpdateCheckRunning_ = false;
    CoreType pendingCoreUpdateType_ = CoreType::Unknown;
    bool pendingCoreUpdateStartAfterSuccess_ = false;
    bool pendingCoreUpdateSkipLocalVersionCheck_ = false;
    QPointer<QObject> pendingCoreUpdateProgressContext_;
    QPointer<QWidget> pendingCoreUpdateDialogParent_;
    std::function<void(const QString&)> pendingCoreUpdateProgressObserver_;
    std::function<void(const OperationResult&)> pendingCoreUpdateCompletionObserver_;
    BackgroundTaskCoordinator::Token speedTestTaskToken_;
    BackgroundTaskCoordinator::Token pendingCoreUpdateTaskToken_;
    QList<QPointer<QThread>> backgroundThreads_;
    QPointer<UwpLoopbackDialog> uwpLoopbackDialog_;
    std::shared_ptr<char> lifetimeGuard_ = std::make_shared<char>();
    std::atomic_bool shuttingDown_{false};
};
