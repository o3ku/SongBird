#pragma once

#include <QtGlobal>

// Pure scoring logic for choosing among several candidate nodes of the same subscription.
//
// Why it exists: the server table reports one probe per node, and the active node is either the
// user's pick or whatever happened to be first. Neither reacts to a node that was fast ten
// minutes ago and has been failing since, nor to a node that is merely mediocre when a better one
// is available. This policy turns a stream of probe outcomes into a comparable score and a
// switch decision, so a caller can keep the best node without flapping between two nodes whose
// latencies differ by noise.
//
// Like ProxyHealthWatchPolicy, it holds no timer, no thread and no I/O: it is a decision table.
// The caller owns the probes and the active-node bookkeeping.
//
// Three mechanisms, each answering a different question:
//
//   * EWMA smooths the latency so one slow probe does not reorder the list. Raw latency is
//     spiky -- a single 900 ms sample behind a 60 ms node is routine, not a reason to switch.
//   * A circuit breaker takes a node that has failed N probes in a row out of consideration for a
//     cooldown, so a dead node cannot win on a stale good score.
//   * Hysteresis demands the challenger be better by a margin before switching, so two nodes
//     within noise of each other do not swap the active connection on every probe.
//
// Failure and latency are deliberately combined into one number rather than compared
// lexicographically: a node with a perfect score that is currently failing has already been
// demoted by the breaker, so a single ordering covers both cases and the caller needs no second
// pass over the candidates.
class RoutingScorePolicy final
{
public:
    // Tunables. Exposed as a struct rather than as constructor arguments because the defaults are
    // the interesting part and a test wants to change exactly one of them.
    struct Config {
        // EWMA weight for a new sample: score = alpha * sample + (1 - alpha) * score.
        // 0.35 means a new latency moves the smoothed value a third of the way, which absorbs one
        // outlier while still reacting within a handful of probes. 1.0 disables smoothing.
        double alpha = 0.35;

        // A probe at or above this latency is treated as a failure for scoring purposes: on a
        // proxy client a 2 s round trip is indistinguishable from "does not work". The node still
        // counts as reachable for the health watch, which is why this is separate.
        int unusableLatencyMs = 2000;

        // Consecutive failures that trip the breaker.
        int failuresToTrip = 3;

        // How long a tripped node stays out of consideration. Long enough that the caller does not
        // re-probe a dead node on every tick, short enough that a node that came back is usable
        // again without a restart.
        int breakerCooldownMs = 60000;

        // The challenger's score must be at least this much better (as a fraction: 0.25 = 25 %)
        // before the caller switches away from a working node.
        double switchMargin = 0.25;

        // Score a node gets before it has any samples. Chosen so an untested node is neither
        // preferred over a known-good one nor permanently excluded; it sits between "excellent"
        // and "unusable" so the first real probe decides.
        double initialScore = 500.0;
    };

    // Per-node state. The caller owns one of these per server and passes it in, because the policy
    // itself is stateless -- that keeps a single policy instance usable for every node and makes
    // the state trivially serialisable if it ever needs to be persisted.
    struct NodeState {
        // Smoothed latency in milliseconds. Meaningless until hasSample is true.
        double score = 0.0;
        bool hasSample = false;
        int consecutiveFailures = 0;
        // Wall-clock timestamp (QDateTime::currentMSecsSinceEpoch) at which the breaker trips;
        // 0 when not tripped.
        qint64 breakerTrippedUntilMs = 0;
    };

    RoutingScorePolicy() = default;
    explicit RoutingScorePolicy(Config config);

    const Config& config() const { return config_; }

    // Seeds a node's score before any probe. Idempotent: a node that already has a sample keeps it,
    // so a caller can call this on every refresh without discarding history.
    void seed(NodeState* state) const;

    // Records one probe outcome.
    //
    // `latencyMs` is ignored when `reachable` is false. A reachable probe whose latency is at or
    // above unusableLatencyMs counts as a failure, because the node answers but cannot carry
    // traffic at a usable speed.
    //
    // `nowMs` is passed in rather than read from the clock so a test can drive the cooldown
    // without sleeping.
    void recordResult(NodeState* state, bool reachable, int latencyMs, qint64 nowMs) const;

    // True while the breaker is holding the node out of consideration.
    bool isBreakerOpen(const NodeState& state, qint64 nowMs) const;

    // The number used to rank nodes. A node with an open breaker sorts last regardless of its
    // smoothed latency, so this returns a value above any usable score in that case.
    double effectiveScore(const NodeState& state, qint64 nowMs) const;

    // Decides whether to move the active node from `current` to `challenger`.
    //
    // Returns false when either side has no sample yet: an untested challenger is not evidence,
    // and an untested current node must be measured before it can be replaced. The caller is
    // expected to probe both before asking.
    bool shouldSwitch(const NodeState& current, const NodeState& challenger, qint64 nowMs) const;

private:
    Config config_;
};
