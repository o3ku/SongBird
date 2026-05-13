#pragma once

inline bool shouldCleanupTunAfterCoreStop(bool runningOnWindows, bool tunEnabledAtCoreStart)
{
    return runningOnWindows && tunEnabledAtCoreStart;
}
