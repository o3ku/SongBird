#pragma once

#include <functional>
#include <memory>

#include <QObject>
#include <QList>
#include <QPointer>

#include "app/SettingsDialogRunner.h"
#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "services/CoreUpdateService.h"

class QThread;
class SettingsDialog;
class QWidget;

class SettingsWorkflowCoordinator final : public QObject
{
    Q_OBJECT

public:
    struct Dependencies {
        std::function<QWidget*()> dialogParent;
        std::function<bool()> isAvailable;
        std::function<Config()> currentConfig;
        std::function<QList<CoreType>()> existingCoreTypes;
        std::function<std::weak_ptr<char>()> lifetimeGuard;
        std::function<void()> refreshExistingCoreTypes;
        std::function<void(QThread*)> trackBackgroundThread;
        std::function<QString(CoreType)> detectCoreVersion;
        std::function<void(
            CoreType,
            QObject*,
            QWidget*,
            const std::function<void(const QString&)>&,
            const CoreUpdateService::DownloadProgressHandler&,
            const std::function<void(const OperationResult&)>&)> updateCore;
        std::function<void()> restoreConfigFromBackup;
        std::function<void(const Config&)> applySettings;
        std::function<OperationResult(const QStringList&, const Config&)> saveAndUpdateSubscriptions;
        // Asks the in-flight core update to stop.
        std::function<void()> cancelCoreUpdate;
    };

    explicit SettingsWorkflowCoordinator(Dependencies dependencies, QObject* parent = nullptr);

    void openSettingsDialog(int initialTab);

private:
    void handleCoreDownload(CoreType requestedCoreType, QPointer<SettingsDialog> dialogGuard);

    Dependencies deps_;
};
