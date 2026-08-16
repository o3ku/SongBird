#include "backends/mihomo/MihomoCoreDescriptor.h"

#include "runtime/core/CoreDescriptorRegistry.h"

namespace {

QList<ConfigType> supportedConfigTypes()
{
    return {
        ConfigType::VMess,
        ConfigType::Custom,
        ConfigType::Shadowsocks,
        ConfigType::Socks,
        ConfigType::VLESS,
        ConfigType::Trojan,
        ConfigType::HTTP,
        ConfigType::Hysteria2
    };
}

} // namespace

CoreDescriptor mihomoCoreDescriptor()
{
    return CoreDescriptor{
        CoreType::Mihomo,
        QStringLiteral("Mihomo"),
        supportedConfigTypes(),
        QStringList{
            QStringLiteral("mihomo.exe"),
            QStringLiteral("mihomo-windows-*.exe"),
            QStringLiteral("clash-meta.exe")},
        15,
        QList<CoreType>{},
        false};
}

namespace {

const CoreDescriptorRegistration kMihomoCoreDescriptorRegistration(mihomoCoreDescriptor());

} // namespace
