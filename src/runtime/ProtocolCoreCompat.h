#pragma once

#include <QList>
#include <QString>

#include "domain/models/Config.h"
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
    case ConfigType::HTTP:
    case ConfigType::Hysteria2:
    case ConfigType::TUIC:
    case ConfigType::WireGuard:
    case ConfigType::AnyTLS:
    case ConfigType::Naive:
        return { CoreType::Xray, CoreType::SingBox };
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

inline bool prefersInstalledCoreForProtocol(ConfigType configType)
{
    switch (configType) {
    case ConfigType::VMess:
    case ConfigType::Shadowsocks:
    case ConfigType::Socks:
    case ConfigType::VLESS:
    case ConfigType::Trojan:
    case ConfigType::HTTP:
    case ConfigType::Hysteria2:
    case ConfigType::TUIC:
    case ConfigType::WireGuard:
    case ConfigType::AnyTLS:
    case ConfigType::Naive:
        return true;
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        return false;
    }
}

inline CoreType defaultCoreTypeForProtocol(ConfigType configType)
{
    switch (configType) {
    case ConfigType::HTTP:
    case ConfigType::Hysteria2:
    case ConfigType::TUIC:
    case ConfigType::WireGuard:
    case ConfigType::AnyTLS:
    case ConfigType::Naive:
        return CoreType::SingBox;
    default:
        return CoreType::Xray;
    }
}

inline QList<CoreType> prioritizedCoreTypesForProtocol(ConfigType configType)
{
    QList<CoreType> prioritized;
    const QList<CoreType> supported = supportedCoreTypes(configType);
    if (supported.contains(CoreType::SingBox)) {
        prioritized.append(CoreType::SingBox);
    }
    if (supported.contains(CoreType::Xray)) {
        prioritized.append(CoreType::Xray);
    }
    for (const CoreType coreType : supported) {
        if (!prioritized.contains(coreType)) {
            prioritized.append(coreType);
        }
    }
    return prioritized;
}

inline CoreType resolveExistingCoreTypeForProtocol(ConfigType configType, const QList<CoreType>& existingCoreTypes)
{
    if (!prefersInstalledCoreForProtocol(configType)) {
        return defaultCoreTypeForProtocol(configType);
    }

    const QList<CoreType> prioritized = prioritizedCoreTypesForProtocol(configType);
    for (const CoreType coreType : prioritized) {
        if (existingCoreTypes.contains(coreType)) {
            return coreType;
        }
    }
    if (prioritized.contains(CoreType::SingBox)) {
        return CoreType::SingBox;
    }
    return prioritized.isEmpty() ? defaultCoreTypeForProtocol(configType) : prioritized.constFirst();
}

inline CoreType resolvePreferredCoreType(const Config& config, const VmessItem& server)
{
    for (const CoreTypeItem& item : config.coreTypeItems) {
        if (item.configType == static_cast<int>(server.configType)) {
            const CoreType configuredCore = static_cast<CoreType>(item.coreType);
            if (configuredCore == CoreType::Auto || configuredCore == CoreType::Unknown) {
                return defaultCoreTypeForProtocol(server.configType);
            }
            return resolveRuntimeCoreType(configuredCore);
        }
    }

    if (server.coreType == CoreType::Auto || server.coreType == CoreType::Unknown) {
        return defaultCoreTypeForProtocol(server.configType);
    }

    return resolveRuntimeCoreType(server.coreType);
}
