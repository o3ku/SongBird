#include <QtTest>

#include "app/AppBootstrapUiCommandPlan.h"

class AppBootstrapUiCommandPlanTests : public QObject {
    Q_OBJECT

private slots:
    void defaultServerSelectionRestartsRunningCoreWhenServerChanges();
    void defaultServerSelectionClearsPendingRestartWhenServerUnchanged();
    void defaultServerSelectionEnablesProxyWhenStopped();
    void routingSelectionRestartsOnlyWhenRunningAndChanged();
};

void AppBootstrapUiCommandPlanTests::defaultServerSelectionRestartsRunningCoreWhenServerChanges()
{
    const DefaultServerSelectionPlan plan = evaluateDefaultServerSelectionPlan(
        true,
        QStringLiteral("server-a"),
        QStringLiteral("server-b"),
        true);

    QVERIFY(plan.shouldClearCurrentServerWarning);
    QVERIFY(plan.shouldMarkCurrentActivationPending);
    QVERIFY(plan.shouldRestartRunningCore);
    QVERIFY(!plan.shouldClearCurrentActivationPending);
    QVERIFY(!plan.shouldEnableProxy);
}

void AppBootstrapUiCommandPlanTests::defaultServerSelectionClearsPendingRestartWhenServerUnchanged()
{
    const DefaultServerSelectionPlan plan = evaluateDefaultServerSelectionPlan(
        true,
        QStringLiteral("server-a"),
        QStringLiteral("server-a"),
        true);

    QVERIFY(plan.shouldClearCurrentServerWarning);
    QVERIFY(plan.shouldMarkCurrentActivationPending);
    QVERIFY(!plan.shouldRestartRunningCore);
    QVERIFY(plan.shouldClearCurrentActivationPending);
    QVERIFY(!plan.shouldEnableProxy);
}

void AppBootstrapUiCommandPlanTests::defaultServerSelectionEnablesProxyWhenStopped()
{
    const DefaultServerSelectionPlan plan = evaluateDefaultServerSelectionPlan(
        true,
        QStringLiteral("server-a"),
        QStringLiteral("server-b"),
        false);

    QVERIFY(plan.shouldClearCurrentServerWarning);
    QVERIFY(plan.shouldMarkCurrentActivationPending);
    QVERIFY(!plan.shouldRestartRunningCore);
    QVERIFY(!plan.shouldClearCurrentActivationPending);
    QVERIFY(plan.shouldEnableProxy);
}

void AppBootstrapUiCommandPlanTests::routingSelectionRestartsOnlyWhenRunningAndChanged()
{
    const RoutingSelectionPlan changedPlan = evaluateRoutingSelectionPlan(
        true,
        false,
        true,
        0,
        0,
        true);
    QVERIFY(changedPlan.shouldRestartRunningCore);

    const RoutingSelectionPlan indexPlan = evaluateRoutingSelectionPlan(
        true,
        true,
        true,
        1,
        2,
        true);
    QVERIFY(indexPlan.shouldRestartRunningCore);

    const RoutingSelectionPlan stoppedPlan = evaluateRoutingSelectionPlan(
        true,
        true,
        false,
        1,
        2,
        false);
    QVERIFY(!stoppedPlan.shouldRestartRunningCore);

    const RoutingSelectionPlan failedPlan = evaluateRoutingSelectionPlan(
        false,
        true,
        false,
        1,
        2,
        true);
    QVERIFY(!failedPlan.shouldRestartRunningCore);
}

QTEST_MAIN(AppBootstrapUiCommandPlanTests)

#include "AppBootstrapUiCommandPlanTests.moc"
