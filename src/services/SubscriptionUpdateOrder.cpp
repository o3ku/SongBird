#include "services/SubscriptionUpdateOrder.h"

#include <QHash>
#include <QUrl>

#include <algorithm>

namespace {

// One host's pending subscriptions, in the order the caller supplied them.
struct HostQueue {
    QString host;
    QList<SubItem> items;
    // The highest failStreak in the queue decides where the whole queue sits in the round-robin.
    // Using the max rather than the sum keeps a single dead URL from dragging a healthy host to the
    // back when its sibling URL still works.
    int worstFailStreak = 0;
};

} // namespace

namespace SubscriptionUpdateOrder {

QString hostKey(const QString& url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QUrl parsed = QUrl::fromUserInput(trimmed);
    if (!parsed.isValid() || parsed.host().isEmpty()) {
        return {};
    }

    // Port deliberately dropped: two subscription URLs on the same host but different ports still
    // hit one machine, and the point of grouping is to avoid bursting that machine.
    return parsed.host().toLower();
}

QList<SubItem> orderForUpdate(const QList<SubItem>& items)
{
    if (items.size() <= 1) {
        return items;
    }

    // Group by host, preserving first-appearance order so the round-robin is deterministic for a
    // given input. An empty host key (unparseable URL) gets a unique synthetic key so such entries
    // never share a queue with each other or with a real host.
    QHash<QString, int> queueIndexByHost;
    QList<HostQueue> queues;
    int anonymousCounter = 0;
    for (const SubItem& item : items) {
        QString host = hostKey(item.url);
        if (host.isEmpty()) {
            host = QStringLiteral("#anonymous-%1").arg(anonymousCounter++);
        }

        auto it = queueIndexByHost.find(host);
        if (it == queueIndexByHost.end()) {
            HostQueue queue;
            queue.host = host;
            queue.worstFailStreak = item.failStreak;
            queue.items.append(item);
            queues.append(queue);
            queueIndexByHost.insert(host, queues.size() - 1);
            continue;
        }

        HostQueue& queue = queues[it.value()];
        queue.worstFailStreak = std::max(queue.worstFailStreak, item.failStreak);
        queue.items.append(item);
    }

    // Healthy hosts first. std::stable_sort so two hosts with the same worst failStreak keep the
    // caller's original order rather than an arbitrary one.
    std::stable_sort(queues.begin(), queues.end(), [](const HostQueue& left, const HostQueue& right) {
        return left.worstFailStreak < right.worstFailStreak;
    });

    // Round-robin: take the n-th subscription from every queue before taking any queue's (n+1)-th.
    // This is what separates two URLs on one host -- they can never be adjacent unless the batch has
    // only that one host, which no ordering could fix.
    QList<SubItem> ordered;
    ordered.reserve(items.size());
    int pass = 0;
    bool appended = true;
    while (appended) {
        appended = false;
        for (const HostQueue& queue : queues) {
            if (pass < queue.items.size()) {
                ordered.append(queue.items.at(pass));
                appended = true;
            }
        }
        ++pass;
    }

    return ordered;
}

} // namespace SubscriptionUpdateOrder
