#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>

#include "common/OperationResult.h"
#include "runtime/CoreInfo.h"

enum class CoreStartupCheckpointStatus {
    Pending,
    Started,
    Passed,
    Skipped,
    Failed
};

inline QString coreStartupCheckpointStatusText(CoreStartupCheckpointStatus status)
{
    switch (status) {
    case CoreStartupCheckpointStatus::Pending:
        return QStringLiteral("PENDING");
    case CoreStartupCheckpointStatus::Started:
        return QStringLiteral("START");
    case CoreStartupCheckpointStatus::Passed:
        return QStringLiteral("OK");
    case CoreStartupCheckpointStatus::Skipped:
        return QStringLiteral("SKIP");
    case CoreStartupCheckpointStatus::Failed:
        return QStringLiteral("FAIL");
    }

    return QStringLiteral("UNKNOWN");
}

inline QString coreStartupChecklistMark(CoreStartupCheckpointStatus status)
{
    switch (status) {
    case CoreStartupCheckpointStatus::Pending:
        return QString(QChar(static_cast<ushort>(0x26AA)));
    case CoreStartupCheckpointStatus::Started:
        return QString(QChar(static_cast<ushort>(0x23F3)));
    case CoreStartupCheckpointStatus::Passed:
        return QString(QChar(static_cast<ushort>(0x2705)));
    case CoreStartupCheckpointStatus::Skipped:
        return QString(QChar(static_cast<ushort>(0x2796)));
    case CoreStartupCheckpointStatus::Failed:
        return QString(QChar(static_cast<ushort>(0x274C)));
    }

    return QString(QChar(static_cast<ushort>(0x2754)));
}

inline QString coreStartupChecklistItem(
    CoreStartupCheckpointStatus status,
    const QString& name)
{
    return QStringLiteral("%1 %2")
        .arg(coreStartupChecklistMark(status), name);
}

inline QChar coreStartupChecklistDetailSeparator()
{
    return QChar(static_cast<ushort>(0x001F));
}

inline QString coreStartupChecklistItem(
    CoreStartupCheckpointStatus status,
    const QString& name,
    const QString& detail)
{
    const QString trimmedDetail = detail.trimmed();
    QString item = coreStartupChecklistItem(status, name);
    if (!trimmedDetail.isEmpty()) {
        item.append(coreStartupChecklistDetailSeparator());
        item.append(trimmedDetail);
    }
    return item;
}

inline OperationResult coreStartupCheckpoint(
    CoreStartupCheckpointStatus status,
    const QString& name,
    const QString& detail = {})
{
    const QString trimmedDetail = detail.trimmed();
    QString message = QStringLiteral("[Core Startup] [%1] %2")
        .arg(coreStartupCheckpointStatusText(status), name);
    if (!trimmedDetail.isEmpty()) {
        message += QStringLiteral(": %1").arg(trimmedDetail);
    }

    return status == CoreStartupCheckpointStatus::Failed
        ? OperationResult::fail(message)
        : OperationResult::ok(message);
}

inline bool coreUsesLegacyGeoFiles(const CoreInfo& coreInfo)
{
    const QString fileName = QFileInfo(coreInfo.program).fileName().toLower();
    return fileName.contains(QStringLiteral("xray"))
        || fileName.contains(QStringLiteral("v2ray"));
}

inline OperationResult validateCoreGeoFilesBeforeStart(const CoreInfo& coreInfo)
{
    if (!coreUsesLegacyGeoFiles(coreInfo)) {
        return OperationResult::ok(
            QStringLiteral("The selected core does not require local geoip.dat/geosite.dat files."));
    }

    const QString directory = coreInfo.workingDirectory.trimmed().isEmpty()
        ? QFileInfo(coreInfo.program).absolutePath()
        : coreInfo.workingDirectory;
    if (directory.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Core working directory is empty."));
    }

    const QStringList requiredFiles{
        QStringLiteral("geoip.dat"),
        QStringLiteral("geosite.dat")
    };
    QStringList missingFiles;
    QStringList emptyFiles;
    for (const QString& fileName : requiredFiles) {
        const QFileInfo fileInfo(QDir(directory).filePath(fileName));
        if (!fileInfo.exists()) {
            missingFiles.append(fileName);
            continue;
        }
        if (fileInfo.size() <= 0) {
            emptyFiles.append(fileName);
        }
    }

    if (!missingFiles.isEmpty() || !emptyFiles.isEmpty()) {
        QStringList parts;
        if (!missingFiles.isEmpty()) {
            parts.append(QStringLiteral("missing %1").arg(missingFiles.join(QStringLiteral(", "))));
        }
        if (!emptyFiles.isEmpty()) {
            parts.append(QStringLiteral("empty %1").arg(emptyFiles.join(QStringLiteral(", "))));
        }
        parts.append(QStringLiteral("directory %1").arg(QDir::toNativeSeparators(directory)));
        return OperationResult::fail(parts.join(QStringLiteral("; ")));
    }

    return OperationResult::ok(
        QStringLiteral("Found geoip.dat and geosite.dat in %1.")
            .arg(QDir::toNativeSeparators(directory)));
}
