#pragma once

#include <QCloseEvent>
#include <QHash>
#include <QMainWindow>
#include <QMap>
#include <QStringList>

#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
#include "domain/models/ServerStatistics.h"
#include "domain/models/StatisticsSessionState.h"

class QAction;
class QComboBox;
class QEvent;
class QLineEdit;
class QMenu;
class QPoint;
class QToolBar;

class QShowEvent;
class QSplitter;
class QTabBar;
class LogPanelWidget;
class BackgroundTaskActionsController;
class ProxyToolbarController;
class RoutingModeController;
class SharePanelWidget;
class SubscriptionViewController;
class ServerListController;
class ServerWorkspaceWidget;
class WindowLayoutStateController;
class ServerActionsController;
class ServerSelectionController;
class StatusBarController;
class ServerFilterProxyModel;
class ServerTableModel;
class ServerTableView;
class QWidget;
struct ServerTableRow;
struct SubItem;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    enum class TransientStatusPriority {
        Important,
        Routine
    };

    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void setConfig(const Config& config, const QList<ServerStatItem>& statistics = {});
    void setExistingCoreTypes(const QList<CoreType>& coreTypes);
    void appendLog(const QString& message);
    void showTransientStatus(
        const QString& message,
        int timeoutMs = 5000,
        TransientStatusPriority priority = TransientStatusPriority::Important);
    void setHideToTrayEnabled(bool enabled);
    void setAllowClose(bool allowClose);
    bool requestExit();
    void setAutoRunEnabled(bool enabled);
    void setSystemProxyState(int mode, bool enabled);
    void setProxyEnabled(bool enabled);
    void setTunEnabled(bool enabled);
    void setCoreProcessRunning(bool running);
    void setCoreRunning(bool enabled, bool pending = false);
    void setCurrentServerName(const QString& name);
    void setCurrentServerLocation(const QString& location);
    void setCurrentServerWarning(const QString& warning);
    void setRoutingSummary(const QString& routingText, const QString& listenText);
    void setStatisticsSessionState(const StatisticsSessionState& state);
    void setSpeedTestRunning(bool running);
    void setSubscriptionUpdateRunning(bool running);
    void setBackgroundTaskRunning(bool running);
    void setBackgroundTaskDescription(const QString& description);
    void restoreUiState(const Config& config);
    void captureUiState(Config& config) const;
    bool selectSubscriptionTab(const QString& selectionId);
    void updateServerTestResult(const QString& indexId, const QString& result);
    void updateServerTestResults(const QStringList& indexIds, const QString& result);

signals:
    void openSettingsAtSubscriptionsTabRequested();
    void openSettingsAtRoutingTabRequested();
    void addServerRequested();
    void addCustomServerRequested();
    void editServerRequested(const QString& indexId);
    void duplicateServerRequested(const QString& indexId);
    void exportClientConfigRequested(const QString& indexId);
    void exportServerConfigRequested(const QString& indexId);
    void importFromClipboardRequested();
    void importClientConfigRequested();
    void importServerConfigRequested();
    void updateSubscriptionsRequested();
    void updateCurrentSubscriptionRequested(const QString& subscriptionId);
    void updateCurrentSubscriptionViaProxyRequested(const QString& subscriptionId);
    void hideSubscriptionRequested(const QString& subscriptionId);
    void deleteSubscriptionRequested(const QString& subscriptionId);
    void testMeRequested();
    void updateCoreRequested(int coreType);
    void updateGeoResourcesRequested();
    void openCustomConfigRequested(const QString& indexId);
    void removeServersRequested(const QStringList& indexIds);
    void moveServersRequested(const QStringList& indexIds, int operation);
    void reorderServersRequested(const QStringList& orderedIndexIds);
    void setDefaultServerRequested(const QString& indexId);
    void testServersRequested(const QStringList& indexIds);
    void systemProxyModeSelected(int mode);
    void routingModeSelected(int mode);
    void enableSystemProxyRequested();
    void disableSystemProxyRequested();
    void tunEnabledChanged(bool enabled);
    void toggleAutoRunRequested();
    void reloadConfigRequested();
    void restoreBackupRequested();
    void globalHotkeySettingsRequested();
    void dnsSettingsRequested();
    void settingsRequested();
    void aboutRequested();
    void openProjectPageRequested();
    void openReleasePageRequested();
    void openDocumentationRequested();
    void openDnsObjectDocumentationRequested();
    void openRuleObjectDocumentationRequested();
    void openLoopbackToolRequested();
    void openXrayReleasePageRequested();
    void openSingBoxReleasePageRequested();
    void openGeoReleasePageRequested();
    void clearStatisticsRequested();
    void hiddenToTray();

private slots:
    void updateActionState();
    void showServerContextMenu(const QPoint& position);

private:
    struct ConfigSnapshot {
        QString currentIndexId;
        QList<SubItem> subscriptions;
        QList<RoutingItem> routingItems;
        int routingIndex = 0;
        QList<CoreTypeItem> coreTypeItems;
    };

    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void setupUi();
    void setupToolbar();
    void createToolbarActions();
    void createServerToolbarActions();
    void createSubscriptionToolbarActions();
    void createUpdateToolbarActions();
    void createSystemToolbarActions();
    void createHelpToolbarActions();
    void createRuntimeToolbarActions();
    void createToolbarMenus(QToolBar* toolBar, QMenu*& settingMenu, QMenu*& helpMenu);
    void populateToolbarWidgets(QToolBar* toolBar, QMenu* helpMenu);
    void initializeToolbarControllers();
    void setupServerView();
    void setupServerWorkspace();
    void setupSubscriptionViewController();
    void setupServerListController();
    void setupDiagnosticsPanel();
    void setupStatusBarController();
    void setupServerSelectionController();
    void setupServerActionsController();
    void setupConnections();
    void setupGeneralActionConnections();
    void setupImportActionConnections();
    void setupUpdateActionConnections();
    void setupSettingsActionConnections();
    void setupHelpActionConnections();
    void setupSystemProxyConnections();
    void setupToggleConnections();
    void setupViewConnections();
    void setupSubscriptionViewConnections();
    void setupSplitterViewConnections();
    void setupShortcutConnections();
    void setupFilterShortcutConnections();
    void setupUpdateShortcutConnections();
    void setupExitShortcutConnection();
    void setupSubscriptionTabShortcutConnections();
    void updateQrPreview();
    void updateQrPanelActionText();
    void updateSelectionForVisibleRows();
    bool canStartBackgroundTask() const;
    bool isUngroupedSubscriptionTabSelected() const;
    QString currentSubscriptionIdForUpdate() const;
    void triggerCurrentSubscriptionUpdate();
    void copyCurrentSubscriptionUrlToClipboard();
    void clearTransientStatus();
    void updateWindowTitle();
    void setServerTableDynamicSortEnabled(bool enabled, bool invalidateModel);
    void updateConfigSnapshot(const Config& config);
    void rebuildShareUrlCache(const QList<VmessItem>& servers);
    void preserveServerSelectionPreference();
    void updateCurrentCoreFromConfig(const Config& config);
    void syncConfigControllers(const Config& config);
    void updateRuntimeUiState();
    void updateStatusPresentation(bool includeWindowTitle = false);
    void showSystemProxyMenu(const QPoint& globalPosition);
    bool confirmExit();
    const ServerTableRow* activeServer() const;
    void refreshServerSelectionUi();
    void syncStatusBarController();
    void syncProxyToolbarController();

    ServerTableModel* serverModel_ = nullptr;
    ServerFilterProxyModel* serverFilterModel_ = nullptr;
    ServerWorkspaceWidget* serverWorkspace_ = nullptr;
    BackgroundTaskActionsController* backgroundTaskActionsController_ = nullptr;
    ProxyToolbarController* proxyToolbarController_ = nullptr;
    RoutingModeController* routingModeController_ = nullptr;
    SubscriptionViewController* subscriptionViewController_ = nullptr;
    ServerListController* serverListController_ = nullptr;
    WindowLayoutStateController* windowLayoutStateController_ = nullptr;
    ServerActionsController* serverActionsController_ = nullptr;
    ServerSelectionController* serverSelectionController_ = nullptr;
    StatusBarController* statusBarController_ = nullptr;
    ServerTableView* serverView_ = nullptr;
    QTabBar* subscriptionTabBar_ = nullptr;
    SharePanelWidget* sharePanel_ = nullptr;
    QAction* addServerAction_ = nullptr;
    QAction* addCustomServerAction_ = nullptr;
    QAction* editServerAction_ = nullptr;
    QAction* duplicateServerAction_ = nullptr;
    QAction* exportClientConfigAction_ = nullptr;
    QAction* exportServerConfigAction_ = nullptr;
    QAction* copyUrlAction_ = nullptr;
    QAction* copyShareLinkAction_ = nullptr;
    QAction* copySubscriptionContentAction_ = nullptr;
    QAction* copySubscriptionUrlAction_ = nullptr;
    QAction* openCustomConfigAction_ = nullptr;
    QAction* importClipboardAction_ = nullptr;
    QAction* importClientConfigAction_ = nullptr;
    QAction* importServerConfigAction_ = nullptr;
    QAction* subAction_ = nullptr;
    QAction* routingSettingsAction_ = nullptr;
    QAction* updateSubscriptionsAction_ = nullptr;
    QAction* testMeAction_ = nullptr;
    QAction* updateXrayCoreAction_ = nullptr;
    QAction* updateSingBoxCoreAction_ = nullptr;
    QAction* updateGeoResourcesAction_ = nullptr;
    QAction* removeServerAction_ = nullptr;
    QAction* moveServerTopAction_ = nullptr;
    QAction* moveServerUpAction_ = nullptr;
    QAction* moveServerDownAction_ = nullptr;
    QAction* moveServerBottomAction_ = nullptr;
    QAction* testAction_ = nullptr;
    QAction* setDefaultServerAction_ = nullptr;
    QAction* updateCurrentSubscriptionAction_ = nullptr;
    QAction* updateCurrentSubscriptionShortcutAction_ = nullptr;
    QAction* clearProxyAction_ = nullptr;
    QAction* globalProxyAction_ = nullptr;
    QAction* unchangedProxyAction_ = nullptr;
    QAction* pacProxyAction_ = nullptr;
    QAction* toggleAutoRunAction_ = nullptr;
    QAction* reloadConfigAction_ = nullptr;
    QAction* restoreBackupAction_ = nullptr;
    QAction* globalHotkeySettingsAction_ = nullptr;
    QAction* dnsSettingsAction_ = nullptr;
    QAction* settingsAction_ = nullptr;
    QAction* aboutAction_ = nullptr;
    QAction* openProjectPageAction_ = nullptr;
    QAction* openReleasePageAction_ = nullptr;
    QAction* openDocumentationAction_ = nullptr;
    QAction* openDnsObjectDocumentationAction_ = nullptr;
    QAction* openRuleObjectDocumentationAction_ = nullptr;
    QAction* openLoopbackToolAction_ = nullptr;
    QAction* openXrayReleasePageAction_ = nullptr;
    QAction* openSingBoxReleasePageAction_ = nullptr;
    QAction* openGeoReleasePageAction_ = nullptr;
    QAction* proxyToggleAction_ = nullptr;
    QAction* tunToggleAction_ = nullptr;
    QAction* clearStatisticsAction_ = nullptr;
    QAction* toggleQrPanelAction_ = nullptr;
    QMenu* systemProxyMenu_ = nullptr;
    QComboBox* systemProxyModeCombo_ = nullptr;
    QComboBox* routingModeCombo_ = nullptr;
    QLineEdit* serverFilterEdit_ = nullptr;
    bool hideToTrayEnabled_ = false;
    bool allowClose_ = false;
    bool systemProxyApplied_ = false;
    bool autoRunEnabled_ = false;
    bool coreProcessRunning_ = false;
    bool coreRunning_ = false;
    bool coreTransitionPending_ = false;
    bool qrPreviewVisible_ = false;
    QString currentServerLocation_;
    bool speedTestRunning_ = false;
    QString currentServerWarning_;
    bool backgroundTaskRunning_ = false;
    QString backgroundTaskDescription_;
    ConfigSnapshot configSnapshot_;
    QHash<QString, QString> shareUrlByIndexId_;
    QList<CoreType> existingCoreTypes_;
    QString currentIndexId_;
    QString currentServerName_;
    QString currentCoreName_;
    QString routingSummary_;
    QString listenSummary_;
    StatisticsSessionState statisticsState_;
    bool tunEnabled_ = false;
    SystemProxyMode systemProxyMode_ = SystemProxyMode::ForcedClear;
};
