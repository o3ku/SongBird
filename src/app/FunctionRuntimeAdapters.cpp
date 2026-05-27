#include "app/FunctionRuntimeAdapters.h"

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
