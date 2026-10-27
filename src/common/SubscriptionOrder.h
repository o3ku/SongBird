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

} // namespace SubscriptionOrder
