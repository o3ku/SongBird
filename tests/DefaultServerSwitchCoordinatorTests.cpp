#include <QtTest>

#include <memory>
#include <utility>

#include <QCoreApplication>

#include "app/DefaultServerSwitchCoordinator.h"
#include "common/OperationResult.h"
#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

namespace {

// A two-member location group session: the target is JP, both members carry the override, and
// the anchor (member-0) is the active server -- the shape AppRuntimeResolver::resolveGroupServers
// produces for a group activation.
Config makeGroupSessionConfig()
{
    Config config;
    config.ui().activeLocationGroupId = QStringLiteral("JP");
    VmessItem anchor;
    anchor.indexId = QStringLiteral("member-0");
    anchor.address = QStringLiteral("https://member-0.example");
    anchor.locationOverride = QStringLiteral("JP");
    VmessItem other;
    other.indexId = QStringLiteral("member-1");
    other.address = QStringLiteral("https://member-1.example");
    other.locationOverride = QStringLiteral("JP");
    config.collection().servers = {anchor, other};
    config.currentIndexId = anchor.indexId;
    return config;
}

Config makeSingleNodeConfig()
{
    Config config;
    VmessItem node;
    node.indexId = QStringLiteral("node-1");
    node.address = QStringLiteral("https://node-1.example");
    VmessItem other;
    other.indexId = QStringLiteral("node-2");
    other.address = QStringLiteral("https://node-2.example");
    config.collection().servers = {node, other};
    config.currentIndexId = node.indexId;
    return config;
}

// The coordinator's whole world is its callbacks: the harness drives the config the callbacks
// read and write (mirroring AppBootstrapServerWiring's setDefaultServer -- it clears the group
// target) and records what the coordinator asked the app to do.
class SwitchRecorder {
public:
    DefaultServerSwitchCoordinator::Callbacks callbacks(Config& config)
    {
        DefaultServerSwitchCoordinator::Callbacks callbacks;
        callbacks.currentIndexId = [&config]() { return config.currentIndexId; };
        callbacks.isCoreRunning = [this]() { return coreRunning; };
        callbacks.isActivationInProgress = [this]() { return activationInProgress; };
        callbacks.isTunEnabled = [&config]() { return config.tun().tunModeItem.enableTun; };
        callbacks.isShuttingDown = []() { return false; };
        callbacks.uiContext = []() { return QCoreApplication::instance(); };
        callbacks.lifetimeGuard = [guard = lifetimeGuard]() { return std::weak_ptr<char>(guard); };
        callbacks.setDefaultServer = [&config](const QString& indexId) {
            // The same contract as the composition root's wiring: activating a single node
            // always exits group mode.
            config.ui().activeLocationGroupId.clear();
            config.currentIndexId = indexId;
            return OperationResult::ok(QStringLiteral("Server set."));
        };
        callbacks.activeLocationGroupId = [&config]() { return config.ui().activeLocationGroupId; };
        callbacks.setActiveLocationGroup = [&config](const QString& locationId) {
            config.ui().activeLocationGroupId = locationId.trimmed();
            return OperationResult::ok(QStringLiteral("Group set."));
        };
        callbacks.appendResult = [this](const OperationResult& result) { messages.append(result.message); };
        callbacks.syncWindow = [this]() { ++syncWindowCalls; };
        callbacks.clearServerWarning = [this]() { ++clearServerWarningCalls; };
        callbacks.setCurrentActivationPending = [this](bool pending) {
            pending ? ++activationPendingSetCalls : ++activationPendingClearCalls;
        };
        callbacks.switchRunningCoreToServer = [this](const QString&, bool) { ++switchCoreCalls; };
        callbacks.restartCoreIfRunning = [this](const QString& reason, bool) {
            ++restartCoreCalls;
            restartReasons.append(reason);
        };
        callbacks.enableSystemProxy = [this](bool) { ++enableProxyCalls; };
        callbacks.setTunEnabled = [&config, this](bool enabled) {
            config.tun().tunModeItem.enableTun = enabled;
            ++setTunEnabledCalls;
        };
        callbacks.startProxyAfterSwitch = [this](bool) { ++startAfterSwitchCalls; };
        return callbacks;
    }

    std::shared_ptr<char> lifetimeGuard = std::make_shared<char>();
    bool coreRunning = true;
    bool activationInProgress = false;
    QString log() const { return messages.join(QChar('\n')); }
    QStringList messages;
    QStringList restartReasons;
    int syncWindowCalls = 0;
    int clearServerWarningCalls = 0;
    int activationPendingSetCalls = 0;
    int activationPendingClearCalls = 0;
    int switchCoreCalls = 0;
    int restartCoreCalls = 0;
    int enableProxyCalls = 0;
    int setTunEnabledCalls = 0;
    int startAfterSwitchCalls = 0;
};

} // namespace

// setDefaultServer() used to decide "nothing to reload" purely by comparing node ids, so the
// group-session escape hatch on the anchor itself (Connect as Single Server / Set Current on the
// anchor) cleared the persisted target and then did nothing: the core kept running the
// auto-select group config. Leaving a group session is an activation change on its own.
class DefaultServerSwitchCoordinatorTests : public QObject {
    Q_OBJECT

private slots:
    void leavingAGroupSessionOnTheAnchorReloadsTheCore();
    void leavingAGroupSessionWithTunOnTheAnchorReloadsTheCore();
    void leavingAGroupSessionWithTunOffTurnsTunOnAndLeavesTheRestartToIt();
    void switchingToAnotherServerWhileProxyingStopsAndSwitches();
    void reselectingTheSingleNodeWhileProxyingIsStillANoOp();
    void selectingAServerWhileStoppedStartsTheProxy();
    void queuesASwitchWhileAnActivationIsInFlightAndAppliesItOnSettle();
    void theLatestQueuedSwitchWins();
    void aQueuedGroupSwitchAppliesOnSettle();
    void aUsersStopDiscardsAQueuedSwitch();
    void resumeWithoutAQueuedSwitchDoesNothing();
};

void DefaultServerSwitchCoordinatorTests::leavingAGroupSessionOnTheAnchorReloadsTheCore()
{
    Config config = makeGroupSessionConfig();
    SwitchRecorder recorder;

    DefaultServerSwitchCoordinator coordinator(recorder.callbacks(config));

    // The anchor is the current node, so the id does not change -- only the target does.
    coordinator.setDefaultServer(QStringLiteral("member-0"));

    QVERIFY(config.ui().activeLocationGroupId.isEmpty());
    QCOMPARE(config.currentIndexId, QStringLiteral("member-0"));
    QCOMPARE(recorder.restartCoreCalls, 1);
    QCOMPARE(recorder.enableProxyCalls, 0);
    QCOMPARE(recorder.switchCoreCalls, 0);
}

void DefaultServerSwitchCoordinatorTests::leavingAGroupSessionWithTunOnTheAnchorReloadsTheCore()
{
    Config config = makeGroupSessionConfig();
    config.tun().tunModeItem.enableTun = true;
    SwitchRecorder recorder;

    DefaultServerSwitchCoordinator coordinator(recorder.callbacks(config));

    coordinator.setDefaultServerWithTun(QStringLiteral("member-0"));

    QVERIFY(config.ui().activeLocationGroupId.isEmpty());
    QCOMPARE(recorder.restartCoreCalls, 1);
    QCOMPARE(recorder.setTunEnabledCalls, 0);
}

void DefaultServerSwitchCoordinatorTests::leavingAGroupSessionWithTunOffTurnsTunOnAndLeavesTheRestartToIt()
{
    Config config = makeGroupSessionConfig();
    SwitchRecorder recorder;

    DefaultServerSwitchCoordinator coordinator(recorder.callbacks(config));

    // TUN is off, so the write goes through the TUN toggle; the toggle owns the restart (the
    // core must come back with the TUN inbound), and the next start reads the single-node
    // config this write produced.
    coordinator.setDefaultServerWithTun(QStringLiteral("member-0"));

    QVERIFY(config.ui().activeLocationGroupId.isEmpty());
    QCOMPARE(recorder.setTunEnabledCalls, 1);
    QCOMPARE(recorder.restartCoreCalls, 0);
}

void DefaultServerSwitchCoordinatorTests::switchingToAnotherServerWhileProxyingStopsAndSwitches()
{
    Config config = makeGroupSessionConfig();
    SwitchRecorder recorder;

    DefaultServerSwitchCoordinator coordinator(recorder.callbacks(config));

    // A different node id keeps the stop-and-switch path: the core is stopped and the post-stop
    // resume writes the new default server before the next start.
    coordinator.setDefaultServer(QStringLiteral("member-1"));

    QCOMPARE(recorder.switchCoreCalls, 1);
    QCOMPARE(recorder.restartCoreCalls, 0);
    QCOMPARE(recorder.enableProxyCalls, 0);
}

void DefaultServerSwitchCoordinatorTests::reselectingTheSingleNodeWhileProxyingIsStillANoOp()
{
    Config config = makeSingleNodeConfig();
    SwitchRecorder recorder;

    DefaultServerSwitchCoordinator coordinator(recorder.callbacks(config));

    // Pin the no-op: no group session, same node, core running -- nothing to reload or start.
    coordinator.setDefaultServer(QStringLiteral("node-1"));

    QCOMPARE(recorder.restartCoreCalls, 0);
    QCOMPARE(recorder.switchCoreCalls, 0);
    QCOMPARE(recorder.enableProxyCalls, 0);
}

void DefaultServerSwitchCoordinatorTests::selectingAServerWhileStoppedStartsTheProxy()
{
    Config config = makeGroupSessionConfig();
    SwitchRecorder recorder;
    recorder.coreRunning = false;

    DefaultServerSwitchCoordinator coordinator(recorder.callbacks(config));

    // Stopped, the write only prepares the target; the proxy start is the enable that follows.
    coordinator.setDefaultServer(QStringLiteral("member-0"));

    QVERIFY(config.ui().activeLocationGroupId.isEmpty());
    QCOMPARE(recorder.enableProxyCalls, 1);
    QCOMPARE(recorder.restartCoreCalls, 0);
}

// While an activation is in flight (a start between "requested" and "settled"), the start
// channel belongs to it: a switch asked for now used to fall into the enable path and be
// rejected as an overlapping start, leaving the persisted target desynced from the running
// session until some later restart. The intent is queued and replayed on settle instead.
void DefaultServerSwitchCoordinatorTests::queuesASwitchWhileAnActivationIsInFlightAndAppliesItOnSettle()
{
    Config config = makeGroupSessionConfig();
    SwitchRecorder recorder;
    recorder.coreRunning = false;
    recorder.activationInProgress = true;

    DefaultServerSwitchCoordinator coordinator(recorder.callbacks(config));

    // Nothing may be written or started while the activation owns the channel.
    coordinator.setDefaultServer(QStringLiteral("member-1"));
    QCOMPARE(config.currentIndexId, QStringLiteral("member-0"));
    QVERIFY(!config.ui().activeLocationGroupId.isEmpty());
    QCOMPARE(recorder.enableProxyCalls, 0);
    QCOMPARE(recorder.switchCoreCalls, 0);
    QCOMPARE(recorder.restartCoreCalls, 0);
    QVERIFY(recorder.log().contains(QStringLiteral("applied once it finishes")));

    // The in-flight activation settled (it failed here): the queued switch replays as a fresh
    // click against a stopped session -- write, then start.
    recorder.activationInProgress = false;
    coordinator.resumePendingActivationSwitch();
    QCOMPARE(config.currentIndexId, QStringLiteral("member-1"));
    QVERIFY(config.ui().activeLocationGroupId.isEmpty());
    QCOMPARE(recorder.enableProxyCalls, 1);
}

void DefaultServerSwitchCoordinatorTests::theLatestQueuedSwitchWins()
{
    Config config = makeGroupSessionConfig();
    SwitchRecorder recorder;
    recorder.coreRunning = false;
    recorder.activationInProgress = true;

    DefaultServerSwitchCoordinator coordinator(recorder.callbacks(config));

    coordinator.setDefaultServer(QStringLiteral("member-1"));
    coordinator.setDefaultServer(QStringLiteral("member-0"));
    recorder.activationInProgress = false;
    coordinator.resumePendingActivationSwitch();

    // The second ask replaced the first: the config ends on member-0, exactly one start.
    QCOMPARE(config.currentIndexId, QStringLiteral("member-0"));
    QCOMPARE(recorder.enableProxyCalls, 1);
}

void DefaultServerSwitchCoordinatorTests::aQueuedGroupSwitchAppliesOnSettle()
{
    Config config = makeSingleNodeConfig();
    SwitchRecorder recorder;
    recorder.coreRunning = false;
    recorder.activationInProgress = true;

    DefaultServerSwitchCoordinator coordinator(recorder.callbacks(config));

    coordinator.activateLocationGroup(QStringLiteral("JP"));
    QCOMPARE(config.ui().activeLocationGroupId, QString());

    recorder.activationInProgress = false;
    coordinator.resumePendingActivationSwitch();
    QCOMPARE(config.ui().activeLocationGroupId, QStringLiteral("JP"));
    QCOMPARE(recorder.enableProxyCalls, 1);
}

void DefaultServerSwitchCoordinatorTests::aUsersStopDiscardsAQueuedSwitch()
{
    Config config = makeGroupSessionConfig();
    SwitchRecorder recorder;
    recorder.coreRunning = false;
    recorder.activationInProgress = true;

    DefaultServerSwitchCoordinator coordinator(recorder.callbacks(config));

    coordinator.setDefaultServer(QStringLiteral("member-1"));
    // The user's own stop outranks the queued intent: replaying it would start the proxy they
    // just asked to have off. The later settle must therefore find nothing to apply.
    coordinator.discardPendingActivationSwitch();
    recorder.activationInProgress = false;
    coordinator.resumePendingActivationSwitch();

    QCOMPARE(config.currentIndexId, QStringLiteral("member-0"));
    QCOMPARE(recorder.enableProxyCalls, 0);
}

void DefaultServerSwitchCoordinatorTests::resumeWithoutAQueuedSwitchDoesNothing()
{
    Config config = makeGroupSessionConfig();
    SwitchRecorder recorder;

    DefaultServerSwitchCoordinator coordinator(recorder.callbacks(config));

    coordinator.resumePendingActivationSwitch();
    coordinator.discardPendingActivationSwitch();

    QCOMPARE(config.currentIndexId, QStringLiteral("member-0"));
    QCOMPARE(recorder.enableProxyCalls, 0);
    QCOMPARE(recorder.restartCoreCalls, 0);
    QCOMPARE(recorder.switchCoreCalls, 0);
}

QTEST_MAIN(DefaultServerSwitchCoordinatorTests)
#include "DefaultServerSwitchCoordinatorTests.moc"
