#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "domain/models/VmessItem.h"

struct CoreDescriptor {
    CoreType type = CoreType::Unknown;
    QString displayName;
    QList<ConfigType> supportedConfigTypes;
    QStringList executableNames;
    int protocolPriority = 0;
};

inline QList<ConfigType> coreCatalogSupportedConfigTypes()
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

inline QList<CoreDescriptor> coreDescriptors()
{
    const QList<ConfigType> supportedConfigTypes = coreCatalogSupportedConfigTypes();
    QList<ConfigType> xraySupportedConfigTypes = supportedConfigTypes;
    xraySupportedConfigTypes.removeAll(ConfigType::Hysteria2);
    return {
        CoreDescriptor{
            CoreType::Xray,
            QStringLiteral("Xray"),
            xraySupportedConfigTypes,
            QStringList{QStringLiteral("xray.exe")},
            20},
        CoreDescriptor{
            CoreType::SingBox,
            QStringLiteral("sing-box"),
            supportedConfigTypes,
            QStringList{
                QStringLiteral("sing-box-client.exe"),
                QStringLiteral("sing-box.exe")},
            10}
    };
}

inline const CoreDescriptor* coreDescriptor(CoreType coreType)
{
    const CoreType runtimeCoreType = resolveRuntimeCoreType(coreType);
    static const QList<CoreDescriptor> descriptors = coreDescriptors();
    for (const CoreDescriptor& descriptor : descriptors) {
        if (descriptor.type == runtimeCoreType) {
            return &descriptor;
        }
    }
    return nullptr;
}

inline bool coreDescriptorSupportsConfigType(CoreType coreType, ConfigType configType)
{
    const CoreDescriptor* descriptor = coreDescriptor(coreType);
    return descriptor != nullptr && descriptor->supportedConfigTypes.contains(configType);
}

inline QList<CoreType> catalogCoreTypes()
{
    QList<CoreType> coreTypes;
    const QList<CoreDescriptor> descriptors = coreDescriptors();
    coreTypes.reserve(descriptors.size());
    for (const CoreDescriptor& descriptor : descriptors) {
        coreTypes.append(descriptor.type);
    }
    return coreTypes;
}

inline QStringList catalogCoreExecutableNames(CoreType coreType)
{
    const CoreDescriptor* descriptor = coreDescriptor(coreType);
    return descriptor != nullptr ? descriptor->executableNames : QStringList{};
}

inline QString catalogCoreDisplayName(CoreType coreType)
{
    const CoreDescriptor* descriptor = coreDescriptor(coreType);
    return descriptor != nullptr ? descriptor->displayName : QStringLiteral("Unknown");
}
