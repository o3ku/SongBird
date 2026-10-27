#pragma once

#include <QString>

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

    // Wall-clock time (ms since the epoch) of when this subscription last circulated its
    // bytes, in UTC. Two moments fill this in and they deliberately fill the same field: a
    // full download (the server sent a new document) and a 304 (the server confirmed the copy
    // we already hold is still current). Both answer the question the user asks of this
    // field -- "how stale is what I am showing?" -- and a 304 that stamped nothing would
    // make a subscription that has been checked every hour look untouched for a week.
    //
    // It is written only when the exchange succeeded, so the number is never the time of a
    // failure dressed up as a refresh. 0 means "never".
    qint64 lastUpdatedAtMs = 0;
};
