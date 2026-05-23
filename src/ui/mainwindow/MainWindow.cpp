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
#include "app/CoreStartupCheckpoint.h"
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
constexpr int ToolbarPadding = 4;
constexpr int ToolbarBottomBorderWidth = 1;
constexpr int ToolbarButtonHorizontalPadding = 8;
constexpr int ToolbarButtonBorderWidth = 1;
constexpr int HeaderFilterMinimumCharacters = 14;
constexpr int InitialRoutingComboMinimumCharacters = 12;
constexpr int ServerTableNoColumn = 0;
constexpr ushort CoreStartupFailedMark = 0x274C;
constexpr int LoadingOverlayHorizontalMargin = 24;
constexpr int LoadingContentHorizontalPadding = 18;
constexpr int LoadingContentVerticalPadding = 14;
constexpr int LoadingContentMinimumWidth = 360;
constexpr int LoadingContentMaximumWidth = 720;
constexpr int LoadingChecklistDetailMaximumWidth = 420;
constexpr int WindowTitleServerNameMaximumWidth = 300;

bool isCoreStartupFailureItem(const QString& item)
{
    const QString trimmedItem = item.trimmed();
    return trimmedItem.startsWith(QString(QChar(CoreStartupFailedMark)))
        || trimmedItem.startsWith(QStringLiteral("[!]"));
}

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
    painter.setPen(QPen(QColor(AppTheme::iconColor(enabled)),
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
    popupView->setObjectName(QStringLiteral("toolbarComboPopupView"));
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
    return AppTheme::themedSvgIcon(QStringLiteral(":/toolbar/%1").arg(fileName));
}

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

int toolbarTextButtonWidth(const QWidget* widget, const QStringList& texts)
{
    if (widget == nullptr) {
        return 0;
    }

    int textWidth = 0;
    const QFontMetrics fontMetrics(widget->font());
    for (const QString& text : texts) {
        textWidth = qMax(textWidth, fontMetrics.horizontalAdvance(text));
    }

    return textWidth + (ToolbarButtonHorizontalPadding * 2) + (ToolbarButtonBorderWidth * 2);
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

QPair<QString, QString> splitCoreStartupChecklistItem(const QString& item)
{
    const int separatorIndex = item.indexOf(coreStartupChecklistDetailSeparator());
    if (separatorIndex < 0) {
        return {item, QString()};
    }

    return {
        item.left(separatorIndex),
        item.mid(separatorIndex + 1).trimmed()
    };
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

void MainWindow::setConfig(const Config& config)
{
    refreshToolbarIcons();
    updateConfigSnapshot(config);
    rebuildShareUrlCache(config.collection().servers);
    preserveServerSelectionPreference();

    serverModel_->setItems(config.collection().servers, config.currentIndexId);
    currentIndexId_ = config.currentIndexId.trimmed();
    tunEnabled_ = config.tun().tunModeItem.enableTun;
    updateCurrentCoreFromConfig(config);
    syncConfigControllers(config);

    if (serverView_ != nullptr && serverView_->selectionModel() != nullptr) {
        updateSelectionForVisibleRows();
    }

    scheduleInitialCurrentServerReveal();
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

void MainWindow::setSystemProxyState(int mode, bool enabled)
{
    Q_UNUSED(mode);
    systemProxyApplied_ = enabled;
    updateRuntimeUiState();
}

void MainWindow::setProxyEnabled(bool enabled)
{
    setSystemProxyState(
        enabled ? toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange)
                : toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear),
        enabled);
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
    setLoadingOverlayVisible(
        running,
        running ? tr("Updating subscriptions...") : QString(),
        {});

    updateActionState();
}

void MainWindow::setCoreStartupChecklist(const QStringList& items)
{
    coreStartupChecklistVisible_ = true;
    coreStartupChecklistFailed_ = false;
    for (const QString& item : items) {
        if (isCoreStartupFailureItem(item)) {
            coreStartupChecklistFailed_ = true;
            break;
        }
    }
    setLoadingOverlayVisible(true, tr("Starting system proxy..."), items);

    updateActionState();
}

void MainWindow::clearCoreStartupChecklist()
{
    coreStartupChecklistVisible_ = false;
    coreStartupChecklistFailed_ = false;
    setLoadingOverlayVisible(false, {}, {});

    updateActionState();
}

void MainWindow::setBackgroundTaskRunning(bool running)
{
    if (backgroundTaskRunning_ == running) {
        return;
    }

    backgroundTaskRunning_ = running;
    setServerTableDynamicSortEnabled(!running, !running);
    updateActionState();
    updateStatusPresentation();
}

void MainWindow::setBackgroundTaskDescription(const QString& description)
{
    backgroundTaskDescription_ = description.trimmed();
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

    if (subscriptionTabBar_ != nullptr && serverModel_ != nullptr) {
        QString preferredTabKey = SubscriptionViewController::resolveTabKeyFromSelectionId(config.ui().mainSelectedSubId);

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

void MainWindow::refreshToolbarIcons()
{
    for (auto it = toolbarIconFiles_.cbegin(); it != toolbarIconFiles_.cend(); ++it) {
        if (it.key() != nullptr) {
            it.key()->setIcon(loadToolbarIcon(it.value()));
        }
    }
    for (auto it = toolbarStandaloneIconFiles_.cbegin(); it != toolbarStandaloneIconFiles_.cend(); ++it) {
        if (it.key() != nullptr) {
            it.key()->setIcon(loadToolbarIcon(it.value()));
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
    applySemanticState(selectServerHintLabel_, QStringLiteral("error"));
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

void MainWindow::showCurrentServerInTable()
{
    if (currentIndexId_.trimmed().isEmpty()) {
        showTransientStatus(tr("No current server."), 4000);
        return;
    }
    if (serverModel_ == nullptr
        || serverFilterModel_ == nullptr
        || serverView_ == nullptr
        || serverView_->selectionModel() == nullptr) {
        return;
    }

    const ServerTableRow* currentServer = nullptr;
    for (int row = 0; row < serverModel_->rowCount(); ++row) {
        const ServerTableRow* item = serverModel_->itemAt(row);
        if (item != nullptr && item->indexId == currentIndexId_) {
            currentServer = item;
            break;
        }
    }
    if (currentServer == nullptr) {
        showTransientStatus(tr("Current server is unavailable."), 4000);
        return;
    }

    if (serverFilterEdit_ != nullptr && !serverFilterEdit_->text().trimmed().isEmpty()) {
        serverFilterEdit_->clear();
    }

    const QString subscriptionId = currentServer->subId.trimmed();
    const QString selectionId = subscriptionId.isEmpty()
        ? QStringLiteral("__unsubscribed__")
        : subscriptionId;
    if (subscriptionViewController_ != nullptr) {
        subscriptionViewController_->selectSubscriptionTab(selectionId);
        subscriptionViewController_->applyCurrentTabFilter();
    }

    QModelIndex targetIndex;
    for (int proxyRow = 0; proxyRow < serverFilterModel_->rowCount(); ++proxyRow) {
        const QModelIndex proxyIndex = serverFilterModel_->index(proxyRow, ServerTableNoColumn);
        const QModelIndex sourceIndex = serverFilterModel_->mapToSource(proxyIndex);
        const ServerTableRow* item = serverModel_->itemAt(sourceIndex.row());
        if (item != nullptr && item->indexId == currentIndexId_) {
            targetIndex = proxyIndex;
            break;
        }
    }

    if (!targetIndex.isValid()) {
        showTransientStatus(tr("Current server is hidden by the current filter."), 4000);
        return;
    }

    serverView_->setCurrentIndex(targetIndex);
    serverView_->selectRow(targetIndex.row());
    serverView_->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
    serverView_->setFocus(Qt::OtherFocusReason);
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
    return !backgroundTaskRunning_
        && proxyUiState_ != ProxyUiState::Transitioning
        && proxyUiState_ != ProxyUiState::Inconsistent
        && (!coreStartupChecklistVisible_ || coreStartupChecklistFailed_);
}

bool MainWindow::ensureCanStartBackgroundTask()
{
    if (canStartBackgroundTask()) {
        return true;
    }

    showTransientStatus(
        proxyUiState_ == ProxyUiState::Transitioning || (coreStartupChecklistVisible_ && !coreStartupChecklistFailed_)
            ? tr("Proxy startup is in progress.")
            : tr("A background task is already running."),
        4000);
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
    setMinimumSize(DefaultMainWindowWidth, DefaultMainWindowHeight);
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
    setupLoadingOverlay();
    updateWindowTitle();
}

void MainWindow::setupLoadingOverlay()
{
    loadingOverlay_ = new QWidget(this);
    loadingOverlay_->setObjectName(QStringLiteral("loadingOverlay"));
    loadingOverlay_->setAttribute(Qt::WA_StyledBackground, true);
    loadingOverlay_->setFocusPolicy(Qt::StrongFocus);

    loadingContentWidget_ = new QWidget(loadingOverlay_);
    loadingContentWidget_->setObjectName(QStringLiteral("loadingContentWidget"));
    loadingContentWidget_->setAttribute(Qt::WA_StyledBackground, true);

    loadingTitleLabel_ = new QLabel(loadingContentWidget_);
    loadingTitleLabel_->setObjectName(QStringLiteral("loadingTitleLabel"));
    loadingTitleLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    loadingTitleLabel_->setWordWrap(true);

    loadingItemsWidget_ = new QWidget(loadingContentWidget_);
    loadingItemsWidget_->setObjectName(QStringLiteral("loadingItemsWidget"));
    loadingItemsLayout_ = new QVBoxLayout(loadingItemsWidget_);
    loadingItemsLayout_->setContentsMargins(0, 0, 0, 0);
    loadingItemsLayout_->setSpacing(6);

    loadingActionWidget_ = new QWidget(loadingContentWidget_);
    loadingActionWidget_->setObjectName(QStringLiteral("loadingActionWidget"));

    loadingRetryButton_ = new QPushButton(tr("Retry"), loadingActionWidget_);
    loadingRetryButton_->setObjectName(QStringLiteral("loadingRetryButton"));
    loadingRetryButton_->setVisible(false);
    connect(loadingRetryButton_, &QPushButton::clicked, this, [this]() {
        emit retryCoreStartupRequested();
    });

    loadingDismissButton_ = new QPushButton(tr("Close"), loadingActionWidget_);
    loadingDismissButton_->setObjectName(QStringLiteral("loadingDismissButton"));
    loadingDismissButton_->setVisible(false);
    connect(loadingDismissButton_, &QPushButton::clicked, this, [this]() {
        coreStartupChecklistVisible_ = false;
        coreStartupChecklistFailed_ = false;
        setLoadingOverlayVisible(false, {}, {});
        updateActionState();
    });

    auto* actionLayout = new QHBoxLayout(loadingActionWidget_);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(0);
    actionLayout->addStretch(1);
    actionLayout->addWidget(loadingRetryButton_);
    actionLayout->addSpacing(8);
    actionLayout->addWidget(loadingDismissButton_);
    actionLayout->addStretch(1);

    auto* contentLayout = new QVBoxLayout(loadingContentWidget_);
    contentLayout->setContentsMargins(
        LoadingContentHorizontalPadding,
        LoadingContentVerticalPadding,
        LoadingContentHorizontalPadding,
        LoadingContentVerticalPadding);
    contentLayout->setSpacing(12);
    contentLayout->addWidget(loadingTitleLabel_);
    contentLayout->addWidget(loadingItemsWidget_);
    contentLayout->addWidget(loadingActionWidget_);

    auto* loadingLayout = new QVBoxLayout(loadingOverlay_);
    loadingLayout->setContentsMargins(24, 24, 24, 24);
    loadingLayout->setSpacing(12);
    loadingLayout->addStretch(1);
    loadingLayout->addWidget(loadingContentWidget_, 0, Qt::AlignCenter);
    loadingLayout->addStretch(1);

    updateLoadingOverlayGeometry();
    loadingOverlay_->hide();
}

void MainWindow::setupToolbar()
{
    auto* toolbarHost = new QWidget(mainContentWidget_);
    toolbarHost->setObjectName(QStringLiteral("mainToolbarHost"));
    toolbarHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toolbarHost->setFixedHeight(ToolbarControlHeight + (ToolbarPadding * 2) + ToolbarBottomBorderWidth);
    auto* toolbarHostLayout = new QVBoxLayout(toolbarHost);
    toolbarHostLayout->setContentsMargins(ToolbarPadding, ToolbarPadding, ToolbarPadding, ToolbarPadding);
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
    toolBar->setFixedHeight(ToolbarControlHeight);
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
    editServerAction_ = new QAction(tr("Edit Server"), this);
    copyUrlAction_ = new QAction(tr("Copy Url"), this);
    copyUrlAction_->setObjectName(QStringLiteral("copyUrlAction"));
    copyShareLinkAction_ = new QAction(tr("Copy Share Link"), this);
    copyShareLinkAction_->setObjectName(QStringLiteral("copyShareLinkAction"));
    removeServerAction_ = new QAction(tr("Remove"), this);
    removeServerAction_->setObjectName(QStringLiteral("removeServerAction"));
    moveServerTopAction_ = new QAction(tr("Move To Top"), this);
    moveServerTopAction_->setObjectName(QStringLiteral("moveServerTopAction"));
    moveServerUpAction_ = new QAction(tr("Move Up"), this);
    moveServerUpAction_->setObjectName(QStringLiteral("moveServerUpAction"));
    moveServerUpAction_->setIcon(AppTheme::themedSvgIcon(QStringLiteral(":/app/up.svg")));
    moveServerDownAction_ = new QAction(tr("Move Down"), this);
    moveServerDownAction_->setObjectName(QStringLiteral("moveServerDownAction"));
    moveServerDownAction_->setIcon(AppTheme::themedSvgIcon(QStringLiteral(":/app/down.svg")));
    moveServerBottomAction_ = new QAction(tr("Move To Bottom"), this);
    moveServerBottomAction_->setObjectName(QStringLiteral("moveServerBottomAction"));
    testAction_ = new QAction(tr("Test"), this);
    testAction_->setObjectName(QStringLiteral("testAction"));
    setDefaultServerAction_ = new QAction(tr("Set Current"), this);
    setDefaultServerAction_->setObjectName(QStringLiteral("setDefaultServerAction"));
    setDefaultServerWithTunAction_ = new QAction(tr("Set Current with TUN On"), this);
    setDefaultServerWithTunAction_->setObjectName(QStringLiteral("setDefaultServerWithTunAction"));
}

void MainWindow::createSubscriptionToolbarActions()
{
    importClipboardAction_ = new QAction(tr("Import Clipboard"), this);
    importClipboardAction_->setObjectName(QStringLiteral("importClipboardAction"));
    subAction_ = new QAction(tr("Subscriptions"), this);
    toolbarIconFiles_.insert(subAction_, QStringLiteral("subscription.svg"));
    subAction_->setIcon(loadToolbarIcon(toolbarIconFiles_.value(subAction_)));
    connect(subAction_, &QAction::triggered, this, &MainWindow::openSettingsAtSubscriptionsTabRequested);
}

void MainWindow::createUpdateToolbarActions()
{
    routingSettingsAction_ = new QAction(tr("Routing"), this);
    routingSettingsAction_->setObjectName(QStringLiteral("routingSettingsAction"));
    toolbarIconFiles_.insert(routingSettingsAction_, QStringLiteral("routing.svg"));
    routingSettingsAction_->setIcon(loadToolbarIcon(toolbarIconFiles_.value(routingSettingsAction_)));
    connect(routingSettingsAction_, &QAction::triggered, this, &MainWindow::openSettingsAtRoutingTabRequested);
    updateSubscriptionsAction_ = new QAction(tr("Update Subscriptions"), this);
    updateSubscriptionsAction_->setObjectName(QStringLiteral("updateSubscriptionsAction"));
    updateXrayCoreAction_ = new QAction(tr("Update Xray Core"), this);
    updateXrayCoreAction_->setObjectName(QStringLiteral("updateXrayCoreAction"));
    updateSingBoxCoreAction_ = new QAction(tr("Update sing-box Core"), this);
    updateSingBoxCoreAction_->setObjectName(QStringLiteral("updateSingBoxCoreAction"));
    updateGeoResourcesAction_ = new QAction(tr("Update Geo Files"), this);
    updateGeoResourcesAction_->setObjectName(QStringLiteral("updateGeoResourcesAction"));
}

void MainWindow::createSystemToolbarActions()
{
    settingsAction_ = new QAction(tr("Settings"), this);
    settingsAction_->setObjectName(QStringLiteral("settingsAction"));
    toolbarIconFiles_.insert(settingsAction_, QStringLiteral("option.svg"));
    settingsAction_->setIcon(loadToolbarIcon(toolbarIconFiles_.value(settingsAction_)));
}

void MainWindow::createHelpToolbarActions()
{
    aboutAction_ = new QAction(tr("About"), this);
    aboutAction_->setObjectName(QStringLiteral("aboutAction"));
    checkAppUpdateAction_ = new QAction(tr("Check for Updates"), this);
    checkAppUpdateAction_->setObjectName(QStringLiteral("checkAppUpdateAction"));
    uwpLoopbackAction_ = new QAction(tr("UWP Loopback"), this);
    uwpLoopbackAction_->setObjectName(QStringLiteral("uwpLoopbackAction"));
}

void MainWindow::createRuntimeToolbarActions()
{
    proxyToggleAction_ = new QAction(tr("START"), this);
    proxyToggleAction_->setObjectName(QStringLiteral("proxyToggleAction"));
    proxyToggleAction_->setCheckable(true);
    tunToggleAction_ = new QAction(tr("TUN"), this);
    tunToggleAction_->setObjectName(QStringLiteral("tunToggleAction"));
    tunToggleAction_->setCheckable(true);
    toggleQrPanelAction_ = new QAction(tr("Share"), this);
    toggleQrPanelAction_->setObjectName(QStringLiteral("toggleQrPanelAction"));
    toggleQrPanelAction_->setCheckable(true);
    toggleQrPanelAction_->setChecked(qrPreviewVisible_);
}

void MainWindow::createToolbarMenus(QToolBar* toolBar, QMenu*& helpMenu)
{
    updateCurrentSubscriptionAction_ = new QAction(tr("Update Current Subscription"), this);
    updateCurrentSubscriptionAction_->setObjectName(QStringLiteral("updateCurrentSubscriptionMenuAction"));
    connect(updateCurrentSubscriptionAction_, &QAction::triggered, this, [this]() {
        triggerCurrentSubscriptionUpdate();
    });

    // Sub button now opens Settings at Subscriptions tab

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
    helpMenu->addAction(updateXrayCoreAction_);
    helpMenu->addAction(updateSingBoxCoreAction_);
}

void MainWindow::populateToolbarWidgets(QToolBar* toolBar, QMenu* helpMenu)
{
    createToolbarActionButton(
        toolBar,
        QStringLiteral("settingButton"),
        tr("Settings"),
        loadToolbarIcon(QStringLiteral("option.svg")),
        settingsAction_);
    toolBar->addWidget(createToolbarSpacing(toolBar, ToolbarControlSpacing));
    createToolbarActionButton(
        toolBar,
        QStringLiteral("subButton"),
        tr("Subscriptions"),
        loadToolbarIcon(QStringLiteral("subscription.svg")),
        subAction_);
    toolBar->addWidget(createToolbarSpacing(toolBar, ToolbarControlSpacing));
    createToolbarActionButton(
        toolBar,
        QStringLiteral("routingButton"),
        tr("Routing"),
        loadToolbarIcon(QStringLiteral("routing.svg")),
        routingSettingsAction_);
    toolBar->addWidget(createToolbarSpacing(toolBar, ToolbarControlSpacing));

    auto* helpButton = createToolbarMenuButton(
        toolBar,
        QStringLiteral("helpButton"),
        tr("Help"),
        loadToolbarIcon(QStringLiteral("help.svg")),
        helpMenu);
    toolbarStandaloneIconFiles_.insert(helpButton, QStringLiteral("help.svg"));

    auto* spacer = new QWidget(toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);

    selectServerHintLabel_ = new QLabel(toolBar);
    selectServerHintLabel_->setObjectName(QStringLiteral("selectServerHintLabel"));
    AppTheme::applyCompactFont(selectServerHintLabel_);
    selectServerHintLabel_->setFixedHeight(ToolbarControlHeight);
    selectServerHintLabel_->setFixedWidth(
        selectServerHintLabel_->fontMetrics().horizontalAdvance(tr("Add a server first")) + 18);
    selectServerHintLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    applySemanticState(selectServerHintLabel_, QStringLiteral("error"));
    selectServerHintLabel_->clear();
    toolBar->addWidget(selectServerHintLabel_);
    toolBar->addWidget(createToolbarSpacing(toolBar, ToolbarControlSpacing));

    auto* proxyToggleButton = createToolbarActionButton(
        toolBar,
        QStringLiteral("proxyToggleButton"),
        tr("START"),
        QIcon(),
        proxyToggleAction_);
    proxyToggleButton->setFixedWidth(toolbarTextButtonWidth(
        proxyToggleButton,
        {tr("START"), tr("STOP")}));
    toolBar->addWidget(createToolbarSpacing(toolBar, ToolbarControlSpacing));
    createToolbarActionButton(
        toolBar,
        QStringLiteral("tunToggleButton"),
        tr("TUN"),
        QIcon(),
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
        QIcon(),
        toggleQrPanelAction_);
}

void MainWindow::initializeToolbarControllers()
{
    backgroundTaskActionsController_ = new BackgroundTaskActionsController({
        importClipboardAction_,
        updateSubscriptionsAction_,
        updateCurrentSubscriptionAction_,
        updateXrayCoreAction_,
        updateSingBoxCoreAction_,
        updateGeoResourcesAction_,
        updateCurrentSubscriptionShortcutAction_});
    proxyToolbarController_ = new ProxyToolbarController(proxyToggleAction_, tunToggleAction_);
    routingModeController_ = new RoutingModeController(routingModeCombo_, [this](int mode) {
        emit routingModeSelected(mode);
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
        [this]() { showCurrentServerInTable(); });
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
        });
    serverActionsController_->setup();
}

void MainWindow::setupConnections()
{
    setupGeneralActionConnections();
    setupToggleConnections();
    setupViewConnections();
    setupShortcutConnections();
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

        if (proxyToolbarController_->shouldDisableProxy(snapshot)) {
            emit disableSystemProxyRequested();
            return;
        }

        const ServerTableRow* currentActiveServer = activeServer();
        if (currentActiveServer == nullptr) {
            const ServerTableRow* selectedServer = serverSelectionController_ == nullptr
                ? nullptr
                : serverSelectionController_->selectedServer();
            if (selectedServer != nullptr && !selectedServer->indexId.isEmpty()) {
                clearSelectServerHint();
                emit setDefaultServerRequested(selectedServer->indexId);
                updateActionState();
                return;
            }
            showSelectServerHint();
            updateActionState();
            return;
        }

        if (proxyToolbarController_->shouldEnableProxy(snapshot, currentActiveServer)) {
            emit enableSystemProxyRequested();
        }
    });
    connect(tunToggleAction_, &QAction::triggered, this, [this](bool checked) {
        if (proxyUiState_ == ProxyUiState::Transitioning || proxyUiState_ == ProxyUiState::Inconsistent) {
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
    if (!ensureCanStartBackgroundTask()) {
        return;
    }

    emit updateCurrentSubscriptionRequested(currentSubscriptionIdForUpdate());
}

void MainWindow::testSubscriptionServers(const QString& subscriptionId)
{
    if (!ensureCanStartBackgroundTask()) {
        return;
    }

    const QString normalizedId = subscriptionId.trimmed();
    if (normalizedId.isEmpty() || serverModel_ == nullptr) {
        showTransientStatus(tr("No servers available for this subscription."), 4000);
        return;
    }

    QStringList indexIds;
    for (int row = 0; row < serverModel_->rowCount(); ++row) {
        const ServerTableRow* item = serverModel_->itemAt(row);
        if (item == nullptr || item->subId.trimmed() != normalizedId || item->indexId.trimmed().isEmpty()) {
            continue;
        }

        indexIds.append(item->indexId);
    }

    if (indexIds.isEmpty()) {
        showTransientStatus(tr("No servers available for this subscription."), 4000);
        return;
    }

    if (ensureCanStartBackgroundTask()) {
        emit testServersRequested(indexIds);
    }
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
    const QString coreName = currentCoreName_.trimmed().isEmpty()
        ? tr("Unknown")
        : currentCoreName_.trimmed();
    const QString serverName = currentServerName_.trimmed().isEmpty()
        ? tr("None")
        : elideTextWithThreeDots(
            fontMetrics(),
            currentServerName_.trimmed(),
            WindowTitleServerNameMaximumWidth);
    const QString proxyState = proxyUiState_ == ProxyUiState::Active ? tr("Proxy ON") : tr("Proxy OFF");
    const QString tunState = tunEnabled_ ? tr("TUN ON") : tr("TUN OFF");

    setWindowTitle(tr("SongBird [%1] [%2] [%3] [%4]")
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

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    updateLoadingOverlayGeometry();
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

void MainWindow::showSelectServerHint()
{
    const bool hasServers = serverModel_ != nullptr && serverModel_->rowCount() > 0;

    if (selectServerHintLabel_ != nullptr) {
        selectServerHintLabel_->setText(tr("Select a server"));
        QTimer::singleShot(4000, this, [this]() {
            clearSelectServerHint();
        });
    }

    if (serverView_ != nullptr) {
        serverView_->flashAttention();
    }

    showTransientStatus(
        hasServers ? tr("Please select a server first.") : tr("No available server. Add or import a server first."),
        4000);
}

void MainWindow::clearSelectServerHint()
{
    if (selectServerHintLabel_ != nullptr) {
        selectServerHintLabel_->clear();
    }
}

void MainWindow::refreshServerSelectionUi()
{
    if (activeServer() != nullptr) {
        clearSelectServerHint();
    }
    updateQrPreview();
    updateActionState();
}

void MainWindow::updateConfigSnapshot(const Config& config)
{
    configSnapshot_.currentIndexId = config.currentIndexId;
    configSnapshot_.subscriptions = config.collection().subscriptions;
    configSnapshot_.routingItems = config.collection().routingItems;
    configSnapshot_.routingIndex = config.collection().routingIndex;
    configSnapshot_.coreTypeItems = config.policy().coreTypeItems;
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
    for (const VmessItem& item : config.collection().servers) {
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
        routingModeController_->setRoutingOptions(config.collection().routingItems, config.collection().routingIndex);
    }
    if (subscriptionViewController_ != nullptr) {
        subscriptionViewController_->setSubscriptions(config.collection().subscriptions);
        subscriptionViewController_->rebuildTabs(subscriptionViewController_->currentTabKey());
        subscriptionViewController_->applyCurrentTabFilter();
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
        backgroundTaskDescription_);
}

void MainWindow::syncProxyToolbarController()
{
    if (proxyToolbarController_ == nullptr) {
        return;
    }

    proxyToolbarController_->refresh(
        proxyUiState_,
        !currentServerLocation_.trimmed().isEmpty(),
        tunEnabled_,
        existingCoreTypes_,
        configSnapshot_.coreTypeItems,
        activeServer());
}

void MainWindow::setLoadingOverlayVisible(bool visible, const QString& title, const QStringList& items)
{
    if (loadingOverlay_ == nullptr) {
        return;
    }

    bool hasFailure = false;
    for (const QString& item : items) {
        if (isCoreStartupFailureItem(item)) {
            hasFailure = true;
            break;
        }
    }

    updateLoadingOverlayItems(items);

    if (loadingTitleLabel_ != nullptr) {
        loadingTitleLabel_->setText(hasFailure ? tr("Proxy startup failed") : title);
        loadingTitleLabel_->setAlignment(
            items.isEmpty() && !hasFailure
                ? Qt::AlignCenter
                : (Qt::AlignLeft | Qt::AlignVCenter));
    }

    if (loadingItemsWidget_ != nullptr) {
        loadingItemsWidget_->setVisible(!items.isEmpty());
    }
    if (loadingDismissButton_ != nullptr) {
        loadingDismissButton_->setVisible(visible && hasFailure);
    }
    if (loadingRetryButton_ != nullptr) {
        loadingRetryButton_->setVisible(visible && hasFailure);
    }
    if (loadingActionWidget_ != nullptr) {
        loadingActionWidget_->setVisible(!items.isEmpty() || hasFailure);
    }

    const bool wasVisible = loadingOverlay_->isVisible();
    if (visible) {
        if (!wasVisible || loadingOverlay_->geometry() != rect()) {
            updateLoadingOverlayGeometry();
        }
        if (!wasVisible) {
            loadingOverlay_->raise();
            loadingOverlay_->show();
            loadingOverlay_->setFocus(Qt::OtherFocusReason);
        }
    } else {
        loadingOverlay_->hide();
    }
}

void MainWindow::clearLoadingOverlayItems()
{
    if (loadingItemsLayout_ == nullptr) {
        loadingChecklistItems_.clear();
        return;
    }

    while (QLayoutItem* item = loadingItemsLayout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            delete widget;
        }
        delete item;
    }
    loadingChecklistItems_.clear();
}

void MainWindow::rebuildLoadingOverlayItems(const QStringList& items)
{
    if (loadingItemsLayout_ == nullptr) {
        loadingChecklistItems_.clear();
        return;
    }

    clearLoadingOverlayItems();
    for (const QString& item : items) {
        auto* row = new QWidget(loadingItemsWidget_);
        row->setObjectName(QStringLiteral("loadingChecklistRow"));
        row->setAttribute(Qt::WA_StyledBackground, true);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);

        auto* label = new QLabel(row);
        label->setObjectName(QStringLiteral("loadingChecklistItem"));
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setWordWrap(false);
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        rowLayout->addWidget(label, 0, Qt::AlignLeft | Qt::AlignVCenter);

        auto* detailLabel = new QLabel(row);
        detailLabel->setObjectName(QStringLiteral("loadingChecklistDetail"));
        detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        detailLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        detailLabel->setWordWrap(false);
        detailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        detailLabel->setMinimumWidth(140);
        detailLabel->setMaximumWidth(LoadingChecklistDetailMaximumWidth);
        detailLabel->setFixedHeight(detailLabel->fontMetrics().height() + 2);
        rowLayout->addWidget(detailLabel, 1, Qt::AlignRight | Qt::AlignVCenter);

        updateLoadingChecklistRow(row, item);
        loadingItemsLayout_->addWidget(row);
    }
    loadingChecklistItems_ = items;
}

void MainWindow::updateLoadingOverlayItems(const QStringList& items)
{
    if (loadingItemsLayout_ == nullptr) {
        loadingChecklistItems_.clear();
        return;
    }

    if (loadingChecklistItems_.size() != items.size()
        || loadingItemsLayout_->count() != items.size()) {
        rebuildLoadingOverlayItems(items);
        return;
    }

    for (int index = 0; index < items.size(); ++index) {
        if (loadingChecklistItems_.at(index) == items.at(index)) {
            continue;
        }

        QLayoutItem* layoutItem = loadingItemsLayout_->itemAt(index);
        QWidget* row = layoutItem == nullptr ? nullptr : layoutItem->widget();
        updateLoadingChecklistRow(row, items.at(index));
    }
    loadingChecklistItems_ = items;
}

void MainWindow::updateLoadingChecklistRow(QWidget* row, const QString& item)
{
    if (row == nullptr) {
        return;
    }

    const auto parts = splitCoreStartupChecklistItem(item);
    auto* label = row->findChild<QLabel*>(QStringLiteral("loadingChecklistItem"));
    if (label != nullptr) {
        label->setText(parts.first);
    }

    auto* detailLabel = row->findChild<QLabel*>(QStringLiteral("loadingChecklistDetail"));
    if (detailLabel == nullptr) {
        return;
    }

    const bool hasDetail = !parts.second.isEmpty();
    detailLabel->setVisible(hasDetail);
    detailLabel->setText(hasDetail
        ? elideTextWithThreeDots(
            detailLabel->fontMetrics(),
            parts.second,
            LoadingChecklistDetailMaximumWidth)
        : QString());
    detailLabel->setToolTip(hasDetail ? parts.second : QString());
}

void MainWindow::updateLoadingOverlayGeometry()
{
    if (loadingOverlay_ != nullptr) {
        loadingOverlay_->setGeometry(rect());
        if (loadingContentWidget_ != nullptr) {
            const int availableWidth = qMax(1, loadingOverlay_->width() - (LoadingOverlayHorizontalMargin * 2));
            const int contentWidth = qBound(
                qMin(LoadingContentMinimumWidth, availableWidth),
                qRound(loadingOverlay_->width() * 0.6),
                qMin(LoadingContentMaximumWidth, availableWidth));
            loadingContentWidget_->setFixedWidth(qMax(1, contentWidth));
        }
        if (loadingOverlay_->isVisible()) {
            loadingOverlay_->raise();
        }
    }
}
