#include "services/ConfigBackupService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <utility>

#include "persistence/JsonConfigRepository.h"

namespace {

QString stateConfigPathFor(const QString& configPath)
{
    const QFileInfo fileInfo(configPath);
    const QString baseName = fileInfo.completeBaseName().trimmed().isEmpty()
        ? QStringLiteral("songbird")
        : fileInfo.completeBaseName();
    return fileInfo.dir().filePath(QStringLiteral("%1.state.json").arg(baseName));
}

QJsonObject readJsonObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

bool writeJsonObject(const QString& path, const QJsonObject& root)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0) {
        return false;
    }

    return file.commit();
}

void mergeStateIntoPrimary(QJsonObject& primaryRoot, const QJsonObject& stateRoot)
{
    if (stateRoot.contains(QStringLiteral("ui")) && stateRoot.value(QStringLiteral("ui")).isObject()) {
        QJsonObject ui = primaryRoot.value(QStringLiteral("ui")).toObject();
        const QJsonObject stateUi = stateRoot.value(QStringLiteral("ui")).toObject();
        for (auto it = stateUi.constBegin(); it != stateUi.constEnd(); ++it) {
            ui.insert(it.key(), it.value());
        }
        if (!ui.isEmpty()) {
            primaryRoot.insert(QStringLiteral("ui"), ui);
        }
    }

    const QJsonArray serverStates = stateRoot.value(QStringLiteral("serverStates")).toArray();
    if (serverStates.isEmpty()) {
        return;
    }

    QJsonArray servers = primaryRoot.value(QStringLiteral("servers")).toArray();
    for (int i = 0; i < servers.size(); ++i) {
        if (!servers.at(i).isObject()) {
            continue;
        }

        QJsonObject server = servers.at(i).toObject();
        const QString indexId = server.value(QStringLiteral("indexId")).toString();
        if (indexId.trimmed().isEmpty()) {
            continue;
        }

        for (const QJsonValue& value : serverStates) {
            if (!value.isObject()) {
                continue;
            }

            const QJsonObject stateObject = value.toObject();
            if (stateObject.value(QStringLiteral("indexId")).toString() != indexId) {
                continue;
            }

            if (stateObject.contains(QStringLiteral("testResult"))) {
                server.insert(QStringLiteral("testResult"), stateObject.value(QStringLiteral("testResult")));
            }
            servers[i] = server;
            break;
        }
    }

    if (!servers.isEmpty()) {
        primaryRoot.insert(QStringLiteral("servers"), servers);
    }
}

QJsonObject extractStateFromMergedRoot(QJsonObject& primaryRoot)
{
    QJsonObject stateRoot;

    if (primaryRoot.contains(QStringLiteral("ui")) && primaryRoot.value(QStringLiteral("ui")).isObject()) {
        QJsonObject ui = primaryRoot.value(QStringLiteral("ui")).toObject();
        QJsonObject stateUi;
        const QStringList stateKeys = {
            QStringLiteral("mainLocationX"),
            QStringLiteral("mainLocationY"),
            QStringLiteral("mainSizeWidth"),
            QStringLiteral("mainSizeHeight"),
            QStringLiteral("mainServerSortColumn"),
            QStringLiteral("mainServerSortOrder"),
            QStringLiteral("mainSelectedSubscriptionId"),
            QStringLiteral("settingsRoutingRuleTabKey"),
            QStringLiteral("mainServerLogSplitPercent"),
            QStringLiteral("mainServerQrSplitPercent"),
            QStringLiteral("mainQrPreviewVisible"),
            QStringLiteral("mainProxyEnabled"),
            QStringLiteral("mainColumnWidths")
        };
        for (const QString& key : stateKeys) {
            if (ui.contains(key)) {
                stateUi.insert(key, ui.take(key));
            }
        }
        if (!stateUi.isEmpty()) {
            stateRoot.insert(QStringLiteral("ui"), stateUi);
        }
        if (ui.isEmpty()) {
            primaryRoot.remove(QStringLiteral("ui"));
        } else {
            primaryRoot.insert(QStringLiteral("ui"), ui);
        }
    }

    if (primaryRoot.contains(QStringLiteral("servers")) && primaryRoot.value(QStringLiteral("servers")).isArray()) {
        QJsonArray servers = primaryRoot.value(QStringLiteral("servers")).toArray();
        QJsonArray serverStates;
        for (int i = 0; i < servers.size(); ++i) {
            if (!servers.at(i).isObject()) {
                continue;
            }

            QJsonObject server = servers.at(i).toObject();
            QJsonObject stateObject;
            const QString indexId = server.value(QStringLiteral("indexId")).toString();
            if (!indexId.trimmed().isEmpty()) {
                stateObject.insert(QStringLiteral("indexId"), indexId);
            }
            if (server.contains(QStringLiteral("testResult"))) {
                stateObject.insert(QStringLiteral("testResult"), server.take(QStringLiteral("testResult")));
            }
            if (stateObject.size() > 1) {
                serverStates.append(stateObject);
            }
            servers[i] = server;
        }
        primaryRoot.insert(QStringLiteral("servers"), servers);
        if (!serverStates.isEmpty()) {
            stateRoot.insert(QStringLiteral("serverStates"), serverStates);
        }
    }

    return stateRoot;
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

    return QFileInfo(configPath_).dir().filePath(QStringLiteral("songbirdBackups"));
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
    if (!sourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return OperationResult::fail(QStringLiteral("Failed to open the selected backup file."));
    }

    const QJsonDocument document = QJsonDocument::fromJson(sourceFile.readAll());
    if (!document.isObject()) {
        return OperationResult::fail(QStringLiteral("Failed to parse the selected backup file."));
    }

    QJsonObject primaryRoot = document.object();
    const QJsonObject stateRoot = extractStateFromMergedRoot(primaryRoot);

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

    const QString statePath = stateConfigPathFor(configPath_);
    if (stateRoot.isEmpty()) {
        if (QFileInfo::exists(statePath) && !QFile::remove(statePath)) {
            return OperationResult::fail(QStringLiteral("Failed to remove the restored UI/runtime state file."));
        }
    } else if (!writeJsonObject(statePath, stateRoot)) {
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
        mergeStateIntoPrimary(backupRoot, readJsonObject(stateConfigPathFor(configPath_)));
        if (!writeJsonObject(targetPath, backupRoot)) {
            return OperationResult::fail(QStringLiteral("Failed to write the backup file."));
        }
    } else {
        JsonConfigRepository backupRepository(targetPath);
        if (!backupRepository.save(fallbackConfig)) {
            return OperationResult::fail(QStringLiteral("Failed to write the initial backup file."));
        }

        QJsonObject backupRoot = readJsonObject(targetPath);
        mergeStateIntoPrimary(backupRoot, readJsonObject(stateConfigPathFor(targetPath)));
        if (!writeJsonObject(targetPath, backupRoot)) {
            return OperationResult::fail(QStringLiteral("Failed to finalize the backup file."));
        }
        const QString targetStatePath = stateConfigPathFor(targetPath);
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

