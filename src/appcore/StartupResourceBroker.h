#pragma once

#include <functional>
#include <memory>

#include <QObject>
#include <QString>
#include <QStringList>

#include "appcore/BackgroundTaskCoordinator.h"
#include "appcore/CoreStartupCheckpoint.h"
#include "appcore/ProxySession.h"
#include "appcore/ProxyRuntimeInterfaces.h"
#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"

class QThread;
struct CoreUpdateConfig;

// The download-and-resume paths that used to live inside ProxySession, split out along the same
// ownership boundary as ProxyHealthWatchdog (see that class for the pattern): a QObject child of
// the session, constructed by it, holding no state of its own beyond the injected seams. Three
// "download what is missing, then re-enter the startup" flows live here -- geo database files,
// sing-box rule sets, and a whole core binary -- plus the progress posting and the shared
// finish handler they resume through.
//
// What this class deliberately does NOT own:
//   * The phase machine. Phase has exactly one writer, ProxySession; every setPhase the moved
//     code performs goes back through SessionHooks::setPhase, preserving the original ordering
//     relative to the checkpoint writes around it (see each call site).
//   * The resume itself. The worker's finished path ends in SessionHooks::resumeStartup, whose
//     session-side body is startInternal(skipTunCleanup, /*environmentAlreadyChecked=*/true,
//     showStartupOverlay) -- the same single entry every other resume uses.
//   * failStartup / checklist bookkeeping / cancellation / logging: session concerns, reached
//     only through the hooks.
//
// Threading contract, inherited unchanged from the code that lived in ProxySession:
//   * Every seam is copied before a worker launches (the config snapshot for the core download
//     is taken by the session at call time), so a worker never reads a member of the session.
//   * Workers post their answers back with QMetaObject::invokeMethod(this, ...) plus the
//     session's lifetime guard, so a broker destroyed mid-download only loses the answer. The
//     guard is the session's because the session outlives the broker (QObject parenting) --
//     expiring the session's guard already covers every case in which the broker is gone too.
//   * Worker threads are registered through SessionHooks::trackThread so the session's bounded
//     shutdown machinery still sees them.
//
// Resume-reentry and ProxySession::pendingResumeId_ (verified against the session code before
// this split; re-read both sides before changing either):
//
//   pendingResumeId_ is the generation counter that keeps a resume from outliving the attempt
//   that armed it: stopInternal() and cleanupAfterFailedStartup() both ++pendingResumeId_, and
//   scheduleCoreStartAfterStop()'s singleShot timer compares its captured id against the
//   counter before resuming. That timer is the counter's only reader, and this broker never
//   arms it -- the broker cannot bypass the generation check because it never touches the
//   machinery the check guards, and adds no second entry into the startup flow. What actually
//   retires an in-flight download after a stop or a failed startup is the phase machine, read
//   at continuation time:
//     * stopInternal() sets the phase to Stopping or Stopped before doing anything else, and
//       ends with cancelActivation(), so by the time control returns to the GUI thread's event
//       loop the phase is outside the startup range.
//     * cleanupAfterFailedStartup() likewise ends at Stopped (or Stopping while a core is still
//       being stopped, which handleCoreExited then finishes; Stopping is outside the startup
//       range too).
//   A download continuation that arrives after either therefore finds
//   SessionHooks::isActivationInProgress() false and handleStartupDownloadFinished takes the
//   cancel branch (keepChecklistForUserDismissal + cancelActivation + statusSync) instead of
//   resuming. "Arrives afterwards" is well-defined because both bump sites and the queued
//   continuation run on the GUI thread. A resume that does run re-enters through
//   SessionHooks::resumeStartup -> startInternal, which still applies ensureChecklistPrepared,
//   rejectOverlappingStart (refuses while a core is running or the phase is Stopping) and
//   beginStartupAttempt exactly as before the split.
class StartupResourceBroker final : public QObject
{
    Q_OBJECT

public:
    // The services the moved code drives directly. References, not copies: they are the
    // session's own Dependencies members, and the session outlives this child object.
    struct Dependencies {
        IRuntimeProfileResolver& profileResolver;
        BackgroundTaskCoordinator& backgroundTasks;
        IProxyActivationCoordinator& activationCoordinator;
    };

    // What only the session may do, handed over as callbacks (the ProxyHealthWatchdog
    // SessionStrings pattern applied to behaviour as well as text). Every hook runs on the GUI
    // thread except where noted.
    struct SessionHooks {
        // The one seam phase writes go through: ProxySession stays the single writer of the
        // phase machine.
        std::function<void(ProxySession::Phase)> setPhase;
        std::function<void(CoreStartupCheckpointStatus, const QString&, const QString&)> setCheckpointStatus;
        std::function<void(const QString&, const QString&)> keepChecklistForUserDismissal;
        std::function<void()> cancelActivation;
        std::function<void(const OperationResult&)> failStartup;
        // The resume: startInternal(skipTunCleanup, /*environmentAlreadyChecked=*/true,
        // showStartupOverlay) on the session.
        std::function<void(bool skipTunCleanup, bool showStartupOverlay)> resumeStartup;
        std::function<bool()> isActivationInProgress;
        // Reads the session's shutdown flag. May be called from the worker thread (the core
        // download's cancel check polls it), so the implementation reads atomic state only.
        std::function<bool()> shuttingDown;
        // The config's system-proxy mode, read at finish time on the GUI thread. The download
        // finishes "completed but canceled" unless the user asked for the managed proxy.
        std::function<SystemProxyMode()> configuredSystemProxyMode;
        std::function<void()> requestStatusSync;
        std::function<void(const QString&)> logMessage;
        std::function<void(QThread*)> trackThread;
    };

    // Every user-visible sentence the broker emits, already localized, supplied by the session
    // so the tr() calls stay in ProxySession.cpp and the .ts's "ProxySession" context entries
    // keep matching exactly. Parameterized sentences receive the values the broker computes
    // (native directories, counts, core display names).
    struct SessionStrings {
        std::function<QString()> coreDataDirectoryEmptySentence;
        std::function<QString(const QString& nativeDirectory)> geoDownloadStartSentence;
        std::function<QString()> geoCanceledSentence;
        std::function<QString()> geoCompletedButCanceledSentence;
        std::function<QString(int count, const QString& nativeDirectory)> ruleSetDownloadStartSentence;
        std::function<QString()> ruleSetCanceledSentence;
        std::function<QString()> ruleSetCompletedButCanceledSentence;
        std::function<QString(const QString& coreName, const QString& nativeDirectory)> coreDownloadStartSentence;
        std::function<QString()> backgroundTaskBusySentence;
        std::function<QString(const QString& coreName)> coreDownloadBusyFailSentence;
        std::function<QString()> coreCanceledSentence;
        std::function<QString()> coreCompletedButCanceledSentence;
    };

    StartupResourceBroker(
        std::weak_ptr<char> sessionLifetimeGuard,
        Dependencies deps,
        SessionStrings sessionStrings,
        SessionHooks hooks,
        QObject* parent);

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
    // `updateConfig` is the config snapshot the worker needs (the pre-release and
    // geo-update-core flags), taken by the session on the GUI thread at call time -- a worker
    // must never read session members.
    void downloadMissingCoreAndResume(
        CoreType coreType,
        const OperationResult& missingCoreResult,
        const QString& downloadCoreStep,
        bool skipTunCleanup,
        bool showStartupOverlay,
        const CoreUpdateConfig& updateConfig);

private:
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

    std::weak_ptr<char> sessionLifetimeGuard_;
    Dependencies deps_;
    SessionStrings sessionStrings_;
    SessionHooks hooks_;
};
