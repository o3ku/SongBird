#pragma once

#include <functional>

#include <QString>

#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"

class ProxySession;
class ServerService;
class WindowsSystemProxyService;

class SystemProxyCoordinator final {
public:
    struct Dependencies {
        Config& config;
        ServerService& serverService;
        WindowsSystemProxyService* systemProxyService = nullptr;
        ProxySession* proxySession = nullptr;
    };

    struct Callbacks {
        std::function<void(const OperationResult&)> appendResult;
        std::function<void()> syncStatusIndicators;
        std::function<void(bool, bool)> startManagedProxyCore;
    };

    SystemProxyCoordinator(Dependencies deps, Callbacks callbacks);

    bool saveMode(SystemProxyMode mode);
    void setMode(
        SystemProxyMode mode,
        bool skipTunCleanup = false,
        bool immediateStop = false,
        bool showStartupOverlay = false);
    void clearStateAfterCoreStopped();
    void applyModeOnExit(bool windowsShutdown);
    void cleanupRuntimeForExit(bool windowsShutdown);
    void enable(bool showStartupOverlay = false);
    void retryStartup(bool showStartupOverlay = true);
    void disable();
    bool updateMode(SystemProxyMode mode) const;
    QString buildExceptions() const;

private:
    void appendResult(const OperationResult& result) const;
    void syncStatusIndicators() const;
    void startManagedProxyCore(bool skipTunCleanup, bool showStartupOverlay) const;

    Dependencies deps_;
    Callbacks callbacks_;
    bool exitProxyStateApplied_ = false;
};
