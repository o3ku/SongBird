#include "ui/mainwindow/ServerTableView.h"
#include "ui/theme/AppTheme.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QTimer>

#include <algorithm>

ServerTableView::ServerTableView(QWidget* parent)
    : QTableView(parent)
{
    setDragDropMode(QAbstractItemView::NoDragDrop);
    setDefaultDropAction(Qt::IgnoreAction);
    setDragDropOverwriteMode(false);
    setDropIndicatorShown(false);
    setRowsReorderEnabled(false);
    AppTheme::applyServerTableStyle(this);
}

void ServerTableView::setRowsReorderEnabled(bool enabled)
{
    rowsReorderEnabled_ = enabled;
    setDragDropMode(enabled ? QAbstractItemView::DragDrop : QAbstractItemView::NoDragDrop);
    setDefaultDropAction(enabled ? Qt::MoveAction : Qt::IgnoreAction);
    setDropIndicatorShown(enabled);
    setDragEnabled(enabled);
    viewport()->setAcceptDrops(enabled);
    setAcceptDrops(enabled);
}

bool ServerTableView::rowsReorderEnabled() const
{
    return rowsReorderEnabled_;
}

void ServerTableView::setRowsMoveHandler(std::function<void(const QList<int>& rows, int targetRow)> handler)
{
    rowsMoveHandler_ = std::move(handler);
}

bool ServerTableView::moveSelectedRowsTo(int targetRow)
{
    return requestMoveSelectedRows(targetRow);
}

void ServerTableView::flashAttention(int durationMs)
{
    setAttentionHighlighted(true);
    setFocus(Qt::OtherFocusReason);
    QTimer::singleShot(qMax(1, durationMs), this, [this]() {
        setAttentionHighlighted(false);
    });
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
    QTableView::leaveEvent(event);
}

void ServerTableView::mousePressEvent(QMouseEvent* event)
{
    if (event != nullptr && event->button() == Qt::RightButton) {
        event->accept();
        return;
    }

    QTableView::mousePressEvent(event);
}

void ServerTableView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event != nullptr && event->button() == Qt::RightButton) {
        event->accept();
        return;
    }

    QTableView::mouseReleaseEvent(event);
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

void ServerTableView::setAttentionHighlighted(bool highlighted)
{
    setProperty("serverTableAttention", highlighted);
    setStyleSheet(highlighted
        ? QStringLiteral("ServerTableView { border: 1px solid #c47f17; border-radius: 4px; }")
        : QString());
    viewport()->update();
}

