#pragma once

#include <QSet>
#include <QString>

#include <algorithm>
#include <optional>

#include "domain/models/SubscriptionUsage.h"

namespace SubscriptionUsageParser {

// Subscription-Userinfo byte counters and Unix timestamps. `reset` is an
// optional provider extension, never inferred from `expire` or the update day.
inline std::optional<SubscriptionUsage> parse(const QString& header, qint64 receivedAt)
{
    SubscriptionUsage usage;
    QSet<QString> seen;
    for (const QString& part : header.split(QLatin1Char(';'))) {
        const int separator = part.indexOf(QLatin1Char('='));
        if (separator < 0) {
            continue;
        }
        const QString key = part.left(separator).trimmed().toLower();
        qint64* destination = nullptr;
        if (key == QStringLiteral("upload")) {
            destination = &usage.uploadBytes;
        } else if (key == QStringLiteral("download")) {
            destination = &usage.downloadBytes;
        } else if (key == QStringLiteral("total")) {
            destination = &usage.totalBytes;
        } else if (key == QStringLiteral("expire")) {
            destination = &usage.expiresAt;
        } else if (key == QStringLiteral("reset")) {
            destination = &usage.resetsAt;
        }
        if (destination == nullptr) {
            continue;
        }

        const QString value = part.mid(separator + 1).trimmed();
        bool ok = false;
        const qint64 number = value.toLongLong(&ok);
        if (!ok || value.isEmpty() || seen.contains(key)
            || !std::all_of(value.cbegin(), value.cend(), [](QChar c) {
                return c >= QLatin1Char('0') && c <= QLatin1Char('9');
            })) {
            return std::nullopt;
        }
        // Qt dates have a wider range than the UI's yyyy-MM-dd representation.
        if ((key == QStringLiteral("expire") || key == QStringLiteral("reset"))
            && number > 253402300799LL) {
            return std::nullopt;
        }
        seen.insert(key);
        *destination = number;
    }
    if (seen.isEmpty()
        || (usage.uploadBytes >= 0 && usage.downloadBytes >= 0 && usage.usedBytes() < 0)) {
        return std::nullopt;
    }
    usage.updatedAt = receivedAt;
    return usage;
}

} // namespace SubscriptionUsageParser
