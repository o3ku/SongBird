#include "app/AppUpdateCheckCoordinator.h"

#include <QCoreApplication>
#include <QDir>
#include <QMetaObject>
#include <QThread>
#include <QWidget>

#include <utility>

#include "app/AppUpdateCheckPresentation.h"

namespace {

bool isShuttingDown(const AppUpdateCheckCoordinator::Dependencies& deps)
{
    return deps.isShuttingDown && deps.isShuttingDown();
}

} // namespace

AppUpdateCheckCoordinator::AppUpdateCheckCoordinator(Dependencies dependencies, QObject* parent)
    : QObject(parent)
    , deps_(std::move(dependencies))
{
}

bool AppUpdateCheckCoordinator::isCheckRunning() const
{
    return checkRunning_;
}

void AppUpdateCheckCoordinator::checkAppUpdates(bool manual)
{
    if (deps_.feedback == nullptr) {
        return;
    }

    if (checkRunning_) {
        Q_UNUSED(manual);
        return;
    }

    checkRunning_ = true;
    const QString title = QCoreApplication::translate("AppBootstrap", "Check for Updates");
    if (manual) {
        const QString message = QCoreApplication::translate("AppBootstrap", "Checking for SongBird updates...");
        deps_.feedback->appendLog(message);
    }

    const QString currentVersion = deps_.currentVersion
        ? deps_.currentVersion()
        : QCoreApplication::applicationVersion();
    const bool allowPrerelease = deps_.allowPrerelease && deps_.allowPrerelease();
    QPointer<QObject> uiContext(deps_.feedback->uiContext());
    QPointer<QWidget> dialogParent(deps_.feedback->dialogParent());
    QPointer<AppUpdateCheckCoordinator> self(this);

    runInBackground([self, title, currentVersion, allowPrerelease, manual, uiContext, dialogParent]() {
        OperationResult result;
        AppUpdateCheckResult updateResult;
        if (self.isNull()) {
            return;
        }

        if (self->deps_.checkForUpdate) {
            result = self->deps_.checkForUpdate(currentVersion, allowPrerelease, &updateResult);
        } else {
            AppUpdateService service;
            result = service.checkForUpdate(currentVersion, allowPrerelease, &updateResult);
        }

        QObject* target = uiContext.isNull()
            ? static_cast<QObject*>(QCoreApplication::instance())
            : uiContext.data();
        self->invokeOnUiThread(target, [self, title, result, updateResult, manual, dialogParent]() {
            if (self.isNull()) {
                return;
            }

            self->checkRunning_ = false;
            if (self->deps_.feedback == nullptr || isShuttingDown(self->deps_)) {
                return;
            }

            QWidget* messageParent = self->resolveMessageParent(dialogParent);
            if (!result.success) {
                if (manual) {
                    self->deps_.feedback->showOperationMessage(title, result, messageParent);
                }
                return;
            }

            if (!updateResult.updateAvailable) {
                self->latestAvailableUpdate_ = {};
                emit self->updateUnavailable();
                if (manual) {
                    self->deps_.feedback->showOperationMessage(title, result, messageParent);
                }
                return;
            }

            self->latestAvailableUpdate_ = updateResult;
            emit self->updateAvailable(updateResult, result.message);
            self->deps_.feedback->recordOperationResult(result);
            self->deps_.feedback->showTrayMessage(
                QCoreApplication::translate("AppBootstrap", "Update Available"),
                result.message,
                false,
                8000);

            if (!manual) {
                return;
            }

            QString prompt = AppUpdateCheckPresentation::buildUpdateAvailablePrompt(updateResult, result.message);

            if (!updateResult.assetDownloadUrl.isValid()) {
                prompt = AppUpdateCheckPresentation::appendMissingDownloadPrompt(prompt);
                if (self->deps_.feedback->askYesNo(
                        messageParent,
                        QCoreApplication::translate("AppBootstrap", "Update Available"),
                        prompt,
                        IUserFeedback::YesNoDefault::Yes)) {
                    self->deps_.feedback->openExternalUrl(updateResult.releaseUrl.toString());
                }
                return;
            }

            if (self->deps_.feedback->askYesNo(
                    messageParent,
                    QCoreApplication::translate("AppBootstrap", "Update Available"),
                    prompt,
                    IUserFeedback::YesNoDefault::Yes)) {
                self->downloadAppUpdate(updateResult, dialogParent);
            }
        });
    });
}

void AppUpdateCheckCoordinator::downloadAppUpdate(const AppUpdateCheckResult& updateResult, QPointer<QWidget> dialogParent)
{
    if (deps_.feedback == nullptr || deps_.backgroundTasks == nullptr) {
        return;
    }

    const BackgroundTaskCoordinator::Token token =
        deps_.backgroundTasks->tryBeginUserTask(BackgroundTaskCoordinator::Kind::AppUpdate);
    if (!token.isValid()) {
        return;
    }

    const QString title = QCoreApplication::translate("AppBootstrap", "Download SongBird Update");
    const QString appDir = deps_.applicationDirectory
        ? deps_.applicationDirectory()
        : QCoreApplication::applicationDirPath();
    const QString updateDirectory = QDir(appDir).filePath(QStringLiteral("updates"));
    const QString assetName = AppUpdateCheckPresentation::resolveDownloadAssetName(updateResult.assetName);
    const QString startMessage = QCoreApplication::translate("AppBootstrap", "Downloading SongBird %1...")
                                     .arg(updateResult.latestVersion);
    deps_.feedback->appendLog(startMessage);

    QPointer<QObject> uiContext(deps_.feedback->uiContext());
    QPointer<AppUpdateCheckCoordinator> self(this);
    runInBackground([self, updateResult, updateDirectory, assetName, title, dialogParent, token, uiContext]() {
        OperationResult result;
        QString savedPath;
        if (self.isNull()) {
            return;
        }

        if (self->deps_.downloadUpdate) {
            result = self->deps_.downloadUpdate(
                updateResult.assetDownloadUrl,
                assetName,
                updateDirectory,
                &savedPath);
        } else {
            AppUpdateService service;
            result = service.downloadUpdate(
                updateResult.assetDownloadUrl,
                assetName,
                updateDirectory,
                &savedPath);
        }

        QObject* target = uiContext.isNull()
            ? static_cast<QObject*>(QCoreApplication::instance())
            : uiContext.data();
        self->invokeOnUiThread(target, [self, result, savedPath, title, dialogParent, token]() {
            if (self.isNull() || self->deps_.backgroundTasks == nullptr) {
                return;
            }
            if (!self->deps_.backgroundTasks->isCurrent(token)) {
                return;
            }

            self->deps_.backgroundTasks->finish(token);
            if (self->deps_.feedback == nullptr || isShuttingDown(self->deps_)) {
                return;
            }

            self->deps_.feedback->recordOperationResult(result);
            QWidget* messageParent = self->resolveMessageParent(dialogParent);
            if (!result.success) {
                self->deps_.feedback->showOperationMessage(title, result, messageParent);
                return;
            }

            if (!savedPath.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
                const QString message = AppUpdateCheckPresentation::buildDownloadedPackageMessage(savedPath, result);
                self->deps_.feedback->showInformation(
                    messageParent,
                    QCoreApplication::translate("AppBootstrap", "Update Downloaded"),
                    message);
                return;
            }

            self->deps_.feedback->promptRestartForDownloadedAppUpdate(savedPath, messageParent);
        });
    });
}

void AppUpdateCheckCoordinator::downloadLatestAvailableUpdate(QPointer<QWidget> dialogParent)
{
    if (!latestAvailableUpdate_.updateAvailable) {
        return;
    }

    if (!latestAvailableUpdate_.assetDownloadUrl.isValid()) {
        if (deps_.feedback != nullptr && latestAvailableUpdate_.releaseUrl.isValid()) {
            deps_.feedback->openExternalUrl(latestAvailableUpdate_.releaseUrl.toString());
        }
        return;
    }

    downloadAppUpdate(latestAvailableUpdate_, dialogParent);
}

void AppUpdateCheckCoordinator::runInBackground(std::function<void()> task)
{
    if (deps_.runInBackground) {
        deps_.runInBackground(std::move(task));
        return;
    }

    QThread* thread = QThread::create([task = std::move(task)]() mutable {
        task();
    });
    if (deps_.trackBackgroundThread) {
        deps_.trackBackgroundThread(thread);
    } else {
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }
    thread->start();
}

void AppUpdateCheckCoordinator::invokeOnUiThread(QObject* context, std::function<void()> callback)
{
    if (deps_.invokeOnUiThread) {
        deps_.invokeOnUiThread(context, std::move(callback));
        return;
    }

    QObject* target = context == nullptr
        ? static_cast<QObject*>(QCoreApplication::instance())
        : context;
    if (target == nullptr || target->thread() == QThread::currentThread()) {
        callback();
        return;
    }

    QMetaObject::invokeMethod(target, [callback = std::move(callback)]() mutable {
        callback();
    }, Qt::QueuedConnection);
}

QWidget* AppUpdateCheckCoordinator::resolveMessageParent(const QPointer<QWidget>& dialogParent) const
{
    if (!dialogParent.isNull()) {
        return dialogParent.data();
    }
    if (deps_.feedback == nullptr) {
        return nullptr;
    }
    return deps_.feedback->dialogParent();
}
