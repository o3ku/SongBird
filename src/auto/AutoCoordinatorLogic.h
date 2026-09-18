#pragma once

#include <QList>
#include <QString>

#include "auto/AutoTypes.h"
#include "domain/models/Config.h"

// Pure helpers extracted from SongBirdAutoCoordinator so they can be unit tested
// without a live config file, network access, or a running core process. They
// must stay free of I/O, logging, and Qt signals -- the coordinator keeps the
// wiring, this file keeps the decisions.
namespace AutoCoordinatorLogic {

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

} // namespace AutoCoordinatorLogic
