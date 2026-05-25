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
    if (requestSwitchAfterCoreStop(indexId, false)) {
        return;
    }

    const QString previousIndexId = currentIndexId();
    const OperationResult result = setDefaultServerOnConfig(indexId);
    appendResult(result);
    syncWindow();

    if (!result.success) {
        return;
    }

    clearServerWarning();
    setCurrentActivationPending(true);
    if (isCoreRunning()) {
        if (previousIndexId != currentIndexId() && callbacks_.restartCoreIfRunning) {
            callbacks_.restartCoreIfRunning(QStringLiteral("Reloading core after switching the default server."), true);
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
    if (requestSwitchAfterCoreStop(indexId, true)) {
        return;
    }

    const QString previousIndexId = currentIndexId();
    const OperationResult result = setDefaultServerOnConfig(indexId);
    appendResult(result);
    syncWindow();
    if (!result.success) {
        return;
    }

    clearServerWarning();
    setCurrentActivationPending(true);

    if (isTunEnabled()) {
        if (isCoreRunning() && previousIndexId != currentIndexId()) {
            if (callbacks_.restartCoreIfRunning) {
                callbacks_.restartCoreIfRunning(
                    QStringLiteral("Reloading core after switching the default server."),
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

bool DefaultServerSwitchCoordinator::requestSwitchAfterCoreStop(const QString& indexId, bool enableTun)
{
    const QString trimmedId = indexId.trimmed();
    if (trimmedId.isEmpty() || !isCoreRunning() || currentIndexId() == trimmedId) {
        return false;
    }

    setCurrentActivationPending(true);
    appendResult(OperationResult::ok(QStringLiteral("Stopping current core before switching the default server.")));
    if (callbacks_.switchRunningCoreToServer) {
        callbacks_.switchRunningCoreToServer(trimmedId, enableTun);
    }
    return true;
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
        appendResult(OperationResult::ok(QStringLiteral("Starting core after switching the default server.")));
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

OperationResult DefaultServerSwitchCoordinator::setDefaultServerOnConfig(const QString& indexId) const
{
    return callbacks_.setDefaultServer
        ? callbacks_.setDefaultServer(indexId)
        : OperationResult::fail(QStringLiteral("Default server selection is unavailable."));
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
