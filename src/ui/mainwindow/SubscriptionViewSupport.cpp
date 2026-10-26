#include "ui/mainwindow/SubscriptionViewSupport.h"

#include <QCoreApplication>
#include <QStringList>

namespace {

const QString UngroupedTabKey = QStringLiteral("ungrouped");
const QString UnsubscribedSelectionId = QStringLiteral("__unsubscribed__");
const QString SubscriptionTabPrefix = QStringLiteral("sub:");

struct ByteUnit {
    double scale = 1.0;
    QString name = QStringLiteral("B");
};

ByteUnit byteUnit(qint64 bytes)
{
    const QStringList units{
        QStringLiteral("B"), QStringLiteral("KB"), QStringLiteral("MB"),
        QStringLiteral("GB"), QStringLiteral("TB"), QStringLiteral("PB"), QStringLiteral("EB")};
    int index = 0;
    double scale = 1.0;
    while (bytes / scale >= 1000.0 && index + 1 < units.size()) {
        scale *= 1000.0;
        ++index;
    }
    return {scale, units.at(index)};
}

QString byteNumber(qint64 bytes, const ByteUnit& unit)
{
    QString number = QString::number(bytes / unit.scale, 'f', bytes > 0 && bytes / unit.scale < 1.0 ? 2 : 1);
    while (number.endsWith(QLatin1Char('0'))) {
        number.chop(1);
    }
    if (number.endsWith(QLatin1Char('.'))) {
        number.chop(1);
    }
    return number;
}

QString formatBytes(qint64 bytes)
{
    const ByteUnit unit = byteUnit(bytes);
    return byteNumber(bytes, unit) + QLatin1Char(' ') + unit.name;
}

QString trafficSummary(const SubscriptionUsage& usage)
{
    const qint64 used = usage.usedBytes();
    if (used < 0 || usage.totalBytes < 0) {
        return {};
    }
    if (usage.totalBytes == 0) {
        return formatBytes(used) + QLatin1Char('/') + QCoreApplication::translate("SubscriptionViewSupport", "Unlimited");
    }
    const ByteUnit unit = byteUnit(usage.totalBytes);
    if (used > 0 && used / unit.scale < 0.01) {
        return formatBytes(used) + QLatin1Char('/') + formatBytes(usage.totalBytes);
    }
    return byteNumber(used, unit) + QLatin1Char('/') + byteNumber(usage.totalBytes, unit)
        + QLatin1Char(' ') + unit.name;
}

QString dateText(qint64 timestamp, const QDateTime& now)
{
    const QDateTime date = QDateTime::fromSecsSinceEpoch(timestamp).toLocalTime();
    if (timestamp <= 0 || !date.isValid() || date.date().year() > 9999) {
        return QCoreApplication::translate("SubscriptionViewSupport", "Not provided");
    }
    const qint64 days = now.date().daysTo(date.date());
    const QString text = date.toString(QStringLiteral("yyyy-MM-dd"));
    return date > now
        ? QCoreApplication::translate("SubscriptionViewSupport", "%1 (%2 days remaining)").arg(text).arg(days)
        : text;
}

} // namespace

QSet<QString> SubscriptionViewSupport::enabledSubscriptionIds(const QList<SubItem>& subscriptions)
{
    QSet<QString> ids;
    for (const SubItem& item : subscriptions) {
        if (!item.enabled) {
            continue;
        }

        const QString subscriptionId = item.id.trimmed();
        if (!subscriptionId.isEmpty()) {
            ids.insert(subscriptionId);
        }
    }
    return ids;
}

QString SubscriptionViewSupport::tabKeyFromSelectionId(const QString& selectionId)
{
    const QString normalized = selectionId.trimmed();
    if (normalized == UnsubscribedSelectionId
        || normalized.compare(UngroupedTabKey, Qt::CaseInsensitive) == 0) {
        return UngroupedTabKey;
    }

    return normalized.isEmpty()
        ? UngroupedTabKey
        : SubscriptionTabPrefix + normalized;
}

QString SubscriptionViewSupport::persistedSelectionIdFromTabKey(const QString& tabKey)
{
    if (tabKey == UngroupedTabKey) {
        return UnsubscribedSelectionId;
    }

    if (tabKey.startsWith(SubscriptionTabPrefix)) {
        return tabKey.mid(SubscriptionTabPrefix.size());
    }

    return {};
}

QString SubscriptionViewSupport::subscriptionIdFromTabKey(const QString& tabKey)
{
    return tabKey.startsWith(SubscriptionTabPrefix)
        ? tabKey.mid(SubscriptionTabPrefix.size())
        : QString();
}

QString SubscriptionViewSupport::subscriptionDisplayName(const SubItem& item)
{
    const QString remarks = item.remarks.trimmed();
    if (!remarks.isEmpty()) {
        return remarks;
    }

    const QString url = item.url.trimmed();
    if (!url.isEmpty()) {
        return url;
    }

    return item.id.trimmed().isEmpty()
        ? QCoreApplication::translate("SubscriptionViewSupport", "Subscription")
        : item.id.trimmed();
}

bool SubscriptionViewSupport::quotaWarning(const SubItem& item)
{
    return item.usage.totalBytes > 0 && item.usage.usedBytes() >= 0
        && double(item.usage.usedBytes()) / item.usage.totalBytes >= 0.9;
}

QString SubscriptionViewSupport::subscriptionTabLabel(const SubItem& item, const QFontMetrics& metrics)
{
    const int maximumNameWidth = metrics.horizontalAdvance(QString(14, QLatin1Char('M')));
    const QString name = metrics.elidedText(subscriptionDisplayName(item), Qt::ElideRight, maximumNameWidth);
    const QString traffic = trafficSummary(item.usage);
    const QString label = traffic.isEmpty() ? name : name + QStringLiteral(" · ") + traffic;
    return quotaWarning(item) ? QStringLiteral("! ") + label : label;
}

QString SubscriptionViewSupport::subscriptionDetails(const SubItem& item, const QDateTime& now)
{
    const SubscriptionUsage& usage = item.usage;
    QStringList lines{subscriptionDisplayName(item)};
    const qint64 used = usage.usedBytes();
    if (used >= 0) {
        QString usedText = formatBytes(used);
        if (usage.totalBytes > 0) {
            usedText += QStringLiteral(" (%1%)").arg(double(used) * 100.0 / usage.totalBytes, 0, 'f', 1);
        }
        lines.append(QCoreApplication::translate("SubscriptionViewSupport", "Used: %1").arg(usedText));
    }
    if (usage.totalBytes >= 0) {
        lines.append(QCoreApplication::translate("SubscriptionViewSupport", "Total: %1").arg(usage.totalBytes == 0
            ? QCoreApplication::translate("SubscriptionViewSupport", "Unlimited") : formatBytes(usage.totalBytes)));
        if (used >= 0 && usage.totalBytes > 0) {
            lines.append(QCoreApplication::translate("SubscriptionViewSupport", "Remaining: %1").arg(formatBytes(qMax(qint64(0), usage.totalBytes - used))));
        }
    }
    if (usage.uploadBytes >= 0) {
        lines.append(QCoreApplication::translate("SubscriptionViewSupport", "Upload: %1").arg(formatBytes(usage.uploadBytes)));
    }
    if (usage.downloadBytes >= 0) {
        lines.append(QCoreApplication::translate("SubscriptionViewSupport", "Download: %1").arg(formatBytes(usage.downloadBytes)));
    }
    if (used < 0 || usage.totalBytes < 0) {
        lines.append(QCoreApplication::translate("SubscriptionViewSupport", "The subscription did not provide complete traffic information."));
    }
    if (quotaWarning(item)) {
        lines.append(used >= usage.totalBytes ? QCoreApplication::translate("SubscriptionViewSupport", "Traffic quota exhausted.")
                                             : QCoreApplication::translate("SubscriptionViewSupport", "Traffic quota is almost used up."));
    }
    lines.append(QCoreApplication::translate("SubscriptionViewSupport", "Expires: %1").arg(usage.expiresAt == 0
        ? QCoreApplication::translate("SubscriptionViewSupport", "No expiry") : dateText(usage.expiresAt, now)));
    lines.append(QCoreApplication::translate("SubscriptionViewSupport", "Updated: %1").arg(usage.updatedAt > 0
        ? QDateTime::fromSecsSinceEpoch(usage.updatedAt).toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : QCoreApplication::translate("SubscriptionViewSupport", "Not provided")));
    lines.append(QCoreApplication::translate("SubscriptionViewSupport", "Traffic is reported by the provider when the subscription is updated."));
    return lines.join(QLatin1Char('\n'));
}

QString SubscriptionViewSupport::subscriptionToolTip(const SubItem& item)
{
    const SubscriptionUsage& usage = item.usage;
    const qint64 used = usage.usedBytes();
    const QString notProvided = QCoreApplication::translate("SubscriptionViewSupport", "Not provided");
    const QString unlimited = QCoreApplication::translate("SubscriptionViewSupport", "Unlimited");
    QString remaining = notProvided;
    if (usage.totalBytes == 0) {
        remaining = unlimited;
    } else if (usage.totalBytes > 0 && used >= 0) {
        remaining = formatBytes(qMax(qint64(0), usage.totalBytes - used));
    }
    const QStringList lines{
        QCoreApplication::translate("SubscriptionViewSupport", "Used: %1").arg(used >= 0 ? formatBytes(used) : notProvided),
        QCoreApplication::translate("SubscriptionViewSupport", "Total: %1").arg(usage.totalBytes == 0
            ? unlimited : usage.totalBytes > 0 ? formatBytes(usage.totalBytes) : notProvided),
        QCoreApplication::translate("SubscriptionViewSupport", "Remaining: %1").arg(remaining)};
    return QStringLiteral("<qt>%1</qt>").arg(lines.join(QLatin1Char('\n')).toHtmlEscaped().replace(
        QLatin1Char('\n'), QStringLiteral("<br>")));
}
