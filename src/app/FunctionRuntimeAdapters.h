#pragma once

#include <functional>
#include <optional>

#include <QObject>
#include <QString>
#include <QStringList>

#include "app/IUserFeedback.h"
#include "app/ProxyRuntimeInterfaces.h"
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"

class FunctionRuntimeEnvironment final : public IRuntimeEnvironment
{
public:
    std::function<void()> cleanupPortProcessesFn;
    std::function<OperationResult()> removeStaleTunAdapterFn;
    std::function<bool()> skipCoreChecksFn;
    std::function<bool()> isWindowsPlatformFn;
    std::function<bool()> isProcessElevatedFn;

    void cleanupPortProcesses() override;
    OperationResult removeStaleTunAdapter() override;
    bool skipCoreChecks() const override;
    bool isWindowsPlatform() const override;
    bool isProcessElevated() const override;
};

class FunctionProxyActivationCoordinator final : public IProxyActivationCoordinator
{
public:
    std::function<void()> cancelBackgroundTasksForStartupFn;
    std::function<void()> refreshExistingCoreTypesFn;
    std::function<bool()> isSystemProxyEnabledFn;
    std::function<bool(SystemProxyMode)> updateSystemProxyModeFn;

    void cancelBackgroundTasksForStartup() override;
    void refreshExistingCoreTypes() override;
    bool isSystemProxyEnabled() const override;
    bool updateSystemProxyMode(SystemProxyMode mode) override;
};

class FunctionUserFeedback final : public IUserFeedback
{
public:
    std::function<QObject*()> uiContextFn;
    std::function<QWidget*()> dialogParentFn;
    std::function<void(const QString&)> appendLogFn;
    std::function<void(const QString&, int, TransientStatusPriority)> showTransientStatusFn;
    std::function<void(const OperationResult&)> recordOperationResultFn;
    std::function<void(const QString&, const OperationResult&, QWidget*)> showOperationMessageFn;
    std::function<bool(QWidget*, const QString&, const QString&, YesNoDefault)> askYesNoFn;
    std::function<void(QWidget*, const QString&, const QString&)> showInformationFn;
    std::function<void(const QString&, const QString&, bool, int)> showTrayMessageFn;
    std::function<void(const QString&)> openExternalUrlFn;
    std::function<bool(const QString&, QWidget*)> promptRestartForDownloadedAppUpdateFn;

    QObject* uiContext() const override;
    QWidget* dialogParent() const override;
    void appendLog(const QString& message) override;
    void showTransientStatus(
        const QString& message,
        int timeoutMs,
        TransientStatusPriority priority = TransientStatusPriority::Routine) override;
    void recordOperationResult(const OperationResult& result) override;
    void showOperationMessage(const QString& title, const OperationResult& result, QWidget* parent) override;
    bool askYesNo(
        QWidget* parent,
        const QString& title,
        const QString& text,
        YesNoDefault defaultButton) override;
    void showInformation(QWidget* parent, const QString& title, const QString& text) override;
    void showTrayMessage(const QString& title, const QString& message, bool critical, int timeoutMs) override;
    void openExternalUrl(const QString& url) override;
    bool promptRestartForDownloadedAppUpdate(const QString& newExecutablePath, QWidget* parent) override;
};
