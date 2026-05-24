#include "backends/xray/XrayCoreDescriptor.h"

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
        ConfigType::Hysteria2,
        ConfigType::TUIC,
        ConfigType::WireGuard,
        ConfigType::AnyTLS,
        ConfigType::Naive
    };
}

} // namespace

CoreDescriptor xrayCoreDescriptor()
{
    return CoreDescriptor{
        CoreType::Xray,
        QStringLiteral("Xray"),
        supportedConfigTypes(),
        QStringList{QStringLiteral("xray.exe")},
        20,
        QList<CoreType>{CoreType::SingBox},
        true};
}

namespace {

const CoreDescriptorRegistration kXrayCoreDescriptorRegistration(xrayCoreDescriptor());

} // namespace
