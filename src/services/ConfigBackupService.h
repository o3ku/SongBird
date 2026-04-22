#pragma once

#include <QString>

#include "common/OperationResult.h"
#include "domain/models/Config.h"

class ConfigBackupService {
public:
    explicit ConfigBackupService(QString configPath);

    QString configPath() const;
    QString backupDirectoryPath() const;
    OperationResult backupCurrentConfig(const Config& fallbackConfig) const;
    OperationResult restoreFromPath(const QString& backupPath) const;

private:
    OperationResult backupToPath(const QString& targetPath, const Config& fallbackConfig) const;
    static QString buildBackupFileName();

    QString configPath_;
};
