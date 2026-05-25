#pragma once

#include <functional>

#include <QString>
#include <QStringList>

#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"

class ServerService;
class WindowsAutoRunService;

class SettingsApplyCoordinator final {
public:
    struct Dependencies {
        Config& config;
        ServerService& serverService;
        WindowsAutoRunService* autoRunService = nullptr;
    };

    struct PlatformCallbacks {
        std::function<bool()> isWindowsPlatform;
        std::function<bool()> isProcessElevated;
    };

    struct RuntimeCallbacks {
        std::function<bool()> isCoreRunning;
        std::function<bool()> isCoreReady;
        std::function<void(const QString&, bool)> restartCoreIfRunning;
    };

    struct SystemProxyCallbacks {
        std::function<bool()> hasService;
        std::function<bool(SystemProxyMode)> updateMode;
        std::function<void(bool)> adoptManagedProxy;
    };

    struct WorkflowCallbacks {
        std::function<bool()> promptRestartAsAdministratorForTun;
        std::function<void()> promptRestartForLanguageChange;
        std::function<void(const QStringList&, bool, const QString&)> updateSubscriptionsByIds;
    };

    struct UiCallbacks {
        std::function<void(const QString&)> selectSubscriptionTab;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void()> syncWindow;
        std::function<void()> syncStatusIndicators;
    };

    struct Callbacks {
        PlatformCallbacks platform;
        RuntimeCallbacks runtime;
        SystemProxyCallbacks systemProxy;
        WorkflowCallbacks workflow;
        UiCallbacks ui;
    };

    SettingsApplyCoordinator(Dependencies deps, Callbacks callbacks);

    void apply(const Config& updatedConfig);

private:
    bool isWindowsPlatform() const;
    bool isProcessElevated() const;
    bool isCoreRunning() const;
    bool isCoreReady() const;
    bool hasSystemProxyService() const;
    void appendResult(const OperationResult& result) const;
    void syncWindow() const;
    void syncStatusIndicators() const;

    Dependencies deps_;
    Callbacks callbacks_;
};
