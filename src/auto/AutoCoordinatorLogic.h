#pragma once

#include <QList>
#include <QString>

#include "appcore/RoutingScorePolicy.h"
#include "auto/AutoTypes.h"
#include "domain/models/Config.h"

// Pure helpers extracted from SongBirdAutoCoordinator so they can be unit tested
// without a live config file, network access, or a running core process. They
// must stay free of I/O, logging, and Qt signals -- the coordinator keeps the
// wiring, this file keeps the decisions.
namespace AutoCoordinatorLogic {

// Automatic selection can be running before the proxy is ready. Geo is display-only;
// the icon also requires the confirmed session signal, never just the auto-loop flag.
inline bool isProxyActive(bool running, bool activationPending, bool outboundReachable)
{
    return running && !activationPending && outboundReachable;
}

// The two strategies auto mode understands. normalizeAutoSelectionStrategy()
// always returns one of these, so comparing against kAutoStrategyFirstAvailable
// is safe.
extern const QString kAutoStrategyFirstAvailable;
extern const QString kAutoStrategyLowestLatency;

// Trims a subscription URL and rejects comment lines (a leading '#') by
// returning an empty string.
QString normalizeSubscriptionUrl(QString value);

// Maps any input to one of the two canonical strategy names. Anything that is
// not "firstAvailable" (case-insensitive) becomes kAutoStrategyLowestLatency, so
// an unknown or corrupt stored value degrades to the latency-based pick instead
// of silently behaving like "first available".
QString normalizeAutoSelectionStrategy(QString value);

// "CC 123 ms" when the node is available, otherwise the recorded error text, or
// "Failed" when there is no error text to show.
QString evaluationStateText(const AutoNodeEvaluation& evaluation);

// The first country that actually has nodes; falls back to the first entry, and
// to an empty string when the list is empty.
QString firstCountryWithNodesOrFirst(const QList<AutoCountrySummary>& countries);

// Exact, case-sensitive indexId lookup. Returns nullptr for a blank id.
const VmessItem* findServerInConfig(const Config& config, const QString& indexId);

// Detaches `activeServerId` from its subscription so a subscription refresh
// cannot delete the server the user is currently using: clears its subId and
// tags the remarks as an "Active copy |" duplicate.
void preserveActiveServerForSubscriptionUpdate(Config& config, const QString& activeServerId);

// After a subscription refresh, folds the preserved "Active copy" back onto the
// freshly downloaded server carrying the same reuse key, so the user keeps the
// same active selection. Returns true when the config was changed, which means
// the caller must persist it.
bool reconcilePreservedActiveServer(Config& config, const QString& activeServerId);

// The server currently carrying traffic: the one the last start actually
// selected. Falls back to `config.currentIndexId` when nothing has been started
// this session, which is the node the saved config points at and therefore the
// one a ranking challenger has to beat -- without the fallback the first round
// after launch compared against nothing and kept whatever was already there.
QString activeServerIdFor(const Config& config, const QString& startedServerId);

// The single gate before moving the active connection, so both the post-test
// switch and the failover path ask the same question.
//
// `activeSinceMs` is when the active node started carrying traffic. The score
// policy only consults it for its minimum hold window; the margin, the breaker
// and the smoothing all live in the policy.
bool shouldMoveActiveServer(
    const RoutingScorePolicy& policy,
    const RoutingScorePolicy::NodeState* current,
    const RoutingScorePolicy::NodeState* challenger,
    qint64 nowMs,
    qint64 activeSinceMs);

} // namespace AutoCoordinatorLogic
