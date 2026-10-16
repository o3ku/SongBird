#pragma once

#include <functional>

#include <QPointer>
#include <QString>

#include "appcore/BackgroundTaskCoordinator.h"
#include "common/OperationResult.h"
#include "domain/models/VmessItem.h"
#include "services/CoreUpdateService.h"

class QObject;
class QWidget;

class CoreUpdatePendingState final {
public:
    struct Snapshot {
        CoreType coreType = CoreType::Unknown;
        bool startAfterSuccess = false;
        bool skipLocalVersionCheck = false;
        QPointer<QObject> progressContext;
        QPointer<QWidget> dialogParent;
        std::function<void(const QString&)> progressObserver;
        CoreUpdateService::DownloadProgressHandler downloadProgressObserver;
        std::function<void(const OperationResult&)> completionObserver;
        BackgroundTaskCoordinator::Token taskToken;
    };

    bool isValid() const;
    void store(
        CoreType coreType,
        bool startAfterSuccess,
        bool skipLocalVersionCheck,
        QPointer<QObject> progressContext,
        QPointer<QWidget> dialogParent,
        std::function<void(const QString&)> progressObserver,
        CoreUpdateService::DownloadProgressHandler downloadProgressObserver,
        std::function<void(const OperationResult&)> completionObserver,
        BackgroundTaskCoordinator::Token taskToken);
    Snapshot take();
    void clear();
    void clearIfGeneration(int generation);

private:
    Snapshot snapshot_;
};
