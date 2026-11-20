#include "persistence/JsonConfigRepository.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStringList>

#include "common/JsonFile.h"
#include "common/LocalInboundPorts.h"
#include "persistence/JsonConfigRootSerialization.h"
#include "persistence/JsonConfigSerialization.h"
#include "persistence/JsonConfigStateSerialization.h"
#include "persistence/SecretFieldCodec.h"
#include "platform/SecretStoreFactory.h"
// The backup fallback uses ConfigBackupService, included here in the .cpp only: the
// service's header does not include this header, so the include graph stays acyclic
// (the service itself builds a repository only when writing a backup).
#include "services/ConfigBackupService.h"

#include <utility>

namespace {

// Turns a JsonFile::WriteResult failure into a message that names both the file
// and the step, so a caller reporting lastSaveError() can say which of the two
// files failed and why instead of just "save failed".
QString describeWriteFailure(
    const QString& label,
    const QString& path,
    JsonFile::WriteFailureStage stage)
{
    const QString target = QDir::toNativeSeparators(path);

    // One complete sentence per step, rather than gluing a step verb and a noun phrase into a
    // shared "Failed to %1 %2: %3" template. Chinese word order puts the verb and its object the
    // other way round ("打开配置文件失败"), which swapping parameters cannot express -- the whole
    // sentence has to be translatable, so each step gets its own source string.
    switch (stage) {
    case JsonFile::WriteFailureStage::Open:
        return QCoreApplication::translate("JsonConfigRepository", "Failed to open %1: %2").arg(label, target);
    case JsonFile::WriteFailureStage::Commit:
        return QCoreApplication::translate("JsonConfigRepository", "Failed to commit %1: %2").arg(label, target);
    case JsonFile::WriteFailureStage::Write:
    case JsonFile::WriteFailureStage::None:
        break;
    }

    return QCoreApplication::translate("JsonConfigRepository", "Failed to write %1: %2").arg(label, target);
}

// Whether the config already on disk holds the same logical content as the
// config about to be saved. The stored bytes are not comparable: a secret
// store that re-protects with fresh randomness (macOS draws a new IV per
// call) produces different bytes on every save, so the byte-compare inside
// writeFileAtomically can never see an unchanged file. Decoding what is on
// disk and comparing the plaintext instead restores the skip. Any doubt --
// missing file, unreadable or unparseable content, a credential that cannot
// be decrypted -- is not evidence of equality, so the save proceeds as
// normal; skipping must never lose a change. Legacy plaintext credentials
// are also not skipped: they still need their migration to protected form.
bool primaryConfigLogicallyUnchanged(
    const QString& configPath,
    const QJsonObject& plaintextRoot,
    const ISecretStore* secretStore)
{
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QByteArray payload = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }

    QJsonObject storedRoot = document.object();
    const SecretFieldCodec::LoadReport report =
        SecretFieldCodec::decodeSecrets(storedRoot, secretStore);
    if (report.legacyPlaintextSecretsFound) {
        return false;
    }
    return storedRoot == plaintextRoot;
}

} // namespace

JsonConfigRepository::JsonConfigRepository(QString configPath)
    : JsonConfigRepository(std::move(configPath), createSecretStore())
{
}

JsonConfigRepository::JsonConfigRepository(QString configPath, std::unique_ptr<const ISecretStore> secretStore)
    : configPath_(std::move(configPath))
    , secretStore_(std::move(secretStore))
{
}

Config JsonConfigRepository::load()
{
    lastLoadError_.clear();
    lastLoadWarning_.clear();
    lastSecretStoreWarning_.clear();
    secretMigrationPending_ = false;
    schemaTooNew_ = false;
    loadedSchemaVersion_ = -1;
    Config config = loadPrimaryConfig();
    if (!lastLoadError_.isEmpty()) {
        // The primary file is unreadable or unparseable: try the backups newest-first
        // before giving up with an empty config. A backup that loads is restored onto
        // the primary path and the load is retried, so the next start is normal again.
        const QString primaryLoadError = lastLoadError_;
        const QString restoredBackup = restoreNewestUsableBackup();
        if (restoredBackup.isEmpty()) {
            lastLoadError_ = primaryLoadError;
            return {};
        }
        lastLoadError_.clear();
        config = loadPrimaryConfig();
        if (!lastLoadError_.isEmpty()) {
            // The restored file itself does not load: that error describes the file now
            // on disk, so it is kept instead of the primary's.
            return {};
        }
        const QString restoreNote = QCoreApplication::translate(
            "JsonConfigRepository",
            "Configuration file was corrupt; restored from backup %1.")
            .arg(QDir::toNativeSeparators(restoredBackup));
        lastLoadWarning_ = lastLoadWarning_.isEmpty()
            ? restoreNote
            : restoreNote + QLatin1Char(' ') + lastLoadWarning_;
    }

    if (!loadStateInto(config)) {
        return {};
    }

    // A port override that no longer matches `localPort` is a leftover from an earlier run,
    // not a decision the user made. Dropping it here is
    // what stops a config that has since changed `localPort` from resolving the HTTP inbound and
    // the location probe off two different bases.
    LocalInboundPorts::clearStaleOverrides(config);

    // Migration: legacy plaintext credentials found while a store is available
    // are re-saved protected. A failed re-save is not a failed load -- the
    // config in memory is complete -- but it is recorded, because the file
    // still holds plaintext. On a successful re-save, save() has already
    // recorded the encode outcome in lastSecretStoreWarning_, so nothing is
    // cleared here. A file from a newer build is never re-saved at all: save()
    // refuses it, and the refusal must not surface as a failed migration.
    if (secretMigrationPending_ && !schemaTooNew_) {
        secretMigrationPending_ = false;
        if (!save(config)) {
            lastSecretStoreWarning_ = QCoreApplication::translate(
                "JsonConfigRepository",
                "Stored proxy credentials could not be re-saved encrypted; they remain in plain text in the configuration file.");
        }
    }

    return config;
}

bool JsonConfigRepository::save(const Config& config)
{
    lastSaveError_.clear();

    // A config written by a newer build may carry fields this build does not know.
    // Saving would silently drop them, so the whole save is refused instead.
    if (schemaTooNew_) {
        lastSaveError_ = QCoreApplication::translate(
            "JsonConfigRepository",
            "Refusing to save the configuration file because it was written by a newer version of SongBird (schema version %1).")
            .arg(loadedSchemaVersion_);
        return false;
    }

    // Write both files even if one fails, so a state-file problem never hides
    // that the primary config was persisted (or vice versa). The return value
    // stays a single boolean, but lastSaveError() records which write failed --
    // without it a state-file failure was indistinguishable from losing the
    // primary config.
    const JsonFile::WriteResult primaryResult = savePrimaryConfig(config);
    const JsonFile::WriteResult stateResult = saveStateConfig(config);

    QStringList failures;
    if (!primaryResult.ok) {
        failures.append(describeWriteFailure(
            QCoreApplication::translate("JsonConfigRepository", "configuration file"),
            configPath_,
            primaryResult.stage));
    }
    if (!stateResult.ok) {
        failures.append(describeWriteFailure(
            QCoreApplication::translate("JsonConfigRepository", "configuration state file"),
            stateConfigPath(),
            stateResult.stage));
    }

    lastSaveError_ = failures.join(QLatin1Char(' '));
    return failures.isEmpty();
}

QString JsonConfigRepository::configPath() const
{
    return configPath_;
}

QString JsonConfigRepository::lastLoadError() const
{
    return lastLoadError_;
}

QString JsonConfigRepository::lastLoadWarning() const
{
    return lastLoadWarning_;
}

QString JsonConfigRepository::lastSaveError() const
{
    return lastSaveError_;
}

QString JsonConfigRepository::lastSecretStoreWarning() const
{
    return lastSecretStoreWarning_;
}

QString JsonConfigRepository::stateConfigPath() const
{
    const QFileInfo fileInfo(configPath_);
    const QString baseName = fileInfo.completeBaseName().trimmed().isEmpty()
        ? QStringLiteral("songbird")
        : fileInfo.completeBaseName();
    return fileInfo.dir().filePath(QStringLiteral("%1.state.json").arg(baseName));
}

Config JsonConfigRepository::loadPrimaryConfig()
{
    QFile file(configPath_);
    if (!file.exists()) {
        return JsonConfigSerialization::parseConfig(QJsonObject());
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        lastLoadError_ = QCoreApplication::translate("JsonConfigRepository", "Failed to open configuration file: %1")
                             .arg(QDir::toNativeSeparators(configPath_));
        return {};
    }

    const QByteArray payload = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        lastLoadError_ = parseError.error != QJsonParseError::NoError
            ? QCoreApplication::translate("JsonConfigRepository", "Failed to parse configuration file: %1 (offset %2: %3).")
                  .arg(QDir::toNativeSeparators(configPath_))
                  .arg(parseError.offset)
                  .arg(parseError.errorString())
            : QCoreApplication::translate("JsonConfigRepository", "Configuration file root must be a JSON object: %1")
                  .arg(QDir::toNativeSeparators(configPath_));
        return {};
    }

    // Credentials are translated before the document is parsed: a protected
    // value must never reach the in-memory config still encrypted, and a
    // legacy plaintext value must be reported so load() can migrate it.
    QJsonObject root = document.object();

    // schemaVersion is written by every save but was never read: a config from a newer
    // build may carry fields this build does not know, which a save would silently
    // drop. Loading still works -- the known fields read the same -- but the file is
    // never overwritten, and the warning is recorded for the log panel.
    loadedSchemaVersion_ = root.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (loadedSchemaVersion_ > JsonConfigRootSerialization::kSongBirdJsonConfigSchemaVersion) {
        schemaTooNew_ = true;
        const QString schemaWarning = QCoreApplication::translate(
            "JsonConfigRepository",
            "Configuration file was written by a newer version of SongBird (schema version %1); it will not be overwritten.")
            .arg(loadedSchemaVersion_);
        lastLoadWarning_ = lastLoadWarning_.isEmpty()
            ? schemaWarning
            : lastLoadWarning_ + QLatin1Char(' ') + schemaWarning;
    }

    const SecretFieldCodec::LoadReport secretReport =
        SecretFieldCodec::decodeSecrets(root, secretStore_.get());
    if (secretReport.secretDecryptFailed) {
        lastSecretStoreWarning_ = QCoreApplication::translate(
            "JsonConfigRepository",
            "Some stored proxy credentials could not be decrypted and were ignored. Re-enter the credentials for the affected servers or re-import them.");
    } else if (secretReport.storeUnavailable) {
        lastSecretStoreWarning_ = QCoreApplication::translate(
            "JsonConfigRepository",
            "No secret store is available on this platform, so proxy credentials are stored in plain text in the configuration file.");
    }
    // A decrypt failure forbids the migration: re-saving would write the
    // undecryptable value away as an empty field.
    secretMigrationPending_ = secretReport.legacyPlaintextSecretsFound && !secretReport.secretDecryptFailed;

    return JsonConfigSerialization::parseConfig(root);
}

QString JsonConfigRepository::restoreNewestUsableBackup()
{
    const QString backupDirectory = ConfigBackupService::backupDirectoryPathFor(configPath_);
    if (backupDirectory.trimmed().isEmpty()) {
        return {};
    }

    QDir directory(backupDirectory);
    QFileInfoList backupFiles = directory.entryInfoList(
        QStringList{QStringLiteral("songbird_*.json")},
        QDir::Files | QDir::NoDotAndDotDot);
    // Newest first, same order the backup service prunes by: last modified, then name.
    std::sort(backupFiles.begin(), backupFiles.end(), [](const QFileInfo& left, const QFileInfo& right) {
        if (left.lastModified() == right.lastModified()) {
            return left.fileName() > right.fileName();
        }
        return left.lastModified() > right.lastModified();
    });

    ConfigBackupService backupService(configPath_);
    for (const QFileInfo& backupFile : backupFiles) {
        if (backupService.restoreFromPath(backupFile.absoluteFilePath()).success) {
            return backupFile.absoluteFilePath();
        }
    }
    return {};
}

bool JsonConfigRepository::loadStateInto(Config& config)
{
    const QString path = stateConfigPath();
    QFile file(path);
    if (!file.exists()) {
        return true;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return true;
    }

    const QByteArray payload = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return true;
    }

    JsonConfigStateSerialization::read(document.object(), config);
    return true;
}

JsonFile::WriteResult JsonConfigRepository::savePrimaryConfig(const Config& config)
{
    QJsonObject root;
    JsonConfigSerialization::applyConfig(root, config);

    // The logical form, before credentials are protected: the stored bytes
    // change on every save when the store re-protects with fresh randomness,
    // so only the plaintext form is a stable comparison for the skip below.
    const QJsonObject plaintextRoot = root;

    // Credentials are protected on the way out. A value that cannot be
    // protected stays plaintext and is reported -- dropping it would lose the
    // credential, and failing the save would lose the whole config.
    const SecretFieldCodec::SaveReport secretReport =
        SecretFieldCodec::encodeSecrets(root, secretStore_.get());
    if (secretReport.secretProtectFailed) {
        lastSecretStoreWarning_ = QCoreApplication::translate(
            "JsonConfigRepository",
            "Some proxy credentials could not be encrypted and are stored in plain text in the configuration file.");
    } else if (secretReport.storeUnavailable) {
        lastSecretStoreWarning_ = QCoreApplication::translate(
            "JsonConfigRepository",
            "No secret store is available on this platform, so proxy credentials are stored in plain text in the configuration file.");
    } else {
        lastSecretStoreWarning_.clear();
    }

    const QFileInfo fileInfo(configPath_);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return {false, JsonFile::WriteFailureStage::Open};
    }

    // The byte-compare inside writeFileAtomically cannot see an unchanged
    // file once credentials are re-protected (see above), so compare the
    // logical content first and skip the rewrite when nothing changed. The
    // byte-compare stays as a second line of defense for the case where the
    // logical content matches but the stored bytes somehow do not.
    if (primaryConfigLogicallyUnchanged(configPath_, plaintextRoot, secretStore_.get())) {
        return {true, JsonFile::WriteFailureStage::None};
    }

    const QJsonDocument document(root);
    return JsonFile::writeFileAtomically(
        configPath_,
        document.toJson(QJsonDocument::Compact),
        QIODevice::Text);
}

JsonFile::WriteResult JsonConfigRepository::saveStateConfig(const Config& config)
{
    QJsonObject root;
    JsonConfigStateSerialization::write(root, config);

    const QString path = stateConfigPath();
    const QFileInfo fileInfo(path);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return {false, JsonFile::WriteFailureStage::Open};
    }

    if (root.isEmpty()) {
        // Removing a stale state file is part of committing this state, so a
        // failed removal is reported at the commit stage.
        if (QFileInfo::exists(path) && !QFile::remove(path)) {
            return {false, JsonFile::WriteFailureStage::Commit};
        }
        return {true, JsonFile::WriteFailureStage::None};
    }

    const QJsonDocument document(root);
    return JsonFile::writeFileAtomically(
        path,
        document.toJson(QJsonDocument::Compact),
        QIODevice::Text);
}
