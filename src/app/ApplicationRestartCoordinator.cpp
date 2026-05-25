#include "app/ApplicationRestartCoordinator.h"

#include <utility>

#include <QCoreApplication>
#include <QDir>
#include <QMessageBox>
#include <QProcess>

#include "app/AppUpdateInstallService.h"
#include "app/SingleInstanceBootstrap.h"
#include "app/StartupAdminElevation.h"
#include "app/TunAdminMessages.h"
#include "common/DialogUtils.h"
#include "common/OperationResult.h"

namespace {

QString tunAdminRestartPromptTitle()
{
    return QCoreApplication::translate("AppBootstrap", "Administrator Permission");
}

QString tunAdminRestartPromptMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "TUN mode requires administrator privileges on Windows.\nRestart SongBird as administrator now to apply the saved TUN setting?");
}

} // namespace

ApplicationRestartCoordinator::ApplicationRestartCoordinator(Dependencies deps, Callbacks callbacks)
    : deps_(deps)
    , callbacks_(std::move(callbacks))
{
}

bool ApplicationRestartCoordinator::askRestartAsAdministratorForTun() const
{
    QWidget* parent = dialogParent();
    if (parent == nullptr || !isWindowsPlatform() || isProcessElevated()) {
        return false;
    }

    return DialogUtils::askYesNoQuestion(
            parent,
            tunAdminRestartPromptTitle(),
            tunAdminRestartPromptMessage(),
            QMessageBox::Yes)
        == QMessageBox::Yes;
}

bool ApplicationRestartCoordinator::promptRestartAsAdministratorForTun()
{
    if (!askRestartAsAdministratorForTun()) {
        return false;
    }

    persistUiState();
    if (!restartApplication(true)) {
        appendResult(OperationResult::fail(tunAdminRestartFailureMessage()));
        return false;
    }

    return true;
}

void ApplicationRestartCoordinator::promptRestartForLanguageChange()
{
    QWidget* parent = dialogParent();
    if (parent == nullptr) {
        return;
    }

    const auto title = QCoreApplication::translate("AppBootstrap", "Restart Required");
    const auto message =
        QCoreApplication::translate("AppBootstrap", "Language changes take effect after restart. Restart now?");
    if (DialogUtils::askYesNoQuestion(parent, title, message, QMessageBox::Yes) != QMessageBox::Yes) {
        return;
    }

    persistUiState();
    const bool requiresAdminRestart = isWindowsPlatform() && !isProcessElevated() && tunEnabled();
    if (!restartApplication(requiresAdminRestart)) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to restart the application.")));
    }
}

bool ApplicationRestartCoordinator::promptRestartForDownloadedAppUpdate(
    const QString& newExecutablePath,
    QWidget* parent)
{
    if (newExecutablePath.trimmed().isEmpty()) {
        return false;
    }

    const QString message = QCoreApplication::translate(
        "AppBootstrap",
        "Downloaded update package: %1\n\nRestart SongBird now and apply the update automatically?")
            .arg(QDir::toNativeSeparators(newExecutablePath));
    if (DialogUtils::askYesNoQuestion(
            parent,
            QCoreApplication::translate("AppBootstrap", "Update Downloaded"),
            message,
            QMessageBox::Yes)
        != QMessageBox::Yes) {
        return false;
    }

    if (deps_.appUpdateInstallService == nullptr
        || !deps_.appUpdateInstallService->launchUpdateScript(newExecutablePath)) {
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to start the update script.")));
        DialogUtils::showWarning(
            parent,
            QCoreApplication::translate("AppBootstrap", "Update Failed"),
            QCoreApplication::translate("AppBootstrap", "Failed to start the update script."));
        return false;
    }

    setShutdownUiStatePersisted(true);
    cleanupRuntimeForExit(false);
    SingleInstanceBootstrap::releaseCurrentInstance();
    setMainWindowAllowClose(true);
    QCoreApplication::quit();
    return true;
}

bool ApplicationRestartCoordinator::restartApplication(bool requireAdministrator)
{
    const QStringList arguments = startupRelaunchArgumentsForRunningInstance(
        QCoreApplication::arguments(),
        requireAdministrator,
        QCoreApplication::applicationPid());

    SingleInstanceBootstrap::releaseCurrentInstance();

    bool restarted = false;
    if (requireAdministrator) {
        restarted = restartProcessAsAdministrator(QCoreApplication::applicationFilePath(), arguments);
    } else {
        restarted = QProcess::startDetached(QCoreApplication::applicationFilePath(), arguments);
    }

    if (!restarted) {
        SingleInstanceBootstrap::reacquireCurrentInstance();
        return false;
    }

    setShutdownUiStatePersisted(true);
    cleanupRuntimeForExit(false);
    setMainWindowAllowClose(true);
    QCoreApplication::quit();
    return true;
}

QWidget* ApplicationRestartCoordinator::dialogParent() const
{
    return callbacks_.dialogParent ? callbacks_.dialogParent() : nullptr;
}

bool ApplicationRestartCoordinator::isWindowsPlatform() const
{
    return callbacks_.isWindowsPlatform ? callbacks_.isWindowsPlatform() : false;
}

bool ApplicationRestartCoordinator::isProcessElevated() const
{
    return callbacks_.isProcessElevated ? callbacks_.isProcessElevated() : false;
}

bool ApplicationRestartCoordinator::tunEnabled() const
{
    return callbacks_.tunEnabled ? callbacks_.tunEnabled() : false;
}

void ApplicationRestartCoordinator::appendResult(const OperationResult& result) const
{
    if (callbacks_.appendResult) {
        callbacks_.appendResult(result);
    }
}

void ApplicationRestartCoordinator::persistUiState() const
{
    if (callbacks_.persistUiState) {
        callbacks_.persistUiState();
    }
}

void ApplicationRestartCoordinator::cleanupRuntimeForExit(bool windowsShutdown) const
{
    if (callbacks_.cleanupRuntimeForExit) {
        callbacks_.cleanupRuntimeForExit(windowsShutdown);
    }
}

void ApplicationRestartCoordinator::setMainWindowAllowClose(bool allowClose) const
{
    if (callbacks_.setMainWindowAllowClose) {
        callbacks_.setMainWindowAllowClose(allowClose);
    }
}

void ApplicationRestartCoordinator::setShutdownUiStatePersisted(bool persisted) const
{
    if (callbacks_.setShutdownUiStatePersisted) {
        callbacks_.setShutdownUiStatePersisted(persisted);
    }
}
