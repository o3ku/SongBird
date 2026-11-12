#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"

#include <memory>
#include <utility>

#include <QCoreApplication>
#include <QMessageBox>
#include <QWidget>

#include "appcore/FunctionRuntimeAdapters.h"
#include "appcore/ProxySession.h"
#include "app/DefaultServerSwitchCoordinator.h"
#include "appcore/IUserFeedback.h"
#include "appcore/LocationGroupScan.h"
#include "app/AutoSwitchCoordinator.h"
#include "app/SpeedTestCoordinator.h"
#include "common/DialogUtils.h"
#include "common/ServerDisplayName.h"
#include "common/LogFileWriter.h"
#include "domain/models/Config.h"
#include "services/ServerService.h"
#include "services/SpeedTestController.h"
#include "subscription/ShareUrlBuilder.h"
#include "runtime/ClientConfigWriter.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/tray/TrayController.h"

void AppBootstrap::wireUiObjects()
{
    objects_->mainWindow = std::make_unique<MainWindow>();
    // Every line the panel shows is also appended to a file, so a report can be
    // sent after the fact. The directory is resolved here rather than inside the
    // window: paths belong to the composition root, and a window constructed
    // without a writer stays a pure in-memory sink, which is what keeps the window
    // tests off the real application data directory.
    objects_->logFileWriter = std::make_unique<LogFileWriter>(
        LogFileWriter::defaultDirectory(), QStringLiteral("songbird"));
    objects_->mainWindow->setLogFileWriter(objects_->logFileWriter.get());
    objects_->mainWindow->setShareUrlResolver([this](const QString& indexId) {
        const VmessItem* server = findServerById(indexId);
        return server == nullptr ? QString() : ShareUrlBuilder::build(*server).trimmed();
    });
    // Batch export: the window owns the save dialog and the write, this layer owns the formatting.
    // Both providers resolve the selected ids against the live config in selection order, so a
    // server removed between the click and the write simply drops out instead of exporting stale
    // data. They return an empty string when nothing is exportable, which the window reports rather
    // than writing an empty file.
    objects_->mainWindow->setShareLinksExportProvider([this](const QStringList& indexIds) {
        QList<VmessItem> servers;
        servers.reserve(indexIds.size());
        for (const QString& indexId : indexIds) {
            if (const VmessItem* server = findServerById(indexId)) {
                servers.append(*server);
            }
        }
        return exportService_.exportUris(servers).join(QLatin1Char('\n'));
    });
    objects_->mainWindow->setClashYamlExportProvider([this](const QStringList& indexIds) {
        QList<VmessItem> servers;
        servers.reserve(indexIds.size());
        for (const QString& indexId : indexIds) {
            if (const VmessItem* server = findServerById(indexId)) {
                servers.append(*server);
            }
        }
        return exportService_.exportClashYaml(servers);
    });
    // Group activation from a location tab: the menu asks "would this group actually
    // activate" and gets the generator's own answer -- the same pure generation the
    // activation will run, so an all-Xray group (or any other unrunnable member set) is
    // refused before the click instead of failing the startup afterwards.
    objects_->mainWindow->setLocationGroupActivatableProvider(
        [this](const QString& locationId) -> bool {
            if (objects_->clientConfigWriter == nullptr) {
                return false;
            }
            const QList<VmessItem> members = LocationGroupScan::embeddableMembers(
                config_.collection().servers, locationId);
            if (members.isEmpty()) {
                return false;
            }
            return objects_->clientConfigWriter
                ->generateClientGroupConfigs(config_, members)
                .generated.result.success;
        });
    objects_->trayController = std::make_unique<TrayController>(objects_->mainWindow.get());
    objects_->speedTestCoordinator = std::make_unique<SpeedTestCoordinator>(
        SpeedTestCoordinator::Dependencies{
            objects_->backgroundTasks.get(),
            objects_->speedTestController.get(),
            [this]() -> Config& { return config_; },
            [this](Config& config) {
                // Same raw literal as AppBootstrapServerWiring's setDefaultServer
                // fallback: this string is not localized yet (see todo 3.8), so
                // keep the two copies identical rather than inventing a third
                // spelling of the same condition.
                return objects_->serverService == nullptr
                    ? OperationResult::fail(QCoreApplication::translate("AppBootstrap", "Server service is unavailable."))
                    : objects_->serverService->save(config);
            },
            [this](const QString& indexId) { return findServerById(indexId); },
            [this](const VmessItem& server) { return resolveLaunchCoreType(server); },
            [this](const VmessItem& server) { return resolveCoreInfo(server); },
            [this](const QString& indexId, const QString& result) {
                return objects_->serverService == nullptr
                    ? OperationResult::fail(QCoreApplication::translate(
                          "AppBootstrap", "Speed test result service is unavailable."))
                    : objects_->serverService->setTestResult(config_, indexId, result);
            },
            [this](const OperationResult& result) { appendResult(result); },
            [this](const QString& message) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->appendLog(message);
                }
            },
            [this](const QString& indexId, const QString& result) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->updateServerTestResult(indexId, result);
                }
            },
            [this](const QStringList& indexIds, const QString& result) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->updateServerTestResults(indexIds, result);
                }
            },
            [this]() {
                if (objects_->trayController != nullptr) {
                    objects_->trayController->setServers(
                        config_.collection().servers,
                        config_.collection().subscriptions,
                        config_.currentIndexId);
                }
            }
        },
        objects_->mainWindow.get());
    auto userFeedback = std::make_unique<FunctionUserFeedback>();
    userFeedback->uiContextFn = [this]() -> QObject* { return objects_->mainWindow.get(); };
    userFeedback->dialogParentFn = [this]() -> QWidget* { return objects_->mainWindow.get(); };
    userFeedback->appendLogFn = [this](const QString& message) {
        if (objects_->mainWindow != nullptr) {
            objects_->mainWindow->appendLog(message);
        }
    };
    userFeedback->recordOperationResultFn = [this](const OperationResult& result) {
        appendResult(result);
    };
    userFeedback->showOperationMessageFn = [this](const QString& title, const OperationResult& result, QWidget* parent) {
        showOperationMessage(title, result, parent);
    };
    userFeedback->askYesNoFn = [](QWidget* parent, const QString& title, const QString& text, IUserFeedback::YesNoDefault defaultButton) {
        return DialogUtils::askYesNoQuestion(
            parent,
            title,
            text,
            defaultButton == IUserFeedback::YesNoDefault::Yes ? QMessageBox::Yes : QMessageBox::No)
            == QMessageBox::Yes;
    };
    userFeedback->showInformationFn = [](QWidget* parent, const QString& title, const QString& text) {
        DialogUtils::showInformation(parent, title, text);
    };
    userFeedback->showTrayMessageFn = [this](const QString& title, const QString& message, bool critical, int timeoutMs) {
        if (objects_->trayController != nullptr && objects_->trayController->isAvailable()) {
            objects_->trayController->showMessage(title, message, critical, timeoutMs);
        }
    };
    userFeedback->openExternalUrlFn = [this](const QString& url) {
        openExternalUrl(url);
    };
    userFeedback->promptRestartForDownloadedAppUpdateFn = [this](const QString& path, QWidget* parent) {
        return promptRestartForDownloadedAppUpdate(path, parent);
    };
    objects_->userFeedback = std::move(userFeedback);

    // Auto switch in group: constructed after the speed-test stack it borrows. The probe
    // runner owns the BackgroundTaskCoordinator token and the SpeedTestController lifetime
    // rules so the coordinator itself stays testable without cores (its ProbeRunner seam).
    objects_->autoSwitchCoordinator = std::make_unique<AutoSwitchCoordinator>(
        AutoSwitchCoordinator::Dependencies{
            [this]() { return config_.ui().autoSwitchInGroup; },
            // The group the session auto-switches *within* -- the running target, never the tab
            // the user happens to be browsing. Group mode: the persisted activation target.
            // Single-node mode: the active node's own location group. The old wiring read
            // ui().mainSelectedLocationId (the location view's selection), so a user who
            // navigated to another tab silently disabled the feature -- the failed node is not a
            // member of the group being looked at, and the scan returned failedIndex -1.
            [this]() -> QString {
                const QString groupTarget = config_.ui().activeLocationGroupId.trimmed();
                if (!groupTarget.isEmpty()) {
                    return groupTarget;
                }
                const VmessItem* active = findServerById(config_.currentIndexId);
                return active != nullptr ? LocationGroupScan::locationIdOfServer(*active) : QString();
            },
            [this]() { return config_.currentIndexId; },
            [this]() { return config_.collection().servers; },
            [this]() {
                return objects_->proxySession != nullptr && objects_->proxySession->isActive();
            },
            [this]() {
                return objects_->proxySession != nullptr
                    && (objects_->proxySession->isActive()
                        || objects_->proxySession->isActivationInProgress());
            },
            [this](const QStringList& indexIds) {
                // The same item building SpeedTestCoordinator does: launch-core resolution
                // first, then the core info the worker needs to spawn the probe core.
                QList<SpeedTestRequestItem> items;
                items.reserve(indexIds.size());
                for (const QString& indexId : indexIds) {
                    const VmessItem* server = findServerById(indexId);
                    if (server == nullptr) {
                        continue;
                    }
                    const CoreType launchCore = resolveLaunchCoreType(*server);
                    VmessItem runtimeServer = *server;
                    runtimeServer.coreType = launchCore;
                    items.append(SpeedTestRequestItem{
                        server->indexId,
                        serverDisplayName(*server),
                        server->configType,
                        runtimeServer,
                        resolveCoreInfo(runtimeServer)});
                }
                return items;
            },
            [this](QObject* completionContext,
                   const QList<SpeedTestRequestItem>& items,
                   std::function<void()> onFinished) {
                return runAutoSwitchProbe(completionContext, items, std::move(onFinished));
            },
            [this](const QString& indexId) {
                if (objects_->defaultServerSwitchCoordinator != nullptr) {
                    objects_->defaultServerSwitchCoordinator->setDefaultServer(indexId);
                }
                startManagedProxyCoreInternal(false, false);
            },
            [this](const OperationResult& result) { appendResult(result); }},
        objects_->mainWindow.get());
    if (objects_->speedTestController != nullptr) {
        // Every finished measurement refreshes the warm data -- the warm round's, the failover
        // probe's, and the user's own "test servers" batch alike.
        QObject::connect(
            objects_->speedTestController.get(),
            &SpeedTestController::testResultReady,
            objects_->autoSwitchCoordinator.get(),
            &AutoSwitchCoordinator::handleProbeResultReady);
    }
}

// One auto-switch probe batch: a startup-scope BackgroundTaskCoordinator token (internal
// bookkeeping, not a user action -- the startup scope is what keeps it from raising the
// "blocked while the core starts" banner when the predicates would apply) plus the one
// controller batch the token gates. Completion rides the controller's finished signal with the
// coordinator as context, so a coordinator destroyed mid-batch simply never hears the end.
bool AppBootstrap::runAutoSwitchProbe(
    QObject* completionContext,
    const QList<SpeedTestRequestItem>& items,
    std::function<void()> onFinished)
{
    if (objects_->speedTestController == nullptr || objects_->backgroundTasks == nullptr) {
        return false;
    }
    // The startup flow cancels background work, so a probe cannot run while a core is coming
    // up or coming down; the user's own activation outranks ours.
    if (objects_->proxySession != nullptr && objects_->proxySession->isActivationInProgress()) {
        return false;
    }
    const BackgroundTaskCoordinator::Token token =
        objects_->backgroundTasks->tryBeginStartupTask(BackgroundTaskCoordinator::Kind::SpeedTest);
    if (!token.isValid()) {
        return false;
    }
    const OperationResult started = objects_->speedTestController->start(config_, items);
    if (!started.success) {
        objects_->backgroundTasks->finish(token);
        return false;
    }

    auto* connection = new QMetaObject::Connection;
    *connection = QObject::connect(
        objects_->speedTestController.get(),
        &SpeedTestController::finished,
        completionContext,
        [this, token, onFinished = std::move(onFinished), connection](const QString&) {
            QObject::disconnect(*connection);
            delete connection;
            if (objects_->backgroundTasks != nullptr) {
                objects_->backgroundTasks->finish(token);
            }
            onFinished();
        });
    return true;
}
