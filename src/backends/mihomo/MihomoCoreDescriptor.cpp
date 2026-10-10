#include "backends/mihomo/MihomoCoreDescriptor.h"

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
        ConfigType::Hysteria2
    };
}

// The generator always emits GEOSITE/GEOIP rules, so mihomo needs both databases. It renames
// geosite.dat to GeoSite.dat on disk but reads geoip.metadb verbatim -- the plain geoip.dat is a
// different format and the core does not accept it.
QList<CoreGeoFileRequirement> geoFileRequirements()
{
    return {
        CoreGeoFileRequirement{
            QStringLiteral("GeoSite.dat"),
            QStringLiteral("geosite.dat"),
            metaRulesDatRepositoryPath()},
        CoreGeoFileRequirement{
            QStringLiteral("geoip.metadb"),
            QStringLiteral("geoip.metadb"),
            metaRulesDatRepositoryPath()}};
}

} // namespace

// The wildcard follows the platform's own release naming: mihomo publishes
// mihomo-windows-amd64-v<version>.gz and mihomo-darwin-<arch>-v<version>.gz, and both unpack
// to a bare "mihomo". "clash-meta" is the name a locally renamed copy is likely to carry.
QStringList mihomoExecutableNames()
{
#if defined(Q_OS_WIN)
    return {
        QStringLiteral("mihomo.exe"),
        QStringLiteral("mihomo-windows-*.exe"),
        QStringLiteral("clash-meta.exe")};
#else
    return {
        QStringLiteral("mihomo"),
        QStringLiteral("mihomo-darwin-*"),
        QStringLiteral("clash-meta")};
#endif
}

CoreDescriptor mihomoCoreDescriptor()
{
    return CoreDescriptor{
        CoreType::Mihomo,
        QStringLiteral("Mihomo"),
        supportedConfigTypes(),
        mihomoExecutableNames(),
        15,
        QList<CoreType>{},
        geoFileRequirements(),
        // mihomo keeps its geodata, caches and generated logs in a "home" directory that defaults
        // to %USERPROFILE%\.config\mihomo. Left there, the app could neither seed it nor find it.
        // The name is a sub-directory of the application directory so the core gets a home of its
        // own instead of sharing one with sing-box's cache.db.
        QStringLiteral("mihomo")};
}

namespace {

const CoreDescriptorRegistration kMihomoCoreDescriptorRegistration(mihomoCoreDescriptor());

} // namespace
