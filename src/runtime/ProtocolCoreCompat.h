#pragma once

#include <QList>
#include <QString>

#include "domain/models/VmessItem.h"

inline QList<CoreType> supportedCoreTypes(ConfigType configType)
{
    switch (configType) {
    case ConfigType::VMess:
    case ConfigType::Custom:
    case ConfigType::Shadowsocks:
    case ConfigType::Socks:
    case ConfigType::VLESS:
    case ConfigType::Trojan:
        return { CoreType::Xray, CoreType::SingBox };
    case ConfigType::HTTP:
        return { CoreType::Xray };
    default:
        return {};
    }
}

inline QList<CoreType> availableCoreTypes()
{
    return { CoreType::Xray, CoreType::SingBox };
}

inline bool protocolSupportsCore(ConfigType configType, CoreType coreType)
{
    return supportedCoreTypes(configType).contains(coreType);
}
