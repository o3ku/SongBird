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
//  - WithheldSubscription: nodes exist and this tab lists none of them -- the
//    "Ungrouped" tab holds only nodes that belong to no subscription, so a grouped
//    config leaves it empty while the table is not empty. Showing a first-run guide
//    here tells the user to import something they already imported, so the panel
//    points at the subscription instead.
//  - SubscriptionWithoutNodes: an enabled subscription holds no node -- the download
//    was never run, failed, or came back empty. This is decided per subscription, not
//    from the model's total row count: a broken subscription next to a healthy one has
//    to offer the refresh, because that is the one case where a refresh is the fix.
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
// nodes the app actually holds. `listedSubscriptionId` names the subscription whose tab
// is on screen (empty for "Ungrouped"), and `listedSubscriptionNodeCount` is how many of
// the nodes on file carry that subscription's id. Both are counts over the *source*, not
// the visible rows: the whole point of the withheld state is that the nodes exist but
// this tab does not list them.
inline State decide(
    bool hasVisibleNodes,
    bool textFilterActive,
    int sourceServerCount,
    const QList<SubItem>& subscriptions,
    const QString& listedSubscriptionId = {},
    int listedSubscriptionNodeCount = 0)
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

    // The table being empty says nothing on its own: it is empty whenever the tab
    // lists nothing, which includes "this subscription downloaded nothing" next to a
    // subscription that did. The tab is what names the subject, so it is asked first.
    const bool aTabIsListed = sourceServerCount > 0 || !listedSubscriptionId.trimmed().isEmpty();
    if (aTabIsListed) {
        if (!listedSubscriptionId.trimmed().isEmpty()) {
            // A subscription tab: refresh if that subscription holds nothing, point at
            // it if its nodes are simply not this tab's to list.
            return listedSubscriptionNodeCount > 0 ? State::WithheldSubscription
                                                   : State::SubscriptionWithoutNodes;
        }
        // "Ungrouped" lists nodes that belong to no subscription, so its empty table
        // with nodes on file is always "they are grouped elsewhere".
        if (sourceServerCount > 0) {
            return State::WithheldSubscription;
        }
    }

    // No tab to ask about (the panel is being built before the tab bar exists) and no
    // node at all while an enabled subscription exists: the subscription is where to
    // look, whichever tab was meant.
    return State::SubscriptionWithoutNodes;
}

} // namespace ServerEmptyStatePolicy
