#pragma once

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
    QString stateConfigPath() const;
    Config loadPrimaryConfig();
    bool loadStateInto(Config& config);
    bool savePrimaryConfig(const Config& config);
    bool saveStateConfig(const Config& config);

    QString configPath_;
    QString lastLoadError_;
};
