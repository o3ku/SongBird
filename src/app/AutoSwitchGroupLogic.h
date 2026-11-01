#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include "appcore/LocationCatalog.h"
#include "domain/models/VmessItem.h"
#include "services/SpeedTestServiceInternal.h"

// The pure half of "auto switch in group" (AppBootstrap::autoSwitchWithinGroupOnFailure and
// AutoSwitchCoordinator): which servers belong to the selected location group, which of them a
// probe batch can rank, what a finished probe says, and which member the failure path should
// switch to. Kept free of QObject, timer and I/O so a test needs neither a config file nor a
// core -- the same discipline as SpeedTestCoordinatorLogic and AutoCoordinatorLogic.

namespace AutoSwitchGroupLogic {

// How often the warm round re-probes the group's top candidates while a session is proxying.
constexpr int kWarmProbeIntervalMs = 5 * 60 * 1000;
// How long a probe result stays fresh enough to switch on without asking again. Deliberately
// one interval plus slack: a result is at most one tick old when a failure arrives, so this
// bounds only the far tail (a node removed from the group and re-added, a clock jump).
constexpr int kWarmResultTtlMs = 6 * 60 * 1000;
// The warm round probes at most this many nodes per tick. The round exists to keep a few good
// candidates fresh, not to re-rank the whole group every few minutes.
constexpr int kWarmProbeBatchLimit = 5;

struct GroupScan {
    // The group's member ids in config list order -- the order every tie-break below defers to.
    QStringList memberIds;
    // Where the failed server sits in memberIds; -1 when the failure happened on a server
    // outside the group, which is not this group's failure to fix.
    int failedIndex = -1;
};

// FAVORITE is the favorite flag; every other location id is the resolved location (manual
// override first, name inference second -- LocationCatalog::resolvedLocationId).
inline GroupScan scanLocationGroup(
    const QList<VmessItem>& servers,
    const QString& locationId,
    const QString& failedId)
{
    GroupScan scan;
    const QString trimmedLocation = locationId.trimmed();
    const bool favoriteGroup = trimmedLocation == QLatin1String("FAVORITE");
    for (const VmessItem& server : servers) {
        const QString id = server.indexId.trimmed();
        if (id.isEmpty()) {
            continue;
        }
        const bool inGroup = favoriteGroup
            ? server.favorite
            : LocationCatalog::resolvedLocationId(server) == trimmedLocation;
        if (!inGroup) {
            continue;
        }
        if (!failedId.trimmed().isEmpty() && id == failedId.trimmed()) {
            scan.failedIndex = scan.memberIds.size();
        }
        scan.memberIds.append(id);
    }
    return scan;
}

// Custom configs are the one member a probe batch cannot rank (the worker answers "Unsupported"
// without running a core), so the ranking skips them; the round-robin fallback still walks them.
inline bool isProbeRankable(const VmessItem& server)
{
    return server.configType != ConfigType::Custom && !server.indexId.trimmed().isEmpty();
}

// One probe answer per node. latencyMs >= 0 is a reachable measurement; < 0 is probed and
// unreachable -- the distinction that keeps a just-failed probe from being re-picked while an
// unmeasured node (no entry / stale entry) is still worth a probe.
struct WarmProbe {
    double latencyMs = -1;
    qint64 measuredAtMs = 0;
};

using WarmProbeMap = QHash<QString, WarmProbe>;

// Records one finished probe. A parseable latency is a measurement; anything else (timeout,
// error text, "Unsupported") is a probed-and-unreachable answer, not an absence of knowledge.
inline void harvestProbeResult(
    WarmProbeMap& warm,
    const QString& indexId,
    const QString& resultText,
    qint64 nowMs)
{
    const QString id = indexId.trimmed();
    if (id.isEmpty()) {
        return;
    }
    double latencyMs = -1;
    if (!SpeedTestServiceInternal::tryParseUrlProbeLatency(resultText, latencyMs)) {
        latencyMs = -1;
    }
    warm.insert(id, WarmProbe{latencyMs, nowMs});
}

// The failure path's first choice: the freshest-measured, reachable member with the lowest
// latency. The failed server itself is never the answer; ties keep the earlier list position.
// An empty return means the warm data names no target (nothing fresh, or everything fresh has
// failed its probe) and the caller falls back to probing or to the round robin.
inline QString pickWarmSwitchTarget(
    const QStringList& memberIds,
    const QString& failedId,
    const WarmProbeMap& warm,
    qint64 nowMs,
    qint64 ttlMs = kWarmResultTtlMs)
{
    const QString failed = failedId.trimmed();
    QString bestId;
    double bestLatencyMs = -1;
    for (const QString& id : memberIds) {
        if (id == failed) {
            continue;
        }
        const auto entry = warm.constFind(id);
        if (entry == warm.constEnd() || entry->measuredAtMs <= 0) {
            continue;
        }
        if (nowMs - entry->measuredAtMs > ttlMs) {
            continue;
        }
        if (entry->latencyMs < 0) {
            continue;
        }
        if (bestId.isEmpty() || entry->latencyMs < bestLatencyMs) {
            bestId = id;
            bestLatencyMs = entry->latencyMs;
        }
    }
    return bestId;
}

// The warm round's probe list: fresh reachable members first by latency, then everyone else in
// list order (unmeasured, stale, or last-probed-unreachable all stay worth re-probing), capped
// at maxCount. The currently connected server is not excluded -- knowing it degraded is exactly
// what makes the next failure's choice informed.
inline QStringList pickWarmProbeCandidates(
    const QStringList& memberIds,
    const WarmProbeMap& warm,
    qint64 nowMs,
    int maxCount = kWarmProbeBatchLimit,
    qint64 ttlMs = kWarmResultTtlMs)
{
    QStringList ranked;
    for (const QString& id : memberIds) {
        const auto entry = warm.constFind(id);
        const bool freshReachable = entry != warm.constEnd()
            && entry->measuredAtMs > 0
            && nowMs - entry->measuredAtMs <= ttlMs
            && entry->latencyMs >= 0;
        if (!freshReachable) {
            continue;
        }
        auto position = ranked.begin();
        const auto entryOf = [&warm](const QString& candidateId) {
            const auto candidate = warm.constFind(candidateId);
            return candidate == warm.constEnd() ? -1.0 : candidate->latencyMs;
        };
        while (position != ranked.end() && entryOf(*position) <= entry->latencyMs) {
            ++position;
        }
        ranked.insert(position, id);
    }
    for (const QString& id : memberIds) {
        if (!ranked.contains(id)) {
            ranked.append(id);
        }
    }
    while (ranked.size() > maxCount) {
        ranked.removeLast();
    }
    return ranked;
}

// The fallback the feature has always had: the member after the failed one, wrapping, in list
// order. memberCount >= 2 is the caller's precondition (a one-member group has nothing to
// switch to and is reported as skipped before this runs).
inline int roundRobinFallbackIndex(int failedIndex, int memberCount)
{
    return memberCount <= 1 ? -1 : (failedIndex + 1) % memberCount;
}

// The failure chain's brake: after the first failure every other member gets exactly one
// auto-switch before the whole group is declared exhausted. The counter resets on any
// successful activation, so the ceiling measures one unbroken streak of failures.
inline bool reachedSwitchCeiling(int consecutiveSwitches, int memberCount)
{
    return memberCount >= 2 && consecutiveSwitches >= memberCount - 1;
}

} // namespace AutoSwitchGroupLogic
