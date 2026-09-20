#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"

#include <memory>
#include <utility>

#include <QCoreApplication>
#include <QMessageBox>
#include <QWidget>

#include "app/FunctionRuntimeAdapters.h"
#include "app/IUserFeedback.h"
#include "app/SpeedTestCoordinator.h"
#include "common/DialogUtils.h"
#include "domain/models/Config.h"
#include "services/ServerService.h"
#include "services/SpeedTestController.h"
#include "subscription/ShareUrlBuilder.h"
#include "ui/mainwindow/MainWindow.h"
#include "ui/tray/TrayController.h"

void AppBootstrap::wireUiObjects()
{
    objects_->mainWindow = std::make_unique<MainWindow>();
    objects_->mainWindow->setShareUrlResolver([this](const QString& indexId) {
        const VmessItem* server = findServerById(indexId);
        return server == nullptr ? QString() : ShareUrlBuilder::build(*server).trimmed();
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
}
