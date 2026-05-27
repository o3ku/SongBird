#include "ui/mainwindow/LogPanelWidget.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMetaObject>
#include <QPoint>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QTime>
#include <QVBoxLayout>

#include "ui/mainwindow/LogItemDelegate.h"
#include "ui/mainwindow/LogPanelSupport.h"
#include "ui/models/LogFilterProxyModel.h"
#include "ui/models/LogListModel.h"
#include "ui/theme/AppTheme.h"

namespace LogUi = LogPanelSupport;

LogPanelWidget::LogPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("logPanel"));
    setMinimumHeight(0);

    auto* logPanelLayout = new QVBoxLayout(this);
    logPanelLayout->setContentsMargins(0, 0, 0, 0);
    logPanelLayout->setSpacing(0);

    logHeaderRow_ = new QWidget(this);
    logHeaderRow_->setObjectName(QStringLiteral("logHeaderRow"));
    logHeaderRow_->setAttribute(Qt::WA_StyledBackground, true);
    logHeaderRow_->setProperty("sectionHeader", true);
    logHeaderRow_->setCursor(Qt::PointingHandCursor);
    logHeaderRow_->setToolTip(tr("Collapse information"));
    logHeaderRow_->installEventFilter(this);
    auto* logHeaderLayout = new QHBoxLayout(logHeaderRow_);
    logHeaderLayout->setContentsMargins(10, 4, 4, 4);
    logHeaderLayout->setSpacing(6);

    auto* logTitleLabel = new QLabel(tr("Information"), logHeaderRow_);
    logTitleLabel->setObjectName(QStringLiteral("logTitleLabel"));
    logTitleLabel->setCursor(Qt::PointingHandCursor);
    logTitleLabel->setProperty("logHeaderCollapseTrigger", true);
    logTitleLabel->installEventFilter(this);
    AppTheme::applyCompactFont(logTitleLabel);
    logTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    logHeaderLayout->addWidget(logTitleLabel);
    logHeaderLayout->addStretch(1);

    logStickToBottomButton_ = new QToolButton(logHeaderRow_);
    logStickToBottomButton_->setObjectName(QStringLiteral("logStickToBottomButton"));
    AppTheme::applyCompactFont(logStickToBottomButton_);
    logStickToBottomButton_->setCheckable(true);
    logStickToBottomButton_->setToolTip(tr("Stick to bottom"));
    logStickToBottomButton_->setText(QString(QChar(0x2193)));
    logStickToBottomButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    logStickToBottomButton_->setChecked(true);
    logHeaderLayout->addWidget(logStickToBottomButton_);

    logFilterEdit_ = new QLineEdit(logHeaderRow_);
    logFilterEdit_->setObjectName(QStringLiteral("logFilterEdit"));
    AppTheme::applyCompactFont(logFilterEdit_);
    logFilterEdit_->setClearButtonEnabled(true);
    logFilterEdit_->setPlaceholderText(tr("Filter logs"));
    LogUi::configureContentSizedLineEdit(logFilterEdit_, LogUi::HeaderFilterMinimumCharacters);
    const int logFilterHeight = logFilterEdit_->sizeHint().height();
    logStickToBottomButton_->setFixedSize(logFilterHeight, logFilterHeight);
    logHeaderLayout->addWidget(logFilterEdit_);

    logPanelLayout->addWidget(logHeaderRow_);

    logModel_ = new LogListModel(this);
    logFilterModel_ = new LogFilterProxyModel(this);
    logFilterModel_->setSourceModel(logModel_);
    logItemDelegate_ = new LogItemDelegate(this);

    logView_ = new QListView(this);
    logView_->setObjectName(QStringLiteral("logView"));
    logView_->setProperty("sectionBody", true);
    logView_->setMinimumHeight(0);
    logView_->setContextMenuPolicy(Qt::CustomContextMenu);
    logView_->setWordWrap(false);
    logView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    logView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    logView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    logView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    logView_->setMouseTracking(true);
    logView_->setModel(logFilterModel_);
    logView_->setItemDelegate(logItemDelegate_);
    logView_->viewport()->installEventFilter(this);
    logView_->viewport()->setMouseTracking(true);
    logView_->installEventFilter(this);
    logPanelLayout->addWidget(logView_, 1);

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
    connect(logView_, &QListView::customContextMenuRequested, this, [this](const QPoint& position) {
        showLogContextMenu(position);
    });
    connect(logView_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        updateLogContextActions();
    });
    connect(selectAllLogAction_, &QAction::triggered, this, [this]() {
        LogUi::selectAllRows(logView_, logFilterModel_);
    });
    connect(copySelectedLogAction_, &QAction::triggered, this, [this]() {
        copySelectedLogLines();
    });
    connect(copyAllLogAction_, &QAction::triggered, this, [this]() {
        copyAllLogLines();
    });
    connect(clearLogAction_, &QAction::triggered, this, [this]() {
        if (logModel_ != nullptr) {
            logModel_->clear();
        }
        applyLogFilter();
    });

    updateLogContextActions();
}

void LogPanelWidget::appendLog(const QString& message)
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

void LogPanelWidget::setCompactMode(bool compact)
{
    if (compactMode_ == compact) {
        return;
    }

    compactMode_ = compact;
    if (logFilterEdit_ != nullptr) {
        logFilterEdit_->setVisible(!compactMode_);
    }
}

bool LogPanelWidget::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == logHeaderRow_ || watched->property("logHeaderCollapseTrigger").toBool()) && event != nullptr
        && event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            toggleCollapsed();
            return true;
        }
    }

    if (logView_ != nullptr && watched == logView_ && event != nullptr
        && event->type() == QEvent::ShortcutOverride) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->matches(QKeySequence::Copy) && LogUi::hasSelectedRows(logView_)) {
            event->accept();
        }
    }

    if (logView_ != nullptr && watched == logView_ && event != nullptr
        && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->matches(QKeySequence::Copy) && LogUi::hasSelectedRows(logView_)) {
            copySelectedLogLines();
            return true;
        }
    }

    if (logView_ != nullptr && logModel_ != nullptr
        && watched == logView_->viewport() && event != nullptr && event->type() == QEvent::Resize) {
        updateWrapConfiguration();
    }

    return QWidget::eventFilter(watched, event);
}

void LogPanelWidget::applyLogFilter()
{
    if (logFilterModel_ == nullptr || logView_ == nullptr) {
        return;
    }

    const QString pattern = logFilterEdit_ == nullptr ? QString() : logFilterEdit_->text().trimmed();
    const bool wasAtBottom = logStickToBottomEnabled_ || shouldStickLogViewToBottom(true);

    logFilterModel_->setPattern(pattern, false);
    if (!logFilterModel_->hasValidPattern()) {
        if (logFilterEdit_ != nullptr) {
            LogUi::setLineEditValidationState(logFilterEdit_, QStringLiteral("error"));
            const QRegularExpression expression(
                pattern,
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
            logFilterEdit_->setToolTip(expression.errorString());
        }
    } else {
        if (logFilterEdit_ != nullptr) {
            LogUi::setLineEditValidationState(logFilterEdit_, QString());
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
            const QModelIndex restoreIndex = logFilterModel_->index(
                qMin(preservedTopRow, logFilterModel_->rowCount() - 1),
                0);
            if (restoreIndex.isValid()) {
                logView_->scrollTo(restoreIndex, QAbstractItemView::PositionAtTop);
            }
        }
    }

    updateLogContextActions();
}

void LogPanelWidget::setCollapsed(bool collapsed)
{
    if (collapsed_ == collapsed) {
        return;
    }

    collapsed_ = collapsed;
    if (logView_ != nullptr) {
        logView_->setVisible(!collapsed_);
        if (!collapsed_ && logStickToBottomEnabled_) {
            logView_->scrollToBottom();
        }
    }

    if (logHeaderRow_ != nullptr) {
        logHeaderRow_->setToolTip(collapsed_ ? tr("Expand information") : tr("Collapse information"));
        setMaximumHeight(collapsed_ ? logHeaderRow_->sizeHint().height() : QWIDGETSIZE_MAX);
        setMinimumHeight(collapsed_ ? logHeaderRow_->sizeHint().height() : 0);
    }

    updateGeometry();
}

void LogPanelWidget::toggleCollapsed()
{
    setCollapsed(!collapsed_);
}

bool LogPanelWidget::shouldStickLogViewToBottom(bool filterActive) const
{
    if (filterActive || logView_ == nullptr) {
        return false;
    }

    return LogUi::viewAtBottom(logView_);
}

void LogPanelWidget::updateLogContextActions()
{
    if (logView_ == nullptr) {
        return;
    }

    const int visibleRows = logFilterModel_ == nullptr ? 0 : logFilterModel_->rowCount();
    const int selectedRows = LogUi::selectedRowCount(logView_);
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

void LogPanelWidget::showLogContextMenu(const QPoint& position)
{
    if (logContextMenu_ == nullptr || logView_ == nullptr) {
        return;
    }

    updateLogContextActions();
    logContextMenu_->popup(logView_->viewport()->mapToGlobal(position));
}

void LogPanelWidget::copySelectedLogLines()
{
    if (logView_ == nullptr
        || QApplication::clipboard() == nullptr
        || logView_->selectionModel() == nullptr) {
        return;
    }

    const QStringList lines = LogUi::selectedRowsText(logView_);
    if (!lines.isEmpty()) {
        QApplication::clipboard()->setText(lines.join(QChar('\n')));
    }
}

void LogPanelWidget::copyAllLogLines()
{
    if (logView_ == nullptr || QApplication::clipboard() == nullptr || logFilterModel_ == nullptr) {
        return;
    }

    const QStringList lines = LogUi::modelRowsText(logFilterModel_);
    if (!lines.isEmpty()) {
        QApplication::clipboard()->setText(lines.join(QChar('\n')));
    }
}

void LogPanelWidget::updateWrapConfiguration()
{
    const int width = logView_->viewport()->width() - 2 * LogItemDelegate::HorizontalPadding;
    const QFont font = logView_->font();
    QMetaObject::invokeMethod(this, [this, font, width]() {
        logModel_->setWrapConfiguration(font, width);
    }, Qt::QueuedConnection);
}
