#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"

#include <memory>

#include <QThread>

#include "app/CoreUpdateCoordinator.h"
#include "app/ProxySession.h"
#include "app/SettingsApplyCoordinator.h"
#include "app/SettingsWorkflowCoordinator.h"
#include "app/SubscriptionWorkflowCoordinator.h"
#include "common/AppPlatform.h"
#include "domain/models/Config.h"
#include "persistence/JsonConfigRepository.h"
#include "services/ServerService.h"
#include "services/SubscriptionService.h"
#include "ui/mainwindow/MainWindow.h"

void AppBootstrap::wireWorkflowCoordinators(const std::function<void(QThread*)>& trackBackgroundThread)
{
    SettingsApplyCoordinator::Callbacks settingsApplyCallbacks;
    settingsApplyCallbacks.platform.isWindowsPlatform = []() { return isWindowsPlatform(); };
    settingsApplyCallbacks.platform.isProcessElevated = []() { return isProcessElevated(); };
    settingsApplyCallbacks.runtime.isCoreRunning = [this]() { return isCoreRunning(); };
    settingsApplyCallbacks.runtime.isCoreReady = [this]() { return isCoreReady(); };
    settingsApplyCallbacks.runtime.restartCoreIfRunning = [this](const QString& reason, bool showOverlay) {
        restartCoreIfRunning(reason, showOverlay);
    };
    settingsApplyCallbacks.systemProxy.hasService = [this]() { return objects_->systemProxyService != nullptr; };
    settingsApplyCallbacks.systemProxy.updateMode = [this](SystemProxyMode mode) {
        return updateSystemProxyMode(mode);
    };
    settingsApplyCallbacks.systemProxy.adoptManagedProxy = [this](bool enabled) {
        if (objects_->proxySession != nullptr) {
            objects_->proxySession->adoptManagedSystemProxy(enabled);
        }
    };
    settingsApplyCallbacks.workflow.promptRestartAsAdministratorForTun = [this]() {
        return promptRestartAsAdministratorForTun();
    };
    settingsApplyCallbacks.workflow.promptRestartForLanguageChange = [this]() {
        promptRestartForLanguageChange();
    };
    settingsApplyCallbacks.workflow.updateSubscriptionsByIds =
        [this](const QStringList& subscriptionIds, bool useProxy, const QString& startupMessage) {
            if (objects_->subscriptionWorkflowCoordinator != nullptr) {
                objects_->subscriptionWorkflowCoordinator->updateByIds(subscriptionIds, useProxy, startupMessage);
            }
        };
    settingsApplyCallbacks.ui.selectSubscriptionTab = [this](const QString& subscriptionId) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->selectSubscriptionTab(subscriptionId);
        }
    };
    settingsApplyCallbacks.ui.appendResult = [this](const OperationResult& result) { appendResult(result); };
    settingsApplyCallbacks.ui.syncWindow = [this]() { syncWindow(); };
    settingsApplyCallbacks.ui.syncStatusIndicators = [this]() { syncStatusIndicators(); };
    objects_->settingsApplyCoordinator = std::make_unique<SettingsApplyCoordinator>(
        SettingsApplyCoordinator::Dependencies{
            config_,
            *objects_->serverService,
            objects_->autoRunService.get()},
        std::move(settingsApplyCallbacks));

    objects_->settingsWorkflowCoordinator = std::make_unique<SettingsWorkflowCoordinator>(
        SettingsWorkflowCoordinator::Dependencies{
            [this]() -> QWidget* { return objects_->mainWindow.get(); },
            [this]() { return objects_->mainWindow != nullptr && objects_->serverService != nullptr; },
            [this]() { return config_; },
            [this]() { return existingCoreTypes_; },
            [this]() { return std::weak_ptr<char>(lifetimeGuard_); },
            [this]() { refreshExistingCoreTypes(); },
            trackBackgroundThread,
            [this](CoreType coreType) { return detectCoreVersion(coreType); },
            [this](
                CoreType requestedCoreType,
                QObject* progressContext,
                QWidget* dialogParent,
                const std::function<void(const QString&)>& progressObserver,
                const std::function<void(const OperationResult&)>& completionObserver) {
                if (objects_->coreUpdateCoordinator == nullptr) {
                    return;
                }

                CoreUpdateCoordinator::Request request;
                request.coreTypeValue = static_cast<int>(requestedCoreType);
                request.startAfterSuccess = false;
                request.progressContext = progressContext;
                request.dialogParent = dialogParent;
                request.skipConfirmation = false;
                request.skipLocalVersionCheck = false;
                request.progressObserver = progressObserver;
                request.completionObserver = completionObserver;
                objects_->coreUpdateCoordinator->updateCore(request);
            },
            [this]() { restoreConfigFromBackup(); },
            [this](const Config& updatedConfig) { applySettingsDialogResult(updatedConfig); },
            [this](const QList<int>& selectedRows, const Config& sessionConfig) {
                const OperationResult saveResult =
                    objects_->subscriptionService->saveSubscriptions(config_, sessionConfig.collection().subscriptions);
                appendResult(saveResult);
                if (saveResult.success) {
                    syncWindow();
                    if (objects_->subscriptionWorkflowCoordinator != nullptr) {
                        objects_->subscriptionWorkflowCoordinator->updateSelected(selectedRows);
                    }
                }
                return saveResult;
            }
        },
        objects_->mainWindow.get());

    SubscriptionWorkflowCoordinator::Callbacks subscriptionCallbacks;
    subscriptionCallbacks.config.configPath = [this]() { return resolveConfigPath(); };
    subscriptionCallbacks.config.currentConfig = [this]() { return config_; };
    subscriptionCallbacks.config.activeSubscriptionId = [this]() {
        const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
        return activeServer.has_value() ? activeServer->subId.trimmed() : QString();
    };
    subscriptionCallbacks.config.customConfigDirectory = [this]() { return resolveCustomConfigDirectory(); };
    subscriptionCallbacks.config.reloadConfig = [this]() {
        if (objects_->repository != nullptr) {
            config_ = objects_->repository->load();
        }
    };
    subscriptionCallbacks.ui.context = [this]() -> QObject* { return objects_->mainWindow.get(); };
    subscriptionCallbacks.ui.lifetimeGuard = [this]() { return std::weak_ptr<char>(lifetimeGuard_); };
    subscriptionCallbacks.ui.showStartupMessage = [this](const QString& message) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->appendLog(message);
            objects_->mainWindow->showTransientStatus(
                message,
                3000,
                MainWindow::TransientStatusPriority::Routine);
        }
    };
    subscriptionCallbacks.ui.setSubscriptionUpdateRunning = [this](bool running) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->setSubscriptionUpdateRunning(running);
        }
    };
    subscriptionCallbacks.ui.clearServerTestResultsAndSync = [this]() {
        for (auto& server : config_.collection().servers) {
            server.testResult = QStringLiteral("...");
        }
        syncWindow();
    };
    subscriptionCallbacks.ui.appendResult = [this](const OperationResult& result) { appendResult(result); };
    subscriptionCallbacks.ui.syncWindow = [this]() { syncWindow(); };
    subscriptionCallbacks.ui.selectSubscriptionTab = [this](const QString& subscriptionId) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->selectSubscriptionTab(subscriptionId);
        }
    };
    subscriptionCallbacks.thread.trackBackgroundThread = trackBackgroundThread;
    subscriptionCallbacks.workflow.restartCoreIfRunning = [this](const QString& reason, bool showOverlay) {
        restartCoreIfRunning(reason, showOverlay);
    };
    objects_->subscriptionWorkflowCoordinator = std::make_unique<SubscriptionWorkflowCoordinator>(
        SubscriptionWorkflowCoordinator::Dependencies{
            *objects_->backgroundTasks,
            std::move(subscriptionCallbacks)},
        objects_->mainWindow.get());
}
