#include "ui/mainwindow/ServerListController.h"

#include <QHeaderView>
#include <QLineEdit>

#include <algorithm>

#include "domain/models/Config.h"
#include "ui/mainwindow/ServerTableView.h"
#include "ui/models/ServerFilterProxyModel.h"
#include "ui/models/ServerTableModel.h"

ServerListController::ServerListController(
    const Context& context,
    std::function<void(const QString&)> applyTextFilter,
    std::function<void()> applyCurrentTabFilter,
    std::function<void()> updateSelectionForVisibleRows,
    std::function<void()> updateActionState,
    std::function<void()> updateQrPreview,
    std::function<void(const QStringList&)> reorderServersRequested,
    QObject* parent)
    : QObject(parent)
    , context_(context)
    , applyTextFilter_(std::move(applyTextFilter))
    , applyCurrentTabFilter_(std::move(applyCurrentTabFilter))
    , updateSelectionForVisibleRows_(std::move(updateSelectionForVisibleRows))
    , updateActionState_(std::move(updateActionState))
    , updateQrPreview_(std::move(updateQrPreview))
    , reorderServersRequested_(std::move(reorderServersRequested))
{
}

ServerListController::~ServerListController()
{
    if (context_.serverView != nullptr) {
        context_.serverView->setRowsMoveHandler({});
    }
}

void ServerListController::setup()
{
    if (context_.serverFilterEdit != nullptr) {
        QObject::connect(context_.serverFilterEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
            handleFilterTextChanged(text);
        });
    }

    if (context_.serverView != nullptr && context_.serverView->horizontalHeader() != nullptr) {
        QObject::connect(
            context_.serverView->horizontalHeader(),
            &QHeaderView::sectionClicked,
            this,
            [this](int logicalIndex) {
                toggleSorting(logicalIndex);
            });
    }

    if (context_.serverView != nullptr) {
        context_.serverView->setRowsMoveHandler([this](const QList<int>& movedRows, int targetRow) {
            const QStringList reorderedIds = buildReorderedServerIds(movedRows, targetRow);
            if (!reorderedIds.isEmpty()) {
                reorderServersRequested_(reorderedIds);
            }
        });
    }
}

void ServerListController::setDynamicSortEnabled(bool enabled, bool invalidateModel)
{
    if (context_.serverFilterModel == nullptr) {
        return;
    }

    dynamicSortSuspended_ = !enabled;
    if (context_.serverFilterModel->dynamicSortFilter() != enabled) {
        context_.serverFilterModel->setDynamicSortFilter(enabled);
    }

    if (enabled && invalidateModel) {
        int sortColumn = sortColumn_;
        Qt::SortOrder sortOrder = sortOrder_;
        if (sortColumn < 0 && context_.serverView != nullptr && context_.serverView->horizontalHeader() != nullptr) {
            sortColumn = context_.serverView->horizontalHeader()->sortIndicatorSection();
            sortOrder = context_.serverView->horizontalHeader()->sortIndicatorOrder();
        }

        if (sortColumn >= 0) {
            context_.serverFilterModel->sort(sortColumn, sortOrder);
        } else {
            context_.serverFilterModel->invalidate();
        }
    }
}

bool ServerListController::dynamicSortSuspended() const
{
    return dynamicSortSuspended_;
}

void ServerListController::handleFilterTextChanged(const QString& text)
{
    if (context_.serverFilterModel == nullptr) {
        return;
    }

    applyTextFilter_(text);
    updateReorderAvailability();
    updateSelectionForVisibleRows_();
}

void ServerListController::handleSubscriptionFilterChanged()
{
    applyCurrentTabFilter_();
    updateReorderAvailability();
    updateSelectionForVisibleRows_();
    updateActionState_();
    updateQrPreview_();
}

void ServerListController::updateReorderAvailability()
{
    if (context_.serverView == nullptr
        || context_.serverView->horizontalHeader() == nullptr
        || context_.serverFilterEdit == nullptr
        || context_.serverFilterModel == nullptr
        || context_.serverModel == nullptr) {
        return;
    }

    context_.serverView->setRowsReorderEnabled(false);
    context_.serverView->setToolTip(QString());
}

void ServerListController::toggleSorting(int logicalIndex)
{
    if (context_.serverView == nullptr
        || context_.serverView->horizontalHeader() == nullptr
        || context_.serverFilterModel == nullptr
        || logicalIndex < 0
        || logicalIndex >= context_.serverFilterModel->columnCount()) {
        return;
    }

    if (sortColumn_ != logicalIndex || sortColumn_ < 0) {
        sortColumn_ = logicalIndex;
        sortOrder_ = Qt::AscendingOrder;
    } else {
        sortOrder_ = sortOrder_ == Qt::AscendingOrder
            ? Qt::DescendingOrder
            : Qt::AscendingOrder;
    }

    applySorting(sortColumn_, sortOrder_);
}

void ServerListController::applySorting(int logicalIndex, Qt::SortOrder order)
{
    if (context_.serverView == nullptr
        || context_.serverView->horizontalHeader() == nullptr
        || context_.serverFilterModel == nullptr
        || logicalIndex < 0
        || logicalIndex >= context_.serverFilterModel->columnCount()) {
        return;
    }

    sortColumn_ = logicalIndex;
    sortOrder_ = order;

    QHeaderView* header = context_.serverView->horizontalHeader();
    header->setSortIndicator(sortColumn_, sortOrder_);
    context_.serverFilterModel->sort(sortColumn_, sortOrder_);

    updateReorderAvailability();
    updateSelectionForVisibleRows_();
    updateActionState_();
    updateQrPreview_();
}

void ServerListController::restoreSortState(const Config& config)
{
    const int column = config.ui().mainServerSortColumn;
    if (column < 0) {
        return;
    }

    const Qt::SortOrder order = config.ui().mainServerSortOrder == static_cast<int>(Qt::DescendingOrder)
        ? Qt::DescendingOrder
        : Qt::AscendingOrder;
    applySorting(column, order);
}

void ServerListController::captureSortState(Config& config) const
{
    if (sortColumn_ < 0) {
        config.ui().mainServerSortColumn = -1;
        config.ui().mainServerSortOrder = 0;
        return;
    }

    config.ui().mainServerSortColumn = sortColumn_;
    config.ui().mainServerSortOrder = static_cast<int>(sortOrder_);
}

QStringList ServerListController::buildReorderedServerIds(const QList<int>& movedRows, int targetRow) const
{
    if (context_.serverFilterModel == nullptr || context_.serverModel == nullptr) {
        return {};
    }

    QStringList visibleIds;
    visibleIds.reserve(context_.serverFilterModel->rowCount());
    for (int proxyRow = 0; proxyRow < context_.serverFilterModel->rowCount(); ++proxyRow) {
        const QModelIndex sourceIndex = context_.serverFilterModel->mapToSource(
            context_.serverFilterModel->index(proxyRow, 0));
        const ServerTableRow* item = context_.serverModel->itemAt(sourceIndex.row());
        if (item != nullptr && !item->indexId.isEmpty()) {
            visibleIds.append(item->indexId);
        }
    }

    if (visibleIds.isEmpty()) {
        return {};
    }

    QList<int> normalizedRows;
    for (int row : movedRows) {
        if (row >= 0 && row < visibleIds.size() && !normalizedRows.contains(row)) {
            normalizedRows.append(row);
        }
    }
    if (normalizedRows.isEmpty()) {
        return {};
    }

    std::sort(normalizedRows.begin(), normalizedRows.end());

    QStringList movedVisibleIds;
    movedVisibleIds.reserve(normalizedRows.size());
    for (int row : normalizedRows) {
        movedVisibleIds.append(visibleIds.at(row));
    }

    QStringList reorderedVisibleIds = visibleIds;
    for (int rowIndex = normalizedRows.size() - 1; rowIndex >= 0; --rowIndex) {
        reorderedVisibleIds.removeAt(normalizedRows.at(rowIndex));
    }

    int insertionIndex = qBound(0, targetRow, visibleIds.size());
    for (int row : normalizedRows) {
        if (row < insertionIndex) {
            --insertionIndex;
        }
    }
    insertionIndex = qBound(0, insertionIndex, reorderedVisibleIds.size());

    for (int index = 0; index < movedVisibleIds.size(); ++index) {
        reorderedVisibleIds.insert(insertionIndex + index, movedVisibleIds.at(index));
    }

    if (reorderedVisibleIds == visibleIds) {
        return {};
    }

    QStringList allIds;
    allIds.reserve(context_.serverModel->rowCount());
    for (int row = 0; row < context_.serverModel->rowCount(); ++row) {
        const ServerTableRow* item = context_.serverModel->itemAt(row);
        if (item != nullptr && !item->indexId.isEmpty()) {
            allIds.append(item->indexId);
        }
    }

    if (allIds.isEmpty()) {
        return {};
    }

    int visibleIndex = 0;
    for (int index = 0; index < allIds.size() && visibleIndex < reorderedVisibleIds.size(); ++index) {
        if (visibleIds.contains(allIds.at(index))) {
            allIds[index] = reorderedVisibleIds.at(visibleIndex);
            ++visibleIndex;
        }
    }

    return allIds;
}
