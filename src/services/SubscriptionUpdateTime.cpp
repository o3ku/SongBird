#include "services/SubscriptionUpdateTime.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QStringList>

namespace {

constexpr qint64 kMinuteMs = 60LL * 1000LL;
constexpr qint64 kHourMs = 60LL * kMinuteMs;
constexpr qint64 kDayMs = 24LL * kHourMs;


} // namespace

qint64 SubscriptionUpdateTime::ageMinutes(qint64 updatedAtMs, qint64 nowMs)
{
    if (updatedAtMs <= 0 || nowMs <= updatedAtMs) {
        return 0;
    }
    return (nowMs - updatedAtMs) / kMinuteMs;
}

QString SubscriptionUpdateTime::relativeLabel(qint64 updatedAtMs, qint64 nowMs)
{
    if (updatedAtMs <= 0) {
        return QCoreApplication::translate("SubscriptionUpdateTime", "Never updated");
    }

    const qint64 ageMs = nowMs > updatedAtMs ? nowMs - updatedAtMs : 0;
    if (ageMs < kMinuteMs) {
        return QCoreApplication::translate("SubscriptionUpdateTime", "Just now");
    }
    if (ageMs < kHourMs) {
        return QCoreApplication::translate("SubscriptionUpdateTime", "%1 min ago").arg(ageMs / kMinuteMs);
    }
    if (ageMs < kDayMs) {
        return QCoreApplication::translate("SubscriptionUpdateTime", "%1 h ago").arg(ageMs / kHourMs);
    }
    return QCoreApplication::translate("SubscriptionUpdateTime", "%1 d ago").arg(ageMs / kDayMs);
}

QString SubscriptionUpdateTime::absoluteLabel(qint64 updatedAtMs)
{
    if (updatedAtMs <= 0) {
        return QCoreApplication::translate("SubscriptionUpdateTime", "Never updated");
    }
    // Local time on purpose: the user compares it against the clock in the corner, not
    // against a UTC header in a download log.
    return QDateTime::fromMSecsSinceEpoch(updatedAtMs).toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString SubscriptionUpdateTime::tooltipText(
    const QString& displayName,
    qint64 updatedAtMs,
    qint64 nowMs,
    const QString& networkError)
{
    if (updatedAtMs <= 0 && networkError.trimmed().isEmpty()) {
        return {};
    }

    QStringList lines;
    if (!displayName.trimmed().isEmpty()) {
        lines.append(displayName.trimmed());
    }
    if (updatedAtMs > 0) {
        lines.append(
            QCoreApplication::translate("SubscriptionUpdateTime", "Updated %1 (%2)")
                .arg(relativeLabel(updatedAtMs, nowMs), absoluteLabel(updatedAtMs)));
    }
    if (!networkError.trimmed().isEmpty()) {
        // `networkError` is the service's own line, and every one of those already leads with the
        // subscription name ("Airport download failed: ..."). Labelling it here as well produced
        // "Airport" / "Last attempt failed: Airport download failed: timeout" -- the name twice,
        // the second time buried mid-sentence. The line under the header is already unambiguously
        // about this subscription, so the reason is shown as written.
        lines.append(networkError.trimmed());
    }
    return lines.join(QStringLiteral("\n"));
}

QString SubscriptionUpdateTime::cellText(qint64 updatedAtMs, qint64 nowMs)
{
    if (updatedAtMs <= 0) {
        return QStringLiteral("-");
    }
    return relativeLabel(updatedAtMs, nowMs);
}
