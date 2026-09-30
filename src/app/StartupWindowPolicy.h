#pragma once

// What the main window should do once the startup sequence reaches the point
// where the UI can be presented. Kept as a pure decision so the tray-less
// fallback (the branch that used to be an untested nested if inside
// AppBootstrap::run()) stays covered without driving a real window or tray.
enum class StartupWindowAction {
    ShowNormally,
    HideToTray,
    ShowMinimizedWithoutTray,
};

inline StartupWindowAction decideStartupWindowAction(bool hideRequested, bool trayAvailable)
{
    if (!hideRequested) {
        return StartupWindowAction::ShowNormally;
    }

    return trayAvailable ? StartupWindowAction::HideToTray
                         : StartupWindowAction::ShowMinimizedWithoutTray;
}
