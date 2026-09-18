#pragma once

#include <QString>

#include "common/JsonFile.h"
#include "persistence/IConfigRepository.h"

class JsonConfigRepository final : public IConfigRepository {
public:
    explicit JsonConfigRepository(QString configPath);

    Config load() override;
    bool save(const Config& config) override;

    QString configPath() const;
    QString lastLoadError() const override;
    QString lastSaveError() const override;

private:
    QString stateConfigPath() const;
    Config loadPrimaryConfig();
    bool loadStateInto(Config& config);
    JsonFile::WriteResult savePrimaryConfig(const Config& config);
    JsonFile::WriteResult saveStateConfig(const Config& config);

    QString configPath_;
    QString lastLoadError_;
    QString lastSaveError_;
};
