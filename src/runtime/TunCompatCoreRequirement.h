#pragma once

#include <QList>

#include "domain/models/Config.h"
#include "runtime/ProtocolCoreCompat.h"

inline bool requiresTunCompatSingBoxCore(const Config& config, const VmessItem& server)
{
    if (!config.tunModeItem.enableTun || server.configType == ConfigType::Custom) {
        return false;
    }
    const CoreType selectedCore = resolveRuntimeCoreType(server.coreType);
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
