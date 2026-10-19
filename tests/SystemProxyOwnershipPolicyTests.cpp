#include <QtTest>

#include <optional>

#include "appcore/SystemProxyOwnershipPolicy.h"
#include "common/SystemProxyMode.h"
#include "common/SystemProxySnapshot.h"

// The three rules that decide whether a failed startup or a stop may touch the system proxy.
//
// These used to be inline boolean expressions inside a 60-line teardown routine, and one of them
// was an OR where it had to be an AND -- which meant a startup that failed before ever writing to
// the registry still deleted the proxy the user had configured themselves. A rule that can be
// stated in one line belongs in a named function with a test, not in the middle of a teardown.
class SystemProxyOwnershipPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void failedStartupLeavesAProxyWeNeverTouchedAlone();
    void failedStartupRestoresWhenASnapshotExists();
    void failedStartupFallsBackToClearingWithoutASnapshot();
    void stopLeavesAProxyWeNeverTouchedAlone();
    void stopDoesNotRewriteAProxySomethingElseAlreadyTurnedOff();
    void stopRestoresTheSnapshotRatherThanClearing();
    void stopFallsBackToClearingWithoutASnapshot();

    // The snapshot itself: the two states a plain QString cannot express.
    void sanitizeLeavesAnUnrelatedSnapshotUntouched();
    void sanitizeTurnsASnapshotThatPointsAtUsIntoADisabledOne();
    void sanitizeKeepsTheSnapshotValidSoTheCallerStillRestores();
    void sanitizeIgnoresAnEmptyServerArgument();
    void anInvalidSnapshotIsNeverRestorable();
    void anEmptyButValidSnapshotIsStillRestorable();
};

void SystemProxyOwnershipPolicyTests::failedStartupLeavesAProxyWeNeverTouchedAlone()
{
    // The regression this exists for: the user was running their own proxy, the app never applied
    // its own, and the startup failed. Nothing may be written.
    QCOMPARE(
        decideFailedStartupProxyAction(/*managedSystemProxyActive=*/false, /*snapshotRestorable=*/true),
        FailedStartupProxyAction::LeaveAlone);
    QCOMPARE(
        decideFailedStartupProxyAction(false, false),
        FailedStartupProxyAction::LeaveAlone);
}

void SystemProxyOwnershipPolicyTests::failedStartupRestoresWhenASnapshotExists()
{
    QCOMPARE(
        decideFailedStartupProxyAction(/*managedSystemProxyActive=*/true, /*snapshotRestorable=*/true),
        FailedStartupProxyAction::Restore);
}

void SystemProxyOwnershipPolicyTests::failedStartupFallsBackToClearingWithoutASnapshot()
{
    // We did write, but the snapshot was lost (a state file from an older build, or a capture the
    // platform refused). Clearing is the only safe answer left -- and it is what the app did before
    // snapshots existed, so this branch is the compatibility path rather than a new behaviour.
    QCOMPARE(
        decideFailedStartupProxyAction(true, false),
        FailedStartupProxyAction::Clear);
}

void SystemProxyOwnershipPolicyTests::stopLeavesAProxyWeNeverTouchedAlone()
{
    QCOMPARE(
        decideStopProxyAction(false, /*systemProxyEnabled=*/true, /*snapshotRestorable=*/true),
        FailedStartupProxyAction::LeaveAlone);
    QCOMPARE(
        decideStopProxyAction(false, false, true),
        FailedStartupProxyAction::LeaveAlone);
}

void SystemProxyOwnershipPolicyTests::stopDoesNotRewriteAProxySomethingElseAlreadyTurnedOff()
{
    // We own the flag, but the registry no longer says "enabled": another tool turned the proxy
    // off while we were running. Restoring our snapshot would undo *their* change.
    QCOMPARE(
        decideStopProxyAction(true, /*systemProxyEnabled=*/false, /*snapshotRestorable=*/true),
        FailedStartupProxyAction::LeaveAlone);
}

void SystemProxyOwnershipPolicyTests::stopRestoresTheSnapshotRatherThanClearing()
{
    QCOMPARE(
        decideStopProxyAction(true, true, true),
        FailedStartupProxyAction::Restore);
}

void SystemProxyOwnershipPolicyTests::stopFallsBackToClearingWithoutASnapshot()
{
    QCOMPARE(
        decideStopProxyAction(true, true, false),
        FailedStartupProxyAction::Clear);
}

void SystemProxyOwnershipPolicyTests::sanitizeLeavesAnUnrelatedSnapshotUntouched()
{
    SystemProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.enabled = true;
    snapshot.server = QStringLiteral("10.0.0.1:3128");
    snapshot.overrideList = QStringLiteral("<local>");

    const SystemProxySnapshot sanitized =
        sanitizeSelfReferentialSnapshot(snapshot, QStringLiteral("127.0.0.1:10808"));

    QCOMPARE(sanitized, snapshot);
}

void SystemProxyOwnershipPolicyTests::sanitizeTurnsASnapshotThatPointsAtUsIntoADisabledOne()
{
    // The crashed-session case: the registry still holds our own port because the process was
    // killed while proxying. Restoring it faithfully would point the system at a dead port.
    SystemProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.enabled = true;
    snapshot.server = QStringLiteral("127.0.0.1:10808");

    const SystemProxySnapshot sanitized =
        sanitizeSelfReferentialSnapshot(snapshot, QStringLiteral("127.0.0.1:10808"));

    QVERIFY(!sanitized.enabled);
    QVERIFY(!sanitized.server.has_value());
    QVERIFY(!sanitized.overrideList.has_value());
}

void SystemProxyOwnershipPolicyTests::sanitizeKeepsTheSnapshotValidSoTheCallerStillRestores()
{
    // Dropping `valid` here would send the caller down the "nothing captured, clear it" path,
    // which is the same outcome by luck -- but it would also lose the AutoConfigURL removal that
    // a restore performs, so the PAC the enable path cleared would stay cleared.
    SystemProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.enabled = true;
    snapshot.server = QStringLiteral("127.0.0.1:10808");

    const SystemProxySnapshot sanitized =
        sanitizeSelfReferentialSnapshot(snapshot, QStringLiteral("127.0.0.1:10808"));

    QVERIFY(sanitized.valid);
    QVERIFY(isRestorableSystemProxySnapshot(sanitized));
}

void SystemProxyOwnershipPolicyTests::sanitizeIgnoresAnEmptyServerArgument()
{
    SystemProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.enabled = true;
    snapshot.server = QStringLiteral("127.0.0.1:10808");

    // Without a server to compare against, there is nothing to recognise as our own.
    QCOMPARE(sanitizeSelfReferentialSnapshot(snapshot, QString()), snapshot);
}

void SystemProxyOwnershipPolicyTests::anInvalidSnapshotIsNeverRestorable()
{
    // A default-constructed snapshot means "capture never ran". Restoring it would delete every
    // key, which is the defect this whole type exists to prevent.
    QVERIFY(!isRestorableSystemProxySnapshot(SystemProxySnapshot{}));
}

void SystemProxyOwnershipPolicyTests::anEmptyButValidSnapshotIsStillRestorable()
{
    // "The proxy was off and none of the keys existed" is a state worth restoring: it turns the
    // proxy off and deletes only our own leftovers.
    SystemProxySnapshot snapshot;
    snapshot.valid = true;
    snapshot.enabled = false;

    QVERIFY(isRestorableSystemProxySnapshot(snapshot));
}

QTEST_MAIN(SystemProxyOwnershipPolicyTests)
#include "SystemProxyOwnershipPolicyTests.moc"
