#pragma once

#include <QList>
#include <QString>

#include <algorithm>

#include "domain/models/Config.h"
#include "domain/models/VmessItem.h"
#include "runtime/core/CoreCatalog.h"

inline QList<CoreType> supportedCoreTypes(ConfigType configType)
{
    QList<CoreType> coreTypes;
    for (const CoreDescriptor& descriptor : coreDescriptors()) {
        if (descriptor.supportedConfigTypes.contains(configType)) {
            coreTypes.append(descriptor.type);
        }
    }
    return coreTypes;
}

inline QList<CoreType> availableCoreTypes()
{
    return catalogCoreTypes();
}

inline QList<ConfigType> configurableCoreProtocols()
{
    QList<ConfigType> protocols;
    for (const ConfigType configType : coreCatalogSupportedConfigTypes()) {
        if (!supportedCoreTypes(configType).isEmpty()) {
            protocols.append(configType);
        }
    }
    return protocols;
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
    QList<CoreDescriptor> matchingDescriptors;
    for (const CoreDescriptor& descriptor : coreDescriptors()) {
        if (descriptor.supportedConfigTypes.contains(configType)) {
            matchingDescriptors.append(descriptor);
        }
    }

    std::sort(
        matchingDescriptors.begin(),
        matchingDescriptors.end(),
        [](const CoreDescriptor& left, const CoreDescriptor& right) {
            return left.protocolPriority < right.protocolPriority;
        });

    QList<CoreType> prioritized;
    prioritized.reserve(matchingDescriptors.size());
    for (const CoreDescriptor& descriptor : matchingDescriptors) {
        prioritized.append(descriptor.type);
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
