#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>

#include "common/AppPaths.h"
#include "common/OperationResult.h"
#include "runtime/CoreInfo.h"
#include "runtime/core/CoreCatalog.h"

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

// The core a CoreInfo really refers to. A CoreInfo built by hand (a TUN sidecar, a test) may leave
// the type unset, so the executable name is the fallback.
inline CoreType coreInfoCoreType(const CoreInfo& coreInfo)
{
    return coreInfo.type == CoreType::Unknown
        ? catalogCoreTypeForExecutableName(coreInfo.program)
        : coreInfo.type;
}

inline QList<CoreGeoFileRequirement> coreGeoFileRequirementsFor(const CoreInfo& coreInfo)
{
    return catalogCoreGeoFileRequirements(coreInfoCoreType(coreInfo));
}

inline bool coreNeedsGeoFiles(const CoreInfo& coreInfo)
{
    return !coreGeoFileRequirementsFor(coreInfo).isEmpty();
}

// Directory holding the core's geodata. A core that declares a data directory keeps them there
// (mihomo reads its home); the rest keep them next to the executable they run from.
//
// The application directory is a parameter rather than read straight from QCoreApplication because
// a core with a data directory puts its geodata under it, and a test that cannot point that
// somewhere writable could never exercise the check at all.
inline QString coreGeoDirectory(
    const CoreInfo& coreInfo,
    const QString& applicationDirectory = AppPaths::applicationDirectory())
{
    const QString dataDirectory = catalogCoreDataDirectory(coreInfoCoreType(coreInfo), applicationDirectory);
    if (!dataDirectory.isEmpty()) {
        return dataDirectory;
    }

    return coreInfo.workingDirectory.trimmed().isEmpty()
        ? QFileInfo(coreInfo.program).absolutePath()
        : coreInfo.workingDirectory;
}

// A zero-byte file is a truncated download rather than a database, so it does not count as present.
inline bool coreGeoFileIsPresent(const QString& directory, const QString& fileName)
{
    const QFileInfo fileInfo(QDir(directory).filePath(fileName));
    return fileInfo.exists() && fileInfo.isFile() && fileInfo.size() > 0;
}

inline OperationResult validateCoreGeoFilesBeforeStart(
    const CoreInfo& coreInfo,
    const QString& applicationDirectory = AppPaths::applicationDirectory())
{
    const QList<CoreGeoFileRequirement> requirements = coreGeoFileRequirementsFor(coreInfo);
    if (requirements.isEmpty()) {
        return OperationResult::ok(
            QCoreApplication::translate("ProxySession", "The selected core does not require local geo database files."));
    }

    const QString directory = coreGeoDirectory(coreInfo, applicationDirectory);
    if (directory.trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("ProxySession", "Core data directory is empty."));
    }

    QStringList missingFiles;
    QStringList emptyFiles;
    for (const CoreGeoFileRequirement& requirement : requirements) {
        const QFileInfo fileInfo(QDir(directory).filePath(requirement.fileName));
        if (!fileInfo.exists()) {
            missingFiles.append(requirement.fileName);
            continue;
        }
        if (!fileInfo.isFile() || fileInfo.size() <= 0) {
            emptyFiles.append(requirement.fileName);
        }
    }

    if (!missingFiles.isEmpty() || !emptyFiles.isEmpty()) {
        QStringList parts;
        if (!missingFiles.isEmpty()) {
            parts.append(QCoreApplication::translate("ProxySession", "missing %1")
                             .arg(missingFiles.join(QStringLiteral(", "))));
        }
        if (!emptyFiles.isEmpty()) {
            parts.append(QCoreApplication::translate("ProxySession", "empty %1")
                             .arg(emptyFiles.join(QStringLiteral(", "))));
        }
        parts.append(QCoreApplication::translate("ProxySession", "directory %1")
                         .arg(QDir::toNativeSeparators(directory)));
        return OperationResult::fail(parts.join(QStringLiteral("; ")));
    }

    QStringList presentFiles;
    presentFiles.reserve(requirements.size());
    for (const CoreGeoFileRequirement& requirement : requirements) {
        presentFiles.append(requirement.fileName);
    }

    return OperationResult::ok(
        QCoreApplication::translate("ProxySession", "Found %1 in %2.")
            .arg(presentFiles.join(QStringLiteral(", ")), QDir::toNativeSeparators(directory)));
}
