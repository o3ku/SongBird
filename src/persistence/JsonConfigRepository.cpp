#include "persistence/JsonConfigRepository.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStringList>
#include "common/JsonFile.h"
#include "persistence/JsonConfigSerialization.h"
#include "persistence/JsonConfigStateSerialization.h"

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

} // namespace

JsonConfigRepository::JsonConfigRepository(QString configPath)
    : configPath_(std::move(configPath))
{
}

Config JsonConfigRepository::load()
{
    lastLoadError_.clear();
    Config config = loadPrimaryConfig();
    if (!lastLoadError_.isEmpty()) {
        return {};
    }

    if (!loadStateInto(config)) {
        return {};
    }

    return config;
}

bool JsonConfigRepository::save(const Config& config)
{
    lastSaveError_.clear();

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

QString JsonConfigRepository::lastSaveError() const
{
    return lastSaveError_;
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

    return JsonConfigSerialization::parseConfig(document.object());
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

    const QFileInfo fileInfo(configPath_);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return {false, JsonFile::WriteFailureStage::Open};
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
