#pragma once

#include <functional>
#include <memory>

#include <QObject>
#include <QProcess>
#include <QString>

#include "appcore/TunRuntimeService.h"
#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"

class CoreDiscoveryService;
class ICoreProcessHost;
class ProxySession;
class QtCoreProcessHost;

// AutoSessionShell's first resident: the TUN runtime SongBirdAuto owns.
//
// ProxySessionHost.h documented why this cluster exists at all: the main app starts its TUN
// runtime inside the session's own startup phases, while SongBirdAuto owns a TUN process of
// its own and starts it BEFORE ProxySession::start() -- which is what forced the session to
// grow `undoManagedSystemProxyAfterAbortedStartup()` (issue #8: a TUN failure aborts before
// start() runs, while the registry may still hold an earlier round's proxy). This class keeps
// that ordering in the front end it belongs to: it starts and stops the TUN core, writes its
// config, and owns the one decision the abort branch needs -- "the start died before the
// session saw it, so the session's ownership undo has to run".
//
// What it deliberately does NOT own:
//   * The ownership decision itself. `undoAfterAbortedStart()` only *calls* the session's
//     documented entry; whether the registry is ours to touch is decided inside the session by
//     SystemProxyOwnershipPolicy, exactly as before. The shell moves the caller, not the rule.
//   * The coordinator's flags. A failed start still has to give busy_/running_ back through
//     rollbackStartAfterTunFailure(); this class reports the failure and the coordinator does
//     that, so the status/flag ordering pinned by SongBirdAutoCoordinatorStopTests is unchanged.
//
// Seam notes, same discipline as every other injected dependency in src/auto/:
//   * The core host and discovery service arrive as pointers the caller keeps alive for the
//     lifetime of this object (the coordinator owns both; a test can hand in a double).
//   * `isProcessElevated` is a function rather than a flag read at construction: elevation can
//     change between calls in principle, and the old inline check read it fresh each start.
//   * Logging goes through the callback so the "tun | <core output>" prefix and the status
//     sentences stay the coordinator's presentation, not this class's.
class AutoTunRuntime final : public QObject
{
    Q_OBJECT

public:
    // How the started TUN device routes: through the local proxy (the normal, proxying state)
    // or direct-only (the standalone device state used while no session is running).
    enum class Routing {
        RelayToLocalProxy,
        DirectOnly
    };
    struct Dependencies {
        // The TUN core's process host. Borrowed, not owned: the coordinator constructs one and
        // hands it in so its teardown order (core hosts before services) stays unchanged.
        ICoreProcessHost& tunCore;
        TunRuntimeService& runtimeService;
        CoreDiscoveryService* coreDiscovery = nullptr;
        QString configPath;
        // Fresh per start, like the inline check this class replaces.
        std::function<bool()> isProcessElevated;
    };

    explicit AutoTunRuntime(Dependencies deps, QObject* parent = nullptr);

    // Starts or stops the TUN runtime for `config`. Idempotent for the same routing; a routing
    // change restarts the core. `session` is consulted only on the failure path -- and only to
    // run the session-side ownership undo documented above. May be null (a test with no
    // registry at all); the failure is then reported without the undo.
    OperationResult syncForConfig(
        const Config& config,
        Routing routing,
        ProxySession* session);

    // The abort branch's undo, exposed for the coordinator's failure path: a start that died
    // before ProxySession::start() runs still has to undo a proxy an earlier round applied.
    // Sync itself does NOT call it on failure -- the coordinator decides which failures abort
    // the start (TUN does; a relay restart does not), so the call stays at its call site.
    void undoSessionProxyAfterAbortedStart(ProxySession* session) const;

    // Full stop: core down, adapter removed. The coordinator's destructor and the TUN-disabled
    // path both end here, as they did with the inline code.
    void shutdown(bool cleanupAdapter);

    bool isRunningWithRouting(Routing routing) const;

signals:
    // Presentation stays the coordinator's: every line, status and sentence it showed before
    // arrives here instead, and the coordinator forwards it to its own log/status presentation
    // with the prefixes it already used. The "routing %1" tag on the output line is what keeps
    // relay and direct distinguishable in the log without a second field.
    void tunOutput(const QString& line);
    void tunStarted(const QString& message);
    void tunStartFailed(const QString& message);
    void tunExitedUnexpectedly(int exitCode);
    void adapterCleanupResult(const OperationResult& result);

private:
    OperationResult start(const Config& config, Routing routing);
    void stop(bool cleanupAdapter);
    QString writeRuntimeConfig(const Config& config, Routing routing) const;
    CoreInfo resolveSingBoxCoreInfo() const;

    Dependencies deps_;
    // The routing the running core (if any) was started with, so an identical sync is a no-op
    // and a change restarts -- the semantics the inline `tunRuntimeRelayToProxy_` had.
    int activeRouting_ = -1;
};
