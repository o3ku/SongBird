#pragma once

#include <QString>

// Reads the operating system's light/dark preference.
//
// Same shape as the other platform seams (IAutoRunService, ISystemProxyService):
// the callers take the interface, so a test can substitute a stand-in instead of
// depending on the machine it runs on. The Windows implementation reads the
// per-user AppsUseLightTheme registry value; every other platform reports
// unknown, which callers treat as "keep the current theme".
class IThemePreferenceService {
public:
    virtual ~IThemePreferenceService() = default;

    // Returns the preferred theme name (AppTheme's "Light"/"Dark") when the
    // platform exposes one, or an empty string when it does not.
    virtual QString preferredThemeName() const = 0;
};
