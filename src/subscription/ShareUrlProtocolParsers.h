#pragma once

#include <QString>

#include "domain/models/VmessItem.h"

namespace ShareUrlProtocolParsers {

VmessItem parseShadowsocks(const QString& shareUrl, bool* ok);
VmessItem parseSocks(const QString& shareUrl, bool* ok);
VmessItem parseTrojan(const QString& shareUrl, bool* ok);
VmessItem parseVless(const QString& shareUrl, bool* ok);
VmessItem parseHysteria2(const QString& shareUrl, bool* ok);
VmessItem parseTuic(const QString& shareUrl, bool* ok);
VmessItem parseWireguard(const QString& shareUrl, bool* ok);
VmessItem parseAnytls(const QString& shareUrl, bool* ok);
VmessItem parseNaive(const QString& shareUrl, bool* ok);

} // namespace ShareUrlProtocolParsers
