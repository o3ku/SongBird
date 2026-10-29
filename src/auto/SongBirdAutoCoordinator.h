#pragma once

#include <atomic>
#include <memory>
#include <utility>

#include <QList>
#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "appcore/AppRuntimeResolver.h"
#include "appcore/BackgroundTaskCoordinator.h"
#include "appcore/CoreProcessCleanupService.h"
#include "appcore/CoreDiscoveryService.h"
#include "appcore/FunctionRuntimeAdapters.h"
#include "appcore/OutboundLivenessProbeService.h"
#include "appcore/OutboundLocationProbeService.h"
#include "appcore/ProxySession.h"
#include "appcore/TunRuntimeService.h"
#include "common/ThreadShutdown.h"
#include "auto/AutoSelectionEngine.h"
#include "auto/AutoTypes.h"
#include "domain/models/Config.h"
#include "persistence/JsonConfigRepository.h"
#include "platform/ISystemProxyService.h"
#include "platform/PlatformServices.h" // PlatformSystemProxyService, the constructor's default
#include "runtime/ClientConfigWriter.h"
#include "runtime/QtCoreProcessHost.h"
#include "services/ServerService.h"
#include "services/SpeedTestRequestItem.h"
#include "services/SubscriptionService.h"

class QNetworkAccessManager;
class QThread;

class SongBirdAutoCoordinator final : public QObject
{
    Q_OBJECT

public:
    // `systemProxy` is the platform's registry writer. The default is what the executable
    // uses; the parameter exists so a test can inject a recording double, which is the only
    // way the coordinator's own teardown -- the half that ran a second, snapshot-blind
    // ForcedClear until issue #12 -- can be pinned from the outside.
    //
    // `mainCore` is the main core's process host, injected for the same kind of reason: a start
    // the coordinator should not have performed is only visible as a call on the host, and the
    // production host cannot report one without a real core binary. It is a borrowed pointer, not
    // an owned one: the caller wants to go on asking it, and it has to stay alive for exactly as
    // long as the session using it does. Null means build the production host, and initialize()
    // keeps whichever host it was given. Typed as the interface's defaulted base so a double can
    // stand in -- the concrete host is final, and a double is the whole point of the parameter.
    explicit SongBirdAutoCoordinator(
        QString configPath,
        std::unique_ptr<ISystemProxyService> systemProxy = std::make_unique<PlatformSystemProxyService>(),
        MainCoreHostBase* mainCore = nullptr,
        QObject* parent = nullptr);
    // The exit teardown is stopProxySession(true), and it is the session's ownership undo and
    // nothing else. There used to be a ForcedClear after it here (issue #12): a snapshot-blind
    // second undo, run over whatever the ownership undo had just decided. Do not put one back --
    // tests/SongBirdAutoCoordinatorStopTests.cpp now fails when you do.
    ~SongBirdAutoCoordinator() override;

    bool initialize();
    // The tail of initialize() that builds the session (and only that). Split out so the
    // teardown half of this class is reachable in a test without a config file, a core or
    // the network -- the wiring inside is the production one, nothing is stubbed.
    void createProxySession();
    QString configPath() const;
    Config currentConfig() const;
    QString selectedCountryCode() const;
    QString subscriptionUrlsText() const;
    QString autoSelectionStrategy() const;
    bool isRunning() const;
    bool isTunEnabled() const;
    // Test seams, same reason as ProxySession's: the TUN-abort path only runs on a machine where
    // TUN cannot start (no elevation, no sing-box, no adapter cleanup), so the coordinator's
    // rollback was not reachable from a test at all. Both are the production wiring, called by the
    // test instead of by start(); nothing is stubbed and the binary never calls them.
    void setTunRuntimeEnvironmentForTest(std::unique_ptr<IRuntimeEnvironment> environment);
    void startSessionForTest();
    // The coordinator's answers to the two events that end an attempt. Both are the production
    // lambdas, reached from the test instead of from the session signal, because the "stopped()
    // has already started the next attempt, and now the failure arrives" ordering cannot be
    // produced through ProxySession without a real core to stop: the only exits that schedule a
    // `stopped` are behind a running main core.
    void handleSessionStoppedForTest();
    void handleSessionFailedForTest(const QString& reason);
    // The test's half of arming one: production arms pendingStartAfterStop_ from three paths that
    // all need a running session (a failover switch, a manual switch, a country re-pick).
    void armPendingStartAfterStopForTest();
    // The two flags a start claims, for a case that drives the endings directly: nothing is
    // running, so there is nothing for the real start to claim them from.
    void setRunningForTest(bool running);
    void setBusyForTest(bool busy);
    // The user's interrupt, without the teardown stop() also performs: the teardown needs a
    // session that had a core, and the ending handlers do not.
    void stopRequestedForTest();
    bool isBusyForTest() const;

public slots:
    void setSelectedCountryCode(const QString& countryCode);
    void switchToCountry(const QString& countryCode);
    void setTunEnabled(bool enabled);
    void setAutoSelectionStrategy(const QString& strategy);
    bool saveRoutingSettings(
        const QList<RoutingItem>& routingItems,
        const QList<RoutingRule>& routingCustomRules,
        const QString& settingsRoutingRuleTabKey);
    void saveSubscriptionUrlsText(const QString& text);
    bool isProxyActive() const;
    void start();
    void stop();
    void updateSubscriptionsAndRetest();

signals:
    void logMessage(const QString& message);
    void taskSummaryMessage(const QString& message);
    void countrySummariesChanged(const QList<AutoCountrySummary>& countries);
    void nodeEvaluationsChanged(const QList<AutoNodeEvaluation>& evaluations);
    void selectedCountryChanged(const QString& countryCode);
    void runningChanged(bool running);
    void proxyActiveChanged(bool active);
    void busyChanged(bool busy);
    void activeServerChanged(const QString& serverId, const QString& serverName, const QString& countryDisplay, const QString& location, qint64 latencyMs);
    void statusMessageChanged(const QString& message);
    // A proxy-undo failure the user must see; it persists on the status bar until a new session
    // activates, because the transient status message is immediately overwritten by the stop's
    // own terminal status ("Stopped"). Empty string clears it.
    void proxyWarningChanged(const QString& warning);
    void subscriptionUrlsTextChanged(const QString& text);
    void tunEnabledChanged(bool enabled);
    void autoSelectionStrategyChanged(const QString& strategy);

private:
    void reloadConfig();
    OperationResult saveConfig();
    QString resolveCustomConfigDirectory() const;
    OperationResult replaceSubscriptionsFromUrls(const QStringList& urls);
    void ensureDefaultSubscriptions();
    QStringList bundledSubscriptionUrls() const;
    void refreshExistingCoreTypes();
    QList<CoreType> missingCoreTypesForEvaluation() const;
    QString resolveCoreInstallDirectory(CoreType coreType) const;
    void downloadMissingCoresThenContinue(
        const QList<CoreType>& coreTypes,
        const OperationResult& updateResult,
        quint64 operationId,
        bool allowSubscriptionUpdateBeforeTesting,
        bool preserveExistingEvaluations,
        bool allowNoUsableRecoveryUpdate);
    void continueAfterSubscriptionUpdate(
        const OperationResult& updateResult,
        quint64 operationId,
        bool allowCoreDownload,
        bool allowSubscriptionUpdateBeforeTesting,
        bool preserveExistingEvaluations,
        bool allowNoUsableRecoveryUpdate);
    QList<AutoCountrySummary> buildCountrySummaries(const QList<AutoNodeEvaluation>& evaluations) const;
    QList<SpeedTestRequestItem> buildEvaluationItems() const;
    QList<SpeedTestRequestItem> buildEvaluationItemsForCountry(const QString& countryCode) const;
    void refreshInferredCountrySummaries(const QList<SpeedTestRequestItem>& items, bool preserveExistingResults);
    void prepareStartFromCurrentConfig();
    void continueStartWithCurrentData(
        bool allowSubscriptionUpdateBeforeTesting,
        bool forceCountryTest,
        bool allowNoUsableRecoveryUpdate);
    void startBackgroundTestForSelectedCountry(bool forceTest);
    void mergeEvaluationResults(const QList<AutoNodeEvaluation>& results);
    void mergeEvaluationResult(const AutoNodeEvaluation& result);
    void maybeStartFirstAvailableNode(const AutoNodeEvaluation& evaluation);
    void maybeSwitchToBestServerAfterEvaluation();
    // Wall-clock ms at which the current node came up, or 0 when nothing has been activated this
    // session. Feeds the scoring policy's minimum hold window.
    qint64 activeServerSinceMs() const;
    bool isProxySessionRunningOrTransitioning() const;
    const VmessItem* findServerById(const QString& indexId) const;
    CoreType resolveLaunchCoreType(const VmessItem& server) const;
    CoreInfo resolveCoreInfo(const VmessItem& server) const;
    QString currentCountryBestServerId() const;
    QString currentCountryFirstAvailableServerId() const;
    bool hasUsableSelectedCountryNode() const;
    bool usesFirstAvailableStrategy() const;
    void startBestServerForSelectedCountry();
    void startSelectedServerForSelectedCountry();
    bool subscriptionUpdateRecentlyCompleted() const;
    bool selectedCountryTestRecentlyCompleted() const;
    void updateSubscriptionsAndRetest(bool forceUpdate);
    bool selectServer(const QString& indexId);
    void startProxySession();
    // Gives back every flag a start claimed when TUN fails before the session is ever asked to
    // start, and writes the status first: busyChanged listeners read the line setBusy() would
    // otherwise precede.
    void rollbackStartAfterTunFailure(const QString& status);
    // End of initialize(). Split out so the sequence stays readable as prepare -> session ->
    // announce: the log line, the country summaries and the startup proxy recovery all need
    // the session to exist already. Tests do not need it -- they build the session alone.
    void completeStartup();
    // The activation coordinator and its lambdas, shared by initialize() and
    // createProxySession()'s standalone path -- one wiring, two entries.
    void wireActivationCoordinator();
    void wireRuntimeEnvironment(FunctionRuntimeEnvironment& runtime);
    void stopProxySession(bool immediate);
    OperationResult startTunRuntime(bool relayToProxy);
    void stopTunRuntime(bool cleanupAdapter);
    bool syncTunRuntimeForCurrentState();
    // A killed process leaves our values in the registry with no one to undo them. The persisted
    // snapshot is the evidence they are ours: a registry that is on with no snapshot may be the
    // user's own proxy, and touching that is what the ownership rules exist to prevent.
    void recoverSystemProxyFromPreviousRun();
    QString writeTunRuntimeConfig(bool relayToProxy) const;
    CoreInfo resolveSingBoxCoreInfo() const;
    bool updateSystemProxyMode(SystemProxyMode mode) const;
    QString proxyExceptions() const;
    QString selectedCountryDisplayName() const;
    void scheduleHealthCheck(int delayMs = 30000);
    void schedulePeriodicRefresh();
    void runHealthCheck();
    void finishHealthCheck(const OperationResult& result);
    void handleUnavailableActiveServer(const QString& reason);
    void clearEvaluationState();
    void handleNoCompatibleServers();
    void normalizeSelectedCountryAfterRetest();
    void maybeRefreshWhenCountryLow();
    bool isSelectedCountryLow() const;
    QString nextFailoverServerId(const QString& failedServerId) const;
    bool switchToNextBestSameCountry(const QString& failedServerId);
    void finishStopped();
    void handleSessionStopped();
    void handleSessionFailed(const QString& reason);
    // Drops the "start again once the session is down" flag and reports whether it was set. Read
    // from every exit that ends the attempt: the flag has to be cleared by whoever sees the exit,
    // not only by `stopped()`, because a failure can arrive without one (restart budget spent)
    // or ahead of one (TUN cleanup, emitted synchronously before the scheduled `stopped()`).
    bool abandonPendingStartAfterStop();
    void setBusy(bool busy);
    void setRunning(bool running);
    // Re-announces the active server. Split out because the presentation is refreshed from two
    // places now: activation, and the exit location arriving later.
    void publishActiveServer(const QString& location);
    void setStatus(const QString& status);
    void setProxyWarning(const QString& warning);
    void log(const QString& message);
    void taskSummary(const QString& message);
    void trackThread(QThread* thread);
    void clearThreads();
    quint64 nextOperationId();
    bool isCurrentOperation(quint64 operationId) const;
    void invalidateOperations();

    QString configPath_;
    Config config_;
    std::unique_ptr<JsonConfigRepository> repository_;
    std::unique_ptr<ServerService> serverService_;
    std::unique_ptr<SubscriptionService> subscriptionService_;
    std::unique_ptr<QNetworkAccessManager> networkAccessManager_;
    std::unique_ptr<CoreDiscoveryService> coreDiscoveryService_;
    std::unique_ptr<CoreProcessCleanupService> coreCleanupService_;
    std::unique_ptr<TunRuntimeService> tunRuntimeService_;
    std::unique_ptr<AppRuntimeResolver> runtimeResolver_;
    std::unique_ptr<ClientConfigWriter> clientConfigWriter_;
    // Borrowed when the caller injected one (the test seam), owned when it is the production host.
    MainCoreHostBase* mainCore_ = nullptr;
    std::unique_ptr<QtCoreProcessHost> ownedMainCore_;
    std::unique_ptr<QtCoreProcessHost> auxiliaryCore_;
    std::unique_ptr<QtCoreProcessHost> tunCore_;
    std::unique_ptr<OutboundLocationProbeService> locationProbe_;
    std::unique_ptr<OutboundLivenessProbeService> livenessProbe_;
    std::unique_ptr<BackgroundTaskCoordinator> backgroundTasks_;
    std::unique_ptr<FunctionRuntimeEnvironment> runtimeEnvironment_;
    std::unique_ptr<FunctionProxyActivationCoordinator> activationCoordinator_;
    std::unique_ptr<ProxySession> proxySession_;
    std::unique_ptr<ISystemProxyService> systemProxyService_;

    QList<CoreType> existingCoreTypes_;
    QList<AutoNodeEvaluation> evaluations_;
    QList<AutoCountrySummary> countrySummaries_;
    // Node-selection scoring state and the decisions made from it (probe feeding, effective-score
    // ranking, the switch gate). It is cleared wherever evaluations_ above is cleared: the state
    // describes nodes that are about to be replaced, and a recycled indexId would otherwise
    // inherit another server's smoothed latency and breaker state.
    AutoSelectionEngine selectionEngine_;
    QString selectedCountryCode_;
    QString activeServerId_;
    QString proxyWarning_;
    bool running_ = false;
    bool busy_ = false;
    bool refreshQueuedAfterBusy_ = false;
    bool refreshQueuedAfterBusyForce_ = false;
    bool stopRequested_ = false;
    bool pendingStartAfterStop_ = false;
    bool pendingStartAfterEvaluation_ = false;
    bool switchToBestAfterEvaluation_ = false;
    bool forceCountryTestAfterSubscriptionUpdate_ = false;
    bool forceSubscriptionUpdateIfNoUsableAfterTest_ = false;
    bool noUsableRecoveryUpdateAttempted_ = false;
    bool tunRuntimeRelayToProxy_ = false;
    bool healthCheckInProgress_ = false;
    // When the active node finished starting. Reset with the other per-session state, because a
    // hold window that survived a stop would delay the first switch of the next session.
    qint64 activeServerSinceMs_ = 0;
    bool refreshAfterFailover_ = false;
    QDateTime lastSubscriptionUpdateAt_;
    QDateTime lastLowCountryRefreshAt_;
    QMap<QString, QDateTime> lastCountryTestAt_;
    std::atomic_bool cancelWorkers_{false};
    std::shared_ptr<std::atomic_bool> evaluationCancel_;
    quint64 evaluationGeneration_ = 0;
    quint64 operationGeneration_ = 0;
    QList<QThread*> workerThreads_;
    // Workers that outlived the shutdown budget. Kept rather than deleted: destroying a running
    // QThread calls qFatal, and the worker may still be reading this object. Non-empty here means
    // the process is about to exit with work in flight, which the shutdown log records.
    QList<QThread*> abandonedThreads_;
    QTimer healthCheckTimer_;
    QTimer periodicRefreshTimer_;
};
