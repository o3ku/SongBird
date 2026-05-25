#pragma once

#include <functional>
#include <optional>

#include "common/OperationResult.h"
#include "domain/models/Config.h"

class ServerService;
struct VmessItem;

class TunModeCoordinator final {
public:
    struct Dependencies {
        Config& config;
        ServerService& serverService;
    };

    struct Callbacks {
        std::function<bool()> isWindowsPlatform;
        std::function<bool()> isProcessElevated;
        std::function<bool()> isCoreRunning;
        std::function<std::optional<VmessItem>()> resolveActiveServer;
        std::function<bool()> askRestartAsAdministratorForTun;
        std::function<bool(bool)> restartApplication;
        std::function<void()> persistUiState;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void()> syncWindow;
        std::function<void()> syncStatusIndicators;
        std::function<void(const QString&, bool)> restartCoreIfRunning;
        std::function<void(bool)> enableSystemProxy;
    };

    TunModeCoordinator(Dependencies deps, Callbacks callbacks);

    void setEnabled(bool enabled);

private:
    bool isWindowsPlatform() const;
    bool isProcessElevated() const;
    bool isCoreRunning() const;
    bool hasActiveServer() const;
    void appendResult(const OperationResult& result) const;
    void syncWindow() const;
    void syncStatusIndicators() const;

    Dependencies deps_;
    Callbacks callbacks_;
};
