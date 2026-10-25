#pragma once

#include <QtGlobal>

#include <limits>

struct SubscriptionUsage {
    // -1 means the provider did not supply a value; zero is a real value.
    qint64 uploadBytes = -1;
    qint64 downloadBytes = -1;
    qint64 totalBytes = -1;
    qint64 expiresAt = -1;
    qint64 resetsAt = -1;
    qint64 updatedAt = 0;

    qint64 usedBytes() const
    {
        if (uploadBytes < 0 || downloadBytes < 0
            || uploadBytes > std::numeric_limits<qint64>::max() - downloadBytes) {
            return -1;
        }
        return uploadBytes + downloadBytes;
    }
};
