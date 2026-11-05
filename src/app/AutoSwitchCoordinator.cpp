#include "app/AutoSwitchCoordinator.h"

#include <QCoreApplication>
#include <QDateTime>

#include <algorithm>
#include <utility>

#include "common/ServerDisplayName.h"

namespace {

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

AutoSwitchCoordinator::AutoSwitchCoordinator(Dependencies dependencies, QObject* parent)
    : QObject(parent)
    , deps_(std::move(dependencies))
{
    // Ticking forever is deliberate: the tick's guards are three cheap predicate calls, and a
    // timer that some other component has to remember to start and stop is a timer that the
    // next refactor leaves stopped.
    warmTimer_.setInterval(AutoSwitchGroupLogic::kWarmProbeIntervalMs);
    connect(&warmTimer_, &QTimer::timeout, this, &AutoSwitchCoordinator::runWarmRound);
    warmTimer_.start();
}

AutoSwitchGroupLogic::GroupScan AutoSwitchCoordinator::scanCurrentGroup() const
{
    if (!deps_.currentLocationId || !deps_.currentServerId || !deps_.servers) {
        return {};
    }
    return AutoSwitchGroupLogic::scanLocationGroup(
        deps_.servers(),
        deps_.currentLocationId(),
        deps_.currentServerId());
}

QString AutoSwitchCoordinator::displayNameFor(const QString& indexId) const
{
    if (deps_.servers) {
        const QList<VmessItem> servers = deps_.servers();
        const auto server = std::find_if(
            servers.cbegin(), servers.cend(),
            [&indexId](const VmessItem& item) { return item.indexId.trimmed() == indexId.trimmed(); });
        if (server != servers.cend()) {
            return serverDisplayName(*server);
        }
    }
    return indexId;
}

void AutoSwitchCoordinator::handleSessionFailure()
{
    if (!deps_.autoSwitchEnabled || !deps_.autoSwitchEnabled()) {
        return;
    }
    // The user may have pressed START in the window between the failure and this decision;
    // their choice outranks the automatic one.
    if (deps_.proxyOccupied && deps_.proxyOccupied()) {
        return;
    }
    const AutoSwitchGroupLogic::GroupScan scan = scanCurrentGroup();
    if (scan.memberIds.isEmpty()) {
        // No selection or an empty group: nothing the feature promised to manage. Silent, the
        // same way the pre-coordinator implementation was -- the failure itself was already
        // reported by the session.
        return;
    }
    if (scan.failedIndex < 0) {
        // The failed server is not in the group being browsed: not this group's failure.
        return;
    }
    if (scan.memberIds.size() < 2) {
        if (deps_.appendResult) {
            deps_.appendResult(OperationResult::ok(QCoreApplication::translate(
                "AppBootstrap",
                "Auto switch skipped: the current group has no other node.")));
        }
        return;
    }
    if (AutoSwitchGroupLogic::reachedSwitchCeiling(consecutiveSwitches_, scan.memberIds.size())) {
        // Deliberately not reset here: the streak stays exhausted until an activation succeeds
        // (noteSessionActivated), which is exactly what the message promises.
        if (deps_.appendResult) {
            deps_.appendResult(OperationResult::ok(QCoreApplication::translate(
                "AppBootstrap",
                "Auto switch stopped: every node in the group has failed. Switching resumes after a successful connection.")));
        }
        return;
    }

    const QString failedId = scan.memberIds.at(scan.failedIndex);
    const QString warmTarget = AutoSwitchGroupLogic::pickWarmSwitchTarget(
        scan.memberIds, failedId, warm_, nowMs());
    if (!warmTarget.isEmpty()) {
        const auto measured = warm_.constFind(warmTarget);
        const qint64 ageSeconds = measured == warm_.constEnd()
            ? 0
            : qMax<qint64>(0, (nowMs() - measured->measuredAtMs) / 1000);
        const QString latency = measured == warm_.constEnd() || measured->latencyMs < 0
            ? QString()
            : QStringLiteral("%1 ms, ").arg(qRound(measured->latencyMs));
        executeSwitch(
            warmTarget,
            QCoreApplication::translate(
                "AppBootstrap",
                "Auto switch in group: switching to \"%1\" (%2measured %3 s ago) after the previous node failed.")
                .arg(displayNameFor(warmTarget), latency, QString::number(ageSeconds)));
        return;
    }

    // No warm answer. Probe the group once, then decide -- unless a probe is already deciding
    // this failure, in which case this one has nothing to add.
    if (failoverProbePending_) {
        return;
    }
    startFailoverProbe(scan);
}

void AutoSwitchCoordinator::startFailoverProbe(const AutoSwitchGroupLogic::GroupScan& scan)
{
    if (!deps_.runProbe || !deps_.buildProbeItems || !deps_.servers) {
        return;
    }

    QStringList rankableIds;
    const QList<VmessItem> servers = deps_.servers();
    for (const QString& id : scan.memberIds) {
        const auto server = std::find_if(
            servers.cbegin(), servers.cend(),
            [&id](const VmessItem& item) { return item.indexId.trimmed() == id; });
        if (server != servers.cend() && AutoSwitchGroupLogic::isProbeRankable(*server)) {
            rankableIds.append(id);
        }
    }

    const QList<SpeedTestRequestItem> items = deps_.buildProbeItems(rankableIds);
    if (!items.isEmpty()) {
        if (deps_.appendResult) {
            deps_.appendResult(OperationResult::ok(QCoreApplication::translate(
                "AppBootstrap",
                "Auto switch in group: probing %1 node(s) for a reachable target...").arg(items.size())));
        }

        failoverProbePending_ = true;
        const bool started = deps_.runProbe(
            this,
            items,
            [this]() {
                // The runner delivers completion through a context-queued path whose context is
                // this coordinator (the ProbeRunner contract); posting to this rather than
                // calling through keeps a destroyed coordinator out of the call -- the
                // async-owner guard the repo's check-async-owners enforces.
                QMetaObject::invokeMethod(
                    this, &AutoSwitchCoordinator::finishFailoverProbe, Qt::QueuedConnection);
            });
        if (started) {
            return;
        }
        failoverProbePending_ = false;
        // Could not start (probe channel busy): fall through to the round robin below.
    }

    const int next = AutoSwitchGroupLogic::roundRobinFallbackIndex(
        scan.failedIndex, scan.memberIds.size());
    if (next >= 0) {
        const QString target = scan.memberIds.at(next);
        executeSwitch(
            target,
            QCoreApplication::translate(
                "AppBootstrap",
                "Auto switch in group: retrying on \"%1\" after the previous node failed.")
                .arg(displayNameFor(target)));
    }
}

void AutoSwitchCoordinator::finishFailoverProbe()
{
    failoverProbePending_ = false;

    // The user may have started (or be starting) a proxy while the probe ran: their choice
    // outranks a decision about a failure they may have already answered.
    if (deps_.proxyOccupied && deps_.proxyOccupied()) {
        return;
    }
    if (!deps_.autoSwitchEnabled || !deps_.autoSwitchEnabled()) {
        return;
    }

    const AutoSwitchGroupLogic::GroupScan scan = scanCurrentGroup();
    if (scan.memberIds.size() < 2 || scan.failedIndex < 0) {
        return;
    }
    if (AutoSwitchGroupLogic::reachedSwitchCeiling(consecutiveSwitches_, scan.memberIds.size())) {
        return;
    }

    const QString failedId = scan.memberIds.at(scan.failedIndex);
    const QString target = AutoSwitchGroupLogic::pickWarmSwitchTarget(
        scan.memberIds, failedId, warm_, nowMs());
    if (!target.isEmpty()) {
        executeSwitch(
            target,
            QCoreApplication::translate(
                "AppBootstrap",
                "Auto switch in group: switching to \"%1\", the best reachable node after probing.")
                .arg(displayNameFor(target)));
        return;
    }

    const int next = AutoSwitchGroupLogic::roundRobinFallbackIndex(
        scan.failedIndex, scan.memberIds.size());
    if (next >= 0) {
        const QString fallback = scan.memberIds.at(next);
        executeSwitch(
            fallback,
            QCoreApplication::translate(
                "AppBootstrap",
                "Auto switch in group: no node answered the probe; retrying on \"%1\".")
                .arg(displayNameFor(fallback)));
    }
}

void AutoSwitchCoordinator::runWarmRound()
{
    if (failoverProbePending_) {
        return;
    }
    if (!deps_.autoSwitchEnabled || !deps_.autoSwitchEnabled()) {
        return;
    }
    if (!deps_.sessionProxying || !deps_.sessionProxying()) {
        return;
    }
    if (!deps_.runProbe || !deps_.buildProbeItems || !deps_.servers) {
        return;
    }

    const AutoSwitchGroupLogic::GroupScan scan = scanCurrentGroup();
    if (scan.memberIds.size() < 2) {
        return;
    }

    QStringList rankableIds;
    const QList<VmessItem> servers = deps_.servers();
    for (const QString& id : scan.memberIds) {
        const auto server = std::find_if(
            servers.cbegin(), servers.cend(),
            [&id](const VmessItem& item) { return item.indexId.trimmed() == id; });
        if (server != servers.cend() && AutoSwitchGroupLogic::isProbeRankable(*server)) {
            rankableIds.append(id);
        }
    }
    const qint64 now = nowMs();
    // Prune against the whole group, not just the probe-rankable subset: a Custom node can
    // still carry a measurement from the user's own test batch, and that knowledge should
    // survive as long as the node is in the group.
    AutoSwitchGroupLogic::pruneWarmResults(warm_, scan.memberIds, now);
    const QStringList candidates = AutoSwitchGroupLogic::pickWarmProbeCandidates(
        rankableIds, warm_, now);
    if (candidates.isEmpty()) {
        return;
    }
    const QList<SpeedTestRequestItem> items = deps_.buildProbeItems(candidates);
    if (items.isEmpty()) {
        return;
    }

    // Quiet by design: the round is bookkeeping, not a user action, and a log line every few
    // minutes would bury the lines the user actually reads. The measurements stay in the warm
    // data (SpeedTestCoordinator's table/persistence path is token-gated to the user's own
    // batches); what the user sees of them is the failure path's "measured N s ago" line.
    deps_.runProbe(this, items, []() {});
}

void AutoSwitchCoordinator::noteSessionActivated()
{
    consecutiveSwitches_ = 0;
    runWarmRound();
}

void AutoSwitchCoordinator::handleProbeResultReady(const QString& indexId, const QString& result)
{
    // The map is bounded, not just its decisions: stale entries are dropped on every harvest,
    // and runWarmRound() also drops keys that no longer belong to the current group. A
    // subscription update can recycle an indexId for a different server; the inherited latency
    // dies with the TTL at worst, and the key leaves the hash on the next warm round.
    const qint64 now = nowMs();
    AutoSwitchGroupLogic::pruneWarmResults(warm_, {}, now);
    AutoSwitchGroupLogic::harvestProbeResult(warm_, indexId, result, now);
}

void AutoSwitchCoordinator::executeSwitch(const QString& indexId, const QString& reason)
{
    ++consecutiveSwitches_;
    if (deps_.appendResult) {
        deps_.appendResult(OperationResult::ok(reason));
    }
    if (deps_.switchToServer) {
        deps_.switchToServer(indexId);
    }
}
