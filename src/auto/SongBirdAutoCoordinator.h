#pragma once

#include <atomic>
#include <memory>

#include <QList>
#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "app/AppRuntimeResolver.h"
#include "app/BackgroundTaskCoordinator.h"
#include "app/CoreProcessCleanupService.h"
#include "app/CoreDiscoveryService.h"
#include "app/FunctionRuntimeAdapters.h"
#include "app/OutboundLocationProbeService.h"
#include "app/ProxySession.h"
#include "app/TunRuntimeService.h"
#include "auto/AutoTypes.h"
#include "domain/models/Config.h"
#include "persistence/JsonConfigRepository.h"
#include "platform/windows/WindowsSystemProxyService.h"
#include "runtime/ClientConfigWriter.h"
#include "runtime/QtCoreProcessHost.h"
#include "services/ProxyAvailabilityCheckService.h"
#include "services/ServerService.h"
#include "services/SpeedTestRequestItem.h"
#include "services/SubscriptionService.h"

class QNetworkAccessManager;
class QThread;

class SongBirdAutoCoordinator final : public QObject
{
    Q_OBJECT

public:
    explicit SongBirdAutoCoordinator(QString configPath, QObject* parent = nullptr);
    ~SongBirdAutoCoordinator() override;

    bool initialize();
    QString configPath() const;
    Config currentConfig() const;
    QString selectedCountryCode() const;
    QString subscriptionUrlsText() const;
    QString autoSelectionStrategy() const;
    bool isRunning() const;
    bool isTunEnabled() const;

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
    void busyChanged(bool busy);
    void activeServerChanged(const QString& serverId, const QString& serverName, const QString& countryDisplay, const QString& location, qint64 latencyMs);
    void statusMessageChanged(const QString& message);
    void subscriptionUrlsTextChanged(const QString& text);
    void tunEnabledChanged(bool enabled);
    void autoSelectionStrategyChanged(const QString& strategy);

private:
    void reloadConfig();
    bool saveConfig();
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
    void stopProxySession(bool immediate);
    bool startTunRuntime(bool relayToProxy);
    void stopTunRuntime(bool cleanupAdapter);
    bool syncTunRuntimeForCurrentState();
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
    void setBusy(bool busy);
    void setRunning(bool running);
    void setStatus(const QString& status);
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
    std::unique_ptr<QtCoreProcessHost> mainCore_;
    std::unique_ptr<QtCoreProcessHost> auxiliaryCore_;
    std::unique_ptr<QtCoreProcessHost> tunCore_;
    std::unique_ptr<OutboundLocationProbeService> locationProbe_;
    std::unique_ptr<BackgroundTaskCoordinator> backgroundTasks_;
    std::unique_ptr<FunctionRuntimeEnvironment> runtimeEnvironment_;
    std::unique_ptr<FunctionProxyActivationCoordinator> activationCoordinator_;
    std::unique_ptr<ProxySession> proxySession_;
    std::unique_ptr<WindowsSystemProxyService> systemProxyService_;
    ProxyAvailabilityCheckService availabilityCheck_;

    QList<CoreType> existingCoreTypes_;
    QList<AutoNodeEvaluation> evaluations_;
    QList<AutoCountrySummary> countrySummaries_;
    QString selectedCountryCode_;
    QString activeServerId_;
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
    bool refreshAfterFailover_ = false;
    QDateTime lastSubscriptionUpdateAt_;
    QDateTime lastLowCountryRefreshAt_;
    QMap<QString, QDateTime> lastCountryTestAt_;
    std::atomic_bool cancelWorkers_{false};
    std::shared_ptr<std::atomic_bool> evaluationCancel_;
    quint64 evaluationGeneration_ = 0;
    quint64 operationGeneration_ = 0;
    QList<QThread*> workerThreads_;
    QTimer healthCheckTimer_;
    QTimer periodicRefreshTimer_;
};
