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

class ClientConfigWriter;
class ProxySession;
class AppUpdateCheckCoordinator;
class AppRuntimeResolver;
class RuntimeStateSnapshotBuilder;
class ApplicationRestartCoordinator;
class ConfigBackupCoordinator;
class CoreUpdateCoordinator;
class BackgroundThreadTracker;
class DefaultServerSwitchCoordinator;
class GeoResourceUpdateCoordinator;
class IUserFeedback;
class IProxyActivationCoordinator;
class IRuntimeEnvironment;
class IRuntimeProfileResolver;
class MainWindow;
class ServerCollectionCoordinator;
class ServerEditorCoordinator;
class SettingsApplyCoordinator;
class SettingsDialog;
class SettingsWorkflowCoordinator;
class SpeedTestCoordinator;
class SystemProxyCoordinator;
class SubscriptionWorkflowCoordinator;
class UwpLoopbackDialog;
class JsonConfigRepository;
class TunModeCoordinator;
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
class SpeedTestController;
class SubscriptionService;
class SubscriptionUpdateService;
class TrayController;
class WindowsAutoRunService;
class WindowsSystemProxyService;
struct AppBootstrapObjects;

class AppBootstrap {
public:
    explicit AppBootstrap(
        QString configPath = {},
        bool startHidden = false,
        bool skipCoreChecks = false);
    ~AppBootstrap();

    bool run();

private:
    void wireMainWindow();
    void wireProxySessionSignals();
    void wireRuntimeStateSignals();
    void wireBackgroundTaskSignals();
    void wireMainWindowCommands();
    void wireTraySignals();
    void wireWorkflowCoordinators(const std::function<void(QThread*)>& trackBackgroundThread);
    void syncWindow();
    void syncStatusIndicators();
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
    void setAutoRunEnabled(bool enabled);
    void clearProxyStateAfterCoreStopped();
    void applySystemProxyModeOnExit(bool windowsShutdown);
    void cleanupRuntimeForExit(bool windowsShutdown);
    void enableSystemProxy(bool showStartupOverlay = false);
    void retryCoreStartup(bool showStartupOverlay = true);
    void disableSystemProxy();
    void setTunEnabled(bool enabled);
    void importFromClipboard();
    void updateCurrentSubscription(const QString& subscriptionId);
    void updateCurrentSubscriptionViaProxy(const QString& subscriptionId);
    void handleRoutingSelectionResult(
        const OperationResult& result,
        const QString& previousRoutingModeId);
    void autoBackupCurrentConfig();
    void restoreConfigFromBackup();
    void checkAppUpdates(bool manual);
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
    bool updateSystemProxyMode(SystemProxyMode mode) const;
    OperationResult removeStaleTunAdapterIfPresent() const;
    void cleanupOrphanCoreProcesses();
    void cleanupCoreProcessesUsingConfiguredPorts();

    QString configPath_;
    bool startHidden_ = false;
    bool skipCoreChecks_ = false;
    Config config_;
    std::unique_ptr<AppBootstrapObjects> objects_;
    QList<CoreType> existingCoreTypes_;
    SystemProxyMode systemProxyMode_ = SystemProxyMode::ForcedClear;
    bool uiReady_ = false;
    bool uiStateRestored_ = false;
    bool shutdownUiStatePersisted_ = false;
    bool windowsShutdownRequested_ = false;
    bool setCurrentActivationPending_ = false;
    QPointer<UwpLoopbackDialog> uwpLoopbackDialog_;
    std::shared_ptr<char> lifetimeGuard_ = std::make_shared<char>();
    std::atomic_bool shuttingDown_{false};
};
