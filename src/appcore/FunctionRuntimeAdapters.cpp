#include "appcore/FunctionRuntimeAdapters.h"

void FunctionRuntimeEnvironment::cleanupPortProcesses()
{
    cleanupPortProcessesFn();
}

OperationResult FunctionRuntimeEnvironment::removeStaleTunAdapter()
{
    return removeStaleTunAdapterFn();
}

bool FunctionRuntimeEnvironment::skipCoreChecks() const
{
    return skipCoreChecksFn();
}

bool FunctionRuntimeEnvironment::isWindowsPlatform() const
{
    return isWindowsPlatformFn();
}

bool FunctionRuntimeEnvironment::isProcessElevated() const
{
    return isProcessElevatedFn();
}

void FunctionProxyActivationCoordinator::cancelBackgroundTasksForStartup()
{
    cancelBackgroundTasksForStartupFn();
}

void FunctionProxyActivationCoordinator::refreshExistingCoreTypes()
{
    refreshExistingCoreTypesFn();
}

bool FunctionProxyActivationCoordinator::isSystemProxyEnabled() const
{
    return isSystemProxyEnabledFn();
}

bool FunctionProxyActivationCoordinator::updateSystemProxyMode(SystemProxyMode mode)
{
    return updateSystemProxyModeFn(mode);
}

SystemProxySnapshot FunctionProxyActivationCoordinator::capturedSystemProxySnapshot() const
{
    return capturedSystemProxySnapshotFn ? capturedSystemProxySnapshotFn() : SystemProxySnapshot{};
}

SystemProxySnapshot FunctionProxyActivationCoordinator::ensureSystemProxySnapshotCaptured()
{
    // The host owns both halves: it is the only thing that can read the registry and the only
    // thing that can remember what it read. Falling back to the plain getter keeps a host that
    // implements just one of the two working.
    if (ensureSystemProxySnapshotCapturedFn) {
        return ensureSystemProxySnapshotCapturedFn();
    }
    return capturedSystemProxySnapshot();
}

bool FunctionProxyActivationCoordinator::restoreSystemProxySnapshot(
    const SystemProxySnapshot& snapshot)
{
    // An unset restore hook means the host has no system proxy to put back, so reporting success
    // is honest -- the same convention ISystemProxyService::restore uses for an invalid snapshot.
    return restoreSystemProxySnapshotFn ? restoreSystemProxySnapshotFn(snapshot) : true;
}

QObject* FunctionUserFeedback::uiContext() const
{
    return uiContextFn ? uiContextFn() : nullptr;
}

QWidget* FunctionUserFeedback::dialogParent() const
{
    return dialogParentFn ? dialogParentFn() : nullptr;
}

void FunctionUserFeedback::appendLog(const QString& message)
{
    if (appendLogFn) {
        appendLogFn(message);
    }
}

void FunctionUserFeedback::recordOperationResult(const OperationResult& result)
{
    if (recordOperationResultFn) {
        recordOperationResultFn(result);
    }
}

void FunctionUserFeedback::showOperationMessage(const QString& title, const OperationResult& result, QWidget* parent)
{
    if (showOperationMessageFn) {
        showOperationMessageFn(title, result, parent);
    }
}

bool FunctionUserFeedback::askYesNo(
    QWidget* parent,
    const QString& title,
    const QString& text,
    YesNoDefault defaultButton)
{
    return askYesNoFn && askYesNoFn(parent, title, text, defaultButton);
}

void FunctionUserFeedback::showInformation(QWidget* parent, const QString& title, const QString& text)
{
    if (showInformationFn) {
        showInformationFn(parent, title, text);
    }
}

void FunctionUserFeedback::showTrayMessage(const QString& title, const QString& message, bool critical, int timeoutMs)
{
    if (showTrayMessageFn) {
        showTrayMessageFn(title, message, critical, timeoutMs);
    }
}

void FunctionUserFeedback::openExternalUrl(const QString& url)
{
    if (openExternalUrlFn) {
        openExternalUrlFn(url);
    }
}

bool FunctionUserFeedback::promptRestartForDownloadedAppUpdate(const QString& newExecutablePath, QWidget* parent)
{
    return promptRestartForDownloadedAppUpdateFn
        && promptRestartForDownloadedAppUpdateFn(newExecutablePath, parent);
}
