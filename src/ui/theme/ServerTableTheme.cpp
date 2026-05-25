#include "ui/theme/ServerTableTheme.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QEvent>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QWidget>

#include "ui/theme/AppThemePalette.h"

namespace {

using namespace AppThemeInternal;

constexpr auto kServerTableStyleProperty = "serverTableStyle";
constexpr auto kServerTableHoveredRowProperty = "serverTableHoveredRow";
constexpr auto kServerTableHoverTrackerName = "serverTableHoverTracker";
constexpr auto kServerTableStyleDelegateName = "serverTableStyleDelegate";

class ServerTableStyleDelegate final : public QStyledItemDelegate {
public:
    explicit ServerTableStyleDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
        setObjectName(QString::fromLatin1(kServerTableStyleDelegateName));
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem styledOption(option);
        initStyleOption(&styledOption, index);

        const auto* tableView = qobject_cast<const QTableView*>(parent());
        const bool selected = (styledOption.state & QStyle::State_Selected) != 0;
        const int hoveredRow = tableView == nullptr
            ? -1
            : tableView->property(kServerTableHoveredRowProperty).toInt();
        const bool hovered = !selected && hoveredRow == index.row();
        const ThemePalette& palette = currentPalette();
        const QColor rowDividerColor(color(palette.borderMuted));
        const QColor hoveredDividerColor(color(palette.border));
        const QColor selectedDividerColor(color(palette.accent));
        const QColor currentDividerColor(color(palette.success));
        const QColor selectedBackgroundColor(color(palette.serverSelectedBackground));
        const QColor currentBackgroundColor(color(palette.serverCurrentBackground));
        const bool currentServer = index.data(Qt::UserRole + 1).toBool();
        const bool currentServerMarkerCell = currentServer && index.column() == 0;
        const QIcon currentServerIcon = currentServerMarkerCell
            ? qvariant_cast<QIcon>(index.data(Qt::DecorationRole))
            : QIcon();
        const QColor baseColor = tableView == nullptr
            ? QColor(color(palette.surface))
            : tableView->palette().color(QPalette::Base);
        const QColor alternateBaseColor = tableView == nullptr
            ? QColor(color(palette.surfaceSubtle))
            : tableView->palette().color(QPalette::AlternateBase);
        const QColor bgColor = selected
            ? selectedBackgroundColor
            : (currentServer
                    ? currentBackgroundColor
                    : (hovered
                    ? QColor(color(palette.hover))
                    : (((index.row() % 2) != 0) ? alternateBaseColor : baseColor)));
        const QColor themeTextColor(color(palette.text));

        if (currentServer) {
            styledOption.font.setBold(true);
        }
        styledOption.palette.setColor(QPalette::Base, baseColor);
        styledOption.palette.setColor(QPalette::AlternateBase, alternateBaseColor);
        const QVariant foregroundData = index.data(Qt::ForegroundRole);
        if (currentServer) {
            const QColor successColor(color(palette.success));
            styledOption.palette.setColor(QPalette::Text, successColor);
            styledOption.palette.setColor(QPalette::WindowText, successColor);
            styledOption.palette.setColor(QPalette::HighlightedText, successColor);
        } else if (!foregroundData.isValid()) {
            styledOption.palette.setColor(QPalette::Text, themeTextColor);
            styledOption.palette.setColor(QPalette::WindowText, themeTextColor);
            styledOption.palette.setColor(QPalette::HighlightedText, themeTextColor);
        }
        styledOption.palette.setColor(QPalette::Highlight, selectedBackgroundColor);

        styledOption.state &= ~QStyle::State_HasFocus;
        styledOption.state &= ~QStyle::State_MouseOver;
        styledOption.state &= ~QStyle::State_Selected;
        styledOption.features.setFlag(QStyleOptionViewItem::Alternate, false);
        styledOption.backgroundBrush = QBrush(Qt::NoBrush);
        if (currentServerMarkerCell) {
            styledOption.text.clear();
            styledOption.icon = QIcon();
        }
        painter->fillRect(option.rect, bgColor);

        const QWidget* widget = styledOption.widget;
        QStyle* style = widget == nullptr ? QApplication::style() : widget->style();
        style->drawControl(QStyle::CE_ItemViewItem, &styledOption, painter, widget);

        painter->save();
        if (currentServerMarkerCell && !currentServerIcon.isNull()) {
            const int iconExtent = qMax(12, qMin(option.rect.width(), option.rect.height()) - 8);
            const QRect iconRect(
                option.rect.left() + ((option.rect.width() - iconExtent) / 2),
                option.rect.top() + ((option.rect.height() - iconExtent) / 2),
                iconExtent,
                iconExtent);
            currentServerIcon.paint(painter, iconRect, Qt::AlignCenter);
        }
        painter->setPen(QPen(selected ? selectedDividerColor : (currentServer ? currentDividerColor : (hovered ? hoveredDividerColor : rowDividerColor))));
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        painter->restore();
    }
};

class ServerTableHoverTracker final : public QObject {
public:
    explicit ServerTableHoverTracker(QTableView* tableView)
        : QObject(tableView)
        , tableView_(tableView)
    {
        setObjectName(QString::fromLatin1(kServerTableHoverTrackerName));
        if (tableView_ != nullptr && tableView_->viewport() != nullptr) {
            tableView_->viewport()->installEventFilter(this);
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (tableView_ == nullptr
            || tableView_->viewport() == nullptr
            || watched != tableView_->viewport()
            || event == nullptr) {
            return QObject::eventFilter(watched, event);
        }

        switch (event->type()) {
        case QEvent::MouseMove: {
            const auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QModelIndex index = tableView_->indexAt(mouseEvent->pos());
            setHoveredRow(index.isValid() ? index.row() : -1);
            break;
        }
        case QEvent::Leave:
            setHoveredRow(-1);
            break;
        default:
            break;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    void setHoveredRow(int row)
    {
        if (tableView_ == nullptr) {
            return;
        }

        const int previousRow = tableView_->property(kServerTableHoveredRowProperty).toInt();
        if (previousRow == row) {
            return;
        }

        tableView_->setProperty(kServerTableHoveredRowProperty, row);

        if (tableView_->model() == nullptr) {
            tableView_->viewport()->update();
            return;
        }

        const int columnCount = tableView_->model()->columnCount();
        for (int column = 0; column < columnCount; ++column) {
            if (previousRow >= 0) {
                tableView_->update(tableView_->model()->index(previousRow, column));
            }
            if (row >= 0) {
                tableView_->update(tableView_->model()->index(row, column));
            }
        }
    }

    QPointer<QTableView> tableView_;
};

} // namespace

void ServerTableTheme::apply(QTableView* tableView)
{
    if (tableView == nullptr) {
        return;
    }

    tableView->setProperty(kServerTableStyleProperty, true);
    tableView->setProperty(kServerTableHoveredRowProperty, -1);
    tableView->setAlternatingRowColors(true);
    tableView->setShowGrid(false);
    tableView->setMouseTracking(true);

    if (tableView->viewport() != nullptr) {
        tableView->viewport()->setMouseTracking(true);
    }

    const ThemePalette& theme = currentPalette();

    if (tableView->findChild<QObject*>(QString::fromLatin1(kServerTableHoverTrackerName)) == nullptr) {
        new ServerTableHoverTracker(tableView);
    }

    if (tableView->itemDelegate() == nullptr
        || tableView->itemDelegate()->objectName() != QString::fromLatin1(kServerTableStyleDelegateName)) {
        tableView->setItemDelegate(new ServerTableStyleDelegate(tableView));
    }

    tableView->style()->unpolish(tableView);
    tableView->style()->polish(tableView);

    QPalette palette = tableView->palette();
    palette.setColor(QPalette::Base, QColor(color(theme.surface)));
    palette.setColor(QPalette::AlternateBase, QColor(color(theme.surfaceSubtle)));
    palette.setColor(QPalette::Text, QColor(color(theme.text)));
    palette.setColor(QPalette::WindowText, QColor(color(theme.text)));
    palette.setColor(QPalette::HighlightedText, QColor(color(theme.text)));
    palette.setColor(QPalette::Highlight, QColor(color(theme.accentSoft)));
    tableView->setPalette(palette);
    if (tableView->viewport() != nullptr) {
        tableView->viewport()->setPalette(palette);
    }

    tableView->update();
}

void ServerTableTheme::applyIfStyled(QWidget* widget)
{
    auto* tableView = qobject_cast<QTableView*>(widget);
    if (tableView != nullptr && tableView->property(kServerTableStyleProperty).toBool()) {
        apply(tableView);
    }
}
