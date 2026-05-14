#pragma once

#include <QList>

#include "domain/models/Config.h"
#include "runtime/ProtocolCoreCompat.h"

inline bool requiresTunCompatSingBoxCore(const Config& config, const VmessItem& server)
{
    if (!config.tunModeItem.enableTun || server.configType == ConfigType::Custom) {
        return false;
    }

    // TUN compatibility must follow the effective runtime core selection, which
    // can come from per-protocol settings instead of the server's stored core.
    const CoreType selectedCore = resolvePreferredCoreType(config, server);
    return selectedCore == CoreType::Xray;
}

inline QList<CoreType> resolveAuxiliaryTunCompatCoreTypes(const Config& config, const VmessItem& server)
{
    QList<CoreType> required;
    if (requiresTunCompatSingBoxCore(config, server)) {
        required.append(CoreType::SingBox);
    }
    return required;
}
