#include "appcore/ProxyHealthWatchdog.h"

#include <utility>

#include <QMetaObject>
#include <QThread>
#include <QTimer>

#include "appcore/OutboundLivenessProbeService.h"
#include "common/BackgroundThreadLaunch.h"
#include "domain/models/Config.h"

// Translation context: every sentence the watchdog emits already exists in the .ts under the
// "ProxySession" context. Rather than duplicating tr() calls here under a new context -- which
// would orphan the existing entries and need a retranslation -- the session hands over the two
// retraction sentences and the activation tag as finished strings (see SessionStrings, which
// the constructor takes). Everything else this class logs is produced by
// ProxyHealthWatchPolicy, which already translates through the session's context. This keeps
// the split translation-neutral: the .ts and the compiled .qm are untouched by it.

ProxyHealthWatchdog::ProxyHealthWatchdog(
    std::weak_ptr<char> sessionLifetimeGuard,
    SessionStrings sessionStrings,
    ThreadTracker trackThread,
    QObject* parent)
    : QObject(parent)
    , sessionLifetimeGuard_(std::move(sessionLifetimeGuard))
    , sessionStrings_(std::move(sessionStrings))
    , trackThread_(std::move(trackThread))
{
}

void ProxyHealthWatchdog::setAvailabilityCheck(AvailabilityCheck check)
{
    availabilityCheck_ = std::move(check);
}

void ProxyHealthWatchdog::setDirectConnectivityCheck(DirectConnectivityCheck check)
{
    directConnectivityCheck_ = std::move(check);
}

void ProxyHealthWatchdog::setIntervalMs(int intervalMs)
{
    intervalMs_ = intervalMs > 0 ? intervalMs : kDefaultIntervalMs;
}

void ProxyHealthWatchdog::arm()
{
    // A fresh activation supersedes whatever the previous attempt concluded: any warning that is
    // up describes a core that is no longer the one running. The policy is reset with it, so the
    // new watch has to earn its warning again from zero rather than inheriting a count.
    policy_.reset();
    active_.store(true);
    scheduleWatch();
}

void ProxyHealthWatchdog::cancel()
{
    active_.store(false);
    if (watchTimer_ != nullptr) {
        watchTimer_->stop();
    }
    if (directRecheckTimer_ != nullptr) {
        directRecheckTimer_->stop();
    }
    watchInProgress_ = false;
    policy_.reset();
}

void ProxyHealthWatchdog::notifyWaivedOutboundWarningShown()
{
    waivedOutboundWarningShown_ = true;
    scheduleDirectRecheck();
}

void ProxyHealthWatchdog::notifyFreshActivation()
{
    // A fresh start owns the status bar again: whatever the previous session concluded about the
    // local network is replaced by this session's own gate.
    waivedOutboundWarningShown_ = false;
    directRecheckInProgress_ = false;
    if (directRecheckTimer_ != nullptr) {
        directRecheckTimer_->stop();
    }
}

void ProxyHealthWatchdog::scheduleWatch()
{
    if (shuttingDown_.load() || !active_.load() || !availabilityCheck_) {
        return;
    }
    if (watchTimer_ == nullptr) {
        watchTimer_ = new QTimer(this);
        watchTimer_->setSingleShot(true);
        QObject::connect(watchTimer_, &QTimer::timeout, this, [this]() { runWatch(); });
    }
    watchTimer_->start(intervalMs_);
}

Config ProxyHealthWatchdog::currentProbeConfig() const
{
    // The whole config rather than a `localPort + 1` computed here: resolving the HTTP inbound
    // is common/LocalInboundPorts' job, and the session used to guess it. The provider is
    // read on the GUI thread, before the worker is launched.
    if (sessionStrings_.probeConfig) {
        return sessionStrings_.probeConfig();
    }
    return {};
}

// --- Waived outbound warning retraction -------------------------------------
//
// The gate has one branch that comes up anyway: the node answered nothing *and* this machine
// cannot reach the probe hosts directly, which makes the node innocent and the local network
// the suspect. That branch raises a warning and lets the session run.
//
// Nothing used to retract it. The health watch only speaks for the node, and this sentence is
// about the machine, so a user who fixed their network (joined a different Wi-Fi, restored a
// VPN) kept reading "this machine cannot reach the probe hosts directly either" for the rest
// of the session and had to restart the proxy to be told the truth. This is the retraction:
// while that specific warning is up, the same unproxied probe runs again on the watch's
// cadence, and the warning is dropped the moment it answers.

void ProxyHealthWatchdog::scheduleDirectRecheck()
{
    if (!waivedOutboundWarningShown_ || shuttingDown_.load() || !active_.load()) {
        return;
    }
    if (directRecheckTimer_ == nullptr) {
        directRecheckTimer_ = new QTimer(this);
        directRecheckTimer_->setSingleShot(true);
        QObject::connect(directRecheckTimer_, &QTimer::timeout, this, [this]() { runDirectRecheck(); });
    }
    directRecheckTimer_->start(intervalMs_);
}

void ProxyHealthWatchdog::runWatch()
{
    if (shuttingDown_.load() || !active_.load() || watchInProgress_) {
        return;
    }

    // Copy the seam before leaving the main thread: the worker must not read a member of an object
    // it does not own, and the host may clear it at any time.
    const AvailabilityCheck check = availabilityCheck_;
    if (!check) {
        return;
    }

    const Config config = currentProbeConfig();

    watchInProgress_ = true;    const std::weak_ptr<char> guard = sessionLifetimeGuard_;
    QThread* thread = launchBackgroundThread(
        [this, guard, check, config]() {
            const OperationResult result = check(config);
            QMetaObject::invokeMethod(this, [this, result, guard]() {
                if (guard.expired()) {
                    return;
                }
                finishWatch(result);
            }, Qt::QueuedConnection);
        },
        [this](QThread* worker) {
            if (trackThread_) {
                trackThread_(worker);
            }
        });
    thread->start();
}

void ProxyHealthWatchdog::finishWatch(const OperationResult& result)
{
    watchInProgress_ = false;
    if (shuttingDown_.load() || !active_.load()) {
        return;
    }

    // The probe's own message is the reason appended to the warning. It is already localized by the
    // service that produced it, so the warning stays one language.
    const ProxyHealthWatchPolicy::Decision decision =
        policy_.recordResult(result.success, result.message);

    switch (decision.action) {
    case ProxyHealthWatchPolicy::Action::Warn:
        emit warningChanged(decision.message);
        emit logMessage(decision.message);
        break;
    case ProxyHealthWatchPolicy::Action::Clear:
        // Clearing means the node answered again, so the warning the user is looking at is no
        // longer true. The sentence still goes to the log, because a warning that disappears
        // without a trace reads as a glitch.
        emit warningChanged(QString());
        emit logMessage(decision.message);
        break;
    case ProxyHealthWatchPolicy::Action::None:
        break;
    }

    scheduleWatch();
}

void ProxyHealthWatchdog::runDirectRecheck()
{
    if (!waivedOutboundWarningShown_ || shuttingDown_.load() || !active_.load()
        || directRecheckInProgress_) {
        return;
    }

    // Copy the seam before leaving the main thread, same rule as runWatch. The fallback is
    // the session's real probe: the watchdog has no dependencies of its own to reach it.
    const DirectConnectivityCheck check = directConnectivityCheck_;
    const DirectConnectivityCheck fallback = sessionStrings_.fallbackDirectProbe;
    const int timeoutMs = OutboundLivenessProbeService::LivenessProbeDirectTimeoutMs;

    directRecheckInProgress_ = true;
    const std::weak_ptr<char> guard = sessionLifetimeGuard_;
    QThread* thread = launchBackgroundThread(
        [this, guard, check, fallback, timeoutMs]() {
            // Unproxied, and therefore independent of this object: the worker reads nothing but
            // the copied checks and the timeout, so a watchdog torn down mid-probe only loses
            // the answer.
            const bool directReachable = check ? check(timeoutMs) : fallback(timeoutMs);
            QMetaObject::invokeMethod(this, [this, guard, directReachable]() {
                if (guard.expired()) {
                    return;
                }
                finishDirectRecheck(directReachable);
            }, Qt::QueuedConnection);
        },
        [this](QThread* worker) {
            if (trackThread_) {
                trackThread_(worker);
            }
        });
    thread->start();
}

void ProxyHealthWatchdog::finishDirectRecheck(bool directReachable)
{
    directRecheckInProgress_ = false;
    if (shuttingDown_.load() || !active_.load() || !waivedOutboundWarningShown_) {
        return;
    }

    if (!directReachable) {
        // Still no route without the core. The warning stays and the retraction keeps trying --
        // a local outage is exactly the case the user expects to be told about once and then left
        // alone until it ends.
        emit logMessage(sessionStrings_.recheckStillUnavailableSentence());
        scheduleDirectRecheck();
        return;
    }

    // The sentence is no longer true, so it goes. Logged because a warning that disappears without
    // a trace reads as a glitch.
    waivedOutboundWarningShown_ = false;
    emit warningChanged(QString());
    emit logMessage(sessionStrings_.recheckAvailableAgainSentence());
}
