#pragma once

#include <QJsonObject>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

namespace MihomoConfigFragments {

QJsonObject buildClientRoot(const Config& config, const VmessItem& server);
QJsonObject buildClientGroupRoot(const Config& config, const QList<VmessItem>& members);
QJsonObject buildPrimaryProxy(const Config& config, const VmessItem& server, const QString& name);

} // namespace MihomoConfigFragments
