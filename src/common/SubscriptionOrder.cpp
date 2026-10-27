#include "common/SubscriptionOrder.h"

#include <QSet>

namespace {

QSet<QString> toIdSet(const QStringList& ids)
{
    QSet<QString> set;
    for (const QString& id : ids) {
        const QString trimmed = id.trimmed();
        if (!trimmed.isEmpty()) {
            set.insert(trimmed);
        }
    }
    return set;
}

} // namespace

namespace SubscriptionOrder {

bool moveSubscriptions(QList<SubItem>& subscriptions, const QStringList& selectedIds, MoveOperation operation)
{
    const QSet<QString> selected = toIdSet(selectedIds);
    if (subscriptions.size() <= 1 || selected.isEmpty()) {
        return false;
    }

    bool moved = false;
    switch (operation) {
    case MoveOperation::Up:
        // Walk downwards so each selected row hops over its unselected predecessor exactly once;
        // walking up would move a run of selected rows past each other.
        for (int index = 1; index < subscriptions.size(); ++index) {
            if (selected.contains(subscriptions.at(index).id.trimmed())
                && !selected.contains(subscriptions.at(index - 1).id.trimmed())) {
                subscriptions.swapItemsAt(index - 1, index);
                moved = true;
            }
        }
        break;
    case MoveOperation::Down:
        for (int index = subscriptions.size() - 2; index >= 0; --index) {
            if (selected.contains(subscriptions.at(index).id.trimmed())
                && !selected.contains(subscriptions.at(index + 1).id.trimmed())) {
                subscriptions.swapItemsAt(index, index + 1);
                moved = true;
            }
        }
        break;
    case MoveOperation::Top:
    case MoveOperation::Bottom: {
        QList<SubItem> selectedItems;
        QList<SubItem> remainingItems;
        selectedItems.reserve(subscriptions.size());
        remainingItems.reserve(subscriptions.size());
        for (const SubItem& item : subscriptions) {
            if (selected.contains(item.id.trimmed())) {
                selectedItems.append(item);
            } else {
                remainingItems.append(item);
            }
        }

        // Moving every row to the top is not a reorder; reporting it as one would persist a save
        // that changed nothing.
        if (selectedItems.isEmpty() || selectedItems.size() == subscriptions.size()) {
            break;
        }

        subscriptions = operation == MoveOperation::Top
            ? selectedItems + remainingItems
            : remainingItems + selectedItems;
        moved = true;
        break;
    }
    }

    return moved;
}

} // namespace SubscriptionOrder
