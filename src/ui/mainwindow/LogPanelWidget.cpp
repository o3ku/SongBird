#include "ui/mainwindow/LogPanelWidget.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFrame>
#include <QFontMetrics>
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
#include <QScrollBar>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTime>
#include <QVBoxLayout>

#include "ui/mainwindow/LogItemDelegate.h"
#include "ui/models/LogFilterProxyModel.h"
#include "ui/models/LogListModel.h"
#include "ui/theme/AppTheme.h"

namespace {

constexpr int HeaderFilterMinimumCharacters = 14;

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

} // namespace

LogPanelWidget::LogPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("logPanel"));
    setMinimumHeight(0);

    auto* logPanelLayout = new QVBoxLayout(this);
    logPanelLayout->setContentsMargins(0, 0, 0, 0);
    logPanelLayout->setSpacing(0);

    auto* logHeaderRow = new QWidget(this);
    logHeaderRow->setObjectName(QStringLiteral("logHeaderRow"));
    logHeaderRow->setAttribute(Qt::WA_StyledBackground, true);
    auto* logHeaderLayout = new QHBoxLayout(logHeaderRow);
    logHeaderLayout->setContentsMargins(10, 4, 2, 4);
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
    logStickToBottomButton_->setChecked(true);
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

    logView_ = new QListView(this);
    logView_->setObjectName(QStringLiteral("logView"));
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

bool LogPanelWidget::eventFilter(QObject* watched, QEvent* event)
{
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

bool LogPanelWidget::shouldStickLogViewToBottom(bool filterActive) const
{
    if (filterActive || logView_ == nullptr) {
        return false;
    }

    QScrollBar* verticalScrollBar = logView_->verticalScrollBar();
    return verticalScrollBar != nullptr && verticalScrollBar->value() >= verticalScrollBar->maximum();
}

void LogPanelWidget::updateLogContextActions()
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

    QStringList lines;
    const QModelIndexList proxyIndexes = logView_->selectionModel()->selectedRows();
    for (const QModelIndex& proxyIndex : proxyIndexes) {
        lines.append(proxyIndex.data(Qt::DisplayRole).toString());
    }
    if (!lines.isEmpty()) {
        QApplication::clipboard()->setText(lines.join(QChar('\n')));
    }
}

void LogPanelWidget::copyAllLogLines()
{
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
}

void LogPanelWidget::updateWrapConfiguration()
{
    const int width = logView_->viewport()->width() - 2 * LogItemDelegate::HorizontalPadding;
    const QFont font = logView_->font();
    QMetaObject::invokeMethod(this, [this, font, width]() {
        logModel_->setWrapConfiguration(font, width);
    }, Qt::QueuedConnection);
}
