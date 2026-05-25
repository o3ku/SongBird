#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

namespace XraySecurityConfigFragments {

void appendFinalmask(QJsonObject& streamSettings, const VmessItem& server);
void appendSecuritySettings(
    QJsonObject& streamSettings,
    const Config& config,
    const VmessItem& server,
    const QString& transportSecurity);

} // namespace XraySecurityConfigFragments
