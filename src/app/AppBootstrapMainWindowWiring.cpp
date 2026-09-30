#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"

#include <QCoreApplication>
#include <QMessageBox>

#include "app/AppUpdateCheckCoordinator.h"
#include "appcore/BackgroundTaskCoordinator.h"
#include "app/CoreUpdateCoordinator.h"
#include "app/DefaultServerSwitchCoordinator.h"
#include "app/GeoResourceUpdateCoordinator.h"
#include "appcore/ProxySession.h"
#include "app/ServerCollectionCoordinator.h"
#include "app/ServerEditorCoordinator.h"
#include "app/SpeedTestCoordinator.h"
#include "app/SubscriptionWorkflowCoordinator.h"
#include "common/DialogUtils.h"
#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/CoreTypeItem.h"
#include "domain/models/RuntimeState.h"
#include "runtime/CoreLaunchCompatDecision.h"
#include "services/RoutingService.h"
#include "services/ServerService.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/tray/TrayController.h"

// Signal wiring for the main window, the tray, the proxy session and the runtime
// state. These were the last wiring methods still living in AppBootstrap.cpp; they
// are moved here so that file stays a pure composition root (see AGENTS.md).
void AppBootstrap::wireMainWindow()
{
    if (objects_->appUpdateCheckCoordinator != nullptr) {
        QObject::connect(
            objects_->appUpdateCheckCoordinator.get(),
            &AppUpdateCheckCoordinator::updateAvailable,
            objects_->mainWindow.get(),
            [this](const AppUpdateCheckResult& result, const QString&) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->setAvailableAppUpdateVersion(result.latestVersion);
                }
            });
        QObject::connect(
            objects_->appUpdateCheckCoordinator.get(),
            &AppUpdateCheckCoordinator::updateUnavailable,
            objects_->mainWindow.get(),
            [this]() {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->clearAvailableAppUpdateVersion();
                }
            });
    }
    wireProxySessionSignals();
    wireRuntimeStateSignals();
    wireBackgroundTaskSignals();
    wireMainWindowCommands();
    wireTraySignals();
}

void AppBootstrap::wireProxySessionSignals()
{
    QObject::connect(objects_->proxySession.get(), &ProxySession::phaseChanged,
                     objects_->mainWindow.get(), [this](ProxySession::Phase phase) {
                         Q_UNUSED(phase);
                         syncStatusIndicators();
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::checklistUpdated,
                     objects_->mainWindow.get(), &MainWindow::setCoreStartupChecklist);
    QObject::connect(objects_->proxySession.get(), &ProxySession::checklistCleared,
                     objects_->mainWindow.get(), &MainWindow::clearCoreStartupChecklist);
    QObject::connect(objects_->proxySession.get(), &ProxySession::statusSyncRequested,
                     objects_->mainWindow.get(), [this]() {
                         syncStatusIndicators();
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::coreOutput,
                     objects_->mainWindow.get(), [this](const QString& line) { appendResult(OperationResult::ok(line)); });
    QObject::connect(objects_->proxySession.get(), &ProxySession::auxiliaryCoreOutput,
                     objects_->mainWindow.get(), [this](const QString& line) {
                         appendResult(OperationResult::ok(QStringLiteral("tun-compat | %1").arg(line)));
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::failed,
                     objects_->mainWindow.get(), [this](const QString& reason) {
                         appendResult(OperationResult::fail(reason));
                         saveSystemProxyMode(SystemProxyMode::ForcedClear);
                         if (setCurrentActivationPending_) {
                             appendResult(OperationResult::fail(
                                 QCoreApplication::translate(
                                     "AppBootstrap",
                                     "Node set successfully. It may be inaccessible. Please verify manually.")));
                             setCurrentActivationPending_ = false;
                         }
                         syncStatusIndicators();
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::activated,
                     objects_->mainWindow.get(), [this]() {
                         setCurrentActivationPending_ = false;
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::stopped,
                     objects_->mainWindow.get(), [this]() {
                         syncStatusIndicators();
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::logMessage,
                     objects_->mainWindow.get(), [this](const QString& msg) { appendResult(OperationResult::ok(msg)); });
    QObject::connect(objects_->proxySession.get(), &ProxySession::coreUpdateResumeRequested,
                     objects_->mainWindow.get(), [this]() {
                         if (objects_->coreUpdateCoordinator != nullptr) {
                             objects_->coreUpdateCoordinator->continuePendingCoreUpdate();
                         }
                     });
    QObject::connect(objects_->proxySession.get(), &ProxySession::serverSwitchResumeRequested,
                     objects_->mainWindow.get(), [this](const QString& indexId, bool enableTun, bool showOverlay) {
                         if (objects_->defaultServerSwitchCoordinator != nullptr) {
                             objects_->defaultServerSwitchCoordinator->scheduleSwitchAfterCoreStopped(
                                 indexId,
                                 enableTun,
                                 showOverlay);
                         }
                     });
    objects_->proxySession->setCoreSwitchConfirmation([this](const CoreLaunchCompatDecision& decision) {
        if (DialogUtils::askYesNoQuestion(
                objects_->mainWindow.get(),
                QCoreApplication::translate("AppBootstrap", "Core Compatibility"),
                coreLaunchCompatSwitchPrompt(decision),
                QMessageBox::Yes)
            != QMessageBox::Yes) {
            return false;
        }

        // Persist the switch so the prompt does not reappear on every start.
        for (CoreTypeItem& item : config_.policy().coreTypeItems) {
            if (item.configType == static_cast<int>(decision.configType)) {
                item.coreType = static_cast<int>(decision.resolvedCore);
            }
        }
        if (objects_->serverService != nullptr) {
            const OperationResult saveResult = objects_->serverService->save(config_);
            if (!saveResult.success) {
                appendResult(OperationResult::fail(QStringLiteral("%1 %2").arg(
                    QCoreApplication::translate("AppBootstrap", "Failed to save settings."),
                    saveResult.message)));
            }
        }
        return true;
    });
}

void AppBootstrap::wireRuntimeStateSignals()
{
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::snapshotApplied,
                     objects_->mainWindow.get(), &MainWindow::applyRuntimeState);
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::currentServerChanged,
                     objects_->trayController.get(), [this](const QString& name, const QString&, const QString&) {
                         if (objects_->trayController != nullptr) {
                             objects_->trayController->setCurrentServerName(name);
                         }
                     });
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::proxyUiStateChanged,
                     objects_->trayController.get(), [this](ProxyUiState state) {
                         if (objects_->trayController == nullptr) {
                             return;
                         }
                         objects_->trayController->setProxyUiState(state);
                     });
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::systemProxyStateChanged,
                     objects_->trayController.get(), [this](int mode, bool enabled) {
                         if (objects_->trayController != nullptr) {
                             objects_->trayController->setSystemProxyState(mode, enabled);
                         }
                     });
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::autoRunChanged,
                     objects_->trayController.get(), [this](bool enabled) {
                         if (objects_->trayController != nullptr) {
                             objects_->trayController->setAutoRunEnabled(enabled);
                         }
                     });
    QObject::connect(objects_->runtimeState.get(), &RuntimeState::routingStatusChanged,
                     objects_->trayController.get(), [this](const QString& routingText, const QString&) {
                         if (objects_->trayController != nullptr) {
                             objects_->trayController->setRoutingSummary(routingText);
                         }
                     });
}

void AppBootstrap::wireBackgroundTaskSignals()
{
    QObject::connect(objects_->backgroundTasks.get(), &BackgroundTaskCoordinator::runningChanged,
                     objects_->mainWindow.get(), &MainWindow::setBackgroundTaskRunning);
    QObject::connect(objects_->backgroundTasks.get(), &BackgroundTaskCoordinator::descriptionChanged,
                     objects_->mainWindow.get(), &MainWindow::setBackgroundTaskDescription);
    QObject::connect(objects_->backgroundTasks.get(), &BackgroundTaskCoordinator::runningChanged,
                     objects_->trayController.get(), &TrayController::setBackgroundTaskRunning);
    QObject::connect(objects_->backgroundTasks.get(), &BackgroundTaskCoordinator::descriptionChanged,
                     objects_->trayController.get(), &TrayController::setBackgroundTaskDescription);
}

void AppBootstrap::wireMainWindowCommands()
{
    QObject::connect(objects_->mainWindow.get(), &MainWindow::addServerRequested, objects_->mainWindow.get(), [this]() {
        if (objects_->serverEditorCoordinator != nullptr) {
            objects_->serverEditorCoordinator->addServer();
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::editServerRequested, objects_->mainWindow.get(), [this](const QString& indexId) {
        if (objects_->serverEditorCoordinator != nullptr) {
            objects_->serverEditorCoordinator->editServer(indexId);
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::importFromClipboardRequested, objects_->mainWindow.get(), [this]() {
        importFromClipboard();
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::updateSubscriptionsRequested, objects_->mainWindow.get(), [this]() {
        if (objects_->subscriptionWorkflowCoordinator != nullptr) {
            objects_->subscriptionWorkflowCoordinator->updateAll();
        }
    });
    QObject::connect(
        objects_->mainWindow.get(),
        &MainWindow::updateCurrentSubscriptionRequested,
        objects_->mainWindow.get(),
        [this](const QString& subscriptionId) {
            updateCurrentSubscription(subscriptionId);
        });
    QObject::connect(
        objects_->mainWindow.get(),
        &MainWindow::updateCurrentSubscriptionViaProxyRequested,
        objects_->mainWindow.get(),
        [this](const QString& subscriptionId) {
            updateCurrentSubscriptionViaProxy(subscriptionId);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::hideSubscriptionRequested, objects_->mainWindow.get(), [this](const QString& subscriptionId) {
        if (objects_->serverCollectionCoordinator != nullptr) {
            objects_->serverCollectionCoordinator->hideSubscription(subscriptionId);
        }
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::deleteSubscriptionRequested, objects_->mainWindow.get(), [this](const QString& subscriptionId) {
        if (objects_->serverCollectionCoordinator != nullptr) {
            objects_->serverCollectionCoordinator->deleteSubscription(subscriptionId);
        }
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::updateCoreRequested, objects_->mainWindow.get(), [this](int coreTypeValue) {
        if (objects_->coreUpdateCoordinator == nullptr) {
            return;
        }

        CoreUpdateCoordinator::Request request;
        request.coreTypeValue = coreTypeValue;
        request.startAfterSuccess = false;
        request.progressContext = objects_->mainWindow.get();
        request.dialogParent = objects_->mainWindow.get();
        objects_->coreUpdateCoordinator->updateCore(request);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::updateGeoResourcesRequested, objects_->mainWindow.get(), [this]() {
        if (objects_->geoResourceUpdateCoordinator != nullptr) {
            objects_->geoResourceUpdateCoordinator->updateGeoResources();
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::enableSystemProxyRequested, objects_->mainWindow.get(), [this]() {
        enableSystemProxy(true);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::retryCoreStartupRequested, objects_->mainWindow.get(), [this]() {
        retryCoreStartup(true);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::disableSystemProxyRequested, objects_->mainWindow.get(), [this]() {
        disableSystemProxy();
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::tunEnabledChanged, objects_->mainWindow.get(), [this](bool enabled) {
        setTunEnabled(enabled);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::routingModeSelected, objects_->mainWindow.get(), [this](const QString& routingModeId) {
        const QString previousRoutingModeId = config_.collection().routingModeId;
        const OperationResult result = objects_->routingService->setRoutingMode(config_, routingModeId);
        handleRoutingSelectionResult(result, previousRoutingModeId);
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::settingsRequested, objects_->mainWindow.get(), [this]() {
        openSettingsDialog();
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::openSettingsAtSubscriptionsTabRequested, objects_->mainWindow.get(), [this]() {
        openSettingsDialog(1);
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::openSettingsAtRoutingTabRequested, objects_->mainWindow.get(), [this]() {
        openSettingsDialog(2);
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::aboutRequested, objects_->mainWindow.get(), [this]() {
        openAboutDialog();
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::checkAppUpdateRequested, objects_->mainWindow.get(), [this]() {
        checkAppUpdates(true);
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::downloadAppUpdateRequested, objects_->mainWindow.get(), [this]() {
        if (objects_->appUpdateCheckCoordinator != nullptr) {
            objects_->appUpdateCheckCoordinator->downloadLatestAvailableUpdate(objects_->mainWindow.get());
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::uwpLoopbackRequested, objects_->mainWindow.get(), [this]() {
        openUwpLoopbackDialog();
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::removeServersRequested, objects_->mainWindow.get(), [this](const QStringList& indexIds) {
        if (objects_->serverCollectionCoordinator != nullptr) {
            objects_->serverCollectionCoordinator->removeServers(indexIds);
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::moveServersRequested, objects_->mainWindow.get(), [this](const QStringList& indexIds, int operation) {
        if (objects_->serverCollectionCoordinator != nullptr) {
            objects_->serverCollectionCoordinator->moveServers(indexIds, static_cast<ServerMoveOperation>(operation));
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::reorderServersRequested, objects_->mainWindow.get(), [this](const QStringList& orderedIndexIds) {
        if (objects_->serverCollectionCoordinator != nullptr) {
            objects_->serverCollectionCoordinator->reorderServers(orderedIndexIds);
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::setDefaultServerRequested, objects_->mainWindow.get(), [this](const QString& indexId) {
        if (objects_->defaultServerSwitchCoordinator != nullptr) {
            objects_->defaultServerSwitchCoordinator->setDefaultServer(indexId);
        }
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::setDefaultServerWithTunRequested, objects_->mainWindow.get(), [this](const QString& indexId) {
        if (objects_->defaultServerSwitchCoordinator != nullptr) {
            objects_->defaultServerSwitchCoordinator->setDefaultServerWithTun(indexId);
        }
    });
    QObject::connect(objects_->mainWindow.get(), &MainWindow::testServersRequested, objects_->mainWindow.get(), [this](const QStringList& indexIds) {
        if (objects_->speedTestCoordinator != nullptr) {
            objects_->speedTestCoordinator->startSpeedTest(indexIds);
        }
    });

    QObject::connect(objects_->mainWindow.get(), &MainWindow::hiddenToTray, objects_->mainWindow.get(), [this]() {
        objects_->mainWindow->appendLog(QStringLiteral("Main window hidden to tray."));
    });
}

void AppBootstrap::wireTraySignals()
{
    QObject::connect(objects_->trayController.get(), &TrayController::defaultServerRequested, objects_->mainWindow.get(), [this](const QString& indexId) {
        if (objects_->defaultServerSwitchCoordinator != nullptr) {
            objects_->defaultServerSwitchCoordinator->setDefaultServer(indexId);
        }
    });

    QObject::connect(objects_->trayController.get(), &TrayController::routingRequested, objects_->mainWindow.get(), [this](const QString& routingModeId) {
        const QString previousRoutingModeId = config_.collection().routingModeId;
        const OperationResult result = objects_->routingService->selectRouting(config_, routingModeId);
        handleRoutingSelectionResult(result, previousRoutingModeId);
    });

    QObject::connect(objects_->trayController.get(), &TrayController::autoRunToggled, objects_->mainWindow.get(), [this](bool enabled) {
        setAutoRunEnabled(enabled);
    });

    QObject::connect(objects_->trayController.get(), &TrayController::quitRequested, objects_->mainWindow.get(), [this]() {
        objects_->mainWindow->requestExit();
    });
}
