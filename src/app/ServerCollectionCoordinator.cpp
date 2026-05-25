#include "app/ServerCollectionCoordinator.h"

#include <utility>

#include "services/SubscriptionService.h"

ServerCollectionCoordinator::ServerCollectionCoordinator(Dependencies deps, Callbacks callbacks)
    : deps_(deps)
    , callbacks_(std::move(callbacks))
{
}

void ServerCollectionCoordinator::removeServers(const QStringList& indexIds)
{
    const std::optional<VmessItem> activeServer = resolveActiveServer();
    const QString activeServerId = activeServer.has_value() ? activeServer->indexId : QString();
    const bool activeServerRemoved = !activeServerId.isEmpty() && indexIds.contains(activeServerId);

    const OperationResult result = deps_.serverService.removeServers(deps_.config, indexIds);
    appendResult(result);
    syncWindow();
    if (!result.success || !activeServerRemoved || !isCoreRunning()) {
        return;
    }

    if (!resolveActiveServer().has_value()) {
        appendResult(OperationResult::ok(QStringLiteral("Stopping core because the active server was removed.")));
        stopCore();
        return;
    }

    restartCoreIfRunning(QStringLiteral("Reloading core after removing the active server."));
}

void ServerCollectionCoordinator::moveServers(const QStringList& indexIds, ServerMoveOperation operation)
{
    appendResult(deps_.serverService.moveServers(deps_.config, indexIds, operation));
    syncWindow();
}

void ServerCollectionCoordinator::reorderServers(const QStringList& orderedIndexIds)
{
    appendResult(deps_.serverService.reorderServers(deps_.config, orderedIndexIds));
    syncWindow();
}

void ServerCollectionCoordinator::hideSubscription(const QString& subscriptionId)
{
    appendResult(deps_.subscriptionService.setSubscriptionEnabled(deps_.config, subscriptionId, false));
    syncWindow();
}

void ServerCollectionCoordinator::deleteSubscription(const QString& subscriptionId)
{
    const QString normalizedId = subscriptionId.trimmed();
    const std::optional<VmessItem> activeServer = resolveActiveServer();
    const QString activeSubscriptionId = activeServer.has_value() ? activeServer->subId.trimmed() : QString();
    const bool activeSubscriptionRemoved = !activeSubscriptionId.isEmpty() && activeSubscriptionId == normalizedId;

    const OperationResult result = deps_.subscriptionService.removeSubscription(deps_.config, normalizedId);
    appendResult(result);
    syncWindow();
    if (!result.success || !activeSubscriptionRemoved || !isCoreRunning()) {
        return;
    }

    if (!resolveActiveServer().has_value()) {
        appendResult(OperationResult::ok(QStringLiteral("Stopping core because the active subscription was deleted.")));
        stopCore();
        return;
    }

    restartCoreIfRunning(QStringLiteral("Reloading core after deleting the active subscription."));
}

void ServerCollectionCoordinator::appendResult(const OperationResult& result) const
{
    if (callbacks_.appendResult) {
        callbacks_.appendResult(result);
    }
}

void ServerCollectionCoordinator::syncWindow() const
{
    if (callbacks_.syncWindow) {
        callbacks_.syncWindow();
    }
}

bool ServerCollectionCoordinator::isCoreRunning() const
{
    return callbacks_.isCoreRunning ? callbacks_.isCoreRunning() : false;
}

std::optional<VmessItem> ServerCollectionCoordinator::resolveActiveServer() const
{
    return callbacks_.resolveActiveServer ? callbacks_.resolveActiveServer() : std::nullopt;
}

void ServerCollectionCoordinator::stopCore() const
{
    if (callbacks_.stopCore) {
        callbacks_.stopCore(false);
    }
}

void ServerCollectionCoordinator::restartCoreIfRunning(const QString& reason) const
{
    if (callbacks_.restartCoreIfRunning) {
        callbacks_.restartCoreIfRunning(reason, true);
    }
}
