#include "services/ServerListOperations.h"

#include <QHash>
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

    // A drop arrives with every index id in the table, so this is the same shape as
    // SubscriptionService::reorderSubscriptions(): one hash of the current positions instead of a
    // scan per requested id, which was O(n^2) string comparisons inside an O(n) rebuild. Keep the
    // two in step -- a second copy of the linear version invites the next reader to copy it back.
    QHash<QString, int> positionById;
    positionById.reserve(servers.size());
    for (int position = 0; position < servers.size(); ++position) {
        const QString& indexId = servers.at(position).indexId;
        if (!indexId.isEmpty() && !positionById.contains(indexId)) {
            positionById.insert(indexId, position);
        }
    }

    QList<VmessItem> reordered;
    reordered.reserve(servers.size());

    QSet<QString> seenIds;
    for (const QString& indexId : orderedIndexIds) {
        if (indexId.isEmpty() || seenIds.contains(indexId)) {
            continue;
        }

        const auto position = positionById.constFind(indexId);
        if (position == positionById.constEnd()) {
            continue;
        }

        seenIds.insert(indexId);
        reordered.append(servers.at(position.value()));
    }

    // Servers the caller never mentioned stay behind the ones it did, in their existing relative
    // order. Every server is therefore in exactly one of the two passes, so the result is the same
    // size by construction and the `Invalid` check below only fires on a contradiction.
    for (const VmessItem& item : servers) {
        if (!seenIds.contains(item.indexId)) {
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
