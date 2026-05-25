#pragma once

#include <QJsonObject>
#include <QString>

namespace ConfigBackupStateDocument {

QString stateConfigPathFor(const QString& configPath);
QJsonObject readJsonObject(const QString& path);
bool writeJsonObject(const QString& path, const QJsonObject& root);
void mergeStateIntoPrimary(QJsonObject& primaryRoot, const QJsonObject& stateRoot);
QJsonObject extractStateFromMergedRoot(QJsonObject& primaryRoot);

} // namespace ConfigBackupStateDocument
