#pragma once

#include <QCloseEvent>
#include <QHash>
#include <QMainWindow>
#include <QMap>
#include <QStringList>

#include "common/SystemProxyMode.h"
#include "domain/models/Config.h"
#include "app/RuntimeState.h"
class QAction;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QMenu;
class QPoint;
class QPushButton;
class QResizeEvent;
class QToolBar;
class QToolButton;
class QVBoxLayout;

class QShowEvent;
class QSplitter;
class QTabBar;
class LogPanelWidget;
class BackgroundTaskActionsController;
class ProxyToolbarController;
class RoutingModeController;
class SharePanelWidget;
class StartupOverlayWidget;
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

    void setConfig(const Config& config);
    void setExistingCoreTypes(const QList<CoreType>& coreTypes);
    void appendLog(const QString& message);
    void showTransientStatus(
        const QString& message,
        int timeoutMs = 5000,
        TransientStatusPriority priority = TransientStatusPriority::Important);
    void setHideToTrayEnabled(bool enabled);
    void setAllowClose(bool allowClose);
    bool requestExit();
    void setSystemProxyState(int mode, bool enabled);
    void setProxyEnabled(bool enabled);
    void setTunEnabled(bool enabled);
    void setProxyUiState(ProxyUiState state);
    void setCurrentServerName(const QString& name);
    void setCurrentServerLocation(const QString& location);
    void setCurrentServerWarning(const QString& warning);
    void setRoutingSummary(const QString& routingText, const QString& listenText);
    void setSubscriptionUpdateRunning(bool running);
    void setCoreStartupChecklist(const QStringList& items);
    void clearCoreStartupChecklist();
    void setBackgroundTaskRunning(bool running);
    void setBackgroundTaskDescription(const QString& description);
    void applyRuntimeState(const RuntimeStateSnapshot& snapshot);
    void restoreUiState(const Config& config);
    void captureUiState(Config& config) const;
    bool selectSubscriptionTab(const QString& selectionId);
    void updateServerTestResult(const QString& indexId, const QString& result);
    void updateServerTestResults(const QStringList& indexIds, const QString& result);

signals:
    void openSettingsAtSubscriptionsTabRequested();
    void openSettingsAtRoutingTabRequested();
    void addServerRequested();
    void editServerRequested(const QString& indexId);
    void importFromClipboardRequested();
    void updateSubscriptionsRequested();
    void updateCurrentSubscriptionRequested(const QString& subscriptionId);
    void updateCurrentSubscriptionViaProxyRequested(const QString& subscriptionId);
    void hideSubscriptionRequested(const QString& subscriptionId);
    void deleteSubscriptionRequested(const QString& subscriptionId);
    void updateCoreRequested(int coreType);
    void updateGeoResourcesRequested();
    void removeServersRequested(const QStringList& indexIds);
    void moveServersRequested(const QStringList& indexIds, int operation);
    void reorderServersRequested(const QStringList& orderedIndexIds);
    void setDefaultServerRequested(const QString& indexId);
    void setDefaultServerWithTunRequested(const QString& indexId);
    void testServersRequested(const QStringList& indexIds);
    void routingModeSelected(int mode);
    void enableSystemProxyRequested();
    void disableSystemProxyRequested();
    void tunEnabledChanged(bool enabled);
    void settingsRequested();
    void aboutRequested();
    void checkAppUpdateRequested();
    void uwpLoopbackRequested();
    void retryCoreStartupRequested();
    void hiddenToTray();

private slots:
    void updateActionState();
    void showServerContextMenu(const QPoint& position);
    void refreshThemeAssets();

private:
    struct ConfigSnapshot {
        QString currentIndexId;
        QList<SubItem> subscriptions;
        QList<RoutingItem> routingItems;
        int routingIndex = 0;
        QList<CoreTypeItem> coreTypeItems;
    };

    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
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
    void createToolbarMenus(QToolBar* toolBar, QMenu*& helpMenu);
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
    void setupToggleConnections();
    void setupViewConnections();
    void setupSubscriptionViewConnections();
    void setupSplitterViewConnections();
    void setupShortcutConnections();
    void setupFilterShortcutConnections();
    void setupUpdateShortcutConnections();
    void setupExitShortcutConnection();
    void setupSubscriptionTabShortcutConnections();
    void scheduleInitialCurrentServerReveal();
    void updateQrPreview();
    void updateQrPanelActionText();
    void updateSelectionForVisibleRows();
    void showCurrentServerInTable();
    bool canStartBackgroundTask() const;
    bool ensureCanStartBackgroundTask();
    bool isUngroupedSubscriptionTabSelected() const;
    QString currentSubscriptionIdForUpdate() const;
    void testSubscriptionServers(const QString& subscriptionId);
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
    bool confirmExit();
    const ServerTableRow* activeServer() const;
    void showSelectServerHint();
    void clearSelectServerHint();
    void refreshToolbarIcons();
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
    QAction* editServerAction_ = nullptr;
    QAction* copyUrlAction_ = nullptr;
    QAction* copyShareLinkAction_ = nullptr;
    QAction* importClipboardAction_ = nullptr;
    QAction* subAction_ = nullptr;
    QAction* routingSettingsAction_ = nullptr;
    QAction* updateSubscriptionsAction_ = nullptr;
    QMap<int, QAction*> updateCoreActions_;
    QAction* updateGeoResourcesAction_ = nullptr;
    QAction* removeServerAction_ = nullptr;
    QAction* moveServerTopAction_ = nullptr;
    QAction* moveServerUpAction_ = nullptr;
    QAction* moveServerDownAction_ = nullptr;
    QAction* moveServerBottomAction_ = nullptr;
    QAction* testAction_ = nullptr;
    QAction* setDefaultServerAction_ = nullptr;
    QAction* setDefaultServerWithTunAction_ = nullptr;
    QAction* updateCurrentSubscriptionAction_ = nullptr;
    QAction* updateCurrentSubscriptionShortcutAction_ = nullptr;
    QAction* settingsAction_ = nullptr;
    QAction* aboutAction_ = nullptr;
    QAction* checkAppUpdateAction_ = nullptr;
    QAction* uwpLoopbackAction_ = nullptr;
    QAction* proxyToggleAction_ = nullptr;
    QAction* tunToggleAction_ = nullptr;
    QAction* toggleQrPanelAction_ = nullptr;
    QLabel* selectServerHintLabel_ = nullptr;
    QComboBox* routingModeCombo_ = nullptr;
    QLineEdit* serverFilterEdit_ = nullptr;
    QWidget* mainContentWidget_ = nullptr;
    QVBoxLayout* mainContentLayout_ = nullptr;
    StartupOverlayWidget* startupOverlay_ = nullptr;
    bool hideToTrayEnabled_ = false;
    bool allowClose_ = false;
    bool systemProxyApplied_ = false;
    ProxyUiState proxyUiState_ = ProxyUiState::Idle;
    bool qrPreviewVisible_ = false;
    QString currentServerLocation_;
    QString currentServerWarning_;
    bool backgroundTaskRunning_ = false;
    bool coreStartupChecklistVisible_ = false;
    bool coreStartupChecklistFailed_ = false;
    QString backgroundTaskDescription_;
    QHash<QAction*, QString> toolbarIconFiles_;
    QHash<QToolButton*, QString> toolbarStandaloneIconFiles_;
    ConfigSnapshot configSnapshot_;
    QHash<QString, QString> shareUrlByIndexId_;
    QList<CoreType> existingCoreTypes_;
    QString currentIndexId_;
    QString currentServerName_;
    QString currentCoreName_;
    QString routingSummary_;
    QString listenSummary_;
    bool tunEnabled_ = false;
    bool initialCurrentServerRevealScheduled_ = false;
    bool initialCurrentServerRevealDone_ = false;
};
