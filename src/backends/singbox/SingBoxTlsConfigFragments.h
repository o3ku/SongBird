#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

namespace SingBoxTlsConfigFragments {

QJsonObject buildTls(const Config& config, const VmessItem& server);

} // namespace SingBoxTlsConfigFragments
