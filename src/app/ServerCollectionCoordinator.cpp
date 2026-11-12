#include "app/ServerCollectionCoordinator.h"

#include <QCoreApplication>
#include <utility>

#include "appcore/LocationCatalog.h"
#include "appcore/LocationGroupScan.h"
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
    // A removal that takes a member out of the running group reloads the core just like removing
    // the anchor: the core's auto-select list is built from the group's members, so a non-anchor
    // member the user deleted must leave that list too.
    const bool groupMemberRemoved = removalTouchesRunningGroup(indexIds);

    const ServerService::RemovalOutcome removal = deps_.serverService.removeServers(deps_.config, indexIds);
    appendResult(removal.result);
    syncWindow();
    // Gate the core work on `applied`, not on `removal.result.success`: a cleanup failure makes the
    // result a failure while the servers are gone from the saved config all the same, and returning
    // early there would leave the core running the server the user just removed.
    if (!removal.applied) {
        return;
    }

    if (stopCoreIfGroupTargetEmptied()) {
        return;
    }

    if (!isCoreRunning()) {
        return;
    }

    if (!activeServerRemoved && !groupMemberRemoved) {
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
        // A location edit can take the last member out of the running group -- the same stale
        // target a removal leaves -- so the emptied-target recovery runs here too (see
        // removeServers). Membership that merely changed shape stays untouched: the core keeps
        // its embedded member list until the next restart, by the same rule as a location
        // change while a single node runs.
        stopCoreIfGroupTargetEmptied();
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
        // Unfavoriting the last favorite empties a running FAVORITE group target, exactly like a
        // removal does; the same recovery clears the target and stops the core.
        stopCoreIfGroupTargetEmptied();
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

    // The subscription's nodes leave the config with it, so collect their ids first: any of them
    // in the running group means the core's auto-select list has to be rebuilt.
    QStringList subscriptionServerIds;
    for (const VmessItem& server : deps_.config.collection().servers) {
        if (server.subId.trimmed() == normalizedId) {
            subscriptionServerIds.append(server.indexId);
        }
    }
    const bool groupMemberRemoved = removalTouchesRunningGroup(subscriptionServerIds);

    const OperationResult result = deps_.subscriptionService.removeSubscription(deps_.config, normalizedId);
    appendResult(result);
    syncWindow();
    if (!result.success) {
        return;
    }

    // Same as removeServers(): clear an emptied group target regardless of the core, stop only when
    // one is running.
    if (stopCoreIfGroupTargetEmptied()) {
        return;
    }

    if (!isCoreRunning()) {
        return;
    }

    if (!activeSubscriptionRemoved && !groupMemberRemoved) {
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

bool ServerCollectionCoordinator::removalTouchesRunningGroup(const QStringList& indexIds) const
{
    const QString groupId = deps_.config.ui().activeLocationGroupId.trimmed();
    if (groupId.isEmpty()) {
        return false;
    }
    const QList<VmessItem> members =
        LocationGroupScan::embeddableMembers(deps_.config.collection().servers, groupId);
    for (const VmessItem& member : members) {
        const QString memberId = member.indexId.trimmed();
        for (const QString& removedId : indexIds) {
            if (removedId.trimmed() == memberId) {
                return true;
            }
        }
    }
    return false;
}

// The emptied-target recovery shared by every path that can take the last member out of the
// running group: clear the persisted target (it can never start again as-is) and stop a running
// core, because its config embeds a member set that no longer exists.
bool ServerCollectionCoordinator::stopCoreIfGroupTargetEmptied()
{
    if (!clearRunningGroupIfEmptied()) {
        return false;
    }
    if (isCoreRunning()) {
        appendResult(OperationResult::ok(QCoreApplication::translate(
            "AppBootstrap", "Stopping core because the active location group is now empty.")));
        stopCore();
    }
    return true;
}

bool ServerCollectionCoordinator::clearRunningGroupIfEmptied()
{
    const QString groupId = deps_.config.ui().activeLocationGroupId.trimmed();
    if (groupId.isEmpty()) {
        return false;
    }
    if (!LocationGroupScan::embeddableMembers(deps_.config.collection().servers, groupId).isEmpty()) {
        return false;
    }
    deps_.config.ui().activeLocationGroupId.clear();
    appendResult(deps_.serverService.save(deps_.config));
    return true;
}
