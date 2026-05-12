#include "ui/mainwindow/ServerTableView.h"

#include "ui/models/ServerTableModel.h"

#include <QBrush>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QItemSelectionModel>
#include <QPalette>
#include <QPainter>
#include <QStyle>
#include <QApplication>
#include <QStyledItemDelegate>

#include <algorithm>

namespace {

class ServerTableItemDelegate final : public QStyledItemDelegate {
public:
    explicit ServerTableItemDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem styledOption(option);
        initStyleOption(&styledOption, index);

        const bool selected = (styledOption.state & QStyle::State_Selected) != 0;
        const auto* tableView = dynamic_cast<const ServerTableView*>(parent());
        const bool hovered = !selected && tableView != nullptr && tableView->hoveredRow() == index.row();
        const QColor rowDividerColor(QStringLiteral("#e1e7ee"));
        const QColor hoveredDividerColor(QStringLiteral("#9babc5"));
        const QColor selectedDividerColor(QStringLiteral("#b0c4e3"));
        styledOption.state &= ~QStyle::State_HasFocus;
        styledOption.state &= ~QStyle::State_MouseOver;
        styledOption.state &= ~QStyle::State_Selected;

        if (selected || hovered) {
            const QColor bgColor = selected ? QColor(QStringLiteral("#c5dbfe")) : QColor(QStringLiteral("#f2f6fc"));
            styledOption.backgroundBrush = QBrush(bgColor);
            painter->fillRect(option.rect, bgColor);
        } else {
            styledOption.backgroundBrush = QBrush(Qt::NoBrush);
        }

        const QWidget* widget = styledOption.widget;
        QStyle* style = widget == nullptr ? QApplication::style() : widget->style();
        style->drawControl(QStyle::CE_ItemViewItem, &styledOption, painter, widget);

        painter->save();
        painter->setPen(QPen(selected ? selectedDividerColor : (hovered ? hoveredDividerColor : rowDividerColor)));
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        painter->restore();
    }
};

} // namespace

ServerTableView::ServerTableView(QWidget* parent)
    : QTableView(parent)
{
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDragDropOverwriteMode(false);
    setDropIndicatorShown(true);
    setRowsReorderEnabled(true);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setItemDelegate(new ServerTableItemDelegate(this));

    connect(this, &QTableView::entered, this, [this](const QModelIndex& index) {
        setHoveredRow(index.isValid() ? index.row() : -1);
    });
}

void ServerTableView::setRowsReorderEnabled(bool enabled)
{
    rowsReorderEnabled_ = enabled;
    setDragEnabled(enabled);
    viewport()->setAcceptDrops(enabled);
    setAcceptDrops(enabled);
}

bool ServerTableView::rowsReorderEnabled() const
{
    return rowsReorderEnabled_;
}

int ServerTableView::hoveredRow() const
{
    return hoveredRow_;
}

void ServerTableView::setRowsMoveHandler(std::function<void(const QList<int>& rows, int targetRow)> handler)
{
    rowsMoveHandler_ = std::move(handler);
}

bool ServerTableView::moveSelectedRowsTo(int targetRow)
{
    return requestMoveSelectedRows(targetRow);
}

void ServerTableView::dragEnterEvent(QDragEnterEvent* event)
{
    if (rowsReorderEnabled_
        && event != nullptr
        && event->source() == this
        && selectionModel() != nullptr
        && !selectionModel()->selectedRows().isEmpty()) {
        event->acceptProposedAction();
        return;
    }

    QTableView::dragEnterEvent(event);
}

void ServerTableView::dragMoveEvent(QDragMoveEvent* event)
{
    if (rowsReorderEnabled_
        && event != nullptr
        && event->source() == this
        && selectionModel() != nullptr
        && !selectionModel()->selectedRows().isEmpty()) {
        event->acceptProposedAction();
        return;
    }

    QTableView::dragMoveEvent(event);
}

void ServerTableView::dropEvent(QDropEvent* event)
{
    if (rowsReorderEnabled_
        && event != nullptr
        && event->source() == this
        && requestMoveSelectedRows(resolveDropRow(event->pos()))) {
        event->acceptProposedAction();
        return;
    }

    QTableView::dropEvent(event);
}

void ServerTableView::leaveEvent(QEvent* event)
{
    setHoveredRow(-1);
    QTableView::leaveEvent(event);
}

int ServerTableView::resolveDropRow(const QPoint& viewportPosition) const
{
    if (model() == nullptr) {
        return 0;
    }

    const QModelIndex targetIndex = indexAt(viewportPosition);
    if (!targetIndex.isValid()) {
        return model()->rowCount();
    }

    const QRect targetRect = visualRect(targetIndex);
    return viewportPosition.y() > targetRect.center().y()
        ? targetIndex.row() + 1
        : targetIndex.row();
}

bool ServerTableView::requestMoveSelectedRows(int targetRow)
{
    if (!rowsReorderEnabled_
        || rowsMoveHandler_ == nullptr
        || model() == nullptr
        || selectionModel() == nullptr) {
        return false;
    }

    QList<int> rows;
    const QModelIndexList selectedRows = selectionModel()->selectedRows();
    rows.reserve(selectedRows.size());
    for (const QModelIndex& index : selectedRows) {
        if (index.isValid() && !rows.contains(index.row())) {
            rows.append(index.row());
        }
    }

    if (rows.isEmpty()) {
        return false;
    }

    std::sort(rows.begin(), rows.end());

    const int rowCount = model()->rowCount();
    const int clampedTargetRow = qBound(0, targetRow, rowCount);
    const int firstRow = rows.constFirst();
    const int lastRow = rows.constLast() + 1;
    if (clampedTargetRow >= firstRow && clampedTargetRow <= lastRow) {
        return false;
    }

    rowsMoveHandler_(rows, clampedTargetRow);
    return true;
}

void ServerTableView::setHoveredRow(int row)
{
    if (hoveredRow_ == row) {
        return;
    }

    hoveredRow_ = row;
    viewport()->update();
}
