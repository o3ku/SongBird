#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>

inline QString resolveDefaultConfigPath()
{
    const QString currentDirectoryPath = QDir::current().filePath(QStringLiteral("songbird.json"));
    if (QFileInfo::exists(currentDirectoryPath)) {
        return currentDirectoryPath;
    }

    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("songbird.json"));
}

inline QString resolveRequestedConfigPath(const QStringList& arguments)
{
    for (int index = 1; index < arguments.size(); ++index) {
        const QString argument = arguments.at(index);
        if (argument == QStringLiteral("--config") && index + 1 < arguments.size()) {
            return arguments.at(index + 1);
        }

        if (argument.startsWith(QStringLiteral("--config="))) {
            return argument.mid(QStringLiteral("--config=").size());
        }
    }

    return resolveDefaultConfigPath();
}


