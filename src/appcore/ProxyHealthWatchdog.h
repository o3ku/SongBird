#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <QObject>
#include <QString>

#include "appcore/ProxyHealthWatchPolicy.h"
#include "common/OperationResult.h"

class QThread;
class QTimer;
struct Config;
class ProxySession;

// The health watch that used to live inside ProxySession, split out along its ownership
// boundary. It owns every piece of state that exists only for the watch: two single-shot
// timers, the policy, the in-flight flags, and the two injected probes. ProxySession keeps
// the phase machine and calls arm()/cancel() from the five places that used to touch the
// watch inline.
//
// Why this split is safe where others would not be: the watch never writes the phase, never
// starts or stops the core, and never touches the registry. Its only outputs are a warning
// sentence (warningChanged) and log lines (logMessage). ProxySession remains the single
// writer of phase_; the watch is a satellite, not a co-owner of the state machine.
//
// Threading contract, inherited unchanged from the code that lived in ProxySession:
//   * The probes (AvailabilityCheck, DirectConnectivityCheck) are called on a worker
//     thread, so their implementations must not touch this object.
//   * The workers post their answers back with QMetaObject::invokeMethod(this, ...) plus a
//     lifetime guard, so a watchdog destroyed mid-probe only loses the answer.
// The guard used for that is supplied by the session (sessionLifetimeGuard): the session
// outlives the watchdog (QObject parenting), so expiring the session's guard already covers
// every case in which the watchdog is gone too.
class ProxyHealthWatchdog final : public QObject
{
    Q_OBJECT

public:
    // Runs the node-availability probe. Injected rather than constructed here for the same
    // reasons ProxySession documented: a host can leave it unset, which is how the auto front
    // end opts out (it already runs its own health check and must not get a second one), and a
    // test can supply a scripted answer without standing up a proxy.
    //
    // Called on a background thread, so the implementation must not touch this object.
    using AvailabilityCheck = std::function<OperationResult(const Config& config)>;

    // The unproxied cross-check consulted only while the waived-outbound warning is up:
    // true means "this machine can still reach the probe hosts on its own", which is what
    // retracts that warning. Same injection rule as AvailabilityCheck. When unset, the
    // session's real probe runs through `fallbackDirectProbe` below -- the watchdog has no
    // dependencies of its own to reach it.
    //
    // Called on a background thread, so the implementation must not touch this object.
    using DirectConnectivityCheck = std::function<bool(int timeoutMs)>;

    // How a started worker thread is registered with whoever tracks background threads.
    using ThreadTracker = std::function<void(QThread*)>;

    // Supplies what only the session knows, read on the GUI thread before a worker launches.
    struct SessionStrings {
        // The short handle this activation's log lines carry, e.g. "id=a1b2".
        std::function<QString()> activationTag;
        // The config the availability probe is run against (the session's current request).
        std::function<Config()> probeConfig;
        // The real unproxied probe, used when no DirectConnectivityCheck was injected.
        DirectConnectivityCheck fallbackDirectProbe;
        // Two finished, already-localized sentences. Supplied by the session so they stay in
        // the .ts's "ProxySession" context; a tr() here would orphan the existing entries.
        std::function<QString()> recheckStillUnavailableSentence;
        std::function<QString()> recheckAvailableAgainSentence;
    };

    ProxyHealthWatchdog(
        std::weak_ptr<char> sessionLifetimeGuard,
        SessionStrings sessionStrings,
        ThreadTracker trackThread,
        QObject* parent);

    void setAvailabilityCheck(AvailabilityCheck check);
    void setDirectConnectivityCheck(DirectConnectivityCheck check);

    // The probe period. `intervalMs` is a parameter rather than a constant because the first
    // probe only fires after a full interval, so a test that cannot shorten it can never
    // reach the watch at all. Production callers leave it at the session default.
    static constexpr int kDefaultIntervalMs = 30000;
    void setIntervalMs(int intervalMs);

    // Armed when the session reaches Proxying: resets the policy, so a fresh activation has
    // to earn its warning from zero rather than inheriting the previous attempt's count.
    void arm();

    // Disarmed whenever the session leaves Proxying. Stops both timers and forgets the
    // in-flight flags; a probe already on a worker is dropped by the lifetime guard.
    void cancel();

    // The session's outbound gate has one branch that comes up anyway: the node answered
    // nothing *and* this machine cannot reach the probe hosts directly. That branch raises a
    // warning the watch itself can never retract (the node is known fine; the sentence is
    // about the local network). This call tells the watchdog the sentence is up, which arms
    // the direct-connectivity retraction on the watch's own cadence.
    void notifyWaivedOutboundWarningShown();

    // A fresh activation supersedes whatever the previous attempt concluded about the local
    // network: the sentence describing the old core's gate must not describe the new one.
    void notifyFreshActivation();

    bool waivedOutboundWarningActive() const { return waivedOutboundWarningShown_; }

    // The check the session's outbound gate consults after the gate has already failed: the
    // injected check when a host supplied one, otherwise the real probe. Returned as a copy
    // so the caller can carry it into a worker lambda without the worker touching this
    // object -- the same seam-copy rule the watch itself follows.
    DirectConnectivityCheck directConnectivityCheckForGate() const
    {
        if (directConnectivityCheck_) {
            return directConnectivityCheck_;
        }
        return sessionStrings_.fallbackDirectProbe;
    }

signals:
    // The sentence for the status bar, already localized. Empty means "nothing to show";
    // the session forwards it to setServerWarning, which deduplicates unchanged values.
    void warningChanged(const QString& warning);
    void logMessage(const QString& message);

private:
    void scheduleWatch();
    void scheduleDirectRecheck();
    void runWatch();
    void finishWatch(const OperationResult& result);
    void runDirectRecheck();
    void finishDirectRecheck(bool directReachable);
    Config currentProbeConfig() const;

    std::weak_ptr<char> sessionLifetimeGuard_;
    SessionStrings sessionStrings_;
    ThreadTracker trackThread_;
    AvailabilityCheck availabilityCheck_;
    DirectConnectivityCheck directConnectivityCheck_;
    ProxyHealthWatchPolicy policy_;
    int intervalMs_ = kDefaultIntervalMs;

    QTimer* watchTimer_ = nullptr;
    QTimer* directRecheckTimer_ = nullptr;
    bool watchInProgress_ = false;
    bool directRecheckInProgress_ = false;
    // Set only while the status bar shows the "cannot reach the probe hosts directly either"
    // waiver. That sentence describes the local network, not the node, so nothing in the
    // health watch ever retracts it -- the dedicated recheck does, and only when the machine
    // can reach the probe hosts again.
    bool waivedOutboundWarningShown_ = false;
    // Stays false while the session is not Proxying: a probe before the core is confirmed
    // listening says nothing about the node, and after the session leaves Proxying the
    // core-exit path owns the reporting.
    std::atomic_bool active_{false};
    std::atomic_bool shuttingDown_{false};
};
