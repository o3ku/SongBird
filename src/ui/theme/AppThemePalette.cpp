#include "ui/theme/AppThemePalette.h"

namespace {

using namespace AppThemeInternal;

constexpr ThemePalette kLightPalette{
    .background = "#f3f5f8",
    .surface = "#ffffff",
    .toolbarSurface = "#ffffff",
    .surfaceRaised = "#ffffff",
    .surfaceMuted = "#eef2f6",
    .surfaceSubtle = "#f8fafc",
    .sectionHeader = "#eef2f6",
    .sectionBody = "#ffffff",
    .control = "#f8fafc",
    .controlHover = "#eef2f6",
    .controlDisabled = "#eef2f6",
    .dialogButtonDisabled = "#eef2f6",
    .border = "#c5ced8",
    .borderMuted = "#dde3ea",
    .borderStrong = "#8b98a8",
    .buttonBorder = "#c5ced8",
    .buttonBorderHover = "#8b98a8",
    .toolbarButtonBorder = "#dde3ea",
    .runtimeDisabledBorder = "#c5ced8",
    .inputBorderHover = "#8b98a8",
    .subtleButtonBorder = "#c5ced8",
    .disabledBorder = "#dde3ea",
    .dialogButtonDisabledBorder = "#dde3ea",
    .panelBorder = "#d8dee7",
    .toolbarBorder = "#d8dee7",
    .focus = "#2f6d56",
    .text = "#1f2328",
    .textMuted = "#667085",
    .textSubtle = "#8a94a6",
    .accent = "#2f6d56",
    .accentSoft = "#e5f0eb",
    .accentHover = "#d8e8df",
    .accentPressed = "#e5f0eb",
    .activeBackground = "#2f6d56",
    .qrPressedBackground = "#e5f0eb",
    .hover = "#f2f6f3",
    .selection = "#e6f0eb",
    .success = "#2f6d45",
    .warning = "#8a6a20",
    .error = "#a33a3a",
    .neutral = "#4d5f73",
    .validationError = "#a33a3a",
    .overlay = "rgba(255, 255, 255, 224)",
    .loadingContent = "rgba(255, 255, 255, 216)",
    .link = "#0969da",
    .scrollbar = "#8b98a8",
    .scrollbarHover = "#667085",
    .menuSeparator = "#e4e7ec",
    .splitter = "#d8dee7",
    .splitterHover = "#8b98a8",
    .serverSelectedBackground = "#b7dec9",
    .serverCurrentBackground = "#e1f3e9",
    .selectionText = "#1f2328",
    .tabSelectedText = "#ffffff",
    .downArrowImage = ":/app/down.svg",
    .upArrowImage = ":/app/up.svg"};

constexpr ThemePalette kDarkPalette{
    .background = "#20262d",
    .surface = "#262d35",
    .toolbarSurface = "#242b33",
    .surfaceRaised = "#2a323b",
    .surfaceMuted = "#303944",
    .surfaceSubtle = "#222932",
    .sectionHeader = "#20262d",
    .sectionBody = "#303944",
    .control = "#303944",
    .controlHover = "#394451",
    .controlDisabled = "#29313a",
    .dialogButtonDisabled = "#242b33",
    .border = "#536170",
    .borderMuted = "#46525f",
    .borderStrong = "#7f8d9e",
    .buttonBorder = "#667586",
    .buttonBorderHover = "#93a1b2",
    .toolbarButtonBorder = "#607080",
    .runtimeDisabledBorder = "#46525f",
    .inputBorderHover = "#8391a3",
    .subtleButtonBorder = "#536170",
    .disabledBorder = "#46525f",
    .dialogButtonDisabledBorder = "#3b4652",
    .panelBorder = "#46525f",
    .toolbarBorder = "#4a5664",
    .focus = "#86d49c",
    .text = "#edf1f6",
    .textMuted = "#bac5d1",
    .textSubtle = "#8793a2",
    .accent = "#63b58e",
    .accentSoft = "#264838",
    .accentHover = "#35654f",
    .accentPressed = "#2b5542",
    .activeBackground = "#35654f",
    .qrPressedBackground = "#2b5542",
    .hover = "#26362e",
    .selection = "#264838",
    .success = "#86d49c",
    .warning = "#d8b45f",
    .error = "#f08080",
    .neutral = "#c4ceda",
    .validationError = "#f08080",
    .overlay = "rgba(32, 38, 45, 224)",
    .loadingContent = "rgba(38, 45, 53, 216)",
    .link = "#8bdfff",
    .scrollbar = "#687585",
    .scrollbarHover = "#b8c4d2",
    .menuSeparator = "#3b4652",
    .splitter = "#536170",
    .splitterHover = "#8391a3",
    .serverSelectedBackground = "#4a8b6d",
    .serverCurrentBackground = "#2b4a3a",
    .selectionText = "#edf1f6",
    .tabSelectedText = "#edf1f6",
    .downArrowImage = ":/app/down-dark.svg",
    .upArrowImage = ":/app/up-dark.svg"};

QString currentThemeName = QString::fromLatin1(kThemeLight);

} // namespace

namespace AppThemeInternal {

const ThemePalette& paletteForName(QStringView themeName)
{
    return themeName.compare(QString::fromLatin1(kThemeDark), Qt::CaseInsensitive) == 0
        ? kDarkPalette
        : kLightPalette;
}

const ThemePalette& currentPalette()
{
    return paletteForName(currentThemeName);
}

void setCurrentThemeName(QStringView themeName)
{
    currentThemeName = themeName.toString();
}

QString color(const char* value)
{
    return QString::fromLatin1(value);
}

} // namespace AppThemeInternal
