#pragma once

#include <QString>

enum class SystemProxyMode : int {
    ForcedClear = 0,
    ForcedChange = 1
};

inline SystemProxyMode normalizeSystemProxyMode(int value)
{
    switch (value) {
    case 1:
        return SystemProxyMode::ForcedChange;
    case 0:
    case 2:
    case 3:
    default:
        return SystemProxyMode::ForcedClear;
    }
}

inline int toLegacySystemProxyModeValue(SystemProxyMode mode)
{
    return static_cast<int>(mode);
}

inline QString systemProxyModeDisplayName(SystemProxyMode mode)
{
    switch (mode) {
    case SystemProxyMode::ForcedChange:
        return QStringLiteral("On");
    case SystemProxyMode::ForcedClear:
    default:
        return QStringLiteral("Off");
    }
}

inline bool expectedSystemProxyEnabled(SystemProxyMode mode)
{
    return mode == SystemProxyMode::ForcedChange;
}

inline bool shouldAdoptManagedSystemProxyOnStartup(
    SystemProxyMode configuredMode,
    bool persistedProxyEnabled,
    bool systemProxyEnabled)
{
    return configuredMode == SystemProxyMode::ForcedChange
        && persistedProxyEnabled
        && systemProxyEnabled;
}

inline bool shouldClearManagedSystemProxy(bool managedSystemProxyActive, bool systemProxyEnabled)
{
    return managedSystemProxyActive && systemProxyEnabled;
}

inline SystemProxyMode resolveSystemProxyModeOnExit(SystemProxyMode /*currentMode*/, bool /*windowsShutdown*/)
{
    return SystemProxyMode::ForcedClear;
}
