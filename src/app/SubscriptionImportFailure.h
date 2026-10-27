#pragma once

#include "common/OperationResult.h"

namespace SubscriptionImportFailure {

// Whether this finished import/update left the server table empty *because of
// itself*, so the panel standing in for that table is where its reason belongs.
//
// The panel is the only surface that can tell "you have not imported anything
// yet" from "your import failed", and it can only do that while there is nothing
// to show. Three things therefore keep a failure away from it: a success (the
// table is about to fill), a cancellation (not a verdict about the payload), and
// a batch that still imported servers (the rows are back, so the panel is gone).
// The batch's node count is the only place the last one is visible; a single
// import or update never mentions a count, and its failure is always total.
bool shouldExplainInEmptyServerPanel(const OperationResult& result);

// What to put in front of the user for a failure that qualifies above, without a
// `[FAIL] ` prefix: the panel's first line, because it shows one. A batch reports a
// summary line and then one line per subscription, so the summary describes the
// state and the line behind it says why -- that reason is the one to show. A single
// failure has no summary to step over and is returned whole, line breaks and all;
// the panel takes its first line and keeps the rest in the tooltip.
QString reasonForEmptyServerPanel(const QString& message);

} // namespace SubscriptionImportFailure
