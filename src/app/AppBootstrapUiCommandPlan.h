#pragma once

#include <QString>

struct DefaultServerSelectionPlan {
    bool shouldClearCurrentServerWarning = false;
    bool shouldMarkCurrentActivationPending = false;
    bool shouldClearCurrentActivationPending = false;
    bool shouldStartCore = false;
    bool shouldRestartRunningCore = false;
};

inline DefaultServerSelectionPlan evaluateDefaultServerSelectionPlan(
    bool commandSucceeded,
    const QString& previousIndexId,
    const QString& currentIndexId,
    bool coreRunning)
{
    DefaultServerSelectionPlan plan;
    if (!commandSucceeded) {
        return plan;
    }

    plan.shouldClearCurrentServerWarning = true;
    plan.shouldMarkCurrentActivationPending = true;

    if (coreRunning) {
        plan.shouldRestartRunningCore = previousIndexId != currentIndexId;
        plan.shouldClearCurrentActivationPending = !plan.shouldRestartRunningCore;
    } else {
        plan.shouldStartCore = true;
    }

    return plan;
}

struct RoutingSelectionPlan {
    bool shouldRestartRunningCore = false;
};

inline RoutingSelectionPlan evaluateRoutingSelectionPlan(
    bool commandSucceeded,
    bool previousAdvancedEnabled,
    bool currentAdvancedEnabled,
    int previousRoutingIndex,
    int currentRoutingIndex,
    bool coreRunning)
{
    RoutingSelectionPlan plan;
    plan.shouldRestartRunningCore =
        commandSucceeded
        && coreRunning
        && (previousAdvancedEnabled != currentAdvancedEnabled || previousRoutingIndex != currentRoutingIndex);
    return plan;
}
