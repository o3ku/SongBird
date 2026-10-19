#include "auto/AutoWindowPalette.h"

namespace AutoThemeInternal {

namespace {

// The values are the ones the stylesheet already carried, moved here verbatim so this refactor
// changes no pixel. They are grouped the way the stylesheet uses them, not alphabetically, so a
// reader can match a rule to its field.
constexpr AutoThemePalette kAutoLightPalette{
    .background = "#f7f7f8",
    .text = "#161618",
    .textMuted = "#4a4a52",
    .textHeader = "#5c5c66",
    .textStatus = "#6c6c75",
    .textSubtle = "#92929b",
    .surface = "#ffffff",
    .control = "#e8e8ed",
    .surfaceMuted = "#f0f0f4",
    .surfaceHover = "#dedee5",
    .surfaceDisabled = "#eeeef2",
    .border = "#d7d7dd",
    .borderMuted = "#ececf1",
    .accent = "#007aff",
    .accentPending = "#4aa3ff",
    .accentToggle = "#0ea5e9",
    .danger = "#ff3b30",
    .selection = "#d8eaff",
    .scrollbar = "#c5ced8",
    .scrollbarHover = "#aeb9c6",
};

} // namespace

const AutoThemePalette& currentPalette()
{
    return kAutoLightPalette;
}

QString color(const char* value)
{
    return QString::fromLatin1(value);
}

const char* onAccentColor()
{
    // Every accent in kAutoLightPalette is dark enough for white text (the lightest, #4aa3ff, is
    // 2.6:1 against #ffffff -- acceptable for large button text, which is the only place it is
    // used). A future pale accent has to change this line, and that is why it is a function.
    return "#ffffff";
}

} // namespace AutoThemeInternal
