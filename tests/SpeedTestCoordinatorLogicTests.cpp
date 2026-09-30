#include <QtTest>

#include "app/SpeedTestCoordinatorLogic.h"

class SpeedTestCoordinatorLogicTests : public QObject {
    Q_OBJECT

private slots:
    void periodicSaveTriggersOnlyAtIntervalBoundary();
    void periodicSaveResetsBelowThreshold();
    void cancelSaveTriggersOnDirtyResults();
    void cancelSaveSkipsCleanBatch();
};

void SpeedTestCoordinatorLogicTests::periodicSaveTriggersOnlyAtIntervalBoundary()
{
    // Below the interval: results stay in memory and wait for more results
    // (or the end-of-batch save).
    QVERIFY(!SpeedTestCoordinatorLogic::shouldSavePeriodically(0));
    QVERIFY(!SpeedTestCoordinatorLogic::shouldSavePeriodically(
        SpeedTestCoordinatorLogic::kSaveIntervalResults - 1));

    // At the interval: a checkpoint save is due.
    QVERIFY(SpeedTestCoordinatorLogic::shouldSavePeriodically(
        SpeedTestCoordinatorLogic::kSaveIntervalResults));
    QVERIFY(SpeedTestCoordinatorLogic::shouldSavePeriodically(
        SpeedTestCoordinatorLogic::kSaveIntervalResults * 3));
}

void SpeedTestCoordinatorLogicTests::periodicSaveResetsBelowThreshold()
{
    // Documented contract for the coordinator: after a successful checkpoint
    // save the counter returns to zero, so the next checkpoint needs a full
    // interval of new results again.
    int resultsSinceLastSave = SpeedTestCoordinatorLogic::kSaveIntervalResults + 3;
    QVERIFY(SpeedTestCoordinatorLogic::shouldSavePeriodically(resultsSinceLastSave));
    resultsSinceLastSave = 0;
    QVERIFY(!SpeedTestCoordinatorLogic::shouldSavePeriodically(resultsSinceLastSave));
}

void SpeedTestCoordinatorLogicTests::cancelSaveTriggersOnDirtyResults()
{
    // Any un-persisted result must be flushed on cancel, even a single one:
    // the coordinator's cancel path runs before the token is dropped and is
    // the last chance to keep the measurements across a restart.
    QVERIFY(SpeedTestCoordinatorLogic::shouldSaveOnCancel(true));
}

void SpeedTestCoordinatorLogicTests::cancelSaveSkipsCleanBatch()
{
    // Nothing measured since the last save: no disk write, no error spam.
    QVERIFY(!SpeedTestCoordinatorLogic::shouldSaveOnCancel(false));
}

QTEST_MAIN(SpeedTestCoordinatorLogicTests)

#include "SpeedTestCoordinatorLogicTests.moc"
