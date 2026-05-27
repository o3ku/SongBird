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

#include "app/BackgroundTaskCoordinator.h"
#include "app/BackgroundThreadTracker.h"
#include "app/CoreStartupCheckpoint.h"
#include "app/CoreStartupChecklist.h"
#include "app/PostStopAction.h"
#include "app/ProxyCrashRestartPolicy.h"
#include "app/ProxyRuntimeInterfaces.h"
#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"
#include "runtime/ICoreProcessHost.h"

class ClientConfigWriter;
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
        ValidateRuntimeResources,
        ValidateCoreConfig,
        StartTunRuntime,
        StartCoreProcess,
        CheckOutboundLocation,
        ApplySystemProxy,
        Proxying,
        Stopping
    };

    struct Dependencies {
        ICoreProcessHost& mainCore;
        ICoreProcessHost& auxiliaryCore;
        ClientConfigWriter& configWriter;
        OutboundLocationProbeService& locationProbe;
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

    explicit ProxySession(Dependencies deps, QObject* parent = nullptr);
    ~ProxySession() override;

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
    void setServerWarning(QString warning);
    bool isManagedProxyActive() const;
    void adoptManagedSystemProxy(bool active);
    void requestChecklistOverlay();
#ifdef QT_TESTLIB_LIB
    void handleCoreExitedForTest(int exitCode, QProcess::ExitStatus status, bool stopRequested, bool auxiliary)
    {
        handleCoreExited(exitCode, static_cast<int>(status), stopRequested, auxiliary);
    }
#endif

signals:
    void phaseChanged(Phase phase);
    void checklistUpdated(const QStringList& items);
    void checklistCleared();
    void activated(const QString& location);
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

    void queryServerLocation(
        const QString& serverIndexId);
    bool applySystemProxyAfterLocation();
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

    void setServerLocation(QString location);
    void clearServerLocation();

    bool updateSystemProxyMode(SystemProxyMode mode) const;
    void trackBackgroundThread(QThread* thread);

    Dependencies deps_;
    Phase phase_ = Phase::Stopped;
    PostStopAction postStopAction_;
    TunCleanupState tunCleanupState_ = TunCleanupState::Idle;

    StartRequest currentRequest_;
    VmessItem currentServer_;
    QString currentServerLocation_;
    QString currentServerWarning_;

    bool coreTunEnabledAtStart_ = false;
    bool coreTunAdapterConflictDetected_ = false;
    bool managedSystemProxyActive_ = false;
    bool skipTunCleanupOnNextCoreExit_ = false;
    qint64 coreStartTime_ = 0;

    QTimer* coreRestartTimer_ = nullptr;
    QTimer* auxiliaryRestartTimer_ = nullptr;
    ProxyCrashRestartPolicy crashRestartPolicy_;

    CoreStartupChecklist checklist_;

    BackgroundThreadTracker backgroundThreads_;
    std::shared_ptr<char> lifetimeGuard_ = std::make_shared<char>();
    std::atomic_bool shuttingDown_{false};
};
