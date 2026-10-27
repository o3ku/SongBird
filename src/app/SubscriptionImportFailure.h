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

} // namespace SubscriptionImportFailure
