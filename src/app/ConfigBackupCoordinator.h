#pragma once

#include <functional>

#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"

class ConfigBackupService;
class QWidget;

class ConfigBackupCoordinator final
{
public:
    struct Dependencies {
        std::function<QWidget*()> dialogParent;
        std::function<QString()> configPath;
        std::function<Config()> currentConfig;
        std::function<void(const OperationResult&)> appendResult;
        std::function<bool()> isCoreRunning;
        std::function<void()> stopCoreImmediately;
        std::function<void()> markUiStateNeedsRestore;
        std::function<bool()> reloadConfig;
        std::function<bool()> hasActiveServer;
        std::function<void()> resumeProxy;
        std::function<void()> clearProxyStateAfterCoreStopped;
        std::function<void()> syncStatusIndicators;
    };

    ConfigBackupCoordinator(ConfigBackupService& backupService, Dependencies dependencies);

    void autoBackupCurrentConfig();
    void restoreConfigFromBackup();

private:
    void appendResult(const OperationResult& result) const;
    QString selectBackupPath(QWidget* parent) const;
    OperationResult validateBackupFile(const QString& backupPath) const;
    QString restoreDialogInitialDirectory() const;

    ConfigBackupService& backupService_;
    Dependencies deps_;
};
