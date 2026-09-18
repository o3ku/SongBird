#pragma once

#include <memory>

class AppRuntimeResolver;
class AppUpdateCheckCoordinator;
class AppUpdateInstallService;
class ApplicationRestartCoordinator;
class BackgroundTaskCoordinator;
class BackgroundThreadTracker;
class ClientConfigWriter;
class ConfigBackupCoordinator;
class ConfigBackupService;
class CoreDiscoveryService;
class CoreProcessCleanupService;
class CoreUpdateCoordinator;
class DefaultServerSwitchCoordinator;
class GeoResourceUpdateCoordinator;
class GeoResourceUpdateService;
class IProxyActivationCoordinator;
class IRuntimeEnvironment;
class IUserFeedback;
class JsonConfigRepository;
class MainWindow;
class OutboundLocationProbeService;
class ProxySession;
class QtCoreProcessHost;
class RoutingService;
class RuntimeState;
class RuntimeStateSnapshotBuilder;
class ServerCollectionCoordinator;
class ServerEditorCoordinator;
class ServerService;
class SettingsApplyCoordinator;
class SettingsWorkflowCoordinator;
class SpeedTestController;
class SpeedTestCoordinator;
class SubscriptionService;
class SubscriptionWorkflowCoordinator;
class SystemProxyCoordinator;
class TrayController;
class TunModeCoordinator;
class TunRuntimeService;
class WindowsAutoRunService;
class WindowsSystemProxyService;

struct AppBootstrapObjects {
    ~AppBootstrapObjects();

    // Members are destroyed in REVERSE declaration order, so declaration order
    // is load bearing:
    //   * `repository` stays first because the services below hold references
    //     to it and must be destroyed before it.
    //   * `mainWindow` must be declared before everything it is used as a
    //     QObject parent for (see the coordinators below). Those objects are
    //     owned twice — by a unique_ptr here and by mainWindow's child list —
    //     so if mainWindow were destroyed first, ~QObject would delete them and
    //     the unique_ptrs would then free the same pointers again.
    // Adding new members at the end is safe; inserting one above `mainWindow`
    // requires re-checking this order.

    std::unique_ptr<JsonConfigRepository> repository;
    std::unique_ptr<ServerService> serverService;
    std::unique_ptr<ConfigBackupService> configBackupService;
    std::unique_ptr<RoutingService> routingService;
    std::unique_ptr<SpeedTestController> speedTestController;
    std::unique_ptr<SubscriptionService> subscriptionService;
    std::unique_ptr<GeoResourceUpdateService> geoResourceUpdateService;
    std::unique_ptr<ClientConfigWriter> clientConfigWriter;
    std::unique_ptr<QtCoreProcessHost> coreProcessHost;
    std::unique_ptr<AppUpdateInstallService> appUpdateInstallService;
    std::unique_ptr<CoreProcessCleanupService> coreProcessCleanupService;
    std::unique_ptr<CoreDiscoveryService> coreDiscoveryService;
    std::unique_ptr<OutboundLocationProbeService> outboundLocationProbeService;
    std::unique_ptr<TunRuntimeService> tunRuntimeService;
    std::unique_ptr<RuntimeState> runtimeState;
    std::unique_ptr<QtCoreProcessHost> auxiliaryCoreProcessHost;
    std::unique_ptr<WindowsAutoRunService> autoRunService;
    std::unique_ptr<WindowsSystemProxyService> systemProxyService;
    std::unique_ptr<BackgroundTaskCoordinator> backgroundTasks;
    std::unique_ptr<BackgroundThreadTracker> backgroundThreadTracker;
    std::unique_ptr<ConfigBackupCoordinator> configBackupCoordinator;
    std::unique_ptr<AppRuntimeResolver> appRuntimeResolver;
    std::unique_ptr<RuntimeStateSnapshotBuilder> runtimeStateSnapshotBuilder;
    std::unique_ptr<IRuntimeEnvironment> runtimeEnvironment;
    std::unique_ptr<IProxyActivationCoordinator> proxyActivationCoordinator;
    std::unique_ptr<ProxySession> proxySession;
    std::unique_ptr<IUserFeedback> userFeedback;
    std::unique_ptr<ApplicationRestartCoordinator> applicationRestartCoordinator;
    // Declared here, before the coordinators that are parented to it, so it is
    // destroyed after them. See the ordering note at the top of this struct.
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<AppUpdateCheckCoordinator> appUpdateCheckCoordinator;
    std::unique_ptr<CoreUpdateCoordinator> coreUpdateCoordinator;
    std::unique_ptr<DefaultServerSwitchCoordinator> defaultServerSwitchCoordinator;
    std::unique_ptr<GeoResourceUpdateCoordinator> geoResourceUpdateCoordinator;
    std::unique_ptr<ServerCollectionCoordinator> serverCollectionCoordinator;
    std::unique_ptr<ServerEditorCoordinator> serverEditorCoordinator;
    std::unique_ptr<SettingsApplyCoordinator> settingsApplyCoordinator;
    std::unique_ptr<SettingsWorkflowCoordinator> settingsWorkflowCoordinator;
    std::unique_ptr<SpeedTestCoordinator> speedTestCoordinator;
    std::unique_ptr<SystemProxyCoordinator> systemProxyCoordinator;
    std::unique_ptr<SubscriptionWorkflowCoordinator> subscriptionWorkflowCoordinator;
    std::unique_ptr<TunModeCoordinator> tunModeCoordinator;
    std::unique_ptr<TrayController> trayController;
};
