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

// The one implementation of the move rule. `keysAt(index)` is the identity of the entry at a
// position and `keyOf(entry)` the identity of the entry itself -- a subscription id for the config
// list, an opaque row key for the settings table -- so "up"/"down"/"top"/"bottom" is written once
// instead of once per caller.
template <typename Container, typename KeyAt, typename KeyOf>
bool moveBy(Container& entries, const QStringList& selectedIds, SubscriptionOrder::MoveOperation operation, KeyAt keysAt, KeyOf keyOf)
{
    const QSet<QString> selected = toIdSet(selectedIds);
    if (entries.size() <= 1 || selected.isEmpty()) {
        return false;
    }

    bool moved = false;
    switch (operation) {
    case SubscriptionOrder::MoveOperation::Up:
        // Walk downwards so each selected row hops over its unselected predecessor exactly once;
        // walking up would move a run of selected rows past each other.
        for (int index = 1; index < entries.size(); ++index) {
            if (selected.contains(keysAt(index)) && !selected.contains(keysAt(index - 1))) {
                entries.swapItemsAt(index - 1, index);
                moved = true;
            }
        }
        break;
    case SubscriptionOrder::MoveOperation::Down:
        for (int index = entries.size() - 2; index >= 0; --index) {
            if (selected.contains(keysAt(index)) && !selected.contains(keysAt(index + 1))) {
                entries.swapItemsAt(index, index + 1);
                moved = true;
            }
        }
        break;
    case SubscriptionOrder::MoveOperation::Top:
    case SubscriptionOrder::MoveOperation::Bottom: {
        Container selectedItems;
        Container remainingItems;
        selectedItems.reserve(entries.size());
        remainingItems.reserve(entries.size());
        for (int index = 0; index < entries.size(); ++index) {
            if (selected.contains(keysAt(index))) {
                selectedItems.append(entries.at(index));
            } else {
                remainingItems.append(entries.at(index));
            }
        }

        // Moving every row to the top is not a reorder; reporting it as one would persist a save
        // that changed nothing.
        if (selectedItems.isEmpty() || selectedItems.size() == entries.size()) {
            break;
        }

        // "Already at the edge" is also not a reorder, and has to be reported as one: the caller
        // skips a save -- and the settings page skips rebuilding its whole table -- only when this
        // returns false. Up/Down get that for free from `moved`.
        // Compared in order, because "already at the edge" is about the sequence, not the
        // membership. Indexed through the same keysAt() so this works for any entry type.
        const int edgeOffset = operation == SubscriptionOrder::MoveOperation::Top
            ? 0
            : entries.size() - selectedItems.size();
        bool alreadyAtEdge = true;
        for (int index = 0; index < selectedItems.size(); ++index) {
            // `index` walks the picked entries; the candidates for "would it land where it already
            // is" are the first (or last) entries of the untouched list, so they are read straight
            // out of `entries` by position -- keysAt() above was only about membership.
            if (keyOf(selectedItems.at(index)) != keysAt(edgeOffset + index)) {
                alreadyAtEdge = false;
                break;
            }
        }
        if (alreadyAtEdge) {
            break;
        }

        entries = operation == SubscriptionOrder::MoveOperation::Top
            ? selectedItems + remainingItems
            : remainingItems + selectedItems;
        moved = true;
        break;
    }
    }

    return moved;
}

} // namespace

namespace SubscriptionOrder {

bool moveSubscriptions(QList<SubItem>& subscriptions, const QStringList& selectedIds, MoveOperation operation)
{
    const auto keyOf = [](const SubItem& item) { return item.id.trimmed(); };
    return moveBy(
        subscriptions,
        selectedIds,
        operation,
        [&subscriptions, &keyOf](int index) { return keyOf(subscriptions.at(index)); },
        keyOf);
}

bool moveKeys(QStringList& orderedKeys, const QStringList& selectedKeys, MoveOperation operation)
{
    // A key stands for exactly one row here: the settings page permutes rows by their slot, and the
    // permutation it gets back is read position by position. A repeated key would either move two
    // rows at once or name a position that no longer exists, so it is refused rather than guessed at.
    const QSet<QString> present(orderedKeys.begin(), orderedKeys.end());
    if (present.size() != orderedKeys.size()) {
        return false;
    }

    for (const QString& key : selectedKeys) {
        if (!present.contains(key.trimmed())) {
            return false;
        }
    }

    const auto keyOf = [](const QString& key) { return key.trimmed(); };
    return moveBy(
        orderedKeys,
        selectedKeys,
        operation,
        [&orderedKeys, &keyOf](int index) { return keyOf(orderedKeys.at(index)); },
        keyOf);
}

} // namespace SubscriptionOrder
