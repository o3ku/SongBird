#pragma once

#include <QList>

#include "domain/models/Config.h"

inline bool requiresTunCompatSingBoxCore(const Config& config, const VmessItem& server)
{
    if (!config.tunModeItem.enableTun || server.configType == ConfigType::Custom) {
        return false;
    }
    CoreType effectiveCore = resolveRuntimeCoreType(server.coreType);
    for (const CoreTypeItem& item : config.coreTypeItems) {
        if (item.configType == static_cast<int>(server.configType)) {
            effectiveCore = resolveRuntimeCoreType(static_cast<CoreType>(item.coreType));
            break;
        }
    }
    return effectiveCore == CoreType::Xray;
}

inline QList<CoreType> resolveAuxiliaryTunCompatCoreTypes(const Config& config, const VmessItem& server)
{
    QList<CoreType> required;
    if (requiresTunCompatSingBoxCore(config, server)) {
        required.append(CoreType::SingBox);
    }
    return required;
}
