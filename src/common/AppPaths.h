#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QString>

// Paths that belong to the installation rather than to the user.
//
// Everything the application derives from its own location resolves through here. Keeping it in one
// place means the set of files that move with the executable can be read off a single header instead
// of being recovered by grepping for `applicationDirPath()`, and a test that has to stay out of the
// real installation directory has exactly one thing to look at.
//
// These are the *defaults*. Code that a test has to be able to redirect takes the directory as a
// parameter rather than calling in here (see `validateCoreGeoFilesBeforeStart`), because a mutable
// global would leak from one test case into the next.
namespace AppPaths {

// The directory the executable runs from. This is the one place the app asks Qt for it.
inline QString applicationDirectory()
{
    return QCoreApplication::applicationDirPath();
}

// Generated core configurations, the core pid file and the auxiliary TUN config live here.
inline QString runtimeDirectory()
{
    return QDir(applicationDirectory()).filePath(QStringLiteral("runtime"));
}

} // namespace AppPaths
