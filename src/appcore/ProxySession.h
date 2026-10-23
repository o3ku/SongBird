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
#include "appcore/ProxyHealthWatchPolicy.h"
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
    static constexpr int kDefaultHealthWatchIntervalMs = 30000;
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

signals:
    void phaseChanged(Phase phase);
    void checklistUpdated(const QStringList& items);
    void checklistCleared();
    void activated(const QString& location);
    // Announced when the exit location is detected, which now happens after the session is already
    // up. Kept separate from activated() because the two facts arrive at two different times: a
    // session that cannot say where it exits is still a working session.
    void serverLocationChanged(const QString& location);
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
    void postStartupDownloadProgress(
        const QString& step,
        const QString& message,
        const std::weak_ptr<char>& guard);
    void postStartupDownloadProgress(
        const QString& step,
        const QString& message,
        const BackgroundTaskCoordinator::Token& token,
        const std::weak_ptr<char>& guard);
    void handleStartupDownloadFinished(
        const QString& step,
        const OperationResult& result,
        bool skipTunCleanup,
        bool showStartupOverlay,
        const QString& canceledMessage,
        const QString& completedButCanceledMessage,
        const std::function<void()>& successAction = {});

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

    void scheduleHealthWatch();
    void runHealthWatch();
    void finishHealthWatch(const OperationResult& result);
    void cancelHealthWatch();
    // Re-probes direct connectivity while a warning that was caused by it is still on screen, and
    // drops the warning once the machine can reach the probe hosts again. Bounded in the worker.
    void recheckWaivedOutboundWarning();
    void runDirectRecheck();
    void finishDirectRecheck(bool directReachable);

    void setServerLocation(QString location);
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
    QTimer* healthWatchTimer_ = nullptr;
    ProxyCrashRestartPolicy crashRestartPolicy_;
    ProxyHealthWatchPolicy healthWatchPolicy_;
    AvailabilityCheck availabilityCheck_;
    DirectConnectivityCheck directConnectivityCheck_;
    int healthWatchIntervalMs_ = kDefaultHealthWatchIntervalMs;
    bool healthWatchInProgress_ = false;
    // Set only while the status bar is showing the "cannot reach the probe hosts directly either"
    // waiver. That sentence describes the local network, not the node, so nothing in the health
    // watch ever retracts it -- it used to survive for the rest of the session even after the
    // network came back.
    bool waivedOutboundWarningShown_ = false;
    bool directRecheckInProgress_ = false;
    QTimer* directRecheckTimer_ = nullptr;

    CoreStartupChecklist checklist_;

    BackgroundThreadTracker backgroundThreads_;
    std::shared_ptr<char> lifetimeGuard_ = std::make_shared<char>();
    std::atomic_bool shuttingDown_{false};
};
