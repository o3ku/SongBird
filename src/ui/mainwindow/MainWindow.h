#pragma once

#include <functional>

#include <QCloseEvent>
#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QStringList>

#include "domain/models/Config.h"
#include "app/RuntimeState.h"
#include "ui/mainwindow/MainWindowConfigState.h"
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
class MainWindowShortcutController;
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
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void setConfig(const Config& config);
    void setShareUrlResolver(std::function<QString(const QString&)> resolver);
    void setExistingCoreTypes(const QList<CoreType>& coreTypes);
    void appendLog(const QString& message);
    void setHideToTrayEnabled(bool enabled);
    void setAllowClose(bool allowClose);
    bool requestExit();
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
    void setAvailableAppUpdateVersion(const QString& version);
    void clearAvailableAppUpdateVersion();
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
    void routingModeSelected(const QString& routingModeId);
    void enableSystemProxyRequested();
    void disableSystemProxyRequested();
    void tunEnabledChanged(bool enabled);
    void settingsRequested();
    void aboutRequested();
    void checkAppUpdateRequested();
    void downloadAppUpdateRequested();
    void uwpLoopbackRequested();
    void retryCoreStartupRequested();
    void hiddenToTray();

private slots:
    void updateActionState();
    void showServerContextMenu(const QPoint& position);
    void refreshThemeAssets();

private:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void setupUi();
    void setupToolbar();
    void createToolbarActions();
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
    void setupShortcutController();
    void scheduleInitialCurrentServerReveal();
    void updateQrPreview();
    void updateQrPanelActionText();
    void updateSelectionForVisibleRows();
    void showCurrentServerInTable();
    void applyBackgroundTaskRunningState(bool running);
    bool canStartBackgroundTask() const;
    bool ensureCanStartBackgroundTask();
    bool isUngroupedSubscriptionTabSelected() const;
    void testSubscriptionServers(const QString& subscriptionId);
    void triggerCurrentSubscriptionUpdate();
    void copyCurrentSubscriptionUrlToClipboard();
    void updateWindowTitle();
    void setServerTableDynamicSortEnabled(bool enabled, bool invalidateModel);
    void preserveServerSelectionPreference();
    void syncConfigControllers(const Config& config);
    void updateRuntimeUiState();
    void updateStatusPresentation(bool includeWindowTitle = false);
    bool confirmExit();
    const ServerTableRow* activeServer() const;
    void showSelectServerHint();
    void refreshToolbarIcons();
    void refreshServerSelectionUi();
    QStringList selectedShareLinks() const;
    void syncStatusBarController();
    void syncProxyToolbarController();
    void applyCompactMode(bool compact);
    void applyToolbarCompactMode(bool compact);
    void applyServerTableCompactMode(bool compact);

    ServerTableModel* serverModel_ = nullptr;
    ServerFilterProxyModel* serverFilterModel_ = nullptr;
    ServerWorkspaceWidget* serverWorkspace_ = nullptr;
    BackgroundTaskActionsController* backgroundTaskActionsController_ = nullptr;
    ProxyToolbarController* proxyToolbarController_ = nullptr;
    RoutingModeController* routingModeController_ = nullptr;
    SubscriptionViewController* subscriptionViewController_ = nullptr;
    ServerListController* serverListController_ = nullptr;
    MainWindowShortcutController* shortcutController_ = nullptr;
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
    QComboBox* routingModeCombo_ = nullptr;
    QLineEdit* serverFilterEdit_ = nullptr;
    QWidget* mainContentWidget_ = nullptr;
    QVBoxLayout* mainContentLayout_ = nullptr;
    QWidget* toolbarFlexibleSpacer_ = nullptr;
    QAction* toolbarFlexibleSpacerAction_ = nullptr;
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
    QString appVersionStatusText_;
    QString appUpdateStatusText_;
    bool appUpdateAvailable_ = false;
    QHash<QAction*, QString> toolbarIconFiles_;
    QHash<QToolButton*, QString> toolbarStandaloneIconFiles_;
    QList<QWidget*> desktopOnlyToolbarWidgets_;
    QList<QAction*> desktopOnlyToolbarWidgetActions_;
    MainWindowConfigSnapshot configSnapshot_;
    std::function<QString(const QString&)> shareUrlResolver_;
    QList<CoreType> existingCoreTypes_;
    QString currentIndexId_;
    QString currentServerName_;
    QString currentCoreName_;
    QString routingSummary_;
    QString listenSummary_;
    bool tunEnabled_ = false;
    bool initialCurrentServerRevealScheduled_ = false;
    bool initialCurrentServerRevealDone_ = false;
    bool compactMode_ = false;
};
