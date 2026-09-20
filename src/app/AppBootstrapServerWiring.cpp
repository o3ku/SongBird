#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"

#include <memory>
#include <utility>

#include <QCoreApplication>
#include <QWidget>

#include "app/DefaultServerSwitchCoordinator.h"
#include "app/ProxySession.h"
#include "app/ServerCollectionCoordinator.h"
#include "app/ServerEditorCoordinator.h"
#include "domain/models/Config.h"
#include "services/ServerService.h"
#include "services/SubscriptionService.h"
#include "ui/mainwindow/MainWindow.h"

void AppBootstrap::wireServerCoordinators()
{
    DefaultServerSwitchCoordinator::Callbacks defaultServerCallbacks;
    defaultServerCallbacks.currentIndexId = [this]() { return config_.currentIndexId; };
    defaultServerCallbacks.isCoreRunning = [this]() { return isCoreRunning(); };
    defaultServerCallbacks.isTunEnabled = [this]() { return config_.tun().tunModeItem.enableTun; };
    defaultServerCallbacks.isShuttingDown = [this]() { return shuttingDown_.load(); };
    defaultServerCallbacks.uiContext = [this]() -> QObject* { return objects_->mainWindow.get(); };
    defaultServerCallbacks.lifetimeGuard = [this]() { return std::weak_ptr<char>(lifetimeGuard_); };
    defaultServerCallbacks.setDefaultServer = [this](const QString& indexId) {
        return objects_->serverService != nullptr
            ? objects_->serverService->setDefaultServer(config_, indexId)
            : OperationResult::fail(QCoreApplication::translate("AppBootstrap", "Server service is unavailable."));
    };
    defaultServerCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    defaultServerCallbacks.syncWindow = [this]() { syncWindow(); };
    defaultServerCallbacks.clearServerWarning = [this]() {
        if (objects_->proxySession != nullptr) {
            objects_->proxySession->setServerWarning({});
        }
    };
    defaultServerCallbacks.setCurrentActivationPending = [this](bool pending) {
        setCurrentActivationPending_ = pending;
    };
    defaultServerCallbacks.switchRunningCoreToServer = [this](const QString& indexId, bool enableTun) {
        if (objects_->proxySession != nullptr) {
            objects_->proxySession->switchServer(indexId, enableTun, true);
        }
    };
    defaultServerCallbacks.restartCoreIfRunning = [this](const QString& reason, bool showOverlay) {
        restartCoreIfRunning(reason, showOverlay);
    };
    defaultServerCallbacks.enableSystemProxy = [this](bool showOverlay) { enableSystemProxy(showOverlay); };
    defaultServerCallbacks.setTunEnabled = [this](bool enabled) { setTunEnabled(enabled); };
    defaultServerCallbacks.startProxyAfterSwitch = [this](bool showOverlay) {
        startManagedProxyCoreInternal(false, showOverlay);
    };
    objects_->defaultServerSwitchCoordinator = std::make_unique<DefaultServerSwitchCoordinator>(
        std::move(defaultServerCallbacks),
        objects_->mainWindow.get());

    ServerCollectionCoordinator::Callbacks serverCollectionCallbacks;
    serverCollectionCallbacks.resolveActiveServer = [this]() { return resolveActiveServerSnapshot(); };
    serverCollectionCallbacks.isCoreRunning = [this]() { return isCoreRunning(); };
    serverCollectionCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    serverCollectionCallbacks.syncWindow = [this]() { syncWindow(); };
    serverCollectionCallbacks.stopCore = [this](bool immediate) { stopCore(immediate); };
    serverCollectionCallbacks.restartCoreIfRunning = [this](const QString& reason, bool showOverlay) {
        restartCoreIfRunning(reason, showOverlay);
    };
    objects_->serverCollectionCoordinator = std::make_unique<ServerCollectionCoordinator>(
        ServerCollectionCoordinator::Dependencies{
            config_,
            *objects_->serverService,
            *objects_->subscriptionService},
        std::move(serverCollectionCallbacks));

    ServerEditorCoordinator::Callbacks serverEditorCallbacks;
    serverEditorCallbacks.dialogParent = [this]() -> QWidget* { return objects_->mainWindow.get(); };
    serverEditorCallbacks.findServer = [this](const QString& indexId) {
        return findServerSnapshotById(indexId);
    };
    serverEditorCallbacks.resolveActiveServer = [this]() { return resolveActiveServerSnapshot(); };
    serverEditorCallbacks.isCoreRunning = [this]() { return isCoreRunning(); };
    serverEditorCallbacks.appendLog = [this](const QString& message) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->appendLog(message);
        }
    };
    serverEditorCallbacks.appendResult = [this](const OperationResult& result) { appendResult(result); };
    serverEditorCallbacks.syncWindow = [this]() { syncWindow(); };
    serverEditorCallbacks.restartCoreIfRunning = [this](const QString& reason, bool showOverlay) {
        restartCoreIfRunning(reason, showOverlay);
    };
    objects_->serverEditorCoordinator = std::make_unique<ServerEditorCoordinator>(
        ServerEditorCoordinator::Dependencies{
            config_,
            *objects_->serverService},
        std::move(serverEditorCallbacks));
}
