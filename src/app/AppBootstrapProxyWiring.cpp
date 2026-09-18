#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"

#include <memory>
#include <utility>

#include <QCoreApplication>
#include <QWidget>

#include "app/AppRuntimeResolver.h"
#include "app/BackgroundTaskCoordinator.h"
#include "app/OutboundLocationProbeService.h"
#include "app/ProxyRuntimeInterfaces.h"
#include "app/ProxySession.h"
#include "app/SystemProxyCoordinator.h"
#include "app/TunModeCoordinator.h"
#include "common/AppPlatform.h"
#include "domain/models/Config.h"
#include "domain/models/RuntimeState.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/QtCoreProcessHost.h"
#include "services/ServerService.h"
#include "ui/mainwindow/MainWindow.h"

void AppBootstrap::wireProxyStack()
{
    objects_->proxySession = std::make_unique<ProxySession>(ProxySession::Dependencies{
        *objects_->coreProcessHost,
        *objects_->auxiliaryCoreProcessHost,
        *objects_->clientConfigWriter,
        *objects_->outboundLocationProbeService,
        *objects_->backgroundTasks,
        *objects_->appRuntimeResolver,
        *objects_->runtimeEnvironment,
        *objects_->proxyActivationCoordinator
    });
    objects_->runtimeState = std::make_unique<RuntimeState>();
    SystemProxyCoordinator::Callbacks systemProxyCallbacks;
    systemProxyCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    systemProxyCallbacks.syncStatusIndicators = [this]() { syncStatusIndicators(); };
    systemProxyCallbacks.startManagedProxyCore = [this](bool skipTunCleanup, bool showStartupOverlay) {
        startManagedProxyCoreInternal(skipTunCleanup, showStartupOverlay);
    };
    objects_->systemProxyCoordinator = std::make_unique<SystemProxyCoordinator>(
        SystemProxyCoordinator::Dependencies{
            config_,
            *objects_->serverService,
            objects_->systemProxyService.get(),
            objects_->proxySession.get()},
        std::move(systemProxyCallbacks));
    TunModeCoordinator::Callbacks tunModeCallbacks;
    tunModeCallbacks.isWindowsPlatform = []() { return isWindowsPlatform(); };
    tunModeCallbacks.isProcessElevated = []() { return isProcessElevated(); };
    tunModeCallbacks.isCoreRunning = [this]() { return isCoreRunning(); };
    tunModeCallbacks.resolveActiveServer = [this]() { return resolveActiveServerSnapshot(); };
    tunModeCallbacks.askRestartAsAdministratorForTun = [this]() { return askRestartAsAdministratorForTun(); };
    tunModeCallbacks.restartApplication = [this](bool requireAdministrator) {
        return restartApplication(requireAdministrator);
    };
    tunModeCallbacks.persistUiState = [this]() { persistUiState(); };
    tunModeCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    tunModeCallbacks.syncWindow = [this]() { syncWindow(); };
    tunModeCallbacks.syncStatusIndicators = [this]() { syncStatusIndicators(); };
    tunModeCallbacks.restartCoreIfRunning = [this](const QString& reason, bool showOverlay) {
        restartCoreIfRunning(reason, showOverlay);
    };
    tunModeCallbacks.enableSystemProxy = [this](bool showOverlay) { enableSystemProxy(showOverlay); };
    objects_->tunModeCoordinator = std::make_unique<TunModeCoordinator>(
        TunModeCoordinator::Dependencies{
            config_,
            *objects_->serverService},
        std::move(tunModeCallbacks));
    objects_->backgroundTasks->setBlockingPredicate([this]() {
        return isProxyActivationInProgress();
    });
    QObject::connect(objects_->backgroundTasks.get(), &BackgroundTaskCoordinator::blockedByCoreStartup,
        objects_->backgroundTasks.get(), [this]() {
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "Proxy startup is in progress.")));
        });
    QObject::connect(objects_->backgroundTasks.get(), &BackgroundTaskCoordinator::busyWithAnotherTask,
        objects_->backgroundTasks.get(), [this](const QString& description) {
            appendResult(OperationResult::fail(
                QCoreApplication::translate("AppBootstrap", "%1 is already running. Please wait for it to finish.")
                    .arg(description)));
        });
}
