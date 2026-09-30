#include <QtTest>

#include "app/StartupWindowPolicy.h"
#include "common/StartupSequencer.h"

class StartupSequencerTests : public QObject {
    Q_OBJECT

private slots:
    void runsStepsInDeclarationOrder();
    void reportsStepNamesInDeclarationOrder();
    void abortsAtFirstFailingGateAndSkipsTheRest();
    void stepsAndGatesInterleaveInDeclarationOrder();
    void reportsNoFailureWhenEveryEntrySucceeds();
    void emptySequenceCompletesImmediately();
    void rerunResetsTheExecutionTrace();
    void decideStartupWindowActionFollowsHideRequestAndTrayAvailability();
};

void StartupSequencerTests::runsStepsInDeclarationOrder()
{
    QStringList order;
    StartupSequencer sequencer;
    sequencer.addStep(QStringLiteral("first"), [&order]() { order.append(QStringLiteral("first")); });
    sequencer.addStep(QStringLiteral("second"), [&order]() { order.append(QStringLiteral("second")); });
    sequencer.addStep(QStringLiteral("third"), [&order]() { order.append(QStringLiteral("third")); });

    QVERIFY(sequencer.runAll());
    QCOMPARE(
        order,
        QStringList({QStringLiteral("first"), QStringLiteral("second"), QStringLiteral("third")}));
    QCOMPARE(sequencer.executedStepCount(), 3);
}

void StartupSequencerTests::reportsStepNamesInDeclarationOrder()
{
    StartupSequencer sequencer;
    sequencer.addStep(QStringLiteral("reload-config"), []() {});
    sequencer.addStep(QStringLiteral("present-main-window"), []() {});

    QCOMPARE(sequencer.stepCount(), 2);
    QCOMPARE(
        sequencer.stepNames(),
        QStringList({QStringLiteral("reload-config"), QStringLiteral("present-main-window")}));
}

void StartupSequencerTests::abortsAtFirstFailingGateAndSkipsTheRest()
{
    QStringList visited;
    StartupSequencer sequencer;
    sequencer.addStep(QStringLiteral("cleanup"), [&visited]() { visited.append(QStringLiteral("cleanup")); });
    sequencer.addGate(QStringLiteral("reload-config"), [&visited]() {
        visited.append(QStringLiteral("reload-config"));
        return false;
    });
    sequencer.addStep(QStringLiteral("present-main-window"), [&visited]() {
        visited.append(QStringLiteral("present-main-window"));
    });

    QVERIFY(!sequencer.runAll());
    QCOMPARE(
        visited,
        QStringList({QStringLiteral("cleanup"), QStringLiteral("reload-config")}));
    QCOMPARE(sequencer.executedStepCount(), 2);
    QCOMPARE(sequencer.failedStepName(), QStringLiteral("reload-config"));
}

void StartupSequencerTests::stepsAndGatesInterleaveInDeclarationOrder()
{
    QStringList order;
    StartupSequencer sequencer;
    sequencer.addGate(QStringLiteral("gate-a"), [&order]() {
        order.append(QStringLiteral("gate-a"));
        return true;
    });
    sequencer.addStep(QStringLiteral("step-b"), [&order]() { order.append(QStringLiteral("step-b")); });
    sequencer.addGate(QStringLiteral("gate-c"), [&order]() {
        order.append(QStringLiteral("gate-c"));
        return true;
    });

    QVERIFY(sequencer.runAll());
    QCOMPARE(
        order,
        QStringList({QStringLiteral("gate-a"), QStringLiteral("step-b"), QStringLiteral("gate-c")}));
}

void StartupSequencerTests::reportsNoFailureWhenEveryEntrySucceeds()
{
    StartupSequencer sequencer;
    sequencer.addStep(QStringLiteral("only"), []() {});

    QVERIFY(sequencer.runAll());
    QCOMPARE(sequencer.executedStepCount(), sequencer.stepCount());
    QVERIFY(sequencer.failedStepName().isEmpty());
}

void StartupSequencerTests::emptySequenceCompletesImmediately()
{
    StartupSequencer sequencer;

    QVERIFY(sequencer.runAll());
    QCOMPARE(sequencer.stepCount(), 0);
    QCOMPARE(sequencer.executedStepCount(), 0);
    QVERIFY(sequencer.failedStepName().isEmpty());
}

void StartupSequencerTests::rerunResetsTheExecutionTrace()
{
    StartupSequencer sequencer;
    sequencer.addGate(QStringLiteral("gate"), []() { return false; });
    sequencer.addStep(QStringLiteral("after"), []() {});

    QVERIFY(!sequencer.runAll());
    QCOMPARE(sequencer.executedStepCount(), 1);
    QCOMPARE(sequencer.failedStepName(), QStringLiteral("gate"));

    QVERIFY(!sequencer.runAll());
    QCOMPARE(sequencer.executedStepCount(), 1);
    QCOMPARE(sequencer.failedStepName(), QStringLiteral("gate"));
}

void StartupSequencerTests::decideStartupWindowActionFollowsHideRequestAndTrayAvailability()
{
    QCOMPARE(decideStartupWindowAction(false, true), StartupWindowAction::ShowNormally);
    QCOMPARE(decideStartupWindowAction(false, false), StartupWindowAction::ShowNormally);
    QCOMPARE(decideStartupWindowAction(true, true), StartupWindowAction::HideToTray);
    QCOMPARE(decideStartupWindowAction(true, false), StartupWindowAction::ShowMinimizedWithoutTray);
}

QTEST_MAIN(StartupSequencerTests)
#include "StartupSequencerTests.moc"
