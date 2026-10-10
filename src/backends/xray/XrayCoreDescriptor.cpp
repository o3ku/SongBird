#include "backends/xray/XrayCoreDescriptor.h"

#include "common/GitHubUrls.h"
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
        ConfigType::WireGuard
    };
}

QList<CoreGeoFileRequirement> geoFileRequirements()
{
    return {
        CoreGeoFileRequirement{
            QStringLiteral("geoip.dat"),
            QStringLiteral("geoip.dat"),
            v2rayRulesDatRepositoryPath()},
        CoreGeoFileRequirement{
            QStringLiteral("geosite.dat"),
            QStringLiteral("geosite.dat"),
            v2rayRulesDatRepositoryPath()}};
}

} // namespace

// The macOS release is published as Xray-macos-64.zip / Xray-macos-arm64-v8a.zip and
// unpacks to a bare "xray", so the ".exe" in the Windows name is a platform detail rather
// than part of the core's identity.
QStringList xrayExecutableNames()
{
#if defined(Q_OS_WIN)
    return {QStringLiteral("xray.exe")};
#else
    return {QStringLiteral("xray")};
#endif
}

CoreDescriptor xrayCoreDescriptor()
{
    return CoreDescriptor{
        CoreType::Xray,
        QStringLiteral("Xray"),
        supportedConfigTypes(),
        xrayExecutableNames(),
        20,
        QList<CoreType>{CoreType::SingBox},
        geoFileRequirements(),
        {}};
}

namespace {

const CoreDescriptorRegistration kXrayCoreDescriptorRegistration(xrayCoreDescriptor());

} // namespace
