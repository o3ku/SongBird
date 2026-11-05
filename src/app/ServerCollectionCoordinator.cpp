#include "app/ServerCollectionCoordinator.h"

#include <QCoreApplication>
#include <utility>

#include "appcore/LocationCatalog.h"
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

    const ServerService::RemovalOutcome removal = deps_.serverService.removeServers(deps_.config, indexIds);
    appendResult(removal.result);
    syncWindow();
    // Gate the core work on `applied`, not on `removal.result.success`: a cleanup failure makes the
    // result a failure while the servers are gone from the saved config all the same, and returning
    // early there would leave the core running the server the user just removed.
    if (!removal.applied || !activeServerRemoved || !isCoreRunning()) {
        return;
    }

    if (!resolveActiveServer().has_value()) {
        appendResult(OperationResult::ok(QCoreApplication::translate("AppBootstrap", "Stopping core because the active server was removed.")));
        stopCore();
        return;
    }

    restartCoreIfRunning(QCoreApplication::translate(
        "AppBootstrap", "Reloading core after removing the active server."));
}

void ServerCollectionCoordinator::moveServers(const QStringList& indexIds, ServerMoveOperation operation)
{
    appendResult(deps_.serverService.moveServers(deps_.config, indexIds, operation));
    syncWindow();
}

void ServerCollectionCoordinator::moveSubscriptions(
    const QStringList& subscriptionIds,
    SubscriptionOrder::MoveOperation operation)
{
    // No core work here on purpose: the subscription order decides the tab order and the order
    // servers are listed in, never which node is active or how it is configured. Restarting the
    // core for it would drop a working connection to reorder a list.
    appendResult(deps_.subscriptionService.moveSubscriptions(deps_.config, subscriptionIds, operation));
    syncWindow();
}

void ServerCollectionCoordinator::reorderSubscriptions(const QStringList& orderedSubscriptionIds)
{
    // Same reasoning as moveSubscriptions(): the order is presentation, so a drag writes the list
    // and refreshes the window without touching the core.
    appendResult(deps_.subscriptionService.reorderSubscriptions(deps_.config, orderedSubscriptionIds));
    syncWindow();
}

void ServerCollectionCoordinator::reorderServers(const QStringList& orderedIndexIds)
{
    appendResult(deps_.serverService.reorderServers(deps_.config, orderedIndexIds));
    syncWindow();
}

void ServerCollectionCoordinator::setServersLocation(const QStringList& indexIds, const QString& locationId)
{
    // A location change is state, not configuration: no subscription, no server order, no core
    // restart -- the tabs and the table refresh from the same config sync every other edit uses.
    // An empty locationId is the reset entry ("Automatic (from name)"): the override is cleared
    // and the name inference takes over on the next read.
    const QString normalized = locationId.trimmed();
    const QString overrideValue = normalized == QLatin1String("FAVORITE")
        ? QString() // favorite is its own flag; a location of FAVORITE is not a thing
        : LocationCatalog::normalizedOverride(normalized);

    int changed = 0;
    for (const QString& indexId : indexIds) {
        const QString trimmedId = indexId.trimmed();
        for (VmessItem& server : deps_.config.collection().servers) {
            if (server.indexId != trimmedId) {
                continue;
            }
            if (normalized == QLatin1String("FAVORITE")) {
                if (!server.favorite) {
                    server.favorite = true;
                    ++changed;
                }
            } else if (server.locationOverride != overrideValue) {
                server.locationOverride = overrideValue;
                ++changed;
            }
            break;
        }
    }

    if (changed > 0) {
        appendResult(deps_.serverService.save(deps_.config));
        syncWindow();
    }
}

void ServerCollectionCoordinator::setServersFavorite(const QStringList& indexIds, bool favorite)
{
    int changed = 0;
    for (const QString& indexId : indexIds) {
        const QString trimmedId = indexId.trimmed();
        for (VmessItem& server : deps_.config.collection().servers) {
            if (server.indexId == trimmedId && server.favorite != favorite) {
                server.favorite = favorite;
                ++changed;
            }
        }
    }

    if (changed > 0) {
        appendResult(deps_.serverService.save(deps_.config));
        syncWindow();
    }
}

void ServerCollectionCoordinator::applyExitLocationToActiveServer(const QString& countryCode)
{
    const std::optional<VmessItem> activeServer = resolveActiveServer();
    if (!activeServer.has_value()) {
        return;
    }

    // The exit fix is a manual decision and writes the override, per the product call: it must
    // survive subscription refreshes (which it does, via the reuse path) and outrank the name.
    const QString locationId = LocationCatalog::locationIdForCountryCode(countryCode);
    if (locationId == QLatin1String("OTHER")) {
        // A node whose exit is a country with no item would be pushed into Other -- losing the
        // name-inferred grouping it already has for no gain. The user asked to fix the location;
        // Other is not a location.
        return;
    }

    setServersLocation({activeServer->indexId}, locationId);
}

void ServerCollectionCoordinator::hideSubscription(const QString& subscriptionId)
{
    // Hiding a subscription is the first half of "turn it off, reorder it, turn it on again" -- the
    // only way a hidden subscription can be moved, since it has no tab to drag. Saving here meant
    // those two clicks wrote the config file twice, and the intermediate write persisted a list the
    // user was still rearranging. The edit stays in memory and syncWindow() refreshes the tabs from
    // it; the save that ends the gesture is the one that decides what is on disk.
    appendResult(deps_.subscriptionService.setSubscriptionEnabled(
        deps_.config, subscriptionId, false, false));
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
        appendResult(OperationResult::ok(QCoreApplication::translate("AppBootstrap", "Stopping core because the active subscription was deleted.")));
        stopCore();
        return;
    }

    restartCoreIfRunning(QCoreApplication::translate(
        "AppBootstrap", "Reloading core after deleting the active subscription."));
}

void ServerCollectionCoordinator::persistPendingSubscriptionEdits()
{
    appendResult(deps_.serverService.save(deps_.config));
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
