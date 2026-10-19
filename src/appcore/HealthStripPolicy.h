#pragma once

#include <QList>
#include <QString>

// Pure logic for the 24-hour health strip shown next to a server.
//
// Why it exists: the Result column reports one probe, and the health watch reports one outage. Neither
// answers the question a user actually asks before switching nodes -- "has this node been reliable
// today?". A node that just answered may have been dead for the last six hours; a node that just timed
// out may have been perfect until now. The strip makes that visible as 24 hour-wide cells.
//
// The representation is deliberately small and integer-only so it can be persisted in the existing
// state JSON without a new file or a new serializer:
//
//   +1  every probe in that hour succeeded
//   -1  at least one probe in that hour failed   (worst-in-hour wins)
//    0  no probe was recorded for that hour
//
// Worst-in-hour is the deliberate choice: an hour that was mostly fine but had an outage is an hour
// the node was unusable, and reporting it as "fine" because 5 of 6 probes passed would hide exactly
// the event the user needs to see.
namespace HealthStripPolicy {

// Number of cells. 24 hours, one per cell.
constexpr int kCellCount = 24;

// One cell's value.
enum class Cell : int {
    Unknown = 0,
    Passed = 1,
    Failed = -1
};

// A single recorded probe, reduced to what the strip needs.
struct ProbeRecord {
    // Wall-clock time of the probe, in milliseconds since the epoch.
    qint64 timestampMs = 0;
    bool passed = false;
};

// Folds `records` into `kCellCount` cells ending at `nowMs`.
//
// Cell 0 is the oldest hour and cell `kCellCount - 1` is the hour containing `nowMs`, so the strip
// reads left-to-right as oldest-to-newest -- the same direction a timeline is read.
//
// Records older than the window and records dated in the future (a clock that moved backwards) are
// both dropped: including a future record would put a result in the newest cell for a probe that has
// not happened yet.
QList<Cell> buildCells(const QList<ProbeRecord>& records, qint64 nowMs);

// Appends `record` to `records`, dropping anything that has fallen out of the window.
//
// Returns the pruned list. A caller that keeps only this return value has a bounded-size history
// without needing its own eviction rule.
QList<ProbeRecord> appendAndPrune(const QList<ProbeRecord>& records, const ProbeRecord& record, qint64 nowMs);

// Serialises cells into the compact string stored in the state file: one character per cell, oldest
// first, using '+', '-' and '0'. Chosen over a JSON array because the whole strip is 24 bytes and the
// state file is read on every launch.
QString encode(const QList<Cell>& cells);

// Parses a string produced by encode(). A malformed string yields an empty list rather than a
// best-effort guess: a strip that silently shows the wrong history is worse than no strip, because
// the user would trust it.
QList<Cell> decode(const QString& text);

} // namespace HealthStripPolicy
