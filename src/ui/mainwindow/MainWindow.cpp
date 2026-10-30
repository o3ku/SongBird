#include "ui/mainwindow/MainWindow.h"
#include "common/AppPlatform.h"
#include "common/DialogUtils.h"
#include "common/LogFileWriter.h"
#include "common/SubscriptionOrder.h"
#include "domain/models/RuntimeState.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCursor>
#include <QCoreApplication>
#include <QDateTime>
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
#include <QSaveFile>
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
#include "ui/mainwindow/LogItemDelegate.h"
#include "ui/mainwindow/LogPanelWidget.h"
#include "ui/mainwindow/BackgroundTaskActionsController.h"
#include "ui/mainwindow/ProxyToolbarController.h"
#include "ui/mainwindow/RoutingModeController.h"
#include "ui/mainwindow/SharePanelWidget.h"
#include "ui/mainwindow/ServerWorkspaceWidget.h"
#include "ui/mainwindow/ServerTableEmptyStateWidget.h"
#include "ui/mainwindow/MainWindowConfigState.h"
#include "ui/mainwindow/MainWindowShortcutController.h"
#include "ui/mainwindow/MainWindowTitleSupport.h"
#include "ui/mainwindow/MainWindowToolbarActions.h"
#include "ui/mainwindow/MainWindowServerSelectionSupport.h"
#include "ui/mainwindow/SubscriptionViewController.h"
#include "ui/mainwindow/SubscriptionViewSupport.h"
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
constexpr int RoutingModeComboWidth = 90;
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
    AppTheme::refreshStyle(label);
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

void MainWindow::initialize(const MainWindowInit& init)
{
    // Set the core list first so the single updateActionState() inside
    // applyConfig() already sees it.
    existingCoreTypes_ = init.existingCoreTypes;
    applyConfig(init.config);
}

void MainWindow::setConfig(const Config& config)
{
    applyConfig(config);
}

void MainWindow::applyConfig(const Config& config)
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

void MainWindow::setShareLinksExportProvider(std::function<QString(const QStringList&)> provider)
{
    shareLinksExportProvider_ = std::move(provider);
}

void MainWindow::setClashYamlExportProvider(std::function<QString(const QStringList&)> provider)
{
    clashYamlExportProvider_ = std::move(provider);
}

void MainWindow::exportServersToFile(
    const QStringList& indexIds,
    const std::function<QString(const QStringList&)>& provider,
    const QString& title,
    const QString& suggestedFileName,
    const QString& filter,
    const QString& defaultSuffix)
{
    if (!provider) {
        return;
    }

    const QString content = provider(indexIds);
    if (content.trimmed().isEmpty()) {
        // Nothing exportable in the selection. Saying so is better than opening a save dialog and
        // writing a file that contains only a YAML key or a blank line.
        appendLog(tr("The selected servers cannot be exported; they have no shareable form."));
        return;
    }

    const QString path = DialogUtils::getSaveFileName(
        this, title, suggestedFileName, filter, defaultSuffix);
    if (path.isEmpty()) {
        return;
    }

    // QSaveFile so a failure half-way through cannot leave a truncated export behind, and so the
    // original is untouched when the write fails.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendLog(tr("Failed to open the export file for writing."));
        return;
    }
    const QByteArray bytes = content.toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        appendLog(tr("Failed to write the export file."));
        return;
    }

    appendLog(tr("Exported %1 server(s) to %2.").arg(indexIds.size()).arg(path));
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

void MainWindow::setLogFileWriter(LogFileWriter* writer)
{
    logFileWriter_ = writer;
}

void MainWindow::appendLog(const QString& message, LogSeverity severity)
{
    // The file first: it is the sink that survives the process, and a line that
    // reached the panel but not the file is exactly the line a crash report is
    // missing.
    if (logFileWriter_ != nullptr) {
        logFileWriter_->append(severity, message);
    }

    if (serverWorkspace_ != nullptr && serverWorkspace_->logPanel() != nullptr) {
        serverWorkspace_->logPanel()->appendLog(message, severity);
    }
}

void MainWindow::setImportFailure(const QString& reason)
{
    if (serverWorkspace_ == nullptr) {
        return;
    }

    serverWorkspace_->setImportFailure(reason);
}

QString MainWindow::importFailure() const
{
    return serverWorkspace_ == nullptr ? QString() : serverWorkspace_->importFailure();
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
    systemProxyUndoWarning_ = snapshot.systemProxyUndoWarning.trimmed();
    routingSummary_ = snapshot.routingSummary.trimmed();
    listenSummary_ = snapshot.listenSummary.trimmed();
    proxyUiState_ = snapshot.proxyUiState;

    updateActionState();
    updateStatusPresentation(true);
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
    tunEnabled_ = config.tun().tunModeItem.enableTun;
    updateRuntimeUiState();
    updateQrPanelActionText();
    if (compactMode_ && sharePanel_ != nullptr) {
        sharePanel_->hide();
    }

    if (subscriptionTabBar_ != nullptr && serverModel_ != nullptr) {
        // The grouping mode is restored before the tab logic below: it decides what the bar even
        // shows, and the saved location item only exists to be selected in that mode.
        persistedLocationSelectionId_ = config.ui().mainSelectedLocationId;
        autoSwitchInGroup_ = config.ui().autoSwitchInGroup;
        if (serverWorkspace_ != nullptr && serverWorkspace_->groupByLocationButton() != nullptr) {
            // Set without emitting: the toggle's handler rebuilds through the controller, and the
            // controller's rebuild below is the one that must win here (it runs on the model that
            // was just populated). A signal round-trip in between would rebuild against stale data.
            QSignalBlocker blocker(serverWorkspace_->groupByLocationButton());
            serverWorkspace_->groupByLocationButton()->setChecked(config.ui().mainGroupByLocation);
        }
        if (subscriptionViewController_ != nullptr) {
            subscriptionViewController_->setGroupByLocation(config.ui().mainGroupByLocation);
            // The bar wash follows the same mode: the toggle's handler only fires on user
            // interaction, and a restored mode must tint the container too.
            if (serverWorkspace_ != nullptr) {
                serverWorkspace_->setGroupByLocationMode(config.ui().mainGroupByLocation);
            }
            // The bar must be rebuilt for the mode it is now in: the tabs sitting in it were
            // built for the other mode (applyConfig ran with the controller still in its
            // previous state), so selecting a "loc:" key against subscription tabs can only
            // fall through. The rebuild takes the restore key below as its previous-tab hint, and
            // the explicit selection afterwards is what settles the current tab.
            subscriptionViewController_->rebuildTabs(
                config.ui().mainGroupByLocation
                    ? SubscriptionViewSupport::tabKeyFromLocationId(config.ui().mainSelectedLocationId)
                    : SubscriptionViewSupport::tabKeyFromSelectionId(config.ui().mainSelectedSubId));
        }

        // The saved subscription is the user's explicit choice and it outranks the active
        // server. Letting the current server win is what used to drag the window back onto
        // its own tab -- and therefore off a subscription that holds no server yet, which is
        // the state the empty panel exists to explain. The current server still wins for a
        // state file that saved no subscription at all, because then there is no choice to
        // respect.
        QString preferredTabKey = SubscriptionViewSupport::tabKeyFromSelectionId(
            config.ui().mainSelectedSubId);
        if (config.ui().mainSelectedSubId.trimmed().isEmpty()) {
            preferredTabKey = ServerSelection::restoredSubscriptionTabKey(
                config.ui().mainSelectedSubId,
                config.currentIndexId,
                serverModel_);
        }
        // The preferred tab may not exist (its subscription was deleted, or it holds no
        // server yet). Selecting whatever tab happened to be current would then leave the
        // window on a view the saved state never asked for, so fall back to "Ungrouped",
        // which always exists.
        const auto selectTabKey = [this](const QString& tabKey) {
            for (int index = 0; index < subscriptionTabBar_->count(); ++index) {
                if (subscriptionTabBar_->tabData(index).toString() == tabKey) {
                    subscriptionTabBar_->setCurrentIndex(index);
                    return true;
                }
            }
            return false;
        };
        // In location mode the saved item wins (it is a hand-picked or inferred grouping the user
        // chose to browse); in subscription mode the saved subscription does. Both fall back to
        // the first tab, which in location mode is the first non-empty item.
        if (config.ui().mainGroupByLocation) {
            const QString locationTabKey = SubscriptionViewSupport::tabKeyFromLocationId(
                config.ui().mainSelectedLocationId);
            if (!selectTabKey(locationTabKey)) {
                selectTabKey(subscriptionTabBar_->count() > 0
                    ? subscriptionTabBar_->tabData(0).toString()
                    : QString());
            }
        } else if (!selectTabKey(preferredTabKey)) {
            selectTabKey(QStringLiteral("ungrouped"));
        }
        // The tab changed, and setCurrentIndex() is not the only way the table empties, so
        // the subscription filter has to be re-applied here instead of waiting for the next
        // config sync. It reads the tab key, not the table, so no loop.
        if (subscriptionViewController_ != nullptr) {
            subscriptionViewController_->applyCurrentTabFilter();
            if (!config.ui().mainGroupByLocation) {
                // The subscription half of the two remembered selections. In location mode the
                // controller's answer is "none" (a location tab is not a subscription), so the
                // member is only refreshed while subscription mode is actually showing.
                persistedSubscriptionSelectionId_ = subscriptionViewController_->persistedSelectionId();
            }
        }
        // The rebuild above moves the current tab inside a signal blocker, so the tab-change
        // slot that re-describes the empty-state panel never runs here; push the context for the
        // tab the restore settled on, exactly the way a config sync does after its own rebuild.
        pushEmptyStateContext(config.collection().subscriptions);
        if (serverWorkspace_ != nullptr) {
            serverWorkspace_->syncCompactSections();
        }
    }

}

void MainWindow::captureUiState(Config& config) const
{
    if (windowLayoutStateController_ != nullptr) {
        windowLayoutStateController_->captureToConfig(config);
    }
    config.ui().mainQrPreviewVisible = qrPreviewVisible_;
    // The location selection is captured here rather than through the controller's
    // persistedSelectionId(): a location tab is "no subscription" from that method's point of
    // view, and the two selections are independent, so each is written by the one place that
    // knows it.
    config.ui().mainGroupByLocation = subscriptionViewController_ != nullptr
        && subscriptionViewController_->groupByLocation();
    config.ui().mainSelectedLocationId = config.ui().mainGroupByLocation
        ? SubscriptionViewSupport::locationIdFromTabKey(
            subscriptionViewController_->currentTabKey())
        : persistedLocationSelectionId_;
    // While location mode is showing, the controller's tab is a location item and its answer is
    // "no subscription"; the member keeps the subscription the user last had selected so the
    // toggle does not cost the other mode its selection.
    const bool groupByLocation = subscriptionViewController_ != nullptr
        && subscriptionViewController_->groupByLocation();
    const QString controllerSelectionId = subscriptionViewController_ == nullptr
        ? QStringLiteral("__unsubscribed__")
        : subscriptionViewController_->persistedSelectionId();
    config.ui().mainSelectedSubId = groupByLocation && controllerSelectionId.trimmed().isEmpty()
        ? persistedSubscriptionSelectionId_
        : controllerSelectionId;
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

    // The refresh button on the empty-state panel is a background task too, and this is
    // the one place that knows the answer just changed. Without it the button keeps
    // whatever enablement the last config sync gave it: clickable while an update runs,
    // grey while the app is idle again.
    if (serverWorkspace_ != nullptr && serverWorkspace_->emptyState() != nullptr) {
        serverWorkspace_->emptyState()->refreshActionEnablement();
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
    exportShareLinksAction_ = actions.exportShareLinksAction;
    exportClashYamlAction_ = actions.exportClashYamlAction;
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
    shortcutsAction_ = actions.shortcutsAction;
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

    // The tab bar's context menu can only move a subscription that has a tab, so a hidden one could
    // not be reordered without turning it back on first. This entry exists for that case: it lists
    // the current subscription and every hidden one, which is the whole set the bar cannot reach.
    moveSubscriptionAction_ = new QAction(tr("Reorder Subscriptions..."), this);
    moveSubscriptionAction_->setObjectName(QStringLiteral("reorderSubscriptionsMenuAction"));
    connect(moveSubscriptionAction_, &QAction::triggered, this, [this]() {
        if (subscriptionViewController_ == nullptr) {
            return;
        }
        // Parented to the window so the menu is destroyed with it; the popup position is the
        // cursor's, which is where the user asked for it by clicking the entry.
        subscriptionViewController_->showSubscriptionMoveMenu(QCursor::pos(), this);
    });

    helpMenu = new QMenu(tr("Help"), toolBar);
    helpMenu->setObjectName(QStringLiteral("helpMenu"));
    helpMenu->addAction(aboutAction_);
    helpMenu->addAction(shortcutsAction_);
    helpMenu->addAction(checkAppUpdateAction_);
    helpMenu->addSeparator();
    // The loopback exemption is a Windows-only concept, and there is no IUwpLoopbackService
    // implementation to open a dialog against on any other platform -- so the entry is not
    // offered there rather than offered and then reporting itself unavailable.
    if (isWindowsPlatform()) {
        helpMenu->addAction(uwpLoopbackAction_);
        helpMenu->addSeparator();
    }
    helpMenu->addAction(updateCurrentSubscriptionAction_);
    helpMenu->addAction(updateSubscriptionsAction_);
    helpMenu->addAction(moveSubscriptionAction_);
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

    // The empty-state buttons drive the same actions the toolbar does, so the
    // signals (not the actions) are what they emit.
    if (ServerTableEmptyStateWidget* emptyState = serverWorkspace_->emptyState()) {
        connect(emptyState, &ServerTableEmptyStateWidget::importRequested, this, [this]() {
            emit openSettingsAtSubscriptionsTabRequested();
        });
        connect(emptyState, &ServerTableEmptyStateWidget::addRequested, this, &MainWindow::addServerRequested);
        connect(emptyState,
                &ServerTableEmptyStateWidget::importFromClipboardRequested,
                this,
                &MainWindow::importFromClipboardRequested);
        // The refresh is a background task, so it obeys the same gate the toolbar's
        // are behind. An empty id means "the tab is Ungrouped", where the target is
        // every subscription rather than one. The predicate is handed to the workspace
        // as well, so a panel repaint re-asks it instead of trusting the enablement of
        // the last config sync.
        const std::function<bool()> backgroundTaskAllowed = [this]() {
            return canStartBackgroundTask();
        };
        emptyState->setCanStartBackgroundTask(backgroundTaskAllowed);
        serverWorkspace_->setEmptyStateCanStartBackgroundTask(backgroundTaskAllowed);
        connect(emptyState,
                &ServerTableEmptyStateWidget::refreshSubscriptionRequested,
                this,
                [this](const QString& subscriptionId) {
                    if (subscriptionId.trimmed().isEmpty()) {
                        emit updateSubscriptionsRequested();
                        return;
                    }
                    emit updateCurrentSubscriptionRequested(subscriptionId);
                });
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
            [this](const QString& subscriptionId, SubscriptionOrder::MoveOperation operation) {
                emit moveSubscriptionsRequested({subscriptionId}, static_cast<int>(operation));
            },
            // A drag is not a step: the bar knows the order the user dropped the tab into, so the
            // whole list is handed over and persisted as it now stands.
            [this](const QList<SubItem>& reordered) {
                QStringList orderedIds;
                orderedIds.reserve(reordered.size());
                for (const SubItem& item : reordered) {
                    orderedIds.append(item.id.trimmed());
                }
                emit reorderSubscriptionsRequested(orderedIds);
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
        // The toggle's menu reads and writes the auto-switch preference; the window owns it, the
        // workspace owns the button. Toggling reports to the composition root, which persists
        // the value (see autoSwitchInGroupChanged wiring).
        serverWorkspace_->setGroupByLocationContextMenu(
            [this]() { return autoSwitchInGroup_; },
            [this](bool enabled) {
                autoSwitchInGroup_ = enabled;
                emit autoSwitchInGroupChanged(enabled);
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
            exportShareLinksAction_,
            exportClashYamlAction_,
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
        // Any unfavorited row makes the toggle read "add": the selection is what the menu acts
        // on as a whole, and a mixed selection is more likely a batch add than a batch remove.
        [this]() {
            if (serverSelectionController_ == nullptr) {
                return true;
            }
            for (const ServerTableRow* row : serverSelectionController_->selectedServers()) {
                if (row != nullptr && !row->favorite) {
                    return true;
                }
            }
            return false;
        },
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
    // The two export actions are window-level because they own the save dialog and the write; the
    // formatter is injected, so this translation unit stays free of services/ExportService.
    connect(this, &MainWindow::exportServersAsShareLinksRequested, this,
        [this](const QStringList& indexIds) {
            exportServersToFile(
                indexIds,
                shareLinksExportProvider_,
                tr("Export as Share Links"),
                QStringLiteral("songbird-servers.txt"),
                // File filters are matched by Qt against the literal glob syntax, so they stay
                // untranslated here, matching CustomServerDialog and ConfigBackupCoordinator.
                QStringLiteral("Text files (*.txt);;All files (*)"),
                QStringLiteral("txt"));
        });
    connect(this, &MainWindow::exportServersAsClashYamlRequested, this,
        [this](const QStringList& indexIds) {
            exportServersToFile(
                indexIds,
                clashYamlExportProvider_,
                tr("Export as Clash YAML"),
                QStringLiteral("songbird-proxies.yaml"),
                QStringLiteral("YAML files (*.yaml *.yml);;All files (*)"),
                QStringLiteral("yaml"));
        });
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
    connect(shortcutsAction_, &QAction::triggered, this, &MainWindow::shortcutsRequested);
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
        // Refreshed on every change while subscription mode is showing, so the remembered
        // selection is the last one the user actually saw (not merely the one at the last config
        // sync). In location mode currentTabKey() is "loc:..." and persistedSelectionId() is
        // "none", which is exactly why the member is not written there.
        if (subscriptionViewController_ != nullptr) {
            if (subscriptionViewController_->groupByLocation()) {
                persistedLocationSelectionId_ = SubscriptionViewSupport::locationIdFromTabKey(
                    subscriptionViewController_->currentTabKey());
            } else {
                persistedSubscriptionSelectionId_ = subscriptionViewController_->persistedSelectionId();
            }
        }
        if (serverWorkspace_ != nullptr) {
            // Every tab change has to re-describe the panel, not just the ones that go
            // through a config sync. Switching from a tab with nodes to one without
            // leaves the visible row count at zero, so the proxy emits no row signal to
            // react to, and the panel would keep naming the previous tab. After the
            // filter, because the state reads the row set that filter produced.
            pushEmptyStateContext();
            serverWorkspace_->syncCompactSections();
        }
    });
    connect(subscriptionTabBar_, &QTabBar::customContextMenuRequested, this, [this](const QPoint& position) {
        if (subscriptionViewController_ != nullptr) {
            subscriptionViewController_->showTabContextMenu(position);
        }
    });
    // QTabBar emits tabMoved for every reorder, including the ones rebuildTabs() performs while
    // refreshing the bar; the controller knows to ignore those.
    connect(subscriptionTabBar_, &QTabBar::tabMoved, this, [this](int from, int to) {
        if (subscriptionViewController_ != nullptr) {
            subscriptionViewController_->handleTabMoved(from, to);
        }
    });
    // The commit half of the drag reorder: tabMoved arrives for every neighbour the dragged tab
    // slides past, so handleTabMoved() only records the order and the release that ends the drag
    // is what writes it back -- once per drag instead of once per crossed neighbour, and off the
    // bar's own drag handling. The bar has the mouse grabbed for the whole drag, so the release
    // is delivered even when the cursor has left the bar. The filter does not consume the event:
    // QTabBar still needs the release to finish the drag.
    subscriptionTabBar_->installEventFilter(this);

    // The grouping toggle swaps the tab bar between subscriptions and location items. The mode
    // change itself is persisted (the window is the one place that holds the ui state), while the
    // rebuild lives in the controller: it is the same rebuild path a config sync takes, so the
    // counts and the restored tab come along for free. Each mode remembers its own selection:
    // the restore key is the id saved while that mode was last showing, because the bar the
    // toggle is tearing down belongs to the *other* mode and none of its tabs can be restored to.
    if (serverWorkspace_ != nullptr && serverWorkspace_->groupByLocationButton() != nullptr) {
        connect(serverWorkspace_->groupByLocationButton(), &QToolButton::toggled, this, [this](bool checked) {
            if (subscriptionViewController_ == nullptr) {
                return;
            }
            subscriptionViewController_->setGroupByLocation(checked);
            if (serverWorkspace_ != nullptr) {
                serverWorkspace_->setGroupByLocationMode(checked);
            }
            const QString restoreKey = checked
                ? SubscriptionViewSupport::tabKeyFromLocationId(persistedLocationSelectionId_)
                : SubscriptionViewSupport::tabKeyFromSelectionId(persistedSubscriptionSelectionId_);
            subscriptionViewController_->rebuildTabs(restoreKey);
            subscriptionViewController_->applyCurrentTabFilter();
            if (serverWorkspace_ != nullptr) {
                pushEmptyStateContext();
                serverWorkspace_->syncCompactSections();
            }
        });
    }
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
            updateCurrentSubscriptionShortcutAction_,
            shortcutsAction_},
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

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);

    // Activation, not every event: what the downstream checkpoint persists is the list the user
    // left behind, and "left behind" is the moment they look at something else or come back to this
    // window. A modal dialog closing returns the activation here too, which is what makes a hide
    // followed by "settings -> OK" reachable without a wait.
    if (event != nullptr && event->type() == QEvent::ActivationChange && isActiveWindow()) {
        emit pendingEditsCheckpointReached();
    }
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

    // Ends a subscription tab drag (see setupSubscriptionViewConnections()); a no-op on every
    // release that did not reorder anything.
    if (watched == subscriptionTabBar_
        && event != nullptr
        && event->type() == QEvent::MouseButtonRelease
        && subscriptionViewController_ != nullptr) {
        subscriptionViewController_->commitDraggedTabOrder();
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

void MainWindow::setSubscriptionUpdateFailures(const QHash<QString, QString>& failures)
{
    subscriptionUpdateFailures_ = failures;
    refreshSubscriptionTabToolTips();
}

QString MainWindow::subscriptionNetworkError(const QString& subscriptionId) const
{
    const QString id = subscriptionId.trimmed();
    return id.isEmpty() ? QString() : subscriptionUpdateFailures_.value(id);
}

void MainWindow::refreshSubscriptionTabToolTips()
{
    if (subscriptionViewController_ == nullptr) {
        return;
    }

    // The labels are relative ("5 min ago"), so they are re-derived from the current clock on
    // every refresh rather than cached with the tab.
    subscriptionViewController_->applyTabToolTips(
        QDateTime::currentMSecsSinceEpoch(),
        [this](const QString& subscriptionId) { return subscriptionNetworkError(subscriptionId); });
}

void MainWindow::pushEmptyStateContext()
{
    pushEmptyStateContext(configSnapshot_.subscriptions);
}

void MainWindow::pushEmptyStateContext(const QList<SubItem>& subscriptions)
{
    if (serverWorkspace_ == nullptr || subscriptionViewController_ == nullptr) {
        return;
    }

    // The tab is the subject: it decides whether the panel asks for a refresh or merely
    // points at where the nodes are listed, so it is read fresh every time rather than
    // carried along from the last config sync.
    serverWorkspace_->setEmptyStateContext(
        subscriptions,
        SubscriptionViewSupport::subscriptionIdFromTabKey(
            subscriptionViewController_->currentTabKey()));
}

void MainWindow::syncConfigControllers(const Config& config)
{
    if (routingModeController_ != nullptr) {
        routingModeController_->setRoutingOptions(RoutingProfiles::routingItems(config.collection()), config.collection().routingModeId);
    }
    if (subscriptionViewController_ != nullptr) {
        subscriptionViewController_->setSubscriptions(config.collection().subscriptions);
        // The servers are what the location items are built from (counts, empty-item dropping),
        // so they are pushed on the same sync a rebuild follows.
        subscriptionViewController_->setServers(config.collection().servers);
        subscriptionViewController_->rebuildTabs(subscriptionViewController_->currentTabKey());
        // After rebuildTabs(): a rebuild recreates the tabs, and a tooltip set before it would be
        // attached to tabs that no longer exist.
        refreshSubscriptionTabToolTips();
        subscriptionViewController_->applyCurrentTabFilter();
        if (serverWorkspace_ != nullptr) {
            // After applyCurrentTabFilter(): the overlay reads the visible row count the
            // filter just produced, and the state it picks depends on the tab the table
            // is showing. The tab key is what the panel names when it explains that the
            // nodes are grouped under a subscription.
            pushEmptyStateContext(config.collection().subscriptions);
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

    // A reachable session whose exit location never arrived still gets a word rather than a blank.
    // The location is decoration and its probe is display-only, so its failure must not read as a
    // broken session -- but an empty field next to a running proxy reads as exactly that.
    QString statusLocation = currentServerLocation_;
    if (statusLocation.isEmpty() && proxyUiState_ == ProxyUiState::Active) {
        statusLocation = tr("Location unknown");
    }

    // The undo warning rides the same segment as the server warning: one slot, the rarer
    // event second, and no new permanent widget -- a layout change here means new README
    // screenshots, which a status text is not worth.
    const QString composedWarning = currentServerWarning_.isEmpty()
        ? systemProxyUndoWarning_
        : (systemProxyUndoWarning_.isEmpty()
               ? currentServerWarning_
               : currentServerWarning_ + QStringLiteral(" | ") + systemProxyUndoWarning_);
    statusBarController_->refresh(
        currentServerName_,
        statusLocation,
        composedWarning,
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
