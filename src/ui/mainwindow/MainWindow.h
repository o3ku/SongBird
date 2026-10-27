#pragma once

#include <functional>
#include <memory>

#include <QCloseEvent>
#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QStringList>

#include "common/LogSeverity.h"
#include "domain/models/Config.h"
#include "domain/models/RuntimeState.h"
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
class LogFileWriter;
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

// Everything the window needs to render one configuration. Passing the config
// and the installed core set together removes the implicit
// "setConfig() must run before setExistingCoreTypes()" ordering the separate
// setters used to require: a caller that got the order wrong rendered the
// toolbar against a stale core list without any visible error.
struct MainWindowInit {
    Config config;
    QList<CoreType> existingCoreTypes;
};

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Single entry point for applying a configuration together with the
    // installed core set; action state is updated once, against both values.
    void initialize(const MainWindowInit& init);
    // Config-only update that reuses the previously applied core set.
    void setConfig(const Config& config);
    void setShareUrlResolver(std::function<QString(const QString&)> resolver);
    // Batch export formatters, injected by the composition root so this window never includes
    // services/ExportService. Each takes the selected indexIds and returns the text to write; an
    // empty result means there was nothing exportable, which the window reports rather than
    // writing an empty file.
    void setShareLinksExportProvider(std::function<QString(const QStringList&)> provider);
    void setClashYamlExportProvider(std::function<QString(const QStringList&)> provider);
    void setExistingCoreTypes(const QList<CoreType>& coreTypes);
    // Borrowed, never owned, and outlives this window: the composition root
    // declares it first so it is destroyed last. Leaving it unset is supported and
    // is what the tests rely on -- the panel is then the only sink, and nothing
    // touches the filesystem.
    void setLogFileWriter(LogFileWriter* writer);
    void appendLog(const QString& message, LogSeverity severity = LogSeverity::Info);
    void setHideToTrayEnabled(bool enabled);
    void setAllowClose(bool allowClose);
    bool requestExit();
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
    // The subscription list order is the tab order, so moving a tab is a config write: the window
    // reports the ids and the operation, and the composition root owns the service call.
    void moveSubscriptionsRequested(const QStringList& subscriptionIds, int operation);
    void reorderSubscriptionsRequested(const QStringList& orderedSubscriptionIds);
    void setDefaultServerRequested(const QString& indexId);
    void setDefaultServerWithTunRequested(const QString& indexId);
    void testServersRequested(const QStringList& indexIds);
    // Batch export. The window owns the selection and the file dialog; the composition root owns
    // the formatter, so the UI layer never depends on services/ExportService.
    void exportServersAsShareLinksRequested(const QStringList& indexIds);
    void exportServersAsClashYamlRequested(const QStringList& indexIds);
    void routingModeSelected(const QString& routingModeId);
    void enableSystemProxyRequested();
    void disableSystemProxyRequested();
    void tunEnabledChanged(bool enabled);
    void settingsRequested();
    void aboutRequested();
    void shortcutsRequested();
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
    void applyConfig(const Config& config);
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
    // Shared implementation of the two export actions: ask the injected provider for the text, ask
    // for a destination, then write. Kept as one method because the two differ only in the provider,
    // the suggested filename and the filter.
    void exportServersToFile(
        const QStringList& indexIds,
        const std::function<QString(const QStringList&)>& provider,
        const QString& title,
        const QString& suggestedFileName,
        const QString& filter,
        const QString& defaultSuffix);
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
    QAction* exportShareLinksAction_ = nullptr;
    QAction* exportClashYamlAction_ = nullptr;
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
    QAction* shortcutsAction_ = nullptr;
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
    ProxyUiState proxyUiState_ = ProxyUiState::Idle;
    bool qrPreviewVisible_ = false;
    QString currentServerLocation_;
    QString currentServerWarning_;
    QString systemProxyUndoWarning_;
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
    std::function<QString(const QStringList&)> shareLinksExportProvider_;
    std::function<QString(const QStringList&)> clashYamlExportProvider_;
    LogFileWriter* logFileWriter_ = nullptr;
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
