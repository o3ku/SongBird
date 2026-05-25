#pragma once

#include <functional>
#include <optional>

#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

class ServerService;
class QWidget;

class ServerEditorCoordinator final {
public:
    struct Dependencies {
        Config& config;
        ServerService& serverService;
    };

    struct Callbacks {
        std::function<QWidget*()> dialogParent;
        std::function<std::optional<VmessItem>(const QString&)> findServer;
        std::function<std::optional<VmessItem>()> resolveActiveServer;
        std::function<bool()> isCoreRunning;
        std::function<void(const QString&)> appendLog;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void()> syncWindow;
        std::function<void(const QString&, bool)> restartCoreIfRunning;
    };

    ServerEditorCoordinator(Dependencies deps, Callbacks callbacks);

    void addServer();
    void editServer(const QString& indexId);

private:
    QWidget* dialogParent() const;
    void appendLog(const QString& message) const;
    void appendResult(const OperationResult& result) const;
    void syncWindow() const;
    bool isCoreRunning() const;
    std::optional<VmessItem> findServer(const QString& indexId) const;
    std::optional<VmessItem> resolveActiveServer() const;
    void restartCoreIfRunning(const QString& reason) const;

    Dependencies deps_;
    Callbacks callbacks_;
};
