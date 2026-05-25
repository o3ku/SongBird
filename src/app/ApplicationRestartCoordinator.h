#pragma once

#include <functional>

#include <QString>

class AppUpdateInstallService;
class QWidget;
struct OperationResult;

class ApplicationRestartCoordinator final {
public:
    struct Dependencies {
        AppUpdateInstallService* appUpdateInstallService = nullptr;
    };

    struct Callbacks {
        std::function<QWidget*()> dialogParent;
        std::function<bool()> isWindowsPlatform;
        std::function<bool()> isProcessElevated;
        std::function<bool()> tunEnabled;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void()> persistUiState;
        std::function<void(bool)> cleanupRuntimeForExit;
        std::function<void(bool)> setMainWindowAllowClose;
        std::function<void(bool)> setShutdownUiStatePersisted;
    };

    ApplicationRestartCoordinator(Dependencies deps, Callbacks callbacks);

    bool askRestartAsAdministratorForTun() const;
    bool promptRestartAsAdministratorForTun();
    void promptRestartForLanguageChange();
    bool promptRestartForDownloadedAppUpdate(const QString& newExecutablePath, QWidget* parent);
    bool restartApplication(bool requireAdministrator);

private:
    QWidget* dialogParent() const;
    bool isWindowsPlatform() const;
    bool isProcessElevated() const;
    bool tunEnabled() const;
    void appendResult(const OperationResult& result) const;
    void persistUiState() const;
    void cleanupRuntimeForExit(bool windowsShutdown) const;
    void setMainWindowAllowClose(bool allowClose) const;
    void setShutdownUiStatePersisted(bool persisted) const;

    Dependencies deps_;
    Callbacks callbacks_;
};
