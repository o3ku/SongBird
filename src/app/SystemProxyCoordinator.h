#pragma once

#include <functional>

#include <QString>

#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"
#include "common/SystemProxySnapshot.h"
#include "domain/models/Config.h"

class ISystemProxyService;
class ProxySession;
class ServerService;

class SystemProxyCoordinator final {
public:
    struct Dependencies {
        Config& config;
        ServerService& serverService;
        ISystemProxyService* systemProxyService = nullptr;
        ProxySession* proxySession = nullptr;
    };

    struct Callbacks {
        std::function<void(const OperationResult&)> appendResult;
        std::function<void()> syncStatusIndicators;
        std::function<void(bool, bool)> startManagedProxyCore;
        // Persists config_ (including the spent system-proxy snapshot) after a restore. Optional:
        // the exit path already saves on its way out, and a test does not need a repository.
        std::function<void()> persistConfig;
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
    // Captures the user's own proxy settings the first time this session writes to the registry.
    void captureSnapshotIfNeeded();
    // Puts the captured snapshot back and marks it spent. False when there is no snapshot to
    // restore or the write failed, which is what tells applyModeOnExit() to fall back to clearing.
    bool restoreCapturedSnapshot();

    Dependencies deps_;
    Callbacks callbacks_;
    bool exitProxyStateApplied_ = false;
};
