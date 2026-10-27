#pragma once

#include <QString>

// Turns "when was this subscription last refreshed" into the two strings the UI shows.
//
// Why a separate unit: the answer is a pure function of two numbers (a stored timestamp and
// the current time), and the parts that usually break are the boundaries -- a refresh two
// minutes ago is not "0 hours ago", and a clock that moved backwards must not produce a
// negative age. Both are asserted in tests, which is only possible while nothing here touches
// a widget, a timer or the config file.
namespace SubscriptionUpdateTime {

// Age in whole minutes, clamped at 0.
//
// A negative age is real: `.state.json` and the primary config travel between machines, and a
// config written by a clock that is ahead arrives with a timestamp in the future. Reporting
// that as "53 years ago" (the unsigned underflow) is worse than reporting it as "just now", so
// the future reads as fresh.
qint64 ageMinutes(qint64 updatedAtMs, qint64 nowMs);

// "just now", "5 minutes ago", "3 hours ago", "2 days ago".
//
// Coarsening is deliberate: the value is a staleness hint, and "14 hours ago" answers the
// question ("is my node list from this morning?") where "13 hours 42 minutes ago" does not.
QString relativeLabel(qint64 updatedAtMs, qint64 nowMs);

// Local wall-clock time, `yyyy-MM-dd HH:mm`, for the tooltip where the exact moment is what
// the user is after.
QString absoluteLabel(qint64 updatedAtMs);

// The tab hover text: the subscription name, when it was last refreshed, and the exact moment.
//
// Returns an empty string for a subscription that has never been refreshed -- a tab with no
// tooltip is honest, while "Never updated" on every tab of a fresh install is an alarm about
// something that has not happened yet.
//
// `networkError` is the in-progress failure message (already translated by the caller) and is
// shown verbatim: the service writes those lines with the subscription name in front of the
// reason, and the name is already the header, so nothing here prefixes it a second time. It
// leads the reasoning, because a subscription that just failed is the one the user is hovering
// to ask about.
QString tooltipText(const QString& displayName, qint64 updatedAtMs, qint64 nowMs, const QString& networkError);

// The subscriptions-table cell: "-" for never, otherwise the relative label.
QString cellText(qint64 updatedAtMs, qint64 nowMs);

} // namespace SubscriptionUpdateTime
