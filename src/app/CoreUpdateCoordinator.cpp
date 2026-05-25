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
        pendingCoreUpdate_.store(
            coreType,
            request.startAfterSuccess,
            request.skipLocalVersionCheck,
            progressContextGuard,
            dialogParentGuard,
            request.progressObserver,
            request.completionObserver,
            token);
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
    if (fallbackUiContext() == nullptr || !pendingCoreUpdate_.isValid()) {
        return;
    }

    CoreUpdatePendingState::Snapshot pending = pendingCoreUpdate_.take();

    if (!isCurrent(pending.taskToken)) {
        return;
    }

    const QString title = QCoreApplication::translate("AppBootstrap", "Install / Update %1 Core")
                              .arg(coreTypeDisplayName(pending.coreType));
    const QString installDirectory =
        deps_.resolveCoreInstallDirectory ? deps_.resolveCoreInstallDirectory(pending.coreType) : QString();
    const CoreUpdateConfig workerConfig =
        deps_.makeWorkerConfig ? deps_.makeWorkerConfig() : CoreUpdateConfig{};

    startCoreUpdateWorker(
        pending.taskToken,
        pending.coreType,
        workerConfig,
        installDirectory,
        title,
        true,
        pending.startAfterSuccess,
        pending.skipLocalVersionCheck,
        pending.progressContext,
        pending.dialogParent,
        std::move(pending.progressObserver),
        std::move(pending.completionObserver));
}

bool CoreUpdateCoordinator::hasPendingCoreUpdate() const
{
    return pendingCoreUpdate_.isValid();
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
    pendingCoreUpdate_.clearIfGeneration(token.generation);
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
