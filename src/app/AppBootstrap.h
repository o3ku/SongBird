#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <QList>
#include <QString>
#include <QStringList>

#include "common/OperationResult.h"
#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
#include "runtime/CoreInfo.h"

class CoreLifecycleService;
class ClientConfigWriter;
class ServerConfigWriter;
class MainWindow;
class JsonConfigRepository;
class QNetworkAccessManager;
class QObject;
class QTcpServer;
class QThread;
class QWidget;
class SingleInstanceGuard;
class GrpcStatisticsBackend;
class ClashRestStatisticsBackend;
class QtCoreProcessHost;
class QTimer;
class RoutingService;
class ConfigBackupService;
class CoreUpdateService;
class GeoResourceUpdateService;
class ProxyAvailabilityCheckService;
class ServerService;
class SpeedTestService;
class StatisticsService;
class SubscriptionService;
class SubscriptionUpdateService;
class TrayController;
class WindowsAutoRunService;
class WindowsGlobalHotkeyService;
class WindowsSystemProxyService;
enum class SpeedTestMode : int;

class AppBootstrap {
public:
    explicit AppBootstrap(
        QString configPath = {},
        bool autoStartCore = false,
        bool startHidden = false,
        bool registerGlobalHotkeys = true,
        bool skipCoreChecks = false);
    ~AppBootstrap();

    bool run();

private:
    void wireMainWindow();
    void syncWindow();
    void syncStatusIndicators();
    bool reloadConfig();
    bool shouldStartCoreOnStartup() const;
    void startCoreOnStartup();
    void appendStartupResourceCheckResults();
    void appendResult(const OperationResult& result);
    void persistUiState();
    void startCore();
    void stopCore();
    void restartCoreIfRunning(const QString& reason);
    void setSystemProxyMode(SystemProxyMode mode);
    void applySystemProxyModeOnExit(bool windowsShutdown);
    void enableSystemProxy();
    void disableSystemProxy();
    void toggleAutoRun();
    void importFromClipboard();
    void importSubscriptionUrlsFromTextAsync(const QString& text);
    void exportClientConfig(const QString& indexId);
    void exportServerConfig(const QString& indexId);
    void importClientConfigFromFile();
    void importServerConfigFromFile();
    OperationResult importCustomConfigText(const QString& text);
    void updateAllSubscriptions();
    void updateCurrentSubscription(const QString& subscriptionId);
    void updateCurrentSubscriptionViaProxy(const QString& subscriptionId);
    void runProxyAvailabilityCheck();
    void updateCore(int coreTypeValue);
    void updateCore(int coreTypeValue, bool startAfterSuccess);
    void updateCore(
        int coreTypeValue,
        bool startAfterSuccess,
        QObject* progressContext,
        QWidget* dialogParent,
        const std::function<void(const QString&)>& progressObserver,
        const std::function<void(const OperationResult&)>& completionObserver);
    void updateGeoResources();
    void updateSelectedSubscriptions(const QList<int>& rowIndexes);
    void updateSubscriptionsByIds(const QStringList& subscriptionIds, bool useProxy, const QString& startupMessage);
    void hideSubscription(const QString& subscriptionId);
    void deleteSubscription(const QString& subscriptionId);
    void autoBackupCurrentConfig();
    void restoreConfigFromBackup();
    void openSettingsDialog(int initialTab = 0);
    void openAboutDialog();
    void openExternalUrl(const QString& url);
    void openLoopbackTool();
    bool promptRestartAsAdministratorForTun();
    void promptRestartForLanguageChange();
    void startSpeedTest(SpeedTestMode mode, const QStringList& indexIds);
    bool isCoreRunning() const;
    const VmessItem* findServerById(const QString& indexId) const;
    const VmessItem* resolveActiveServer() const;
    QString describeServer(const VmessItem* server) const;
    QString resolveConfigPath() const;
    QString resolveCustomConfigDirectory() const;
    QString resolveCustomConfigPath(const QString& address) const;
    QString resolveRuntimeConfigPath(const VmessItem& server) const;
    QString resolveStatisticsFilePath() const;
    int resolveStatisticsPort() const;
    CoreInfo resolveCoreInfo(const VmessItem& server) const;
    CoreType resolveEffectiveCoreType(const VmessItem& server) const;
    QStringList resolveCoreCandidates(CoreType coreType) const;
    QStringList resolveCoreCandidates(const VmessItem& server) const;
    QString locateFirstExistingFile(const QStringList& candidates) const;
    QString resolveCoreInstallDirectory(CoreType coreType) const;
    QString buildTrafficSummaryText() const;
    QString buildStatisticsSummaryText() const;
    QString buildRoutingSummaryText() const;
    QString buildListenSummaryText() const;
    QString buildSystemProxyExceptions() const;
    void trackBackgroundThread(QThread* thread);
    void waitForBackgroundThreads();
    void stopAuxiliaryCore();
    void removeStaleTunAdapter();
    void removeStaleSingBoxCache();
    void cleanupOrphanCoreProcesses();
    void handleCoreExited(int exitCode, int status, bool stopRequested, bool auxiliary);
    void scheduleCoreRestart(const QString& reason, bool auxiliary, int delayMs = 3000);
    void cancelPendingCoreRestarts();

    QString configPath_;
    bool autoStartCore_ = false;
    bool startHidden_ = false;
    bool registerGlobalHotkeys_ = true;
    bool skipCoreChecks_ = false;
    Config config_;
    std::unique_ptr<JsonConfigRepository> repository_;
    std::unique_ptr<ServerService> serverService_;
    std::unique_ptr<ConfigBackupService> configBackupService_;
    std::unique_ptr<RoutingService> routingService_;
    std::unique_ptr<SpeedTestService> speedTestService_;
    std::unique_ptr<StatisticsService> statisticsService_;
    std::unique_ptr<GrpcStatisticsBackend> grpcStatisticsBackend_;
    std::unique_ptr<ClashRestStatisticsBackend> clashStatisticsBackend_;
    std::unique_ptr<SubscriptionService> subscriptionService_;
    std::unique_ptr<ProxyAvailabilityCheckService> proxyAvailabilityCheckService_;
    std::unique_ptr<CoreUpdateService> coreUpdateService_;
    std::unique_ptr<GeoResourceUpdateService> geoResourceUpdateService_;
    std::unique_ptr<QNetworkAccessManager> networkAccessManager_;
    std::unique_ptr<SubscriptionUpdateService> subscriptionUpdateService_;
    std::unique_ptr<ClientConfigWriter> clientConfigWriter_;
    std::unique_ptr<ServerConfigWriter> serverConfigWriter_;
    std::unique_ptr<QtCoreProcessHost> coreProcessHost_;
    std::unique_ptr<CoreLifecycleService> coreLifecycleService_;
    std::unique_ptr<QtCoreProcessHost> auxiliaryCoreProcessHost_;
    std::unique_ptr<CoreLifecycleService> auxiliaryCoreLifecycleService_;
    std::unique_ptr<WindowsAutoRunService> autoRunService_;
    std::unique_ptr<WindowsGlobalHotkeyService> globalHotkeyService_;
    std::unique_ptr<WindowsSystemProxyService> systemProxyService_;
    std::unique_ptr<TrayController> trayController_;
    std::unique_ptr<MainWindow> mainWindow_;
    SystemProxyMode systemProxyMode_ = SystemProxyMode::ForcedClear;
    bool uiReady_ = false;
    bool uiStateRestored_ = false;
    bool exitProxyStateApplied_ = false;
    bool windowsShutdownRequested_ = false;
    bool subscriptionUpdateRunning_ = false;
    bool proxyAvailabilityCheckRunning_ = false;
    bool coreUpdateRunning_ = false;
    bool geoUpdateRunning_ = false;
    QList<QThread*> backgroundThreads_;
    std::atomic_bool shuttingDown_{false};
    QTimer* coreRestartTimer_ = nullptr;
    QTimer* auxiliaryRestartTimer_ = nullptr;
};
