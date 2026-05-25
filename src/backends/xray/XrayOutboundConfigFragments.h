#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

namespace XrayOutboundConfigFragments {

QJsonObject buildPrimaryOutbound(const Config& config, const VmessItem& server);
QJsonObject buildDirectOutbound(const Config& config);
QJsonObject buildBlackholeOutbound();
QJsonObject buildFragmentOutbound();

} // namespace XrayOutboundConfigFragments
