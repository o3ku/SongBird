#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "common/OperationResult.h"

namespace CoreUpdateInstallFiles {

QString findFirstExistingFile(const QString& directoryPath, const QStringList& patterns);
bool hasAnyInstalledCore(const QString& targetDirectory);
OperationResult writeBytesToFile(const QString& filePath, const QByteArray& content);
// Adds the executable bits a freshly installed core needs to be launched.
//
// Windows has no such bit and ignores this. On macOS it is load-bearing: a core only ever arrives as
// a download, and a file that is not executable cannot be started. The archive carries the bit, but
// writeBytesToFile creates its target with the process default, so it has to be put back.
void markExecutable(const QString& filePath);
OperationResult copyExtractedFiles(
    const QString& extractionDirectory,
    const QString& targetDirectory,
    bool ignoreGeoFiles);

} // namespace CoreUpdateInstallFiles
