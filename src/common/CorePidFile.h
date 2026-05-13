#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QString>
#include <QTextStream>

inline QString corePidFilePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("runtime/core.pid"));
}

inline QSet<qint64> readCorePids()
{
    QSet<qint64> pids;
    QFile file(corePidFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return pids;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        bool ok = false;
        const qint64 pid = line.toLongLong(&ok);
        if (ok && pid > 0) {
            pids.insert(pid);
        }
    }
    return pids;
}

inline void writeCorePids(const QSet<qint64>& pids)
{
    const QString path = corePidFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return;
    }

    QTextStream stream(&file);
    for (qint64 pid : pids) {
        stream << pid << '\n';
    }
}

inline void recordCorePid(qint64 pid)
{
    if (pid <= 0) {
        return;
    }
    QSet<qint64> pids = readCorePids();
    pids.insert(pid);
    writeCorePids(pids);
}

inline void removeCorePid(qint64 pid)
{
    if (pid <= 0) {
        return;
    }
    QSet<qint64> pids = readCorePids();
    pids.remove(pid);
    writeCorePids(pids);
}
