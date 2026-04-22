#include "services/ConfigBackupService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <utility>

#include "persistence/JsonConfigRepository.h"

ConfigBackupService::ConfigBackupService(QString configPath)
    : configPath_(std::move(configPath))
{
}

QString ConfigBackupService::configPath() const
{
    return configPath_;
}

QString ConfigBackupService::backupDirectoryPath() const
{
    if (configPath_.trimmed().isEmpty()) {
        return {};
    }

    return QFileInfo(configPath_).dir().filePath(QStringLiteral("guiBackups"));
}

OperationResult ConfigBackupService::backupCurrentConfig(const Config& fallbackConfig) const
{
    const QString directoryPath = backupDirectoryPath();
    if (directoryPath.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Backup directory is unavailable."));
    }

    return backupToPath(QDir(directoryPath).filePath(buildBackupFileName()), fallbackConfig);
}

OperationResult ConfigBackupService::restoreFromPath(const QString& backupPath) const
{
    if (configPath_.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Configuration path is unavailable."));
    }

    const QString sourcePath = QFileInfo(backupPath).absoluteFilePath();
    QFile sourceFile(sourcePath);
    if (!sourceFile.exists()) {
        return OperationResult::fail(QStringLiteral("Backup file does not exist."));
    }
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        return OperationResult::fail(QStringLiteral("Failed to open the selected backup file."));
    }

    const QFileInfo targetFileInfo(configPath_);
    if (!targetFileInfo.dir().exists() && !QDir().mkpath(targetFileInfo.dir().absolutePath())) {
        return OperationResult::fail(QStringLiteral("Failed to create the configuration directory."));
    }

    QSaveFile targetFile(configPath_);
    if (!targetFile.open(QIODevice::WriteOnly)) {
        return OperationResult::fail(QStringLiteral("Failed to open the configuration file for restore."));
    }

    if (targetFile.write(sourceFile.readAll()) < 0) {
        return OperationResult::fail(QStringLiteral("Failed to write the restored configuration file."));
    }

    if (!targetFile.commit()) {
        return OperationResult::fail(QStringLiteral("Failed to commit the restored configuration file."));
    }

    return OperationResult::ok(QStringLiteral("Configuration restored from %1.").arg(QDir::toNativeSeparators(sourcePath)));
}

OperationResult ConfigBackupService::backupToPath(const QString& targetPath, const Config& fallbackConfig) const
{
    if (targetPath.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Backup target path is empty."));
    }

    const QFileInfo targetFileInfo(targetPath);
    if (!targetFileInfo.dir().exists() && !QDir().mkpath(targetFileInfo.dir().absolutePath())) {
        return OperationResult::fail(QStringLiteral("Failed to create the backup directory."));
    }

    const QString sourcePath = QFileInfo(configPath_).absoluteFilePath();
    QFile sourceFile(sourcePath);
    if (sourceFile.exists()) {
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            return OperationResult::fail(QStringLiteral("Failed to open the configuration file for backup."));
        }

        QSaveFile targetFile(targetPath);
        if (!targetFile.open(QIODevice::WriteOnly)) {
            return OperationResult::fail(QStringLiteral("Failed to open the backup file for writing."));
        }

        if (targetFile.write(sourceFile.readAll()) < 0) {
            return OperationResult::fail(QStringLiteral("Failed to write the backup file."));
        }

        if (!targetFile.commit()) {
            return OperationResult::fail(QStringLiteral("Failed to commit the backup file."));
        }
    } else {
        JsonConfigRepository backupRepository(targetPath);
        if (!backupRepository.save(fallbackConfig)) {
            return OperationResult::fail(QStringLiteral("Failed to write the initial backup file."));
        }
    }

    return OperationResult::ok(QStringLiteral("Configuration backed up to %1.").arg(QDir::toNativeSeparators(targetPath)));
}

QString ConfigBackupService::buildBackupFileName()
{
    return QStringLiteral("guiNConfig_%1.json")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy_MM_dd_HH_mm_ss_zzz")));
}
