#pragma once

#include <QJsonObject>

#include "domain/models/VmessItem.h"

namespace SingBoxTransportConfigFragments {

QJsonObject buildTransport(const VmessItem& server);

} // namespace SingBoxTransportConfigFragments
