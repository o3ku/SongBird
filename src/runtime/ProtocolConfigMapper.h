#pragma once

#include <QString>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"

class ProtocolConfigMapper {
public:
    static QString resolveSingBoxNetwork(const VmessItem& server);
    static QString resolveSingBoxOutboundType(ConfigType type);
    static QString normalizeSingBoxLogLevel(const QString& level);
    static QString resolveServerName(const VmessItem& server);
    static QString resolveFingerprint(const Config& config, const VmessItem& server);
    static QString resolveRealityFingerprint(const Config& config, const VmessItem& server);
    static bool isRealityTransport(const QString& value);
    static bool isValidRealityShortId(const QString& value);
    static bool resolveAllowInsecure(const QString& value, bool fallbackValue);
};
