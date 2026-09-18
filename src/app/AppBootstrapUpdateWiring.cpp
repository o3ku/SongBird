#include "app/AppBootstrap.h"
#include "app/AppBootstrapObjects.h"

#include <functional>
#include <memory>
#include <utility>

#include <QCoreApplication>
#include <QMessageBox>
#include <QThread>
#include <QWidget>

#include "app/AppUpdateCheckCoordinator.h"
#include "app/CoreUpdateCoordinator.h"
#include "app/GeoResourceUpdateCoordinator.h"
#include "app/ProxySession.h"
#include "app/UiThreadInvocation.h"
#include "common/DialogUtils.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "runtime/core/CoreCatalog.h"
#include "services/CoreUpdateService.h"
#include "services/GeoResourceUpdateService.h"
#include "ui/mainwindow/MainWindow.h"

void AppBootstrap::wireUpdateCoordinators(const std::function<void(QThread*)>& trackBackgroundThread)
{
    objects_->appUpdateCheckCoordinator = std::make_unique<AppUpdateCheckCoordinator>(
        AppUpdateCheckCoordinator::Dependencies{
            objects_->backgroundTasks.get(),
            objects_->userFeedback.get(),
            []() { return QCoreApplication::applicationVersion(); },
            [this]() { return config_.checkPreReleaseUpdate; },
            []() { return QCoreApplication::applicationDirPath(); },
            [this]() { return shuttingDown_.load(); },
            trackBackgroundThread,
            {},
            {},
            {},
            [](QObject* context, std::function<void()> callback) {
                invokeOnUiThread(context, std::move(callback));
            }
        },
        objects_->mainWindow.get());

    objects_->coreUpdateCoordinator = std::make_unique<CoreUpdateCoordinator>(
        CoreUpdateCoordinator::Dependencies{
            objects_->backgroundTasks.get(),
            lifetimeGuard_,
            [this]() { return shuttingDown_.load(); },
            [this]() -> QObject* { return objects_->mainWindow.get(); },
            [this]() -> QWidget* { return objects_->mainWindow.get(); },
            [this]() -> QObject* { return objects_->mainWindow.get(); },
            [](CoreType coreType) { return resolveRuntimeCoreType(coreType); },
            [this](CoreType coreType) { return resolveCoreInstallDirectory(coreType); },
            [this]() {
                return CoreUpdateConfig{config_.checkPreReleaseUpdate, config_.ignoreGeoUpdateCore};
            },
            [this](CoreType coreType) {
                const std::optional<VmessItem> activeServer = resolveActiveServerSnapshot();
                return isCoreRunning()
                    && activeServer.has_value()
                    && resolveRuntimeCoreType(activeServer->coreType) == coreType;
            },
            [](QWidget* parent, const QString& title, const QString& prompt) {
                return DialogUtils::askYesNoQuestion(parent, title, prompt, QMessageBox::Yes) == QMessageBox::Yes;
            },
            [this](const OperationResult& result) { appendResult(result); },
            [this](const QString& message) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->appendLog(message);
                }
            },
            [this](const QString& title, const OperationResult& result, QWidget* parent) {
                showOperationMessage(title, result, parent);
            },
            [this]() {
                if (objects_->proxySession != nullptr) {
                    objects_->proxySession->stopForCoreUpdate();
                }
            },
            [this](bool showOverlay) { enableSystemProxy(showOverlay); },
            [this]() { clearProxyStateAfterCoreStopped(); },
            [this]() { syncStatusIndicators(); },
            [this]() { refreshExistingCoreTypes(); },
            trackBackgroundThread,
            {},
            {}
        },
        objects_->mainWindow.get());

    objects_->geoResourceUpdateCoordinator = std::make_unique<GeoResourceUpdateCoordinator>(
        GeoResourceUpdateCoordinator::Dependencies{
            objects_->backgroundTasks.get(),
            [this]() { return QFileInfo(resolveConfigPath()).dir().absolutePath(); },
            [this]() -> QObject* { return objects_->mainWindow.get(); },
            [this]() -> QWidget* { return objects_->mainWindow.get(); },
            [this]() { return std::weak_ptr<char>(lifetimeGuard_); },
            [this]() { return objects_->geoResourceUpdateService != nullptr && objects_->mainWindow != nullptr; },
            trackBackgroundThread,
            [this](const QString& message) {
                if (objects_->mainWindow != nullptr) {
                    objects_->mainWindow->appendLog(message);
                }
            },
            [this](const OperationResult& result) { appendResult(result); },
            [](QWidget* parent, const QString& title, const QString& message) {
                DialogUtils::showWarning(parent, title, message);
            },
            [](QWidget* parent, const QString& title, const QString& message) {
                DialogUtils::showInformation(parent, title, message);
            },
            [this](const QString& reason) { restartCoreIfRunning(reason); }
        },
        objects_->mainWindow.get());
}
