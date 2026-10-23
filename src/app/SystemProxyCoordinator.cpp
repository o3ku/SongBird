#include "app/SystemProxyCoordinator.h"

#include <optional>

#include <utility>

#include <QCoreApplication>

#include "appcore/ProxySession.h"
#include "common/LocalInboundPorts.h"
#include "common/SystemProxyExceptions.h"
#include "platform/ISystemProxyService.h"
#include "services/ServerService.h"

SystemProxyCoordinator::SystemProxyCoordinator(Dependencies deps, Callbacks callbacks)
    : deps_(deps)
    , callbacks_(std::move(callbacks))
{
}

bool SystemProxyCoordinator::saveMode(SystemProxyMode mode)
{
    const int previousValue = deps_.config.sysProxyType;
    const bool previousMainProxyEnabled = deps_.config.ui().mainProxyEnabled;
    deps_.config.sysProxyType = toLegacySystemProxyModeValue(mode);
    deps_.config.ui().mainProxyEnabled = mode == SystemProxyMode::ForcedChange;
    const OperationResult saveResult = deps_.serverService.save(deps_.config);
    if (!saveResult.success) {
        deps_.config.sysProxyType = previousValue;
        deps_.config.ui().mainProxyEnabled = previousMainProxyEnabled;
        appendResult(OperationResult::fail(
            QCoreApplication::translate("AppBootstrap", "Failed to save the selected system proxy mode: %1").arg(saveResult.message)));
        syncStatusIndicators();
        return false;
    }

    return true;
}

void SystemProxyCoordinator::setMode(
    SystemProxyMode mode,
    bool skipTunCleanup,
    bool immediateStop,
    bool showStartupOverlay)
{
    if (deps_.proxySession == nullptr) {
        return;
    }

    const bool coreProcessRunning = deps_.proxySession->isCoreRunning();
    const bool coreReady = deps_.proxySession->isActive();
    const bool coreActivationPending = deps_.proxySession->isActivationInProgress();
    if (mode == SystemProxyMode::ForcedChange
        && !coreReady
        && coreActivationPending) {
        if (showStartupOverlay) {
            deps_.proxySession->requestChecklistOverlay();
        }
        appendResult(OperationResult::ok(QCoreApplication::translate("AppBootstrap", "Core start is already in progress.")));
        syncStatusIndicators();
        return;
    }

    if (!saveMode(mode)) {
        return;
    }

    if (mode == SystemProxyMode::ForcedChange) {
        if (coreReady) {
            // Capture before the first write of this session, and only once. A second capture
            // would record our own port as "what the user had", which is the bug the snapshot
            // exists to prevent.
            captureSnapshotIfNeeded();
            const bool proxyApplied = updateMode(mode);
            if (proxyApplied) {
                deps_.proxySession->adoptManagedSystemProxy(true);
            }
            appendResult(proxyApplied
                ? OperationResult::ok(QCoreApplication::translate(
                      "AppBootstrap", "System proxy mode set to %1.").arg(systemProxyModeDisplayName(mode)))
                : OperationResult::fail(QCoreApplication::translate(
                      "AppBootstrap", "Failed to apply system proxy mode %1.").arg(systemProxyModeDisplayName(mode))));
            syncStatusIndicators();
            return;
        }
        appendResult(OperationResult::ok(
            QCoreApplication::translate("AppBootstrap", "System proxy mode set to %1. It will be applied after the core starts.")
                .arg(systemProxyModeDisplayName(mode))));
        appendResult(OperationResult::ok(QCoreApplication::translate("AppBootstrap", "Starting the active core because Global system proxy mode was enabled.")));
        startManagedProxyCore(skipTunCleanup, showStartupOverlay);
        return;
    }

    const bool shouldStop = coreProcessRunning || coreActivationPending;
    const bool proxyApplied = updateMode(mode);
    if (proxyApplied && mode == SystemProxyMode::ForcedClear) {
        deps_.proxySession->adoptManagedSystemProxy(false);
    }
    appendResult(proxyApplied
        ? OperationResult::ok(QCoreApplication::translate(
              "AppBootstrap", "System proxy mode set to %1.").arg(systemProxyModeDisplayName(mode)))
        : OperationResult::fail(QCoreApplication::translate(
              "AppBootstrap", "Failed to apply system proxy mode %1.").arg(systemProxyModeDisplayName(mode))));
    if (shouldStop) {
        appendResult(OperationResult::ok(QCoreApplication::translate("AppBootstrap", "Stopping the active core because system proxy was cleared.")));
        deps_.proxySession->stop(immediateStop);
        return;
    }

    syncStatusIndicators();
}

void SystemProxyCoordinator::clearStateAfterCoreStopped()
{
    if (deps_.config.sysProxyType == toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear)
        && !deps_.config.ui().mainProxyEnabled) {
        return;
    }

    saveMode(SystemProxyMode::ForcedClear);
}

void SystemProxyCoordinator::applyModeOnExit(bool windowsShutdown)
{
    if (exitProxyStateApplied_ || deps_.systemProxyService == nullptr) {
        return;
    }

    exitProxyStateApplied_ = true;
    const bool managedSystemProxyActive =
        deps_.proxySession != nullptr && deps_.proxySession->isManagedProxyActive();
    if (!managedSystemProxyActive) {
        // Never touched it, so there is nothing to undo. This used to be reached only after the
        // windowsShutdown branch had already wiped the registry; both branches now share the same
        // ownership gate, because "the app is quitting" is not evidence that we changed anything.
        return;
    }

    // Restore, not clear.
    //
    // resolveSystemProxyModeOnExit() returns ForcedClear unconditionally, and acting on it meant
    // quitting deleted ProxyServer/ProxyOverride even when the user had a proxy of their own
    // before starting the app. The snapshot is the only thing that knows the difference, so it is
    // consulted first and clearing is only the fallback when no snapshot survived.
    if (restoreCapturedSnapshot()) {
        deps_.proxySession->adoptManagedSystemProxy(false);
        return;
    }

    // windowsShutdown has no time budget for a full restore, so it falls back to the cheap
    // clear it has always done. Outside a shutdown the same fallback runs only when the snapshot
    // is missing, which means the state file was lost -- the one case where clearing is the best
    // available answer.
    if (windowsShutdown) {
        deps_.systemProxyService->resetOnShutdown();
        deps_.proxySession->adoptManagedSystemProxy(false);
        return;
    }

    const SystemProxyMode mode = resolveSystemProxyModeOnExit(
        normalizeSystemProxyMode(deps_.config.sysProxyType),
        windowsShutdown);
    updateMode(mode);
    deps_.proxySession->adoptManagedSystemProxy(expectedSystemProxyEnabled(mode));
}

void SystemProxyCoordinator::captureSnapshotIfNeeded()
{
    if (deps_.systemProxyService == nullptr || deps_.config.systemProxySnapshot().valid) {
        return;
    }
    deps_.config.systemProxySnapshot() = deps_.systemProxyService->capture();
    if (deps_.config.systemProxySnapshot().valid && callbacks_.persistConfig) {
        // Persisted immediately, not on the way out: the process can be killed while proxying,
        // and the snapshot is the only thing that can put the user's own proxy back afterwards.
        callbacks_.persistConfig();
    }
}

bool SystemProxyCoordinator::restoreCapturedSnapshot()
{
    if (deps_.systemProxyService == nullptr) {
        return false;
    }
    const SystemProxySnapshot snapshot = deps_.config.systemProxySnapshot();
    if (!isRestorableSystemProxySnapshot(snapshot)) {
        return false;
    }
    if (!deps_.systemProxyService->restore(snapshot)) {
        return false;
    }
    // Spent: leaving it would make the next quit "restore" a state the user has since changed.
    deps_.config.systemProxySnapshot() = SystemProxySnapshot{};
    if (callbacks_.persistConfig) {
        callbacks_.persistConfig();
    }
    return true;
}

void SystemProxyCoordinator::cleanupRuntimeForExit(bool windowsShutdown)
{
    applyModeOnExit(windowsShutdown);
    if (deps_.proxySession != nullptr
        && (deps_.proxySession->isCoreRunning() || deps_.proxySession->isTransitioning())) {
        deps_.proxySession->stop(true);
    }
    syncStatusIndicators();
}

void SystemProxyCoordinator::enable(bool showStartupOverlay)
{
    setMode(SystemProxyMode::ForcedChange, false, false, showStartupOverlay);
}

void SystemProxyCoordinator::retryStartup(bool showStartupOverlay)
{
    enable(showStartupOverlay);
}

void SystemProxyCoordinator::disable()
{
    setMode(SystemProxyMode::ForcedClear);
}

bool SystemProxyCoordinator::updateMode(SystemProxyMode mode) const
{
    return deps_.systemProxyService == nullptr
        || deps_.systemProxyService->update(
            mode,
            LocalInboundPorts::httpPort(deps_.config),
            deps_.config.localPort,
            buildExceptions(),
            deps_.config.systemProxyAdvancedProtocol);
}

QString SystemProxyCoordinator::buildExceptions() const
{
    // Resolved by common/SystemProxyExceptions, which SongBirdAuto calls too. The two front ends
    // used to hold identical copies of the default list and then diverged over whether the routing
    // rules' direct hosts were merged -- so the same config bypassed the proxy differently
    // depending on which executable was in front of it.
    return SystemProxyExceptions::resolve(deps_.config);
}

void SystemProxyCoordinator::appendResult(const OperationResult& result) const
{
    if (callbacks_.appendResult) {
        callbacks_.appendResult(result);
    }
}

void SystemProxyCoordinator::syncStatusIndicators() const
{
    if (callbacks_.syncStatusIndicators) {
        callbacks_.syncStatusIndicators();
    }
}

void SystemProxyCoordinator::startManagedProxyCore(bool skipTunCleanup, bool showStartupOverlay) const
{
    if (callbacks_.startManagedProxyCore) {
        callbacks_.startManagedProxyCore(skipTunCleanup, showStartupOverlay);
    }
}
