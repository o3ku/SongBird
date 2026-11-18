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
    // The last load warning, if the load succeeded but something needs the user's
    // attention: the primary file was corrupt and a backup was restored, or the file
    // was written by a newer build. Empty when the load was clean. Set by load(); read
    // it right after the call, the way lastLoadError() is read.
    QString lastLoadWarning() const;
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
    // The newest backup that restores onto the primary path, or empty when none does.
    // Backups are tried newest-first; a corrupt backup is skipped, not fatal.
    QString restoreNewestUsableBackup();
    JsonFile::WriteResult savePrimaryConfig(const Config& config);
    JsonFile::WriteResult saveStateConfig(const Config& config);

    QString configPath_;
    QString lastLoadError_;
    QString lastLoadWarning_;
    QString lastSaveError_;
    QString lastSecretStoreWarning_;
    // Set by loadPrimaryConfig when legacy plaintext credentials were found
    // while a store is available: load() re-saves them protected.
    bool secretMigrationPending_ = false;
    // Set by loadPrimaryConfig when the file's schemaVersion is newer than this build
    // understands: the config loads, but save() refuses to overwrite it.
    bool schemaTooNew_ = false;
    int loadedSchemaVersion_ = -1;
    std::unique_ptr<const ISecretStore> secretStore_;
};
