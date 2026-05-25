#pragma once

#include <QString>
#include <QStringView>

namespace AppThemeInternal {

inline constexpr auto kThemeLight = "Light";
inline constexpr auto kThemeDark = "Dark";

struct ThemePalette {
    const char* background;
    const char* surface;
    const char* toolbarSurface;
    const char* surfaceRaised;
    const char* surfaceMuted;
    const char* surfaceSubtle;
    const char* sectionHeader;
    const char* sectionBody;
    const char* control;
    const char* controlHover;
    const char* controlDisabled;
    const char* dialogButtonDisabled;
    const char* border;
    const char* borderMuted;
    const char* borderStrong;
    const char* buttonBorder;
    const char* buttonBorderHover;
    const char* toolbarButtonBorder;
    const char* runtimeDisabledBorder;
    const char* inputBorderHover;
    const char* subtleButtonBorder;
    const char* disabledBorder;
    const char* dialogButtonDisabledBorder;
    const char* panelBorder;
    const char* toolbarBorder;
    const char* focus;
    const char* text;
    const char* textMuted;
    const char* textSubtle;
    const char* accent;
    const char* accentSoft;
    const char* accentHover;
    const char* accentPressed;
    const char* activeBackground;
    const char* qrPressedBackground;
    const char* hover;
    const char* selection;
    const char* success;
    const char* warning;
    const char* error;
    const char* neutral;
    const char* validationError;
    const char* overlay;
    const char* loadingContent;
    const char* link;
    const char* scrollbar;
    const char* scrollbarHover;
    const char* menuSeparator;
    const char* splitter;
    const char* splitterHover;
    const char* serverSelectedBackground;
    const char* serverCurrentBackground;
    const char* selectionText;
    const char* tabSelectedText;
    const char* downArrowImage;
    const char* upArrowImage;
};

const ThemePalette& paletteForName(QStringView themeName);
const ThemePalette& currentPalette();
void setCurrentThemeName(QStringView themeName);
QString color(const char* value);

} // namespace AppThemeInternal
