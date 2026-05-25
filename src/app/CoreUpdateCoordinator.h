#pragma once

#include <functional>
#include <memory>

#include <QObject>
#include <QPointer>
#include <QString>

#include "app/BackgroundTaskCoordinator.h"
#include "app/CoreUpdatePendingState.h"
#include "common/OperationResult.h"
#include "domain/models/VmessItem.h"
#include "services/CoreUpdateService.h"

class QThread;
class QWidget;

class CoreUpdateCoordinator : public QObject {
    Q_OBJECT

public:
    struct Request {
        int coreTypeValue = static_cast<int>(CoreType::Unknown);
        bool startAfterSuccess = false;
        QObject* progressContext = nullptr;
        QWidget* dialogParent = nullptr;
        bool skipConfirmation = false;
        bool skipLocalVersionCheck = false;
        std::function<void(const QString&)> progressObserver;
        std::function<void(const OperationResult&)> completionObserver;
    };

    struct Dependencies {
        BackgroundTaskCoordinator* backgroundTasks = nullptr;
        std::weak_ptr<char> lifetimeGuard;

        std::function<bool()> isShuttingDown;
        std::function<QObject*()> defaultProgressContext;
        std::function<QWidget*()> defaultDialogParent;
        std::function<QObject*()> uiContext;

        std::function<CoreType(CoreType)> resolveRuntimeCoreType;
        std::function<QString(CoreType)> resolveCoreInstallDirectory;
        std::function<CoreUpdateConfig()> makeWorkerConfig;
        std::function<bool(CoreType)> shouldStopRunningCoreForUpdate;

        std::function<bool(QWidget*, const QString&, const QString&)> confirmUpdate;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void(const QString&)> appendLog;
        std::function<void(const QString&, int)> showRoutineTransientStatus;
        std::function<void(const QString&, int)> showTransientStatus;
        std::function<void(const QString&, const OperationResult&, QWidget*)> showOperationMessage;

        std::function<void()> stopForCoreUpdate;
        std::function<void(bool)> enableSystemProxy;
        std::function<void()> clearProxyStateAfterCoreStopped;
        std::function<void()> syncStatusIndicators;
        std::function<void()> refreshExistingCoreTypes;
        std::function<void(QThread*)> trackBackgroundThread;

        std::function<OperationResult(
            CoreType,
            const CoreUpdateConfig&,
            const QString&,
            const CoreUpdateService::UpdateOptions&)>
            updateRunner;
        std::function<void(std::function<void()>)> startBackgroundTask;
    };

    explicit CoreUpdateCoordinator(Dependencies dependencies, QObject* parent = nullptr);

    void updateCore(const Request& request);
    void continuePendingCoreUpdate();

    bool hasPendingCoreUpdate() const;

private:
    void runCoreUpdateTask(
        BackgroundTaskCoordinator::Token token,
        CoreType coreType,
        CoreUpdateConfig workerConfig,
        QString installDirectory,
        QPointer<QObject> progressContextGuard,
        bool skipLocalVersionCheck,
        std::function<void(const QString&)> progressObserver,
        std::function<void(const OperationResult&)> completionObserver);
    void finalizeCoreUpdate(
        BackgroundTaskCoordinator::Token token,
        const QString& title,
        const OperationResult& result,
        bool stoppedForInstall,
        bool startAfterSuccess,
        QPointer<QWidget> dialogParentGuard,
        const std::function<void(const OperationResult&)>& completionObserver);
    void startCoreUpdateWorker(
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
        std::function<void(const OperationResult&)> completionObserver);

    QObject* fallbackUiContext() const;
    QWidget* fallbackDialogParent() const;
    bool isCurrent(const BackgroundTaskCoordinator::Token& token) const;
    bool isShuttingDown() const;
    std::weak_ptr<char> weakLifetimeGuard() const;

    Dependencies deps_;
    CoreUpdatePendingState pendingCoreUpdate_;
};
