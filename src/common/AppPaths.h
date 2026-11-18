#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

namespace AppPaths {

// The directory the executable runs from. This is the one place the app asks Qt for it.
inline QString applicationDirectory()
{
    return QCoreApplication::applicationDirPath();
}

inline QString userDataDirectory()
{
#if defined(Q_OS_MACOS)
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("SongBird"));
#else
    return applicationDirectory();
#endif
}

inline QString dataDirectory()
{
#if defined(Q_OS_MACOS)
    // Keep reading an older installation in place until the updater has migrated its data.
    const QString configName = QStringLiteral("songbird.json");
    if (!QFileInfo::exists(QDir(userDataDirectory()).filePath(configName))
        && QFileInfo::exists(QDir(applicationDirectory()).filePath(configName))) {
        return applicationDirectory();
    }
#endif
    return userDataDirectory();
}

// Generated core configurations, the core pid file and the auxiliary TUN config live here.
inline QString runtimeDirectory()
{
    return QDir(dataDirectory()).filePath(QStringLiteral("runtime"));
}

} // namespace AppPaths
