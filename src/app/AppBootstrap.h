#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>

#include "appcore/BackgroundTaskCoordinator.h"
#include "common/LogSeverity.h"
#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"
#include "domain/models/RuntimeState.h"
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"
#include "services/ExportService.h"

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
struct SpeedTestRequestItem;
class ServerService;
class SpeedTestController;
class SubscriptionService;
class SubscriptionUpdateService;
class TrayController;
class IAutoRunService;
class ISystemProxyService;
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
    // Composition phases of run(), split by domain. Each one only constructs
    // objects_ entries and injects callbacks; none of them touch the UI or
    // start work. See the corresponding *Wiring.cpp files.
    void wireCoreServices();
    void wireProxyStack();
    void wireUiObjects();
    void wireUpdateCoordinators(const std::function<void(QThread*)>& trackBackgroundThread);
    void wireServerCoordinators();
    void wireShutdownHooks();

    void wireMainWindow();
    void wireProxySessionSignals();
    void wireRuntimeStateSignals();
    void wireBackgroundTaskSignals();
    void wireMainWindowCommands();
    void wireTraySignals();
    // What a user's server click means right now (single node, group switch, or nothing) --
    // shared by the main window's and the tray's server-selection entries so the two stay in
    // step. Deliberately NOT inside DefaultServerSwitchCoordinator: the auto-switch failure
    // fallback switches to a specific node on purpose, even inside the group it came from.
    void routeServerActivationGesture(const QString& indexId);
    // The leave-group-mode offer after a group activation failed because its target cannot
    // run (ProxySession::groupTargetUnavailable): spend the target, then ask whether to fall
    // back to subscription grouping with the current single node.
    void offerSubscriptionModeAfterGroupTargetFailure(const QString& locationId, const QString& reason);
    // Group activation display state, owned by the poller connection: the member the core's
    // auto-select group is currently on ("" while the core has not answered or a single-node
    // session runs).
    QString currentInCoreServerId_;
    QString currentInCoreServerName_;
    void wireWorkflowCoordinators(const std::function<void(QThread*)>& trackBackgroundThread);

    // Startup steps, run in this order by run() through StartupSequencer. They
    // are the bodies that used to be inlined in run(); keeping them named is
    // what lets run() stay a pure assembly step.
    void initializeTray();
    void logStartupBanner();
    void adoptManagedSystemProxyOnStartup();
    void presentMainWindowOnStartup();
    void scheduleAppUpdateChecks();
    // The scheduled subscription refresh's tick: every SubscriptionAutoUpdateLogic::kCheckIntervalMs
    // (plus one early check after startup), refresh the subscriptions the logic names as due.
    // The refresh itself is the manual update path (updateByIds), which already no-ops while
    // another background task holds the slot -- the tick's own guards only keep it from racing
    // a proxy activation.
    void scheduleSubscriptionAutoUpdate();
    void offerCoreDownloadOnStartup();

    void syncWindow();
    void syncStatusIndicators();
    bool reloadConfig();
    void applyStartupSystemProxyPreference();
    void appendStartupResourceCheckResults();
    // Severity is derived from the result: success is Info, a cancellation is a
    // Warning, and anything else is an Error. The two-argument overload exists
    // for the handful of lines whose wording and outcome disagree -- "the node
    // was set, but verify it yourself" is a failure of activation yet not an
    // error the user can act on.
    void appendResult(const OperationResult& result);
    void appendResult(const OperationResult& result, LogSeverity severity);
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
    void cleanupRuntimeForExit(bool windowsShutdown);
    void enableSystemProxy(bool showStartupOverlay = false);
    void retryCoreStartup(bool showStartupOverlay = true);
    // The "auto switch in group" preference (config.ui().autoSwitchInGroup): after a session
    // failure that the crash-restart path could not recover, hands the decision to
    // AutoSwitchCoordinator -- warm-probe target first, one probe round second, round-robin
    // neighbour last, with a per-group exhaustion ceiling. No-op when the preference is off,
    // the location view holds no selection, or the group has no other node.
    void autoSwitchWithinGroupOnFailure();
    // One AutoSwitchCoordinator probe batch: BackgroundTaskCoordinator token (startup scope)
    // plus the SpeedTestController run it gates. A member of AppBootstrap rather than a lambda
    // so the token/cancel rules live in one named, reviewable place.
    bool runAutoSwitchProbe(
        QObject* completionContext,
        const QList<SpeedTestRequestItem>& items,
        std::function<void()> onFinished);
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
    void openShortcutsCheatSheet();
    // Applies config_.ui().themeName to the application, resolving a "System"
    // preference through the platform service first.
    void applyConfiguredTheme();
    // Subscribes to the OS light/dark broadcast so a "System" theme follows a
    // change made while the app is running. No-op where there is no broadcast.
    void setupSystemThemeWatcher();
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

    QString systemProxyUndoWarning_;
    QString configPath_;
    bool startHidden_ = false;
    bool skipCoreChecks_ = false;
    Config config_;
    // Stateless and cheap to construct, so it lives here rather than in AppBootstrapObjects: it owns
    // no QObject, borrows nothing, and therefore has no destruction-order constraint to respect.
    ExportService exportService_;
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
