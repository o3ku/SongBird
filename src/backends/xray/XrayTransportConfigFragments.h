#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

namespace XrayTransportConfigFragments {

QString resolveNetwork(const VmessItem& server);
QJsonObject buildHysteria2StreamSettings(const VmessItem& server);
void appendTransportSettings(
    QJsonObject& streamSettings,
    const Config& config,
    const VmessItem& server,
    const QString& network);

} // namespace XrayTransportConfigFragments
