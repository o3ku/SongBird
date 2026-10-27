#include "services/SubscriptionUpdateFailureSummary.h"

#include "services/SubscriptionUpdateOrder.h"

namespace {

const QString kFailPrefix = QStringLiteral("[FAIL]");
const QString kNotModifiedSuffix = QStringLiteral("(not modified).");
const QString kCancelledSuffix = QStringLiteral("update cancelled.");

// Two service outcomes are written as [FAIL] lines but are not reasons to show on a tab: a 304
// means the check succeeded and there was nothing new, and a subscription cancelled by shutdown
// says nothing about the subscription. Matched on the tail the line ends with, so a renamed reason
// in the service shows up as a wrongly-shown reason rather than as a hidden one.
bool isReasonTheUserCannotActOn(const QString& detail)
{
    return detail.endsWith(kNotModifiedSuffix) || detail.endsWith(kCancelledSuffix);
}

QList<SubItem> candidatesFrom(const Config& config, const QStringList& subscriptionIds)
{
    QList<SubItem> candidates;
    for (const SubItem& item : config.collection().subscriptions) {
        if (!subscriptionIds.isEmpty() && !subscriptionIds.contains(item.id.trimmed())) {
            continue;
        }
        candidates.append(item);
    }
    return candidates;
}

} // namespace

QString SubscriptionUpdateFailureSummary::displayName(const SubItem& item)
{
    const QString remarks = item.remarks.trimmed();
    return remarks.isEmpty() ? item.url.trimmed() : remarks;
}

QHash<QString, QString> SubscriptionUpdateFailureSummary::perSubscription(
    const Config& config,
    const QStringList& subscriptionIds,
    const QString& message)
{
    QHash<QString, QString> failures;
    if (message.trimmed().isEmpty()) {
        return failures;
    }

    // The service writes the lines in SubscriptionUpdateOrder order, not in config order, so the
    // candidates have to be permuted the same way before a first-wins match means anything.
    const QList<SubItem> candidates = SubscriptionUpdateOrder::orderForUpdate(candidatesFrom(config, subscriptionIds));

    QList<bool> matched;
    matched.reserve(candidates.size());
    for (int index = 0; index < candidates.size(); ++index) {
        matched.append(false);
    }

    // Every line is considered, and the best match wins rather than the first one. Two reasons:
    // the service's two spellings of a failure ("is disabled." / "download failed: ...") separate
    // the name from the reason in different ways, and a name test that only knew one of them would
    // quietly drop those reasons; and a name that is a prefix of another must not claim the longer
    // one's line. So each candidate keeps its best-ranked unmatched line -- exact and counted
    // beats a raw prefix -- and only afterwards claims it.
    const QStringList lines = message.split(QChar('\n'), Qt::KeepEmptyParts);
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (!line.startsWith(kFailPrefix)) {
            continue;
        }
        const QString detail = line.mid(kFailPrefix.size()).trimmed();
        if (detail.isEmpty()) {
            continue;
        }

        int bestIndex = -1;
        int bestRank = -1;
        for (int index = 0; index < candidates.size(); ++index) {
            if (matched.at(index)) {
                continue;
            }
            const QString ownedName = displayName(candidates.at(index));
            if (ownedName.isEmpty()) {
                continue;
            }
            for (const QString& separator : {QStringLiteral(": "), QStringLiteral(" ")}) {
                const QString prefix = ownedName + separator;
                // A name that runs into `(` is the not-modified/cancelled wording, which puts the
                // name in front of the parenthesis and gives no reason -- nothing to show.
                if (!detail.startsWith(prefix) && detail != ownedName) {
                    continue;
                }
                if (isReasonTheUserCannotActOn(detail)) {
                    continue;
                }
                const int rank = separator.size() == 2 ? 2 : 1;
                if (rank > bestRank) {
                    bestRank = rank;
                    bestIndex = index;
                }
            }
        }

        if (bestIndex >= 0) {
            matched[bestIndex] = true;
            failures.insert(candidates.at(bestIndex).id.trimmed(), detail);
        }
    }

    return failures;
}

QHash<QString, QString> SubscriptionUpdateFailureSummary::perSubscriptionFromMessage(
    const Config& config,
    const QStringList& subscriptionIds,
    const OperationResult& result)
{
    const QList<SubItem> candidates = candidatesFrom(config, subscriptionIds);
    QHash<QString, QString> failures;
    for (int index = 0; index < result.subjectFailures.size(); ++index) {
        const QString id = result.subjectFailures.at(index).id.trimmed();
        if (id.isEmpty()) {
            continue;
        }
        bool known = candidates.isEmpty();
        for (const SubItem& item : candidates) {
            if (item.id.trimmed() == id) {
                known = true;
                break;
            }
        }
        if (!known) {
            continue;
        }
        const QString reason = result.subjectFailures.at(index).reason.trimmed();
        if (reason.isEmpty() || isReasonTheUserCannotActOn(reason)) {
            continue;
        }
        failures.insert(id, reason);
    }

    if (!failures.isEmpty() || result.success || !result.subjectFailures.isEmpty()) {
        return failures;
    }
    return perSubscription(config, subscriptionIds, result.message);
}
