#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"

#include <memory>
#include <utility>

#include <QFileInfo>
#include <QWidget>

#include "app/AppUpdateInstallService.h"
#include "app/ApplicationRestartCoordinator.h"
#include "app/AppRuntimeResolver.h"
#include "app/BackgroundTaskCoordinator.h"
#include "app/BackgroundThreadTracker.h"
#include "app/ConfigBackupCoordinator.h"
#include "app/CoreDiscoveryService.h"
#include "app/CoreProcessCleanupService.h"
#include "app/FunctionRuntimeAdapters.h"
#include "app/OutboundLocationProbeService.h"
#include "app/ProxyRuntimeInterfaces.h"
#include "app/ProxySession.h"
#include "app/RuntimeStateSnapshotBuilder.h"
#include "app/StartupAdminElevation.h"
#include "app/TunRuntimeService.h"
#include "common/AppPlatform.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
#include "domain/models/RuntimeState.h"
#include "persistence/JsonConfigRepository.h"
#include "platform/windows/WindowsAutoRunService.h"
#include "platform/windows/WindowsSystemProxyService.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/QtCoreProcessHost.h"
#include "services/ConfigBackupService.h"
#include "services/GeoResourceUpdateService.h"
#include "services/RoutingService.h"
#include "services/ServerService.h"
#include "services/SpeedTestController.h"
#include "services/SubscriptionService.h"
#include "ui/mainwindow/MainWindow.h"

void AppBootstrap::wireCoreServices()
{
    objects_->repository = std::make_unique<JsonConfigRepository>(resolveConfigPath());
    objects_->serverService = std::make_unique<ServerService>(*objects_->repository, resolveCustomConfigDirectory());
    objects_->configBackupService = std::make_unique<ConfigBackupService>(resolveConfigPath());
    objects_->configBackupCoordinator = std::make_unique<ConfigBackupCoordinator>(
        *objects_->configBackupService,
        ConfigBackupCoordinator::Dependencies{
            [this]() -> QWidget* { return objects_->mainWindow.get(); },
            [this]() { return resolveConfigPath(); },
            [this]() { return config_; },
            [this](const OperationResult& result) { appendResult(result); },
            [this]() { return isCoreRunning(); },
            [this]() { objects_->proxySession->stop(true); },
            [this]() { uiStateRestored_ = false; },
            [this]() { return reloadConfig(); },
            [this]() { return resolveActiveServer() != nullptr; },
            [this]() { enableSystemProxy(true); },
            [this]() { clearProxyStateAfterCoreStopped(); },
            [this]() { syncStatusIndicators(); }
        });
    objects_->routingService = std::make_unique<RoutingService>(*objects_->repository);
    objects_->speedTestController = std::make_unique<SpeedTestController>(resolveCustomConfigDirectory());
    objects_->subscriptionService = std::make_unique<SubscriptionService>(*objects_->repository);
    objects_->geoResourceUpdateService = std::make_unique<GeoResourceUpdateService>(
        QFileInfo(resolveConfigPath()).dir().absolutePath());
    objects_->clientConfigWriter = std::make_unique<ClientConfigWriter>(resolveCustomConfigDirectory());
    objects_->coreProcessHost = std::make_unique<QtCoreProcessHost>();
    objects_->backgroundTasks = std::make_unique<BackgroundTaskCoordinator>();
    objects_->backgroundThreadTracker = std::make_unique<BackgroundThreadTracker>();
    objects_->coreProcessCleanupService = std::make_unique<CoreProcessCleanupService>();
    objects_->coreDiscoveryService = std::make_unique<CoreDiscoveryService>();
    objects_->appRuntimeResolver = std::make_unique<AppRuntimeResolver>(
        resolveConfigPath(),
        config_,
        existingCoreTypes_,
        objects_->coreDiscoveryService.get());
    RuntimeStateSnapshotBuilder::Callbacks runtimeStatusCallbacks;
    runtimeStatusCallbacks.appendLog = [this](const QString& message) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->appendLog(message);
        }
    };
    objects_->runtimeStateSnapshotBuilder = std::make_unique<RuntimeStateSnapshotBuilder>(std::move(runtimeStatusCallbacks));
    objects_->outboundLocationProbeService = std::make_unique<OutboundLocationProbeService>();
    objects_->appUpdateInstallService = std::make_unique<AppUpdateInstallService>(
        [](const QString& program, const QStringList& arguments) {
            return restartProcessAsAdministrator(program, arguments);
        });
    ApplicationRestartCoordinator::Callbacks restartCallbacks;
    restartCallbacks.dialogParent = [this]() -> QWidget* { return objects_->mainWindow.get(); };
    restartCallbacks.isWindowsPlatform = []() { return isWindowsPlatform(); };
    restartCallbacks.isProcessElevated = []() { return isProcessElevated(); };
    restartCallbacks.tunEnabled = [this]() { return config_.tun().tunModeItem.enableTun; };
    restartCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    restartCallbacks.persistUiState = [this]() { persistUiState(); };
    restartCallbacks.cleanupRuntimeForExit = [this](bool windowsShutdown) {
        cleanupRuntimeForExit(windowsShutdown);
    };
    restartCallbacks.setMainWindowAllowClose = [this](bool allowClose) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->setAllowClose(allowClose);
        }
    };
    restartCallbacks.setShutdownUiStatePersisted = [this](bool persisted) {
        shutdownUiStatePersisted_ = persisted;
    };
    objects_->applicationRestartCoordinator = std::make_unique<ApplicationRestartCoordinator>(
        ApplicationRestartCoordinator::Dependencies{objects_->appUpdateInstallService.get()},
        std::move(restartCallbacks));
    objects_->tunRuntimeService = std::make_unique<TunRuntimeService>();
    objects_->auxiliaryCoreProcessHost = std::make_unique<QtCoreProcessHost>();
    objects_->autoRunService = std::make_unique<WindowsAutoRunService>();
    objects_->systemProxyService = std::make_unique<WindowsSystemProxyService>();
    auto runtimeEnvironment = std::make_unique<FunctionRuntimeEnvironment>();
    runtimeEnvironment->cleanupPortProcessesFn = [this]() { cleanupCoreProcessesUsingConfiguredPorts(); };
    runtimeEnvironment->removeStaleTunAdapterFn = [this]() { return removeStaleTunAdapterIfPresent(); };
    runtimeEnvironment->skipCoreChecksFn = [this]() { return skipCoreChecks_; };
    runtimeEnvironment->isWindowsPlatformFn = []() { return isWindowsPlatform(); };
    runtimeEnvironment->isProcessElevatedFn = []() { return isProcessElevated(); };
    objects_->runtimeEnvironment = std::move(runtimeEnvironment);

    auto proxyActivationCoordinator = std::make_unique<FunctionProxyActivationCoordinator>();
    proxyActivationCoordinator->cancelBackgroundTasksForStartupFn = [this]() { cancelBackgroundTasksForProxyStartup(); };
    proxyActivationCoordinator->refreshExistingCoreTypesFn = [this]() { refreshExistingCoreTypes(); };
    proxyActivationCoordinator->isSystemProxyEnabledFn = [this]() {
        return objects_->systemProxyService != nullptr && objects_->systemProxyService->isEnabled();
    };
    proxyActivationCoordinator->updateSystemProxyModeFn = [this](SystemProxyMode mode) {
        return updateSystemProxyMode(mode);
    };
    objects_->proxyActivationCoordinator = std::move(proxyActivationCoordinator);
}
