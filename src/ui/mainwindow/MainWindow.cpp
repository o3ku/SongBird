#include "ui/mainwindow/MainWindow.h"

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
#include <QGuiApplication>
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
#include <QScreen>
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
#include "ui/mainwindow/ServerTableView.h"
#include "ui/models/LogFilterProxyModel.h"
#include "ui/models/LogListModel.h"
#include "ui/models/ServerFilterProxyModel.h"
#include "ui/models/ServerTableModel.h"
#include "ui/qr/QrCodeRenderer.h"
#include "ui/theme/AppTheme.h"

namespace {

constexpr int DefaultServerLogSplitPercent = 60;
constexpr int DefaultServerQrSplitPercent = 78;
constexpr int DefaultMainWindowWidth = 1000;
constexpr int DefaultMainWindowHeight = 640;
constexpr int ToolbarControlSpacing = 2;
constexpr int HeaderFilterMinimumCharacters = 14;
constexpr int RoutingComboMinimumCharacters = 12;
constexpr int QrPreviewPadding = 10;
constexpr int ShareLinkHorizontalPadding = 6;
constexpr int ShareLinkBottomPadding = 10;
constexpr int ServerTableNoColumn = 0;

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

class ShareLinkTextEdit final : public QPlainTextEdit {
public:
    explicit ShareLinkTextEdit(QWidget* parent = nullptr)
        : QPlainTextEdit(parent)
    {
        setReadOnly(true);
        setFrameStyle(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setLineWrapMode(QPlainTextEdit::WidgetWidth);
        setWordWrapMode(QTextOption::WrapAnywhere);
        setBackgroundVisible(false);
        setViewportMargins(ShareLinkHorizontalPadding, 0, ShareLinkHorizontalPadding, ShareLinkBottomPadding);
        setProperty("shareLinkBottomPadding", ShareLinkBottomPadding);
        document()->setDocumentMargin(0.0);

        connect(document(), &QTextDocument::contentsChanged, this, [this]() {
            updateGeometry();
        });
    }

    bool hasHeightForWidth() const override
    {
        return true;
    }

    int heightForWidth(int width) const override
    {
        return documentHeightForWidth(width);
    }

    QSize sizeHint() const override
    {
        const int widthHint = qMax(minimumWidth(), width() > 0 ? width() : minimumWidth());
        return QSize(QPlainTextEdit::sizeHint().width(), documentHeightForWidth(widthHint));
    }

    QSize minimumSizeHint() const override
    {
        const int minimumHeight = fontMetrics().lineSpacing() + ShareLinkBottomPadding;
        return QSize(QPlainTextEdit::minimumSizeHint().width(), minimumHeight);
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QPlainTextEdit::resizeEvent(event);
    }

private:
    int documentHeightForWidth(int width) const
    {
        if (width <= 0) {
            return 0;
        }

        QTextDocument document;
        document.setDefaultFont(font());
        QTextOption option = document.defaultTextOption();
        option.setWrapMode(QTextOption::WrapAnywhere);
        document.setDefaultTextOption(option);
        document.setDocumentMargin(0.0);
        document.setPlainText(toPlainText());
        document.setTextWidth(qMax(0, width - (ShareLinkHorizontalPadding * 2)));
        return qCeil(document.size().height()) + ShareLinkBottomPadding;
    }
};

const VmessItem* resolveActiveServerForWindow(const Config& config)
{
    if (!config.currentIndexId.trimmed().isEmpty()) {
        for (const VmessItem& item : config.servers) {
            if (item.indexId == config.currentIndexId) {
                return &item;
            }
        }
    }

    return config.servers.isEmpty() ? nullptr : &config.servers.constFirst();
}

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

QString resolvePersistedSelectionId(const QString& tabKey)
{
    if (tabKey == QStringLiteral("ungrouped")) {
        return QStringLiteral("__unsubscribed__");
    }

    if (tabKey.startsWith(QStringLiteral("sub:"))) {
        return tabKey.mid(QStringLiteral("sub:").size());
    }

    return {};
}

QString resolveTabKeyFromSelectionId(const QString& selectionId)
{
    const QString normalized = selectionId.trimmed();
    if (normalized.isEmpty() || normalized == QStringLiteral("__all__")) {
        return QStringLiteral("all");
    }

    if (normalized == QStringLiteral("__unsubscribed__")
        || normalized.compare(QStringLiteral("ungrouped"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("ungrouped");
    }

    return QStringLiteral("sub:%1").arg(normalized);
}

QString resolveSubscriptionIdFromTabKey(const QString& tabKey)
{
    return tabKey.startsWith(QStringLiteral("sub:"))
        ? tabKey.mid(QStringLiteral("sub:").size())
        : QString();
}

QPoint clampWindowPosition(const QPoint& topLeft, const QSize& size)
{
    QList<QRect> workingAreas;
    const auto screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        if (screen != nullptr) {
            workingAreas.append(screen->availableGeometry());
        }
    }

    QRect targetArea;
    for (const QRect& area : workingAreas) {
        if (area.contains(topLeft) || area.intersects(QRect(topLeft, size))) {
            targetArea = area;
            break;
        }
    }

    if (!targetArea.isValid()) {
        if (QScreen* primaryScreen = QGuiApplication::primaryScreen()) {
            targetArea = primaryScreen->availableGeometry();
        }
    }

    if (!targetArea.isValid()) {
        return topLeft;
    }

    const int maxX = std::max(targetArea.left(), targetArea.right() - size.width() + 1);
    const int maxY = std::max(targetArea.top(), targetArea.bottom() - size.height() + 1);
    return QPoint(
        qBound(targetArea.left(), topLeft.x(), maxX),
        qBound(targetArea.top(), topLeft.y(), maxY));
}

QIcon loadToolbarIcon(const QString& fileName)
{
    return QIcon(QStringLiteral(":/toolbar/%1").arg(fileName));
}

QStringList defaultServerColumnKeys()
{
    return {
        QStringLiteral("Default"),
        QStringLiteral("Type"),
        QStringLiteral("Remarks"),
        QStringLiteral("Address"),
        QStringLiteral("Security"),
        QStringLiteral("Network"),
        QStringLiteral("StreamSecurity"),
        QStringLiteral("TestResult")
    };
}

constexpr int kServerResultColumn = 7;

QList<int> defaultServerColumnWidths(int availableWidth = 0)
{
    QList<int> widths{
        44,  // No.
        100, // Type
        160, // Alias
        240, // Address
        80,  // Security
        80,  // Network
        64,  // TLS
        84   // Result
    };

    const int minimumTotalWidth = std::accumulate(widths.cbegin(), widths.cend(), 0);
    if (availableWidth <= 0) {
        availableWidth = DefaultMainWindowWidth;
    }
    if (availableWidth <= minimumTotalWidth) {
        return widths;
    }

    const int extraWidth = availableWidth - minimumTotalWidth;
    const int aliasExtraWidth = qRound((extraWidth * 3.0) / 7.0);
    widths[2] += aliasExtraWidth;
    widths[3] += extraWidth - aliasExtraWidth;
    return widths;
}

bool hasLegacyServerColumnWidths(const QMap<QString, int>& widths)
{
    return widths.contains(QStringLiteral("Port"));
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

    // Defer the actual width calculation until after the first show — font
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

void MainWindow::setConfig(const Config& config, const QList<ServerStatItem>& statistics)
{
    config_ = config;
    const QString preferredSelectedId = selectedServerId().trimmed();
    if (!preferredSelectedId.isEmpty()) {
        lastSelectedServerId_ = preferredSelectedId;
    }

    serverModel_->setItems(config.servers, statistics, config.currentIndexId);
    currentIndexId_ = config.currentIndexId.trimmed();
    tunEnabled_ = config.tunModeItem.enableTun;
    currentCoreName_ = QStringLiteral("Unknown");
    for (const VmessItem& item : config.servers) {
        if (item.indexId == currentIndexId_) {
            currentCoreName_ = coreTypeDisplayName(resolveSelectedCoreType(config, item, availableCoreTypes()));
            break;
        }
    }
    updateRoutingModeOptions(config);
    rebuildSubscriptionTabs(config);

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

    if (serverFilterModel_ != nullptr && indexIds.size() > 1) {
        serverFilterModel_->setDynamicSortFilter(false);
    }

    for (const QString& indexId : indexIds) {
        serverModel_->updateTestResult(indexId, result);
    }

    if (serverFilterModel_ != nullptr && indexIds.size() > 1) {
        serverFilterModel_->setDynamicSortFilter(true);
        serverFilterModel_->invalidate();
    }
}

void MainWindow::appendLog(const QString& message)
{
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")))
                             .arg(message);

    if (logModel_ == nullptr) {
        return;
    }

    if (logScrollTimer_ == nullptr) {
        logScrollTimer_ = new QTimer(this);
        logScrollTimer_->setSingleShot(true);
        logScrollTimer_->setInterval(100);
        connect(logScrollTimer_, &QTimer::timeout, this, [this]() {
            if (logView_ == nullptr) {
                return;
            }

            const bool filterActive = logFilterEdit_ != nullptr && !logFilterEdit_->text().trimmed().isEmpty();
            if (logStickToBottomEnabled_ || (!filterActive && logWasAtBottom_)) {
                logView_->scrollToBottom();
            }
            logWasAtBottom_ = false;
            updateLogContextActions();
        });
    }

    if (!logScrollTimer_->isActive()) {
        logWasAtBottom_ = logStickToBottomEnabled_ || shouldStickLogViewToBottom(false);
        logScrollTimer_->start();
    }

    logModel_->appendLine(line);
}

void MainWindow::showTransientStatus(const QString& message, int timeoutMs)
{
    Q_UNUSED(message)
    Q_UNUSED(timeoutMs)
}

void MainWindow::clearTransientStatus()
{
    updateStatusIndicators();
}

void MainWindow::setHideToTrayEnabled(bool enabled)
{
    hideToTrayEnabled_ = enabled;
}

void MainWindow::setAllowClose(bool allowClose)
{
    allowClose_ = allowClose;
}

void MainWindow::setAutoRunEnabled(bool enabled)
{
    autoRunEnabled_ = enabled;
    if (toggleAutoRunAction_ != nullptr) {
        toggleAutoRunAction_->setText(enabled
            ? tr("Disable Auto Run")
            : tr("Enable Auto Run"));
    }
    updateStatusIndicators();
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

    updateActionState();
    updateStatusIndicators();
    updateWindowTitle();
}

void MainWindow::setProxyEnabled(bool enabled)
{
    setSystemProxyState(
        enabled ? toLegacySystemProxyModeValue(SystemProxyMode::ForcedChange)
                : toLegacySystemProxyModeValue(SystemProxyMode::ForcedClear),
        enabled);
}

void MainWindow::setCoreRunning(bool enabled, bool pending)
{
    coreRunning_ = enabled;
    coreTransitionPending_ = pending;
    updateActionState();
    updateStatusIndicators();
    updateWindowTitle();
}

void MainWindow::updateCoreToggleAction()
{
    if (coreToggleAction_ == nullptr) {
        return;
    }

    coreToggleAction_->setText(tr("START"));
    coreToggleAction_->setCheckable(true);
    coreToggleAction_->setChecked(coreRunning_ && !coreTransitionPending_);

    QString toolTip = tr("Start or stop the core.");
    const VmessItem* activeServer = resolveActiveServerForWindow(config_);
    if (coreTransitionPending_) {
        toolTip = coreRunning_
            ? tr("Core start/stop is in progress.")
            : tr("Core start is in progress.");
    } else if (activeServer == nullptr) {
        toolTip = tr("No available server. Add or import a server first.");
    } else {
        const CoreType requiredCore = resolveSelectedCoreType(config_, *activeServer, existingCoreTypes_);
        if (!existingCoreTypes_.contains(requiredCore)) {
            toolTip = tr("No compatible %1 core is installed for the active server \"%2\".")
                          .arg(coreTypeDisplayName(requiredCore))
                          .arg(activeServer->remarks.trimmed().isEmpty()
                                   ? tr("Unnamed server")
                                   : activeServer->remarks.trimmed());
        } else if (coreRunning_) {
            toolTip = tr("Stop the running core.");
        } else {
            toolTip = tr("Start the core with the active server.");
        }
    }
    coreToggleAction_->setToolTip(toolTip);
}

void MainWindow::updateProxyToggleAction()
{
    if (proxyToggleAction_ == nullptr) {
        return;
    }

    proxyToggleAction_->setText(tr("PROXY"));
    proxyToggleAction_->setCheckable(true);
    proxyToggleAction_->setChecked(systemProxyApplied_);
}

void MainWindow::setCurrentServerName(const QString& name)
{
    currentServerName_ = name.trimmed();
    updateStatusIndicators();
    updateWindowTitle();
}

void MainWindow::setRoutingSummary(const QString& routingText, const QString& listenText)
{
    routingSummary_ = routingText.trimmed();
    listenSummary_ = listenText.trimmed();
    updateStatusIndicators();
}

void MainWindow::setStatisticsSessionState(const StatisticsSessionState& state)
{
    statisticsState_ = state;
    if (statisticsState_.running && statisticsState_.coreType != CoreType::Unknown) {
        currentCoreName_ = coreTypeDisplayName(statisticsState_.coreType);
    }
    updateStatusIndicators();
    updateWindowTitle();
}

void MainWindow::setTrafficSummary(const QString& text)
{
    Q_UNUSED(text)
}

void MainWindow::setSpeedTestRunning(bool running)
{
    speedTestRunning_ = running;
    updateActionState();
}

void MainWindow::setSubscriptionUpdateRunning(bool running)
{
    if (loadingOverlay_ != nullptr && serverView_ != nullptr && serverView_->viewport() != nullptr) {
        if (running) {
            loadingOverlay_->setGeometry(serverView_->viewport()->rect());
            loadingOverlay_->raise();
            loadingOverlay_->show();
            serverView_->setEnabled(false);
        } else {
            loadingOverlay_->hide();
            serverView_->setEnabled(true);
        }
    }

    updateActionState();
}

void MainWindow::setBackgroundTaskRunning(bool running)
{
    backgroundTaskRunning_ = running;
    updateActionState();
    updateStatusIndicators();
}

void MainWindow::setBackgroundTaskDescription(const QString& description)
{
    backgroundTaskDescription_ = description.trimmed();
    updateStatusIndicators();
}

void MainWindow::restoreUiState(const Config& config)
{
    const QSize restoredSize(
        qMax(minimumWidth(), config.mainSizeWidth > 0 ? config.mainSizeWidth : width()),
        qMax(minimumHeight(), config.mainSizeHeight > 0 ? config.mainSizeHeight : height()));
    if (restoredSize.width() > 0 && restoredSize.height() > 0) {
        resize(restoredSize);
    }

    if (config.mainLocationX != 0 || config.mainLocationY != 0) {
        move(clampWindowPosition(QPoint(config.mainLocationX, config.mainLocationY), size()));
    }

    pendingColumnWidths_ = config.mainColumnWidths;
    serverLogSplitPercent_ = clampSplitPercent(config.mainServerLogSplitPercent, DefaultServerLogSplitPercent);
    serverQrSplitPercent_ = clampSplitPercent(config.mainServerQrSplitPercent, DefaultServerQrSplitPercent);
    qrPreviewVisible_ = false;
    if (qrPanel_ != nullptr) {
        qrPanel_->setVisible(qrPreviewVisible_);
    }
    coreRunning_ = config.mainCoreRunning;
    coreTransitionPending_ = false;
    systemProxyApplied_ = config.mainProxyEnabled;
    updateActionState();
    updateStatusIndicators();
    updateWindowTitle();
    updateQrPanelActionText();

    if (subscriptionTabBar_ != nullptr && serverModel_ != nullptr) {
        QString preferredTabKey = resolveTabKeyFromSelectionId(config.mainSelectedSubId);

        // Switch to the tab that contains the current server
        if (!config.currentIndexId.isEmpty()) {
            const VmessItem* currentServer = nullptr;
            for (int i = 0; i < serverModel_->rowCount(); ++i) {
                const VmessItem* item = serverModel_->itemAt(i);
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

                // Check if the preferred tab already shows this server
                bool serverVisibleInPreferred = (preferredTabKey == QStringLiteral("all"));
                if (!serverVisibleInPreferred && preferredTabKey == serverTabKey) {
                    serverVisibleInPreferred = true;
                }

                if (!serverVisibleInPreferred) {
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

    uiStateRestorePending_ = true;
}

void MainWindow::captureUiState(Config& config) const
{
    QRect bounds = geometry();
    const QRect normalBounds = normalGeometry();
    if ((isMaximized() || isMinimized()) && normalBounds.isValid()) {
        bounds = normalBounds;
    }

    config.mainLocationX = bounds.x();
    config.mainLocationY = bounds.y();
    config.mainSizeWidth = bounds.width();
    config.mainSizeHeight = bounds.height();
    config.mainColumnWidths = captureServerColumnWidths();
    config.mainSelectedSubId = persistedSubscriptionSelectionId();
    config.mainServerLogSplitPercent = captureSplitPercent(rootSplitter_, serverLogSplitPercent_);
    config.mainServerQrSplitPercent = captureSplitPercent(topSplitter_, serverQrSplitPercent_);
    config.mainQrPreviewVisible = false;
    config.mainCoreRunning = coreRunning_;
    config.mainProxyEnabled = systemProxyApplied_;
}

bool MainWindow::selectSubscriptionTab(const QString& selectionId)
{
    if (subscriptionTabBar_ == nullptr) {
        return false;
    }

    const QString targetTabKey = resolveTabKeyFromSelectionId(selectionId);
    for (int index = 0; index < subscriptionTabBar_->count(); ++index) {
        if (subscriptionTabBar_->tabData(index).toString() == targetTabKey) {
            subscriptionTabBar_->setCurrentIndex(index);
            return true;
        }
    }

    return false;
}

void MainWindow::onServerSelectionChanged()
{
    const VmessItem* item = selectedServer();
    if (item == nullptr) {
        lastSelectedServerId_.clear();
        clearTransientStatus();
        updateQrPreview();
        updateActionState();
        return;
    }

    lastSelectedServerId_ = item->indexId;
    showTransientStatus(tr("Selected %1").arg(item->remarks), 3000);
    updateQrPreview();
    updateActionState();
}

void MainWindow::onServerFilterTextChanged(const QString& text)
{
    if (serverFilterModel_ == nullptr) {
        return;
    }

    const QString pattern = text.trimmed();
    serverFilterModel_->setFilterRegularExpression(
        QRegularExpression(QRegularExpression::escape(pattern), QRegularExpression::CaseInsensitiveOption));
    updateServerReorderAvailability();
    updateSelectionForVisibleRows();
}

void MainWindow::updateActionState()
{
    const QList<const VmessItem*> selectedItems = selectedServers();
    const VmessItem* selectedItem = selectedItems.isEmpty() ? nullptr : selectedItems.constFirst();
    const bool hasSelection = selectedItem != nullptr;
    const bool hasSingleSelection = selectedItems.size() == 1;
    const bool hasCustomSelection = hasSingleSelection && selectedItem->configType == ConfigType::Custom;
    const bool hasServerConfigSelection = hasSingleSelection
        && (selectedItem->configType == ConfigType::VMess || selectedItem->configType == ConfigType::VLESS)
        && selectedItem->streamSecurity.trimmed().compare(QStringLiteral("reality"), Qt::CaseInsensitive) != 0;
    bool hasShareExports = false;
    for (const VmessItem* item : selectedItems) {
        if (item != nullptr && item->configType != ConfigType::Custom && item->configType != ConfigType::Unknown) {
            hasShareExports = true;
            break;
        }
    }
    const bool hasServers = serverModel_ != nullptr && serverModel_->rowCount() > 0;
    const bool canReorder = hasSelection && hasServers && serverModel_->rowCount() > 1;
    const bool canStartTask = canStartBackgroundTask();
    const bool canSpeedTest = hasSelection && canStartTask;
    if (editServerAction_ != nullptr) {
        editServerAction_->setEnabled(hasSelection);
    }

    if (duplicateServerAction_ != nullptr) {
        duplicateServerAction_->setEnabled(hasSelection);
    }

    if (exportClientConfigAction_ != nullptr) {
        exportClientConfigAction_->setEnabled(hasSingleSelection);
    }

    if (exportServerConfigAction_ != nullptr) {
        exportServerConfigAction_->setEnabled(hasServerConfigSelection);
    }

    if (copyShareLinkAction_ != nullptr) {
        copyShareLinkAction_->setEnabled(hasShareExports);
    }

    if (copySubscriptionContentAction_ != nullptr) {
        copySubscriptionContentAction_->setEnabled(hasShareExports);
    }

    if (copySubscriptionUrlAction_ != nullptr) {
        copySubscriptionUrlAction_->setEnabled(!currentSubscriptionUrl().isEmpty());
    }

    if (openCustomConfigAction_ != nullptr) {
        openCustomConfigAction_->setEnabled(hasCustomSelection);
    }

    if (removeServerAction_ != nullptr) {
        removeServerAction_->setEnabled(hasSelection);
    }

    if (moveServerTopAction_ != nullptr) {
        moveServerTopAction_->setEnabled(canReorder);
    }

    if (moveServerUpAction_ != nullptr) {
        moveServerUpAction_->setEnabled(canReorder);
    }

    if (moveServerDownAction_ != nullptr) {
        moveServerDownAction_->setEnabled(canReorder);
    }

    if (moveServerBottomAction_ != nullptr) {
        moveServerBottomAction_->setEnabled(canReorder);
    }

    if (setDefaultServerAction_ != nullptr) {
        setDefaultServerAction_->setEnabled(hasSelection);
    }

    if (testAction_ != nullptr) {
        testAction_->setEnabled(canSpeedTest);
    }

    if (importClipboardAction_ != nullptr) {
        importClipboardAction_->setEnabled(canStartTask);
    }

    if (updateSubscriptionsAction_ != nullptr) {
        updateSubscriptionsAction_->setEnabled(canStartTask);
    }

    if (updateCurrentSubscriptionAction_ != nullptr) {
        updateCurrentSubscriptionAction_->setEnabled(canStartTask);
    }

    if (testMeAction_ != nullptr) {
        testMeAction_->setEnabled(canStartTask);
    }

    if (updateV2RayCoreAction_ != nullptr) {
        updateV2RayCoreAction_->setEnabled(canStartTask);
    }

    if (updateXrayCoreAction_ != nullptr) {
        updateXrayCoreAction_->setEnabled(canStartTask);
    }

    if (updateSingBoxCoreAction_ != nullptr) {
        updateSingBoxCoreAction_->setEnabled(canStartTask);
    }

    if (updateClashCoreAction_ != nullptr) {
        updateClashCoreAction_->setEnabled(canStartTask);
    }

    if (updateClashMetaCoreAction_ != nullptr) {
        updateClashMetaCoreAction_->setEnabled(canStartTask);
    }

    if (updateGeoResourcesAction_ != nullptr) {
        updateGeoResourcesAction_->setEnabled(canStartTask);
    }

    if (updateCurrentSubscriptionShortcutAction_ != nullptr) {
        updateCurrentSubscriptionShortcutAction_->setEnabled(canStartTask);
    }

    updateCoreToggleAction();
    const bool startToggleEnabled = hasServers && !coreTransitionPending_;
    if (coreToggleAction_ != nullptr) {
        coreToggleAction_->setEnabled(startToggleEnabled);
    }

    updateProxyToggleAction();
    if (proxyToggleAction_ != nullptr) {
        proxyToggleAction_->setEnabled(coreRunning_ && !coreTransitionPending_);
    }
}

void MainWindow::toggleServerSorting(int logicalIndex)
{
    if (serverView_ == nullptr
        || serverView_->horizontalHeader() == nullptr
        || serverFilterModel_ == nullptr
        || logicalIndex < 0
        || logicalIndex >= serverFilterModel_->columnCount()) {
        return;
    }

    if (serverSortColumn_ != logicalIndex || serverSortColumn_ < 0) {
        serverSortColumn_ = logicalIndex;
        serverSortOrder_ = Qt::AscendingOrder;
    } else {
        serverSortOrder_ = serverSortOrder_ == Qt::AscendingOrder
            ? Qt::DescendingOrder
            : Qt::AscendingOrder;
    }

    QHeaderView* header = serverView_->horizontalHeader();
    header->setSortIndicator(serverSortColumn_, serverSortOrder_);
    serverFilterModel_->sort(serverSortColumn_, serverSortOrder_);

    updateServerReorderAvailability();
    updateSelectionForVisibleRows();
    updateActionState();
    updateQrPreview();
}

void MainWindow::updateQrPreview()
{
    if (qrPlaceholder_ == nullptr || shareLinkLabel_ == nullptr) {
        return;
    }

    if (!qrPreviewVisible_ || qrPanel_ == nullptr || !qrPanel_->isVisible()) {
        return;
    }

    const QList<const VmessItem*> selectedItems = selectedServers();
    if (selectedItems.isEmpty()) {
        shareLinkLabel_->clear();
        shareLinkLabel_->setToolTip(QString());
        qrPlaceholder_->setPixmap(QPixmap());
        qrPlaceholder_->setText(tr("QR preview placeholder"));
        qrPlaceholder_->setToolTip(QString());
        return;
    }

    const VmessItem* firstItem = selectedItems.constFirst();
    const QString firstShareUrl = firstItem != nullptr ? ShareUrlBuilder::build(*firstItem).trimmed() : QString();

    if (selectedItems.size() == 1) {
        shareLinkLabel_->setPlainText(firstShareUrl);
        shareLinkLabel_->setToolTip(firstShareUrl);
    } else {
        const QStringList shareLinks = buildShareLinks(selectedItems);
        const QString shareText = shareLinks.join(QChar('\n'));
        shareLinkLabel_->setPlainText(shareText);
        shareLinkLabel_->setToolTip(shareText);
    }

    if (firstShareUrl.isEmpty()) {
        qrPlaceholder_->setPixmap(QPixmap());
        qrPlaceholder_->setText(tr("QR preview unavailable for this server."));
        qrPlaceholder_->setToolTip(QString());
        return;
    }

    const int qrMargin = qrPlaceholder_->margin() * 2;
    const int qrExtent = qMin(
        qMax(0, qrPlaceholder_->width() - qrMargin),
        qMax(0, qrPlaceholder_->height() - qrMargin));
    qrPlaceholder_->setText(QString());
    if (qrExtent <= 0) {
        qrPlaceholder_->setPixmap(QPixmap());
        qrPlaceholder_->setToolTip(firstShareUrl);
        return;
    }
    qrPlaceholder_->setPixmap(QrCodeRenderer::render(firstShareUrl, qrExtent));
    qrPlaceholder_->setToolTip(firstShareUrl);
}

void MainWindow::updateQrPanelActionText()
{
    if (toggleQrPanelAction_ != nullptr) {
        const QSignalBlocker blocker(toggleQrPanelAction_);
        toggleQrPanelAction_->setChecked(qrPreviewVisible_);
    }
}

void MainWindow::applyLogFilter()
{
    if (logFilterModel_ == nullptr || logView_ == nullptr) {
        return;
    }

    const auto setValidationState = [this](const char* value) {
        if (logFilterEdit_ == nullptr) {
            return;
        }

        logFilterEdit_->setProperty("validationState", QString::fromLatin1(value));
        logFilterEdit_->style()->unpolish(logFilterEdit_);
        logFilterEdit_->style()->polish(logFilterEdit_);
        logFilterEdit_->update();
    };

    const QString pattern = logFilterEdit_ == nullptr ? QString() : logFilterEdit_->text().trimmed();
    const bool wasAtBottom = logStickToBottomEnabled_ || shouldStickLogViewToBottom(true);

    logFilterModel_->setPattern(pattern, false);
    if (!logFilterModel_->hasValidPattern()) {
        if (logFilterEdit_ != nullptr) {
            setValidationState("error");
            const QRegularExpression expression(
                pattern,
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            logFilterEdit_->setToolTip(expression.errorString());
        }
    } else {
        if (logFilterEdit_ != nullptr) {
            setValidationState("");
            logFilterEdit_->setToolTip(QString());
        }
    }

    if (pattern.isEmpty() && wasAtBottom) {
        logView_->scrollToBottom();
        logWasAtBottom_ = true;
    } else if (logFilterModel_->rowCount() > 0 && logView_->verticalScrollBar() != nullptr) {
        const QModelIndex topIndex = logView_->indexAt(QPoint(1, 1));
        const int preservedTopRow = topIndex.isValid() ? topIndex.row() : -1;
        if (preservedTopRow >= 0) {
            const QModelIndex restoreIndex = logFilterModel_->index(qMin(preservedTopRow, logFilterModel_->rowCount() - 1), 0);
            if (restoreIndex.isValid()) {
                logView_->scrollTo(restoreIndex, QAbstractItemView::PositionAtTop);
            }
        }
    }

    updateLogContextActions();
}

bool MainWindow::shouldStickLogViewToBottom(bool filterActive) const
{
    if (filterActive || logView_ == nullptr) {
        return false;
    }

    QScrollBar* verticalScrollBar = logView_->verticalScrollBar();
    return verticalScrollBar != nullptr && verticalScrollBar->value() >= verticalScrollBar->maximum();
}

void MainWindow::updateLogContextActions()
{
    if (logView_ == nullptr) {
        return;
    }

    const int visibleRows = logFilterModel_ == nullptr ? 0 : logFilterModel_->rowCount();
    const int selectedRows = logView_->selectionModel() == nullptr
        ? 0
        : logView_->selectionModel()->selectedRows().size();
    if (selectAllLogAction_ != nullptr) {
        selectAllLogAction_->setEnabled(visibleRows > 0);
    }
    if (copySelectedLogAction_ != nullptr) {
        copySelectedLogAction_->setEnabled(selectedRows > 0);
    }
    if (copyAllLogAction_ != nullptr) {
        copyAllLogAction_->setEnabled(visibleRows > 0);
    }
    if (clearLogAction_ != nullptr) {
        clearLogAction_->setEnabled(logModel_ != nullptr && logModel_->rowCount() > 0);
    }
}

void MainWindow::showLogContextMenu(const QPoint& position)
{
    if (logContextMenu_ == nullptr || logView_ == nullptr) {
        return;
    }

    updateLogContextActions();
    logContextMenu_->popup(logView_->mapToGlobal(position));
}

void MainWindow::updateSelectionForVisibleRows()
{
    if (serverView_ == nullptr
        || serverView_->selectionModel() == nullptr
        || serverFilterModel_ == nullptr
        || serverModel_ == nullptr) {
        return;
    }

    QModelIndex targetIndex;
    const QStringList preferredIds{
        lastSelectedServerId_.trimmed(),
        currentIndexId_.trimmed()};

    for (const QString& preferredId : preferredIds) {
        if (preferredId.isEmpty()) {
            continue;
        }

        for (int proxyRow = 0; proxyRow < serverFilterModel_->rowCount(); ++proxyRow) {
            const QModelIndex proxyIndex = serverFilterModel_->index(proxyRow, 0);
            const QModelIndex sourceIndex = serverFilterModel_->mapToSource(proxyIndex);
            const VmessItem* item = serverModel_->itemAt(sourceIndex.row());
            if (item != nullptr && item->indexId == preferredId) {
                targetIndex = proxyIndex;
                break;
            }
        }

        if (targetIndex.isValid()) {
            break;
        }
    }

    if (!targetIndex.isValid() && serverFilterModel_->rowCount() > 0) {
        targetIndex = serverFilterModel_->index(0, 0);
    }

    serverView_->clearSelection();
    if (!targetIndex.isValid()) {
        if (serverView_->selectionModel() != nullptr) {
            serverView_->selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
        }
        lastSelectedServerId_.clear();
        updateQrPreview();
        updateActionState();
        return;
    }

    serverView_->setCurrentIndex(targetIndex);
    serverView_->selectRow(targetIndex.row());
}

void MainWindow::updateServerReorderAvailability()
{
    if (serverView_ == nullptr
        || serverView_->horizontalHeader() == nullptr
        || serverFilterEdit_ == nullptr
        || serverFilterModel_ == nullptr
        || serverModel_ == nullptr) {
        return;
    }

    const QHeaderView* header = serverView_->horizontalHeader();
    const int sortingSection = header->sortIndicatorSection();
    const bool sourceOrderRestored = sortingSection == ServerTableNoColumn
        && header->sortIndicatorOrder() == Qt::AscendingOrder;
    const bool sortingActive = sortingSection >= 0 && !sourceOrderRestored;
    const bool showingAllVisibleRows = serverFilterModel_->rowCount() == serverModel_->rowCount();
    const bool enabled = serverFilterEdit_->text().trimmed().isEmpty()
        && showingAllVisibleRows
        && !sortingActive;
    serverView_->setRowsReorderEnabled(enabled);
    serverView_->setToolTip(enabled
        ? tr("Drag rows to reorder servers.")
        : (sortingActive
                ? tr("Switch back to No. ascending order before dragging to reorder servers.")
                : tr("Drag reordering is only available when the full server list is visible.")));
}

void MainWindow::updateSubscriptionFilter()
{
    if (serverFilterModel_ == nullptr) {
        return;
    }

    const QString tabKey = currentSubscriptionTabKey();
    if (tabKey == QStringLiteral("ungrouped")) {
        serverFilterModel_->setSubscriptionFilterMode(ServerFilterProxyModel::SubscriptionFilterMode::Ungrouped);
    } else if (tabKey.startsWith(QStringLiteral("sub:"))) {
        serverFilterModel_->setSubscriptionFilterMode(
            ServerFilterProxyModel::SubscriptionFilterMode::Subscription,
            tabKey.mid(QStringLiteral("sub:").size()));
    } else {
        serverFilterModel_->setSubscriptionFilterMode(ServerFilterProxyModel::SubscriptionFilterMode::All);
    }

    updateServerReorderAvailability();
    updateSelectionForVisibleRows();
    updateActionState();
    updateQrPreview();
}

bool MainWindow::canStartBackgroundTask() const
{
    return !backgroundTaskRunning_ && !speedTestRunning_;
}

bool MainWindow::isUngroupedSubscriptionTabSelected() const
{
    return currentSubscriptionTabKey() == QStringLiteral("ungrouped");
}

QString MainWindow::currentSubscriptionTabKey() const
{
    if (subscriptionTabBar_ == nullptr || subscriptionTabBar_->currentIndex() < 0) {
        return QStringLiteral("all");
    }

    return subscriptionTabBar_->tabData(subscriptionTabBar_->currentIndex()).toString();
}

QString MainWindow::currentSubscriptionUrl() const
{
    const QString subscriptionId = resolveSubscriptionIdFromTabKey(currentSubscriptionTabKey());
    if (subscriptionId.isEmpty()) {
        return {};
    }

    for (const SubItem& item : config_.subscriptions) {
        if (item.id.trimmed() == subscriptionId) {
            return item.url.trimmed();
        }
    }

    return {};
}

QString MainWindow::describeSubscription(const SubItem& item)
{
    const QString remarks = item.remarks.trimmed();
    if (!remarks.isEmpty()) {
        return remarks;
    }

    const QString url = item.url.trimmed();
    if (!url.isEmpty()) {
        return url;
    }

    return item.id.trimmed().isEmpty()
        ? QCoreApplication::translate("MainWindow", "Subscription")
        : item.id.trimmed();
}

QStringList MainWindow::buildShareLinks(const QList<const VmessItem*>& items)
{
    QStringList shareLinks;
    for (const VmessItem* item : items) {
        if (item == nullptr) {
            continue;
        }

        const QString shareUrl = ShareUrlBuilder::build(*item).trimmed();
        if (!shareUrl.isEmpty()) {
            shareLinks.append(shareUrl);
        }
    }

    return shareLinks;
}

void MainWindow::rebuildSubscriptionTabs(const Config& config)
{
    if (subscriptionTabBar_ == nullptr || serverFilterModel_ == nullptr) {
        return;
    }

    const QString previousTabKey = currentSubscriptionTabKey();
    QSet<QString> knownSubscriptionIds;
    for (const SubItem& item : config.subscriptions) {
        if (!item.enabled) {
            continue;
        }
        const QString subscriptionId = item.id.trimmed();
        if (!subscriptionId.isEmpty()) {
            knownSubscriptionIds.insert(subscriptionId);
        }
    }
    serverFilterModel_->setKnownSubscriptionIds(knownSubscriptionIds);

    {
        const QSignalBlocker blocker(subscriptionTabBar_);
        for (int index = subscriptionTabBar_->count() - 1; index >= 0; --index) {
            subscriptionTabBar_->removeTab(index);
        }

        QSet<QString> addedSubscriptionIds;
        for (const SubItem& item : config.subscriptions) {
            if (!item.enabled) {
                continue;
            }
            const QString subscriptionId = item.id.trimmed();
            if (subscriptionId.isEmpty() || addedSubscriptionIds.contains(subscriptionId)) {
                continue;
            }

            addedSubscriptionIds.insert(subscriptionId);
            const int index = subscriptionTabBar_->insertTab(0, describeSubscription(item));
            subscriptionTabBar_->setTabData(index, QStringLiteral("sub:%1").arg(subscriptionId));
        }

        const int ungroupedIndex = subscriptionTabBar_->addTab(tr("Ungrouped"));
        subscriptionTabBar_->setTabData(ungroupedIndex, QStringLiteral("ungrouped"));

        int restoreIndex = -1;
        for (int index = 0; index < subscriptionTabBar_->count(); ++index) {
            if (subscriptionTabBar_->tabData(index).toString() == previousTabKey) {
                restoreIndex = index;
                break;
            }
        }

        if (subscriptionTabBar_->count() > 0) {
            subscriptionTabBar_->setCurrentIndex(restoreIndex >= 0 ? restoreIndex : 0);
        }
    }

    updateSubscriptionFilter();
}

QStringList MainWindow::buildReorderedServerIds(const QList<int>& movedRows, int targetRow) const
{
    if (serverFilterModel_ == nullptr || serverModel_ == nullptr) {
        return {};
    }

    QStringList orderedIds;
    orderedIds.reserve(serverFilterModel_->rowCount());
    for (int proxyRow = 0; proxyRow < serverFilterModel_->rowCount(); ++proxyRow) {
        const QModelIndex sourceIndex = serverFilterModel_->mapToSource(serverFilterModel_->index(proxyRow, 0));
        const VmessItem* item = serverModel_->itemAt(sourceIndex.row());
        if (item != nullptr && !item->indexId.isEmpty()) {
            orderedIds.append(item->indexId);
        }
    }

    if (orderedIds.isEmpty()) {
        return {};
    }

    QList<int> normalizedRows;
    for (int row : movedRows) {
        if (row >= 0 && row < orderedIds.size() && !normalizedRows.contains(row)) {
            normalizedRows.append(row);
        }
    }
    if (normalizedRows.isEmpty()) {
        return {};
    }

    std::sort(normalizedRows.begin(), normalizedRows.end());

    QStringList movedIds;
    movedIds.reserve(normalizedRows.size());
    for (int row : normalizedRows) {
        movedIds.append(orderedIds.at(row));
    }

    QStringList remainingIds = orderedIds;
    for (const QString& indexId : movedIds) {
        remainingIds.removeAll(indexId);
    }

    int insertionIndex = qBound(0, targetRow, orderedIds.size());
    for (int row : normalizedRows) {
        if (row < insertionIndex) {
            --insertionIndex;
        }
    }
    insertionIndex = qBound(0, insertionIndex, remainingIds.size());

    QStringList reorderedIds = remainingIds;
    for (int index = 0; index < movedIds.size(); ++index) {
        reorderedIds.insert(insertionIndex + index, movedIds.at(index));
    }

    return reorderedIds == orderedIds ? QStringList() : reorderedIds;
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

    addServerAction_ = new QAction(tr("Add Server"), this);
    addCustomServerAction_ = new QAction(tr("Import Custom"), this);
    editServerAction_ = new QAction(tr("Edit Server"), this);
    duplicateServerAction_ = new QAction(tr("Duplicate Server"), this);
    exportClientConfigAction_ = new QAction(tr("Export Client Config"), this);
    exportClientConfigAction_->setObjectName(QStringLiteral("exportClientConfigAction"));
    exportServerConfigAction_ = new QAction(tr("Export Server Config"), this);
    exportServerConfigAction_->setObjectName(QStringLiteral("exportServerConfigAction"));
    copyShareLinkAction_ = new QAction(tr("Copy Share Link"), this);
    copyShareLinkAction_->setObjectName(QStringLiteral("copyShareLinkAction"));
    copySubscriptionContentAction_ = new QAction(tr("Copy Subscription Content"), this);
    copySubscriptionContentAction_->setObjectName(QStringLiteral("copySubscriptionContentAction"));
    copySubscriptionUrlAction_ = new QAction(tr("Copy Subscription Url"), this);
    copySubscriptionUrlAction_->setObjectName(QStringLiteral("copySubscriptionUrlAction"));
    openCustomConfigAction_ = new QAction(tr("Open Config"), this);
    importClipboardAction_ = new QAction(tr("Import Clipboard"), this);
    importClipboardAction_->setObjectName(QStringLiteral("importClipboardAction"));
    importClientConfigAction_ = new QAction(tr("Import Client Config"), this);
    importServerConfigAction_ = new QAction(tr("Import Server Config"), this);
    subAction_ = new QAction(tr("Subscriptions"), this);
    subAction_->setIcon(loadToolbarIcon(QStringLiteral("sub.png")));
    connect(subAction_, &QAction::triggered, this, &MainWindow::openSettingsAtSubscriptionsTabRequested);
    updateSubscriptionsAction_ = new QAction(tr("Update Subscriptions"), this);
    updateSubscriptionsAction_->setObjectName(QStringLiteral("updateSubscriptionsAction"));
    testMeAction_ = new QAction(tr("TestMe"), this);
    testMeAction_->setObjectName(QStringLiteral("testMeAction"));
    updateV2RayCoreAction_ = new QAction(tr("Update V2Ray Core"), this);
    updateV2RayCoreAction_->setObjectName(QStringLiteral("updateV2RayCoreAction"));
    updateXrayCoreAction_ = new QAction(tr("Update Xray Core"), this);
    updateXrayCoreAction_->setObjectName(QStringLiteral("updateXrayCoreAction"));
    updateSingBoxCoreAction_ = new QAction(tr("Update sing-box Core"), this);
    updateSingBoxCoreAction_->setObjectName(QStringLiteral("updateSingBoxCoreAction"));
    updateClashCoreAction_ = new QAction(tr("Update Clash Core"), this);
    updateClashCoreAction_->setObjectName(QStringLiteral("updateClashCoreAction"));
    updateClashMetaCoreAction_ = new QAction(tr("Update Clash.Meta Core"), this);
    updateClashMetaCoreAction_->setObjectName(QStringLiteral("updateClashMetaCoreAction"));
    updateGeoResourcesAction_ = new QAction(tr("Update Geo Files"), this);
    updateGeoResourcesAction_->setObjectName(QStringLiteral("updateGeoResourcesAction"));
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
    openV2RayReleasePageAction_ = new QAction(tr("Open V2Ray Releases"), this);
    openV2RayReleasePageAction_->setObjectName(QStringLiteral("openV2RayReleasePageAction"));
    openXrayReleasePageAction_ = new QAction(tr("Open Xray Releases"), this);
    openXrayReleasePageAction_->setObjectName(QStringLiteral("openXrayReleasePageAction"));
    openSingBoxReleasePageAction_ = new QAction(tr("Open sing-box Releases"), this);
    openSingBoxReleasePageAction_->setObjectName(QStringLiteral("openSingBoxReleasePageAction"));
    openGeoReleasePageAction_ = new QAction(tr("Open Geo Releases"), this);
    openGeoReleasePageAction_->setObjectName(QStringLiteral("openGeoReleasePageAction"));
    coreToggleAction_ = new QAction(tr("START"), this);
    coreToggleAction_->setObjectName(QStringLiteral("coreToggleAction"));
    coreToggleAction_->setCheckable(true);
    proxyToggleAction_ = new QAction(tr("PROXY"), this);
    proxyToggleAction_->setObjectName(QStringLiteral("proxyToggleAction"));
    proxyToggleAction_->setCheckable(true);
    clearStatisticsAction_ = new QAction(tr("Clear Statistics"), this);
    toggleQrPanelAction_ = new QAction(tr("Share"), this);
    toggleQrPanelAction_->setObjectName(QStringLiteral("toggleQrPanelAction"));
    toggleQrPanelAction_->setCheckable(true);
    toggleQrPanelAction_->setChecked(qrPreviewVisible_);

    updateCurrentSubscriptionAction_ = new QAction(tr("Update Current Subscription"), this);
    updateCurrentSubscriptionAction_->setObjectName(QStringLiteral("updateCurrentSubscriptionMenuAction"));
    connect(updateCurrentSubscriptionAction_, &QAction::triggered, this, [this]() {
        const QString tabKey = currentSubscriptionTabKey();
        emit updateCurrentSubscriptionRequested(
            tabKey.startsWith(QStringLiteral("sub:"))
                ? tabKey.mid(QStringLiteral("sub:").size())
                : QString());
    });

    // Sub button now opens Settings at Subscriptions tab
    // updateCurrentSubscriptionAction and updateSubscriptionsAction_ are moved to Update menu

    auto* updateMenu = new QMenu(tr("Update"), toolBar);
    updateMenu->setObjectName(QStringLiteral("updateMenu"));
    updateMenu->addAction(updateV2RayCoreAction_);
    updateMenu->addAction(updateXrayCoreAction_);
    updateMenu->addAction(updateSingBoxCoreAction_);
    updateMenu->addAction(updateClashCoreAction_);
    updateMenu->addAction(updateClashMetaCoreAction_);
    updateMenu->addAction(updateGeoResourcesAction_);
    updateMenu->addSeparator();
    updateMenu->addAction(updateCurrentSubscriptionAction_);
    updateMenu->addAction(updateSubscriptionsAction_);
    updateMenu->addSeparator();
    updateMenu->addAction(openReleasePageAction_);
    updateMenu->addAction(openV2RayReleasePageAction_);
    updateMenu->addAction(openXrayReleasePageAction_);
    updateMenu->addAction(openSingBoxReleasePageAction_);
    updateMenu->addAction(openGeoReleasePageAction_);

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

    auto* settingMenu = new QMenu(toolBar);
    settingMenu->setObjectName(QStringLiteral("settingMenu"));
    settingMenu->addAction(settingsAction_);
    settingMenu->addAction(dnsSettingsAction_);
    settingMenu->addAction(globalHotkeySettingsAction_);

    auto* helpMenu = new QMenu(tr("Help"), toolBar);
    helpMenu->setObjectName(QStringLiteral("helpMenu"));
    helpMenu->addAction(aboutAction_);
    helpMenu->addSeparator();
    helpMenu->addAction(openProjectPageAction_);
    helpMenu->addAction(openDocumentationAction_);
    helpMenu->addAction(openDnsObjectDocumentationAction_);
    helpMenu->addAction(openRuleObjectDocumentationAction_);
    helpMenu->addAction(openLoopbackToolAction_);

    createToolbarActionButton(
        toolBar,
        QStringLiteral("subButton"),
        tr("Subscriptions"),
        loadToolbarIcon(QStringLiteral("sub.png")),
        subAction_);
    toolBar->addSeparator();
    createToolbarActionButton(
        toolBar,
        QStringLiteral("settingButton"),
        tr("Setting"),
        loadToolbarIcon(QStringLiteral("option.png")),
        settingsAction_);
    toolBar->addSeparator();

    createToolbarMenuButton(
        toolBar,
        QStringLiteral("updateButton"),
        tr("Update"),
        loadToolbarIcon(QStringLiteral("checkupdate.png")),
        updateMenu);
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
        QStringLiteral("coreToggleButton"),
        tr("START"),
        loadToolbarIcon(QStringLiteral("restart.png")),
        coreToggleAction_);
    toolBar->addWidget(createToolbarSpacing(toolBar, ToolbarControlSpacing));
    createToolbarActionButton(
        toolBar,
        QStringLiteral("proxyToggleButton"),
        tr("PROXY"),
        loadToolbarIcon(QStringLiteral("option.png")),
        proxyToggleAction_);
    toolBar->addWidget(createToolbarSpacing(toolBar, ToolbarControlSpacing));

    routingModeCombo_ = new StyledComboBox(toolBar);
    routingModeCombo_->setObjectName(QStringLiteral("routingModeCombo"));
    AppTheme::applyCompactFont(routingModeCombo_);
    updateContentSizedComboBox(routingModeCombo_, RoutingComboMinimumCharacters);
    configureStyledComboBox(routingModeCombo_);
    toolBar->addWidget(routingModeCombo_);
    toolBar->addWidget(createToolbarSpacing(toolBar, ToolbarControlSpacing));

    createToolbarActionButton(
        toolBar,
        QStringLiteral("qrCodeButton"),
        tr("Share"),
        loadToolbarIcon(QStringLiteral("share.png")),
        toggleQrPanelAction_);

    updateCoreToggleAction();
    updateProxyToggleAction();
    updateQrPanelActionText();
}

void MainWindow::setupServerView()
{
    serverModel_ = new ServerTableModel(this);
    serverFilterModel_ = new ServerFilterProxyModel(this);
    serverFilterModel_->setSourceModel(serverModel_);
    serverFilterModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    serverFilterModel_->setFilterKeyColumn(-1);
    serverFilterModel_->setDynamicSortFilter(true);
    serverFilterModel_->sort(-1);
    serverView_ = new ServerTableView(this);
    serverView_->setObjectName(QStringLiteral("serverTableView"));
    serverView_->setModel(serverFilterModel_);
    serverView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    serverView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    serverView_->setAlternatingRowColors(true);
    serverView_->setSortingEnabled(false);
    serverView_->setShowGrid(false);
    serverView_->setMinimumHeight(0);
    serverView_->setMouseTracking(true);
    serverView_->viewport()->setMouseTracking(true);
    serverView_->setFrameShape(QFrame::NoFrame);
    serverView_->setContentsMargins(0, 0, 0, 0);
    serverView_->installEventFilter(this);
    serverView_->verticalHeader()->setVisible(false);
    const int rowHeight = serverView_->fontMetrics().height() + 8;
    serverView_->verticalHeader()->setDefaultSectionSize(rowHeight);
    serverView_->verticalHeader()->setMinimumSectionSize(rowHeight);
    serverView_->horizontalHeader()->setStretchLastSection(true);
    serverView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    serverView_->horizontalHeader()->setSectionResizeMode(kServerResultColumn, QHeaderView::Stretch);
    serverView_->horizontalHeader()->setSectionsClickable(true);
    serverView_->horizontalHeader()->setHighlightSections(false);
    const int headerHeight = serverView_->horizontalHeader()->fontMetrics().height() + 8;
    serverView_->horizontalHeader()->setDefaultSectionSize(headerHeight);
    serverView_->horizontalHeader()->setMinimumSectionSize(headerHeight);
    serverView_->horizontalHeader()->setSortIndicatorShown(true);
    serverView_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    const QList<int> columnWidths = defaultServerColumnWidths();
    for (int index = 0; index < columnWidths.size(); ++index) {
        serverView_->horizontalHeader()->resizeSection(index, columnWidths.at(index));
    }

    subscriptionTabBar_ = new QTabBar(this);
    subscriptionTabBar_->setObjectName(QStringLiteral("subscriptionTabBar"));
    AppTheme::applyCompactFont(subscriptionTabBar_);
    QFont subscriptionTabFont = subscriptionTabBar_->font();
    subscriptionTabFont.setBold(true);
    subscriptionTabBar_->setFont(subscriptionTabFont);
    subscriptionTabBar_->setContextMenuPolicy(Qt::CustomContextMenu);
    subscriptionTabBar_->setDocumentMode(true);
    subscriptionTabBar_->setDrawBase(false);
    subscriptionTabBar_->setExpanding(false);
    subscriptionTabBar_->setAutoHide(false);
    subscriptionTabBar_->setUsesScrollButtons(true);
    updateServerReorderAvailability();

    // Loading overlay on server table for subscription updates.
    loadingOverlay_ = new QWidget(serverView_->viewport());
    loadingOverlay_->setObjectName(QStringLiteral("loadingOverlay"));
    auto* loadingLabel = new QLabel(tr("Updating subscriptions..."), loadingOverlay_);
    loadingLabel->setAlignment(Qt::AlignCenter);
    auto* loadingLayout = new QVBoxLayout(loadingOverlay_);
    loadingLayout->addWidget(loadingLabel);
    loadingOverlay_->hide();
    serverView_->viewport()->installEventFilter(this);

    auto* subscriptionTabBarContainer = new QWidget(this);
    subscriptionTabBarContainer->setObjectName(QStringLiteral("subscriptionTabBarContainer"));
    auto* subscriptionTabBarLayout = new QHBoxLayout(subscriptionTabBarContainer);
    subscriptionTabBarLayout->setContentsMargins(0, 0, 0, 0);
    subscriptionTabBarLayout->setSpacing(6);
    subscriptionTabBarLayout->addWidget(subscriptionTabBar_, 1, Qt::AlignBottom);

    serverFilterEdit_ = new QLineEdit(this);
    serverFilterEdit_->setObjectName(QStringLiteral("serverFilterEdit"));
    AppTheme::applyCompactFont(serverFilterEdit_);
    serverFilterEdit_->setClearButtonEnabled(true);
    serverFilterEdit_->setPlaceholderText(tr("Filter servers"));
    configureContentSizedLineEdit(serverFilterEdit_, HeaderFilterMinimumCharacters);

    subscriptionTabBarLayout->addWidget(serverFilterEdit_, 0, Qt::AlignVCenter);

    qrPanel_ = new QWidget(this);
    qrPanel_->setObjectName(QStringLiteral("qrPanel"));
    qrPanel_->setMinimumWidth(260);
    qrPanel_->installEventFilter(this);
    auto* qrLayout = new QVBoxLayout(qrPanel_);
    qrLayout->setContentsMargins(0, 0, 0, 0);
    qrLayout->setSpacing(0);

    shareContentPanel_ = new QWidget(qrPanel_);
    shareContentPanel_->setObjectName(QStringLiteral("shareContentPanel"));
    shareContentPanel_->setMinimumWidth(260);
    shareContentPanel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto* shareContentLayout = new QVBoxLayout(shareContentPanel_);
    shareContentLayout->setContentsMargins(0, 0, 0, 0);
    shareContentLayout->setSpacing(0);

    qrPlaceholder_ = new QLabel(tr("QR preview placeholder"), shareContentPanel_);
    qrPlaceholder_->setObjectName(QStringLiteral("qrPlaceholder"));
    qrPlaceholder_->setAlignment(Qt::AlignCenter);
    qrPlaceholder_->setMargin(QrPreviewPadding);
    qrPlaceholder_->setMinimumHeight(0);
    qrPlaceholder_->setMinimumWidth(260);
    qrPlaceholder_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    qrPlaceholder_->installEventFilter(this);
    shareContentLayout->addWidget(qrPlaceholder_, 1);

    shareLinkLabel_ = new ShareLinkTextEdit(shareContentPanel_);
    shareLinkLabel_->setObjectName(QStringLiteral("shareLinkLabel"));
    shareLinkLabel_->setMinimumWidth(260);
    shareLinkLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    shareLinkLabel_->installEventFilter(this);
    AppTheme::applyCompactFont(shareLinkLabel_);
    shareContentLayout->addWidget(shareLinkLabel_, 0);
    qrLayout->addWidget(shareContentPanel_, 1);

    auto* serverPanel = new QWidget(this);
    serverPanel->setMinimumHeight(0);
    auto* serverPanelLayout = new QVBoxLayout(serverPanel);
    serverPanelLayout->setContentsMargins(0, 0, 0, 0);
    serverPanelLayout->setSpacing(0);

    auto* serverHeaderRow = new QWidget(serverPanel);
    serverHeaderRow->setObjectName(QStringLiteral("serverHeaderRow"));
    auto* serverHeaderLayout = new QHBoxLayout(serverHeaderRow);
    serverHeaderLayout->setContentsMargins(0, 0, 2, 0);
    serverHeaderLayout->setSpacing(0);
    serverHeaderLayout->addWidget(subscriptionTabBarContainer);
    serverPanelLayout->addWidget(serverHeaderRow);
    serverPanelLayout->addWidget(serverView_, 1);

    topSplitter_ = new QSplitter(Qt::Horizontal, this);
    topSplitter_->setObjectName(QStringLiteral("topSplitter"));
    topSplitter_->setChildrenCollapsible(false);
    topSplitter_->addWidget(serverPanel);
    topSplitter_->addWidget(qrPanel_);
    topSplitter_->setStretchFactor(0, 4);
    topSplitter_->setStretchFactor(1, 1);
    topSplitter_->setCollapsible(0, false);
    topSplitter_->setCollapsible(1, false);
    qrPanel_->setMinimumHeight(0);
    qrPanel_->setVisible(qrPreviewVisible_);

    auto* logPanel = new QWidget(this);
    logPanel->setObjectName(QStringLiteral("logPanel"));
    logPanel->setMinimumHeight(0);
    auto* logPanelLayout = new QVBoxLayout(logPanel);
    logPanelLayout->setContentsMargins(0, 0, 0, 0);
    logPanelLayout->setSpacing(0);

    auto* logHeaderRow = new QWidget(logPanel);
    logHeaderRow->setObjectName(QStringLiteral("logHeaderRow"));
    auto* logHeaderLayout = new QHBoxLayout(logHeaderRow);
    logHeaderLayout->setContentsMargins(10, 7, 2, 3);
    logHeaderLayout->setSpacing(6);

    auto* logTitleLabel = new QLabel(tr("Information"), logHeaderRow);
    logTitleLabel->setObjectName(QStringLiteral("logTitleLabel"));
    AppTheme::applyCompactFont(logTitleLabel);
    logTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    logHeaderLayout->addWidget(logTitleLabel);
    logHeaderLayout->addStretch(1);

    logStickToBottomButton_ = new QToolButton(logHeaderRow);
    logStickToBottomButton_->setObjectName(QStringLiteral("logStickToBottomButton"));
    AppTheme::applyCompactFont(logStickToBottomButton_);
    logStickToBottomButton_->setCheckable(true);
    logStickToBottomButton_->setToolTip(tr("Stick to bottom"));
    logStickToBottomButton_->setText(QString(QChar(0x2193)));
    logStickToBottomButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    logHeaderLayout->addWidget(logStickToBottomButton_);

    logFilterEdit_ = new QLineEdit(logHeaderRow);
    logFilterEdit_->setObjectName(QStringLiteral("logFilterEdit"));
    AppTheme::applyCompactFont(logFilterEdit_);
    logFilterEdit_->setClearButtonEnabled(true);
    logFilterEdit_->setPlaceholderText(tr("Filter logs"));
    configureContentSizedLineEdit(logFilterEdit_, HeaderFilterMinimumCharacters);
    const int logFilterHeight = logFilterEdit_->sizeHint().height();
    logStickToBottomButton_->setFixedSize(logFilterHeight, logFilterHeight);
    logHeaderLayout->addWidget(logFilterEdit_);

    logPanelLayout->addWidget(logHeaderRow);

    logModel_ = new LogListModel(this);
    logFilterModel_ = new LogFilterProxyModel(this);
    logFilterModel_->setSourceModel(logModel_);
    logItemDelegate_ = new LogItemDelegate(this);

    logView_ = new QListView(logPanel);
    logView_->setObjectName(QStringLiteral("logView"));
    logView_->setMinimumHeight(0);
    logView_->setContextMenuPolicy(Qt::CustomContextMenu);
    logView_->setWordWrap(false);
    logView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    logView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    logView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    logView_->setMouseTracking(true);
    logView_->setModel(logFilterModel_);
    logView_->setItemDelegate(logItemDelegate_);
    logView_->viewport()->installEventFilter(this);
    logView_->viewport()->setMouseTracking(true);
    logView_->installEventFilter(this);
    logPanelLayout->addWidget(logView_, 1);

    rootSplitter_ = new QSplitter(Qt::Vertical, this);
    rootSplitter_->setObjectName(QStringLiteral("rootSplitter"));
    rootSplitter_->setContentsMargins(0, 4, 0, 0);
    rootSplitter_->setChildrenCollapsible(false);
    rootSplitter_->addWidget(topSplitter_);
    rootSplitter_->addWidget(logPanel);
    rootSplitter_->setStretchFactor(0, 4);
    rootSplitter_->setStretchFactor(1, 1);
    rootSplitter_->setCollapsible(0, false);
    rootSplitter_->setCollapsible(1, false);

    setCentralWidget(rootSplitter_);
}

void MainWindow::setupDiagnosticsPanel()
{
    currentServerStatusLabel_ = new QLabel(this);
    routingStatusLabel_ = new QLabel(this);
    coreStatusLabel_ = new QLabel(this);
    proxyStatusLabel_ = new QLabel(this);
    autoRunStatusLabel_ = new QLabel(this);
    statisticsStatusLabel_ = new QLabel(this);
    transientStatusLabel_ = new QLabel(this);

    currentServerStatusLabel_->setObjectName(QStringLiteral("currentServerStatusLabel"));
    routingStatusLabel_->setObjectName(QStringLiteral("routingStatusLabel"));
    coreStatusLabel_->setObjectName(QStringLiteral("coreStatusLabel"));
    proxyStatusLabel_->setObjectName(QStringLiteral("proxyStatusLabel"));
    autoRunStatusLabel_->setObjectName(QStringLiteral("autoRunStatusLabel"));
    statisticsStatusLabel_->setObjectName(QStringLiteral("statisticsStatusLabel"));
    transientStatusLabel_->setObjectName(QStringLiteral("transientStatusLabel"));

    AppTheme::applyCompactFont({
        statusBar(),
        currentServerStatusLabel_,
        routingStatusLabel_,
        coreStatusLabel_,
        proxyStatusLabel_,
        autoRunStatusLabel_,
        statisticsStatusLabel_,
        transientStatusLabel_});

    logContextMenu_ = new QMenu(this);
    logContextMenu_->setObjectName(QStringLiteral("logContextMenu"));
    selectAllLogAction_ = logContextMenu_->addAction(tr("Select All"));
    selectAllLogAction_->setObjectName(QStringLiteral("selectAllLogAction"));
    copySelectedLogAction_ = logContextMenu_->addAction(tr("Copy Selected"));
    copySelectedLogAction_->setObjectName(QStringLiteral("copySelectedLogAction"));
    copyAllLogAction_ = logContextMenu_->addAction(tr("Copy All"));
    copyAllLogAction_->setObjectName(QStringLiteral("copyAllLogAction"));
    logContextMenu_->addSeparator();
    clearLogAction_ = logContextMenu_->addAction(tr("Clear"));
    clearLogAction_->setObjectName(QStringLiteral("clearLogAction"));

    proxyStatusLabel_->setCursor(Qt::PointingHandCursor);
    proxyStatusLabel_->setToolTip(tr("Click to change system proxy mode."));
    proxyStatusLabel_->installEventFilter(this);
    routingStatusLabel_->setCursor(Qt::PointingHandCursor);
    routingStatusLabel_->setToolTip(tr("Click to open settings."));
    routingStatusLabel_->installEventFilter(this);
    transientStatusLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    statusBar()->addPermanentWidget(currentServerStatusLabel_);
    statusBar()->addPermanentWidget(routingStatusLabel_);
    statusBar()->addPermanentWidget(transientStatusLabel_, 1);
    coreStatusLabel_->hide();
    proxyStatusLabel_->hide();
    autoRunStatusLabel_->hide();
    statisticsStatusLabel_->hide();

    updateStatusIndicators();
    updateActionState();
    updateLogContextActions();
}

void MainWindow::setupConnections()
{
    connect(addServerAction_, &QAction::triggered, this, &MainWindow::addServerRequested);
    connect(addCustomServerAction_, &QAction::triggered, this, &MainWindow::addCustomServerRequested);
    connect(editServerAction_, &QAction::triggered, this, [this]() {
        const QString indexId = selectedServerId();
        if (!indexId.isEmpty()) {
            emit editServerRequested(indexId);
        }
    });
    connect(duplicateServerAction_, &QAction::triggered, this, [this]() {
        const QString indexId = selectedServerId();
        if (!indexId.isEmpty()) {
            emit duplicateServerRequested(indexId);
        }
    });
    connect(exportClientConfigAction_, &QAction::triggered, this, [this]() {
        const QString indexId = selectedServerId();
        if (!indexId.isEmpty()) {
            emit exportClientConfigRequested(indexId);
        }
    });
    connect(exportServerConfigAction_, &QAction::triggered, this, [this]() {
        const QString indexId = selectedServerId();
        if (!indexId.isEmpty()) {
            emit exportServerConfigRequested(indexId);
        }
    });
    connect(copyShareLinkAction_, &QAction::triggered, this, [this]() {
        const QStringList shareLinks = buildShareLinks(selectedServers());
        if (shareLinks.isEmpty()) {
            showTransientStatus(tr("Share link unavailable for the selected server."), 4000);
            return;
        }

        if (QApplication::clipboard() != nullptr) {
            QApplication::clipboard()->setText(shareLinks.join(QChar('\n')));
        }
        const QString message = tr("Copied %1 share link(s) to the clipboard.").arg(shareLinks.size());
        appendLog(message);
        showTransientStatus(message, 3000);
    });
    connect(copySubscriptionContentAction_, &QAction::triggered, this, [this]() {
        const QStringList shareLinks = buildShareLinks(selectedServers());
        if (shareLinks.isEmpty()) {
            showTransientStatus(tr("Subscription content unavailable for the selected server."), 4000);
            return;
        }

        if (QApplication::clipboard() != nullptr) {
            QApplication::clipboard()->setText(QString::fromLatin1(shareLinks.join(QChar('\n')).toUtf8().toBase64()));
        }
        const QString message =
            tr("Copied subscription content for %1 server(s) to the clipboard.").arg(shareLinks.size());
        appendLog(message);
        showTransientStatus(message, 3000);
    });
    connect(copySubscriptionUrlAction_, &QAction::triggered, this, &MainWindow::copyCurrentSubscriptionUrlToClipboard);
    connect(openCustomConfigAction_, &QAction::triggered, this, [this]() {
        const QString indexId = selectedServerId();
        if (!indexId.isEmpty()) {
            emit openCustomConfigRequested(indexId);
        }
    });
    connect(importClipboardAction_, &QAction::triggered, this, &MainWindow::importFromClipboardRequested);
    connect(importClientConfigAction_, &QAction::triggered, this, &MainWindow::importClientConfigRequested);
    connect(importServerConfigAction_, &QAction::triggered, this, &MainWindow::importServerConfigRequested);
    connect(updateSubscriptionsAction_, &QAction::triggered, this, &MainWindow::updateSubscriptionsRequested);
    connect(testMeAction_, &QAction::triggered, this, &MainWindow::testMeRequested);
    connect(updateV2RayCoreAction_, &QAction::triggered, this, [this]() {
        emit updateCoreRequested(static_cast<int>(CoreType::V2Fly));
    });
    connect(updateXrayCoreAction_, &QAction::triggered, this, [this]() {
        emit updateCoreRequested(static_cast<int>(CoreType::Xray));
    });
    connect(updateSingBoxCoreAction_, &QAction::triggered, this, [this]() {
        emit updateCoreRequested(static_cast<int>(CoreType::SingBox));
    });
    connect(updateClashCoreAction_, &QAction::triggered, this, [this]() {
        emit updateCoreRequested(static_cast<int>(CoreType::Clash));
    });
    connect(updateClashMetaCoreAction_, &QAction::triggered, this, [this]() {
        emit updateCoreRequested(static_cast<int>(CoreType::ClashMeta));
    });
    connect(updateGeoResourcesAction_, &QAction::triggered, this, &MainWindow::updateGeoResourcesRequested);
    connect(removeServerAction_, &QAction::triggered, this, [this]() {
        const QStringList indexIds = selectedServerIds();
        if (indexIds.isEmpty()) {
            return;
        }

        const QString message = indexIds.size() == 1
            ? tr("Remove the selected server?")
            : tr("Remove the selected %1 servers?").arg(indexIds.size());
        if (QMessageBox::question(
                this,
                tr("Remove Servers"),
                message,
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }

        emit removeServersRequested(indexIds);
    });
    connect(moveServerTopAction_, &QAction::triggered, this, [this]() {
        emit moveServersRequested(selectedServerIds(), static_cast<int>(ServerMoveOperation::Top));
    });
    connect(moveServerUpAction_, &QAction::triggered, this, [this]() {
        emit moveServersRequested(selectedServerIds(), static_cast<int>(ServerMoveOperation::Up));
    });
    connect(moveServerDownAction_, &QAction::triggered, this, [this]() {
        emit moveServersRequested(selectedServerIds(), static_cast<int>(ServerMoveOperation::Down));
    });
    connect(moveServerBottomAction_, &QAction::triggered, this, [this]() {
        emit moveServersRequested(selectedServerIds(), static_cast<int>(ServerMoveOperation::Bottom));
    });
    connect(testAction_, &QAction::triggered, this, [this]() {
        emit testServersRequested(selectedServerIds());
    });
    connect(setDefaultServerAction_, &QAction::triggered, this, [this]() {
        const QString indexId = selectedServerId();
        if (!indexId.isEmpty()) {
            lastSelectedServerId_ = indexId;
            emit setDefaultServerRequested(indexId);
            QTimer::singleShot(0, this, [this, indexId]() {
                if (lastSelectedServerId_ == indexId) {
                    updateSelectionForVisibleRows();
                }
            });
        }
    });
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
    if (routingModeCombo_ != nullptr) {
        connect(routingModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (routingModeCombo_ == nullptr || index < 0) {
                return;
            }

            emit routingModeSelected(routingModeCombo_->itemData(index).toInt());
        });
    }
    connect(toggleAutoRunAction_, &QAction::triggered, this, &MainWindow::toggleAutoRunRequested);
    connect(reloadConfigAction_, &QAction::triggered, this, &MainWindow::reloadConfigRequested);
    connect(restoreBackupAction_, &QAction::triggered, this, &MainWindow::restoreBackupRequested);
    connect(globalHotkeySettingsAction_, &QAction::triggered, this, &MainWindow::globalHotkeySettingsRequested);
    connect(dnsSettingsAction_, &QAction::triggered, this, &MainWindow::dnsSettingsRequested);
    connect(settingsAction_, &QAction::triggered, this, &MainWindow::settingsRequested);
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
    connect(openV2RayReleasePageAction_, &QAction::triggered, this, &MainWindow::openV2RayReleasePageRequested);
    connect(openXrayReleasePageAction_, &QAction::triggered, this, &MainWindow::openXrayReleasePageRequested);
    connect(
        openSingBoxReleasePageAction_,
        &QAction::triggered,
        this,
        &MainWindow::openSingBoxReleasePageRequested);
    connect(openGeoReleasePageAction_, &QAction::triggered, this, &MainWindow::openGeoReleasePageRequested);
    connect(coreToggleAction_, &QAction::triggered, this, [this]() {
        if (coreTransitionPending_) {
            return;
        }
        if (coreRunning_) {
            emit stopCoreRequested();
            return;
        }

        emit startCoreRequested();
    });
    connect(proxyToggleAction_, &QAction::triggered, this, [this]() {
        if (systemProxyApplied_) {
            emit disableSystemProxyRequested();
            return;
        }

        emit enableSystemProxyRequested();
    });
    connect(clearStatisticsAction_, &QAction::triggered, this, &MainWindow::clearStatisticsRequested);
    connect(toggleQrPanelAction_, &QAction::triggered, this, [this](bool checked) {
        if (qrPreviewVisible_ && !checked) {
            serverQrSplitPercent_ = captureSplitPercent(topSplitter_, serverQrSplitPercent_);
        }
        qrPreviewVisible_ = checked;
        if (qrPanel_ != nullptr) {
            qrPanel_->setVisible(qrPreviewVisible_);
        }
        if (qrPreviewVisible_) {
            QTimer::singleShot(0, this, [this]() {
                applySplitPercent(topSplitter_, serverQrSplitPercent_);
            });
        }
        updateQrPanelActionText();
    });
    connect(serverFilterEdit_, &QLineEdit::textChanged, this, &MainWindow::onServerFilterTextChanged);
    connect(serverView_->horizontalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::toggleServerSorting);
    connect(logFilterEdit_, &QLineEdit::textChanged, this, [this](const QString&) {
        applyLogFilter();
    });
    connect(logStickToBottomButton_, &QToolButton::toggled, this, [this](bool checked) {
        logStickToBottomEnabled_ = checked;
        if (logView_ != nullptr) {
            logView_->setVerticalScrollBarPolicy(
                checked ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
        }
        if (checked && logView_ != nullptr) {
            logView_->scrollToBottom();
        }
    });
    connect(logView_, &QListView::customContextMenuRequested, this, &MainWindow::showLogContextMenu);
    connect(logView_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        updateLogContextActions();
    });
    connect(selectAllLogAction_, &QAction::triggered, this, [this]() {
        if (logView_ != nullptr && logFilterModel_ != nullptr && logView_->selectionModel() != nullptr) {
            const QModelIndex topLeft = logFilterModel_->index(0, 0);
            const QModelIndex bottomRight = logFilterModel_->index(logFilterModel_->rowCount() - 1, 0);
            if (topLeft.isValid() && bottomRight.isValid()) {
                logView_->selectionModel()->select(
                    QItemSelection(topLeft, bottomRight),
                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
        }
    });
    connect(copySelectedLogAction_, &QAction::triggered, this, [this]() {
        if (logView_ == nullptr
            || QApplication::clipboard() == nullptr
            || logView_->selectionModel() == nullptr
            || logFilterModel_ == nullptr) {
            return;
        }

        QStringList lines;
        const QModelIndexList proxyIndexes = logView_->selectionModel()->selectedRows();
        for (const QModelIndex& proxyIndex : proxyIndexes) {
            lines.append(proxyIndex.data(Qt::DisplayRole).toString());
        }
        if (!lines.isEmpty()) {
            QApplication::clipboard()->setText(lines.join(QChar('\n')));
        }
    });
    connect(copyAllLogAction_, &QAction::triggered, this, [this]() {
        if (logView_ == nullptr || QApplication::clipboard() == nullptr || logFilterModel_ == nullptr) {
            return;
        }

        QStringList lines;
        for (int row = 0; row < logFilterModel_->rowCount(); ++row) {
            lines.append(logFilterModel_->index(row, 0).data(Qt::DisplayRole).toString());
        }
        if (!lines.isEmpty()) {
            QApplication::clipboard()->setText(lines.join(QChar('\n')));
        }
    });
    connect(clearLogAction_, &QAction::triggered, this, [this]() {
        if (logModel_ != nullptr) {
            logModel_->clear();
        }
        applyLogFilter();
    });
    connect(subscriptionTabBar_, &QTabBar::currentChanged, this, [this](int) {
        updateSubscriptionFilter();
    });
    connect(subscriptionTabBar_, &QTabBar::customContextMenuRequested, this, &MainWindow::showSubscriptionTabContextMenu);
    serverView_->setRowsMoveHandler([this](const QList<int>& movedRows, int targetRow) {
        const QStringList reorderedIds = buildReorderedServerIds(movedRows, targetRow);
        if (!reorderedIds.isEmpty()) {
            emit reorderServersRequested(reorderedIds);
        }
    });

    if (topSplitter_ != nullptr) {
        connect(topSplitter_, &QSplitter::splitterMoved, this, [this](int, int) {
            serverQrSplitPercent_ = captureSplitPercent(topSplitter_, serverQrSplitPercent_);
        });
    }

    if (rootSplitter_ != nullptr) {
        connect(rootSplitter_, &QSplitter::splitterMoved, this, [this](int, int) {
            serverLogSplitPercent_ = captureSplitPercent(rootSplitter_, serverLogSplitPercent_);
        });
    }

    if (serverView_ != nullptr && serverView_->selectionModel() != nullptr) {
        connect(
            serverView_->selectionModel(),
            &QItemSelectionModel::currentRowChanged,
            this,
            [this](const QModelIndex&, const QModelIndex&) {
                onServerSelectionChanged();
            });

        connect(serverView_, &QTableView::doubleClicked, this, [this](const QModelIndex&) {
            const QString indexId = selectedServerId();
            if (!indexId.isEmpty()) {
                lastSelectedServerId_ = indexId;
                emit setDefaultServerRequested(indexId);
                QTimer::singleShot(0, this, [this, indexId]() {
                    if (lastSelectedServerId_ == indexId) {
                        updateSelectionForVisibleRows();
                    }
                });
            }
        });
    }

    if (serverView_ != nullptr) {
        serverView_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(serverView_, &QWidget::customContextMenuRequested, this, &MainWindow::showServerContextMenu);

        copyShareLinkAction_->setShortcut(QKeySequence::Copy);
        copyShareLinkAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        serverView_->addAction(copyShareLinkAction_);

        importClipboardAction_->setShortcut(QKeySequence::Paste);
        importClipboardAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        serverView_->addAction(importClipboardAction_);

        setDefaultServerAction_->setShortcut(QKeySequence(Qt::Key_Return));
        setDefaultServerAction_->setShortcuts({
            QKeySequence(Qt::Key_Return),
            QKeySequence(Qt::Key_Enter)});
        setDefaultServerAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        serverView_->addAction(setDefaultServerAction_);

        removeServerAction_->setShortcut(QKeySequence::Delete);
        removeServerAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        serverView_->addAction(removeServerAction_);

        moveServerTopAction_->setShortcut(QKeySequence(Qt::Key_T));
        moveServerTopAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        serverView_->addAction(moveServerTopAction_);

        moveServerBottomAction_->setShortcut(QKeySequence(Qt::Key_B));
        moveServerBottomAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        serverView_->addAction(moveServerBottomAction_);

        moveServerUpAction_->setShortcut(QKeySequence(Qt::Key_U));
        moveServerUpAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        serverView_->addAction(moveServerUpAction_);

        moveServerDownAction_->setShortcut(QKeySequence(Qt::Key_D));
        moveServerDownAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        serverView_->addAction(moveServerDownAction_);

        auto* selectAllServersShortcutAction = new QAction(this);
        selectAllServersShortcutAction->setObjectName(QStringLiteral("selectAllServersShortcutAction"));
        selectAllServersShortcutAction->setShortcut(QKeySequence::SelectAll);
        selectAllServersShortcutAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(selectAllServersShortcutAction, &QAction::triggered, this, [this]() {
            if (serverView_ != nullptr && serverView_->model() != nullptr && serverView_->model()->rowCount() > 0) {
                serverView_->selectAll();
            }
        });
        serverView_->addAction(selectAllServersShortcutAction);

        auto* sortServerResultShortcutAction = new QAction(this);
        sortServerResultShortcutAction->setObjectName(QStringLiteral("sortServerResultShortcutAction"));
        sortServerResultShortcutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
        sortServerResultShortcutAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(sortServerResultShortcutAction, &QAction::triggered, this, [this]() {
            toggleServerSorting(kServerResultColumn);
        });
        serverView_->addAction(sortServerResultShortcutAction);
    }

    if (serverFilterEdit_ != nullptr) {
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
        const QString tabKey = currentSubscriptionTabKey();
        emit updateCurrentSubscriptionRequested(
            tabKey.startsWith(QStringLiteral("sub:"))
                ? tabKey.mid(QStringLiteral("sub:").size())
                : QString());
    });
    addAction(updateCurrentSubscriptionShortcutAction_);

    auto* exitShortcutAction = new QAction(this);
    exitShortcutAction->setObjectName(QStringLiteral("exitShortcutAction"));
    exitShortcutAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_X));
    exitShortcutAction->setShortcutContext(Qt::WindowShortcut);
    connect(exitShortcutAction, &QAction::triggered, this, [this]() {
        setAllowClose(true);
        close();
    });
    addAction(exitShortcutAction);

    if (subscriptionTabBar_ != nullptr) {
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
}

void MainWindow::showSubscriptionTabContextMenu(const QPoint& position)
{
    if (subscriptionTabBar_ == nullptr) {
        return;
    }

    const int tabIndex = subscriptionTabBar_->tabAt(position);
    if (tabIndex < 0) {
        return;
    }

    if (subscriptionTabBar_->currentIndex() != tabIndex) {
        subscriptionTabBar_->setCurrentIndex(tabIndex);
    }

    const QString tabKey = subscriptionTabBar_->tabData(tabIndex).toString();
    const QString subscriptionId = resolveSubscriptionIdFromTabKey(tabKey);
    QMenu menu(subscriptionTabBar_);

    if (!subscriptionId.isEmpty()) {
        auto* copySubscriptionUrlAction = menu.addAction(tr("Copy Subscription Url"));
        copySubscriptionUrlAction->setEnabled(!currentSubscriptionUrl().isEmpty());
        connect(copySubscriptionUrlAction, &QAction::triggered, &menu, [this]() {
            copyCurrentSubscriptionUrlToClipboard();
        });

        menu.addSeparator();

        auto* updateAction = menu.addAction(tr("Update Subscription"));
        updateAction->setEnabled(canStartBackgroundTask());
        connect(updateAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            emit updateCurrentSubscriptionRequested(subscriptionId);
        });

        auto* updateViaProxyAction = menu.addAction(tr("Update Subscription via Proxy"));
        updateViaProxyAction->setEnabled(canStartBackgroundTask());
        connect(updateViaProxyAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            emit updateCurrentSubscriptionViaProxyRequested(subscriptionId);
        });

        auto* hideAction = menu.addAction(tr("Hide"));
        connect(hideAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            emit hideSubscriptionRequested(subscriptionId);
        });

        auto* deleteAction = menu.addAction(tr("Delete"));
        connect(deleteAction, &QAction::triggered, &menu, [this, subscriptionId]() {
            if (QMessageBox::question(
                    this,
                    tr("Delete Subscription"),
                    tr("Delete the selected subscription and all servers inside it?"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No)
                == QMessageBox::Yes) {
                emit deleteSubscriptionRequested(subscriptionId);
            }
        });

        menu.addSeparator();
    }

    auto* manageAction = menu.addAction(tr("Subscription Management"));
    connect(manageAction, &QAction::triggered, &menu, [this]() {
        emit openSettingsAtSubscriptionsTabRequested();
    });

    menu.exec(subscriptionTabBar_->mapToGlobal(position));
}

void MainWindow::showServerContextMenu(const QPoint& position)
{
    if (serverView_ == nullptr || serverView_->selectionModel() == nullptr) {
        return;
    }

    const QModelIndex clickedIndex = serverView_->indexAt(position);
    if (clickedIndex.isValid() && !serverView_->selectionModel()->isRowSelected(clickedIndex.row(), QModelIndex())) {
        serverView_->clearSelection();
        serverView_->setCurrentIndex(clickedIndex);
        serverView_->selectRow(clickedIndex.row());
    }

    updateActionState();
    const QStringList selectedIds = selectedServerIds();
    if (selectedIds.isEmpty()) {
        return;
    }

    const bool singleSelection = selectedIds.size() == 1;
    const bool ungroupedTabSelected = isUngroupedSubscriptionTabSelected();
    QMenu menu(serverView_);

    if (singleSelection) {
        menu.addAction(setDefaultServerAction_);

        if (ungroupedTabSelected) {
            menu.addAction(editServerAction_);
            menu.addAction(removeServerAction_);
            menu.addSeparator();
            menu.addAction(testAction_);
            menu.addSeparator();
            menu.addAction(addServerAction_);
        } else {
            menu.addSeparator();
            menu.addAction(testAction_);
        }
    } else {
        menu.addAction(testAction_);
        if (ungroupedTabSelected) {
            menu.addSeparator();
            menu.addAction(removeServerAction_);
        }
    }

    menu.exec(serverView_->viewport()->mapToGlobal(position));
}

void MainWindow::copyCurrentSubscriptionUrlToClipboard()
{
    const QString url = currentSubscriptionUrl();
    if (url.isEmpty()) {
        showTransientStatus(tr("Subscription URL unavailable for the current tab."), 4000);
        return;
    }

    if (QApplication::clipboard() != nullptr) {
        QApplication::clipboard()->setText(url);
    }

    appendLog(tr("Copied subscription URL to the clipboard."));
    showTransientStatus(tr("Copied subscription URL to the clipboard."), 3000);
}

void MainWindow::applyDeferredUiState()
{
    restoreServerColumnWidths(pendingColumnWidths_);
    pendingColumnWidths_.clear();

    if (qrPanel_ != nullptr && qrPreviewVisible_) {
        qrPanel_->setVisible(true);
    }
    applySplitPercent(topSplitter_, serverQrSplitPercent_);
    if (qrPanel_ != nullptr) {
        qrPanel_->setVisible(qrPreviewVisible_);
    }

    applySplitPercent(rootSplitter_, serverLogSplitPercent_);
    updateQrPreview();
    updateQrPanelActionText();
}

void MainWindow::updateRoutingModeOptions(const Config& config)
{
    if (routingModeCombo_ == nullptr) {
        return;
    }

    const QSignalBlocker blocker(routingModeCombo_);
    routingModeCombo_->clear();

    for (int index = 0; index < config.routingItems.size(); ++index) {
        routingModeCombo_->addItem(describeRoutingMode(config.routingItems.at(index), index), index);
    }

    const int selectedMode = config.routingItems.isEmpty()
        ? -1
        : qBound(0, config.routingIndex, config.routingItems.size() - 1);
    const int comboIndex = routingModeCombo_->findData(selectedMode);
    routingModeCombo_->setCurrentIndex(comboIndex >= 0 ? comboIndex : (routingModeCombo_->count() > 0 ? 0 : -1));
    updateContentSizedComboBox(routingModeCombo_, RoutingComboMinimumCharacters);
}

void MainWindow::restoreServerColumnWidths(const QMap<QString, int>& widths)
{
    if (serverView_ == nullptr || serverView_->horizontalHeader() == nullptr) {
        return;
    }

    const QMap<QString, int> effectiveWidths = hasLegacyServerColumnWidths(widths)
        ? QMap<QString, int>()
        : widths;
    const int availableWidth = serverView_->viewport() == nullptr
        ? serverView_->width()
        : serverView_->viewport()->width();
    const QList<int> defaultWidths = defaultServerColumnWidths(availableWidth);
    const QStringList keys = serverColumnKeys();
    QHeaderView* header = serverView_->horizontalHeader();
    for (int index = 0; index < keys.size() && index < header->count(); ++index) {
        if (index < defaultWidths.size() && defaultWidths.at(index) > 0) {
            header->resizeSection(index, defaultWidths.at(index));
        }
        const auto it = effectiveWidths.constFind(keys.at(index));
        if (it != effectiveWidths.constEnd() && it.value() > 0) {
            header->resizeSection(index, it.value());
        }
    }
}

QMap<QString, int> MainWindow::captureServerColumnWidths() const
{
    QMap<QString, int> widths;
    if (serverView_ == nullptr || serverView_->horizontalHeader() == nullptr) {
        return widths;
    }

    const QStringList keys = serverColumnKeys();
    const QHeaderView* header = serverView_->horizontalHeader();
    for (int index = 0; index < keys.size() && index < header->count(); ++index) {
        widths.insert(keys.at(index), header->sectionSize(index));
    }

    return widths;
}

int MainWindow::captureSplitPercent(QSplitter* splitter, int fallback) const
{
    if (splitter == nullptr) {
        return clampSplitPercent(fallback, fallback);
    }

    if (splitter == topSplitter_ && (qrPanel_ == nullptr || !qrPanel_->isVisible())) {
        return clampSplitPercent(serverQrSplitPercent_, fallback);
    }

    const QList<int> sizes = splitter->sizes();
    if (sizes.size() < 2) {
        return clampSplitPercent(fallback, fallback);
    }

    const int total = qMax(0, sizes.at(0)) + qMax(0, sizes.at(1));
    if (total <= 0) {
        return clampSplitPercent(fallback, fallback);
    }

    const int percent = static_cast<int>(qRound((sizes.at(0) * 100.0) / total));
    return clampSplitPercent(percent, fallback);
}

void MainWindow::applySplitPercent(QSplitter* splitter, int percent)
{
    if (splitter == nullptr) {
        return;
    }

    const QList<int> sizes = splitter->sizes();
    if (sizes.size() < 2) {
        return;
    }

    const int normalizedPercent = clampSplitPercent(percent, splitter == rootSplitter_
            ? DefaultServerLogSplitPercent
            : DefaultServerQrSplitPercent);
    int total = qMax(0, sizes.at(0)) + qMax(0, sizes.at(1));
    if (total <= 0) {
        total = splitter->orientation() == Qt::Horizontal
            ? splitter->contentsRect().width()
            : splitter->contentsRect().height();
    }

    if (total <= 0) {
        return;
    }

    const int firstSize = qMax(1, static_cast<int>(qRound((total * normalizedPercent) / 100.0)));
    const int secondSize = qMax(1, total - firstSize);
    splitter->setSizes({firstSize, secondSize});
}

void MainWindow::showSystemProxyMenu(const QPoint& globalPosition)
{
    if (systemProxyMenu_ == nullptr) {
        return;
    }

    systemProxyMenu_->popup(globalPosition);
}

QString MainWindow::persistedSubscriptionSelectionId() const
{
    return resolvePersistedSelectionId(currentSubscriptionTabKey());
}

bool MainWindow::confirmExit()
{
    return QMessageBox::question(
               this,
               tr("Quit v2rayq"),
               tr("Quit v2rayq now?"),
               QMessageBox::Yes | QMessageBox::No,
               QMessageBox::No)
        == QMessageBox::Yes;
}

int MainWindow::clampSplitPercent(int value, int fallback)
{
    return value < 10 || value > 90 ? fallback : value;
}

QStringList MainWindow::serverColumnKeys()
{
    return defaultServerColumnKeys();
}

QString MainWindow::describeRoutingMode(const RoutingItem& item, int index)
{
    const QString remarks = item.remarks.trimmed();
    if (!remarks.isEmpty()) {
        return remarks;
    }

    return QCoreApplication::translate("MainWindow", "Routing %1").arg(index + 1);
}

void MainWindow::updateWindowTitle()
{
    const QString coreName = currentCoreName_.trimmed().isEmpty()
        ? QStringLiteral("Unknown")
        : currentCoreName_.trimmed();
    const QString serverName = currentServerName_.trimmed().isEmpty()
        ? QStringLiteral("None")
        : currentServerName_.trimmed();
    const QString proxyState = systemProxyApplied_ ? QStringLiteral("Proxy ON") : QStringLiteral("Proxy OFF");
    const QString tunState = tunEnabled_ ? QStringLiteral("TUN ON") : QStringLiteral("TUN OFF");

    setWindowTitle(QStringLiteral("V2RAYQ [%1] [%2] [%3] [%4]")
                       .arg(coreName, serverName, proxyState, tunState));
}

void MainWindow::updateStatusIndicators()
{
    if (currentServerStatusLabel_ != nullptr) {
        currentServerStatusLabel_->setText(tr("Current: %1").arg(
            currentServerName_));
    }

    if (routingStatusLabel_ != nullptr) {
        const QString listenText = listenSummary_.isEmpty()
            ? tr("Unavailable")
            : listenSummary_;
        routingStatusLabel_->setText(tr("Listening: %1").arg(listenText));
    }

    if (coreStatusLabel_ != nullptr) {
        QString coreStateText;
        QString color;
        if (coreTransitionPending_) {
            coreStateText = coreRunning_ ? tr("Stopping") : tr("Starting");
            color = AppTheme::warningStatusColor();
        } else if (coreRunning_) {
            coreStateText = tr("Running");
            color = AppTheme::successStatusColor();
        } else {
            coreStateText = tr("Stopped");
            color = AppTheme::errorStatusColor();
        }

        coreStatusLabel_->setText(tr("Core: %1").arg(coreStateText));
        applySemanticState(
            coreStatusLabel_,
            AppTheme::semanticStatusProperty(color));
    }

    if (proxyStatusLabel_ != nullptr) {
        const bool expectedApplied = expectedSystemProxyEnabled(systemProxyMode_);
        const bool mismatch = systemProxyMode_ != SystemProxyMode::Unchanged
            && systemProxyApplied_ != expectedApplied;
        QString proxyText;
        QString color;
        switch (systemProxyMode_) {
        case SystemProxyMode::ForcedChange:
            proxyText = tr("Global");
            color = AppTheme::successStatusColor();
            break;
        case SystemProxyMode::Unchanged:
            proxyText = tr("Unchanged");
            color = QStringLiteral("#4d4d4d");
            break;
        case SystemProxyMode::ForcedClear:
        default:
            proxyText = tr("Clear");
            color = AppTheme::errorStatusColor();
            break;
        }

        if (mismatch) {
            proxyText += tr(" (not applied)");
            color = AppTheme::warningStatusColor();
        }

        proxyStatusLabel_->setText(tr("Proxy: %1").arg(proxyText));
        applySemanticState(proxyStatusLabel_, AppTheme::semanticStatusProperty(color));
    }

    if (autoRunStatusLabel_ != nullptr) {
        autoRunStatusLabel_->setText(tr("Auto Run: %1").arg(
            autoRunEnabled_ ? tr("Enabled") : tr("Disabled")));
        applySemanticState(
            autoRunStatusLabel_,
            AppTheme::semanticStatusProperty(
                autoRunEnabled_ ? AppTheme::successStatusColor() : AppTheme::errorStatusColor()));
    }

    if (statisticsStatusLabel_ != nullptr) {
        QString summary = tr("Stats: Off");
        QString color = AppTheme::errorStatusColor();
        if (statisticsState_.enabled) {
            if (!statisticsState_.running) {
                summary = tr("Stats: Idle");
                color = AppTheme::warningStatusColor();
            } else if (statisticsState_.externallyManaged) {
                summary = tr("Stats: External");
                color = QStringLiteral("#4d4d4d");
            } else if (statisticsState_.runtimeConfigReady
                && statisticsState_.statePort > 0
                && statisticsState_.pollingAvailable) {
                summary = tr("Stats API: 127.0.0.1:%1").arg(statisticsState_.statePort);
                color = AppTheme::successStatusColor();
            } else if (statisticsState_.runtimeConfigReady && statisticsState_.statePort > 0) {
                summary = tr("Stats API: 127.0.0.1:%1 (waiting)").arg(statisticsState_.statePort);
                color = AppTheme::warningStatusColor();
            } else {
                summary = tr("Stats: Enabled");
                color = AppTheme::warningStatusColor();
            }
        }

        statisticsStatusLabel_->setText(summary);
        applySemanticState(statisticsStatusLabel_, AppTheme::semanticStatusProperty(color));
    }

    if (transientStatusLabel_ != nullptr) {
        const QString statusText = backgroundTaskRunning_ && !backgroundTaskDescription_.isEmpty()
            ? tr("Task: %1").arg(backgroundTaskDescription_)
            : tr("Ready");
        transientStatusLabel_->setText(statusText);
    }

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
    if (watched == routingStatusLabel_ && event != nullptr) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                emit settingsRequested();
                return true;
            }
        }

        if (event->type() == QEvent::ContextMenu) {
            emit settingsRequested();
            return true;
        }
    }

    if (watched == proxyStatusLabel_ && event != nullptr) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton || mouseEvent->button() == Qt::RightButton) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                showSystemProxyMenu(mouseEvent->globalPosition().toPoint());
#else
                showSystemProxyMenu(mouseEvent->globalPos());
#endif
                return true;
            }
        }

        if (event->type() == QEvent::ContextMenu) {
            auto* contextEvent = static_cast<QContextMenuEvent*>(event);
            showSystemProxyMenu(contextEvent->globalPos());
            return true;
        }
    }

    if (serverView_ != nullptr
        && serverView_->viewport() != nullptr
        && watched == serverView_->viewport()
        && event != nullptr
        && event->type() == QEvent::Resize) {
        if (loadingOverlay_ != nullptr && loadingOverlay_->isVisible()) {
            loadingOverlay_->setGeometry(serverView_->viewport()->rect());
        }
    }

    if ((watched == serverView_ || watched == qrPanel_) && event != nullptr && event->type() == QEvent::Resize) {
        updateQrPreview();
    }

    if (watched == qrPlaceholder_ && event != nullptr && event->type() == QEvent::Resize) {
        updateQrPreview();
    }

    if (watched == shareLinkLabel_ && event != nullptr && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && QApplication::clipboard() != nullptr) {
            const QTextCursor cursor = shareLinkLabel_->cursorForPosition(mouseEvent->pos());
            const QTextBlock block = cursor.block();
            const QString shareUrl = block.text().trimmed();
            if (!shareUrl.isEmpty()) {
                QApplication::clipboard()->setText(shareUrl);
                showTransientStatus(tr("Copied share URL to the clipboard."), 2000);
                return true;
            }
        }
    }

    if (logView_ != nullptr && watched == logView_ && event != nullptr
        && event->type() == QEvent::ShortcutOverride) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->matches(QKeySequence::Copy) && logView_->selectionModel() != nullptr
            && !logView_->selectionModel()->selectedRows().isEmpty()) {
            event->accept();
        }
    }

    if (logView_ != nullptr && watched == logView_ && event != nullptr
        && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->matches(QKeySequence::Copy) && logView_->selectionModel() != nullptr
            && !logView_->selectionModel()->selectedRows().isEmpty()) {
            QStringList lines;
            const QModelIndexList proxyIndexes = logView_->selectionModel()->selectedRows();
            for (const QModelIndex& proxyIndex : proxyIndexes) {
                lines.append(proxyIndex.data(Qt::DisplayRole).toString());
            }
            if (!lines.isEmpty()) {
                QApplication::clipboard()->setText(lines.join(QChar('\n')));
            }
            return true;
        }
    }

    if (logView_ != nullptr && logModel_ != nullptr
        && watched == logView_->viewport() && event != nullptr && event->type() == QEvent::Resize) {
        const int width = logView_->viewport()->width() - 2 * LogItemDelegate::HorizontalPadding;
        const QFont font = logView_->font();
        QMetaObject::invokeMethod(this, [this, font, width]() {
            logModel_->setWrapConfiguration(font, width);
        }, Qt::QueuedConnection);
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::applyFrameAdjustedWindowMetrics()
{
    if (frameAdjustedWindowMetricsApplied_) {
        return;
    }

    const QSize clientSize = size();
    const QSize frameSize = frameGeometry().size();
    if (!clientSize.isValid() || !frameSize.isValid()) {
        return;
    }

    const QSize frameDelta(
        qMax(0, frameSize.width() - clientSize.width()),
        qMax(0, frameSize.height() - clientSize.height()));
    const QSize targetClientSize(
        qMax(1, DefaultMainWindowWidth - frameDelta.width()),
        qMax(1, DefaultMainWindowHeight - frameDelta.height()));

    frameAdjustedWindowMetricsApplied_ = true;
    setMinimumSize(targetClientSize);

    if (clientSize == QSize(DefaultMainWindowWidth, DefaultMainWindowHeight)) {
        resize(targetClientSize);
    }
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    applyFrameAdjustedWindowMetrics();
    QTimer::singleShot(0, this, [this]() {
        updateRoutingModeOptions(config_);
        updateQrPreview();
    });
    if (uiStateRestorePending_) {
        uiStateRestorePending_ = false;
        initialServerColumnLayoutPending_ = false;
        QTimer::singleShot(0, this, [this]() {
            applyDeferredUiState();
        });
        return;
    }

    if (!initialServerColumnLayoutPending_) {
        return;
    }

    initialServerColumnLayoutPending_ = false;
    QTimer::singleShot(0, this, [this]() {
        restoreServerColumnWidths({});
    });
}

QString MainWindow::selectedServerId() const
{
    const VmessItem* item = selectedServer();
    return item == nullptr ? QString() : item->indexId;
}

const VmessItem* MainWindow::selectedServer() const
{
    const QList<const VmessItem*> items = selectedServers();
    return items.isEmpty() ? nullptr : items.constFirst();
}

QList<const VmessItem*> MainWindow::selectedServers() const
{
    QList<const VmessItem*> items;
    if (serverView_ == nullptr
        || serverView_->selectionModel() == nullptr
        || serverFilterModel_ == nullptr
        || serverModel_ == nullptr) {
        return items;
    }

    QSet<int> seenRows;
    const QModelIndexList rows = serverView_->selectionModel()->selectedRows();
    for (const QModelIndex& proxyRow : rows) {
        const QModelIndex sourceRow = serverFilterModel_->mapToSource(proxyRow);
        if (!sourceRow.isValid() || seenRows.contains(sourceRow.row())) {
            continue;
        }

        seenRows.insert(sourceRow.row());
        const VmessItem* item = serverModel_->itemAt(sourceRow.row());
        if (item != nullptr) {
            items.append(item);
        }
    }

    return items;
}

QStringList MainWindow::selectedServerIds() const
{
    QStringList ids;
    const QList<const VmessItem*> items = selectedServers();
    for (const VmessItem* item : items) {
        if (item != nullptr && !item->indexId.isEmpty()) {
            ids.append(item->indexId);
        }
    }

    ids.removeDuplicates();
    return ids;
}
