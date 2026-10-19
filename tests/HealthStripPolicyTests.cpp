#include <QtTest>

#include "appcore/HealthStripPolicy.h"

using HealthStripPolicy::Cell;

namespace {

constexpr qint64 kHourMs = 60LL * 60LL * 1000LL;

// A fixed reference instant: 2026-01-01T00:00:00Z, aligned to an hour boundary so the arithmetic in
// the assertions is readable. Every test drives the clock explicitly rather than reading it, so the
// strip logic is testable without waiting an hour.
constexpr qint64 kNow = 1767225600000LL;

HealthStripPolicy::ProbeRecord probe(qint64 timestampMs, bool passed)
{
    HealthStripPolicy::ProbeRecord record;
    record.timestampMs = timestampMs;
    record.passed = passed;
    return record;
}

} // namespace

// Characterization tests for the 24-hour health strip.
//
// The representation is a fixed 24-cell list where cell 0 is the oldest hour and cell 23 contains
// "now", so the strip reads left-to-right as oldest-to-newest. Three properties matter and each can
// fail on its own:
//
//   * an hour with no probe stays Unknown, so "never tested" is not shown as "fine",
//   * one failure marks the whole hour (worst-in-hour), so a mostly-good hour with an outage is not
//     reported as good, and
//   * anything outside the window -- too old, or dated in the future -- is dropped rather than
//     clamped into the nearest cell.
class HealthStripPolicyTests : public QObject {
    Q_OBJECT

private slots:
    // --- buildCells ---
    void emptyHistoryYieldsAllUnknown();
    void aPassingProbeMarksItsHour();
    void theNewestCellIsTheHourContainingNow();
    void oneFailureMarksTheWholeHour();
    void failureIsNotUndoneByALaterPassInTheSameHour();
    void probesAnHourApartLandInAdjacentCells();
    void probesOlderThanTheWindowAreDropped();
    void probesInTheFutureAreDropped();

    // --- appendAndPrune ---
    void appendKeepsRecentRecords();
    void appendDropsRecordsThatLeftTheWindow();

    // --- encode/decode ---
    void encodeUsesOneCharacterPerCell();
    void encodeDecodeRoundTrips();
    void decodeRejectsMalformedInput();
    void decodeRejectsAnEmptyString();
};

void HealthStripPolicyTests::emptyHistoryYieldsAllUnknown()
{
    const QList<Cell> cells = HealthStripPolicy::buildCells({}, kNow);
    QCOMPARE(cells.size(), HealthStripPolicy::kCellCount);
    for (const Cell cell : cells) {
        QCOMPARE(cell, Cell::Unknown);
    }
}

void HealthStripPolicyTests::aPassingProbeMarksItsHour()
{
    const QList<Cell> cells = HealthStripPolicy::buildCells({probe(kNow, true)}, kNow);
    QCOMPARE(cells.size(), HealthStripPolicy::kCellCount);
    QCOMPARE(cells.constLast(), Cell::Passed);
}

void HealthStripPolicyTests::theNewestCellIsTheHourContainingNow()
{
    // A probe one hour before now belongs in the second-newest cell, not the newest.
    const QList<Cell> cells = HealthStripPolicy::buildCells(
        {probe(kNow - kHourMs, true)}, kNow);
    QCOMPARE(cells.at(HealthStripPolicy::kCellCount - 2), Cell::Passed);
    QCOMPARE(cells.constLast(), Cell::Unknown);
}

void HealthStripPolicyTests::oneFailureMarksTheWholeHour()
{
    const QList<Cell> cells = HealthStripPolicy::buildCells({probe(kNow, false)}, kNow);
    QCOMPARE(cells.constLast(), Cell::Failed);
}

void HealthStripPolicyTests::failureIsNotUndoneByALaterPassInTheSameHour()
{
    // Worst-in-hour: the pass came second and must not clear the failure.
    const QList<Cell> cells = HealthStripPolicy::buildCells(
        {probe(kNow, false), probe(kNow + 1000, true)}, kNow);
    QCOMPARE(cells.constLast(), Cell::Failed);
}

void HealthStripPolicyTests::probesAnHourApartLandInAdjacentCells()
{
    const QList<Cell> cells = HealthStripPolicy::buildCells(
        {probe(kNow - kHourMs, true), probe(kNow, false)}, kNow);
    QCOMPARE(cells.at(HealthStripPolicy::kCellCount - 2), Cell::Passed);
    QCOMPARE(cells.constLast(), Cell::Failed);
}

void HealthStripPolicyTests::probesOlderThanTheWindowAreDropped()
{
    // One hour further back than the window allows.
    const QList<Cell> cells = HealthStripPolicy::buildCells(
        {probe(kNow - (HealthStripPolicy::kCellCount * kHourMs), true)}, kNow);
    for (const Cell cell : cells) {
        QCOMPARE(cell, Cell::Unknown);
    }
}

void HealthStripPolicyTests::probesInTheFutureAreDropped()
{
    // A clock that moved backwards: a probe dated after now must not appear in the newest cell.
    const QList<Cell> cells = HealthStripPolicy::buildCells(
        {probe(kNow + (2 * kHourMs), true)}, kNow);
    for (const Cell cell : cells) {
        QCOMPARE(cell, Cell::Unknown);
    }
}

void HealthStripPolicyTests::appendKeepsRecentRecords()
{
    const QList<HealthStripPolicy::ProbeRecord> existing{probe(kNow - kHourMs, true)};
    const QList<HealthStripPolicy::ProbeRecord> pruned =
        HealthStripPolicy::appendAndPrune(existing, probe(kNow, false), kNow);
    QCOMPARE(pruned.size(), 2);
}

void HealthStripPolicyTests::appendDropsRecordsThatLeftTheWindow()
{
    const QList<HealthStripPolicy::ProbeRecord> existing{
        probe(kNow - (HealthStripPolicy::kCellCount * kHourMs), true)};
    const QList<HealthStripPolicy::ProbeRecord> pruned =
        HealthStripPolicy::appendAndPrune(existing, probe(kNow, true), kNow);
    // The old record fell out; only the new one remains.
    QCOMPARE(pruned.size(), 1);
    QCOMPARE(pruned.constFirst().timestampMs, kNow);
}

void HealthStripPolicyTests::encodeUsesOneCharacterPerCell()
{
    QList<Cell> cells;
    for (int i = 0; i < HealthStripPolicy::kCellCount; ++i) {
        cells.append(Cell::Passed);
    }
    QCOMPARE(HealthStripPolicy::encode(cells).size(), HealthStripPolicy::kCellCount);
}

void HealthStripPolicyTests::encodeDecodeRoundTrips()
{
    const QList<Cell> cells = HealthStripPolicy::buildCells(
        {probe(kNow - (2 * kHourMs), true), probe(kNow - kHourMs, false), probe(kNow, true)},
        kNow);
    const QString encoded = HealthStripPolicy::encode(cells);
    QCOMPARE(HealthStripPolicy::decode(encoded), cells);
}

void HealthStripPolicyTests::decodeRejectsMalformedInput()
{
    // An unknown character means the string did not come from encode(); returning a partial strip
    // would show the user a history that is silently wrong.
    QVERIFY(HealthStripPolicy::decode(QStringLiteral("+-x0")).isEmpty());
}

void HealthStripPolicyTests::decodeRejectsAnEmptyString()
{
    QVERIFY(HealthStripPolicy::decode(QString()).isEmpty());
}

QTEST_GUILESS_MAIN(HealthStripPolicyTests)
#include "HealthStripPolicyTests.moc"
