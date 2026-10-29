#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include "appcore/RoutingScorePolicy.h"
#include "auto/AutoTypes.h"

// The node-selection scoring half of SongBirdAutoCoordinator.
//
// Why it exists: the coordinator held this state inline -- a per-node RoutingScorePolicy state map
// beside the evaluation list, the probe-feeding loop inside mergeEvaluationResults(), the
// effective-score rebuild duplicated in currentCountryBestServerId() and nextFailoverServerId(),
// and direct RoutingScorePolicy calls from the two switch gates -- which made the scoring rules
// unreachable from a test without the coordinator's config file, core process and UI wiring. Per
// the decomposition discipline src/auto/ inherits from src/app/, the state and the decisions made
// from it move into this class; the coordinator keeps the orchestration.
//
// Like RoutingScorePolicy, it holds no QObject, no timer and no thread: plain state plus pure
// decisions. There is still exactly one selection loop -- bestAutoServerIdForCountryByScore()
// ranks; this class only builds the score map that feeds it.
class AutoSelectionEngine final
{
public:
    AutoSelectionEngine() = default;

    // Drops every node's scoring history. Must be called wherever the caller clears its
    // evaluation list: the history describes nodes that are about to be replaced, and a recycled
    // indexId would otherwise inherit another server's smoothed latency and breaker state.
    void reset();

    // Feeds a batch of completed probes into the per-node scoring state, so the ranking and the
    // switch gate see the smoothed values and the breaker rather than one raw reading.
    //
    // An inconclusive or untested probe measured nothing -- no core ever ran, typically because a
    // temporary port could not be reserved -- so it is skipped instead of counted: feeding it in
    // as a failure is how a port shortage used to trip the breaker on healthy nodes and take them
    // out of the ranking for a minute.
    void recordProbeResults(const QList<AutoNodeEvaluation>& results);

    // Records a failed health check against `serverId`, seeding the node's state first when no
    // probe ever measured it. The seed stands in for "exists, unmeasured": failures alone must not
    // manufacture a sample, and the switch gate refuses to compare an unmeasured side, so without
    // the seed the breaker would trip invisibly and the gate could never open against this node.
    // What the seed buys is the comparison, not the verdict: until the breaker trips, the margin
    // still governs. A node with no state at all is left alone -- creating an entry here would
    // turn the gate's missing-side refusal into a comparison the evidence does not support.
    void recordHealthCheckFailure(const QString& serverId);

    // The best tested-and-available node of `countryCode` by effective score: EWMA-smoothed, with
    // a node whose breaker is open sorting last regardless of its last good latency. Lower wins.
    // `excludedServerId` removes a node from consideration -- the failover path uses it so a node
    // that just went dark is not immediately re-picked as its own replacement. Returns an empty
    // string when no node of that country qualifies.
    QString bestServerIdForCountry(
        const QList<AutoNodeEvaluation>& evaluations,
        const QString& countryCode,
        const QString& excludedServerId = {}) const;

    // The single gate before moving the active connection: true when `challengerId` may replace
    // `currentId` under the policy's switch margin, minimum hold window and circuit breaker
    // (forwarded through AutoCoordinatorLogic::shouldMoveActiveServer, the same question every
    // path that moves the connection asks). A node with no state is an unmeasured side and always
    // refuses the move. `activeSinceMs` is when the current node came up (0 = unknown, which
    // skips the hold window).
    bool shouldSwitch(
        const QString& currentId,
        const QString& challengerId,
        qint64 activeSinceMs) const;

    // True when `serverId` has state carrying at least one real sample (a seed counts). Lets the
    // caller tell "cannot compare -- a side is unmeasured" from "the policy says keep", which its
    // log lines distinguish.
    bool hasMeasuredNode(const QString& serverId) const;

    // The policy this engine feeds. Exposed so tests can read the same tunables the coordinator
    // runs with instead of restating them; the policy's own arithmetic is pinned by
    // RoutingScorePolicyTests and is not re-tested here.
    const RoutingScorePolicy& policy() const { return policy_; }

private:
    // One entry per server that has been probed (or seeded by a health-check failure), keyed by
    // indexId. Kept here rather than as a field of AutoNodeEvaluation because it is history -- a
    // smoothed value plus breaker state -- and the evaluation list is a snapshot that the window
    // renders and the table sorts.
    QHash<QString, RoutingScorePolicy::NodeState> nodeStates_;
    RoutingScorePolicy policy_;
};
