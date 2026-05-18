#include "ui/mainwindow/MainWindow.h"
#include "common/AppPlatform.h"
#include "common/DialogUtils.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPointF>
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
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QTabBar>
#include <QTableView>
#include <QTime>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTextDocument>
#include <QTextOption>
#include <QtMath>
#include <QVBoxLayout>
#include <QWidget>
#include <QAbstractItemView>

#include "domain/models/VmessItem.h"
#include "runtime/ProtocolCoreCompat.h"
#include "subscription/ShareUrlBuilder.h"
#include "services/ServerService.h"
#include "ui/mainwindow/LogItemDelegate.h"
#include "ui/mainwindow/LogPanelWidget.h"
#include "ui/mainwindow/BackgroundTaskActionsController.h"
#include "ui/mainwindow/ProxyToolbarController.h"
#include "ui/mainwindow/RoutingModeController.h"
#include "ui/mainwindow/SharePanelWidget.h"
#include "ui/mainwindow/ServerWorkspaceWidget.h"
#include "ui/mainwindow/SubscriptionViewController.h"
#include "ui/mainwindow/ServerListController.h"
#include "ui/mainwindow/WindowLayoutStateController.h"
#include "ui/mainwindow/ServerActionsController.h"
#include "ui/mainwindow/ServerSelectionController.h"
#include "ui/mainwindow/ServerTableView.h"
#include "ui/mainwindow/StatusBarController.h"
#include "ui/models/LogFilterProxyModel.h"
#include "ui/models/LogListModel.h"
#include "ui/models/ServerFilterProxyModel.h"
#include "ui/models/ServerTableModel.h"
#include "ui/qr/QrCodeRenderer.h"
#include "ui/theme/AppTheme.h"

namespace {

constexpr int DefaultMainWindowWidth = 1000;
constexpr int DefaultMainWindowHeight = 640;
constexpr int ToolbarControlSpacing = 2;
constexpr int ToolbarControlHeight = 28;
constexpr int HeaderFilterMinimumCharacters = 14;
constexpr int InitialRoutingComboMinimumCharacters = 12;
constexpr int ServerTableNoColumn = 0;

QString elideTextWithThreeDots(const QFontMetrics& fontMetrics, const QString& text, int availableWidth)
{
    static const QString ellipsis = QStringLiteral("...");
    if (availableWidth <= 0 || fontMetrics.horizontalAdvance(text) <= availableWidth) {
        return text;
    }

    if (fontMetrics.horizontalAdvance(ellipsis) >= availableWidth) {
        return ellipsis;
    }

    int low = 0;
    int high = text.size();
    while (low < high) {
        const int mid = (low + high + 1) / 2;
        const QString candidate = text.left(mid) + ellipsis;
        if (fontMetrics.horizontalAdvance(candidate) <= availableWidth) {
            low = mid;
        } else {
            high = mid - 1;
        }
    }

    return text.left(low) + ellipsis;
}

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

void paintChevron(QPainter& painter, const QRect& rect, bool enabled)
{
    if (!rect.isValid()) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(enabled ? QColor(QStringLiteral("#5b6574")) : QColor(QStringLiteral("#a7b0bf")),
                        1.6,
                        Qt::SolidLine,
                        Qt::RoundCap,
                        Qt::RoundJoin));

    const QPointF center(rect.center().x(), rect.center().y() + 2.0);
    QPainterPath chevron;
    chevron.moveTo(center.x() - 4.0, center.y() - 1.0);
    chevron.lineTo(center.x(), center.y() + 2.5);
    chevron.lineTo(center.x() + 4.0, center.y() - 1.0);
    painter.drawPath(chevron);
}

class StyledComboBoxBase : public QComboBox {
public:
    explicit StyledComboBoxBase(QWidget* parent = nullptr)
        : QComboBox(parent)
    {
    }

    QSize sizeHint() const override
    {
        return stabilizedSizeHint(QComboBox::sizeHint());
    }

    QSize minimumSizeHint() const override
    {
        return stabilizedSizeHint(QComboBox::minimumSizeHint());
    }

protected:
    void paintChevronForOption(const QStyleOptionComboBox& option)
    {
        const QRect arrowRect = style()->subControlRect(
            QStyle::CC_ComboBox,
            &option,
            QStyle::SC_ComboBoxArrow,
            this);
        if (!arrowRect.isValid()) {
            return;
        }

        QPainter painter(this);
        paintChevron(painter, arrowRect, isEnabled());
    }

private:
    QSize stabilizedSizeHint(QSize hint) const
    {
        const int width = property("contentSizedWidth").toInt();
        if (width > 0) {
            hint.setWidth(width);
        }
        return hint;
    }
};

class StyledComboBox final : public StyledComboBoxBase {
public:
    explicit StyledComboBox(QWidget* parent = nullptr)
        : StyledComboBoxBase(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QComboBox::paintEvent(event);

        QStyleOptionComboBox option;
        initStyleOption(&option);
        paintChevronForOption(option);
    }
};

class StyledToolbarMenuButton final : public QToolButton {
public:
    explicit StyledToolbarMenuButton(QWidget* parent = nullptr)
        : QToolButton(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QToolButton::paintEvent(event);

        Q_UNUSED(event)

        if (menu() == nullptr || !property("toolbarMenuButton").toBool()) {
            return;
        }

        const QRect indicatorRect(width() - 16, ((height() - 10) / 2) - 1, 10, 10);
        QPainter painter(this);
        paintChevron(painter, indicatorRect, isEnabled());
    }
};

void configureStyledComboBox(QComboBox* comboBox)
{
    if (comboBox == nullptr) {
        return;
    }

    comboBox->setProperty("styledChevron", true);
    auto* popupView = new QListView(comboBox);
    popupView->setObjectName(QStringLiteral("comboPopupView"));
    popupView->setMouseTracking(true);
    popupView->setUniformItemSizes(true);
    popupView->setSelectionRectVisible(false);
    popupView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    popupView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    popupView->setFrameShape(QFrame::NoFrame);
    comboBox->setView(popupView);
}

QIcon loadToolbarIcon(const QString& fileName)
{
    return QIcon(QStringLiteral(":/toolbar/%1").arg(fileName));
}

constexpr int kServerResultColumn = 7;

void syncToolbarActionButton(QToolButton* button, QAction* action)
{
    if (button == nullptr || action == nullptr) {
        return;
    }

    const auto syncState = [button, action]() {
        button->setText(action->text());
        button->setIcon(action->icon());
        button->setToolTip(action->toolTip().isEmpty() ? action->text() : action->toolTip());
        button->setEnabled(action->isEnabled());
        button->setVisible(action->isVisible());
        if (action->isCheckable()) {
            button->setChecked(action->isChecked());
        }
    };

    syncState();
    QObject::connect(action, &QAction::changed, button, syncState);
}

QToolButton* createToolbarMenuButton(
    QToolBar* toolBar,
    const QString& objectName,
    const QString& text,
    const QIcon& icon,
    QMenu* menu)
{
    auto* button = new StyledToolbarMenuButton(toolBar);
    button->setObjectName(objectName);
    button->setText(text);
    button->setIcon(icon);
    button->setProperty("toolbarMenuButton", true);
    button->setMenu(menu);
    button->setPopupMode(QToolButton::InstantPopup);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(true);
    button->setFixedHeight(ToolbarControlHeight);
    toolBar->addWidget(button);
    return button;
}

QToolButton* createToolbarActionButton(
    QToolBar* toolBar,
    const QString& objectName,
    const QString& text,
    const QIcon& icon,
    QAction* action)
{
    auto* button = new QToolButton(toolBar);
    button->setObjectName(objectName);
    button->setText(text);
    button->setIcon(icon);
    button->setCheckable(action != nullptr && action->isCheckable());
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(true);
    button->setFixedHeight(ToolbarControlHeight);

    if (action != nullptr) {
        QObject::connect(button, &QToolButton::clicked, action, &QAction::trigger);
        syncToolbarActionButton(button, action);
    }

    toolBar->addWidget(button);
    return button;
}

QWidget* createToolbarSpacing(QWidget* parent, int width)
{
    auto* spacer = new QWidget(parent);
    spacer->setFixedWidth(width);
    spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    return spacer;
}

int textControlMinimumWidth(const QWidget* widget, const QString& text, int minimumCharacters, int chromeWidth)
{
    if (widget == nullptr) {
        return 0;
    }

    const QFontMetrics metrics(widget->font());
    const int textWidth = metrics.horizontalAdvance(text);
    const int characterWidth = metrics.horizontalAdvance(QString(minimumCharacters, QLatin1Char('M')));
    return qMax(textWidth, characterWidth) + chromeWidth;
}

void configureContentSizedLineEdit(QLineEdit* edit, int minimumCharacters)
{
    if (edit == nullptr) {
        return;
    }

    edit->setMinimumWidth(textControlMinimumWidth(edit, edit->placeholderText(), minimumCharacters, 40));
    edit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void updateContentSizedComboBox(QComboBox* comboBox, int minimumCharacters)
{
    if (comboBox == nullptr) {
        return;
    }

    // Always set the policy so tests can verify it before the window is shown.
    comboBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Defer the actual width calculation until after the first show; font
    // metrics differ before the style has fully resolved.
    if (!comboBox->window()->isVisible()) {
        return;
    }

    QString widestText = comboBox->currentText();
    for (int index = 0; index < comboBox->count(); ++index) {
        if (comboBox->fontMetrics().horizontalAdvance(comboBox->itemText(index))
            > comboBox->fontMetrics().horizontalAdvance(widestText)) {
            widestText = comboBox->itemText(index);
        }
    }

    const int comboWidth = textControlMinimumWidth(comboBox, widestText, minimumCharacters, 16);
    comboBox->setProperty("contentSizedWidth", comboWidth);
    comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    comboBox->setFixedWidth(comboWidth);
    comboBox->updateGeometry();
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete serverActionsController_;
    delete serverSelectionController_;
    delete statusBarController_;
    delete serverListController_;
    delete subscriptionViewController_;
    delete routingModeController_;
    delete proxyToolbarController_;
    delete backgroundTaskActionsController_;
}

void MainWindow::setConfig(const Config& config, const QList<ServerStatItem>& statistics)
{
    updateConfigSnapshot(config);
    rebuildShareUrlCache(config.servers);
    preserveServerSelectionPreference();

    serverModel_->setItems(config.servers, statistics, config.currentIndexId);
    currentIndexId_ = config.currentIndexId.trimmed();
    tunEnabled_ = config.tunModeItem.enableTun;
    updateCurrentCoreFromConfig(config);
    syncConfigControllers(config);

    if (serverView_ != nullptr && serverView_->selectionModel() != nullptr) {
        updateSelectionForVisibleRows();
    }

    updateQrPreview();
    updateActionState();
    updateWindowTitle();
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

void MainWindow::showTransientStatus(
    const QString& message,
    int timeoutMs,
    TransientStatusPriority priority)
{
    if (statusBarController_ != nullptr) {
        const StatusBarController::TransientStatusPriority controllerPriority =
            priority == TransientStatusPriority::Routine
            ? StatusBarController::TransientStatusPriority::Routine
            : StatusBarController::TransientStatusPriority::Important;
        statusBarController_->showTransientStatus(message, timeoutMs, controllerPriority);
    }
}

void MainWindow::clearTransientStatus()
{
    if (statusBarController_ != nullptr) {
        statusBarController_->clearTransientStatus();
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

void MainWindow::setAutoRunEnabled(bool enabled)
{
    autoRunEnabled_ = enabled;
    if (toggleAutoRunAction_ != nullptr) {
        toggleAutoRunAction_->setText(enabled
            ? tr("Disable Auto Run")
            : tr("Enable Auto Run"));
    }
    updateStatusPresentation();
}

void MainWindow::setSystemProxyState(int mode, bool enabled)
{
    systemProxyMode_ = normalizeSystemProxyMode(mode);
    systemProxyApplied_ = enabled;

    if (clearProxyAction_ != nullptr) {
        clearProxyAction_->setChecked(systemProxyMode_ == SystemProxyMode::ForcedClear);
    }
    if (globalProxyAction_ != nullptr) {
        globalProxyAction_->setChecked(systemProxyMode_ == SystemProxyMode::ForcedChange);
    }
    if (unchangedProxyAction_ != nullptr) {
        unchangedProxyAction_->setChecked(systemProxyMode_ == SystemProxyMode::Unchanged);
    }
    if (pacProxyAction_ != nullptr) {
        pacProxyAction_->setChecked(systemProxyMode_ == SystemProxyMode::Pac);
    }
    if (systemProxyModeCombo_ != nullptr) {
        const QSignalBlocker blocker(systemProxyModeCombo_);
        const int comboIndex = systemProxyModeCombo_->findData(toLegacySystemProxyModeValue(systemProxyMode_));
        if (comboIndex >= 0) {
            systemProxyModeCombo_->setCurrentIndex(comboIndex);
        }
    }

    updateRuntimeUiState();
}

void MainWindow::setProxyEnabled(bool enabled)
{
    setSystemProxyState(
        enabled ? toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange)
                : toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear),
        enabled);
}

void MainWindow::setCoreProcessRunning(bool running)
{
    coreProcessRunning_ = running;
    updateRuntimeUiState();
}

void MainWindow::setCoreRunning(bool enabled, bool pending)
{
    coreRunning_ = enabled;
    coreTransitionPending_ = pending;
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

void MainWindow::setStatisticsSessionState(const StatisticsSessionState& state)
{
    statisticsState_ = state;
    if (statisticsState_.running && statisticsState_.coreType != CoreType::Unknown) {
        currentCoreName_ = coreTypeDisplayName(statisticsState_.coreType);
    }
    updateStatusPresentation(true);
}

void MainWindow::setSpeedTestRunning(bool running)
{
    speedTestRunning_ = running;
    setServerTableDynamicSortEnabled(!running, !running);
    updateActionState();
}

void MainWindow::setSubscriptionUpdateRunning(bool running)
{
    if (serverWorkspace_ != nullptr) {
        serverWorkspace_->setSubscriptionUpdateRunning(running);
    }

    updateActionState();
}

void MainWindow::setBackgroundTaskRunning(bool running)
{
    backgroundTaskRunning_ = running;
    updateActionState();
    updateStatusPresentation();
}

void MainWindow::setBackgroundTaskDescription(const QString& description)
{
    backgroundTaskDescription_ = description.trimmed();
    updateStatusPresentation();
}

void MainWindow::setTunEnabled(bool enabled)
{
    tunEnabled_ = enabled;
    updateRuntimeUiState();
}

void MainWindow::restoreUiState(const Config& config)
{
    if (windowLayoutStateController_ != nullptr) {
        windowLayoutStateController_->restoreFromConfig(config);
    }
    qrPreviewVisible_ = config.mainQrPreviewVisible;
    if (sharePanel_ != nullptr) {
        sharePanel_->setPreviewVisible(qrPreviewVisible_);
    }
    coreProcessRunning_ = false;
    coreRunning_ = false;
    coreTransitionPending_ = false;
    systemProxyApplied_ = false;
    tunEnabled_ = config.tunModeItem.enableTun;
    updateRuntimeUiState();
    updateQrPanelActionText();

    if (subscriptionTabBar_ != nullptr && serverModel_ != nullptr) {
        QString preferredTabKey = SubscriptionViewController::resolveTabKeyFromSelectionId(config.mainSelectedSubId);

        // Switch to the tab that contains the current server
        if (!config.currentIndexId.isEmpty()) {
            const ServerTableRow* currentServer = nullptr;
            for (int i = 0; i < serverModel_->rowCount(); ++i) {
                const ServerTableRow* item = serverModel_->itemAt(i);
                if (item != nullptr && item->indexId == config.currentIndexId) {
                    currentServer = item;
                    break;
                }
            }
            if (currentServer != nullptr) {
                const QString serverSubId = currentServer->subId.trimmed();
                QString serverTabKey;
                if (serverSubId.isEmpty()) {
                    serverTabKey = QStringLiteral("ungrouped");
                } else {
                    serverTabKey = QStringLiteral("sub:%1").arg(serverSubId);
                }

                if (preferredTabKey != serverTabKey) {
                    preferredTabKey = serverTabKey;
                }
            }
        }

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
    config.mainQrPreviewVisible = qrPreviewVisible_;
    config.mainSelectedSubId = subscriptionViewController_ == nullptr
        ? QStringLiteral("__unsubscribed__")
        : subscriptionViewController_->persistedSelectionId();
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
        ServerActionsController::SelectionSnapshot snapshot;
        snapshot.selectedItems = serverSelectionController_ == nullptr
            ? QList<const ServerTableRow*>()
            : serverSelectionController_->selectedServers();
        snapshot.hasServers = serverModel_ != nullptr && serverModel_->rowCount() > 1;
        snapshot.canStartTask = canStartTask;
        snapshot.selectedShareLinks = serverSelectionController_ == nullptr
            ? QStringList()
            : serverSelectionController_->selectedShareLinks(shareUrlByIndexId_);
        serverActionsController_->syncActionState(serverActionsController_->buildActionState(snapshot));
    }

    if (backgroundTaskActionsController_ != nullptr) {
        backgroundTaskActionsController_->sync({
            subscriptionViewController_ != nullptr
                && !subscriptionViewController_->currentSubscriptionUrl().isEmpty(),
            canStartTask});
    }

    syncProxyToolbarController();
}

void MainWindow::updateQrPreview()
{
    if (sharePanel_ == nullptr) {
        return;
    }

    sharePanel_->setShareLinks(
        serverSelectionController_ == nullptr
            ? QStringList()
            : serverSelectionController_->selectedShareLinks(shareUrlByIndexId_));
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

bool MainWindow::canStartBackgroundTask() const
{
    return !backgroundTaskRunning_ && !speedTestRunning_;
}

bool MainWindow::isUngroupedSubscriptionTabSelected() const
{
    return subscriptionViewController_ != nullptr
        && subscriptionViewController_->isUngroupedTabSelected();
}

void MainWindow::setupUi()
{
    setMinimumSize(DefaultMainWindowWidth, DefaultMainWindowHeight);
    resize(DefaultMainWindowWidth, DefaultMainWindowHeight);

    setupToolbar();
    setupServerView();
    setupDiagnosticsPanel();
    updateWindowTitle();
}

void MainWindow::setupToolbar()
{
    auto* toolBar = addToolBar(tr("Main"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    AppTheme::applyCompactFont(toolBar);
    const int iconExtent = qRound(toolBar->fontMetrics().height() * 1.2);
    toolBar->setIconSize(QSize(iconExtent, iconExtent));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar->setContentsMargins(0, 2, 0, 2);
    createToolbarActions();
    QMenu* settingMenu = nullptr;
    QMenu* helpMenu = nullptr;
    createToolbarMenus(toolBar, settingMenu, helpMenu);
    Q_UNUSED(settingMenu)
    populateToolbarWidgets(toolBar, helpMenu);
    initializeToolbarControllers();
}

void MainWindow::createToolbarActions()
{
    createServerToolbarActions();
    createSubscriptionToolbarActions();
    createUpdateToolbarActions();
    createSystemToolbarActions();
    createHelpToolbarActions();
    createRuntimeToolbarActions();
}

void MainWindow::createServerToolbarActions()
{
    addServerAction_ = new QAction(tr("Add Server"), this);
    addServerAction_->setObjectName(QStringLiteral("addServerAction"));
    addCustomServerAction_ = new QAction(tr("Import Custom"), this);
    editServerAction_ = new QAction(tr("Edit Server"), this);
    duplicateServerAction_ = new QAction(tr("Duplicate Server"), this);
    exportClientConfigAction_ = new QAction(tr("Export Client Config"), this);
    exportClientConfigAction_->setObjectName(QStringLiteral("exportClientConfigAction"));
    exportServerConfigAction_ = new QAction(tr("Export Server Config"), this);
    exportServerConfigAction_->setObjectName(QStringLiteral("exportServerConfigAction"));
    copyUrlAction_ = new QAction(tr("Copy Url"), this);
    copyUrlAction_->setObjectName(QStringLiteral("copyUrlAction"));
    copyShareLinkAction_ = new QAction(tr("Copy Share Link"), this);
    copyShareLinkAction_->setObjectName(QStringLiteral("copyShareLinkAction"));
    copySubscriptionContentAction_ = new QAction(tr("Copy Subscription Content"), this);
    copySubscriptionContentAction_->setObjectName(QStringLiteral("copySubscriptionContentAction"));
    copySubscriptionUrlAction_ = new QAction(tr("Copy Subscription Url"), this);
    copySubscriptionUrlAction_->setObjectName(QStringLiteral("copySubscriptionUrlAction"));
    openCustomConfigAction_ = new QAction(tr("Open Config"), this);
    removeServerAction_ = new QAction(tr("Remove"), this);
    removeServerAction_->setObjectName(QStringLiteral("removeServerAction"));
    moveServerTopAction_ = new QAction(tr("Move To Top"), this);
    moveServerTopAction_->setObjectName(QStringLiteral("moveServerTopAction"));
    moveServerUpAction_ = new QAction(tr("Move Up"), this);
    moveServerUpAction_->setObjectName(QStringLiteral("moveServerUpAction"));
    moveServerDownAction_ = new QAction(tr("Move Down"), this);
    moveServerDownAction_->setObjectName(QStringLiteral("moveServerDownAction"));
    moveServerBottomAction_ = new QAction(tr("Move To Bottom"), this);
    moveServerBottomAction_->setObjectName(QStringLiteral("moveServerBottomAction"));
    testAction_ = new QAction(tr("Test"), this);
    testAction_->setObjectName(QStringLiteral("testAction"));
    setDefaultServerAction_ = new QAction(tr("Set Current"), this);
    setDefaultServerAction_->setObjectName(QStringLiteral("setDefaultServerAction"));
}

void MainWindow::createSubscriptionToolbarActions()
{
    importClipboardAction_ = new QAction(tr("Import Clipboard"), this);
    importClipboardAction_->setObjectName(QStringLiteral("importClipboardAction"));
    importClientConfigAction_ = new QAction(tr("Import Client Config"), this);
    importServerConfigAction_ = new QAction(tr("Import Server Config"), this);
    subAction_ = new QAction(tr("Subscriptions"), this);
    subAction_->setIcon(loadToolbarIcon(QStringLiteral("sub.png")));
    connect(subAction_, &QAction::triggered, this, &MainWindow::openSettingsAtSubscriptionsTabRequested);
}

void MainWindow::createUpdateToolbarActions()
{
    routingSettingsAction_ = new QAction(tr("Routing"), this);
    routingSettingsAction_->setObjectName(QStringLiteral("routingSettingsAction"));
    routingSettingsAction_->setIcon(loadToolbarIcon(QStringLiteral("server.png")));
    connect(routingSettingsAction_, &QAction::triggered, this, &MainWindow::openSettingsAtRoutingTabRequested);
    updateSubscriptionsAction_ = new QAction(tr("Update Subscriptions"), this);
    updateSubscriptionsAction_->setObjectName(QStringLiteral("updateSubscriptionsAction"));
    testMeAction_ = new QAction(tr("TestMe"), this);
    testMeAction_->setObjectName(QStringLiteral("testMeAction"));
    updateXrayCoreAction_ = new QAction(tr("Update Xray Core"), this);
    updateXrayCoreAction_->setObjectName(QStringLiteral("updateXrayCoreAction"));
    updateSingBoxCoreAction_ = new QAction(tr("Update sing-box Core"), this);
    updateSingBoxCoreAction_->setObjectName(QStringLiteral("updateSingBoxCoreAction"));
    updateGeoResourcesAction_ = new QAction(tr("Update Geo Files"), this);
    updateGeoResourcesAction_->setObjectName(QStringLiteral("updateGeoResourcesAction"));
}

void MainWindow::createSystemToolbarActions()
{
    clearProxyAction_ = new QAction(tr("Clear Proxy"), this);
    clearProxyAction_->setObjectName(QStringLiteral("clearProxyAction"));
    clearProxyAction_->setCheckable(true);
    globalProxyAction_ = new QAction(tr("Global Proxy"), this);
    globalProxyAction_->setObjectName(QStringLiteral("globalProxyAction"));
    globalProxyAction_->setCheckable(true);
    unchangedProxyAction_ = new QAction(tr("Unchanged Proxy"), this);
    unchangedProxyAction_->setObjectName(QStringLiteral("unchangedProxyAction"));
    unchangedProxyAction_->setCheckable(true);
    pacProxyAction_ = new QAction(tr("PAC Proxy"), this);
    pacProxyAction_->setObjectName(QStringLiteral("pacProxyAction"));
    pacProxyAction_->setCheckable(true);
    pacProxyAction_->setVisible(false);
    toggleAutoRunAction_ = new QAction(tr("Enable Auto Run"), this);
    reloadConfigAction_ = new QAction(tr("Reload Config"), this);
    restoreBackupAction_ = new QAction(tr("Restore Backup"), this);
    restoreBackupAction_->setObjectName(QStringLiteral("restoreBackupAction"));
    globalHotkeySettingsAction_ = new QAction(tr("Global Hotkey Settings"), this);
    globalHotkeySettingsAction_->setObjectName(QStringLiteral("globalHotkeySettingsAction"));
    dnsSettingsAction_ = new QAction(tr("DNS Settings"), this);
    dnsSettingsAction_->setObjectName(QStringLiteral("dnsSettingsAction"));
    settingsAction_ = new QAction(tr("Settings"), this);
    settingsAction_->setObjectName(QStringLiteral("settingsAction"));
    settingsAction_->setIcon(loadToolbarIcon(QStringLiteral("option.png")));
}

void MainWindow::createHelpToolbarActions()
{
    aboutAction_ = new QAction(tr("About"), this);
    aboutAction_->setObjectName(QStringLiteral("aboutAction"));
    openProjectPageAction_ = new QAction(tr("Project Page"), this);
    openProjectPageAction_->setObjectName(QStringLiteral("openProjectPageAction"));
    openReleasePageAction_ = new QAction(tr("Release Page"), this);
    openReleasePageAction_->setObjectName(QStringLiteral("openReleasePageAction"));
    openDocumentationAction_ = new QAction(tr("Documentation"), this);
    openDocumentationAction_->setObjectName(QStringLiteral("openDocumentationAction"));
    openDnsObjectDocumentationAction_ = new QAction(tr("DNSObject"), this);
    openDnsObjectDocumentationAction_->setObjectName(QStringLiteral("openDnsObjectDocumentationAction"));
    openRuleObjectDocumentationAction_ = new QAction(tr("RuleObject"), this);
    openRuleObjectDocumentationAction_->setObjectName(QStringLiteral("openRuleObjectDocumentationAction"));
    openLoopbackToolAction_ = new QAction(tr("Loopback"), this);
    openLoopbackToolAction_->setObjectName(QStringLiteral("openLoopbackToolAction"));
    openXrayReleasePageAction_ = new QAction(tr("Open Xray Releases"), this);
    openXrayReleasePageAction_->setObjectName(QStringLiteral("openXrayReleasePageAction"));
    openSingBoxReleasePageAction_ = new QAction(tr("Open sing-box Releases"), this);
    openSingBoxReleasePageAction_->setObjectName(QStringLiteral("openSingBoxReleasePageAction"));
    openGeoReleasePageAction_ = new QAction(tr("Open Geo Releases"), this);
    openGeoReleasePageAction_->setObjectName(QStringLiteral("openGeoReleasePageAction"));
}

void MainWindow::createRuntimeToolbarActions()
{
    proxyToggleAction_ = new QAction(tr("PROXY"), this);
    proxyToggleAction_->setObjectName(QStringLiteral("proxyToggleAction"));
    proxyToggleAction_->setCheckable(true);
    tunToggleAction_ = new QAction(tr("TUN"), this);
    tunToggleAction_->setObjectName(QStringLiteral("tunToggleAction"));
    tunToggleAction_->setCheckable(true);
    clearStatisticsAction_ = new QAction(tr("Clear Statistics"), this);
    toggleQrPanelAction_ = new QAction(tr("Share"), this);
    toggleQrPanelAction_->setObjectName(QStringLiteral("toggleQrPanelAction"));
    toggleQrPanelAction_->setCheckable(true);
    toggleQrPanelAction_->setChecked(qrPreviewVisible_);
}

void MainWindow::createToolbarMenus(QToolBar* toolBar, QMenu*& settingMenu, QMenu*& helpMenu)
{
    updateCurrentSubscriptionAction_ = new QAction(tr("Update Current Subscription"), this);
    updateCurrentSubscriptionAction_->setObjectName(QStringLiteral("updateCurrentSubscriptionMenuAction"));
    connect(updateCurrentSubscriptionAction_, &QAction::triggered, this, [this]() {
        triggerCurrentSubscriptionUpdate();
    });

    // Sub button now opens Settings at Subscriptions tab

    systemProxyMenu_ = new QMenu(tr("System Proxy"), toolBar);
    systemProxyMenu_->setObjectName(QStringLiteral("systemProxyMenu"));
    auto* systemProxyGroup = new QActionGroup(systemProxyMenu_);
    systemProxyGroup->setExclusive(true);
    systemProxyGroup->addAction(clearProxyAction_);
    systemProxyGroup->addAction(globalProxyAction_);
    systemProxyGroup->addAction(unchangedProxyAction_);
    systemProxyGroup->addAction(pacProxyAction_);
    systemProxyMenu_->addAction(clearProxyAction_);
    systemProxyMenu_->addAction(globalProxyAction_);
    systemProxyMenu_->addAction(unchangedProxyAction_);
    systemProxyMenu_->addAction(pacProxyAction_);

    settingMenu = new QMenu(toolBar);
    settingMenu->setObjectName(QStringLiteral("settingMenu"));
    settingMenu->addAction(settingsAction_);
    settingMenu->addAction(routingSettingsAction_);
    settingMenu->addAction(dnsSettingsAction_);
    settingMenu->addAction(globalHotkeySettingsAction_);

    helpMenu = new QMenu(tr("Help"), toolBar);
    helpMenu->setObjectName(QStringLiteral("helpMenu"));
    helpMenu->addAction(aboutAction_);
    helpMenu->addAction(openProjectPageAction_);
    helpMenu->addSeparator();
    helpMenu->addAction(updateCurrentSubscriptionAction_);
    helpMenu->addAction(updateSubscriptionsAction_);
    helpMenu->addSeparator();
    helpMenu->addAction(updateGeoResourcesAction_);
    helpMenu->addAction(updateXrayCoreAction_);
    helpMenu->addAction(updateSingBoxCoreAction_);
}

void MainWindow::populateToolbarWidgets(QToolBar* toolBar, QMenu* helpMenu)
{
    createToolbarActionButton(
        toolBar,
        QStringLiteral("settingButton"),
        tr("Settings"),
        loadToolbarIcon(QStringLiteral("option.png")),
        settingsAction_);
    toolBar->addSeparator();
    createToolbarActionButton(
        toolBar,
        QStringLiteral("subButton"),
        tr("Subscriptions"),
        loadToolbarIcon(QStringLiteral("sub.png")),
        subAction_);
    toolBar->addSeparator();
    createToolbarActionButton(
        toolBar,
        QStringLiteral("routingButton"),
        tr("Routing"),
        loadToolbarIcon(QStringLiteral("server.png")),
        routingSettingsAction_);
    toolBar->addSeparator();

    createToolbarMenuButton(
        toolBar,
        QStringLiteral("helpButton"),
        tr("Help"),
        loadToolbarIcon(QStringLiteral("help.png")),
        helpMenu);

    auto* spacer = new QWidget(toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);

    createToolbarActionButton(
        toolBar,
        QStringLiteral("proxyToggleButton"),
        tr("PROXY"),
        loadToolbarIcon(QStringLiteral("option.png")),
        proxyToggleAction_);
    toolBar->addWidget(createToolbarSpacing(toolBar, ToolbarControlSpacing));
    createToolbarActionButton(
        toolBar,
        QStringLiteral("tunToggleButton"),
        tr("TUN"),
        loadToolbarIcon(QStringLiteral("server.png")),
        tunToggleAction_);
    toolBar->addWidget(createToolbarSpacing(toolBar, ToolbarControlSpacing));

    routingModeCombo_ = new StyledComboBox(toolBar);
    routingModeCombo_->setObjectName(QStringLiteral("routingModeCombo"));
    AppTheme::applyCompactFont(routingModeCombo_);
    routingModeCombo_->setFixedHeight(ToolbarControlHeight);
    updateContentSizedComboBox(routingModeCombo_, InitialRoutingComboMinimumCharacters);
    configureStyledComboBox(routingModeCombo_);
    toolBar->addWidget(routingModeCombo_);
    toolBar->addWidget(createToolbarSpacing(toolBar, ToolbarControlSpacing));

    createToolbarActionButton(
        toolBar,
        QStringLiteral("qrCodeButton"),
        tr("Share"),
        loadToolbarIcon(QStringLiteral("share.png")),
        toggleQrPanelAction_);
}

void MainWindow::initializeToolbarControllers()
{
    backgroundTaskActionsController_ = new BackgroundTaskActionsController({
        copySubscriptionUrlAction_,
        importClipboardAction_,
        updateSubscriptionsAction_,
        updateCurrentSubscriptionAction_,
        testMeAction_,
        updateXrayCoreAction_,
        updateSingBoxCoreAction_,
        updateGeoResourcesAction_,
        updateCurrentSubscriptionShortcutAction_});
    proxyToolbarController_ = new ProxyToolbarController(proxyToggleAction_, tunToggleAction_);
    routingModeController_ = new RoutingModeController(routingModeCombo_, [this](int mode) {
        emit routingModeSelected(mode);
    });
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
    setCentralWidget(serverWorkspace_);
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
        [this](const QStringList& reorderedIds) { emit reorderServersRequested(reorderedIds); });
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
        [this](const QPoint& globalPosition) { showSystemProxyMenu(globalPosition); });
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
            duplicateServerAction_,
            exportClientConfigAction_,
            exportServerConfigAction_,
            copyUrlAction_,
            copyShareLinkAction_,
            copySubscriptionContentAction_,
            openCustomConfigAction_,
            importClipboardAction_,
            removeServerAction_,
            moveServerTopAction_,
            moveServerUpAction_,
            moveServerDownAction_,
            moveServerBottomAction_,
            testAction_,
            setDefaultServerAction_},
        [this]() { return serverSelectionController_ == nullptr ? QString() : serverSelectionController_->selectedServerId(); },
        [this]() { return serverSelectionController_ == nullptr ? QStringList() : serverSelectionController_->selectedServerIds(); },
        [this]() { return isUngroupedSubscriptionTabSelected(); },
        [this]() { updateActionState(); },
        [this](const QString& message) { appendLog(message); },
        [this](const QString& message, int timeoutMs, int priority) {
            showTransientStatus(
                message,
                timeoutMs,
                priority == 1 ? TransientStatusPriority::Routine : TransientStatusPriority::Important);
        },
        [this]() {
            return serverSelectionController_ == nullptr
                ? QStringList()
                : serverSelectionController_->selectedShareLinks(shareUrlByIndexId_);
        },
        [this]() {
            return serverSelectionController_ == nullptr
                ? QStringList()
                : serverSelectionController_->selectedShareLinks(shareUrlByIndexId_);
        });
    serverActionsController_->setup();
}

void MainWindow::setupConnections()
{
    setupGeneralActionConnections();
    setupSystemProxyConnections();
    setupToggleConnections();
    setupViewConnections();
    setupShortcutConnections();
}

void MainWindow::setupGeneralActionConnections()
{
    connect(addServerAction_, &QAction::triggered, this, &MainWindow::addServerRequested);
    connect(addCustomServerAction_, &QAction::triggered, this, &MainWindow::addCustomServerRequested);
    connect(copySubscriptionUrlAction_, &QAction::triggered, this, &MainWindow::copyCurrentSubscriptionUrlToClipboard);
    setupImportActionConnections();
    setupUpdateActionConnections();
    setupSettingsActionConnections();
    setupHelpActionConnections();
}

void MainWindow::setupImportActionConnections()
{
    connect(importClipboardAction_, &QAction::triggered, this, &MainWindow::importFromClipboardRequested);
    connect(importClientConfigAction_, &QAction::triggered, this, &MainWindow::importClientConfigRequested);
    connect(importServerConfigAction_, &QAction::triggered, this, &MainWindow::importServerConfigRequested);
}

void MainWindow::setupUpdateActionConnections()
{
    connect(updateSubscriptionsAction_, &QAction::triggered, this, &MainWindow::updateSubscriptionsRequested);
    connect(testMeAction_, &QAction::triggered, this, &MainWindow::testMeRequested);
    connect(updateXrayCoreAction_, &QAction::triggered, this, [this]() {
        emit updateCoreRequested(static_cast<int>(CoreType::Xray));
    });
    connect(updateSingBoxCoreAction_, &QAction::triggered, this, [this]() {
        emit updateCoreRequested(static_cast<int>(CoreType::SingBox));
    });
    connect(updateGeoResourcesAction_, &QAction::triggered, this, &MainWindow::updateGeoResourcesRequested);
}

void MainWindow::setupSettingsActionConnections()
{
    connect(toggleAutoRunAction_, &QAction::triggered, this, &MainWindow::toggleAutoRunRequested);
    connect(reloadConfigAction_, &QAction::triggered, this, &MainWindow::reloadConfigRequested);
    connect(restoreBackupAction_, &QAction::triggered, this, &MainWindow::restoreBackupRequested);
    connect(globalHotkeySettingsAction_, &QAction::triggered, this, &MainWindow::globalHotkeySettingsRequested);
    connect(dnsSettingsAction_, &QAction::triggered, this, &MainWindow::dnsSettingsRequested);
    connect(settingsAction_, &QAction::triggered, this, &MainWindow::settingsRequested);
}

void MainWindow::setupHelpActionConnections()
{
    connect(aboutAction_, &QAction::triggered, this, &MainWindow::aboutRequested);
    connect(openProjectPageAction_, &QAction::triggered, this, &MainWindow::openProjectPageRequested);
    connect(openReleasePageAction_, &QAction::triggered, this, &MainWindow::openReleasePageRequested);
    connect(openDocumentationAction_, &QAction::triggered, this, &MainWindow::openDocumentationRequested);
    connect(
        openDnsObjectDocumentationAction_,
        &QAction::triggered,
        this,
        &MainWindow::openDnsObjectDocumentationRequested);
    connect(
        openRuleObjectDocumentationAction_,
        &QAction::triggered,
        this,
        &MainWindow::openRuleObjectDocumentationRequested);
    connect(openLoopbackToolAction_, &QAction::triggered, this, &MainWindow::openLoopbackToolRequested);
    connect(openXrayReleasePageAction_, &QAction::triggered, this, &MainWindow::openXrayReleasePageRequested);
    connect(
        openSingBoxReleasePageAction_,
        &QAction::triggered,
        this,
        &MainWindow::openSingBoxReleasePageRequested);
    connect(openGeoReleasePageAction_, &QAction::triggered, this, &MainWindow::openGeoReleasePageRequested);
}

void MainWindow::setupSystemProxyConnections()
{
    connect(clearProxyAction_, &QAction::triggered, this, [this]() {
        emit systemProxyModeSelected(toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear));
    });
    connect(globalProxyAction_, &QAction::triggered, this, [this]() {
        emit systemProxyModeSelected(toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange));
    });
    connect(unchangedProxyAction_, &QAction::triggered, this, [this]() {
        emit systemProxyModeSelected(toLegacySystemProxyModeValue(SystemProxyMode::Unchanged));
    });
    connect(pacProxyAction_, &QAction::triggered, this, [this]() {
        emit systemProxyModeSelected(toLegacySystemProxyModeValue(SystemProxyMode::Pac));
    });
    if (systemProxyModeCombo_ != nullptr) {
        connect(systemProxyModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (systemProxyModeCombo_ == nullptr || index < 0) {
                return;
            }

            emit systemProxyModeSelected(systemProxyModeCombo_->itemData(index).toInt());
        });
    }
}

void MainWindow::setupToggleConnections()
{
    connect(proxyToggleAction_, &QAction::triggered, this, [this]() {
        const bool isProxyChecked = coreRunning_ && systemProxyApplied_ && !coreTransitionPending_;
        const bool isProxyUnchecked =
            !coreProcessRunning_ && !coreRunning_ && !systemProxyApplied_ && !coreTransitionPending_;
        if (isProxyChecked) {
            emit disableSystemProxyRequested();
            return;
        }

        if (isProxyUnchecked) {
            emit enableSystemProxyRequested();
        }
    });
    connect(tunToggleAction_, &QAction::triggered, this, [this](bool checked) {
        if (coreTransitionPending_) {
            return;
        }

        emit tunEnabledChanged(checked);
    });
    connect(clearStatisticsAction_, &QAction::triggered, this, &MainWindow::clearStatisticsRequested);
    if (sharePanel_ != nullptr) {
        connect(sharePanel_, &SharePanelWidget::transientStatusRequested, this, [this](const QString& message, int timeoutMs) {
            showTransientStatus(message, timeoutMs, TransientStatusPriority::Routine);
        });
    }
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

void MainWindow::setupShortcutConnections()
{
    setupFilterShortcutConnections();
    setupUpdateShortcutConnections();
    setupExitShortcutConnection();
    setupSubscriptionTabShortcutConnections();
}

void MainWindow::setupFilterShortcutConnections()
{
    if (serverFilterEdit_ == nullptr) {
        return;
    }

    auto* focusServerFilterShortcutAction = new QAction(this);
    focusServerFilterShortcutAction->setObjectName(QStringLiteral("focusServerFilterShortcutAction"));
    focusServerFilterShortcutAction->setShortcut(QKeySequence::Find);
    focusServerFilterShortcutAction->setShortcutContext(Qt::WindowShortcut);
    connect(focusServerFilterShortcutAction, &QAction::triggered, this, [this]() {
        if (serverFilterEdit_ != nullptr) {
            serverFilterEdit_->setFocus();
            serverFilterEdit_->selectAll();
        }
    });
    addAction(focusServerFilterShortcutAction);
}

void MainWindow::setupUpdateShortcutConnections()
{
    testAction_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_T));
    testAction_->setShortcutContext(Qt::WindowShortcut);
    addAction(testAction_);

    updateSubscriptionsAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
    updateSubscriptionsAction_->setShortcutContext(Qt::WindowShortcut);
    addAction(updateSubscriptionsAction_);

    updateCurrentSubscriptionShortcutAction_ = new QAction(this);
    updateCurrentSubscriptionShortcutAction_->setObjectName(QStringLiteral("updateCurrentSubscriptionShortcutAction"));
    updateCurrentSubscriptionShortcutAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    updateCurrentSubscriptionShortcutAction_->setShortcutContext(Qt::WindowShortcut);
    connect(updateCurrentSubscriptionShortcutAction_, &QAction::triggered, this, [this]() {
        triggerCurrentSubscriptionUpdate();
    });
    addAction(updateCurrentSubscriptionShortcutAction_);
}

void MainWindow::setupExitShortcutConnection()
{
    auto* exitShortcutAction = new QAction(this);
    exitShortcutAction->setObjectName(QStringLiteral("exitShortcutAction"));
    exitShortcutAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_X));
    exitShortcutAction->setShortcutContext(Qt::WindowShortcut);
    connect(exitShortcutAction, &QAction::triggered, this, [this]() {
        requestExit();
    });
    addAction(exitShortcutAction);
}

void MainWindow::setupSubscriptionTabShortcutConnections()
{
    if (subscriptionTabBar_ == nullptr) {
        return;
    }

    for (int index = 0; index < 9; ++index) {
        auto* switchTabShortcutAction = new QAction(this);
        switchTabShortcutAction->setObjectName(QStringLiteral("subscriptionTabShortcutAction%1").arg(index + 1));
        switchTabShortcutAction->setShortcut(QKeySequence(Qt::ALT | (Qt::Key_1 + index)));
        switchTabShortcutAction->setShortcutContext(Qt::WindowShortcut);
        connect(switchTabShortcutAction, &QAction::triggered, this, [this, index]() {
            if (subscriptionTabBar_ != nullptr && index < subscriptionTabBar_->count()) {
                subscriptionTabBar_->setCurrentIndex(index);
            }
        });
        addAction(switchTabShortcutAction);
    }
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
        showTransientStatus(tr("Subscription URL unavailable for the current tab."), 4000);
        return;
    }

    if (QApplication::clipboard() != nullptr) {
        QApplication::clipboard()->setText(url);
    }

    appendLog(tr("Copied subscription URL to the clipboard."));
    showTransientStatus(
        tr("Copied subscription URL to the clipboard."),
        3000,
        TransientStatusPriority::Routine);
}

QString MainWindow::currentSubscriptionIdForUpdate() const
{
    const QString tabKey = subscriptionViewController_ == nullptr
        ? QStringLiteral("ungrouped")
        : subscriptionViewController_->currentTabKey();
    return tabKey.startsWith(QStringLiteral("sub:"))
        ? tabKey.mid(QStringLiteral("sub:").size())
        : QString();
}

void MainWindow::triggerCurrentSubscriptionUpdate()
{
    emit updateCurrentSubscriptionRequested(currentSubscriptionIdForUpdate());
}

void MainWindow::showSystemProxyMenu(const QPoint& globalPosition)
{
    if (systemProxyMenu_ == nullptr) {
        return;
    }

    systemProxyMenu_->popup(globalPosition);
}

bool MainWindow::confirmExit()
{
    return DialogUtils::askYesNoQuestion(
               this,
               tr("Quit v2rayq"),
               tr("Quit v2rayq now?"),
               QMessageBox::No)
        == QMessageBox::Yes;
}

void MainWindow::updateWindowTitle()
{
    const QString coreName = currentCoreName_.trimmed().isEmpty()
        ? tr("Unknown")
        : currentCoreName_.trimmed();
    const QString serverName = currentServerName_.trimmed().isEmpty()
        ? tr("None")
        : currentServerName_.trimmed();
    const QString proxyState = systemProxyApplied_ ? tr("Proxy ON") : tr("Proxy OFF");
    const QString tunState = tunEnabled_ ? tr("TUN ON") : tr("TUN OFF");

    setWindowTitle(tr("V2RAYQ [%1] [%2] [%3] [%4]")
                       .arg(coreName, serverName, proxyState, tunState));
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
            routingModeController_->setRoutingOptions(configSnapshot_.routingItems, configSnapshot_.routingIndex);
        }
        updateQrPreview();
        updateQrPanelActionText();
        if (statusBarController_ != nullptr) {
            statusBarController_->refreshTransientStatusLabel(isVisible());
        }
    });
}

const ServerTableRow* MainWindow::activeServer() const
{
    return serverModel_ == nullptr ? nullptr : serverModel_->currentItem();
}

void MainWindow::refreshServerSelectionUi()
{
    updateQrPreview();
    updateActionState();
}

void MainWindow::updateConfigSnapshot(const Config& config)
{
    configSnapshot_.currentIndexId = config.currentIndexId;
    configSnapshot_.subscriptions = config.subscriptions;
    configSnapshot_.routingItems = config.routingItems;
    configSnapshot_.routingIndex = config.routingIndex;
    configSnapshot_.coreTypeItems = config.coreTypeItems;
}

void MainWindow::rebuildShareUrlCache(const QList<VmessItem>& servers)
{
    shareUrlByIndexId_.clear();
    shareUrlByIndexId_.reserve(servers.size());
    for (const VmessItem& item : servers) {
        const QString shareUrl = ShareUrlBuilder::build(item).trimmed();
        if (!item.indexId.isEmpty() && !shareUrl.isEmpty()) {
            shareUrlByIndexId_.insert(item.indexId, shareUrl);
        }
    }
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

void MainWindow::updateCurrentCoreFromConfig(const Config& config)
{
    currentCoreName_ = QStringLiteral("Unknown");
    for (const VmessItem& item : config.servers) {
        if (item.indexId == currentIndexId_) {
            currentCoreName_ =
                coreTypeDisplayName(resolveSelectedCoreType(config, item, availableCoreTypes()));
            return;
        }
    }
}

void MainWindow::syncConfigControllers(const Config& config)
{
    if (routingModeController_ != nullptr) {
        routingModeController_->setRoutingOptions(config.routingItems, config.routingIndex);
    }
    if (subscriptionViewController_ != nullptr) {
        subscriptionViewController_->setSubscriptions(config.subscriptions);
        subscriptionViewController_->rebuildTabs(subscriptionViewController_->currentTabKey());
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
        currentCoreName_,
        statisticsState_,
        systemProxyApplied_,
        autoRunEnabled_,
        coreProcessRunning_,
        coreRunning_,
        coreTransitionPending_,
        backgroundTaskRunning_,
        backgroundTaskDescription_,
        systemProxyMode_);
}

void MainWindow::syncProxyToolbarController()
{
    if (proxyToolbarController_ == nullptr) {
        return;
    }

    proxyToolbarController_->refresh(
        coreProcessRunning_,
        coreRunning_,
        coreTransitionPending_,
        systemProxyApplied_,
        tunEnabled_,
        serverModel_ != nullptr && serverModel_->rowCount() > 0,
        existingCoreTypes_,
        configSnapshot_.coreTypeItems,
        activeServer());
}
