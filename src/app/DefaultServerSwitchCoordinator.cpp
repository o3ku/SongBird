#include "app/DefaultServerSwitchCoordinator.h"

#include <utility>

#include <QCoreApplication>
#include <QPointer>
#include <QTimer>

DefaultServerSwitchCoordinator::DefaultServerSwitchCoordinator(Callbacks callbacks, QObject* parent)
    : QObject(parent)
    , callbacks_(std::move(callbacks))
{
}

void DefaultServerSwitchCoordinator::setDefaultServer(const QString& indexId)
{
    if (queueSwitchUntilActivationSettles(PendingSwitch{PendingSwitch::Kind::Node, indexId, false})) {
        return;
    }
    if (requestSwitchAfterCoreStop(indexId, false)) {
        return;
    }

    const QString previousIndexId = currentIndexId();
    // The group target before the write clears it: leaving a group session is an activation
    // change even when the node id stays the anchor (the "Connect as Single Server" entry on
    // the anchor itself), because the running core holds the auto-select group config until a
    // restart rewrites it as a single outbound. Without this the escape hatch on the anchor
    // only cleared the persisted target and left the group session running.
    const bool leftGroupSession = !activeLocationGroupId().trimmed().isEmpty();
    const OperationResult result = setDefaultServerOnConfig(indexId);
    appendResult(result);
    syncWindow();

    if (!result.success) {
        return;
    }

    clearServerWarning();
    setCurrentActivationPending(true);
    if (isCoreRunning()) {
        if ((previousIndexId != currentIndexId() || leftGroupSession) && callbacks_.restartCoreIfRunning) {
            callbacks_.restartCoreIfRunning(QCoreApplication::translate(
                "AppBootstrap", "Reloading core after switching the default server."),
                true);
            return;
        }
        setCurrentActivationPending(false);
        return;
    }

    if (callbacks_.enableSystemProxy) {
        callbacks_.enableSystemProxy(true);
    }
}

void DefaultServerSwitchCoordinator::setDefaultServerWithTun(const QString& indexId)
{
    if (queueSwitchUntilActivationSettles(PendingSwitch{PendingSwitch::Kind::Node, indexId, true})) {
        return;
    }
    if (requestSwitchAfterCoreStop(indexId, true)) {
        return;
    }

    const QString previousIndexId = currentIndexId();
    // Same group-session rule as setDefaultServer(): "Set Current with TUN On" on the anchor of
    // a running group must also leave the group, which is a restart even with an unchanged id.
    const bool leftGroupSession = !activeLocationGroupId().trimmed().isEmpty();
    const OperationResult result = setDefaultServerOnConfig(indexId);
    appendResult(result);
    syncWindow();
    if (!result.success) {
        return;
    }

    clearServerWarning();
    setCurrentActivationPending(true);

    if (isTunEnabled()) {
        if (isCoreRunning() && (previousIndexId != currentIndexId() || leftGroupSession)) {
            if (callbacks_.restartCoreIfRunning) {
                callbacks_.restartCoreIfRunning(
                    QCoreApplication::translate(
                        "AppBootstrap", "Reloading core after switching the default server."),
                    true);
            }
            return;
        }
        if (callbacks_.enableSystemProxy) {
            callbacks_.enableSystemProxy(true);
        }
        return;
    }

    if (callbacks_.setTunEnabled) {
        callbacks_.setTunEnabled(true);
    }
}

// Group activation: set the location group as the session's target and start (or reload) the
// core on it. Mirrors setDefaultServer's shape -- stop-before-switch when a core runs, plain
// enable otherwise -- and leaves the node target (currentIndexId) untouched for the anchor.
void DefaultServerSwitchCoordinator::activateLocationGroup(const QString& locationId)
{
    const QString trimmedLocationId = locationId.trimmed();
    if (trimmedLocationId.isEmpty()) {
        return;
    }
    if (queueSwitchUntilActivationSettles(PendingSwitch{PendingSwitch::Kind::Group, trimmedLocationId, false})) {
        return;
    }

    if (isCoreRunning()) {
        if (activeLocationGroupId() == trimmedLocationId) {
            return;
        }
        setCurrentActivationPending(true);
        if (callbacks_.setActiveLocationGroup) {
            appendResult(callbacks_.setActiveLocationGroup(trimmedLocationId));
        }
        syncWindow();
        if (callbacks_.restartCoreIfRunning) {
            callbacks_.restartCoreIfRunning(QCoreApplication::translate(
                "AppBootstrap", "Reloading core after switching the location group."),
                true);
        }
        return;
    }

    setCurrentActivationPending(true);
    if (callbacks_.setActiveLocationGroup) {
        appendResult(callbacks_.setActiveLocationGroup(trimmedLocationId));
    }
    syncWindow();
    clearServerWarning();
    if (callbacks_.enableSystemProxy) {
        callbacks_.enableSystemProxy(true);
    }
}

bool DefaultServerSwitchCoordinator::requestSwitchAfterCoreStop(const QString& indexId, bool enableTun)
{
    const QString trimmedId = indexId.trimmed();
    if (trimmedId.isEmpty() || !isCoreRunning() || currentIndexId() == trimmedId) {
        return false;
    }

    setCurrentActivationPending(true);
    appendResult(OperationResult::ok(QCoreApplication::translate(
        "AppBootstrap", "Stopping current core before switching the default server.")));
    if (callbacks_.switchRunningCoreToServer) {
        callbacks_.switchRunningCoreToServer(trimmedId, enableTun);
    }
    return true;
}

// A switch asked for while an activation is in flight. The start channel belongs to that
// activation until it settles: enabling now would only be rejected as an overlapping start
// (leaving the persisted target desynced from the running session until some later restart),
// and stopping what has not started is not possible either. So the whole gesture is recorded
// and replayed the moment the session settles -- as if freshly clicked, which keeps every
// existing decision path (stop-and-switch, restart, group no-op) intact. The latest ask wins.
bool DefaultServerSwitchCoordinator::queueSwitchUntilActivationSettles(PendingSwitch pending)
{
    if (!isActivationInProgress()) {
        return false;
    }
    pendingSwitch_ = std::move(pending);
    setCurrentActivationPending(true);
    appendResult(OperationResult::ok(QCoreApplication::translate(
        "AppBootstrap", "The proxy is starting; the selection will be applied once it finishes.")));
    return true;
}

void DefaultServerSwitchCoordinator::resumePendingActivationSwitch()
{
    if (!pendingSwitch_.has_value()) {
        return;
    }
    const PendingSwitch pending = *pendingSwitch_;
    pendingSwitch_.reset();
    if (isShuttingDown()) {
        setCurrentActivationPending(false);
        return;
    }
    if (pending.kind == PendingSwitch::Kind::Group) {
        activateLocationGroup(pending.id);
        return;
    }
    if (pending.enableTun) {
        setDefaultServerWithTun(pending.id);
        return;
    }
    setDefaultServer(pending.id);
}

void DefaultServerSwitchCoordinator::discardPendingActivationSwitch()
{
    if (!pendingSwitch_.has_value()) {
        return;
    }
    pendingSwitch_.reset();
    setCurrentActivationPending(false);
}

void DefaultServerSwitchCoordinator::scheduleSwitchAfterCoreStopped(
    const QString& indexId,
    bool enableTun,
    bool showStartupOverlay)
{
    QPointer<QObject> context(resolvedUiContext());
    const std::weak_ptr<char> lifetimeGuard = resolvedLifetimeGuard();
    QTimer::singleShot(
        0,
        context.isNull() ? QCoreApplication::instance() : context.data(),
        [this, indexId, enableTun, showStartupOverlay, lifetimeGuard]() {
            if (lifetimeGuard.expired() || isShuttingDown()) {
                return;
            }

            switchAfterCoreStopped(indexId, enableTun, showStartupOverlay);
        });
}

void DefaultServerSwitchCoordinator::switchAfterCoreStopped(
    const QString& indexId,
    bool enableTun,
    bool showStartupOverlay)
{
    if (indexId.isEmpty() || isShuttingDown()) {
        setCurrentActivationPending(false);
        return;
    }

    const QString previousIndexId = currentIndexId();
    const OperationResult result = setDefaultServerOnConfig(indexId);
    appendResult(result);
    syncWindow();
    if (!result.success) {
        setCurrentActivationPending(false);
        return;
    }

    clearServerWarning();

    if (enableTun && !isTunEnabled()) {
        if (callbacks_.setTunEnabled) {
            callbacks_.setTunEnabled(true);
        }
        return;
    }

    if (previousIndexId != currentIndexId() || enableTun) {
        appendResult(OperationResult::ok(QCoreApplication::translate(
            "AppBootstrap", "Starting core after switching the default server.")));
        if (callbacks_.startProxyAfterSwitch) {
            callbacks_.startProxyAfterSwitch(showStartupOverlay);
        }
        return;
    }

    setCurrentActivationPending(false);
}

QString DefaultServerSwitchCoordinator::currentIndexId() const
{
    return callbacks_.currentIndexId ? callbacks_.currentIndexId() : QString();
}

bool DefaultServerSwitchCoordinator::isCoreRunning() const
{
    return callbacks_.isCoreRunning ? callbacks_.isCoreRunning() : false;
}

bool DefaultServerSwitchCoordinator::isActivationInProgress() const
{
    return callbacks_.isActivationInProgress && callbacks_.isActivationInProgress();
}

bool DefaultServerSwitchCoordinator::isTunEnabled() const
{
    return callbacks_.isTunEnabled ? callbacks_.isTunEnabled() : false;
}

bool DefaultServerSwitchCoordinator::isShuttingDown() const
{
    return callbacks_.isShuttingDown ? callbacks_.isShuttingDown() : false;
}

QObject* DefaultServerSwitchCoordinator::resolvedUiContext() const
{
    if (callbacks_.uiContext) {
        QObject* context = callbacks_.uiContext();
        if (context != nullptr) {
            return context;
        }
    }
    return QCoreApplication::instance();
}

std::weak_ptr<char> DefaultServerSwitchCoordinator::resolvedLifetimeGuard() const
{
    if (callbacks_.lifetimeGuard) {
        return callbacks_.lifetimeGuard();
    }

    static const std::shared_ptr<char> fallbackGuard = std::make_shared<char>();
    return fallbackGuard;
}

QString DefaultServerSwitchCoordinator::activeLocationGroupId() const
{
    return callbacks_.activeLocationGroupId ? callbacks_.activeLocationGroupId() : QString();
}

OperationResult DefaultServerSwitchCoordinator::setDefaultServerOnConfig(const QString& indexId) const
{
    return callbacks_.setDefaultServer
        ? callbacks_.setDefaultServer(indexId)
        : OperationResult::fail(QCoreApplication::translate(
            "AppBootstrap", "Default server selection is unavailable."));
}

void DefaultServerSwitchCoordinator::appendResult(const OperationResult& result) const
{
    if (callbacks_.appendResult) {
        callbacks_.appendResult(result);
    }
}

void DefaultServerSwitchCoordinator::syncWindow() const
{
    if (callbacks_.syncWindow) {
        callbacks_.syncWindow();
    }
}

void DefaultServerSwitchCoordinator::clearServerWarning() const
{
    if (callbacks_.clearServerWarning) {
        callbacks_.clearServerWarning();
    }
}

void DefaultServerSwitchCoordinator::setCurrentActivationPending(bool pending) const
{
    if (callbacks_.setCurrentActivationPending) {
        callbacks_.setCurrentActivationPending(pending);
    }
}
