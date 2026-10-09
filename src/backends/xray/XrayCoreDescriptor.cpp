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

CoreDescriptor xrayCoreDescriptor()
{
    return CoreDescriptor{
        CoreType::Xray,
        QStringLiteral("Xray"),
        supportedConfigTypes(),
        QStringList{QStringLiteral("xray.exe")},
        20,
        QList<CoreType>{CoreType::SingBox},
        geoFileRequirements(),
        {}};
}

namespace {

const CoreDescriptorRegistration kXrayCoreDescriptorRegistration(xrayCoreDescriptor());

} // namespace
