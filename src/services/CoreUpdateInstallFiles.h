#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "common/OperationResult.h"

namespace CoreUpdateInstallFiles {

QString findFirstExistingFile(const QString& directoryPath, const QStringList& patterns);
bool hasAnyInstalledCore(const QString& targetDirectory);
OperationResult writeBytesToFile(const QString& filePath, const QByteArray& content);
OperationResult copyExtractedFiles(
    const QString& extractionDirectory,
    const QString& targetDirectory,
    bool ignoreGeoFiles);

} // namespace CoreUpdateInstallFiles
