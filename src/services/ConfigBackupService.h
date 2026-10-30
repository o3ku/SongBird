#pragma once

#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"

class ConfigBackupService {
public:
    explicit ConfigBackupService(QString configPath);

    QString configPath() const;
    QString backupDirectoryPath() const;
    // The one true spelling of "where do backups live", shared with callers that
    // only need the directory (subscription raw backups) and have no service instance.
    static QString backupDirectoryPathFor(const QString& configPath);
    OperationResult backupCurrentConfig(const Config& fallbackConfig) const;
    OperationResult restoreFromPath(const QString& backupPath) const;

private:
    OperationResult backupToPath(const QString& targetPath, const Config& fallbackConfig) const;
    static QString buildBackupFileName();

    QString configPath_;
};
