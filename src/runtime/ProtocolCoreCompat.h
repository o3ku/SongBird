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

inline QList<ConfigType> configurableCoreProtocols()
{
    return {
        ConfigType::VMess,
        ConfigType::Custom,
        ConfigType::Shadowsocks,
        ConfigType::Socks,
        ConfigType::VLESS,
        ConfigType::Trojan,
        ConfigType::HTTP,
        ConfigType::Hysteria2,
        ConfigType::TUIC,
        ConfigType::WireGuard,
        ConfigType::AnyTLS,
        ConfigType::Naive
    };
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
    Q_UNUSED(configType)
    return CoreType::SingBox;
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

inline CoreType configuredCoreTypeForProtocol(const Config& config, ConfigType configType)
{
    for (const CoreTypeItem& item : config.policy().coreTypeItems) {
        if (item.configType != static_cast<int>(configType)) {
            continue;
        }

        const CoreType configuredCore = resolveRuntimeCoreType(static_cast<CoreType>(item.coreType));
        if (configuredCore != CoreType::Unknown && protocolSupportsCore(configType, configuredCore)) {
            return configuredCore;
        }
    }

    return CoreType::Unknown;
}

inline CoreType resolvePreferredCoreType(const Config& config, const VmessItem& server)
{
    const CoreType configuredCore = configuredCoreTypeForProtocol(config, server.configType);
    if (configuredCore != CoreType::Unknown) {
        return configuredCore;
    }

    return resolveExistingCoreTypeForProtocol(server.configType, availableCoreTypes());
}

inline CoreType resolveSelectedCoreType(
    const Config& config,
    const VmessItem& server,
    const QList<CoreType>& existingCoreTypes)
{
    const CoreType configuredCore = configuredCoreTypeForProtocol(config, server.configType);
    if (configuredCore != CoreType::Unknown) {
        return configuredCore;
    }

    return resolveExistingCoreTypeForProtocol(server.configType, existingCoreTypes);
}

inline QList<CoreTypeItem> defaultCoreTypeItems()
{
    QList<CoreTypeItem> items;
    const QList<ConfigType> protocols = configurableCoreProtocols();
    items.reserve(protocols.size());

    for (const ConfigType configType : protocols) {
        items.append(CoreTypeItem{
            static_cast<int>(configType),
            static_cast<int>(defaultCoreTypeForProtocol(configType))});
    }

    return items;
}
