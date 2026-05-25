#include "app/CoreUpdatePendingState.h"

#include <utility>

bool CoreUpdatePendingState::isValid() const
{
    return snapshot_.taskToken.isValid();
}

void CoreUpdatePendingState::store(
    CoreType coreType,
    bool startAfterSuccess,
    bool skipLocalVersionCheck,
    QPointer<QObject> progressContext,
    QPointer<QWidget> dialogParent,
    std::function<void(const QString&)> progressObserver,
    std::function<void(const OperationResult&)> completionObserver,
    BackgroundTaskCoordinator::Token taskToken)
{
    snapshot_.coreType = coreType;
    snapshot_.startAfterSuccess = startAfterSuccess;
    snapshot_.skipLocalVersionCheck = skipLocalVersionCheck;
    snapshot_.progressContext = progressContext;
    snapshot_.dialogParent = dialogParent;
    snapshot_.progressObserver = std::move(progressObserver);
    snapshot_.completionObserver = std::move(completionObserver);
    snapshot_.taskToken = taskToken;
}

CoreUpdatePendingState::Snapshot CoreUpdatePendingState::take()
{
    Snapshot snapshot = std::move(snapshot_);
    clear();
    return snapshot;
}

void CoreUpdatePendingState::clear()
{
    snapshot_ = {};
}

void CoreUpdatePendingState::clearIfGeneration(int generation)
{
    if (snapshot_.taskToken.generation == generation) {
        clear();
    }
}
