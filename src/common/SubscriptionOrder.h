#pragma once

#include <QList>
#include <QString>

#include "domain/models/SubItem.h"

// Moving subscriptions is a different question from moving servers: a subscription row is the
// header of a group, so "move up" has to answer what the user meant when the selection is not
// contiguous, and "top"/"bottom" must not scatter a multi-row selection. The rules live here as
// pure functions for the same reason ServerListOperations does -- both front ends' UI and the
// settings dialog's context menu need them, and neither should own a copy.
namespace SubscriptionOrder {

enum class MoveOperation {
    Top = 0,
    Up,
    Down,
    Bottom
};

// Moves the selected subscriptions one step, or to the edge of the list, keeping their relative
// order. A selection that is already at the edge is left alone, so the operation is idempotent and
// the caller can report "unchanged" instead of saving for nothing.
bool moveSubscriptions(QList<SubItem>& subscriptions, const QStringList& selectedIds, MoveOperation operation);

// The same rule over bare keys, for the settings page's editing buffer: a row the user just added
// has no subscription id yet, so the page identifies its rows by position and permutes them through
// the keys of one row list into the keys of the other. Both lists must be the same size -- a key
// identifies exactly one entry, and a duplicate would move two rows at once. Returns false, leaving
// both lists untouched, when the move is a no-op or when the keys do not line up.
bool moveKeys(QStringList& orderedKeys, const QStringList& selectedKeys, MoveOperation operation);

} // namespace SubscriptionOrder
