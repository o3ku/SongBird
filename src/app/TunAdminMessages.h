#pragma once

#include <QCoreApplication>
#include <QString>

inline QString tunAdminRequiredSaveMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "TUN mode requires administrator privileges on Windows. The settings were saved and will take effect after restarting SongBird as administrator.");
}

inline QString tunAdminRestartFailureMessage()
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "Failed to restart SongBird with administrator privileges.");
}
