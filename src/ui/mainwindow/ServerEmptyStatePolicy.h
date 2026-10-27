#pragma once

#include <QList>
#include <QString>

#include "domain/models/SubItem.h"

// What the panel that replaces an empty server table should say.
//
// The table can be empty for four different reasons and they need different
// words, because the user's next move differs:
//
//  - FirstRun: nothing has been imported yet. This is the only case that gets the
//    onboarding page offering all three ways in.
//  - WithheldSubscription: a subscription exists and is enabled, but its nodes are
//    hidden from this table (the "Ungrouped" tab lists only nodes that belong to no
//    subscription, and the non-empty branches of a grouped config can be invisible
//    here). Showing a first-run guide here tells the user to import something they
//    already imported, so the panel points at the subscription instead.
//  - SubscriptionWithoutNodes: a subscription exists and is enabled but the app
//    holds no node for it -- the download was never run, failed, or came back empty.
//    The fix is a refresh, not a re-import.
//  - NodesElsewhere: the app does hold nodes, this tab just shows none of them.
//
// The empty list is deliberately not the discriminator: "no servers at all" reads
// as first-run only when there is also no subscription on record. A config that
// has a subscription but no nodes is a broken import, not a first run.
namespace ServerEmptyStatePolicy {

enum class State {
    FirstRun,
    WithheldSubscription,
    SubscriptionWithoutNodes,
    NodesElsewhere
};

// True when at least one subscription is present and enabled. This is what
// separates "nothing has been imported" from "something was imported and produced
// nothing"; a disabled subscription does not count, because it is not imported
// into any view and its absence of nodes is not a fault.
inline bool hasEnabledSubscription(const QList<SubItem>& subscriptions)
{
    for (const SubItem& item : subscriptions) {
        if (item.enabled) {
            return true;
        }
    }
    return false;
}

// `sourceServerCount` is the model's row count *before* filtering -- the number of
// nodes the app actually holds.
inline State decide(
    bool hasVisibleNodes,
    bool textFilterActive,
    int sourceServerCount,
    const QList<SubItem>& subscriptions)
{
    if (hasVisibleNodes) {
        return State::NodesElsewhere;
    }

    // A filter the user typed is what emptied the table; the nodes exist and the
    // message has to say how to bring them back, whatever else is going on.
    if (textFilterActive) {
        return State::NodesElsewhere;
    }

    if (!hasEnabledSubscription(subscriptions)) {
        return State::FirstRun;
    }

    // The nodes are there; this tab is just not one that shows them.
    if (sourceServerCount > 0) {
        return State::WithheldSubscription;
    }

    // The app holds no node at all while an enabled subscription exists: whatever
    // tab is selected, the subscription is where to look. Which tab it was does not
    // change the advice, so it is not a parameter.
    return State::SubscriptionWithoutNodes;
}

} // namespace ServerEmptyStatePolicy
