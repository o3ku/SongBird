#include "backends/singbox/SingBoxCoreBackend.h"

#include <optional>

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include "common/AppPaths.h"
#include "common/GitHubUrls.h"
#include "runtime/DnsConfigFragments.h"
#include "runtime/ProtocolConfigMapper.h"
#include "runtime/RoutingConfigFragments.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "backends/singbox/SingBoxCoreDescriptor.h"
#include "backends/singbox/SingBoxConfigFragments.h"
#include "backends/singbox/SingBoxRoutingConfigFragments.h"

namespace {

// Written when the user turns logging on, next to the core like Xray's own Vaccess.log/Verror.log.
// A per-core name, so switching core does not mix two cores' output into one file.
const QString kDefaultLogFileName = QStringLiteral("singbox.log");

// The one place the bootstrap version appears.
//
// It is a freshness choice rather than a compatibility requirement: the generated config is
// accepted by 1.13.x and 1.14.x alike, so this tag is only about shipping a current core when no
// core is installed and GitHub's release lookup is unavailable. Every asset name embeds the version
// twice over -- once in the file name, once in the tag -- so keeping it in one place is what stops
// a bump from leaving three of the four names behind.
constexpr char kFallbackVersion[] = "1.14.2";

QString fallbackTagName()
{
    return QStringLiteral("v%1").arg(QLatin1String(kFallbackVersion));
}

// `platformPart` is the vendor's own platform-and-architecture token, e.g. "windows-amd64".
QString fallbackAssetName(const QString& platformPart, const QString& extension)
{
    return QStringLiteral("sing-box-%1-%2.%3")
        .arg(QLatin1String(kFallbackVersion), platformPart, extension);
}

bool isSupportedSingBoxNetwork(const QString& network)
{
    static const QSet<QString> supportedNetworks{
        QStringLiteral("tcp"),
        QStringLiteral("ws"),
        QStringLiteral("grpc"),
        QStringLiteral("h2"),
        QStringLiteral("httpupgrade"),
        QStringLiteral("quic")};
    return supportedNetworks.contains(network);
}

bool isSupportedSingBoxNonTcpTransport(ConfigType type)
{
    switch (type) {
    case ConfigType::VMess:
    case ConfigType::VLESS:
    case ConfigType::Trojan:
    case ConfigType::Shadowsocks:
    case ConfigType::Hysteria2:
    case ConfigType::TUIC:
    case ConfigType::AnyTLS:
    case ConfigType::Naive:
        return true;
    case ConfigType::WireGuard:
    case ConfigType::Socks:
    case ConfigType::HTTP:
    case ConfigType::Custom:
    case ConfigType::Unknown:
    default:
        return false;
    }
}

bool isSupportedSingBoxShadowsocksTransport(const QString& network)
{
    static const QSet<QString> supportedNetworks{
        QStringLiteral("tcp"),
        QStringLiteral("ws"),
        QStringLiteral("quic")};
    return supportedNetworks.contains(network);
}

} // namespace

CoreDescriptor SingBoxCoreBackend::descriptor() const
{
    return singBoxCoreDescriptor();
}

CoreType SingBoxCoreBackend::type() const
{
    return descriptor().type;
}

QString SingBoxCoreBackend::displayName() const
{
    return descriptor().displayName;
}

bool SingBoxCoreBackend::supportsConfigType(ConfigType configType) const
{
    return descriptor().supportedConfigTypes.contains(configType);
}

QStringList SingBoxCoreBackend::executableNames() const
{
    return descriptor().executableNames;
}

QStringList SingBoxCoreBackend::launchArguments(const QString& configPlaceholder) const
{
    return {
        QStringLiteral("run"),
        QStringLiteral("-c"),
        configPlaceholder
    };
}

bool SingBoxCoreBackend::appendConfigArgument() const
{
    return false;
}

QStringList SingBoxCoreBackend::configPreflightArguments(const QString& configFilePath) const
{
    return {
        QStringLiteral("check"),
        QStringLiteral("-c"),
        QDir::toNativeSeparators(configFilePath)
    };
}

QStringList SingBoxCoreBackend::versionCommandArguments() const
{
    return {QStringLiteral("version")};
}

QString SingBoxCoreBackend::extractVersionFromOutput(const QString& output) const
{
    const QRegularExpressionMatch match =
        QRegularExpression(QStringLiteral("\\bsing-box\\s+version\\s+([0-9A-Za-z._-]+)")).match(output.trimmed());
    return match.hasMatch() ? normalizeCoreVersionTag(match.captured(1)) : QString();
}

OperationResult SingBoxCoreBackend::validateServer(const VmessItem& server) const
{
    if (ProtocolConfigMapper::resolveSingBoxOutboundType(server.configType).isEmpty()) {
        return OperationResult::fail(QCoreApplication::translate(
            "SingBoxCoreBackend", "The selected server type is not supported by the current sing-box generator."));
    }

    const QString network = server.network.trimmed().isEmpty()
        ? QStringLiteral("tcp")
        : server.network.trimmed().toLower();
    if (!isSupportedSingBoxNetwork(network)) {
        return OperationResult::fail(
            QCoreApplication::translate("SingBoxCoreBackend", "sing-box config generation does not support network %1 yet.").arg(network));
    }

    if (network != QStringLiteral("tcp") && !isSupportedSingBoxNonTcpTransport(server.configType)) {
        return OperationResult::fail(
            QCoreApplication::translate("SingBoxCoreBackend", "sing-box does not support %1 transport for %2 nodes.")
                .arg(network, configTypeDisplayName(server.configType)));
    }

    if (server.configType == ConfigType::Shadowsocks
        && !isSupportedSingBoxShadowsocksTransport(network)) {
        return OperationResult::fail(
            QCoreApplication::translate("SingBoxCoreBackend", "sing-box does not support %1 transport for %2 nodes.")
                .arg(network, configTypeDisplayName(server.configType)));
    }

    return OperationResult::ok();
}

QJsonObject SingBoxCoreBackend::buildClientRoot(const Config& config, const VmessItem& server) const
{
    QJsonObject root;
    root.insert(QStringLiteral("log"), buildLog(config, true));
    root.insert(QStringLiteral("inbounds"), buildInbounds(config));
    root.insert(QStringLiteral("outbounds"), buildOutbounds(config, server));
    const std::optional<RoutingItem> selectedRouting = RoutingConfigFragments::resolveSelectedRouting(config);
    const RoutingItem* selectedRoutingPtr = selectedRouting.has_value() ? &*selectedRouting : nullptr;
    root.insert(QStringLiteral("route"), SingBoxRoutingConfigFragments::buildRoute(config, selectedRoutingPtr));

    const QJsonObject dns = DnsConfigFragments::buildSingBoxDns(config, selectedRoutingPtr);
    if (!dns.isEmpty()) {
        root.insert(QStringLiteral("dns"), dns);
    }

    const QJsonObject experimental = buildExperimental(config);
    if (!experimental.isEmpty()) {
        root.insert(QStringLiteral("experimental"), experimental);
    }

    SingBoxRoutingConfigFragments::migrateGeoToRuleSet(root);
    return root;
}

QJsonObject SingBoxCoreBackend::buildAuxiliaryTunClientRoot(const Config& config, AuxiliaryTunRouting routing) const
{
    return buildTunCompatClientRoot(config, routing);
}

QUrl SingBoxCoreBackend::releasesApiUrl() const
{
    return githubReleasesApiUrl(singBoxRepositoryPath(), 20);
}

CoreUpdateAssetPolicy SingBoxCoreBackend::updateAssetPolicy(CoreAssetPlatform platform) const
{
    // macOS publishes the core as a .tar.gz rather than a .zip, one per architecture and with no
    // 32-bit build at all, so only the primary slot is filled there.
    if (platform.isMacOS()) {
        const QString macosAssetName = fallbackAssetName(
            platform.appleSilicon ? QStringLiteral("darwin-arm64") : QStringLiteral("darwin-amd64"),
            QStringLiteral("tar.gz"));
        return CoreUpdateAssetPolicy{
            fallbackTagName(),
            macosAssetName,
            {},
            QStringLiteral("SagerNet/sing-box"),
            {},
            {},
            {},
            {}
        };
    }

    return CoreUpdateAssetPolicy{
        fallbackTagName(),
        fallbackAssetName(QStringLiteral("windows-amd64"), QStringLiteral("zip")),
        fallbackAssetName(QStringLiteral("windows-386"), QStringLiteral("zip")),
        QStringLiteral("SagerNet/sing-box"),
        {},
        {},
        {},
        {}
    };
}

int SingBoxCoreBackend::scoreReleaseAssetName(const QString& assetName, CoreAssetPlatform platform) const
{
    const QString normalized = assetName.trimmed().toLower();
    if (!normalized.startsWith(QStringLiteral("sing-box-"))) {
        return -1;
    }

    if (platform.isMacOS()) {
        // The darwin assets are .tar.gz, not .zip, and "legacy-macos-10.13" is the Intel build for
        // macOS releases before 11 -- the plain "darwin-amd64" one is the answer for every machine
        // that can run the rest of this app.
        if (!normalized.endsWith(QStringLiteral(".tar.gz"))
            || !normalized.contains(QStringLiteral("darwin-"))
            || normalized.contains(QStringLiteral("legacy-macos-"))) {
            return -1;
        }
        const bool assetIsArm64 = normalized.contains(QStringLiteral("arm64"));
        return assetIsArm64 == platform.appleSilicon ? 350 : -1;
    }

    if (!normalized.endsWith(QStringLiteral(".zip")) && !normalized.endsWith(QStringLiteral(".exe"))) {
        return -1;
    }
    if (normalized.contains(QStringLiteral("arm64")) || normalized.contains(QStringLiteral("armv"))) {
        return -1;
    }
    if (!normalized.contains(QStringLiteral("windows-"))
        || normalized.contains(QStringLiteral("legacy-windows-7"))) {
        return -1;
    }

    if (platform.sixtyFourBit) {
        return normalized.contains(QStringLiteral("windows-amd64.zip")) ? 350 : -1;
    }

    return normalized.contains(QStringLiteral("windows-386.zip")) ? 350 : -1;
}

QJsonObject SingBoxCoreBackend::buildTunCompatClientRoot(const Config& config, AuxiliaryTunRouting routing)
{
    const bool relayToProxy = routing == AuxiliaryTunRouting::RelayToLocalProxy;

    QJsonObject root;
    root.insert(QStringLiteral("log"), buildLog(config, false));

    QJsonArray inbounds;
    inbounds.append(SingBoxConfigFragments::buildTunInbound(config));
    root.insert(QStringLiteral("inbounds"), inbounds);

    root.insert(QStringLiteral("outbounds"), relayToProxy
                                                ? SingBoxConfigFragments::buildTunCompatOutbounds(config)
                                                : SingBoxConfigFragments::buildTunCompatDirectOutbounds());
    root.insert(QStringLiteral("route"), relayToProxy
                                             ? SingBoxConfigFragments::buildTunCompatRoute(config)
                                             : SingBoxConfigFragments::buildTunCompatDirectRoute(config));

    const QJsonObject dns = SingBoxConfigFragments::buildTunCompatDns();
    if (!dns.isEmpty()) {
        root.insert(QStringLiteral("dns"), dns);
    }

    return root;
}

QJsonObject SingBoxCoreBackend::buildLog(const Config& config, bool writeLogFile)
{
    QJsonObject log;
    log.insert(QStringLiteral("disabled"), false);
    log.insert(QStringLiteral("level"), ProtocolConfigMapper::normalizeSingBoxLogLevel(config.logLevel));
    // logEnabled means the same thing here as it does for Xray: also write the log to a file.
    // Logging to stdout continues either way, because the front end reads the core's output from
    // the process pipe, so "disabled" stays false.
    //
    // writeLogFile is false for the TUN sidecar. That root exists only when Xray is the real core
    // (sing-box's own descriptor lists no auxiliary TUN core), so naming a file there would leave a
    // singbox.log containing an internal helper's output while the user's actual core is Xray.
    if (writeLogFile && config.logEnabled) {
        log.insert(QStringLiteral("output"), kDefaultLogFileName);
    }
    return log;
}

QJsonObject SingBoxCoreBackend::buildExperimental(const Config& config)
{
    QJsonObject experimental;

    if (config.dns().enableCacheFile4Sbox) {
        QJsonObject cacheFile;
        cacheFile.insert(QStringLiteral("enabled"), true);
        cacheFile.insert(
            QStringLiteral("path"),
            QDir(AppPaths::applicationDirectory()).filePath(QStringLiteral("cache.db")));
        cacheFile.insert(QStringLiteral("store_fakeip"), config.dns().fakeIp);
        experimental.insert(QStringLiteral("cache_file"), cacheFile);
    }

    return experimental;
}

QJsonArray SingBoxCoreBackend::buildInbounds(const Config& config)
{
    QJsonArray inbounds;
    if (config.tun().tunModeItem.enableTun) {
        inbounds.append(SingBoxConfigFragments::buildTunInbound(config));
    }
    inbounds.append(SingBoxConfigFragments::buildSocksInbound(config, false, 0));
    inbounds.append(SingBoxConfigFragments::buildHttpInbound(config, false, 1));
    inbounds.append(SingBoxConfigFragments::buildHttpInboundWithTag(
        config,
        RoutingConfigFragments::locationProbeTag(),
        RoutingConfigFragments::locationProbePortOffset()));

    if (config.allowLanConnection) {
        inbounds.append(SingBoxConfigFragments::buildSocksInbound(config, true, 2));
        inbounds.append(SingBoxConfigFragments::buildHttpInbound(config, true, 3));
    }

    return inbounds;
}

QJsonArray SingBoxCoreBackend::buildOutbounds(const Config& config, const VmessItem& server)
{
    QJsonArray outbounds;
    outbounds.append(SingBoxConfigFragments::buildPrimaryOutbound(config, server));
    outbounds.append(SingBoxConfigFragments::buildDirectOutbound());
    outbounds.append(SingBoxConfigFragments::buildBlockOutbound());
    return outbounds;
}
