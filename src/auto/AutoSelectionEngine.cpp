#include "auto/AutoSelectionEngine.h"

#include <QDateTime>

#include "auto/AutoCoordinatorLogic.h"
#include "auto/AutoCountrySelection.h"

void AutoSelectionEngine::reset()
{
    nodeStates_.clear();
}

void AutoSelectionEngine::recordProbeResults(const QList<AutoNodeEvaluation>& results)
{
    // Feed every completed probe into the scoring policy, so a ranking made later (and the switch
    // gate) sees the smoothed value and the breaker rather than the raw reading of the last probe.
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (const AutoNodeEvaluation& result : results) {
        // An inconclusive probe measured nothing: no core ever ran, so nothing was learned about
        // the node. Feeding it to the policy as a failure is how a temporary port shortage used
        // to trip the breaker on healthy nodes and take them out of the ranking for a minute.
        if (!result.tested || result.inconclusive) {
            continue;
        }
        RoutingScorePolicy::NodeState& state = nodeStates_[result.indexId];
        // A probe that reports a latency is a measurement; one that reports failure is not. The
        // policy takes both through the same call so the failure counter and the breaker advance
        // together.
        policy_.recordResult(
            &state, result.available, static_cast<int>(result.latencyMs), nowMs);
    }
}

void AutoSelectionEngine::recordHealthCheckFailure(const QString& serverId)
{
    // A node with no state at all is left alone: creating an entry here would turn the switch
    // gate's missing-side refusal into a comparison, and a fabricated side is not evidence.
    const auto failedIt = nodeStates_.find(serverId);
    if (failedIt == nodeStates_.end()) {
        return;
    }
    RoutingScorePolicy::NodeState& failedState = failedIt.value();
    policy_.seed(&failedState);
    policy_.recordResult(&failedState, false, 0, QDateTime::currentMSecsSinceEpoch());
}

QString AutoSelectionEngine::bestServerIdForCountry(
    const QList<AutoNodeEvaluation>& evaluations,
    const QString& countryCode,
    const QString& excludedServerId) const
{
    // Rank by the policy's effective score rather than by the raw latency of the last probe, so a
    // node that is currently failing cannot win on a stale good reading, and one slow sample does
    // not reorder the list. The map is rebuilt per call because it is small (one entry per tested
    // node) and a cached copy would have to be invalidated from four different places.
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QHash<QString, double> effectiveScores;
    effectiveScores.reserve(nodeStates_.size());
    for (auto it = nodeStates_.constBegin(); it != nodeStates_.constEnd(); ++it) {
        effectiveScores.insert(it.key(), policy_.effectiveScore(it.value(), nowMs));
    }
    return bestAutoServerIdForCountryByScore(
        evaluations, countryCode, effectiveScores, excludedServerId);
}

bool AutoSelectionEngine::shouldSwitch(
    const QString& currentId,
    const QString& challengerId,
    qint64 activeSinceMs) const
{
    const auto currentIt = nodeStates_.constFind(currentId);
    const auto challengerIt = nodeStates_.constFind(challengerId);
    return AutoCoordinatorLogic::shouldMoveActiveServer(
        policy_,
        currentIt == nodeStates_.constEnd() ? nullptr : &currentIt.value(),
        challengerIt == nodeStates_.constEnd() ? nullptr : &challengerIt.value(),
        QDateTime::currentMSecsSinceEpoch(),
        activeSinceMs);
}

bool AutoSelectionEngine::hasMeasuredNode(const QString& serverId) const
{
    const auto it = nodeStates_.constFind(serverId);
    return it != nodeStates_.constEnd() && it->hasSample;
}
