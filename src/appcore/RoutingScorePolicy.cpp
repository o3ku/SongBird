#include "appcore/RoutingScorePolicy.h"

#include <algorithm>

namespace {

// A node with an open breaker must sort behind every node that is merely slow, so the sentinel is
// above unusableLatencyMs rather than equal to it: a node whose last probe was just under the
// unusable threshold still has a finite score and must win against a tripped one.
constexpr double kOpenBreakerScore = 1.0e9;

} // namespace

RoutingScorePolicy::RoutingScorePolicy(Config config)
    : config_(config)
{
    // Clamp the tunables into the ranges the arithmetic assumes. alpha outside (0, 1] would make
    // the EWMA diverge or freeze; a non-positive margin would let noise-driven switches through;
    // and a non-positive cooldown would make the breaker a no-op.
    if (config_.alpha <= 0.0) {
        config_.alpha = 0.35;
    } else if (config_.alpha > 1.0) {
        config_.alpha = 1.0;
    }
    if (config_.unusableLatencyMs <= 0) {
        config_.unusableLatencyMs = 2000;
    }
    if (config_.failuresToTrip < 1) {
        config_.failuresToTrip = 1;
    }
    if (config_.breakerCooldownMs < 0) {
        config_.breakerCooldownMs = 0;
    }
    if (config_.switchMargin < 0.0) {
        config_.switchMargin = 0.0;
    }
    if (config_.initialScore <= 0.0) {
        config_.initialScore = 500.0;
    }
}

void RoutingScorePolicy::seed(NodeState* state) const
{
    if (state == nullptr || state->hasSample) {
        return;
    }
    state->score = config_.initialScore;
    state->hasSample = true;
}

void RoutingScorePolicy::recordResult(
    NodeState* state, bool reachable, int latencyMs, qint64 nowMs) const
{
    if (state == nullptr) {
        return;
    }

    const bool usable = reachable && latencyMs > 0 && latencyMs < config_.unusableLatencyMs;
    if (!usable) {
        ++state->consecutiveFailures;
        if (state->consecutiveFailures >= config_.failuresToTrip) {
            state->breakerTrippedUntilMs = nowMs + config_.breakerCooldownMs;
        }
        // A failing node keeps its old smoothed score. Overwriting it with a failure value would
        // destroy the history that makes recovery cheap: once the breaker closes, one good probe
        // moves the score back toward the last known latency instead of starting from scratch.
        return;
    }

    state->consecutiveFailures = 0;
    // A successful probe means the node is answering again, so a breaker that was tripped by an
    // earlier outage no longer describes reality. Clear it here rather than waiting out the
    // cooldown: the probe that just succeeded is fresher evidence than the timer.
    state->breakerTrippedUntilMs = 0;

    if (!state->hasSample) {
        state->score = static_cast<double>(latencyMs);
        state->hasSample = true;
        return;
    }
    state->score = config_.alpha * static_cast<double>(latencyMs)
        + (1.0 - config_.alpha) * state->score;
}

bool RoutingScorePolicy::isBreakerOpen(const NodeState& state, qint64 nowMs) const
{
    return state.breakerTrippedUntilMs > 0 && nowMs < state.breakerTrippedUntilMs;
}

double RoutingScorePolicy::effectiveScore(const NodeState& state, qint64 nowMs) const
{
    if (isBreakerOpen(state, nowMs)) {
        return kOpenBreakerScore;
    }
    return state.hasSample ? state.score : config_.initialScore;
}

bool RoutingScorePolicy::shouldSwitch(
    const NodeState& current, const NodeState& challenger, qint64 nowMs) const
{
    // No evidence, no switch. An untested challenger could be worse than a known-good current
    // node, and an untested current node must be measured before it can be judged.
    if (!current.hasSample || !challenger.hasSample) {
        return false;
    }

    // A tripped current node is the one case where "better" needs no margin: the node is known to
    // be failing, so anything that is not also tripped is an improvement. Without this the
    // breaker would take a node out of consideration but never actually move off it.
    const bool currentOpen = isBreakerOpen(current, nowMs);
    const bool challengerOpen = isBreakerOpen(challenger, nowMs);
    if (currentOpen != challengerOpen) {
        return currentOpen;
    }
    if (challengerOpen) {
        // Both tripped: no reason to churn.
        return false;
    }

    // Scores are latencies, so lower is better. Requiring the challenger to be better by a margin
    // (rather than merely better) is what stops two nodes within noise of each other from trading
    // the active connection on every probe.
    const double required = current.score * (1.0 - config_.switchMargin);
    return challenger.score < required;
}
