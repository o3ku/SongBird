#pragma once

#include <QJsonObject>
#include <QString>

#include "common/OperationResult.h"

namespace ConfigBackupStateDocument {

QString stateConfigPathFor(const QString& configPath);
QJsonObject readJsonObject(const QString& path);

// Writes `root` to `path`. On failure the message names the file, the step that failed and
// whatever the OS reported, so a caller can say which file failed and why -- a bare bool left
// callers reporting "the backup could not be written" with nothing to act on.
OperationResult writeJsonObject(const QString& path, const QJsonObject& root);

void mergeStateIntoPrimary(QJsonObject& primaryRoot, const QJsonObject& stateRoot);
QJsonObject extractStateFromMergedRoot(QJsonObject& primaryRoot);

} // namespace ConfigBackupStateDocument
