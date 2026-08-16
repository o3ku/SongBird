#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

namespace MihomoConfigFragments {

QJsonObject buildClientRoot(const Config& config, const VmessItem& server);
QJsonObject buildPrimaryProxy(const Config& config, const VmessItem& server, const QString& name);

} // namespace MihomoConfigFragments
