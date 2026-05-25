#include "services/ServerListOperations.h"

#include <QSet>

#include <algorithm>

namespace {

QSet<QString> toIndexIdSet(const QStringList& indexIds)
{
    QSet<QString> ids;
    for (const QString& indexId : indexIds) {
        ids.insert(indexId);
    }
    return ids;
}

bool hasSameServerOrder(const QList<VmessItem>& left, const QList<VmessItem>& right)
{
    return std::equal(
        left.cbegin(),
        left.cend(),
        right.cbegin(),
        right.cend(),
        [](const VmessItem& leftItem, const VmessItem& rightItem) {
            return leftItem.indexId == rightItem.indexId;
        });
}

} // namespace

namespace ServerListOperations {

QStringList normalizedIndexIds(const QList<QString>& indexIds)
{
    QStringList normalizedIds;
    for (const QString& indexId : indexIds) {
        const QString trimmed = indexId.trimmed();
        if (!trimmed.isEmpty() && !normalizedIds.contains(trimmed)) {
            normalizedIds.append(trimmed);
        }
    }
    return normalizedIds;
}

bool moveServers(QList<VmessItem>& servers, const QStringList& selectedIndexIds, ServerMoveOperation operation)
{
    if (servers.size() <= 1 || selectedIndexIds.isEmpty()) {
        return false;
    }

    const QSet<QString> selectedIds = toIndexIdSet(selectedIndexIds);
    bool moved = false;

    switch (operation) {
    case ServerMoveOperation::Up:
        for (int index = 1; index < servers.size(); ++index) {
            if (selectedIds.contains(servers.at(index).indexId)
                && !selectedIds.contains(servers.at(index - 1).indexId)) {
                servers.swapItemsAt(index - 1, index);
                moved = true;
            }
        }
        break;
    case ServerMoveOperation::Down:
        for (int index = servers.size() - 2; index >= 0; --index) {
            if (selectedIds.contains(servers.at(index).indexId)
                && !selectedIds.contains(servers.at(index + 1).indexId)) {
                servers.swapItemsAt(index, index + 1);
                moved = true;
            }
        }
        break;
    case ServerMoveOperation::Top:
    case ServerMoveOperation::Bottom: {
        QList<VmessItem> selectedItems;
        QList<VmessItem> remainingItems;
        selectedItems.reserve(servers.size());
        remainingItems.reserve(servers.size());
        for (const VmessItem& item : servers) {
            if (selectedIds.contains(item.indexId)) {
                selectedItems.append(item);
            } else {
                remainingItems.append(item);
            }
        }

        if (!selectedItems.isEmpty() && selectedItems.size() != servers.size()) {
            servers = operation == ServerMoveOperation::Top
                ? selectedItems + remainingItems
                : remainingItems + selectedItems;
            moved = true;
        }
        break;
    }
    }

    return moved;
}

ReorderResult reorderServers(QList<VmessItem>& servers, const QStringList& orderedIndexIds)
{
    if (orderedIndexIds.isEmpty()) {
        return ReorderResult::Unchanged;
    }

    QList<VmessItem> reordered;
    reordered.reserve(servers.size());

    for (const QString& indexId : orderedIndexIds) {
        const auto it = std::find_if(
            servers.cbegin(),
            servers.cend(),
            [&indexId](const VmessItem& item) {
                return item.indexId == indexId;
            });
        if (it != servers.cend()) {
            reordered.append(*it);
        }
    }

    const QSet<QString> orderedIds = toIndexIdSet(orderedIndexIds);
    for (const VmessItem& item : servers) {
        if (!orderedIds.contains(item.indexId)) {
            reordered.append(item);
        }
    }

    if (reordered.size() != servers.size()) {
        return ReorderResult::Invalid;
    }

    if (hasSameServerOrder(reordered, servers)) {
        return ReorderResult::Unchanged;
    }

    servers = reordered;
    return ReorderResult::Updated;
}

} // namespace ServerListOperations
