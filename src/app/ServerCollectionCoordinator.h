#pragma once

#include <functional>
#include <optional>

#include <QString>
#include <QStringList>

#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "services/ServerService.h"

class SubscriptionService;

class ServerCollectionCoordinator final {
public:
    struct Dependencies {
        Config& config;
        ServerService& serverService;
        SubscriptionService& subscriptionService;
    };

    struct Callbacks {
        std::function<std::optional<VmessItem>()> resolveActiveServer;
        std::function<bool()> isCoreRunning;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void()> syncWindow;
        std::function<void(bool)> stopCore;
        std::function<void(const QString&, bool)> restartCoreIfRunning;
    };

    ServerCollectionCoordinator(Dependencies deps, Callbacks callbacks);

    void removeServers(const QStringList& indexIds);
    void moveServers(const QStringList& indexIds, ServerMoveOperation operation);
    void reorderServers(const QStringList& orderedIndexIds);
    void hideSubscription(const QString& subscriptionId);
    void deleteSubscription(const QString& subscriptionId);

private:
    void appendResult(const OperationResult& result) const;
    void syncWindow() const;
    bool isCoreRunning() const;
    std::optional<VmessItem> resolveActiveServer() const;
    void stopCore() const;
    void restartCoreIfRunning(const QString& reason) const;

    Dependencies deps_;
    Callbacks callbacks_;
};
