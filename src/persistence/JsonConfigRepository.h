#pragma once

#include <memory>

#include <QString>

#include "common/JsonFile.h"
#include "persistence/IConfigRepository.h"
#include "platform/ISecretStore.h"

class JsonConfigRepository final : public IConfigRepository {
public:
    explicit JsonConfigRepository(QString configPath);
    // Test seam: a caller that owns a store (a fake in tests, a platform
    // store in production) passes it in; the default constructor builds the
    // platform store itself.
    JsonConfigRepository(QString configPath, std::unique_ptr<const ISecretStore> secretStore);

    Config load() override;
    bool save(const Config& config) override;

    QString configPath() const;
    QString lastLoadError() const override;
    QString lastSaveError() const override;
    // The last secret-store outcome, if it needs the user's attention: no
    // store on this platform, a protect/decrypt failure, or a failed
    // migration. Empty when credentials are protected or there are none.
    // Set by load() and save(); read it right after the call that produced
    // it, the way lastLoadError()/lastSaveError() are read.
    QString lastSecretStoreWarning() const;

private:
    QString stateConfigPath() const;
    Config loadPrimaryConfig();
    bool loadStateInto(Config& config);
    JsonFile::WriteResult savePrimaryConfig(const Config& config);
    JsonFile::WriteResult saveStateConfig(const Config& config);

    QString configPath_;
    QString lastLoadError_;
    QString lastSaveError_;
    QString lastSecretStoreWarning_;
    // Set by loadPrimaryConfig when legacy plaintext credentials were found
    // while a store is available: load() re-saves them protected.
    bool secretMigrationPending_ = false;
    std::unique_ptr<const ISecretStore> secretStore_;
};
