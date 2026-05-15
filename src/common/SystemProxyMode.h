#pragma once

#include <QString>

enum class SystemProxyMode : int {
    ForcedClear = 0,
    ForcedChange = 1,
    Unchanged = 2,
    Pac = 3
};

inline SystemProxyMode normalizeSystemProxyMode(int value)
{
    switch (value) {
    case 1:
        return SystemProxyMode::ForcedChange;
    case 2:
        return SystemProxyMode::Unchanged;
    case 3:
        return SystemProxyMode::Pac;
    case 0:
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
        return QStringLiteral("Global");
    case SystemProxyMode::Unchanged:
        return QStringLiteral("Unchanged");
    case SystemProxyMode::Pac:
        return QStringLiteral("PAC");
    case SystemProxyMode::ForcedClear:
    default:
        return QStringLiteral("Clear");
    }
}

inline bool expectedSystemProxyEnabled(SystemProxyMode mode)
{
    return mode == SystemProxyMode::ForcedChange || mode == SystemProxyMode::Pac;
}

inline bool shouldAdoptManagedSystemProxyOnStartup(
    SystemProxyMode configuredMode,
    bool persistedProxyEnabled,
    bool systemProxyEnabled)
{
    return expectedSystemProxyEnabled(configuredMode)
        && persistedProxyEnabled
        && systemProxyEnabled;
}

inline bool shouldClearManagedSystemProxy(bool managedSystemProxyActive, bool systemProxyEnabled)
{
    return managedSystemProxyActive && systemProxyEnabled;
}

inline SystemProxyMode resolveSystemProxyModeOnExit(SystemProxyMode currentMode, bool windowsShutdown)
{
    if (windowsShutdown) {
        return SystemProxyMode::ForcedClear;
    }

    switch (currentMode) {
    case SystemProxyMode::Unchanged:
        return SystemProxyMode::Unchanged;
    case SystemProxyMode::ForcedClear:
    case SystemProxyMode::ForcedChange:
    case SystemProxyMode::Pac:
    default:
        return SystemProxyMode::ForcedClear;
    }
}
