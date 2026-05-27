#include "ui/mainwindow/MainWindow.h"
#include "common/AppPlatform.h"
#include "common/DialogUtils.h"
#include "app/RuntimeState.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QHeaderView>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QTextBlock>
#include <QTextCursor>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTabBar>
#include <QTableView>
#include <QTime>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTextDocument>
#include <QTextOption>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <QAbstractItemView>

#include "domain/models/RoutingProfiles.h"
#include "services/ServerService.h"
#include "ui/mainwindow/LogItemDelegate.h"
#include "ui/mainwindow/LogPanelWidget.h"
#include "ui/mainwindow/BackgroundTaskActionsController.h"
#include "ui/mainwindow/ProxyToolbarController.h"
#include "ui/mainwindow/RoutingModeController.h"
#include "ui/mainwindow/SharePanelWidget.h"
#include "ui/mainwindow/ServerWorkspaceWidget.h"
#include "ui/mainwindow/MainWindowConfigState.h"
#include "ui/mainwindow/MainWindowShortcutController.h"
#include "ui/mainwindow/MainWindowTitleSupport.h"
#include "ui/mainwindow/MainWindowToolbarActions.h"
#include "ui/mainwindow/MainWindowServerSelectionSupport.h"
#include "ui/mainwindow/SubscriptionViewController.h"
#include "ui/mainwindow/ServerListController.h"
#include "ui/mainwindow/WindowLayoutStateController.h"
#include "ui/mainwindow/ServerActionsController.h"
#include "ui/mainwindow/ServerSelectionController.h"
#include "ui/mainwindow/ServerTableView.h"
#include "ui/mainwindow/StartupOverlayWidget.h"
#include "ui/mainwindow/StatusBarController.h"
#include "ui/mainwindow/ToolbarWidgets.h"
#include "ui/models/LogFilterProxyModel.h"
#include "ui/models/LogListModel.h"
#include "ui/models/ServerFilterProxyModel.h"
#include "ui/models/ServerTableModel.h"
#include "ui/qr/QrCodeRenderer.h"
#include "ui/theme/AppTheme.h"

namespace {

constexpr int DefaultMainWindowWidth = 1000;
constexpr int DefaultMainWindowHeight = 640;
constexpr int MinimumMainWindowWidth = 360;
constexpr int MinimumMainWindowHeight = 540;
constexpr int CompactMainWindowWidth = 520;
constexpr int ProxyToggleButtonWidth = 64;
constexpr int RoutingModeComboWidth = 87;
constexpr int WindowTitleServerNameMaximumWidth = 300;
constexpr int ServerTypeColumn = 1;
constexpr int ServerAddressColumn = 3;
namespace ServerSelection = MainWindowServerSelectionSupport;
namespace WindowTitle = MainWindowTitleSupport;

enum class BackgroundTaskBlockReason {
    None,
    BackgroundTaskUnavailable,
    ProxyStartupInProgress,
};

enum class ProxyToggleCommand {
    None,
    DisableProxy,
    EnableProxy,
    PromoteSelectedServer,
    SelectServer,
};

struct ProxyToggleDecision {
    ProxyToggleCommand command = ProxyToggleCommand::None;
    QString selectedServerId;
};

void applySemanticState(QLabel* label, const QString& state)
{
    if (label == nullptr) {
        return;
    }

    label->setProperty("semanticState", state);
    label->style()->unpolish(label);
    label->style()->polish(label);
    label->update();
}

QAction* toolbarWidgetAction(QWidget* widget)
{
    return widget == nullptr
        ? nullptr
        : qobject_cast<QAction*>(widget->property("toolbarWidgetAction").value<QObject*>());
}

ServerActionsController::SelectionSnapshot makeServerActionSelectionSnapshot(
    const ServerSelectionController* serverSelectionController,
    const ServerTableModel* serverModel,
    const QStringList& selectedShareLinks,
    bool canStartTask)
{
    ServerActionsController::SelectionSnapshot snapshot;
    snapshot.selectedItems = serverSelectionController == nullptr
        ? QList<const ServerTableRow*>()
        : serverSelectionController->selectedServers();
    snapshot.hasServers = serverModel != nullptr && serverModel->rowCount() > 1;
    snapshot.canStartTask = canStartTask;
    snapshot.selectedShareLinks = selectedShareLinks;
    return snapshot;
}

BackgroundTaskBlockReason backgroundTaskBlockReason(
    bool backgroundTaskRunning,
    ProxyUiState proxyUiState,
    bool coreStartupChecklistVisible,
    bool coreStartupChecklistFailed)
{
    if (backgroundTaskRunning) {
        return BackgroundTaskBlockReason::BackgroundTaskUnavailable;
    }
    if (proxyUiState == ProxyUiState::Transitioning
        || (coreStartupChecklistVisible && !coreStartupChecklistFailed)) {
        return BackgroundTaskBlockReason::ProxyStartupInProgress;
    }
    if (proxyUiState == ProxyUiState::Inconsistent) {
        return BackgroundTaskBlockReason::BackgroundTaskUnavailable;
    }

    return BackgroundTaskBlockReason::None;
}

ProxyToggleDecision decideProxyToggle(
    const ProxyToolbarController::Snapshot& snapshot,
    const ServerTableRow* activeServer,
    const ServerTableRow* selectedServer)
{
    if (snapshot.uiState == ProxyUiState::Active) {
        return {ProxyToggleCommand::DisableProxy, {}};
    }

    if (snapshot.uiState != ProxyUiState::Idle) {
        return {};
    }

    if (activeServer != nullptr) {
        return {ProxyToggleCommand::EnableProxy, {}};
    }

    if (selectedServer != nullptr && !selectedServer->indexId.isEmpty()) {
        return {ProxyToggleCommand::PromoteSelectedServer, selectedServer->indexId};
    }

    return {ProxyToggleCommand::SelectServer, {}};
}

bool shouldIgnoreTunToggle(ProxyUiState uiState)
{
    return uiState == ProxyUiState::Transitioning
        || uiState == ProxyUiState::Inconsistent;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    const QString version = QCoreApplication::applicationVersion().trimmed();
    appVersionStatusText_ = version.isEmpty()
        ? tr("SongBird")
        : version;
    setupUi();
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete serverActionsController_;
    delete serverSelectionController_;
    delete statusBarController_;
    delete serverListController_;
    delete shortcutController_;
    delete subscriptionViewController_;
    delete routingModeController_;
    delete proxyToolbarController_;
    delete backgroundTaskActionsController_;
}

void MainWindow::setConfig(const Config& config)
{
    refreshToolbarIcons();
    configSnapshot_ = makeMainWindowConfigSnapshot(config);
    preserveServerSelectionPreference();

    serverModel_->setItems(config.collection().servers, config.currentIndexId);
    currentIndexId_ = config.currentIndexId.trimmed();
    tunEnabled_ = config.tun().tunModeItem.enableTun;
    currentCoreName_ = currentCoreDisplayNameFromConfig(config, currentIndexId_);
    syncConfigControllers(config);

    if (serverView_ != nullptr && serverView_->selectionModel() != nullptr) {
        updateSelectionForVisibleRows();
    }

    scheduleInitialCurrentServerReveal();
    updateQrPreview();
    updateActionState();
    updateWindowTitle();
}

void MainWindow::setShareUrlResolver(std::function<QString(const QString&)> resolver)
{
    shareUrlResolver_ = std::move(resolver);
    refreshServerSelectionUi();
}

void MainWindow::setExistingCoreTypes(const QList<CoreType>& coreTypes)
{
    existingCoreTypes_ = coreTypes;
    updateActionState();
}

void MainWindow::updateServerTestResult(const QString& indexId, const QString& result)
{
    if (serverModel_ == nullptr) {
        return;
    }

    serverModel_->updateTestResult(indexId, result);
}

void MainWindow::updateServerTestResults(const QStringList& indexIds, const QString& result)
{
    if (serverModel_ == nullptr) {
        return;
    }

    const bool temporarilySuspendSort =
        serverFilterModel_ != nullptr
        && indexIds.size() > 1
        && !(serverListController_ != nullptr && serverListController_->dynamicSortSuspended());
    if (temporarilySuspendSort) {
        setServerTableDynamicSortEnabled(false, false);
    }

    for (const QString& indexId : indexIds) {
        serverModel_->updateTestResult(indexId, result);
    }

    if (temporarilySuspendSort) {
        setServerTableDynamicSortEnabled(true, true);
    }
}

void MainWindow::setServerTableDynamicSortEnabled(bool enabled, bool invalidateModel)
{
    if (serverListController_ != nullptr) {
        serverListController_->setDynamicSortEnabled(enabled, invalidateModel);
    }
}

void MainWindow::appendLog(const QString& message)
{
    if (serverWorkspace_ != nullptr && serverWorkspace_->logPanel() != nullptr) {
        serverWorkspace_->logPanel()->appendLog(message);
    }
}

void MainWindow::setHideToTrayEnabled(bool enabled)
{
    hideToTrayEnabled_ = enabled;
}

void MainWindow::setAllowClose(bool allowClose)
{
    allowClose_ = allowClose;
}

bool MainWindow::requestExit()
{
    if (!confirmExit()) {
        return false;
    }

    allowClose_ = true;
    close();
    return true;
}

void MainWindow::setProxyEnabled(bool enabled)
{
    systemProxyApplied_ = enabled;
    updateRuntimeUiState();
}

void MainWindow::setProxyUiState(ProxyUiState state)
{
    proxyUiState_ = state;
    updateRuntimeUiState();
}

void MainWindow::setCurrentServerName(const QString& name)
{
    currentServerName_ = name.trimmed();
    updateStatusPresentation(true);
}

void MainWindow::setCurrentServerLocation(const QString& location)
{
    currentServerLocation_ = location.trimmed();
    updateStatusPresentation();
    syncProxyToolbarController();
}

void MainWindow::setCurrentServerWarning(const QString& warning)
{
    currentServerWarning_ = warning.trimmed();
    updateStatusPresentation();
}

void MainWindow::setRoutingSummary(const QString& routingText, const QString& listenText)
{
    routingSummary_ = routingText.trimmed();
    listenSummary_ = listenText.trimmed();
    updateStatusPresentation();
}

void MainWindow::setSubscriptionUpdateRunning(bool running)
{
    backgroundTaskDescription_ = running ? tr("Updating subscriptions...") : QString();
    if (running) {
        startupOverlay_->showChecklist(tr("Updating subscriptions..."), {});
    } else {
        startupOverlay_->hideOverlay();
    }
    applyBackgroundTaskRunningState(running);
}

void MainWindow::setCoreStartupChecklist(const QStringList& items)
{
    coreStartupChecklistVisible_ = true;
    startupOverlay_->showChecklist(tr("Starting system proxy..."), items);
    coreStartupChecklistFailed_ = startupOverlay_->isChecklistFailed();
    updateActionState();
}

void MainWindow::clearCoreStartupChecklist()
{
    coreStartupChecklistVisible_ = false;
    coreStartupChecklistFailed_ = false;
    startupOverlay_->hideOverlay();
    updateActionState();
}

void MainWindow::setBackgroundTaskRunning(bool running)
{
    if (backgroundTaskRunning_ == running) {
        return;
    }

    applyBackgroundTaskRunningState(running);
}

void MainWindow::setBackgroundTaskDescription(const QString& description)
{
    backgroundTaskDescription_ = description.trimmed();
    updateStatusPresentation();
}

void MainWindow::setAvailableAppUpdateVersion(const QString& version)
{
    const QString trimmedVersion = version.trimmed();
    appUpdateAvailable_ = !trimmedVersion.isEmpty();
    appUpdateStatusText_ = appUpdateAvailable_
        ? tr("Update available: %1").arg(trimmedVersion)
        : QString();
    updateStatusPresentation();
}

void MainWindow::clearAvailableAppUpdateVersion()
{
    if (!appUpdateAvailable_ && appUpdateStatusText_.isEmpty()) {
        return;
    }

    appUpdateAvailable_ = false;
    appUpdateStatusText_.clear();
    updateStatusPresentation();
}

void MainWindow::applyRuntimeState(const RuntimeStateSnapshot& snapshot)
{
    currentServerName_ = snapshot.currentServerName.trimmed();
    currentServerLocation_ = snapshot.currentServerLocation.trimmed();
    currentServerWarning_ = snapshot.currentServerWarning.trimmed();
    routingSummary_ = snapshot.routingSummary.trimmed();
    listenSummary_ = snapshot.listenSummary.trimmed();
    proxyUiState_ = snapshot.proxyUiState;
    systemProxyApplied_ = snapshot.systemProxyApplied;

    updateActionState();
    updateStatusPresentation(true);
}

void MainWindow::setTunEnabled(bool enabled)
{
    tunEnabled_ = enabled;
    updateRuntimeUiState();
}

void MainWindow::restoreUiState(const Config& config)
{
    refreshToolbarIcons();

    if (windowLayoutStateController_ != nullptr) {
        windowLayoutStateController_->restoreFromConfig(config);
    }
    if (serverListController_ != nullptr) {
        serverListController_->restoreSortState(config);
    }
    qrPreviewVisible_ = config.ui().mainQrPreviewVisible;
    if (sharePanel_ != nullptr) {
        sharePanel_->setPreviewVisible(qrPreviewVisible_);
    }
    proxyUiState_ = ProxyUiState::Idle;
    systemProxyApplied_ = false;
    tunEnabled_ = config.tun().tunModeItem.enableTun;
    updateRuntimeUiState();
    updateQrPanelActionText();
    if (compactMode_ && sharePanel_ != nullptr) {
        sharePanel_->hide();
    }

    if (subscriptionTabBar_ != nullptr && serverModel_ != nullptr) {
        const QString preferredTabKey = ServerSelection::restoredSubscriptionTabKey(
            config.ui().mainSelectedSubId,
            config.currentIndexId,
            serverModel_);
        for (int index = 0; index < subscriptionTabBar_->count(); ++index) {
            if (subscriptionTabBar_->tabData(index).toString() == preferredTabKey) {
                subscriptionTabBar_->setCurrentIndex(index);
                break;
            }
        }
    }

}

void MainWindow::captureUiState(Config& config) const
{
    if (windowLayoutStateController_ != nullptr) {
        windowLayoutStateController_->captureToConfig(config);
    }
    config.ui().mainQrPreviewVisible = qrPreviewVisible_;
    config.ui().mainSelectedSubId = subscriptionViewController_ == nullptr
        ? QStringLiteral("__unsubscribed__")
        : subscriptionViewController_->persistedSelectionId();
    if (serverListController_ != nullptr) {
        serverListController_->captureSortState(config);
    }
}

bool MainWindow::selectSubscriptionTab(const QString& selectionId)
{
    return subscriptionViewController_ != nullptr
        && subscriptionViewController_->selectSubscriptionTab(selectionId);
}

void MainWindow::updateActionState()
{
    const bool canStartTask = canStartBackgroundTask();
    if (serverActionsController_ != nullptr) {
        const ServerActionsController::SelectionSnapshot snapshot =
            makeServerActionSelectionSnapshot(
                serverSelectionController_,
                serverModel_,
                selectedShareLinks(),
                canStartTask);
        serverActionsController_->syncActionState(serverActionsController_->buildActionState(snapshot));
    }

    if (backgroundTaskActionsController_ != nullptr) {
        backgroundTaskActionsController_->sync(BackgroundTaskActionsController::State{canStartTask});
    }

    syncProxyToolbarController();
}

void MainWindow::refreshToolbarIcons()
{
    for (auto it = toolbarIconFiles_.cbegin(); it != toolbarIconFiles_.cend(); ++it) {
        if (it.key() != nullptr) {
            it.key()->setIcon(ToolbarWidgets::loadIcon(it.value()));
        }
    }
    for (auto it = toolbarStandaloneIconFiles_.cbegin(); it != toolbarStandaloneIconFiles_.cend(); ++it) {
        if (it.key() != nullptr) {
            it.key()->setIcon(ToolbarWidgets::loadIcon(it.value()));
        }
    }
    if (moveServerUpAction_ != nullptr) {
        moveServerUpAction_->setIcon(AppTheme::themedSvgIcon(QStringLiteral(":/app/up.svg")));
    }
    if (moveServerDownAction_ != nullptr) {
        moveServerDownAction_->setIcon(AppTheme::themedSvgIcon(QStringLiteral(":/app/down.svg")));
    }
}

void MainWindow::refreshThemeAssets()
{
    refreshToolbarIcons();
}

void MainWindow::updateQrPreview()
{
    if (sharePanel_ == nullptr) {
        return;
    }

    sharePanel_->setShareLinks(selectedShareLinks());
}

void MainWindow::updateQrPanelActionText()
{
    if (toggleQrPanelAction_ != nullptr) {
        const QSignalBlocker blocker(toggleQrPanelAction_);
        if (windowLayoutStateController_ != nullptr) {
            qrPreviewVisible_ = windowLayoutStateController_->qrPreviewVisible();
        }
        toggleQrPanelAction_->setChecked(qrPreviewVisible_);
    }
}

void MainWindow::updateSelectionForVisibleRows()
{
    if (serverSelectionController_ != nullptr) {
        serverSelectionController_->updateSelectionForVisibleRows();
    }
}

void MainWindow::showCurrentServerInTable()
{
    if (currentIndexId_.trimmed().isEmpty()) {
        return;
    }
    if (serverModel_ == nullptr
        || serverFilterModel_ == nullptr
        || serverView_ == nullptr
        || serverView_->selectionModel() == nullptr) {
        return;
    }

    const ServerTableRow* currentServer = serverModel_->itemByIndexId(currentIndexId_);
    if (currentServer == nullptr) {
        return;
    }

    if (serverFilterEdit_ != nullptr && !serverFilterEdit_->text().trimmed().isEmpty()) {
        serverFilterEdit_->clear();
    }

    if (subscriptionViewController_ != nullptr) {
        subscriptionViewController_->selectSubscriptionTab(ServerSelection::subscriptionSelectionIdForServer(*currentServer));
        subscriptionViewController_->applyCurrentTabFilter();
    }

    const QModelIndex targetIndex = ServerSelection::visibleProxyIndexForServer(
        serverFilterModel_,
        serverModel_,
        currentIndexId_);
    if (!targetIndex.isValid()) {
        return;
    }

    serverView_->setCurrentIndex(targetIndex);
    serverView_->selectRow(targetIndex.row());
    serverView_->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
    serverView_->setFocus(Qt::OtherFocusReason);
}

void MainWindow::applyBackgroundTaskRunningState(bool running)
{
    backgroundTaskRunning_ = running;
    setServerTableDynamicSortEnabled(!running, !running);
    updateActionState();
    updateStatusPresentation();
}

void MainWindow::scheduleInitialCurrentServerReveal()
{
    if (initialCurrentServerRevealDone_
        || initialCurrentServerRevealScheduled_
        || currentIndexId_.trimmed().isEmpty()
        || serverModel_ == nullptr
        || serverModel_->rowCount() <= 0) {
        return;
    }

    initialCurrentServerRevealScheduled_ = true;
    QTimer::singleShot(0, this, [this]() {
        QTimer::singleShot(0, this, [this]() {
            initialCurrentServerRevealScheduled_ = false;
            if (initialCurrentServerRevealDone_ || currentIndexId_.trimmed().isEmpty()) {
                return;
            }

            initialCurrentServerRevealDone_ = true;
            showCurrentServerInTable();
        });
    });
}

bool MainWindow::canStartBackgroundTask() const
{
    return backgroundTaskBlockReason(
        backgroundTaskRunning_,
        proxyUiState_,
        coreStartupChecklistVisible_,
        coreStartupChecklistFailed_) == BackgroundTaskBlockReason::None;
}

bool MainWindow::ensureCanStartBackgroundTask()
{
    const BackgroundTaskBlockReason blockReason = backgroundTaskBlockReason(
        backgroundTaskRunning_,
        proxyUiState_,
        coreStartupChecklistVisible_,
        coreStartupChecklistFailed_);
    if (blockReason == BackgroundTaskBlockReason::None) {
        return true;
    }

    updateActionState();
    return false;
}

bool MainWindow::isUngroupedSubscriptionTabSelected() const
{
    return subscriptionViewController_ != nullptr
        && subscriptionViewController_->isUngroupedTabSelected();
}

void MainWindow::setupUi()
{
    setMinimumSize(MinimumMainWindowWidth, MinimumMainWindowHeight);
    resize(DefaultMainWindowWidth, DefaultMainWindowHeight);

    mainContentWidget_ = new QWidget(this);
    mainContentWidget_->setObjectName(QStringLiteral("mainContentWidget"));
    mainContentLayout_ = new QVBoxLayout(mainContentWidget_);
    mainContentLayout_->setContentsMargins(0, 0, 0, 0);
    mainContentLayout_->setSpacing(0);
    setCentralWidget(mainContentWidget_);

    setupToolbar();
    setupServerView();
    setupDiagnosticsPanel();

    startupOverlay_ = new StartupOverlayWidget(this);
    connect(startupOverlay_, &StartupOverlayWidget::retryRequested,
            this, &MainWindow::retryCoreStartupRequested);
    connect(startupOverlay_, &StartupOverlayWidget::dismissRequested, this, [this]() {
        coreStartupChecklistVisible_ = false;
        coreStartupChecklistFailed_ = false;
        startupOverlay_->hideOverlay();
        updateActionState();
    });

    updateWindowTitle();
}

void MainWindow::setupToolbar()
{
    auto* toolbarHost = new QWidget(mainContentWidget_);
    toolbarHost->setObjectName(QStringLiteral("mainToolbarHost"));
    toolbarHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toolbarHost->setFixedHeight(ToolbarWidgets::ControlHeight + (ToolbarWidgets::Padding * 2) + ToolbarWidgets::BottomBorderWidth);
    auto* toolbarHostLayout = new QVBoxLayout(toolbarHost);
    toolbarHostLayout->setContentsMargins(ToolbarWidgets::Padding, ToolbarWidgets::Padding, ToolbarWidgets::Padding, ToolbarWidgets::Padding);
    toolbarHostLayout->setSpacing(0);

    auto* toolBar = new QToolBar(tr("Main"), toolbarHost);
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    AppTheme::applyCompactFont(toolBar);
    const int iconExtent = qRound(toolBar->fontMetrics().height() * 1.2);
    toolBar->setIconSize(QSize(iconExtent, iconExtent));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar->setContentsMargins(0, 0, 0, 0);
    toolBar->setFixedHeight(ToolbarWidgets::ControlHeight);
    toolbarHostLayout->addWidget(toolBar, 0, Qt::AlignTop);
    mainContentLayout_->addWidget(toolbarHost);

    createToolbarActions();
    QMenu* helpMenu = nullptr;
    createToolbarMenus(toolBar, helpMenu);
    populateToolbarWidgets(toolBar, helpMenu);
    initializeToolbarControllers();
}

void MainWindow::createToolbarActions()
{
    const MainWindowToolbarActions actions = createMainWindowToolbarActions(this, qrPreviewVisible_);
    addServerAction_ = actions.addServerAction;
    editServerAction_ = actions.editServerAction;
    copyUrlAction_ = actions.copyUrlAction;
    copyShareLinkAction_ = actions.copyShareLinkAction;
    importClipboardAction_ = actions.importClipboardAction;
    subAction_ = actions.subAction;
    routingSettingsAction_ = actions.routingSettingsAction;
    updateSubscriptionsAction_ = actions.updateSubscriptionsAction;
    updateCoreActions_ = actions.updateCoreActions;
    updateGeoResourcesAction_ = actions.updateGeoResourcesAction;
    removeServerAction_ = actions.removeServerAction;
    moveServerTopAction_ = actions.moveServerTopAction;
    moveServerUpAction_ = actions.moveServerUpAction;
    moveServerDownAction_ = actions.moveServerDownAction;
    moveServerBottomAction_ = actions.moveServerBottomAction;
    testAction_ = actions.testAction;
    setDefaultServerAction_ = actions.setDefaultServerAction;
    setDefaultServerWithTunAction_ = actions.setDefaultServerWithTunAction;
    settingsAction_ = actions.settingsAction;
    aboutAction_ = actions.aboutAction;
    checkAppUpdateAction_ = actions.checkAppUpdateAction;
    uwpLoopbackAction_ = actions.uwpLoopbackAction;
    proxyToggleAction_ = actions.proxyToggleAction;
    tunToggleAction_ = actions.tunToggleAction;
    updateCurrentSubscriptionShortcutAction_ = actions.updateCurrentSubscriptionShortcutAction;
    toggleQrPanelAction_ = actions.toggleQrPanelAction;
    toolbarIconFiles_ = actions.iconFiles;

    connect(subAction_, &QAction::triggered, this, &MainWindow::openSettingsAtSubscriptionsTabRequested);
    connect(routingSettingsAction_, &QAction::triggered, this, &MainWindow::openSettingsAtRoutingTabRequested);
}

void MainWindow::createToolbarMenus(QToolBar* toolBar, QMenu*& helpMenu)
{
    updateCurrentSubscriptionAction_ = new QAction(tr("Update Current Subscription"), this);
    updateCurrentSubscriptionAction_->setObjectName(QStringLiteral("updateCurrentSubscriptionMenuAction"));
    connect(updateCurrentSubscriptionAction_, &QAction::triggered, this, [this]() {
        triggerCurrentSubscriptionUpdate();
    });

    helpMenu = new QMenu(tr("Help"), toolBar);
    helpMenu->setObjectName(QStringLiteral("helpMenu"));
    helpMenu->addAction(aboutAction_);
    helpMenu->addAction(checkAppUpdateAction_);
    helpMenu->addSeparator();
    helpMenu->addAction(uwpLoopbackAction_);
    helpMenu->addSeparator();
    helpMenu->addAction(updateCurrentSubscriptionAction_);
    helpMenu->addAction(updateSubscriptionsAction_);
    helpMenu->addSeparator();
    helpMenu->addAction(updateGeoResourcesAction_);
    for (QAction* action : updateCoreActions_.values()) {
        helpMenu->addAction(action);
    }
}

void MainWindow::populateToolbarWidgets(QToolBar* toolBar, QMenu* helpMenu)
{
    ToolbarWidgets::createActionButton(
        toolBar,
        QStringLiteral("settingButton"),
        tr("Settings"),
        ToolbarWidgets::loadIcon(QStringLiteral("option.svg")),
        settingsAction_);
    QWidget* settingsSpacing = ToolbarWidgets::createSpacing(toolBar, ToolbarWidgets::ControlSpacing);
    desktopOnlyToolbarWidgetActions_.append(toolBar->addWidget(settingsSpacing));
    desktopOnlyToolbarWidgets_.append(settingsSpacing);
    QToolButton* subButton = ToolbarWidgets::createActionButton(
        toolBar,
        QStringLiteral("subButton"),
        tr("Subscriptions"),
        ToolbarWidgets::loadIcon(QStringLiteral("subscription.svg")),
        subAction_);
    desktopOnlyToolbarWidgets_.append(subButton);
    desktopOnlyToolbarWidgetActions_.append(toolbarWidgetAction(subButton));
    QWidget* subSpacing = ToolbarWidgets::createSpacing(toolBar, ToolbarWidgets::ControlSpacing);
    desktopOnlyToolbarWidgetActions_.append(toolBar->addWidget(subSpacing));
    desktopOnlyToolbarWidgets_.append(subSpacing);
    QToolButton* routingButton = ToolbarWidgets::createActionButton(
        toolBar,
        QStringLiteral("routingButton"),
        tr("Routing"),
        ToolbarWidgets::loadIcon(QStringLiteral("routing.svg")),
        routingSettingsAction_);
    desktopOnlyToolbarWidgets_.append(routingButton);
    desktopOnlyToolbarWidgetActions_.append(toolbarWidgetAction(routingButton));
    QWidget* routingSpacing = ToolbarWidgets::createSpacing(toolBar, ToolbarWidgets::ControlSpacing);
    desktopOnlyToolbarWidgetActions_.append(toolBar->addWidget(routingSpacing));
    desktopOnlyToolbarWidgets_.append(routingSpacing);

    auto* helpButton = ToolbarWidgets::createMenuButton(
        toolBar,
        QStringLiteral("helpButton"),
        tr("Help"),
        ToolbarWidgets::loadIcon(QStringLiteral("help.svg")),
        helpMenu);
    toolbarStandaloneIconFiles_.insert(helpButton, QStringLiteral("help.svg"));
    desktopOnlyToolbarWidgets_.append(helpButton);
    desktopOnlyToolbarWidgetActions_.append(toolbarWidgetAction(helpButton));

    toolbarFlexibleSpacer_ = new QWidget(toolBar);
    toolbarFlexibleSpacer_->setObjectName(QStringLiteral("toolbarFlexibleSpacer"));
    toolbarFlexibleSpacer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbarFlexibleSpacerAction_ = toolBar->addWidget(toolbarFlexibleSpacer_);

    auto* proxyToggleButton = ToolbarWidgets::createActionButton(
        toolBar,
        QStringLiteral("proxyToggleButton"),
        tr("START"),
        QIcon(),
        proxyToggleAction_);
    proxyToggleButton->setFixedWidth(ProxyToggleButtonWidth);
    toolBar->addWidget(ToolbarWidgets::createSpacing(toolBar, ToolbarWidgets::ControlSpacing));
    ToolbarWidgets::createActionButton(
        toolBar,
        QStringLiteral("tunToggleButton"),
        tr("TUN"),
        QIcon(),
        tunToggleAction_);
    toolBar->addWidget(ToolbarWidgets::createSpacing(toolBar, ToolbarWidgets::ControlSpacing));

    routingModeCombo_ = ToolbarWidgets::createStyledComboBox(toolBar);
    routingModeCombo_->setObjectName(QStringLiteral("routingModeCombo"));
    AppTheme::applyCompactFont(routingModeCombo_);
    routingModeCombo_->setFixedHeight(ToolbarWidgets::ControlHeight);
    routingModeCombo_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    routingModeCombo_->setFixedWidth(RoutingModeComboWidth);
    ToolbarWidgets::configureStyledComboBox(routingModeCombo_);
    toolBar->addWidget(routingModeCombo_);
    QWidget* routingComboSpacing = ToolbarWidgets::createSpacing(toolBar, ToolbarWidgets::ControlSpacing);
    desktopOnlyToolbarWidgetActions_.append(toolBar->addWidget(routingComboSpacing));
    desktopOnlyToolbarWidgets_.append(routingComboSpacing);

    QToolButton* qrButton = ToolbarWidgets::createActionButton(
        toolBar,
        QStringLiteral("qrCodeButton"),
        tr("Share"),
        QIcon(),
        toggleQrPanelAction_);
    desktopOnlyToolbarWidgets_.append(qrButton);
    desktopOnlyToolbarWidgetActions_.append(toolbarWidgetAction(qrButton));
}

void MainWindow::initializeToolbarControllers()
{
    backgroundTaskActionsController_ = new BackgroundTaskActionsController({
        importClipboardAction_,
        updateSubscriptionsAction_,
        updateCurrentSubscriptionAction_,
        updateCoreActions_.values(),
        updateGeoResourcesAction_,
        updateCurrentSubscriptionShortcutAction_});
    proxyToolbarController_ = new ProxyToolbarController(proxyToggleAction_, tunToggleAction_);
    routingModeController_ = new RoutingModeController(routingModeCombo_, [this](const QString& routingModeId) {
        emit routingModeSelected(routingModeId);
    }, this);
    routingModeController_->setup();
    syncProxyToolbarController();
    updateQrPanelActionText();
}

void MainWindow::setupServerView()
{
    setupServerWorkspace();
    setupSubscriptionViewController();
    setupServerListController();
}

void MainWindow::setupServerWorkspace()
{
    serverModel_ = new ServerTableModel(this);
    serverFilterModel_ = new ServerFilterProxyModel(this);
    serverFilterModel_->setSourceModel(serverModel_);
    serverFilterModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    serverFilterModel_->setFilterKeyColumn(-1);
    serverFilterModel_->setDynamicSortFilter(true);
    serverFilterModel_->sort(-1);

    serverWorkspace_ = new ServerWorkspaceWidget(
        serverFilterModel_,
        qrPreviewVisible_,
        WindowLayoutStateController::defaultServerColumnWidths(),
        this);
    serverView_ = serverWorkspace_->serverView();
    subscriptionTabBar_ = serverWorkspace_->subscriptionTabBar();
    serverFilterEdit_ = serverWorkspace_->serverFilterEdit();
    sharePanel_ = serverWorkspace_->sharePanel();
    windowLayoutStateController_ = new WindowLayoutStateController(
        this,
        serverWorkspace_,
        serverView_,
        sharePanel_);
    qrPreviewVisible_ = windowLayoutStateController_->qrPreviewVisible();
    if (mainContentLayout_ != nullptr) {
        mainContentLayout_->addWidget(serverWorkspace_, 1);
    }
}

void MainWindow::setupSubscriptionViewController()
{
    subscriptionViewController_ = new SubscriptionViewController(
        {
            this,
            subscriptionTabBar_,
            serverFilterEdit_,
            serverFilterModel_},
        {
            [this]() { return canStartBackgroundTask(); },
            [this]() { copyCurrentSubscriptionUrlToClipboard(); },
            [this](const QString& subscriptionId) { testSubscriptionServers(subscriptionId); },
            [this](const QString& subscriptionId) { emit updateCurrentSubscriptionRequested(subscriptionId); },
            [this](const QString& subscriptionId) { emit updateCurrentSubscriptionViaProxyRequested(subscriptionId); },
            [this](const QString& subscriptionId) { emit hideSubscriptionRequested(subscriptionId); },
            [this](const QString& subscriptionId) {
                if (DialogUtils::askYesNoQuestion(
                        this,
                        tr("Delete Subscription"),
                        tr("Delete the selected subscription and all servers inside it?"),
                        QMessageBox::No)
                    == QMessageBox::Yes) {
                    emit deleteSubscriptionRequested(subscriptionId);
                    return true;
                }
                return false;
            },
            [this]() { emit openSettingsAtSubscriptionsTabRequested(); }});
    if (serverWorkspace_ != nullptr) {
        serverWorkspace_->setCompactSelectionHandler([this](const QString&) {
            if (serverListController_ != nullptr) {
                serverListController_->handleSubscriptionFilterChanged();
            }
            if (serverWorkspace_ != nullptr) {
                serverWorkspace_->syncCompactSections();
            }
        });
        serverWorkspace_->setCompactContextMenuHandler([this](const QString& tabKey, const QPoint& globalPosition) {
            if (subscriptionViewController_ != nullptr) {
                subscriptionViewController_->showContextMenuForTabKey(tabKey, globalPosition);
            }
        });
    }
}

void MainWindow::setupServerListController()
{
    serverListController_ = new ServerListController(
        {
            serverView_,
            serverFilterEdit_,
            serverFilterModel_,
            serverModel_},
        [this](const QString& text) {
            if (subscriptionViewController_ != nullptr) {
                subscriptionViewController_->applyFilter(text);
            }
        },
        [this]() {
            if (subscriptionViewController_ != nullptr) {
                subscriptionViewController_->applyCurrentTabFilter();
            }
        },
        [this]() { updateSelectionForVisibleRows(); },
        [this]() { updateActionState(); },
        [this]() { updateQrPreview(); },
        [this](const QStringList& reorderedIds) { emit reorderServersRequested(reorderedIds); },
        this);
    serverListController_->setup();
    serverListController_->updateReorderAvailability();
}

void MainWindow::setupDiagnosticsPanel()
{
    setupStatusBarController();
    setupServerSelectionController();
    setupServerActionsController();
    syncStatusBarController();
    updateActionState();
}

void MainWindow::setupStatusBarController()
{
    statusBarController_ = new StatusBarController(
        this,
        statusBar(),
        [this]() { emit settingsRequested(); },
        [this]() { showCurrentServerInTable(); },
        [this]() { emit downloadAppUpdateRequested(); });
}

void MainWindow::setupServerSelectionController()
{
    serverSelectionController_ = new ServerSelectionController(
        serverView_,
        serverFilterModel_,
        serverModel_,
        [this]() { return currentIndexId_; },
        [this]() { refreshServerSelectionUi(); });
    serverSelectionController_->setup();
}

void MainWindow::setupServerActionsController()
{
    serverActionsController_ = new ServerActionsController(
        {
            this,
            serverView_,
            addServerAction_,
            editServerAction_,
            copyUrlAction_,
            copyShareLinkAction_,
            importClipboardAction_,
            removeServerAction_,
            moveServerTopAction_,
            moveServerUpAction_,
            moveServerDownAction_,
            moveServerBottomAction_,
            testAction_,
            setDefaultServerAction_,
            setDefaultServerWithTunAction_},
        [this]() { return serverSelectionController_ == nullptr ? QString() : serverSelectionController_->selectedServerId(); },
        [this]() { return serverSelectionController_ == nullptr ? QStringList() : serverSelectionController_->selectedServerIds(); },
        [this]() { return isUngroupedSubscriptionTabSelected(); },
        [this]() { return ensureCanStartBackgroundTask(); },
        [this]() { updateActionState(); },
        [this](const QString& message) { appendLog(message); },
        [this]() { return selectedShareLinks(); });
    serverActionsController_->setup();
}

void MainWindow::setupConnections()
{
    setupGeneralActionConnections();
    setupToggleConnections();
    setupViewConnections();
    setupShortcutController();
}

void MainWindow::setupGeneralActionConnections()
{
    connect(addServerAction_, &QAction::triggered, this, &MainWindow::addServerRequested);
    setupImportActionConnections();
    setupUpdateActionConnections();
    setupSettingsActionConnections();
    setupHelpActionConnections();
}

void MainWindow::setupImportActionConnections()
{
    connect(importClipboardAction_, &QAction::triggered, this, &MainWindow::importFromClipboardRequested);
}

void MainWindow::setupUpdateActionConnections()
{
    connect(updateSubscriptionsAction_, &QAction::triggered, this, [this]() {
        if (ensureCanStartBackgroundTask()) {
            emit updateSubscriptionsRequested();
        }
    });
    for (auto it = updateCoreActions_.cbegin(); it != updateCoreActions_.cend(); ++it) {
        const int coreType = it.key();
        connect(it.value(), &QAction::triggered, this, [this, coreType]() {
            emit updateCoreRequested(coreType);
        });
    }
    connect(updateGeoResourcesAction_, &QAction::triggered, this, &MainWindow::updateGeoResourcesRequested);
}

void MainWindow::setupSettingsActionConnections()
{
    connect(settingsAction_, &QAction::triggered, this, &MainWindow::settingsRequested);
}

void MainWindow::setupHelpActionConnections()
{
    connect(aboutAction_, &QAction::triggered, this, &MainWindow::aboutRequested);
    connect(checkAppUpdateAction_, &QAction::triggered, this, &MainWindow::checkAppUpdateRequested);
    connect(uwpLoopbackAction_, &QAction::triggered, this, &MainWindow::uwpLoopbackRequested);
}

void MainWindow::setupToggleConnections()
{
    connect(proxyToggleAction_, &QAction::triggered, this, [this]() {
        if (proxyToolbarController_ == nullptr) {
            return;
        }

        ProxyToolbarController::Snapshot snapshot;
        snapshot.uiState = proxyUiState_;
        snapshot.outboundLocationAvailable = !currentServerLocation_.trimmed().isEmpty();
        snapshot.tunEnabled = tunEnabled_;
        snapshot.existingCoreTypes = existingCoreTypes_;
        snapshot.coreTypeItems = configSnapshot_.coreTypeItems;
        const ProxyToggleDecision decision = decideProxyToggle(
            snapshot,
            activeServer(),
            serverSelectionController_ == nullptr ? nullptr : serverSelectionController_->selectedServer());
        switch (decision.command) {
        case ProxyToggleCommand::DisableProxy:
            emit disableSystemProxyRequested();
            return;
        case ProxyToggleCommand::EnableProxy:
            emit enableSystemProxyRequested();
            return;
        case ProxyToggleCommand::PromoteSelectedServer:
            emit setDefaultServerRequested(decision.selectedServerId);
            updateActionState();
            return;
        case ProxyToggleCommand::SelectServer:
            showSelectServerHint();
            updateActionState();
            return;
        case ProxyToggleCommand::None:
            return;
        }
    });
    connect(tunToggleAction_, &QAction::triggered, this, [this](bool checked) {
        if (shouldIgnoreTunToggle(proxyUiState_)) {
            updateActionState();
            return;
        }
        if (activeServer() == nullptr) {
            showSelectServerHint();
            updateActionState();
            return;
        }

        emit tunEnabledChanged(checked);
    });
    connect(toggleQrPanelAction_, &QAction::triggered, this, [this](bool checked) {
        if (windowLayoutStateController_ != nullptr) {
            windowLayoutStateController_->handleQrPreviewToggled(checked);
            qrPreviewVisible_ = windowLayoutStateController_->qrPreviewVisible();
        }
        updateQrPanelActionText();
    });
}

void MainWindow::setupViewConnections()
{
    setupSubscriptionViewConnections();
    setupSplitterViewConnections();
}

void MainWindow::setupSubscriptionViewConnections()
{
    connect(subscriptionTabBar_, &QTabBar::currentChanged, this, [this](int) {
        if (serverListController_ != nullptr) {
            serverListController_->handleSubscriptionFilterChanged();
        }
        if (serverWorkspace_ != nullptr) {
            serverWorkspace_->syncCompactSections();
        }
    });
    connect(subscriptionTabBar_, &QTabBar::customContextMenuRequested, this, [this](const QPoint& position) {
        if (subscriptionViewController_ != nullptr) {
            subscriptionViewController_->showTabContextMenu(position);
        }
    });
}

void MainWindow::setupSplitterViewConnections()
{
    if (serverWorkspace_ != nullptr && serverWorkspace_->topSplitter() != nullptr) {
        connect(serverWorkspace_->topSplitter(), &QSplitter::splitterMoved, this, [this](int, int) {
            if (windowLayoutStateController_ != nullptr) {
                windowLayoutStateController_->handleTopSplitterMoved();
            }
        });
    }

    if (serverWorkspace_ != nullptr && serverWorkspace_->rootSplitter() != nullptr) {
        connect(serverWorkspace_->rootSplitter(), &QSplitter::splitterMoved, this, [this](int, int) {
            if (windowLayoutStateController_ != nullptr) {
                windowLayoutStateController_->handleRootSplitterMoved();
            }
        });
    }
}

void MainWindow::setupShortcutController()
{
    shortcutController_ = new MainWindowShortcutController(
        {
            this,
            serverFilterEdit_,
            subscriptionTabBar_,
            testAction_,
            updateSubscriptionsAction_,
            updateCurrentSubscriptionShortcutAction_},
        {
            [this]() { triggerCurrentSubscriptionUpdate(); },
            [this]() { requestExit(); }},
        this);
    shortcutController_->setup();
}

void MainWindow::showServerContextMenu(const QPoint& position)
{
    if (serverActionsController_ != nullptr) {
        serverActionsController_->showContextMenu(position);
    }
}

void MainWindow::copyCurrentSubscriptionUrlToClipboard()
{
    const QString url = subscriptionViewController_ == nullptr
        ? QString()
        : subscriptionViewController_->currentSubscriptionUrl();
    if (url.isEmpty()) {
        return;
    }

    if (QApplication::clipboard() != nullptr) {
        QApplication::clipboard()->setText(url);
    }

    appendLog(tr("Copied subscription URL to the clipboard."));
}

void MainWindow::triggerCurrentSubscriptionUpdate()
{
    if (!ensureCanStartBackgroundTask()) {
        return;
    }

    const QString tabKey = subscriptionViewController_ == nullptr
        ? QStringLiteral("ungrouped")
        : subscriptionViewController_->currentTabKey();
    emit updateCurrentSubscriptionRequested(subscriptionIdFromTabKey(tabKey));
}

void MainWindow::testSubscriptionServers(const QString& subscriptionId)
{
    if (!ensureCanStartBackgroundTask()) {
        return;
    }

    const QString normalizedId = subscriptionId.trimmed();
    if (normalizedId.isEmpty() || serverModel_ == nullptr) {
        return;
    }

    const QStringList indexIds = ServerSelection::subscriptionServerIndexIds(serverModel_, normalizedId);
    if (indexIds.isEmpty()) {
        return;
    }

    emit testServersRequested(indexIds);
}

bool MainWindow::confirmExit()
{
    return DialogUtils::askYesNoQuestion(
               this,
               tr("Quit SongBird"),
               tr("Quit SongBird now?"),
               QMessageBox::No)
        == QMessageBox::Yes;
}

void MainWindow::updateWindowTitle()
{
    setWindowTitle(WindowTitle::formatWindowTitle(
        currentCoreName_,
        currentServerName_,
        proxyUiState_,
        tunEnabled_,
        fontMetrics(),
        WindowTitleServerNameMaximumWidth));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (hideToTrayEnabled_ && !allowClose_) {
        hide();
        emit hiddenToTray();
        event->ignore();
        return;
    }

    if (!allowClose_ && !confirmExit()) {
        allowClose_ = false;
        event->ignore();
        return;
    }

    allowClose_ = true;
    QTimer::singleShot(0, qApp, []() {
        QCoreApplication::quit();
    });
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    applyCompactMode(width() < CompactMainWindowWidth);
    if (startupOverlay_ != nullptr) {
        startupOverlay_->updateGeometry(rect());
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (statusBarController_ != nullptr && statusBarController_->handleEvent(watched, event)) {
        return true;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    if (windowLayoutStateController_ != nullptr) {
        windowLayoutStateController_->handleShowEvent();
        qrPreviewVisible_ = windowLayoutStateController_->qrPreviewVisible();
    }
    QTimer::singleShot(0, this, [this]() {
        if (routingModeController_ != nullptr) {
            routingModeController_->setRoutingOptions(configSnapshot_.routingItems, configSnapshot_.routingModeId);
        }
        updateQrPreview();
        updateQrPanelActionText();
        if (statusBarController_ != nullptr) {
            statusBarController_->refreshBackgroundTaskStatusLabel(isVisible());
        }
    });
}

const ServerTableRow* MainWindow::activeServer() const
{
    return serverModel_ == nullptr ? nullptr : serverModel_->currentItem();
}

void MainWindow::showSelectServerHint()
{
    if (serverView_ != nullptr) {
        serverView_->flashAttention();
    }
}

void MainWindow::refreshServerSelectionUi()
{
    updateQrPreview();
    updateActionState();
}

QStringList MainWindow::selectedShareLinks() const
{
    return serverSelectionController_ == nullptr
        ? QStringList()
        : serverSelectionController_->selectedShareLinks(shareUrlResolver_);
}

void MainWindow::preserveServerSelectionPreference()
{
    if (serverSelectionController_ == nullptr) {
        return;
    }

    const QString preferredSelectedId = serverSelectionController_->selectedServerId().trimmed();
    if (!preferredSelectedId.isEmpty()) {
        serverSelectionController_->setPreferredSelectionId(preferredSelectedId);
    }
}

void MainWindow::syncConfigControllers(const Config& config)
{
    if (routingModeController_ != nullptr) {
        routingModeController_->setRoutingOptions(RoutingProfiles::routingItems(config.collection()), config.collection().routingModeId);
    }
    if (subscriptionViewController_ != nullptr) {
        subscriptionViewController_->setSubscriptions(config.collection().subscriptions);
        subscriptionViewController_->rebuildTabs(subscriptionViewController_->currentTabKey());
        subscriptionViewController_->applyCurrentTabFilter();
        if (serverWorkspace_ != nullptr) {
            serverWorkspace_->syncCompactSections();
        }
    }
}

void MainWindow::updateRuntimeUiState()
{
    updateActionState();
    updateStatusPresentation(true);
}

void MainWindow::updateStatusPresentation(bool includeWindowTitle)
{
    syncStatusBarController();
    if (includeWindowTitle) {
        updateWindowTitle();
    }
}

void MainWindow::syncStatusBarController()
{
    if (statusBarController_ == nullptr) {
        return;
    }

    statusBarController_->refresh(
        currentServerName_,
        currentServerLocation_,
        currentServerWarning_,
        listenSummary_,
        backgroundTaskRunning_,
        backgroundTaskDescription_,
        appVersionStatusText_,
        appUpdateStatusText_,
        appUpdateAvailable_);
}

void MainWindow::syncProxyToolbarController()
{
    if (proxyToolbarController_ == nullptr) {
        return;
    }

    ProxyToolbarController::Snapshot snapshot;
    snapshot.uiState = proxyUiState_;
    snapshot.outboundLocationAvailable = !currentServerLocation_.trimmed().isEmpty();
    snapshot.tunEnabled = tunEnabled_;
    snapshot.existingCoreTypes = existingCoreTypes_;
    snapshot.coreTypeItems = configSnapshot_.coreTypeItems;
    proxyToolbarController_->sync(snapshot, activeServer());
}

void MainWindow::applyCompactMode(bool compact)
{
    if (compactMode_ == compact) {
        return;
    }

    compactMode_ = compact;
    applyToolbarCompactMode(compactMode_);
    applyServerTableCompactMode(compactMode_);
    if (serverWorkspace_ != nullptr) {
        serverWorkspace_->setCompactMode(compactMode_);
    }
    if (statusBarController_ != nullptr) {
        statusBarController_->setCompactMode(compactMode_);
    }
    if (serverWorkspace_ != nullptr && serverWorkspace_->logPanel() != nullptr) {
        serverWorkspace_->logPanel()->setCompactMode(compactMode_);
    }
}

void MainWindow::applyToolbarCompactMode(bool compact)
{
    for (QWidget* widget : desktopOnlyToolbarWidgets_) {
        if (widget != nullptr) {
            widget->setVisible(!compact);
        }
    }
    if (toolbarFlexibleSpacer_ != nullptr) {
        toolbarFlexibleSpacer_->setVisible(true);
    }
    if (toolbarFlexibleSpacerAction_ != nullptr) {
        toolbarFlexibleSpacerAction_->setVisible(true);
    }
    for (QAction* action : desktopOnlyToolbarWidgetActions_) {
        if (action != nullptr) {
            action->setVisible(!compact);
        }
    }
}

void MainWindow::applyServerTableCompactMode(bool compact)
{
    if (serverView_ == nullptr) {
        return;
    }

    serverView_->setColumnHidden(ServerTypeColumn, compact);
    serverView_->setColumnHidden(ServerAddressColumn, compact);
}
