#include "backends/singbox/SingBoxCoreDescriptor.h"

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

CoreDescriptor singBoxCoreDescriptor()
{
    return CoreDescriptor{
        CoreType::SingBox,
        QStringLiteral("sing-box"),
        supportedConfigTypes(),
        QStringList{
            QStringLiteral("sing-box-client.exe"),
            QStringLiteral("sing-box.exe")},
        10,
        QList<CoreType>{},
        false};
}

namespace {

const CoreDescriptorRegistration kSingBoxCoreDescriptorRegistration(singBoxCoreDescriptor());

} // namespace
