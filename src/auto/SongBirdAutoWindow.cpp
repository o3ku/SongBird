#include "auto/SongBirdAutoWindow.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCloseEvent>
#include <QEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QTextEdit>
#include <QFontMetrics>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <limits>

#include "auto/AutoCountrySelection.h"
#include "auto/SongBirdAutoCoordinator.h"
#include "app/StartupAdminElevation.h"
#include "common/AppPlatform.h"
#include "common/DialogUtils.h"
#include "ui/dialogs/RoutingSettingsPageWidget.h"

namespace {

constexpr int kSubscriptionAutosaveDelayMs = 1200;
constexpr int kRunButtonAnimationIntervalMs = 350;
constexpr int kMaxBufferedLogLines = 1000;
const QString kAutoStartArgument = QStringLiteral("--auto-start");
const QString kStrategyFirstAvailable = QStringLiteral("firstAvailable");
const QString kStrategyLowestLatency = QStringLiteral("lowestLatency");

QString latencyText(qint64 latencyMs)
{
    return latencyMs >= 0 ? QStringLiteral("%1 ms").arg(latencyMs) : QStringLiteral("-");
}

QString compactLatencyText(qint64 latencyMs)
{
    return latencyMs >= 0 ? QStringLiteral("%1ms").arg(latencyMs) : QStringLiteral("-");
}

QString availabilityText(const AutoNodeEvaluation& evaluation)
{
    if (!evaluation.tested) {
        return QStringLiteral("Pending");
    }
    if (evaluation.available) {
        return QStringLiteral("OK");
    }
    return evaluation.error.trimmed().isEmpty() ? QStringLiteral("Failed") : evaluation.error.trimmed();
}

int evaluationSortRank(const AutoNodeEvaluation& evaluation)
{
    if (evaluation.tested && evaluation.available && evaluation.latencyMs >= 0) {
        return 0;
    }
    if (evaluation.tested && evaluation.available) {
        return 1;
    }
    if (!evaluation.tested) {
        return 2;
    }
    return 3;
}

bool evaluationLessThan(const AutoNodeEvaluation& left, const AutoNodeEvaluation& right)
{
    const int leftRank = evaluationSortRank(left);
    const int rightRank = evaluationSortRank(right);
    if (leftRank != rightRank) {
        return leftRank < rightRank;
    }
    if (leftRank == 0 && left.latencyMs != right.latencyMs) {
        return left.latencyMs < right.latencyMs;
    }
    return left.displayName.localeAwareCompare(right.displayName) < 0;
}

class NodeTableItem final : public QTableWidgetItem
{
public:
    explicit NodeTableItem(const QString& text)
        : QTableWidgetItem(text)
        , textSortValue_(text.toCaseFolded())
    {
    }

    NodeTableItem(const QString& text, qlonglong sortValue)
        : QTableWidgetItem(text)
        , sortValue_(sortValue)
        , numericSort_(true)
    {
    }

    bool operator<(const QTableWidgetItem& other) const override
    {
        const auto* right = dynamic_cast<const NodeTableItem*>(&other);
        if (numericSort_ && right != nullptr && right->numericSort_) {
            return sortValue_ < right->sortValue_;
        }
        const QString rightText = right == nullptr
            ? other.text().toCaseFolded()
            : right->textSortValue_;
        return textSortValue_.localeAwareCompare(rightText) < 0;
    }

private:
    qlonglong sortValue_ = 0;
    QString textSortValue_;
    bool numericSort_ = false;
};

qlonglong latencySortValue(qint64 latencyMs)
{
    return latencyMs >= 0 ? latencyMs : std::numeric_limits<qlonglong>::max();
}

qlonglong checkedAtSortValue(const QDateTime& checkedAt)
{
    return checkedAt.isValid() ? checkedAt.toMSecsSinceEpoch() : std::numeric_limits<qlonglong>::max();
}

qlonglong stateSortValue(const AutoNodeEvaluation& evaluation)
{
    if (evaluation.tested && evaluation.available) {
        return 0;
    }
    if (!evaluation.tested) {
        return 1;
    }
    return 2;
}

qlonglong currentSortValue(bool current)
{
    return current ? 0 : 1;
}

void configureNodeTable(QTableWidget* table)
{
    table->setColumnCount(7);
    table->setHorizontalHeaderLabels({
        QObject::tr("Current"),
        QObject::tr("Node"),
        QObject::tr("Country"),
        QObject::tr("Location"),
        QObject::tr("Latency"),
        QObject::tr("State"),
        QObject::tr("Checked")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setSectionsClickable(true);
    table->horizontalHeader()->setSortIndicatorShown(true);
    table->horizontalHeader()->setMinimumSectionSize(48);
    table->horizontalHeader()->setDefaultSectionSize(120);
    table->setColumnWidth(0, 70);
    table->setColumnWidth(1, 250);
    table->setColumnWidth(2, 110);
    table->setColumnWidth(3, 180);
    table->setColumnWidth(4, 80);
    table->setColumnWidth(5, 90);
    table->setColumnWidth(6, 120);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setAlternatingRowColors(true);
    table->setColumnHidden(2, true);
    table->setColumnHidden(6, true);
}

} // namespace

SongBirdAutoWindow::SongBirdAutoWindow(SongBirdAutoCoordinator& coordinator, QWidget* parent)
    : QMainWindow(parent)
    , coordinator_(coordinator)
{
    buildUi();
    buildTray();
    connectSignals();
    idleIcon_ = QIcon(QStringLiteral(":/app/logo-auto.ico"));
    activeIcon_ = QIcon(QStringLiteral(":/app/logo-auto-active.svg"));
    setWindowTitle(QStringLiteral("SongBirdAuto"));
    setMinimumSize(340, 220);
    resize(360, 260);
    updateAppIcon();
}

void SongBirdAutoWindow::buildUi()
{
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(14, 14, 14, 10);
    root->setSpacing(12);

    auto* topActions = new QHBoxLayout();
    topActions->setSpacing(8);
    subscriptionsButton_ = new QPushButton(tr("Subscriptions"), central);
    subscriptionsButton_->setMinimumHeight(36);
    topActions->addWidget(subscriptionsButton_);
    routingButton_ = new QPushButton(tr("Routing"), central);
    routingButton_->setObjectName(QStringLiteral("routingButton"));
    routingButton_->setMinimumHeight(36);
    topActions->addWidget(routingButton_);
    strategyCombo_ = new QComboBox(central);
    strategyCombo_->setObjectName(QStringLiteral("strategyCombo"));
    strategyCombo_->setMinimumHeight(36);
    strategyCombo_->setFixedWidth(128);
    strategyCombo_->addItem(tr("Lowest latency"), kStrategyLowestLatency);
    strategyCombo_->addItem(tr("First available"), kStrategyFirstAvailable);
    topActions->addWidget(strategyCombo_);
    topActions->addStretch(1);
    tunButton_ = new QPushButton(tr("TUN"), central);
    tunButton_->setObjectName(QStringLiteral("tunToggleButton"));
    tunButton_->setCheckable(true);
    tunButton_->setMinimumHeight(36);
    topActions->addWidget(tunButton_);
    root->addLayout(topActions);

    auto* countryRow = new QHBoxLayout();
    countryRow->setSpacing(8);
    countryCombo_ = new QComboBox(central);
    countryCombo_->setObjectName(QStringLiteral("countryCombo"));
    countryCombo_->setMinimumHeight(44);
    countryCombo_->setFixedWidth(220);
    countryCombo_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    countryRow->addWidget(countryCombo_);
    nodesButton_ = new QPushButton(tr("Nodes"), central);
    nodesButton_->setMinimumHeight(44);
    countryRow->addWidget(nodesButton_);
    root->addLayout(countryRow);

    runButton_ = new QPushButton(tr("Start"), central);
    runButton_->setMinimumHeight(52);
    root->addWidget(runButton_);

    lastSavedSubscriptionText_ = coordinator_.subscriptionUrlsText();
    subscriptionDraftText_ = lastSavedSubscriptionText_;

    subscriptionSaveTimer_ = new QTimer(this);
    subscriptionSaveTimer_->setSingleShot(true);
    runButtonAnimationTimer_ = new QTimer(this);
    runButtonAnimationTimer_->setInterval(kRunButtonAnimationIntervalMs);

    root->addStretch(1);

    setCentralWidget(central);
    logsStatusLabel_ = new QLabel(this);
    logsStatusLabel_->setObjectName(QStringLiteral("logsStatusLabel"));
    logsStatusLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    logsStatusLabel_->setCursor(Qt::PointingHandCursor);
    logsStatusLabel_->setMinimumWidth(0);
    logsStatusLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    logsStatusLabel_->installEventFilter(this);
    statusBar()->addPermanentWidget(logsStatusLabel_, 1);
    statusBar()->showMessage(tr("Initializing"));
    applyCompactStyle();
    updateRunButtonState();
    setTunEnabled(coordinator_.isTunEnabled());
    updateNodesButtonText();
    updateLogStatusText();
}

void SongBirdAutoWindow::buildTray()
{
    trayMenu_ = new QMenu(this);
    trayShowAction_ = trayMenu_->addAction(tr("Show SongBirdAuto"));
    trayRunAction_ = trayMenu_->addAction(tr("Start"));
    trayTunAction_ = trayMenu_->addAction(tr("Enable TUN"));
    trayTunAction_->setCheckable(true);
    trayCountriesMenu_ = trayMenu_->addMenu(tr("Country"));
    trayMenu_->addSeparator();
    trayExitAction_ = trayMenu_->addAction(tr("Exit"));

    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setIcon(windowIcon());
    trayIcon_->setContextMenu(trayMenu_);
    updateTrayMenu();
    trayIcon_->show();
}

void SongBirdAutoWindow::connectSignals()
{
    connect(runButton_, &QPushButton::clicked, this, [this]() {
        if (running_) {
            coordinator_.stop();
        } else {
            startProxy();
        }
    });
    connect(nodesButton_, &QPushButton::clicked, this, &SongBirdAutoWindow::showNodeTable);
    connect(subscriptionsButton_, &QPushButton::clicked, this, &SongBirdAutoWindow::showSubscriptionEditor);
    connect(routingButton_, &QPushButton::clicked, this, &SongBirdAutoWindow::showRoutingSettings);
    connect(tunButton_, &QPushButton::clicked, this, [this](bool checked) {
        coordinator_.setTunEnabled(checked);
    });
    connect(strategyCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (updatingStrategyCombo_ || index < 0) {
            return;
        }
        coordinator_.setAutoSelectionStrategy(strategyCombo_->itemData(index).toString());
    });
    connect(subscriptionSaveTimer_, &QTimer::timeout, this, &SongBirdAutoWindow::saveSubscriptionUrlsIfChanged);
    connect(runButtonAnimationTimer_, &QTimer::timeout, this, &SongBirdAutoWindow::advanceRunButtonAnimation);
    connect(trayShowAction_, &QAction::triggered, this, &SongBirdAutoWindow::showMainWindowFromTray);
    connect(trayRunAction_, &QAction::triggered, this, [this]() {
        if (running_) {
            coordinator_.stop();
        } else {
            startProxy();
        }
    });
    connect(trayTunAction_, &QAction::triggered, this, [this](bool checked) {
        coordinator_.setTunEnabled(checked);
    });
    connect(trayExitAction_, &QAction::triggered, this, &SongBirdAutoWindow::requestExit);
    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            showMainWindowFromTray();
        }
    });
    connect(countryCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (updatingCombo_ || index < 0) {
            return;
        }
        coordinator_.switchToCountry(countryCombo_->itemData(index).toString());
        updateNodesButtonText();
    });

    connect(&coordinator_, &SongBirdAutoCoordinator::countrySummariesChanged, this, &SongBirdAutoWindow::setCountrySummaries);
    connect(&coordinator_, &SongBirdAutoCoordinator::nodeEvaluationsChanged, this, &SongBirdAutoWindow::setNodeEvaluations);
    connect(&coordinator_, &SongBirdAutoCoordinator::logMessage, this, &SongBirdAutoWindow::appendLog);
    connect(&coordinator_, &SongBirdAutoCoordinator::taskSummaryMessage, this, &SongBirdAutoWindow::appendTaskSummary);
    connect(&coordinator_, &SongBirdAutoCoordinator::busyChanged, this, &SongBirdAutoWindow::setBusy);
    connect(&coordinator_, &SongBirdAutoCoordinator::runningChanged, this, &SongBirdAutoWindow::setRunning);
    connect(&coordinator_, &SongBirdAutoCoordinator::activeServerChanged, this, &SongBirdAutoWindow::setActiveServer);
    connect(&coordinator_, &SongBirdAutoCoordinator::statusMessageChanged, this, [this](const QString& message) {
        statusBar()->showMessage(message);
    });
    connect(&coordinator_, &SongBirdAutoCoordinator::subscriptionUrlsTextChanged, this, &SongBirdAutoWindow::setSubscriptionUrlsText);
    connect(&coordinator_, &SongBirdAutoCoordinator::tunEnabledChanged, this, &SongBirdAutoWindow::setTunEnabled);
    connect(&coordinator_, &SongBirdAutoCoordinator::autoSelectionStrategyChanged, this, &SongBirdAutoWindow::setAutoSelectionStrategy);
    connect(&coordinator_, &SongBirdAutoCoordinator::selectedCountryChanged, this, [this](const QString& countryCode) {
        updatingCombo_ = true;
        if (countryCode.trimmed().isEmpty()) {
            countryCombo_->setCurrentIndex(-1);
            updatingCombo_ = false;
            updateNodesButtonText();
            return;
        }
        for (int i = 0; i < countryCombo_->count(); ++i) {
            if (countryCombo_->itemData(i).toString() == countryCode) {
                countryCombo_->setCurrentIndex(i);
                updatingCombo_ = false;
                updateNodesButtonText();
                updateTrayMenu();
                return;
            }
        }
        countryCombo_->setCurrentIndex(-1);
        updatingCombo_ = false;
        updateNodesButtonText();
        updateTrayMenu();
    });
}

void SongBirdAutoWindow::startProxy()
{
    if (running_ || isActivationPending()) {
        return;
    }
    if (!ensureTunAdminReadyForStart()) {
        return;
    }

    activationInProgress_ = true;
    runButtonAnimationFrame_ = 0;
    startRunButtonAnimation();
    updateRunButtonState();
    coordinator_.start();
}

void SongBirdAutoWindow::closeEvent(QCloseEvent* event)
{
    if (exiting_) {
        QMainWindow::closeEvent(event);
        return;
    }
    if (trayIcon_ == nullptr || !QSystemTrayIcon::isSystemTrayAvailable()) {
        event->accept();
        QApplication::quit();
        return;
    }

    event->ignore();
    hide();
    if (!trayCloseMessageShown_) {
        trayCloseMessageShown_ = true;
        trayIcon_->showMessage(
            tr("SongBirdAuto"),
            tr("SongBirdAuto is still running in the tray."),
            QSystemTrayIcon::Information,
            2500);
    }
}

bool SongBirdAutoWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == logsStatusLabel_ && event != nullptr) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                showLogPanel();
                return true;
            }
        }

        if (event->type() == QEvent::ContextMenu) {
            showLogPanel();
            return true;
        }

        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) {
            refreshLogStatusLabel();
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void SongBirdAutoWindow::showMainWindowFromTray()
{
    show();
    raise();
    activateWindow();
}

void SongBirdAutoWindow::requestExit()
{
    exiting_ = true;
    saveSubscriptionUrlsIfChanged();
    coordinator_.stop();
    QApplication::quit();
}

void SongBirdAutoWindow::updateTrayMenu()
{
    if (trayMenu_ == nullptr || trayCountriesMenu_ == nullptr) {
        return;
    }

    const bool activationPending = isActivationPending();
    trayShowAction_->setText(isVisible() ? tr("Show SongBirdAuto") : tr("Show SongBirdAuto"));
    trayRunAction_->setText(activationPending ? tr("Starting Proxy") : (running_ ? tr("Stop Proxy") : tr("Start Proxy")));
    trayRunAction_->setEnabled(!busy_ && !activationPending);
    trayTunAction_->setText(tunEnabled_ ? tr("Disable TUN") : tr("Enable TUN"));
    trayTunAction_->setChecked(tunEnabled_);
    trayTunAction_->setEnabled(!activationPending && !(busy_ && !running_));

    trayCountriesMenu_->clear();
    const QString selectedCountryCode = coordinator_.selectedCountryCode();
    for (const AutoCountrySummary& country : countries_) {
        auto* action = trayCountriesMenu_->addAction(country.displayName);
        action->setCheckable(true);
        action->setChecked(country.countryCode == selectedCountryCode);
        action->setData(country.countryCode);
        connect(action, &QAction::triggered, this, [this, action]() {
            coordinator_.switchToCountry(action->data().toString());
        });
    }
    if (trayCountriesMenu_->actions().isEmpty()) {
        auto* emptyAction = trayCountriesMenu_->addAction(tr("No countries"));
        emptyAction->setEnabled(false);
    }
    updateTrayToolTip();
}

void SongBirdAutoWindow::updateTrayToolTip()
{
    if (trayIcon_ == nullptr) {
        return;
    }

    QStringList lines;
    lines.append(QStringLiteral("SongBirdAuto"));
    lines.append(isActivationPending()
        ? tr("Proxy starting")
        : (running_ ? tr("Proxy running") : tr("Proxy stopped")));
    if (!currentCountryDisplay_.isEmpty()) {
        lines.append(currentCountryDisplay_);
    }
    if (!currentServerName_.isEmpty()) {
        lines.append(currentServerName_);
    }
    trayIcon_->setToolTip(lines.join(QStringLiteral("\n")));
}

void SongBirdAutoWindow::applyCompactStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget {
            background: #f7f7f8;
            color: #161618;
            font-size: 14px;
        }
        QPushButton {
            border: 0;
            padding: 10px 14px;
            background: #e8e8ed;
            color: #161618;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #dedee5;
        }
        QPushButton:disabled {
            color: #92929b;
            background: #eeeef2;
        }
        QPushButton#primaryAction {
            background: #007aff;
            color: white;
            font-size: 16px;
        }
        QPushButton#primaryAction[mode="starting"] {
            background: #4aa3ff;
            color: white;
        }
        QPushButton#primaryAction[mode="starting"]:disabled {
            background: #4aa3ff;
            color: white;
        }
        QPushButton#dangerAction {
            background: #ff3b30;
            color: white;
            font-size: 16px;
        }
        QPushButton#primaryAction[mode="stop"] {
            background: #ff3b30;
        }
        QPushButton#tunToggleButton {
            min-width: 52px;
            padding: 8px 12px;
            border: 1px solid #d7d7dd;
            background: white;
            color: #4a4a52;
        }
        QPushButton#tunToggleButton:checked {
            background: #0ea5e9;
            border-color: #0ea5e9;
            color: white;
        }
        QComboBox#countryCombo {
            border: 1px solid #d7d7dd;
            padding: 8px 34px 8px 12px;
            background: white;
            font-size: 17px;
            font-weight: 700;
            font-family: Consolas, "Courier New", monospace;
        }
        QComboBox#countryCombo::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border-left: 1px solid #d7d7dd;
            background: #f0f0f4;
        }
        QComboBox#countryCombo::down-arrow {
            image: url(:/app/down.svg);
            width: 11px;
            height: 11px;
        }
        QComboBox#countryCombo QAbstractItemView {
            background: white;
            border: 1px solid #d7d7dd;
            selection-background-color: #d8eaff;
            selection-color: #161618;
            outline: 0;
            font-family: Consolas, "Courier New", monospace;
        }
        QComboBox#strategyCombo {
            border: 1px solid #d7d7dd;
            padding: 7px 28px 7px 10px;
            background: white;
            font-size: 12px;
            font-weight: 600;
        }
        QComboBox#strategyCombo:disabled {
            color: #92929b;
            background: #eeeef2;
        }
        QComboBox#strategyCombo::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border-left: 1px solid #d7d7dd;
            background: #f0f0f4;
        }
        QComboBox#strategyCombo::down-arrow {
            image: url(:/app/down.svg);
            width: 10px;
            height: 10px;
        }
        QTableWidget {
            background: white;
            border: 1px solid #ececf1;
            gridline-color: #ececf1;
            selection-background-color: #d8eaff;
            selection-color: #161618;
        }
        QScrollBar:vertical {
            background: #f0f0f4;
            width: 12px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #c5ced8;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover {
            background: #aeb9c6;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
            background: transparent;
        }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
        }
        QScrollBar:horizontal {
            background: #f0f0f4;
            height: 12px;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #c5ced8;
            min-width: 24px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #aeb9c6;
        }
        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            width: 0;
            background: transparent;
        }
        QScrollBar::add-page:horizontal,
        QScrollBar::sub-page:horizontal {
            background: transparent;
        }
        QHeaderView::section {
            background: #f0f0f4;
            border: 0;
            padding: 7px;
            color: #5c5c66;
            font-weight: 700;
        }
        QStatusBar {
            background: #f7f7f8;
            color: #6c6c75;
        }
        QLabel#logsStatusLabel {
            color: #4a4a52;
            font-size: 12px;
            font-weight: 500;
            padding-right: 2px;
        }
        QDialog {
            background: #f7f7f8;
        }
        QPlainTextEdit, QTextEdit {
            background: white;
            border: 1px solid #d7d7dd;
            padding: 10px;
            font-family: Consolas, monospace;
            font-size: 13px;
        }
    )"));
    runButton_->setObjectName(QStringLiteral("primaryAction"));
    style()->unpolish(runButton_);
    style()->polish(runButton_);
}

void SongBirdAutoWindow::showSubscriptionEditor()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Subscriptions"));
    dialog.resize(420, 520);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto* editor = new QPlainTextEdit(&dialog);
    editor->setPlainText(subscriptionDraftText_);
    editor->setPlaceholderText(tr("One subscription URL per line"));
    layout->addWidget(editor, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);

    connect(editor, &QPlainTextEdit::textChanged, this, [this, editor]() {
        if (updatingSubscriptionText_) {
            return;
        }
        subscriptionDraftText_ = editor->toPlainText();
        scheduleSubscriptionAutosave();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    dialog.exec();
    subscriptionDraftText_ = editor->toPlainText();
    saveSubscriptionUrlsIfChanged();
}

void SongBirdAutoWindow::showNodeTable()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Nodes"));
    dialog.resize(620, 560);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto* table = new QTableWidget(&dialog);
    configureNodeTable(table);
    renderNodeEvaluations(table);
    layout->addWidget(table, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    dialog.exec();
}

void SongBirdAutoWindow::showLogPanel()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Logs"));
    dialog.resize(430, 560);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto* logView = new QTextEdit(&dialog);
    logView->setReadOnly(true);
    logView->setPlainText(logLines_.join(QChar('\n')));
    layout->addWidget(logView, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    dialog.exec();
}

bool SongBirdAutoWindow::ensureTunAdminReadyForStart()
{
    if (!coordinator_.isTunEnabled() || !isWindowsPlatform() || isProcessElevated()) {
        return true;
    }

    if (DialogUtils::askYesNoQuestion(
            this,
            tr("Administrator Permission"),
            tr("TUN requires administrator privileges.\nRestart SongBirdAuto as administrator and start proxy now?"),
            QMessageBox::Yes)
        != QMessageBox::Yes) {
        appendLog(tr("TUN start canceled because administrator privileges are required."));
        return false;
    }

    saveSubscriptionUrlsIfChanged();
    QStringList arguments = startupRelaunchArgumentsForRunningInstance(
        QCoreApplication::arguments(),
        true,
        QCoreApplication::applicationPid());
    arguments.removeAll(kAutoStartArgument);
    arguments.append(kAutoStartArgument);

    if (!restartProcessAsAdministrator(QCoreApplication::applicationFilePath(), arguments)) {
        DialogUtils::showWarning(
            this,
            tr("Administrator Permission"),
            tr("Failed to restart SongBirdAuto with administrator privileges."));
        appendLog(tr("Failed to restart SongBirdAuto with administrator privileges."));
        return false;
    }

    exiting_ = true;
    QApplication::quit();
    return false;
}

void SongBirdAutoWindow::scheduleSubscriptionAutosave()
{
    if (updatingSubscriptionText_ || subscriptionSaveTimer_ == nullptr) {
        return;
    }
    subscriptionSaveTimer_->start(kSubscriptionAutosaveDelayMs);
}

void SongBirdAutoWindow::saveSubscriptionUrlsIfChanged()
{
    if (updatingSubscriptionText_) {
        return;
    }
    const QString text = subscriptionDraftText_;
    if (text == lastSavedSubscriptionText_) {
        pendingSubscriptionSave_ = false;
        return;
    }
    if (busy_) {
        pendingSubscriptionSave_ = true;
        return;
    }
    pendingSubscriptionSave_ = false;
    lastSavedSubscriptionText_ = text;
    coordinator_.saveSubscriptionUrlsText(text);
}

void SongBirdAutoWindow::showRoutingSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Routing"));
    dialog.resize(720, 540);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto* routingPage = new RoutingSettingsPageWidget(&dialog);
    routingPage->setConfig(coordinator_.currentConfig());
    layout->addWidget(routingPage, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    DialogUtils::localizeStandardDialogButtonBox(buttons);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (!coordinator_.saveRoutingSettings(
            routingPage->routingItems(),
            routingPage->routingCustomRules(),
            routingPage->settingsRoutingRuleTabKey())) {
        DialogUtils::showWarning(this, tr("Routing"), tr("Failed to save routing settings."));
    }
}

void SongBirdAutoWindow::setSubscriptionUrlsText(const QString& text)
{
    lastSavedSubscriptionText_ = text;
    subscriptionDraftText_ = text;
    pendingSubscriptionSave_ = false;
    if (subscriptionSaveTimer_ != nullptr) {
        subscriptionSaveTimer_->stop();
    }
}

void SongBirdAutoWindow::setAutoSelectionStrategy(const QString& strategy)
{
    if (strategyCombo_ == nullptr) {
        return;
    }

    updatingStrategyCombo_ = true;
    const QString normalized = strategy == kStrategyFirstAvailable ? kStrategyFirstAvailable : kStrategyLowestLatency;
    for (int index = 0; index < strategyCombo_->count(); ++index) {
        if (strategyCombo_->itemData(index).toString() == normalized) {
            strategyCombo_->setCurrentIndex(index);
            break;
        }
    }
    updatingStrategyCombo_ = false;
}

void SongBirdAutoWindow::setCountrySummaries(const QList<AutoCountrySummary>& summaries)
{
    countries_ = summaries.isEmpty() ? buildAutoCountrySummaries({}) : summaries;
    const QString selected = coordinator_.selectedCountryCode();
    updatingCombo_ = true;
    countryCombo_->clear();
    for (const AutoCountrySummary& country : countries_) {
        countryCombo_->addItem(country.displayName, country.countryCode);
    }
    for (int i = 0; i < countryCombo_->count(); ++i) {
        if (countryCombo_->itemData(i).toString() == selected) {
            countryCombo_->setCurrentIndex(i);
            break;
        }
    }
    countryCombo_->setEnabled(countryCombo_->count() > 0);
    updatingCombo_ = false;
    updateNodesButtonText();
    updateTrayMenu();
}

void SongBirdAutoWindow::setNodeEvaluations(const QList<AutoNodeEvaluation>& evaluations)
{
    evaluations_ = evaluations;
    updateNodesButtonText();
    updateTrayMenu();
}

QList<AutoNodeEvaluation> SongBirdAutoWindow::visibleNodeEvaluations() const
{
    const QString selectedCountryCode = coordinator_.selectedCountryCode();
    QList<AutoNodeEvaluation> visibleEvaluations;
    visibleEvaluations.reserve(evaluations_.size());
    for (const AutoNodeEvaluation& evaluation : evaluations_) {
        if (selectedCountryCode.isEmpty() || autoCountryKeyForEvaluation(evaluation) == selectedCountryCode) {
            visibleEvaluations.append(evaluation);
        }
    }
    std::sort(visibleEvaluations.begin(), visibleEvaluations.end(), evaluationLessThan);
    return visibleEvaluations;
}

void SongBirdAutoWindow::renderNodeEvaluations(QTableWidget* table)
{
    if (table == nullptr) {
        return;
    }
    table->setSortingEnabled(false);
    const QList<AutoNodeEvaluation> visibleEvaluations = visibleNodeEvaluations();
    table->setRowCount(visibleEvaluations.size());
    for (int row = 0; row < visibleEvaluations.size(); ++row) {
        const AutoNodeEvaluation& evaluation = visibleEvaluations.at(row);
        const bool current = !currentServerId_.trimmed().isEmpty() && evaluation.indexId == currentServerId_;
        table->setItem(row, 0, new NodeTableItem(current ? QStringLiteral("*") : QString(), currentSortValue(current)));
        table->setItem(row, 1, new NodeTableItem(evaluation.displayName));
        table->setItem(row, 2, new NodeTableItem(evaluation.countryDisplay));
        table->setItem(row, 3, new NodeTableItem(evaluation.locationSummary));
        table->setItem(row, 4, new NodeTableItem(latencyText(evaluation.latencyMs), latencySortValue(evaluation.latencyMs)));
        table->setItem(row, 5, new NodeTableItem(availabilityText(evaluation), stateSortValue(evaluation)));
        table->setItem(row, 6, new NodeTableItem(evaluation.checkedAt.isValid()
            ? evaluation.checkedAt.toLocalTime().toString(QStringLiteral("MM-dd hh:mm:ss"))
            : QStringLiteral("-"), checkedAtSortValue(evaluation.checkedAt)));
    }
    table->setSortingEnabled(true);
    table->sortByColumn(currentServerId_.trimmed().isEmpty() ? 4 : 0, Qt::AscendingOrder);
}

void SongBirdAutoWindow::updateNodesButtonText()
{
    if (nodesButton_ == nullptr) {
        return;
    }
    const QString selectedCountryCode = coordinator_.selectedCountryCode();
    if (selectedCountryCode.isEmpty()) {
        nodesButton_->setText(QStringLiteral("0 -"));
        return;
    }

    const auto summaryIt = std::find_if(countries_.cbegin(), countries_.cend(), [&selectedCountryCode](const AutoCountrySummary& country) {
        return country.countryCode == selectedCountryCode;
    });
    if (summaryIt == countries_.cend()) {
        nodesButton_->setText(QStringLiteral("0 -"));
        return;
    }

    nodesButton_->setText(QStringLiteral("%1 %2")
                              .arg(summaryIt->availableCount)
                              .arg(compactLatencyText(summaryIt->bestLatencyMs)));
}

void SongBirdAutoWindow::updateLogStatusText()
{
    if (logsStatusLabel_ == nullptr) {
        return;
    }
    QString text;
    if (!taskSummaryLines_.isEmpty()) {
        text = taskSummaryLines_.constLast();
    } else {
        const QString location = currentLocation_.trimmed().isEmpty()
            ? currentCountryDisplay_.trimmed()
            : currentLocation_.trimmed();
        if (currentServerName_.trimmed().isEmpty() && location.isEmpty()) {
            text.clear();
        } else {
            QStringList parts;
            if (!currentServerName_.trimmed().isEmpty()) {
                parts.append(currentServerName_.trimmed());
            }
            if (!location.isEmpty()) {
                parts.append(location);
            }
            text = QStringLiteral("Current Node %1 %2")
                       .arg(parts.isEmpty() ? QStringLiteral("-") : parts.join(QStringLiteral(" | ")))
                       .arg(compactLatencyText(currentLatencyMs_));
        }
    }
    logStatusText_ = text;
    refreshLogStatusLabel();
}

void SongBirdAutoWindow::refreshLogStatusLabel()
{
    if (logsStatusLabel_ == nullptr) {
        return;
    }
    const int availableWidth = logsStatusLabel_->contentsRect().width();
    const QString visibleText = availableWidth > 0
        ? logsStatusLabel_->fontMetrics().elidedText(logStatusText_, Qt::ElideRight, availableWidth)
        : logStatusText_;
    logsStatusLabel_->setText(visibleText);
    logsStatusLabel_->setToolTip(logStatusText_.isEmpty()
        ? tr("Click to open logs.")
        : QStringLiteral("%1\n%2").arg(logStatusText_, tr("Click to open logs.")));
}

void SongBirdAutoWindow::updateRunButtonState()
{
    if (runButton_ == nullptr) {
        return;
    }

    const bool activationPending = isActivationPending();
    if (strategyCombo_ != nullptr) {
        strategyCombo_->setEnabled(!activationPending && !busy_);
    }
    if (activationPending) {
        const int dotCount = (runButtonAnimationFrame_ % 3) + 1;
        runButton_->setText(tr("Starting%1").arg(QString(dotCount, QLatin1Char('.'))));
        runButton_->setProperty("mode", QStringLiteral("starting"));
        runButton_->setEnabled(false);
        startRunButtonAnimation();
    } else {
        stopRunButtonAnimation();
        runButton_->setText(running_ ? tr("Stop") : tr("Start"));
        runButton_->setProperty("mode", running_ ? QStringLiteral("stop") : QStringLiteral("start"));
        runButton_->setEnabled(!busy_);
    }

    style()->unpolish(runButton_);
    style()->polish(runButton_);
}

void SongBirdAutoWindow::startRunButtonAnimation()
{
    if (runButtonAnimationTimer_ != nullptr && !runButtonAnimationTimer_->isActive()) {
        runButtonAnimationTimer_->start();
    }
}

void SongBirdAutoWindow::stopRunButtonAnimation()
{
    if (runButtonAnimationTimer_ != nullptr) {
        runButtonAnimationTimer_->stop();
    }
    runButtonAnimationFrame_ = 0;
}

void SongBirdAutoWindow::advanceRunButtonAnimation()
{
    ++runButtonAnimationFrame_;
    updateRunButtonState();
}

void SongBirdAutoWindow::appendLog(const QString& message)
{
    logLines_.append(QStringLiteral("[%1] %2")
                         .arg(QDateTime::currentDateTime().toString(QStringLiteral("MM/dd hh:mm:ss")), message));
    while (logLines_.size() > kMaxBufferedLogLines) {
        logLines_.removeFirst();
    }
}

void SongBirdAutoWindow::setTunEnabled(bool enabled)
{
    tunEnabled_ = enabled;
    updateTunButtonState();
    updateTrayMenu();
}

void SongBirdAutoWindow::appendTaskSummary(const QString& message)
{
    const QString trimmed = message.trimmed();
    if (trimmed.isEmpty()) {
        taskSummaryLines_.clear();
        updateLogStatusText();
        return;
    }
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("MM/dd hh:mm:ss")), trimmed);
    taskSummaryLines_.append(line);
    while (taskSummaryLines_.size() > 4) {
        taskSummaryLines_.removeFirst();
    }
    logLines_.append(line);
    while (logLines_.size() > kMaxBufferedLogLines) {
        logLines_.removeFirst();
    }
    updateLogStatusText();
}

bool SongBirdAutoWindow::isActivationPending() const
{
    return activationInProgress_ || (running_ && busy_);
}

bool SongBirdAutoWindow::isProxyActive() const
{
    return running_ && !isActivationPending() && !currentLocation_.isEmpty();
}

void SongBirdAutoWindow::updateAppIcon()
{
    const QIcon icon = isProxyActive() && !activeIcon_.isNull()
        ? activeIcon_
        : (!idleIcon_.isNull() ? idleIcon_ : windowIcon());
    if (!icon.isNull()) {
        setWindowIcon(icon);
        if (trayIcon_ != nullptr) {
            trayIcon_->setIcon(icon);
        }
    }
}

void SongBirdAutoWindow::setBusy(bool busy)
{
    const bool wasBusy = busy_;
    busy_ = busy;
    countryCombo_->setEnabled(countryCombo_->count() > 0);
    if (routingButton_ != nullptr) {
        routingButton_->setEnabled(!busy_);
    }
    if (strategyCombo_ != nullptr) {
        strategyCombo_->setEnabled(!isActivationPending() && !busy_);
    }
    updateRunButtonState();
    updateTunButtonState();
    updateTrayMenu();
    updateAppIcon();
    if (wasBusy && !busy_ && pendingSubscriptionSave_ && subscriptionSaveTimer_ != nullptr) {
        pendingSubscriptionSave_ = false;
        subscriptionSaveTimer_->start(0);
    }
}

void SongBirdAutoWindow::setRunning(bool running)
{
    running_ = running;
    if (!running_) {
        activationInProgress_ = false;
    }
    updateRunButtonState();
    updateTrayMenu();
    updateAppIcon();
}

void SongBirdAutoWindow::setActiveServer(
    const QString& serverId,
    const QString& serverName,
    const QString& countryDisplay,
    const QString& location,
    qint64 latencyMs)
{
    currentServerId_ = serverId.trimmed();
    currentServerName_ = serverName.trimmed();
    currentCountryDisplay_ = countryDisplay.trimmed();
    currentLocation_ = location.trimmed();
    currentLatencyMs_ = latencyMs;
    if (running_ && !currentLocation_.isEmpty()) {
        activationInProgress_ = false;
    }
    updateLogStatusText();
    updateTrayToolTip();
    updateAppIcon();
}

void SongBirdAutoWindow::updateTunButtonState()
{
    if (tunButton_ == nullptr) {
        return;
    }
    const bool blocked = isActivationPending() || (busy_ && !running_);
    tunButton_->setEnabled(!blocked);
    tunButton_->setChecked(tunEnabled_);
    tunButton_->setToolTip(tunEnabled_ ? tr("Disable TUN") : tr("Enable TUN"));
    style()->unpolish(tunButton_);
    style()->polish(tunButton_);
}
