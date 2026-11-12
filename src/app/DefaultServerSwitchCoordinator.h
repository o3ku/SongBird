#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <QObject>
#include <QString>

#include "common/OperationResult.h"

class DefaultServerSwitchCoordinator final : public QObject {
public:
    struct Callbacks {
        std::function<QString()> currentIndexId;
        std::function<bool()> isCoreRunning;
        std::function<bool()> isTunEnabled;
        // True while the session is between "start requested" and "settled" (activated/failed/
        // stopped): the start channel is owned by the in-flight activation, so a switch asked
        // for now must queue (see resumePendingActivationSwitch()).
        std::function<bool()> isActivationInProgress;
        std::function<bool()> isShuttingDown;
        std::function<QObject*()> uiContext;
        std::function<std::weak_ptr<char>()> lifetimeGuard;
        std::function<OperationResult(const QString&)> setDefaultServer;
        // Group activation target on the running config ("" = single-node session).
        std::function<QString()> activeLocationGroupId;
        std::function<OperationResult(const QString&)> setActiveLocationGroup;
        std::function<void(const OperationResult&)> appendResult;
        std::function<void()> syncWindow;
        std::function<void()> clearServerWarning;
        std::function<void(bool)> setCurrentActivationPending;
        std::function<void(const QString&, bool)> switchRunningCoreToServer;
        std::function<void(const QString&, bool)> restartCoreIfRunning;
        std::function<void(bool)> enableSystemProxy;
        std::function<void(bool)> setTunEnabled;
        std::function<void(bool)> startProxyAfterSwitch;
    };

    explicit DefaultServerSwitchCoordinator(Callbacks callbacks, QObject* parent = nullptr);

    void setDefaultServer(const QString& indexId);
    void setDefaultServerWithTun(const QString& indexId);
    void activateLocationGroup(const QString& locationId);
    // Applies the switch queued while an activation was in flight, if any. Called by the host
    // exactly when a session settles: after activated() and after failed(). A plain stopped()
    // (the user's own stop) should discardPendingActivationSwitch() instead.
    void resumePendingActivationSwitch();
    void discardPendingActivationSwitch();
    bool requestSwitchAfterCoreStop(const QString& indexId, bool enableTun);
    void scheduleSwitchAfterCoreStopped(const QString& indexId, bool enableTun, bool showStartupOverlay);
    void switchAfterCoreStopped(const QString& indexId, bool enableTun, bool showStartupOverlay);

private:
    QString currentIndexId() const;
    bool isCoreRunning() const;
    bool isActivationInProgress() const;
    bool isTunEnabled() const;
    bool isShuttingDown() const;
    QObject* resolvedUiContext() const;
    std::weak_ptr<char> resolvedLifetimeGuard() const;
    OperationResult setDefaultServerOnConfig(const QString& indexId) const;
    QString activeLocationGroupId() const;
    void appendResult(const OperationResult& result) const;
    void syncWindow() const;
    void clearServerWarning() const;
    void setCurrentActivationPending(bool pending) const;
    // A switch asked for while an activation was in flight. The intent is recorded, not
    // applied: writing the config early would desync it from whatever the in-flight start is
    // about to run, so the queue holds the whole gesture and replays it as if freshly clicked.
    struct PendingSwitch {
        enum class Kind { Node, Group };
        Kind kind = Kind::Node;
        QString id;
        bool enableTun = false;
    };
    bool queueSwitchUntilActivationSettles(PendingSwitch pending);

    Callbacks callbacks_;
    std::optional<PendingSwitch> pendingSwitch_;
};
