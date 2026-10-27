#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include "common/OperationResult.h"
#include "domain/models/Config.h"

// Works out which subscription each failure line of a batch update belongs to.
//
// Why this is not just "parse the message": SubscriptionUpdateService reports a batch as one
// multi-line OperationResult, because that is what the log panel shows. The hover text on a
// subscription tab needs the opposite shape -- one reason per subscription -- and the only
// reliable key in that text is the subscription's display name, which is what the service
// prefixes every line with. A name is not an id, though: two subscriptions can share one (two
// remarks of "Airport"), so a line is attributed to the *first* subscription whose name matches
// and that has not been matched yet. A duplicate therefore does not put one subscription's
// failure on the other.
//
// It lives in services/ rather than beside the widgets that call it because the producer reads it
// too: SubscriptionUpdateService shares displayName() so the line it writes and the line that is
// matched cannot drift apart, and a service reaching into ui/ for that would be an upward edge in
// a tree that is otherwise ordered.
//
// The order that "first" depends on is the service's, not the config's. updateInternal() walks
// batches through SubscriptionUpdateOrder::orderForUpdate(), which spreads subscriptions sharing
// a host across round-robin passes and defers the ones with a failStreak -- so the config's list
// order is not the order the lines were written in, and two same-named subscriptions on different
// hosts would trade reasons. perSubscription() therefore applies the same permutation before it
// matches, and perSubscriptionFromMessage() skips the matching altogether when the service can
// hand back the attribution it already knows.
namespace SubscriptionUpdateFailureSummary {

// `subscriptionIds` is the set the batch was asked to update (empty means "all enabled
// subscriptions", which selects every subscription the same way SubscriptionUpdateService
// does). Only the `[FAIL]` lines are read -- the `[OK]` lines are not evidence of a problem.
QHash<QString, QString> perSubscription(const Config& config, const QStringList& subscriptionIds, const QString& message);

// The same question answered from the batch result alone. OperationResult carries the per-subject
// attribution the service built while it walked the batch (subject = subscription id, see
// OperationResult::failures), which is both more accurate and cheaper than re-deriving it from
// translated text -- the old path went through name matching only because there was nothing else
// to read. Falls back to perSubscription() when the result carries none.
QHash<QString, QString> perSubscriptionFromMessage(const Config& config, const QStringList& subscriptionIds, const OperationResult& result);

// The name SubscriptionUpdateService prefixes a line with: the trimmed remarks, or the URL when
// there are none. Kept here so the two cannot drift apart silently; a mismatch would show up as
// a failure nobody is told about rather than as a compile error.
QString displayName(const SubItem& item);

} // namespace SubscriptionUpdateFailureSummary
