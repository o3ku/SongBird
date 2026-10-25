#pragma once

#include <QString>

#include "domain/models/SubscriptionUsage.h"

struct SubItem {
    QString id;
    QString remarks;
    QString url;
    bool enabled = true;
    QString userAgent;

    // HTTP caching headers from the last successful download. When present they
    // are sent as If-None-Match / If-Modified-Since on the next request so the
    // server can respond with 304 Not Modified and save bandwidth.
    QString etag;
    QString lastModified;

    // Consecutive download failures since the last success. Reset to 0 after
    // any successful download (including 304). Used by the retry logic to
    // decide the per-attempt back-off interval.
    int failStreak = 0;

    SubscriptionUsage usage;
};
