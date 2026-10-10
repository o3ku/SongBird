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

// Windows resolves a program by name plus the PATHEXT suffix, every other platform by the
// exact file name -- so the names here cannot be one list for all platforms. The macOS
// release publishes sing-box-<version>-darwin-<arch>.tar.gz, which unpacks to a bare
// "sing-box"; the "sing-box-client" alias is kept for a renamed local copy.
QStringList singBoxExecutableNames()
{
#if defined(Q_OS_WIN)
    return {
        QStringLiteral("sing-box-client.exe"),
        QStringLiteral("sing-box.exe")};
#else
    return {
        QStringLiteral("sing-box-client"),
        QStringLiteral("sing-box")};
#endif
}

CoreDescriptor singBoxCoreDescriptor()
{
    return CoreDescriptor{
        CoreType::SingBox,
        QStringLiteral("sing-box"),
        supportedConfigTypes(),
        singBoxExecutableNames(),
        10,
        QList<CoreType>{},
        {},
        {}};
}

namespace {

const CoreDescriptorRegistration kSingBoxCoreDescriptorRegistration(singBoxCoreDescriptor());

} // namespace
