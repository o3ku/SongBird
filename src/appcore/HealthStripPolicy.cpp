#include "appcore/HealthStripPolicy.h"

#include <algorithm>

namespace {

constexpr qint64 kHourMs = 60LL * 60LL * 1000LL;

} // namespace

namespace HealthStripPolicy {

QList<Cell> buildCells(const QList<ProbeRecord>& records, qint64 nowMs)
{
    // Qt5's QList::fill() does not accept an enum class here (template deduction fails on the
    // scoped enum), so the list is sized and filled explicitly.
    QList<Cell> cells;
    cells.reserve(kCellCount);
    for (int i = 0; i < kCellCount; ++i) {
        cells.append(Cell::Unknown);
    }

    // The window ends with the hour that contains nowMs. Aligning to the hour rather than to the
    // exact instant is what makes a cell mean "this hour" instead of "the 60 minutes up to now",
    // which would shift the whole strip on every probe.
    const qint64 windowEndMs = ((nowMs / kHourMs) + 1) * kHourMs;
    const qint64 windowStartMs = windowEndMs - (kCellCount * kHourMs);

    for (const ProbeRecord& record : records) {
        if (record.timestampMs < windowStartMs || record.timestampMs >= windowEndMs) {
            // Older than the window, or dated in the future (a clock that moved backwards). A future
            // record would otherwise land in the newest cell for a probe that has not happened.
            continue;
        }

        const qint64 hourIndex = (record.timestampMs - windowStartMs) / kHourMs;
        if (hourIndex < 0 || hourIndex >= kCellCount) {
            continue;
        }

        Cell& cell = cells[static_cast<int>(hourIndex)];
        if (!record.passed) {
            // Worst-in-hour wins, and it is sticky: one failure marks the hour even if later probes
            // in the same hour pass.
            cell = Cell::Failed;
        } else if (cell == Cell::Unknown) {
            cell = Cell::Passed;
        }
    }

    return cells;
}

QList<ProbeRecord> appendAndPrune(
    const QList<ProbeRecord>& records, const ProbeRecord& record, qint64 nowMs)
{
    QList<ProbeRecord> pruned;
    pruned.reserve(records.size() + 1);

    const qint64 windowEndMs = ((nowMs / kHourMs) + 1) * kHourMs;
    const qint64 windowStartMs = windowEndMs - (kCellCount * kHourMs);

    for (const ProbeRecord& existing : records) {
        if (existing.timestampMs >= windowStartMs && existing.timestampMs < windowEndMs) {
            pruned.append(existing);
        }
    }
    if (record.timestampMs >= windowStartMs && record.timestampMs < windowEndMs) {
        pruned.append(record);
    }
    return pruned;
}

QString encode(const QList<Cell>& cells)
{
    QString text;
    text.reserve(cells.size());
    for (const Cell cell : cells) {
        switch (cell) {
        case Cell::Passed:
            text.append(QLatin1Char('+'));
            break;
        case Cell::Failed:
            text.append(QLatin1Char('-'));
            break;
        case Cell::Unknown:
        default:
            text.append(QLatin1Char('0'));
            break;
        }
    }
    return text;
}

QList<Cell> decode(const QString& text)
{
    // Only the strip this policy writes can be read back. A string of the wrong length is as
    // malformed as one with an unknown character: buildCells() always produces exactly kCellCount
    // cells, so anything else came from a different writer and cannot be trusted to line up with
    // the hours the UI will label.
    if (text.size() != kCellCount) {
        return {};
    }

    QList<Cell> cells;
    cells.reserve(text.size());
    for (const QChar ch : text) {
        if (ch == QLatin1Char('+')) {
            cells.append(Cell::Passed);
        } else if (ch == QLatin1Char('-')) {
            cells.append(Cell::Failed);
        } else if (ch == QLatin1Char('0')) {
            cells.append(Cell::Unknown);
        } else {
            // A malformed string yields nothing rather than a best-effort guess: a strip that shows
            // the wrong history is worse than no strip, because the user would trust it.
            return {};
        }
    }
    return cells;
}

} // namespace HealthStripPolicy
