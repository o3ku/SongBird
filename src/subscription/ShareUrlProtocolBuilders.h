#pragma once

#include <QString>

#include "domain/models/VmessItem.h"

namespace ShareUrlProtocolBuilders {

QString buildVmess(const VmessItem& item);
QString buildShadowsocks(const VmessItem& item);
QString buildSocks(const VmessItem& item);
QString buildTrojan(const VmessItem& item);
QString buildVless(const VmessItem& item);
QString buildHysteria2(const VmessItem& item);
QString buildTuic(const VmessItem& item);
QString buildWireguard(const VmessItem& item);
QString buildAnytls(const VmessItem& item);
QString buildNaive(const VmessItem& item);

} // namespace ShareUrlProtocolBuilders
