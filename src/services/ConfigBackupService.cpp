#include "services/ConfigBackupService.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <utility>

#include "persistence/JsonConfigRepository.h"
#include "services/ConfigBackupStateDocument.h"

namespace {

constexpr int kMaxBackupFileCount = 10;

OperationResult pruneBackupDirectory(const QString& directoryPath)
{
    QDir directory(directoryPath);
    QFileInfoList backupFiles = directory.entryInfoList(
        QStringList{QStringLiteral("songbird_*.json")},
        QDir::Files | QDir::NoDotAndDotDot);
    if (backupFiles.size() <= kMaxBackupFileCount) {
        return OperationResult::ok(QString());
    }

    std::sort(backupFiles.begin(), backupFiles.end(), [](const QFileInfo& left, const QFileInfo& right) {
        if (left.lastModified() == right.lastModified()) {
            return left.fileName() > right.fileName();
        }
        return left.lastModified() > right.lastModified();
    });

    for (int i = kMaxBackupFileCount; i < backupFiles.size(); ++i) {
        if (!QFile::remove(backupFiles.at(i).absoluteFilePath())) {
            return OperationResult::fail(QStringLiteral("Failed to remove an old backup file."));
        }
    }

    return OperationResult::ok(QString());
}

} // namespace

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

    return QFileInfo(configPath_).dir().filePath(QStringLiteral("backups"));
}

OperationResult ConfigBackupService::backupCurrentConfig(const Config& fallbackConfig) const
{
    const QString directoryPath = backupDirectoryPath();
    if (directoryPath.trimmed().isEmpty()) {
        return OperationResult::fail(QStringLiteral("Backup directory is unavailable."));
    }

    OperationResult result = backupToPath(QDir(directoryPath).filePath(buildBackupFileName()), fallbackConfig);
    if (!result.success) {
        return result;
    }

    const OperationResult pruneResult = pruneBackupDirectory(directoryPath);
    if (!pruneResult.success) {
        return pruneResult;
    }

    return result;
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
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return OperationResult::fail(QStringLiteral("Failed to open the selected backup file."));
    }

    const QJsonDocument document = QJsonDocument::fromJson(sourceFile.readAll());
    if (!document.isObject()) {
        return OperationResult::fail(QStringLiteral("Failed to parse the selected backup file."));
    }

    QJsonObject primaryRoot = document.object();
    const QJsonObject stateRoot = ConfigBackupStateDocument::extractStateFromMergedRoot(primaryRoot);

    const QFileInfo targetFileInfo(configPath_);
    if (!targetFileInfo.dir().exists() && !QDir().mkpath(targetFileInfo.dir().absolutePath())) {
        return OperationResult::fail(QStringLiteral("Failed to create the configuration directory."));
    }

    QSaveFile targetFile(configPath_);
    if (!targetFile.open(QIODevice::WriteOnly)) {
        return OperationResult::fail(QStringLiteral("Failed to open the configuration file for restore."));
    }

    if (targetFile.write(QJsonDocument(primaryRoot).toJson(QJsonDocument::Compact)) < 0) {
        return OperationResult::fail(QStringLiteral("Failed to write the restored configuration file."));
    }

    if (!targetFile.commit()) {
        return OperationResult::fail(QStringLiteral("Failed to commit the restored configuration file."));
    }

    const QString statePath = ConfigBackupStateDocument::stateConfigPathFor(configPath_);
    if (stateRoot.isEmpty()) {
        if (QFileInfo::exists(statePath) && !QFile::remove(statePath)) {
            return OperationResult::fail(QStringLiteral("Failed to remove the restored UI/runtime state file."));
        }
    } else if (!ConfigBackupStateDocument::writeJsonObject(statePath, stateRoot)) {
        return OperationResult::fail(QStringLiteral("Failed to restore the UI/runtime state file."));
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
        if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return OperationResult::fail(QStringLiteral("Failed to open the configuration file for backup."));
        }

        const QJsonDocument document = QJsonDocument::fromJson(sourceFile.readAll());
        if (!document.isObject()) {
            return OperationResult::fail(QStringLiteral("Failed to parse the configuration file for backup."));
        }

        QJsonObject backupRoot = document.object();
        ConfigBackupStateDocument::mergeStateIntoPrimary(
            backupRoot,
            ConfigBackupStateDocument::readJsonObject(ConfigBackupStateDocument::stateConfigPathFor(configPath_)));
        if (!ConfigBackupStateDocument::writeJsonObject(targetPath, backupRoot)) {
            return OperationResult::fail(QStringLiteral("Failed to write the backup file."));
        }
    } else {
        JsonConfigRepository backupRepository(targetPath);
        if (!backupRepository.save(fallbackConfig)) {
            return OperationResult::fail(QStringLiteral("Failed to write the initial backup file."));
        }

        QJsonObject backupRoot = ConfigBackupStateDocument::readJsonObject(targetPath);
        ConfigBackupStateDocument::mergeStateIntoPrimary(
            backupRoot,
            ConfigBackupStateDocument::readJsonObject(ConfigBackupStateDocument::stateConfigPathFor(targetPath)));
        if (!ConfigBackupStateDocument::writeJsonObject(targetPath, backupRoot)) {
            return OperationResult::fail(QStringLiteral("Failed to finalize the backup file."));
        }
        const QString targetStatePath = ConfigBackupStateDocument::stateConfigPathFor(targetPath);
        if (QFileInfo::exists(targetStatePath) && !QFile::remove(targetStatePath)) {
            return OperationResult::fail(QStringLiteral("Failed to remove the temporary backup state file."));
        }
    }

    return OperationResult::ok(QStringLiteral("Configuration backed up to %1.").arg(QDir::toNativeSeparators(targetPath)));
}

QString ConfigBackupService::buildBackupFileName()
{
    return QStringLiteral("songbird_%1.json")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy_MM_dd_HH_mm_ss_zzz")));
}

