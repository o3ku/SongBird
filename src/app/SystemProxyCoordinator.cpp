#include "app/SystemProxyCoordinator.h"

#include <cstdio>
#include <optional>

#include <utility>

#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpSocket>

#include "appcore/ProxySession.h"
#include "appcore/SystemProxyOwnershipPolicy.h"
#include "common/LocalInboundPorts.h"
#include "common/PortValidator.h"
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

// Startup with the app's proxy off. The old disabled branch called disable() (ForcedClear),
// which deleted ProxyServer/ProxyOverride/AutoConfigURL on every launch -- so a user who had
// their own enterprise proxy or PAC script before opening the app lost it by simply starting
// the app with its own proxy off. The registry holding a proxy is not evidence that *we* set
// it (SystemProxyOwnershipPolicy); only a captured snapshot is. Restoring that snapshot heals
// the crash case (our dead 127.0.0.1:<port> is still there) and puts the user's own settings
// back; with no snapshot there is nothing of ours to undo, so the registry is left alone --
// unless it still points at our own port with nothing listening on it. That state can only
// be a leftover of ours (a snapshot lost with the state file, or a build from before snapshots
// existed), and leaving it means the machine stays offline. It is removed by restoring the
// sanitized capture, which is proxy off; a listener means the proxy may be working, so it is
// left alone instead.
namespace {

// Whether anything answers on a loopback port. A bare connect is not proof:
// on macOS arm64 a connect to a just-freed loopback port has been observed to
// succeed, twice in a row ([probe] diagnostics, run 44693424035), so connect
// success alone cannot tell a dead port from a live one there. The probe
// therefore proves a peer by exchanging bytes. The port is the HTTP inbound
// (LocalInboundPorts::httpPort), so a real listener speaks HTTP: a complete
// malformed request is sent -- malformed on purpose, so a real proxy can
// never act on it, and complete (request line plus an empty line), so an HTTP
// peer must answer it (400) or close rather than wait for more input.
//   - bytes back: a live peer answered.
//   - socket error or disconnect during the exchange: the peer is gone; on
//     macOS arm64 this is the RST the phantom connect had not delivered yet.
//   - silence with the connection still healthy: a real listener holding
//     the connection (a silent TCP listener never answers, and the test
//     fixture models a live proxy that way). The phantom's RST surfaces
//     within the wait, so a connection still in ConnectedState afterwards
//     is a live peer.
// A peer that closes without answering is treated as dead: it is
// indistinguishable from the phantom RST, and a working HTTP proxy inbound
// answers a complete malformed request with bytes (400) rather than closing
// silently.
const char *socketStateName(QAbstractSocket::SocketState state)
{
    switch (state) {
    case QAbstractSocket::UnconnectedState:
        return "Unconnected";
    case QAbstractSocket::HostLookupState:
        return "HostLookup";
    case QAbstractSocket::ConnectingState:
        return "Connecting";
    case QAbstractSocket::ConnectedState:
        return "Connected";
    case QAbstractSocket::BoundState:
        return "Bound";
    case QAbstractSocket::ClosingState:
        return "Closing";
    case QAbstractSocket::ListeningState:
        return "Listening";
    }
    return "Unknown";
}

// Diagnostic: one greppable line per probe attempt, written to stderr so it
// survives in the CI log when the calling test fails (ctest runs with
// --output-on-failure). The port is loopback-only; nothing sensitive is
// logged. The error string is flattened to a single line so each attempt
// stays greppable.
void logProbeAttempt(int port, int attempt, const QTcpSocket &socket)
{
    QString error = socket.errorString();
    error.replace(QLatin1Char('\n'), QLatin1Char(' '));
    std::fprintf(stderr,
                 "[probe] port=%d attempt=%d connected=%d error=\"%s\" state=%s\n",
                 port, attempt,
                 socket.state() == QAbstractSocket::ConnectedState ? 1 : 0,
                 error.toUtf8().constData(), socketStateName(socket.state()));
}

void logProbeVerdict(int port, const char *verdict, const char *reason)
{
    std::fprintf(stderr, "[probe] port=%d verdict=%s reason=%s\n", port, verdict, reason);
}

// One line for the exchange phase: what the peer did with the probe request.
void logProbeExchange(int port, qint64 written, const QTcpSocket &socket)
{
    QString error = socket.errorString();
    error.replace(QLatin1Char('\n'), QLatin1Char(' '));
    std::fprintf(stderr,
                 "[probe] port=%d exchange written=%lld available=%lld error=\"%s\" state=%s\n",
                 port, written, socket.bytesAvailable(),
                 error.toUtf8().constData(), socketStateName(socket.state()));
}

bool isLocalTcpPortListening(int port)
{
    if (!isValidTcpPort(port)) {
        logProbeVerdict(port, "not-listening", "invalid-port");
        return false;
    }
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
    if (!socket.waitForConnected(200)) {
        logProbeAttempt(port, 1, socket);
        logProbeVerdict(port, "not-listening", "first-connect-failed");
        return false;
    }
    logProbeAttempt(port, 1, socket);
    // Deliberately malformed but complete: no proxy can act on it, and an
    // HTTP peer must answer (400) or close instead of waiting for more.
    static const char kProbeRequest[] = "INVALID\r\n\r\n";
    const qint64 written = socket.write(kProbeRequest);
    if (written < 0) {
        logProbeExchange(port, written, socket);
        logProbeVerdict(port, "not-listening", "write-failed");
        return false;
    }
    socket.waitForReadyRead(200);
    logProbeExchange(port, written, socket);
    if (socket.bytesAvailable() > 0) {
        logProbeVerdict(port, "listening", "response-received");
        socket.disconnectFromHost();
        return true;
    }
    if (socket.state() != QAbstractSocket::ConnectedState) {
        logProbeVerdict(port, "not-listening", "exchange-failed");
        return false;
    }
    logProbeVerdict(port, "listening", "silent-peer-connected");
    socket.disconnectFromHost();
    return true;
}

} // namespace

void SystemProxyCoordinator::recoverOwnedSystemProxyOnStartup()
{
    if (deps_.systemProxyService == nullptr) {
        return;
    }
    // The registry as this process found it: the crash case is a proxy that still points at
    // our own port with no core behind it, and only that state is a leftover of ours. A
    // snapshot alone cannot prove it -- the registry may have moved on since the snapshot was
    // captured -- so the current values are read first.
    const SystemProxySnapshot current = deps_.systemProxyService->capture();
    const int httpPort = LocalInboundPorts::httpPort(deps_.config);
    const QString ourServer = QStringLiteral("127.0.0.1:%1").arg(httpPort);
    const bool listening = isLocalTcpPortListening(httpPort);
    switch (decideStartupLeftoverProxyAction(
        current, ourServer, listening,
        isRestorableSystemProxySnapshot(deps_.config.systemProxySnapshot()))) {
    case StartupLeftoverProxyAction::LeaveAlone:
        return;
    case StartupLeftoverProxyAction::Restore:
        if (!restoreCapturedSnapshot()) {
            return;
        }
        if (deps_.proxySession != nullptr) {
            deps_.proxySession->adoptManagedSystemProxy(false);
        }
        appendResult(OperationResult::ok(QCoreApplication::translate(
            "AppBootstrap", "Restored the system proxy settings captured before the previous session.")));
        return;
    case StartupLeftoverProxyAction::Disable:
        // No snapshot survived to restore, so the sanitized capture is the restore: proxy
        // off, leftover keys deleted. sanitizeSelfReferentialSnapshot turns a snapshot that
        // points at our own port into exactly that.
        if (!deps_.systemProxyService->restore(sanitizeSelfReferentialSnapshot(current, ourServer))) {
            appendResult(OperationResult::fail(QCoreApplication::translate(
                "AppBootstrap", "Could not remove the leftover system proxy pointing at %1; the system proxy still points at a port nothing is listening on.").arg(ourServer)));
            return;
        }
        if (deps_.proxySession != nullptr) {
            deps_.proxySession->adoptManagedSystemProxy(false);
        }
        appendResult(OperationResult::ok(QCoreApplication::translate(
            "AppBootstrap", "Removed a leftover system proxy pointing at %1; nothing was listening on that port, so it was turned off.").arg(ourServer)));
        return;
    }
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
    // Resolved by common/SystemProxyExceptions. Two call sites used to hold identical copies
    // of the default list and then diverged over whether the routing rules' direct hosts were
    // merged -- so the same config bypassed the proxy differently
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
