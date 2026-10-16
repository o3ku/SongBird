#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include "common/OperationResult.h"

namespace XrayTlsCompatibility {

bool supportsAllowInsecure(const QString& version, const QDateTime& nowUtc);
OperationResult adaptConfig(QJsonObject& root, const QString& version, const QDateTime& nowUtc);

} // namespace XrayTlsCompatibility
