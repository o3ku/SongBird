#pragma once

#include <functional>
#include <memory>

#include <QObject>
#include <QList>
#include <QPointer>

#include "app/SettingsDialogSession.h"
#include "common/OperationResult.h"
#include "domain/models/Config.h"

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
            const std::function<void(const OperationResult&)>&)> updateCore;
        std::function<void()> restoreConfigFromBackup;
        std::function<void(const Config&)> applySettings;
        std::function<OperationResult(const QList<int>&, const Config&)> saveAndUpdateSubscriptions;
    };

    explicit SettingsWorkflowCoordinator(Dependencies dependencies, QObject* parent = nullptr);

    void openSettingsDialog(int initialTab);

private:
    void handleCoreDownload(CoreType requestedCoreType, QPointer<SettingsDialog> dialogGuard);

    Dependencies deps_;
};
