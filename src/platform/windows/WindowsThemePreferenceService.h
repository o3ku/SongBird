#pragma once

#include <QObject>
#include <QString>

#include "platform/IThemePreferenceService.h"

class QAbstractNativeEventFilter;

// Windows implementation of the OS light/dark preference.
//
// The value lives at HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize
// under AppsUseLightTheme: 1 (or absent) means light, 0 means dark. This is the same
// switch the Settings app flips, so it is the closest thing Windows has to a
// "prefer dark mode" flag for desktop applications.
class WindowsThemePreferenceService : public QObject, public IThemePreferenceService {
    Q_OBJECT

public:
    explicit WindowsThemePreferenceService(QObject* parent = nullptr);
    ~WindowsThemePreferenceService() override;

    QString preferredThemeName() const override;

    // Starts watching for the WM_SETTINGCHANGE broadcast Windows sends when the
    // user flips the mode, so a "follow system" theme can update without a restart.
    // Returns false when the filter could not be installed.
    bool startWatching();

signals:
    // Emitted on the GUI thread after the OS preference changed.
    void preferredThemeChanged();

private:
    class NativeFilter;
    NativeFilter* nativeFilter_ = nullptr;
};
