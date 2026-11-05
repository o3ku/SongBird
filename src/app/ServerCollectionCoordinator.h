#pragma once

#include <functional>
#include <optional>

#include <QString>
#include <QStringList>

#include "appcore/LocationCatalog.h"
#include "common/OperationResult.h"
#include "common/SubscriptionOrder.h"
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
    void moveSubscriptions(const QStringList& subscriptionIds, SubscriptionOrder::MoveOperation operation);
    void reorderSubscriptions(const QStringList& orderedSubscriptionIds);
    void reorderServers(const QStringList& orderedIndexIds);
    // Location-grouping state (favorite flag, manual location override). These touch only the
    // per-server state fields, never a subscription or the server order, so they neither stop nor
    // restart the core and they persist through the same .state.json path as test results.
    void setServersLocation(const QStringList& indexIds, const QString& locationId);
    void setServersFavorite(const QStringList& indexIds, bool favorite);
    void applyExitLocationToActiveServer(const QString& countryCode);
    void hideSubscription(const QString& subscriptionId);
    void deleteSubscription(const QString& subscriptionId);
    // Writes whatever the in-memory config holds now. Hiding a subscription is deferred (see
    // hideSubscription()), and this is the point where such an edit -- possibly several of them, with
    // moves in between -- reaches the file: when the window regains focus and when the process is
    // about to exit. Leaving it until one of those is deliberate; a hide is never the last word.
    void persistPendingSubscriptionEdits();

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
