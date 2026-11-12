#include <QtTest>

#include <QCoreApplication>
#include <QDateTime>

#include <algorithm>
#include <functional>

#include "app/AutoSwitchCoordinator.h"
#include "app/AutoSwitchGroupLogic.h"
#include "app/ServerActivationGestureLogic.h"
#include "appcore/LocationGroupScan.h"

namespace {

VmessItem makeServer(
    const QString& indexId,
    const QString& remarks,
    const QString& locationOverride = {},
    bool favorite = false,
    ConfigType configType = ConfigType::VMess)
{
    VmessItem server;
    server.indexId = indexId;
    server.remarks = remarks;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    server.configType = configType;
    server.locationOverride = locationOverride;
    server.favorite = favorite;
    return server;
}

// The scripted environment the coordinator decides against. Every dependency is a plain field
// or recording lambda, so a test reads like the scenario it describes: set the group, fail a
// node, deliver the probe's answers, assert the switch.
struct FakeEnvironment {
    bool enabled = true;
    QString location = QStringLiteral("US");
    QString currentServer;
    QList<VmessItem> servers;
    bool proxying = false;
    bool occupied = false;

    int probeStartCount = 0;
    bool probeStartResult = true;
    QStringList probedIds;
    std::function<void()> pendingFinish;

    QStringList switchedTo;
    QStringList results;

    AutoSwitchCoordinator::Dependencies dependencies()
    {
        AutoSwitchCoordinator::Dependencies deps;
        deps.autoSwitchEnabled = [this]() { return enabled; };
        deps.currentLocationId = [this]() { return location; };
        deps.currentServerId = [this]() { return currentServer; };
        deps.servers = [this]() { return servers; };
        deps.sessionProxying = [this]() { return proxying; };
        deps.proxyOccupied = [this]() { return occupied; };
        deps.buildProbeItems = [](const QStringList& indexIds) {
            QList<SpeedTestRequestItem> items;
            for (const QString& id : indexIds) {
                items.append(SpeedTestRequestItem{id, id, ConfigType::VMess, VmessItem{}, CoreInfo{}});
            }
            return items;
        };
        deps.runProbe = [this](QObject*, const QList<SpeedTestRequestItem>& items, std::function<void()> onFinished) {
            ++probeStartCount;
            for (const SpeedTestRequestItem& item : items) {
                probedIds.append(item.indexId);
            }
            pendingFinish = std::move(onFinished);
            return probeStartResult;
        };
        deps.switchToServer = [this](const QString& indexId) { switchedTo.append(indexId); };
        deps.appendResult = [this](const OperationResult& result) { results.append(result.message); };
        return deps;
    }

    std::unique_ptr<AutoSwitchCoordinator> makeCoordinator()
    {
        return std::make_unique<AutoSwitchCoordinator>(dependencies());
    }
};


} // namespace

// What is pinned here:
//
//   * the pure decisions (AutoSwitchGroupLogic): group membership, probe rankability, what a
//     finished probe says, the warm target choice, the warm round's candidate list, the
//     round robin and the exhaustion ceiling;
//   * the coordinator's sequencing (with a scripted probe): guards, warm-hit switch without a
//     probe, probe-then-switch, both round-robin fallbacks, the ceiling, and the reset on
//     activation.
class AutoSwitchGroupTests : public QObject {
    Q_OBJECT

private slots:
    // Logic
    void groupScanResolvesFavoriteAndLocationGroups();
    void customConfigsAreNotProbeRankable();
    void harvestRecordsLatencyAndUnreachableAnswers();
    void pruneWarmResultsBoundsTheMapOverTime();
    void pickWarmSwitchTargetPrefersFreshLowestLatency();
    void pickWarmProbeCandidatesRankFreshReachableThenCap();
    void roundRobinAndCeilingArithmetic();
    void locationGroupScanEmbedsOnlyNonCustomMembers();
    void activationGestureFromStoppedActivatesTheNode();
    void activationGestureInGroupSessionSwitchesNoOpsAndFallsBack();
    void activationGestureTreatsAFavoritedGroupMemberAsSameGroup();

    // Coordinator
    // Coordinator
    void failureIsIgnoredWhenPreferenceOff();
    void failureOutsideTheGroupDoesNotSwitch();
    void singleMemberGroupReportsSkip();
    void warmTargetSwitchesWithoutProbing();
    void coldFailureProbesThenSwitchesToBestAnswer();
    void busyProbeChannelFallsBackToRoundRobin();
    void unansweredProbeFallsBackToRoundRobin();
    void exhaustedGroupStopsSwitching();
    void activationResetsTheStreakAndKicksAWarmRound();
    void probeCompletionDefersToUserActivation();
    void warmRoundNeedsAProxyingSession();
};

void AutoSwitchGroupTests::groupScanResolvesFavoriteAndLocationGroups()
{
    QList<VmessItem> servers;
    // Name says US and is a favorite; a second node named US but overridden to JP (the
    // override is the user's hand and wins); a plain JP node; an unmarked relay.
    servers.append(makeServer(QStringLiteral("us-fav"), QStringLiteral("🇺🇸 US 01"), {}, true));
    servers.append(makeServer(QStringLiteral("moved"), QStringLiteral("🇺🇸 US 02"), QStringLiteral("JP")));
    servers.append(makeServer(QStringLiteral("jp-1"), QStringLiteral("🇯🇵 JP 01")));
    servers.append(makeServer(QStringLiteral("mystery"), QStringLiteral("Mystery Relay")));

    const AutoSwitchGroupLogic::GroupScan us =
        AutoSwitchGroupLogic::scanLocationGroup(servers, QStringLiteral("US"), QStringLiteral("us-fav"));
    QCOMPARE(us.memberIds, QStringList({QStringLiteral("us-fav")}));
    QCOMPARE(us.failedIndex, 0);

    const AutoSwitchGroupLogic::GroupScan jp =
        AutoSwitchGroupLogic::scanLocationGroup(servers, QStringLiteral("JP"), QStringLiteral("moved"));
    QCOMPARE(jp.memberIds, QStringList({QStringLiteral("moved"), QStringLiteral("jp-1")}));
    QCOMPARE(jp.failedIndex, 0);

    const AutoSwitchGroupLogic::GroupScan favorite =
        AutoSwitchGroupLogic::scanLocationGroup(servers, QStringLiteral("FAVORITE"), QString());
    QCOMPARE(favorite.memberIds, QStringList({QStringLiteral("us-fav")}));
    QCOMPARE(favorite.failedIndex, -1);

    // A failure on a server outside the browsed group is not that group's to fix.
    const AutoSwitchGroupLogic::GroupScan foreign =
        AutoSwitchGroupLogic::scanLocationGroup(servers, QStringLiteral("JP"), QStringLiteral("us-fav"));
    QCOMPARE(foreign.memberIds.size(), 2);
    QCOMPARE(foreign.failedIndex, -1);
}

void AutoSwitchGroupTests::customConfigsAreNotProbeRankable()
{
    QVERIFY(AutoSwitchGroupLogic::isProbeRankable(makeServer(QStringLiteral("a"), QStringLiteral("A"))));
    QVERIFY(!AutoSwitchGroupLogic::isProbeRankable(
        makeServer(QStringLiteral("c"), QStringLiteral("C"), {}, false, ConfigType::Custom)));
    QVERIFY(!AutoSwitchGroupLogic::isProbeRankable(makeServer(QStringLiteral("  "), QStringLiteral("B"))));
}

void AutoSwitchGroupTests::harvestRecordsLatencyAndUnreachableAnswers()
{
    AutoSwitchGroupLogic::WarmProbeMap warm;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    AutoSwitchGroupLogic::harvestProbeResult(warm, QStringLiteral("ok"), QStringLiteral("123 ms"), now);
    AutoSwitchGroupLogic::harvestProbeResult(warm, QStringLiteral("ok-decorated"), QStringLiteral("Accessible 45 ms"), now);
    AutoSwitchGroupLogic::harvestProbeResult(warm, QStringLiteral("timeout"), QStringLiteral("Timeout"), now);
    AutoSwitchGroupLogic::harvestProbeResult(warm, QStringLiteral("blocked"), QStringLiteral("Blocked"), now);

    QCOMPARE(warm.value(QStringLiteral("ok")).latencyMs, 123.0);
    QCOMPARE(warm.value(QStringLiteral("ok-decorated")).latencyMs, 45.0);
    QVERIFY(warm.value(QStringLiteral("timeout")).latencyMs < 0);
    QVERIFY(warm.value(QStringLiteral("blocked")).latencyMs < 0);
    // Both answers carry a timestamp: probed-and-unreachable is knowledge, not absence.
    QCOMPARE(warm.value(QStringLiteral("timeout")).measuredAtMs, now);
}

void AutoSwitchGroupTests::pruneWarmResultsBoundsTheMapOverTime()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 ttl = AutoSwitchGroupLogic::kWarmResultTtlMs;
    AutoSwitchGroupLogic::WarmProbeMap warm;

    // A long-lived session keeps harvesting recycled ids: the map must not grow without bound.
    warm.insert(QStringLiteral("fresh-in-group"), {80.0, now});
    warm.insert(QStringLiteral("stale-in-group"), {100.0, now - ttl - 1000});
    warm.insert(QStringLiteral("fresh-orphan"), {50.0, now});
    warm.insert(QStringLiteral("stale-orphan"), {60.0, now - ttl - 1000});

    const QStringList active{QStringLiteral("fresh-in-group"), QStringLiteral("stale-in-group")};
    AutoSwitchGroupLogic::pruneWarmResults(warm, active, now);

    QCOMPARE(warm.size(), 1);
    QVERIFY(warm.contains(QStringLiteral("fresh-in-group")));

    // With no member set (the harvest-time call) only the time bound applies, so a key that is
    // still fresh survives until the next warm round names the group's real members.
    warm.clear();
    warm.insert(QStringLiteral("fresh"), {80.0, now});
    warm.insert(QStringLiteral("old"), {100.0, now - ttl - 1000});
    AutoSwitchGroupLogic::pruneWarmResults(warm, {}, now);
    QCOMPARE(warm.size(), 1);
    QVERIFY(warm.contains(QStringLiteral("fresh")));
}

void AutoSwitchGroupTests::pickWarmSwitchTargetPrefersFreshLowestLatency()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 ttl = AutoSwitchGroupLogic::kWarmResultTtlMs;

    AutoSwitchGroupLogic::WarmProbeMap warm;
    warm.insert(QStringLiteral("slow-fresh"), {120.0, now - 60 * 1000});
    warm.insert(QStringLiteral("fast-fresh"), {80.0, now - 10 * 1000});
    warm.insert(QStringLiteral("probed-dead"), {-1.0, now - 5 * 1000});
    warm.insert(QStringLiteral("stale"), {90.0, now - ttl - 1000});
    warm.insert(QStringLiteral("tie-first"), {90.0, now - 30 * 1000});
    warm.insert(QStringLiteral("tie-second"), {90.0, now - 1 * 1000});

    const QStringList members{
        QStringLiteral("slow-fresh"), QStringLiteral("fast-fresh"), QStringLiteral("probed-dead"),
        QStringLiteral("stale"), QStringLiteral("tie-first"), QStringLiteral("tie-second"),
        QStringLiteral("unmeasured")};

    // The failed node is never the answer; the best fresh reachable member wins.
    QCOMPARE(
        AutoSwitchGroupLogic::pickWarmSwitchTarget(members, QStringLiteral("unmeasured"), warm, now),
        QStringLiteral("fast-fresh"));
    // Ties keep the earlier list position.
    QCOMPARE(
        AutoSwitchGroupLogic::pickWarmSwitchTarget(members, QStringLiteral("fast-fresh"), warm, now),
        QStringLiteral("tie-first"));
    // Everything fresh is either the failed node or dead, and nothing else is measured.
    QStringList onlyDead;
    onlyDead << QStringLiteral("fast-fresh") << QStringLiteral("probed-dead");
    warm.clear();
    warm.insert(QStringLiteral("fast-fresh"), {80.0, now});
    warm.insert(QStringLiteral("probed-dead"), {-1.0, now});
    QVERIFY(AutoSwitchGroupLogic::pickWarmSwitchTarget(onlyDead, QStringLiteral("fast-fresh"), warm, now).isEmpty());
}

void AutoSwitchGroupTests::pickWarmProbeCandidatesRankFreshReachableThenCap()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    AutoSwitchGroupLogic::WarmProbeMap warm;
    warm.insert(QStringLiteral("m2"), {300.0, now});
    warm.insert(QStringLiteral("m5"), {100.0, now});

    const QStringList members{
        QStringLiteral("m1"), QStringLiteral("m2"), QStringLiteral("m3"),
        QStringLiteral("m4"), QStringLiteral("m5"), QStringLiteral("m6")};

    // Fresh reachable first by latency, then the unmeasured in list order, capped at three.
    QCOMPARE(
        AutoSwitchGroupLogic::pickWarmProbeCandidates(members, warm, now, 3),
        QStringList({QStringLiteral("m5"), QStringLiteral("m2"), QStringLiteral("m1")}));
}

void AutoSwitchGroupTests::roundRobinAndCeilingArithmetic()
{
    QCOMPARE(AutoSwitchGroupLogic::roundRobinFallbackIndex(1, 3), 2);
    QCOMPARE(AutoSwitchGroupLogic::roundRobinFallbackIndex(2, 3), 0);
    QCOMPARE(AutoSwitchGroupLogic::roundRobinFallbackIndex(0, 1), -1);

    QVERIFY(!AutoSwitchGroupLogic::reachedSwitchCeiling(0, 2));
    QVERIFY(AutoSwitchGroupLogic::reachedSwitchCeiling(1, 2));
    QVERIFY(!AutoSwitchGroupLogic::reachedSwitchCeiling(1, 3));
    QVERIFY(AutoSwitchGroupLogic::reachedSwitchCeiling(2, 3));
    QVERIFY(!AutoSwitchGroupLogic::reachedSwitchCeiling(8, 10));
    QVERIFY(AutoSwitchGroupLogic::reachedSwitchCeiling(9, 10));
}

void AutoSwitchGroupTests::locationGroupScanEmbedsOnlyNonCustomMembers()
{
    const QList<VmessItem> servers{
        makeServer(QStringLiteral("us-1"), QStringLiteral("us 1"), QStringLiteral("US")),
        makeServer(QStringLiteral("custom"), QStringLiteral("custom"), QStringLiteral("US"), false, ConfigType::Custom),
        makeServer(QStringLiteral("jp-1"), QStringLiteral("jp 1"), QStringLiteral("JP")),
        makeServer(QStringLiteral("fav-1"), QStringLiteral("fav 1"), QStringLiteral("JP"), true),
    };

    const QList<VmessItem> us = LocationGroupScan::embeddableMembers(servers, QStringLiteral("US"));
    QCOMPARE(us.size(), 1);
    QCOMPARE(us.first().indexId, QStringLiteral("us-1"));

    const QList<VmessItem> favorite = LocationGroupScan::embeddableMembers(servers, QStringLiteral("FAVORITE"));
    QCOMPARE(favorite.size(), 1);
    QCOMPARE(favorite.first().indexId, QStringLiteral("fav-1"));

    QCOMPARE(LocationGroupScan::locationIdOfServer(servers.first()), QStringLiteral("US"));
    QCOMPARE(LocationGroupScan::locationIdOfServer(servers.last()), QStringLiteral("FAVORITE"));
}

void AutoSwitchGroupTests::activationGestureFromStoppedActivatesTheNode()
{
    const QList<VmessItem> servers{
        makeServer(QStringLiteral("us-1"), QStringLiteral("us 1"), QStringLiteral("US")),
        makeServer(QStringLiteral("jp-1"), QStringLiteral("jp 1"), QStringLiteral("JP")),
    };

    // Nothing runs, and a stale group target must not turn the click into a group switch:
    // activation from stopped keeps the user's node.
    const auto stopped = ServerActivationGestureLogic::resolveServerActivationGesture(
        false, QStringLiteral("US"), servers, QStringLiteral("jp-1"));
    QCOMPARE(static_cast<int>(stopped.kind),
             static_cast<int>(ServerActivationGestureLogic::ActivationGesture::Kind::ActivateNode));
    QCOMPARE(stopped.indexId, QStringLiteral("jp-1"));

    // Unknown and empty clicks do nothing.
    const auto unknown = ServerActivationGestureLogic::resolveServerActivationGesture(
        true, QStringLiteral("US"), servers, QStringLiteral("nope"));
    QCOMPARE(static_cast<int>(unknown.kind),
             static_cast<int>(ServerActivationGestureLogic::ActivationGesture::Kind::NoOp));
}

void AutoSwitchGroupTests::activationGestureInGroupSessionSwitchesNoOpsAndFallsBack()
{
    const QList<VmessItem> servers{
        makeServer(QStringLiteral("us-1"), QStringLiteral("us 1"), QStringLiteral("US")),
        makeServer(QStringLiteral("us-2"), QStringLiteral("us 2"), QStringLiteral("US")),
        makeServer(QStringLiteral("jp-1"), QStringLiteral("jp 1"), QStringLiteral("JP")),
        makeServer(QStringLiteral("custom"), QStringLiteral("custom"), QStringLiteral("JP"), false, ConfigType::Custom),
    };

    // Same group: a no-op, so the running session is not restarted for nothing.
    const auto sameGroup = ServerActivationGestureLogic::resolveServerActivationGesture(
        true, QStringLiteral("US"), servers, QStringLiteral("us-2"));
    QCOMPARE(static_cast<int>(sameGroup.kind),
             static_cast<int>(ServerActivationGestureLogic::ActivationGesture::Kind::NoOp));

    // Another group while a group session runs: switch the auto-select to that group.
    const auto otherGroup = ServerActivationGestureLogic::resolveServerActivationGesture(
        true, QStringLiteral("US"), servers, QStringLiteral("jp-1"));
    QCOMPARE(static_cast<int>(otherGroup.kind),
             static_cast<int>(ServerActivationGestureLogic::ActivationGesture::Kind::ActivateGroup));
    QCOMPARE(otherGroup.locationId, QStringLiteral("JP"));

    // A custom config can never join a group, so clicking one is a single-node activation.
    const auto custom = ServerActivationGestureLogic::resolveServerActivationGesture(
        true, QStringLiteral("JP"), servers, QStringLiteral("custom"));
    QCOMPARE(static_cast<int>(custom.kind),
             static_cast<int>(ServerActivationGestureLogic::ActivationGesture::Kind::ActivateNode));
    QCOMPARE(custom.indexId, QStringLiteral("custom"));

    // A single-node session (no group target) keeps the pre-group behavior on every click.
    const auto plain = ServerActivationGestureLogic::resolveServerActivationGesture(
        true, QString(), servers, QStringLiteral("jp-1"));
    QCOMPARE(static_cast<int>(plain.kind),
             static_cast<int>(ServerActivationGestureLogic::ActivationGesture::Kind::ActivateNode));
}

void AutoSwitchGroupTests::activationGestureTreatsAFavoritedGroupMemberAsSameGroup()
{
    // A favorited node is still a member of its location group: it shows in that group's tab and
    // group activation embeds it. Clicking one while that group runs must be a no-op, not a jump
    // to the FAVORITE pseudo-group -- locationIdOfServer resolves FAVORITE first and used to turn
    // the click into a cross-group switch that restarted the session.
    const QList<VmessItem> servers{
        makeServer(QStringLiteral("us-1"), QStringLiteral("us 1"), QStringLiteral("US")),
        makeServer(QStringLiteral("us-fav"), QStringLiteral("us fav"), QStringLiteral("US"), true),
    };

    const auto sameGroup = ServerActivationGestureLogic::resolveServerActivationGesture(
        true, QStringLiteral("US"), servers, QStringLiteral("us-fav"));
    QCOMPARE(static_cast<int>(sameGroup.kind),
             static_cast<int>(ServerActivationGestureLogic::ActivationGesture::Kind::NoOp));

    // The FAVORITE group itself still resolves to FAVORITE for a node outside the running group.
    const auto favoriteGroup = ServerActivationGestureLogic::resolveServerActivationGesture(
        true, QStringLiteral("JP"), servers, QStringLiteral("us-fav"));
    QCOMPARE(static_cast<int>(favoriteGroup.kind),
             static_cast<int>(ServerActivationGestureLogic::ActivationGesture::Kind::ActivateGroup));
    QCOMPARE(favoriteGroup.locationId, QStringLiteral("FAVORITE"));
}

void AutoSwitchGroupTests::failureIsIgnoredWhenPreferenceOff()
{
    FakeEnvironment env;
    env.enabled = false;
    env.currentServer = QStringLiteral("s1");
    env.servers = {makeServer(QStringLiteral("s1"), QStringLiteral("US A")),
                   makeServer(QStringLiteral("s2"), QStringLiteral("US B"))};
    auto coordinator = env.makeCoordinator();

    coordinator->handleSessionFailure();
    QCOMPARE(env.probeStartCount, 0);
    QCOMPARE(env.switchedTo.size(), 0);
}

void AutoSwitchGroupTests::failureOutsideTheGroupDoesNotSwitch()
{
    FakeEnvironment env;
    env.currentServer = QStringLiteral("outsider");
    env.servers = {makeServer(QStringLiteral("s1"), QStringLiteral("US A")),
                   makeServer(QStringLiteral("s2"), QStringLiteral("US B")),
                   makeServer(QStringLiteral("outsider"), QStringLiteral("Mystery"))};
    auto coordinator = env.makeCoordinator();

    coordinator->handleSessionFailure();
    QCOMPARE(env.probeStartCount, 0);
    QCOMPARE(env.switchedTo.size(), 0);
}

void AutoSwitchGroupTests::singleMemberGroupReportsSkip()
{
    FakeEnvironment env;
    env.currentServer = QStringLiteral("s1");
    env.servers = {makeServer(QStringLiteral("s1"), QStringLiteral("US A"))};
    auto coordinator = env.makeCoordinator();

    coordinator->handleSessionFailure();
    QCOMPARE(env.switchedTo.size(), 0);
    QVERIFY(env.results.join(QLatin1Char('\n')).contains(QStringLiteral("no other node")));
}

void AutoSwitchGroupTests::warmTargetSwitchesWithoutProbing()
{
    FakeEnvironment env;
    env.currentServer = QStringLiteral("s1");
    env.servers = {makeServer(QStringLiteral("s1"), QStringLiteral("US A")),
                   makeServer(QStringLiteral("s2"), QStringLiteral("US B")),
                   makeServer(QStringLiteral("s3"), QStringLiteral("US C"))};
    auto coordinator = env.makeCoordinator();

    // A recent "test servers" batch measured the group; the failure path reads it and never
    // starts a probe.
    coordinator->handleProbeResultReady(QStringLiteral("s2"), QStringLiteral("80 ms"));
    coordinator->handleProbeResultReady(QStringLiteral("s3"), QStringLiteral("150 ms"));

    coordinator->handleSessionFailure();
    QCOMPARE(env.probeStartCount, 0);
    QCOMPARE(env.switchedTo, QStringList({QStringLiteral("s2")}));
    QVERIFY(env.results.join(QLatin1Char('\n')).contains(QStringLiteral("US B")));
    QVERIFY(env.results.join(QLatin1Char('\n')).contains(QStringLiteral("80 ms")));
}

void AutoSwitchGroupTests::coldFailureProbesThenSwitchesToBestAnswer()
{
    FakeEnvironment env;
    env.currentServer = QStringLiteral("s1");
    env.servers = {makeServer(QStringLiteral("s1"), QStringLiteral("US A")),
                   makeServer(QStringLiteral("s2"), QStringLiteral("US B")),
                   makeServer(QStringLiteral("s3"), QStringLiteral("US C"))};
    auto coordinator = env.makeCoordinator();

    // No warm data: the failure starts a probe round over the group's rankable members and
    // switches nothing until it answers.
    coordinator->handleSessionFailure();
    QCOMPARE(env.probeStartCount, 1);
    QCOMPARE(env.probedIds, QStringList({QStringLiteral("s1"), QStringLiteral("s2"), QStringLiteral("s3")}));
    QCOMPARE(env.switchedTo.size(), 0);
    QVERIFY(env.pendingFinish != nullptr);

    // The probe's answers arrive as results; the completion then picks the best reachable
    // member (not the round-robin neighbour: s3 measured faster than s2).
    coordinator->handleProbeResultReady(QStringLiteral("s1"), QStringLiteral("Timeout"));
    coordinator->handleProbeResultReady(QStringLiteral("s2"), QStringLiteral("200 ms"));
    coordinator->handleProbeResultReady(QStringLiteral("s3"), QStringLiteral("150 ms"));
    env.pendingFinish();
    // The completion is delivered queued: one event-loop turn lets it land.
    QCoreApplication::processEvents();

    QCOMPARE(env.switchedTo, QStringList({QStringLiteral("s3")}));
    QVERIFY(env.results.join(QLatin1Char('\n')).contains(QStringLiteral("US C")));
}

void AutoSwitchGroupTests::busyProbeChannelFallsBackToRoundRobin()
{
    FakeEnvironment env;
    env.probeStartResult = false;
    env.currentServer = QStringLiteral("s1");
    env.servers = {makeServer(QStringLiteral("s1"), QStringLiteral("US A")),
                   makeServer(QStringLiteral("s2"), QStringLiteral("US B")),
                   makeServer(QStringLiteral("s3"), QStringLiteral("US C"))};
    auto coordinator = env.makeCoordinator();

    coordinator->handleSessionFailure();
    QCOMPARE(env.probeStartCount, 1);
    QCOMPARE(env.switchedTo, QStringList({QStringLiteral("s2")}));
}

void AutoSwitchGroupTests::unansweredProbeFallsBackToRoundRobin()
{
    FakeEnvironment env;
    env.currentServer = QStringLiteral("s1");
    env.servers = {makeServer(QStringLiteral("s1"), QStringLiteral("US A")),
                   makeServer(QStringLiteral("s2"), QStringLiteral("US B")),
                   makeServer(QStringLiteral("s3"), QStringLiteral("US C"))};
    auto coordinator = env.makeCoordinator();

    coordinator->handleSessionFailure();
    QCOMPARE(env.probeStartCount, 1);

    // Every probe answer is a failure text: the round robin answers instead.
    coordinator->handleProbeResultReady(QStringLiteral("s1"), QStringLiteral("Timeout"));
    coordinator->handleProbeResultReady(QStringLiteral("s2"), QStringLiteral("Blocked"));
    coordinator->handleProbeResultReady(QStringLiteral("s3"), QStringLiteral("Timeout"));
    env.pendingFinish();
    QCoreApplication::processEvents();

    QCOMPARE(env.switchedTo, QStringList({QStringLiteral("s2")}));
    QVERIFY(env.results.join(QLatin1Char('\n')).contains(QStringLiteral("no node answered")));
}

void AutoSwitchGroupTests::exhaustedGroupStopsSwitching()
{
    FakeEnvironment env;
    env.probeStartResult = false;
    env.servers = {makeServer(QStringLiteral("s1"), QStringLiteral("US A")),
                   makeServer(QStringLiteral("s2"), QStringLiteral("US B")),
                   makeServer(QStringLiteral("s3"), QStringLiteral("US C"))};
    auto coordinator = env.makeCoordinator();

    env.currentServer = QStringLiteral("s1");
    coordinator->handleSessionFailure();
    QCOMPARE(env.switchedTo, QStringList({QStringLiteral("s2")}));

    env.currentServer = QStringLiteral("s2");
    coordinator->handleSessionFailure();
    QCOMPARE(env.switchedTo, QStringList({QStringLiteral("s2"), QStringLiteral("s3")}));

    // Two switches in a three-member group: every other member has failed once. The streak is
    // exhausted until an activation succeeds.
    env.currentServer = QStringLiteral("s3");
    coordinator->handleSessionFailure();
    QCOMPARE(env.switchedTo.size(), 2);
    QVERIFY(env.results.join(QLatin1Char('\n')).contains(QStringLiteral("stopped")));

    // Still exhausted on the next failure.
    env.currentServer = QStringLiteral("s1");
    coordinator->handleSessionFailure();
    QCOMPARE(env.switchedTo.size(), 2);
}

void AutoSwitchGroupTests::activationResetsTheStreakAndKicksAWarmRound()
{
    FakeEnvironment env;
    env.probeStartResult = false;
    env.servers = {makeServer(QStringLiteral("s1"), QStringLiteral("US A")),
                   makeServer(QStringLiteral("s2"), QStringLiteral("US B")),
                   makeServer(QStringLiteral("s3"), QStringLiteral("US C"))};
    auto coordinator = env.makeCoordinator();

    env.currentServer = QStringLiteral("s1");
    coordinator->handleSessionFailure();
    env.currentServer = QStringLiteral("s2");
    coordinator->handleSessionFailure();
    QCOMPARE(env.switchedTo.size(), 2);

    // The switched-to node connected: the streak ends and, because a session is now proxying,
    // a warm round measures the group (the busy-channel fake records it like any probe).
    env.proxying = true;
    env.probeStartResult = true;
    coordinator->noteSessionActivated();
    QCOMPARE(env.switchedTo.size(), 2);
    QCOMPARE(env.probeStartCount, 3);

    // A later failure can switch again.
    env.probeStartResult = false;
    env.currentServer = QStringLiteral("s3");
    coordinator->handleSessionFailure();
    QCOMPARE(env.switchedTo.size(), 3);
}

void AutoSwitchGroupTests::probeCompletionDefersToUserActivation()
{
    FakeEnvironment env;
    env.currentServer = QStringLiteral("s1");
    env.servers = {makeServer(QStringLiteral("s1"), QStringLiteral("US A")),
                   makeServer(QStringLiteral("s2"), QStringLiteral("US B"))};
    auto coordinator = env.makeCoordinator();

    coordinator->handleSessionFailure();
    QCOMPARE(env.probeStartCount, 1);
    coordinator->handleProbeResultReady(QStringLiteral("s2"), QStringLiteral("50 ms"));

    // The user pressed START while the probe ran: their activation outranks the probe answer.
    env.occupied = true;
    env.pendingFinish();
    QCoreApplication::processEvents();
    QCOMPARE(env.switchedTo.size(), 0);
}

void AutoSwitchGroupTests::warmRoundNeedsAProxyingSession()
{
    FakeEnvironment env;
    env.currentServer = QStringLiteral("s1");
    env.servers = {makeServer(QStringLiteral("s1"), QStringLiteral("US A")),
                   makeServer(QStringLiteral("s2"), QStringLiteral("US B"))};
    auto coordinator = env.makeCoordinator();

    // Not proxying: an activation that immediately failed again must not warm-probe.
    coordinator->noteSessionActivated();
    QCOMPARE(env.probeStartCount, 0);
}

QTEST_MAIN(AutoSwitchGroupTests)
#include "AutoSwitchGroupTests.moc"
