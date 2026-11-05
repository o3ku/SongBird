#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>

#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "appcore/BackgroundTaskCoordinator.h"
#include "appcore/BackgroundThreadTracker.h"
#include "appcore/CoreStartupCheckpoint.h"
#include "appcore/CoreStartupChecklist.h"
#include "appcore/PostStopAction.h"
#include "appcore/ProxyCrashRestartPolicy.h"
#include "appcore/ProxyHealthWatchdog.h"
#include "appcore/ProxyRuntimeInterfaces.h"
#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"
#include "common/SystemProxySnapshot.h"
#include "appcore/SystemProxyOwnershipPolicy.h"

// Which event is undoing a system-proxy change. Only used to pick the log sentence: the two
// callers differ in what the user was doing, not in what has to happen to the registry.
enum class SystemProxyUndoTrigger {
    CoreStopped,
    StartupFailed
};
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"
#include "runtime/CoreLaunchCompatDecision.h"
#include "runtime/ICoreProcessHost.h"

class ClientConfigWriter;
class OutboundLivenessProbeService;
class OutboundLocationProbeService;
class QThread;
class QTimer;
class StartupResourceBroker;
class ProxySession final : public QObject
{
    Q_OBJECT

public:
    enum class Phase {
        Stopped,
        EnvironmentCleanup,
        ValidateCoreApplication,
        // Runtime config generation needs the resolved core info, and rule-set
        // validation parses the generated config, so the core config is
        // validated before runtime resources.
        ValidateCoreConfig,
        ValidateRuntimeResources,
        StartTunRuntime,
        StartCoreProcess,
        // The startup gate. Kept separate from ApplySystemProxy because the outbound check is the
        // one step that answers "does traffic actually leave this machine", and a failure there has
        // to be reported before the system proxy is touched.
        CheckOutboundConnection,
        ApplySystemProxy,
        Proxying,
        Stopping
    };

    struct Dependencies {
        ICoreProcessHost& mainCore;
        ICoreProcessHost& auxiliaryCore;
        ClientConfigWriter& configWriter;
        // Display only. The location is decoration for the status bar; it decides nothing, and a
        // failure here must never fail the startup.
        OutboundLocationProbeService& locationProbe;
        // The gate. A generate_204 host answering through the local proxy is the cheapest evidence
        // that the outbound works, and it is the only signal startup is allowed to fail on.
        OutboundLivenessProbeService& livenessProbe;
        BackgroundTaskCoordinator& backgroundTasks;
        IRuntimeProfileResolver& profileResolver;
        IRuntimeEnvironment& environment;
        IProxyActivationCoordinator& activationCoordinator;
    };

    struct StartRequest {
        Config config;
        QList<CoreType> existingCoreTypes;
        bool skipTunCleanup = false;
        bool showOverlay = false;
        bool warnOnFailure = false;
    };

    // Asks the user whether to launch a protocol on a core other than the one
    // stored in settings. Returning false aborts the startup. Headless hosts
    // leave this unset, which accepts the switch without prompting.
    using CoreSwitchConfirmation = std::function<bool(const CoreLaunchCompatDecision&)>;

    explicit ProxySession(Dependencies deps, QObject* parent = nullptr);
    ~ProxySession() override;

    void setCoreSwitchConfirmation(CoreSwitchConfirmation confirmation);

    // Runs the node-availability probe. Injected rather than constructed in here for two reasons:
    // a host can leave it unset, which is how the auto front end opts out (it already runs its own
    // health check and must not get a second one), and a test can supply a scripted answer without
    // standing up a proxy.
    //
    // Called on a background thread, so the implementation must not touch this object. It gets the
    // whole config rather than a `localPort + 1` computed here: resolving the HTTP inbound is
    // common/LocalInboundPorts' job, and the session used to guess it.
    using AvailabilityCheck = std::function<OperationResult(const Config& config)>;

    // The probe period. `intervalMs` is a parameter rather than a constant because the first probe
    // only fires after a full interval, so a test that cannot shorten it can never reach the watch
    // at all -- the same reason ThreadShutdownBudget is injectable. Production callers leave it.
    static constexpr int kDefaultHealthWatchIntervalMs = ProxyHealthWatchdog::kDefaultIntervalMs;
    void setAvailabilityCheck(
        AvailabilityCheck check,
        int intervalMs = kDefaultHealthWatchIntervalMs);

    // The unproxied cross-check the gate consults only after it has already failed: true means
    // "this machine can still reach the probe hosts on its own", which is what makes the failure
    // the node's fault rather than the local network's. Injected for the same reason
    // AvailabilityCheck is -- a test cannot stand up the public internet, so without a seam the
    // fail-open branch would be decided by whatever network the machine running the test happens
    // to have. Unset means the real probe is used.
    //
    // Called on a background thread, so the implementation must not touch this object.
    using DirectConnectivityCheck = std::function<bool(int timeoutMs)>;
    void setDirectConnectivityCheck(DirectConnectivityCheck check);

    void start(const StartRequest& request);
    // Issue #8: SongBirdAuto owns its TUN runtime and starts it before the session, so a TUN
    // failure can abort a start without ProxySession::start() ever running -- while the
    // registry still holds a proxy an earlier round of this session applied. This entry runs
    // the ownership decision a failed startup would have run (decideFailedStartupProxyAction,
    // undo) without touching the core or the checklist: there is no core to stop.
    void undoManagedSystemProxyAfterAbortedStartup();

    void stop(bool immediate = false);
    void restartIfRunning(const StartRequest& request);
    void switchServer(const QString& indexId, bool enableTun, bool showOverlay = false);
    void stopForCoreUpdate();

    Phase phase() const;
    bool isActive() const;
    bool isCoreRunning() const;
    bool isActivationInProgress() const;
    bool isTransitioning() const;
    QString serverLocation() const;
    // ISO country code of the current exit (from the display probe; empty when unknown).
    QString serverCountryCode() const;
    QString serverWarning() const;
    // The short handle this activation's log lines carry. One activation interleaves its own gate,
    // location and core-output lines with the next one's, and the timestamp is the only thing that
    // would otherwise tie a core's DNS error to the attempt that provoked it. Empty until the first
    // activation starts; it keeps its last value across a stop, so a core line that arrives while
    // the process is still tearing down still names the attempt that produced it.
    QString activationId() const;
    // Whether the outbound was confirmed reachable in this activation -- or explicitly waived by the
    // user's skip-outbound setting, which is the only way this is true without a probe having
    // answered. This -- not the location string -- is what the UI state uses to decide the proxy is
    // really up, so a node that answers nothing can never be reported as active just because its geo
    // lookup happened to succeed.
    bool outboundReachable() const;
    void setServerWarning(QString warning);
    bool isManagedProxyActive() const;
    void adoptManagedSystemProxy(bool active);
    void requestChecklistOverlay();
    // Deliberately not gated on QT_TESTLIB_LIB: ProxySession.cpp is compiled once
    // into songbird_shared_objs and linked by both the app and the test targets, so
    // a member that only exists under a test-only macro would be missing from
    // whichever side did not define it.
    void handleCoreExitedForTest(int exitCode, QProcess::ExitStatus status, bool stopRequested, bool auxiliary)
    {
        handleCoreExited(exitCode, static_cast<int>(status), stopRequested, auxiliary);
    }
    // The entry the crash-restart timer uses (scheduleCoreRestart's singleShot body calls
    // startInternal with these two flags). Exposed for the same reason as the line above: a crash
    // loop cannot re-drive start(), because a live core and an armed restart timer make
    // rejectOverlappingStart() answer the request instead of starting anything.
    void startInternalForTest(bool skipTunCleanup, bool environmentAlreadyChecked, bool showStartupOverlay)
    {
        startInternal(skipTunCleanup, environmentAlreadyChecked, showStartupOverlay);
    }

signals:
    void phaseChanged(Phase phase);
    void checklistUpdated(const QStringList& items);
    void checklistCleared();
    void activated(const QString& location);
    // Announced when the exit location is detected, which now happens after the session is already
    // up. Kept separate from activated() because the two facts arrive at two different times: a
    // session that cannot say where it exits is still a working session.
    void serverLocationChanged(const QString& location, const QString& countryCode);
    // The undo write itself failed: the registry still holds whatever it held, and the message
    // says what (if anything) will retry. The log line inside the undo says the same thing to
    // the log; this signal exists because a user offline behind a dead proxy cannot see the log.
    // Display only -- receivers must not write the registry from it (issue #14, item A).
    void systemProxyUndoFailed(const QString& warning);
    void failed(const QString& reason);
    void stopped();
    void coreOutput(const QString& line);
    void auxiliaryCoreOutput(const QString& line);
    void coreUpdateResumeRequested();
    void serverSwitchResumeRequested(const QString& indexId, bool enableTun, bool showOverlay);
    void logMessage(const QString& message);
    void statusSyncRequested();

private:
    enum class TunCleanupState {
        Idle,
        Cleaning,
        CleaningThenResume
    };

    struct StartupSteps {
        QString environment;
        QString downloadCore;
        QString runtimeResources;
        QString validateConfig;
        QString tunRuntime;
        QString startCore;
    };

    struct StartupContext {
        StartupSteps steps;
        bool skipTunCleanup = false;
        bool environmentAlreadyChecked = false;
        bool showStartupOverlay = false;
    };

    struct StartupProfile {
        VmessItem server;
        CoreType launchCore = CoreType::Unknown;
        CoreInfo coreInfo;
        QString coreConfigPath;
    };

    static StartupSteps startupSteps();

    void setPhase(Phase phase);
    void startInternal(
        bool skipTunCleanup,
        bool environmentAlreadyChecked = false,
        bool showStartupOverlay = false);
    void ensureChecklistPrepared(const StartupContext& context);
    bool rejectOverlappingStart(const StartupContext& context);
    void beginStartupAttempt(const StartupContext& context);
    bool ensureTunCleanupCompleted(const StartupContext& context);
    std::optional<StartupProfile> resolveStartupProfile(const StartupContext& context);
    void beginActivation();
    void finishActivation();
    void cancelActivation();

    void prepareChecklist(bool showOverlay);
    void setCheckpointStatus(
        CoreStartupCheckpointStatus status,
        const QString& step,
        const QString& detail = {});
    void clearChecklist();
    void clearChecklistAfterStableRun(int delayMs);
    void keepChecklistForUserDismissal(const QString& step, const QString& detail);

    void failStartup(const OperationResult& result);
    void cleanupAfterFailedStartup();

    // Thin entries over StartupResourceBroker, the QObject child that owns the
    // download-and-resume paths (geo files, sing-box rule sets, core binaries). The call sites
    // inside startInternal/resolveStartupProfile keep their names; the broker supplies the
    // workers, the progress posting and the shared finish handler, and hands the resume back
    // through a hook whose body is startInternal(skipTunCleanup, true, showStartupOverlay).
    // Phase writes stay here: the broker performs them through an injected callback, so the
    // session remains the single writer of the phase machine. See StartupResourceBroker.h for
    // the threading contract and the resume-generation analysis.
    void downloadMissingGeoFilesAndResume(
        const CoreInfo& coreInfo,
        const QString& geoValidationMessage,
        const QString& checkGeoStep,
        bool skipTunCleanup,
        bool showStartupOverlay);
    void downloadMissingSingBoxRuleSetsAndResume(
        const QStringList& missingRuleSetTags,
        const QString& checkGeoStep,
        bool skipTunCleanup,
        bool showStartupOverlay);
    void downloadMissingCoreAndResume(
        CoreType coreType,
        const OperationResult& missingCoreResult,
        const QString& downloadCoreStep,
        bool skipTunCleanup,
        bool showStartupOverlay);

    void handleCoreStarted(const QString& serverIndexId);
    void validateCoreListeningAndHandleStarted(
        const QString& serverIndexId,
        bool customServer);
    void handleCoreStartFailed(const QString& message);
    void handleMainCoreOutput(const QString& line);

    void queryOutboundConnection(const QString& serverIndexId);
    // True when this machine can reach the probe hosts without going through the core. Runs on a
    // worker thread, so it reads the injected check and the probe service and nothing else.
    bool directConnectivityReachable() const;
    void queryServerLocation(
        const QString& serverIndexId);
    bool applySystemProxyAfterOutboundCheck();
    void stopInternal(bool immediate, bool clearPostStopAction);
    void handleCoreExited(int exitCode, int status, bool stopRequested, bool auxiliary);
    void scheduleCoreStartAfterStop(bool showStartupOverlay);
    void clearAppliedProxyAfterStopped();
    void clearProxyStateAfterStopped();

    void setTunCleanupState(TunCleanupState state);
    void removeStaleTunAdapterAsync(const std::function<void(const OperationResult&)>& completion);
    void removeStaleTunAdapter();
    void stopAuxiliaryCore(bool immediate = false);

    void scheduleCoreRestart(const QString& reason, bool auxiliary, int delayMs = 3000);
    void cancelPendingCoreRestarts();

    void setServerLocation(QString location, QString countryCode);
    void clearServerLocation();

    bool updateSystemProxyMode(SystemProxyMode mode) const;
    // Restores the captured snapshot, or clears when there is none. `trigger` picks the log line.
    bool undoSystemProxyChange(SystemProxyUndoTrigger trigger);
    void trackBackgroundThread(QThread* thread);

    Dependencies deps_;
    Phase phase_ = Phase::Stopped;
    PostStopAction postStopAction_;
    TunCleanupState tunCleanupState_ = TunCleanupState::Idle;
    CoreSwitchConfirmation coreSwitchConfirmation_;

    StartRequest currentRequest_;
    VmessItem currentServer_;
    QString currentServerLocation_;
    // ISO country code of the last location probe (empty when unknown). Kept beside the display
    // string so the "fix location from exit" action can use the probe's answer without the UI
    // parsing the formatted location text.
    QString currentServerCountryCode_;
    QString currentServerWarning_;
    // Regenerated only when a fresh activation begins, so the resume paths that re-enter
    // startInternal mid-activation keep the id their earlier log lines were written under.
    QString activationId_;

    bool coreTunEnabledAtStart_ = false;
    bool coreTunAdapterConflictDetected_ = false;
    bool managedSystemProxyActive_ = false;
    bool outboundReachable_ = false;
    bool skipTunCleanupOnNextCoreExit_ = false;
    qint64 coreStartTime_ = 0;

    QTimer* coreRestartTimer_ = nullptr;
    QTimer* auxiliaryRestartTimer_ = nullptr;
    // The identity of the armed scheduleCoreStartAfterStop() lambda. Bumped by stopInternal() and
    // cleanupAfterFailedStartup() so a resume armed before either of them cannot run afterwards.
    quint64 pendingResumeId_ = 0;

    // Every piece of health-watch state lives in this satellite object (two timers, the
    // policy, the in-flight flags, the two injected probes). It is a QObject child of the
    // session rather than a member of AppBootstrapObjects: the session outlives it by
    // construction, so the load-bearing member order in AppBootstrapObjects.h does not gain
    // a new entry, and it dies with the session in every teardown path.
    ProxyHealthWatchdog* healthWatch_ = nullptr;
    // The download-and-resume satellite (geo files, rule sets, core binaries). Same shape as
    // healthWatch_: a QObject child of this session, so it dies with it in every teardown path
    // and the AppBootstrapObjects member order gains nothing. It holds no state of its own --
    // only the injected seams -- so there is no cancel()/arm() to call from the phase machine;
    // an in-flight worker's continuation retires itself through the phase check in the finish
    // handler (see StartupResourceBroker.h).
    StartupResourceBroker* resourceBroker_ = nullptr;
    ProxyCrashRestartPolicy crashRestartPolicy_;

    CoreStartupChecklist checklist_;

    BackgroundThreadTracker backgroundThreads_;
    std::shared_ptr<char> lifetimeGuard_ = std::make_shared<char>();
    std::atomic_bool shuttingDown_{false};
};
