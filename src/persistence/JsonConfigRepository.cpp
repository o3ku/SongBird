#include "persistence/JsonConfigRepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

#include "persistence/JsonConfigSerialization.h"
#include "persistence/JsonConfigStateSerialization.h"

#include <utility>

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
    return savePrimaryConfig(config) && saveStateConfig(config);
}

QString JsonConfigRepository::configPath() const
{
    return configPath_;
}

QString JsonConfigRepository::lastLoadError() const
{
    return lastLoadError_;
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
        lastLoadError_ = QStringLiteral("Failed to open configuration file: %1")
                             .arg(QDir::toNativeSeparators(configPath_));
        return {};
    }

    const QByteArray payload = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        lastLoadError_ = parseError.error != QJsonParseError::NoError
            ? QStringLiteral("Failed to parse configuration file: %1 (offset %2: %3).")
                  .arg(QDir::toNativeSeparators(configPath_))
                  .arg(parseError.offset)
                  .arg(parseError.errorString())
            : QStringLiteral("Configuration file root must be a JSON object: %1")
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

bool JsonConfigRepository::savePrimaryConfig(const Config& config)
{
    QJsonObject root;
    JsonConfigSerialization::applyConfig(root, config);

    const QFileInfo fileInfo(configPath_);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return false;
    }

    QSaveFile file(configPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    const QJsonDocument document(root);
    if (file.write(document.toJson(QJsonDocument::Compact)) < 0) {
        return false;
    }

    return file.commit();
}

bool JsonConfigRepository::saveStateConfig(const Config& config)
{
    QJsonObject root;
    JsonConfigStateSerialization::write(root, config);

    const QString path = stateConfigPath();
    const QFileInfo fileInfo(path);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return false;
    }

    if (root.isEmpty()) {
        if (QFileInfo::exists(path) && !QFile::remove(path)) {
            return false;
        }
        return true;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    const QJsonDocument document(root);
    if (file.write(document.toJson(QJsonDocument::Compact)) < 0) {
        return false;
    }

    return file.commit();
}
