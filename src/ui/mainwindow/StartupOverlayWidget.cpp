#include "StartupOverlayWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "app/CoreStartupCheckpoint.h"

namespace {

constexpr int OverlayHorizontalMargin = 24;
constexpr int ContentHorizontalPadding = 18;
constexpr int ContentVerticalPadding = 14;
constexpr int ContentMinimumWidth = 360;
constexpr int ContentMaximumWidth = 720;
constexpr int ChecklistDetailMaximumWidth = 420;
constexpr ushort CoreStartupFailedMark = 0x274C;

bool isCoreStartupFailureItem(const QString& item)
{
    const QString trimmedItem = item.trimmed();
    return trimmedItem.startsWith(QString(QChar(CoreStartupFailedMark)))
        || trimmedItem.startsWith(QStringLiteral("[!]"));
}

QPair<QString, QString> splitChecklistItem(const QString& item)
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

} // namespace

StartupOverlayWidget::StartupOverlayWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("loadingOverlay"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    contentWidget_ = new QWidget(this);
    contentWidget_->setObjectName(QStringLiteral("loadingContentWidget"));
    contentWidget_->setAttribute(Qt::WA_StyledBackground, true);

    titleLabel_ = new QLabel(contentWidget_);
    titleLabel_->setObjectName(QStringLiteral("loadingTitleLabel"));
    titleLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    titleLabel_->setWordWrap(true);

    itemsWidget_ = new QWidget(contentWidget_);
    itemsWidget_->setObjectName(QStringLiteral("loadingItemsWidget"));
    itemsLayout_ = new QVBoxLayout(itemsWidget_);
    itemsLayout_->setContentsMargins(0, 0, 0, 0);
    itemsLayout_->setSpacing(6);

    actionWidget_ = new QWidget(contentWidget_);
    actionWidget_->setObjectName(QStringLiteral("loadingActionWidget"));

    retryButton_ = new QPushButton(tr("Retry"), actionWidget_);
    retryButton_->setObjectName(QStringLiteral("loadingRetryButton"));
    retryButton_->setVisible(false);
    connect(retryButton_, &QPushButton::clicked, this, &StartupOverlayWidget::retryRequested);

    dismissButton_ = new QPushButton(tr("Close"), actionWidget_);
    dismissButton_->setObjectName(QStringLiteral("loadingDismissButton"));
    dismissButton_->setVisible(false);
    connect(dismissButton_, &QPushButton::clicked, this, &StartupOverlayWidget::dismissRequested);
    auto* actionLayout = new QHBoxLayout(actionWidget_);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(0);
    actionLayout->addStretch(1);
    actionLayout->addWidget(retryButton_);
    actionLayout->addSpacing(8);
    actionLayout->addWidget(dismissButton_);
    actionLayout->addStretch(1);

    auto* contentLayout = new QVBoxLayout(contentWidget_);
    contentLayout->setContentsMargins(
        ContentHorizontalPadding,
        ContentVerticalPadding,
        ContentHorizontalPadding,
        ContentVerticalPadding);
    contentLayout->setSpacing(12);
    contentLayout->addWidget(titleLabel_);
    contentLayout->addWidget(itemsWidget_);
    contentLayout->addWidget(actionWidget_);

    auto* overlayLayout = new QVBoxLayout(this);
    overlayLayout->setContentsMargins(24, 24, 24, 24);
    overlayLayout->setSpacing(12);
    overlayLayout->addStretch(1);
    overlayLayout->addWidget(contentWidget_, 0, Qt::AlignCenter);
    overlayLayout->addStretch(1);

    hide();
}

void StartupOverlayWidget::showChecklist(const QString& title, const QStringList& items)
{
    checklistFailed_ = false;
    for (const QString& item : items) {
        if (isCoreStartupFailureItem(item)) {
            checklistFailed_ = true;
            break;
        }
    }

    updateItems(items);

    titleLabel_->setText(checklistFailed_ ? tr("Proxy startup failed") : title);
    titleLabel_->setAlignment(
        items.isEmpty() && !checklistFailed_
            ? Qt::AlignCenter
            : (Qt::AlignLeft | Qt::AlignVCenter));

    itemsWidget_->setVisible(!items.isEmpty());
    dismissButton_->setVisible(checklistFailed_);
    retryButton_->setVisible(checklistFailed_);
    actionWidget_->setVisible(!items.isEmpty() || checklistFailed_);
    const bool wasVisible = isVisible();
    if (!wasVisible || geometry() != parentWidget()->rect()) {
        updateGeometry(parentWidget()->rect());
    }
    if (!wasVisible) {
        raise();
        show();
        setFocus(Qt::OtherFocusReason);
    }
}

void StartupOverlayWidget::hideOverlay()
{
    hide();
}

bool StartupOverlayWidget::isChecklistVisible() const
{
    return isVisible();
}

bool StartupOverlayWidget::isChecklistFailed() const
{
    return checklistFailed_;
}

void StartupOverlayWidget::updateGeometry(const QRect& parentRect)
{
    setGeometry(parentRect);
    if (contentWidget_ != nullptr) {
        const int availableWidth = qMax(1, width() - (OverlayHorizontalMargin * 2));
        const int contentWidth = qBound(
            qMin(ContentMinimumWidth, availableWidth),
            qRound(width() * 0.6),
            qMin(ContentMaximumWidth, availableWidth));
        contentWidget_->setFixedWidth(qMax(1, contentWidth));
    }
    if (isVisible()) {
        raise();
    }
}

void StartupOverlayWidget::updateItems(const QStringList& items)
{
    if (checklistItems_.size() != items.size()
        || itemsLayout_->count() != items.size()) {
        rebuildItems(items);
        return;
    }

    for (int index = 0; index < items.size(); ++index) {
        if (checklistItems_.at(index) == items.at(index)) {
            continue;
        }

        QLayoutItem* layoutItem = itemsLayout_->itemAt(index);
        QWidget* row = layoutItem == nullptr ? nullptr : layoutItem->widget();
        updateChecklistRow(row, items.at(index));
    }
    checklistItems_ = items;
}
void StartupOverlayWidget::rebuildItems(const QStringList& items)
{
    clearItems();
    for (const QString& item : items) {
        auto* row = new QWidget(itemsWidget_);
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
        detailLabel->setMaximumWidth(ChecklistDetailMaximumWidth);
        detailLabel->setFixedHeight(detailLabel->fontMetrics().height() + 2);
        rowLayout->addWidget(detailLabel, 1, Qt::AlignRight | Qt::AlignVCenter);

        updateChecklistRow(row, item);
        itemsLayout_->addWidget(row);
    }
    checklistItems_ = items;
}

void StartupOverlayWidget::clearItems()
{
    while (QLayoutItem* item = itemsLayout_->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            delete widget;
        }
        delete item;
    }
    checklistItems_.clear();
}

void StartupOverlayWidget::updateChecklistRow(QWidget* row, const QString& item)
{
    if (row == nullptr) {
        return;
    }

    const auto parts = splitChecklistItem(item);
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
            ChecklistDetailMaximumWidth)
        : QString());
    detailLabel->setToolTip(hasDetail ? parts.second : QString());
}
