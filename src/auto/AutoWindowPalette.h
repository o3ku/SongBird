#pragma once

#include <QString>

// The SongBirdAuto window's own palette.
//
// Why it is separate from ui/theme/AppThemePalette: the auto front end deliberately looks
// different from the main window. It is a small, always-on-top control panel with one big
// action button, and it uses a blue accent where the main app uses green -- that is a design
// choice, not an oversight, and folding it into the shared palette would either change how it
// looks or give the main window an accent it does not want.
//
// Why it exists at all: these colours used to be written inline in a QSS string literal inside
// applyCompactStyle(), which made SongBirdAutoWindow.cpp the only file outside
// AppThemePalette.cpp that carried raw hex. That is the exact shape a colour guard has to reject
// (a value in a conditional branch, or in a stylesheet a runtime probe never builds, is invisible
// to every test), so the values moved here and the stylesheet interpolates them.
//
// Adding a colour to this struct is cheap. Writing a hex literal in a .cpp file is what the
// guard exists to stop, because the next person cannot tell whether it was deliberate.
namespace AutoThemeInternal {

struct AutoThemePalette {
    const char* background;
    const char* text;
    const char* textMuted;
    // Two quieter steps used only by the table header and the status bar. They are close to
    // textMuted (#4a4a52) but not equal, and collapsing them changed both surfaces' contrast.
    const char* textHeader;
    const char* textStatus;
    const char* textSubtle;
    const char* surface;
    // A push button at rest. Distinct from surfaceMuted (#f0f0f4), which is the scrollbar trough
    // and header background: the two were the same field for one careless minute and that alone
    // made every button in this window lighter.
    const char* control;
    const char* surfaceMuted;
    const char* surfaceHover;
    const char* surfaceDisabled;
    const char* border;
    const char* borderMuted;
    const char* accent;
    const char* accentPending;
    const char* accentToggle;
    const char* danger;
    const char* selection;
    const char* scrollbar;
    const char* scrollbarHover;
};

// The single palette this front end has. A function rather than a constant so a future variant
// (a dark auto window, say) has somewhere to go without moving every call site.
const AutoThemePalette& currentPalette();

// Turns a stored colour into the value a stylesheet wants. Kept here so the styling code never
// has to know whether the palette is a literal or derived.
QString color(const char* value);

// The label colour drawn on top of an accent or a danger fill. Exposed as a function rather than a
// struct field because it is the one value that must be re-chosen if the accent ever becomes pale.
const char* onAccentColor();

} // namespace AutoThemeInternal
