#pragma once

#include <QString>

inline bool shouldCleanupTunAfterCoreStop(bool runningOnWindows, bool tunEnabledAtCoreStart)
{
    return runningOnWindows && tunEnabledAtCoreStart;
}

inline bool shouldResumeCoreStartAfterTunCleanup(
    bool cleanupSucceeded,
    bool startRequested,
    bool shuttingDown,
    bool coreStopPending)
{
    return cleanupSucceeded && startRequested && !shuttingDown && !coreStopPending;
}

inline bool shouldRunPostStopActionAfterTunCleanup(
    bool cleanupSucceeded,
    bool actionRequested,
    bool shuttingDown)
{
    return cleanupSucceeded && actionRequested && !shuttingDown;
}

inline bool isTunAdapterConflictOutput(const QString& line)
{
    const QString normalized = line.toLower();
    return normalized.contains(QStringLiteral("configure tun interface"))
        && normalized.contains(QStringLiteral("cannot create a file when that file already exists"));
}

inline bool shouldRetryAfterTunAdapterConflict(
    bool runningOnWindows,
    bool tunEnabledAtCoreStart,
    bool conflictDetected,
    int retryCount,
    int maxRetries)
{
    return runningOnWindows
        && tunEnabledAtCoreStart
        && conflictDetected
        && retryCount < maxRetries;
}
