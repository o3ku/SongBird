#pragma once

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
