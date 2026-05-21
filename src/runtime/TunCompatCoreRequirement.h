#pragma once

#include <QList>

#include "domain/models/Config.h"
#include "runtime/ProtocolCoreCompat.h"

inline bool requiresTunCompatSingBoxCore(
    const Config& config,
    const VmessItem& server,
    const QList<CoreType>& existingCoreTypes)
{
    if (!config.tun().tunModeItem.enableTun || server.configType == ConfigType::Custom) {
        return false;
    }

    // TUN compatibility must match the core that will actually launch. The runtime
    // launch path goes through resolveSelectedCoreType (which honors installed cores),
    // so we mirror that here instead of resolvePreferredCoreType, otherwise the
    // sidecar decision can disagree with the binary AppBootstrap spawns.
    const CoreType selectedCore = resolveSelectedCoreType(config, server, existingCoreTypes);
    return selectedCore == CoreType::Xray;
}

inline bool requiresTunCompatSingBoxCore(const Config& config, const VmessItem& server)
{
    return requiresTunCompatSingBoxCore(config, server, availableCoreTypes());
}

inline QList<CoreType> resolveAuxiliaryTunCompatCoreTypes(
    const Config& config,
    const VmessItem& server,
    const QList<CoreType>& existingCoreTypes)
{
    QList<CoreType> required;
    if (requiresTunCompatSingBoxCore(config, server, existingCoreTypes)) {
        required.append(CoreType::SingBox);
    }
    return required;
}

inline QList<CoreType> resolveAuxiliaryTunCompatCoreTypes(const Config& config, const VmessItem& server)
{
    return resolveAuxiliaryTunCompatCoreTypes(config, server, availableCoreTypes());
}
