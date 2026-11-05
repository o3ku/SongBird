#pragma once

#include <QString>
#include <QStringList>

#include "common/SubscriptionAutoUpdateInterval.h"
#include "domain/models/Config.h"
#include "domain/models/SubItem.h"

// The pure half of scheduled subscription refresh: which subscriptions a timer tick should
// update. Free of QObject, timer and I/O so a test needs neither a config file nor a network --
// the same discipline as AutoSwitchGroupLogic and SpeedTestCoordinatorLogic. The orchestration
// half is AppBootstrap::scheduleSubscriptionAutoUpdate (the tick) plus
// SubscriptionWorkflowCoordinator::updateByIds (the refresh itself, reused verbatim from the
// manual path: serial batches, conditional requests, retry/failStreak handling). A
// subscription that keeps failing is retried on an exponentially thinning set of ticks, so a
// dead URL does not cost a full download round every five minutes for the life of the process.

namespace SubscriptionAutoUpdateLogic {

// How often the tick asks "is anything due?". One tick granularity coarser than the smallest
// interval keeps the check itself unnoticeable; the due computation is what decides.
constexpr int kCheckIntervalMs = 5 * 60 * 1000;
// The startup delay before the first check: late enough that the startup overlay, first
// subscription render and auto proxy preference have all settled, early enough that a stale
// subscription set is refreshed in the same sitting the user started the app for.
constexpr int kFirstCheckDelayMs = 15 * 1000;
// A failing subscription is retried on every 2^min(failStreak, kMaxBackoffStreak)-th tick:
// streak 1 -> every 2nd tick (10 min), 2 -> every 4th (20 min), ... capped at every 64th
// tick (~5 h 20 min). A permanently dead URL therefore costs one download round per few hours
// instead of one every five minutes. The cap keeps a long-broken subscription reachable again
// eventually without an unbounded exponent.
constexpr int kMaxBackoffStreak = 6;

// The number of ticks a failing subscription waits between attempts. Pure so the due
// computation stays unit-testable with an injected clock and no config file or timer.
inline qint64 backoffTickPeriod(int failStreak)
{
    const int capped = failStreak < 0
        ? 0
        : (failStreak > kMaxBackoffStreak ? kMaxBackoffStreak : failStreak);
    return qint64(1) << capped;
}

// The interval clamp lives in common/ (SubscriptionAutoUpdateInterval.h) so the settings page
// and this half share the exact two numbers; re-exported here so callers of the due computation
// need one include.
using SubscriptionAutoUpdateInterval::kMaximumHours;
using SubscriptionAutoUpdateInterval::kMinimumHours;
using SubscriptionAutoUpdateInterval::normalizedHours;
constexpr int kMinimumIntervalHours = kMinimumHours;
constexpr int kMaximumIntervalHours = kMaximumHours;
inline int normalizedIntervalHours(int intervalHours)
{
    return normalizedHours(intervalHours);
}

// The subscriptions a tick should refresh, in list order. Empty when the feature is off or
// nothing is due. A subscription that has never been updated (lastUpdatedAtMs == 0) is due
// immediately: enabling the feature baselines every subscription once, which is also what makes
// it recover a subscription set that went stale while the feature was off.
inline QStringList dueSubscriptionIds(const Config& config, qint64 nowMs)
{
    QStringList due;
    if (!config.subscriptionAutoUpdateEnabled) {
        return due;
    }
    const qint64 intervalMs = qint64(normalizedIntervalHours(config.subscriptionAutoUpdateIntervalHours)) * 60 * 60 * 1000;
    // Ticks are 5 minutes apart and first fired at kFirstCheckDelayMs, so their wall-clock
    // index advances by exactly one each time. Deriving the retry schedule from that index
    // (rather than from a persisted "last attempt" field) survives a restart and needs no new
    // config state: a URL that has failed N times simply waits 2^N ticks before the next try.
    // A streak of zero keeps the original behaviour -- due as soon as the interval elapses.
    const qint64 tickIndex = nowMs / kCheckIntervalMs;
    for (const SubItem& item : config.collection().subscriptions) {
        if (!item.enabled || item.url.trimmed().isEmpty()) {
            continue;
        }
        if (item.failStreak > 0 && tickIndex % backoffTickPeriod(item.failStreak) != 0) {
            continue;
        }
        if (item.lastUpdatedAtMs <= 0 || nowMs - item.lastUpdatedAtMs >= intervalMs) {
            const QString id = item.id.trimmed();
            if (!id.isEmpty() && !due.contains(id)) {
                due.append(id);
            }
        }
    }
    return due;
}

} // namespace SubscriptionAutoUpdateLogic
