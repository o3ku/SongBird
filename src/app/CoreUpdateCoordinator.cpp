#include "app/CoreUpdateCoordinator.h"

#include <utility>

#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>
#include <QWidget>

namespace {

template <typename Callback>
void invokeOnUiThread(QObject* context, Callback&& callback)
{
    if (context == nullptr) {
        return;
    }

    if (QThread::currentThread() == context->thread()) {
        callback();
        return;
    }

    QMetaObject::invokeMethod(context, std::forward<Callback>(callback), Qt::QueuedConnection);
}

void appendResult(
    const std::function<void(const OperationResult&)>& appendResult,
    const OperationResult& result)
{
    if (appendResult) {
        appendResult(result);
    }
}

void appendLog(const std::function<void(const QString&)>& appendLog, const QString& message)
{
    if (appendLog) {
        appendLog(message);
    }
}

void showTransientStatus(
    const std::function<void(const QString&, int)>& showTransientStatus,
    const QString& message,
    int timeout)
{
    if (showTransientStatus) {
        showTransientStatus(message, timeout);
    }
}

} // namespace

CoreUpdateCoordinator::CoreUpdateCoordinator(Dependencies dependencies, QObject* parent)
    : QObject(parent)
    , deps_(std::move(dependencies))
{
    if (!deps_.resolveRuntimeCoreType) {
        deps_.resolveRuntimeCoreType = [](CoreType coreType) {
            return ::resolveRuntimeCoreType(coreType);
        };
    }
    if (!deps_.updateRunner) {
        deps_.updateRunner = [](
                                 CoreType coreType,
                                 const CoreUpdateConfig& config,
                                 const QString& installDirectory,
                                 const CoreUpdateService::UpdateOptions& options) {
            CoreUpdateService coreUpdateService;
            return coreUpdateService.update(coreType, config, installDirectory, options);
        };
    }
}

void CoreUpdateCoordinator::updateCore(const Request& request)
{
    if (deps_.backgroundTasks == nullptr || fallbackUiContext() == nullptr) {
        return;
    }

    const auto notifyCompletion = [&request](const OperationResult& result) {
        if (request.completionObserver) {
            request.completionObserver(result);
        }
    };

    const BackgroundTaskCoordinator::Token token =
        deps_.backgroundTasks->tryBeginUserTask(BackgroundTaskCoordinator::Kind::CoreUpdate);
    if (!token.isValid()) {
        return;
    }

    const CoreType coreType = deps_.resolveRuntimeCoreType(static_cast<CoreType>(request.coreTypeValue));
    const QString title = QCoreApplication::translate("AppBootstrap", "Install / Update %1 Core")
                              .arg(coreTypeDisplayName(coreType));
    const QString installDirectory =
        deps_.resolveCoreInstallDirectory ? deps_.resolveCoreInstallDirectory(coreType) : QString();
    const CoreUpdateConfig workerConfig =
        deps_.makeWorkerConfig ? deps_.makeWorkerConfig() : CoreUpdateConfig{};
    const bool shouldRestartRunningCore =
        deps_.shouldStopRunningCoreForUpdate && deps_.shouldStopRunningCoreForUpdate(coreType);
    QPointer<QObject> progressContextGuard(request.progressContext != nullptr
            ? request.progressContext
            : fallbackUiContext());
    QPointer<QWidget> dialogParentGuard(request.dialogParent != nullptr
            ? request.dialogParent
            : fallbackDialogParent());

    QWidget* messageParent = dialogParentGuard.isNull() ? fallbackDialogParent() : dialogParentGuard.data();
    const QString prompt = QCoreApplication::translate(
                               "CoreUpdateService",
                               "Install / Update %1?\nThe running core will be stopped before installation if needed.")
                               .arg(coreTypeDisplayName(coreType));
    const bool confirmed = request.skipConfirmation
        || (deps_.confirmUpdate && deps_.confirmUpdate(messageParent, title, prompt));
    if (!confirmed) {
        if (deps_.backgroundTasks->isCurrent(token)) {
            deps_.backgroundTasks->finish(token);
        }
        const OperationResult result = OperationResult::ok(
            QCoreApplication::translate("AppBootstrap", "%1 update was canceled.")
                .arg(coreTypeDisplayName(coreType)));
        appendResult(deps_.appendResult, result);
        notifyCompletion(result);
        return;
    }

    if (shouldRestartRunningCore) {
        const QString message = QCoreApplication::translate(
            "AppBootstrap",
            "Stopping the running core before installing the update.");
        appendResult(deps_.appendResult, OperationResult::ok(message));
        pendingCoreUpdateType_ = coreType;
        pendingCoreUpdateStartAfterSuccess_ = request.startAfterSuccess;
        pendingCoreUpdateSkipLocalVersionCheck_ = request.skipLocalVersionCheck;
        pendingCoreUpdateProgressContext_ = progressContextGuard;
        pendingCoreUpdateDialogParent_ = dialogParentGuard;
        pendingCoreUpdateProgressObserver_ = request.progressObserver;
        pendingCoreUpdateCompletionObserver_ = request.completionObserver;
        pendingCoreUpdateTaskToken_ = token;
        if (deps_.stopForCoreUpdate) {
            deps_.stopForCoreUpdate();
        }
        return;
    }

    startCoreUpdateWorker(
        token,
        coreType,
        workerConfig,
        installDirectory,
        title,
        false,
        request.startAfterSuccess,
        request.skipLocalVersionCheck,
        progressContextGuard,
        dialogParentGuard,
        request.progressObserver,
        request.completionObserver);
}

void CoreUpdateCoordinator::continuePendingCoreUpdate()
{
    if (fallbackUiContext() == nullptr || !pendingCoreUpdateTaskToken_.isValid()) {
        return;
    }

    const CoreType coreType = pendingCoreUpdateType_;
    const bool startAfterSuccess = pendingCoreUpdateStartAfterSuccess_;
    const bool skipLocalVersionCheck = pendingCoreUpdateSkipLocalVersionCheck_;
    QPointer<QObject> progressContextGuard = pendingCoreUpdateProgressContext_;
    QPointer<QWidget> dialogParentGuard = pendingCoreUpdateDialogParent_;
    std::function<void(const QString&)> progressObserver = pendingCoreUpdateProgressObserver_;
    std::function<void(const OperationResult&)> completionObserver = pendingCoreUpdateCompletionObserver_;
    const BackgroundTaskCoordinator::Token token = pendingCoreUpdateTaskToken_;

    clearPendingCoreUpdate();

    if (!isCurrent(token)) {
        return;
    }

    const QString title = QCoreApplication::translate("AppBootstrap", "Install / Update %1 Core")
                              .arg(coreTypeDisplayName(coreType));
    const QString installDirectory =
        deps_.resolveCoreInstallDirectory ? deps_.resolveCoreInstallDirectory(coreType) : QString();
    const CoreUpdateConfig workerConfig =
        deps_.makeWorkerConfig ? deps_.makeWorkerConfig() : CoreUpdateConfig{};

    startCoreUpdateWorker(
        token,
        coreType,
        workerConfig,
        installDirectory,
        title,
        true,
        startAfterSuccess,
        skipLocalVersionCheck,
        progressContextGuard,
        dialogParentGuard,
        std::move(progressObserver),
        std::move(completionObserver));
}

bool CoreUpdateCoordinator::hasPendingCoreUpdate() const
{
    return pendingCoreUpdateTaskToken_.isValid();
}

void CoreUpdateCoordinator::runCoreUpdateTask(
    BackgroundTaskCoordinator::Token token,
    CoreType coreType,
    CoreUpdateConfig workerConfig,
    QString installDirectory,
    QPointer<QObject> progressContextGuard,
    bool skipLocalVersionCheck,
    std::function<void(const QString&)> progressObserver,
    std::function<void(const OperationResult&)> completionObserver)
{
    const std::weak_ptr<char> lifetimeGuard = weakLifetimeGuard();

    const auto reportProgress = [this, progressContextGuard, progressObserver, token, lifetimeGuard](const QString& message) {
        if (message.trimmed().isEmpty()) {
            return;
        }

        QObject* uiTarget = progressContextGuard.isNull() ? fallbackUiContext() : progressContextGuard.data();
        invokeOnUiThread(uiTarget, [this, message, progressObserver, token, lifetimeGuard]() {
            if (lifetimeGuard.expired() || !isCurrent(token) || fallbackUiContext() == nullptr) {
                return;
            }

            appendLog(deps_.appendLog, message);
            showTransientStatus(deps_.showRoutineTransientStatus, message, 0);
            if (progressObserver) {
                progressObserver(message);
            }
        });
    };

    CoreUpdateService::UpdateOptions updateOptions;
    updateOptions.progressHandler = reportProgress;
    updateOptions.cancelCheck = [this]() {
        QThread* currentThread = QThread::currentThread();
        return isShuttingDown()
            || (currentThread != nullptr && currentThread->isInterruptionRequested());
    };
    updateOptions.skipLocalVersionCheck = skipLocalVersionCheck;

    const OperationResult result = deps_.updateRunner(
        coreType,
        workerConfig,
        installDirectory,
        updateOptions);

    QObject* uiTarget = progressContextGuard.isNull() ? fallbackUiContext() : progressContextGuard.data();
    invokeOnUiThread(uiTarget, [this, completionObserver, result, token, lifetimeGuard]() {
        if (lifetimeGuard.expired() || !isCurrent(token)) {
            return;
        }
        if (completionObserver) {
            completionObserver(result);
        }
    });
}

void CoreUpdateCoordinator::finalizeCoreUpdate(
    BackgroundTaskCoordinator::Token token,
    const QString& title,
    const OperationResult& result,
    bool stoppedForInstall,
    bool startAfterSuccess,
    QPointer<QWidget> dialogParentGuard,
    const std::function<void(const OperationResult&)>& completionObserver)
{
    if (!isCurrent(token)) {
        return;
    }

    deps_.backgroundTasks->finish(token);
    if (pendingCoreUpdateTaskToken_.generation == token.generation) {
        pendingCoreUpdateTaskToken_ = {};
    }
    if (isShuttingDown()) {
        return;
    }

    if (fallbackUiContext() != nullptr && !result.message.trimmed().isEmpty()) {
        showTransientStatus(deps_.showTransientStatus, result.message, 5000);
    }
    if (result.success && deps_.refreshExistingCoreTypes) {
        deps_.refreshExistingCoreTypes();
    }
    if (completionObserver) {
        completionObserver(result);
    }

    if (stoppedForInstall) {
        if (result.success && result.requiresRestart && startAfterSuccess) {
            const QString message = QCoreApplication::translate(
                "AppBootstrap",
                "Restarting the updated core...");
            if (fallbackUiContext() != nullptr) {
                appendLog(deps_.appendLog, message);
                showTransientStatus(deps_.showRoutineTransientStatus, message, 0);
            }
            if (deps_.enableSystemProxy) {
                deps_.enableSystemProxy(true);
            }
        } else if (!result.success) {
            const QString message = QCoreApplication::translate(
                "AppBootstrap",
                "Update failed. Restoring the previous running core...");
            if (fallbackUiContext() != nullptr) {
                appendLog(deps_.appendLog, message);
                showTransientStatus(deps_.showTransientStatus, message, 0);
            }
            if (deps_.enableSystemProxy) {
                deps_.enableSystemProxy(false);
            }
        } else {
            if (deps_.clearProxyStateAfterCoreStopped) {
                deps_.clearProxyStateAfterCoreStopped();
            }
            if (deps_.syncStatusIndicators) {
                deps_.syncStatusIndicators();
            }
        }
    }

    if (!stoppedForInstall && startAfterSuccess && result.success) {
        const QString message = QCoreApplication::translate(
            "AppBootstrap",
            "Starting the downloaded core...");
        if (fallbackUiContext() != nullptr) {
            appendLog(deps_.appendLog, message);
            showTransientStatus(deps_.showRoutineTransientStatus, message, 0);
        }
        if (deps_.enableSystemProxy) {
            deps_.enableSystemProxy(true);
        }
    }

    if (fallbackUiContext() == nullptr || result.message.trimmed().isEmpty()) {
        return;
    }

    QWidget* messageParent = dialogParentGuard.isNull() ? fallbackDialogParent() : dialogParentGuard.data();
    if (deps_.showOperationMessage) {
        deps_.showOperationMessage(title, result, messageParent);
    }
}

void CoreUpdateCoordinator::startCoreUpdateWorker(
    BackgroundTaskCoordinator::Token token,
    CoreType coreType,
    CoreUpdateConfig workerConfig,
    QString installDirectory,
    QString title,
    bool stoppedForInstall,
    bool startAfterSuccess,
    bool skipLocalVersionCheck,
    QPointer<QObject> progressContextGuard,
    QPointer<QWidget> dialogParentGuard,
    std::function<void(const QString&)> progressObserver,
    std::function<void(const OperationResult&)> completionObserver)
{
    const QString startupMessage = QCoreApplication::translate("AppBootstrap", "Starting %1 update...")
                                       .arg(coreTypeDisplayName(coreType));
    appendLog(deps_.appendLog, startupMessage);
    showTransientStatus(deps_.showRoutineTransientStatus, startupMessage, 3000);

    const std::weak_ptr<char> lifetimeGuard = weakLifetimeGuard();
    auto task = [this,
                 coreType,
                 workerConfig,
                 installDirectory = std::move(installDirectory),
                 title = std::move(title),
                 stoppedForInstall,
                 startAfterSuccess,
                 skipLocalVersionCheck,
                 progressContextGuard,
                 dialogParentGuard,
                 progressObserver = std::move(progressObserver),
                 completionObserver = std::move(completionObserver),
                 token,
                 lifetimeGuard]() {
        if (lifetimeGuard.expired()) {
            return;
        }
        runCoreUpdateTask(
            token,
            coreType,
            workerConfig,
            installDirectory,
            progressContextGuard,
            skipLocalVersionCheck,
            progressObserver,
            [this, title, stoppedForInstall, startAfterSuccess, dialogParentGuard, completionObserver, token, lifetimeGuard](const OperationResult& result) {
                if (lifetimeGuard.expired() || !isCurrent(token)) {
                    return;
                }
                finalizeCoreUpdate(
                    token,
                    title,
                    result,
                    stoppedForInstall,
                    startAfterSuccess,
                    dialogParentGuard,
                    completionObserver);
            });
    };

    if (deps_.startBackgroundTask) {
        deps_.startBackgroundTask(std::move(task));
        return;
    }

    QThread* thread = QThread::create(std::move(task));
    if (deps_.trackBackgroundThread) {
        deps_.trackBackgroundThread(thread);
    }
    thread->start();
}

void CoreUpdateCoordinator::clearPendingCoreUpdate()
{
    pendingCoreUpdateType_ = CoreType::Unknown;
    pendingCoreUpdateStartAfterSuccess_ = false;
    pendingCoreUpdateSkipLocalVersionCheck_ = false;
    pendingCoreUpdateProgressContext_.clear();
    pendingCoreUpdateDialogParent_.clear();
    pendingCoreUpdateProgressObserver_ = {};
    pendingCoreUpdateCompletionObserver_ = {};
    pendingCoreUpdateTaskToken_ = {};
}

QObject* CoreUpdateCoordinator::fallbackUiContext() const
{
    if (deps_.uiContext) {
        return deps_.uiContext();
    }
    if (deps_.defaultProgressContext) {
        return deps_.defaultProgressContext();
    }
    return nullptr;
}

QWidget* CoreUpdateCoordinator::fallbackDialogParent() const
{
    return deps_.defaultDialogParent ? deps_.defaultDialogParent() : nullptr;
}

bool CoreUpdateCoordinator::isCurrent(const BackgroundTaskCoordinator::Token& token) const
{
    return deps_.backgroundTasks != nullptr && deps_.backgroundTasks->isCurrent(token);
}

bool CoreUpdateCoordinator::isShuttingDown() const
{
    return deps_.isShuttingDown && deps_.isShuttingDown();
}

std::weak_ptr<char> CoreUpdateCoordinator::weakLifetimeGuard() const
{
    return deps_.lifetimeGuard;
}
