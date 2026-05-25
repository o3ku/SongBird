#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

namespace SingBoxOutboundConfigSupport {

QStringList splitCsv(const QString& value);
QString resolvePrimaryPath(const QString& value);
bool resolveMuxEnabled(const Config& config, const VmessItem& server);
void insertPositiveIntOrString(QJsonObject& object, const QString& key, const QString& value);
QStringList splitPemCertificates(const QString& value);
QStringList splitPinnedCertificateHashes(const QString& value);

} // namespace SingBoxOutboundConfigSupport
