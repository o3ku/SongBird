#pragma once

#include <QList>
#include <QString>

#include "domain/models/SubItem.h"

// Pure ordering logic for a subscription update batch.
//
// Why it exists: updateInternal() walks the batch serially, so the order it walks in is the order
// the servers see requests. Two properties matter and neither was true of the plain list order:
//
//   1. Subscriptions sharing a host should not be hammered back to back. Many providers serve
//      several subscription URLs from one host, and firing all of them in a row looks like a burst
//      to that host -- which is exactly the traffic shape a rate limiter reacts to.
//   2. A subscription that has failed repeatedly should go last, so a dead URL does not delay the
//      healthy ones. `SubItem::failStreak` already records the count; nothing was reading it.
//
// The result is a stable permutation: the caller keeps its own list untouched, and subscriptions
// with equal priority keep their relative order.
namespace SubscriptionUpdateOrder {

// Returns `items` reordered so that:
//   * every group of subscriptions sharing a host is spread out rather than adjacent, and
//   * subscriptions with a higher failStreak sort later.
//
// The implementation is a round-robin over per-host queues, sorted by descending failStreak at the
// group level: the first pass takes one subscription from each host in priority order, the second
// pass takes the next from each, and so on. That satisfies both properties at once -- two URLs on
// the same host always land in different passes -- without needing a concurrency limit the serial
// loop could not honour anyway.
QList<SubItem> orderForUpdate(const QList<SubItem>& items);

// The host used to group subscriptions, lower-cased with the port dropped.
// Returns an empty string for a URL that cannot be parsed; every such subscription is treated as
// its own group, so an unparseable URL never blocks a healthy one behind it.
QString hostKey(const QString& url);

} // namespace SubscriptionUpdateOrder
