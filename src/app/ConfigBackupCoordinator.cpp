#include "app/ConfigBackupCoordinator.h"

#include <utility>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QWidget>

#include "common/DialogUtils.h"
#include "persistence/JsonConfigRepository.h"
#include "services/ConfigBackupService.h"

ConfigBackupCoordinator::ConfigBackupCoordinator(
    ConfigBackupService& backupService,
    Dependencies dependencies)
    : backupService_(backupService)
    , deps_(std::move(dependencies))
{
}

void ConfigBackupCoordinator::autoBackupCurrentConfig()
{
    const OperationResult result = backupService_.backupCurrentConfig(
        deps_.currentConfig ? deps_.currentConfig() : Config());
    if (!result.success) {
        appendResult(result);
    }
}

void ConfigBackupCoordinator::restoreConfigFromBackup()
{
    QWidget* parent = deps_.dialogParent ? deps_.dialogParent() : nullptr;
    if (parent == nullptr) {
        return;
    }

    const QString backupPath = selectBackupPath(parent);
    if (backupPath.trimmed().isEmpty()) {
        return;
    }

    const OperationResult validationResult = validateBackupFile(backupPath);
    if (!validationResult.success) {
        appendResult(validationResult);
        DialogUtils::showWarning(
            parent,
            QCoreApplication::translate("AppBootstrap", "Restore GUI Config"),
            validationResult.message);
        return;
    }

    const bool wasCoreRunning = deps_.isCoreRunning && deps_.isCoreRunning();
    if (wasCoreRunning && deps_.stopCoreImmediately) {
        deps_.stopCoreImmediately();
    }

    const OperationResult backupResult = backupService_.backupCurrentConfig(
        deps_.currentConfig ? deps_.currentConfig() : Config());
    if (!backupResult.success) {
        appendResult(backupResult);
        return;
    }

    const OperationResult restoreResult = backupService_.restoreFromPath(backupPath);
    if (!restoreResult.success) {
        appendResult(restoreResult);
        return;
    }

    if (deps_.markUiStateNeedsRestore) {
        deps_.markUiStateNeedsRestore();
    }

    if (deps_.reloadConfig && !deps_.reloadConfig()) {
        return;
    }

    if (wasCoreRunning && deps_.hasActiveServer && deps_.hasActiveServer()) {
        if (deps_.resumeProxy) {
            deps_.resumeProxy();
        }
    } else {
        if (deps_.clearProxyStateAfterCoreStopped) {
            deps_.clearProxyStateAfterCoreStopped();
        }
        if (deps_.syncStatusIndicators) {
            deps_.syncStatusIndicators();
        }
    }

    appendResult(OperationResult::ok(
        QCoreApplication::translate("AppBootstrap", "Configuration restored from backup.")));
}

void ConfigBackupCoordinator::appendResult(const OperationResult& result) const
{
    if (deps_.appendResult) {
        deps_.appendResult(result);
    }
}

QString ConfigBackupCoordinator::selectBackupPath(QWidget* parent) const
{
    return DialogUtils::getOpenFileName(
        parent,
        QCoreApplication::translate("AppBootstrap", "Restore GUI Config"),
        restoreDialogInitialDirectory(),
        QCoreApplication::translate("AppBootstrap", "Config Files (*.json);;All Files (*.*)"));
}

OperationResult ConfigBackupCoordinator::validateBackupFile(const QString& backupPath) const
{
    JsonConfigRepository validationRepository(backupPath);
    validationRepository.load();
    const QString validationError = validationRepository.lastLoadError().trimmed();
    if (!validationError.isEmpty()) {
        return OperationResult::fail(validationError);
    }
    return OperationResult::ok({});
}

QString ConfigBackupCoordinator::restoreDialogInitialDirectory() const
{
    QString initialDirectory = backupService_.backupDirectoryPath();
    if (!initialDirectory.trimmed().isEmpty() && QDir(initialDirectory).exists()) {
        return initialDirectory;
    }

    const QString configPath = deps_.configPath ? deps_.configPath() : QString();
    return QFileInfo(configPath).dir().absolutePath();
}
