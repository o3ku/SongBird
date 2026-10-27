#include <QtTest>

#include "domain/models/RuntimeState.h"

class RuntimeStateTests : public QObject {
    Q_OBJECT

private slots:
    void applySnapshotEmitsOnlyChangedGroups();
    void undoWarningAloneCountsAsCurrentServerChange();
};

void RuntimeStateTests::applySnapshotEmitsOnlyChangedGroups()
{
    RuntimeState state;
    QSignalSpy currentServerSpy(&state, &RuntimeState::currentServerSegmentChanged);
    QSignalSpy proxyUiStateSpy(&state, &RuntimeState::proxyUiStateChanged);
    QSignalSpy systemProxySpy(&state, &RuntimeState::systemProxyStateChanged);
    QSignalSpy autoRunSpy(&state, &RuntimeState::autoRunChanged);
    QSignalSpy routingSpy(&state, &RuntimeState::routingStatusChanged);

    RuntimeStateSnapshot snapshot;
    snapshot.currentServerName = QStringLiteral("HK-01");
    snapshot.currentServerLocation = QStringLiteral("Hong Kong");
    snapshot.currentServerWarning = QStringLiteral("Please verify manually");
    snapshot.proxyUiState = ProxyUiState::Transitioning;
    snapshot.systemProxyMode = SystemProxyMode::ForcedChange;
    snapshot.systemProxyApplied = true;
    snapshot.autoRunEnabled = true;
    snapshot.routingSummary = QStringLiteral("Global");
    snapshot.listenSummary = QStringLiteral("127.0.0.1:1080");

    state.applySnapshot(snapshot);

    QCOMPARE(currentServerSpy.count(), 1);
    QCOMPARE(currentServerSpy.at(0).at(2).toString(), QStringLiteral("Please verify manually"));
    QCOMPARE(proxyUiStateSpy.count(), 1);
    QCOMPARE(systemProxySpy.count(), 1);
    QCOMPARE(autoRunSpy.count(), 1);
    QCOMPARE(routingSpy.count(), 1);

    state.applySnapshot(snapshot);

    QCOMPARE(currentServerSpy.count(), 1);
    QCOMPARE(proxyUiStateSpy.count(), 1);
    QCOMPARE(systemProxySpy.count(), 1);
    QCOMPARE(autoRunSpy.count(), 1);
    QCOMPARE(routingSpy.count(), 1);

    snapshot.currentServerLocation = QStringLiteral("Japan");
    state.applySnapshot(snapshot);

    QCOMPARE(currentServerSpy.count(), 2);
    QCOMPARE(proxyUiStateSpy.count(), 1);
    QCOMPARE(systemProxySpy.count(), 1);
    QCOMPARE(autoRunSpy.count(), 1);
    QCOMPARE(routingSpy.count(), 1);
    QCOMPARE(currentServerSpy.takeLast().at(1).toString(), QStringLiteral("Japan"));
}
void RuntimeStateTests::undoWarningAloneCountsAsCurrentServerChange()
{
    RuntimeState state;
    QSignalSpy currentServerSpy(&state, &RuntimeState::currentServerSegmentChanged);
    QSignalSpy proxyUiStateSpy(&state, &RuntimeState::proxyUiStateChanged);

    RuntimeStateSnapshot snapshot;
    snapshot.currentServerName = QStringLiteral("HK-01");
    state.applySnapshot(snapshot);
    QCOMPARE(currentServerSpy.count(), 1);

    // The undo warning rides the current-server segment (PR #19). A snapshot whose only
    // semantic change is that warning must still fire the segment signal -- this is exactly
    // the shape of a failed undo on a stopped session, where nothing else moved.
    snapshot.systemProxyUndoWarning = QStringLiteral("Failed to restore your system proxy.");
    state.applySnapshot(snapshot);
    QCOMPARE(currentServerSpy.count(), 2);

    // And an identical replay fires nothing.
    state.applySnapshot(snapshot);
    QCOMPARE(currentServerSpy.count(), 2);
    QCOMPARE(proxyUiStateSpy.count(), 0);
}
QTEST_MAIN(RuntimeStateTests)

#include "RuntimeStateTests.moc"
