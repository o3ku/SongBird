#pragma once

#include <QString>

namespace SpeedTestCoordinatorLogic {

// Save policy for partial speed-test results.
//
// Results are normally persisted once, when the whole batch finishes
// (SpeedTestCoordinator::handleRunningChanged). If the batch is cancelled or
// the process dies midway, everything already measured used to survive only
// in the in-memory config and the UI — a restart threw it away. A periodic
// checkpoint plus a save-on-cancel path closes that gap. Both decisions are
// kept here as pure functions so they can be unit tested without a live
// config file or a running worker.

// How many newly measured results have to pile up before a checkpoint is
// written. The coordinator only polls on a timer, so a large batch is persisted
// every kSaveIntervalResults results instead of once at the end; smaller
// batches finish before this gate is ever reached and only hit the normal
// end-of-batch save.
constexpr int kSaveIntervalResults = 10;

// Whether a save is due: dirty results exist and enough new results have
// accumulated since the last save.
inline bool shouldSavePeriodically(int dirtyResultCount)
{
    return dirtyResultCount >= kSaveIntervalResults;
}

// Whether the cancel path must flush: any un-persisted result is worth saving
// when the batch is being torn down, and the dirty flag is what the
// coordinator already uses to decide whether the end-of-batch save runs.
inline bool shouldSaveOnCancel(bool resultsDirty)
{
    return resultsDirty;
}

} // namespace SpeedTestCoordinatorLogic
