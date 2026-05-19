#include "persistence/JsonConfigRepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

#include "persistence/JsonConfigMigration.h"
#include "persistence/JsonConfigSerialization.h"

#include <utility>

JsonConfigRepository::JsonConfigRepository(QString configPath)
    : configPath_(std::move(configPath))
{
}

Config JsonConfigRepository::load()
{
    lastLoadError_.clear();
    QFile file(configPath_);
    if (!file.exists()) {
        return JsonConfigSerialization::parseConfig(migrateJsonConfigRoot(QJsonObject()));
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        lastLoadError_ = QStringLiteral("Failed to open configuration file: %1")
                             .arg(QDir::toNativeSeparators(configPath_));
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
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

    return JsonConfigSerialization::parseConfig(migrateJsonConfigRoot(document.object()));
}

bool JsonConfigRepository::save(const Config& config)
{
    QJsonObject root = migrateJsonConfigRoot(loadExistingRootObject());
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
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0) {
        return false;
    }

    if (!file.commit()) {
        return false;
    }

    return true;
}

QString JsonConfigRepository::configPath() const
{
    return configPath_;
}

QString JsonConfigRepository::lastLoadError() const
{
    return lastLoadError_;
}

QJsonObject JsonConfigRepository::loadExistingRootObject() const
{
    QFile file(configPath_);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    return document.object();
}
