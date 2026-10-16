#pragma once

#include <QString>

#include "platform/IThemePreferenceService.h"

// Stand-in for platforms with no light/dark preference to read. It reports
// "unknown" (empty string), which callers resolve to "keep the current theme",
// so a "follow system" selection degrades to the default theme instead of
// failing to start.
class NullThemePreferenceService : public IThemePreferenceService {
public:
    QString preferredThemeName() const override { return {}; }
};
