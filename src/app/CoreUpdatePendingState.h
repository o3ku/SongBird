#pragma once

#include <functional>

#include <QPointer>
#include <QString>

#include "app/BackgroundTaskCoordinator.h"
#include "common/OperationResult.h"
#include "domain/models/VmessItem.h"

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
        std::function<void(const OperationResult&)> completionObserver,
        BackgroundTaskCoordinator::Token taskToken);
    Snapshot take();
    void clear();
    void clearIfGeneration(int generation);

private:
    Snapshot snapshot_;
};
