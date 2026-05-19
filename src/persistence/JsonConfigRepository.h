#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "persistence/IConfigRepository.h"

class JsonConfigRepository final : public IConfigRepository {
public:
    explicit JsonConfigRepository(QString configPath);

    Config load() override;
    bool save(const Config& config) override;

    QString configPath() const;
    QString lastLoadError() const;

private:
    QJsonObject loadExistingRootObject() const;

    QString configPath_;
    QString lastLoadError_;
};
