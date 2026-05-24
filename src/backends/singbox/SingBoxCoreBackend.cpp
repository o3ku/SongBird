#include "backends/singbox/SingBoxCoreBackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include "common/GitHubUrls.h"
#include "runtime/DnsConfigFragments.h"
#include "runtime/ProtocolConfigMapper.h"
#include "runtime/RoutingConfigFragments.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "backends/singbox/SingBoxCoreDescriptor.h"
#include "backends/singbox/SingBoxConfigFragments.h"
#include "backends/singbox/SingBoxRoutingConfigFragments.h"

namespace {

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
        return OperationResult::fail(QStringLiteral("The selected server type is not supported by the current sing-box generator."));
    }

    const QString network = server.network.trimmed().isEmpty()
        ? QStringLiteral("tcp")
        : server.network.trimmed().toLower();
    if (!isSupportedSingBoxNetwork(network)) {
        return OperationResult::fail(
            QStringLiteral("sing-box config generation does not support network %1 yet.").arg(network));
    }

    if (network != QStringLiteral("tcp") && !isSupportedSingBoxNonTcpTransport(server.configType)) {
        return OperationResult::fail(
            QStringLiteral("sing-box does not support %1 transport for %2 nodes.")
                .arg(network, configTypeDisplayName(server.configType)));
    }

    if (server.configType == ConfigType::Shadowsocks
        && !isSupportedSingBoxShadowsocksTransport(network)) {
        return OperationResult::fail(
            QStringLiteral("sing-box does not support %1 transport for %2 nodes.")
                .arg(network, configTypeDisplayName(server.configType)));
    }

    return OperationResult::ok();
}

QJsonObject SingBoxCoreBackend::buildClientRoot(const Config& config, const VmessItem& server) const
{
    QJsonObject root;
    root.insert(QStringLiteral("log"), buildLog(config));
    root.insert(QStringLiteral("inbounds"), buildInbounds(config));
    root.insert(QStringLiteral("outbounds"), buildOutbounds(config, server));
    const RoutingItem* selectedRouting = RoutingConfigFragments::resolveSelectedRouting(config);
    root.insert(QStringLiteral("route"), SingBoxRoutingConfigFragments::buildRoute(config, selectedRouting));

    const QJsonObject dns = DnsConfigFragments::buildSingBoxDns(config, selectedRouting);
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

QJsonObject SingBoxCoreBackend::buildAuxiliaryTunClientRoot(const Config& config) const
{
    return buildTunCompatClientRoot(config);
}

QUrl SingBoxCoreBackend::releasesApiUrl() const
{
    return githubReleasesApiUrl(singBoxRepositoryPath(), 20);
}

CoreUpdateAssetPolicy SingBoxCoreBackend::updateAssetPolicy() const
{
    return CoreUpdateAssetPolicy{
        QStringLiteral("v1.13.11"),
        QStringLiteral("sing-box-1.13.11-windows-amd64.zip"),
        QStringLiteral("sing-box-1.13.11-windows-386.zip"),
        QStringLiteral("SagerNet/sing-box"),
        {},
        {},
        {},
        {}
    };
}

int SingBoxCoreBackend::scoreReleaseAssetName(const QString& assetName, bool prefer64Bit) const
{
    const QString normalized = assetName.trimmed().toLower();
    if (!normalized.endsWith(QStringLiteral(".zip")) && !normalized.endsWith(QStringLiteral(".exe"))) {
        return -1;
    }
    if (normalized.contains(QStringLiteral("arm64")) || normalized.contains(QStringLiteral("armv"))) {
        return -1;
    }
    if (!normalized.startsWith(QStringLiteral("sing-box-"))
        || !normalized.contains(QStringLiteral("windows-"))
        || normalized.contains(QStringLiteral("legacy-windows-7"))) {
        return -1;
    }

    if (prefer64Bit) {
        return normalized.contains(QStringLiteral("windows-amd64.zip")) ? 350 : -1;
    }

    return normalized.contains(QStringLiteral("windows-386.zip")) ? 350 : -1;
}

QJsonObject SingBoxCoreBackend::buildTunCompatClientRoot(const Config& config)
{
    QJsonObject root;
    root.insert(QStringLiteral("log"), buildLog(config));

    QJsonArray inbounds;
    inbounds.append(SingBoxConfigFragments::buildTunInbound(config));
    root.insert(QStringLiteral("inbounds"), inbounds);

    root.insert(QStringLiteral("outbounds"), SingBoxConfigFragments::buildTunCompatOutbounds(config));
    root.insert(QStringLiteral("route"), SingBoxConfigFragments::buildTunCompatRoute(config));

    const QJsonObject dns = SingBoxConfigFragments::buildTunCompatDns();
    if (!dns.isEmpty()) {
        root.insert(QStringLiteral("dns"), dns);
    }

    return root;
}

QJsonObject SingBoxCoreBackend::buildLog(const Config& config)
{
    QJsonObject log;
    log.insert(QStringLiteral("disabled"), false);
    log.insert(QStringLiteral("level"), ProtocolConfigMapper::normalizeSingBoxLogLevel(config.logLevel));
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
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("cache.db")));
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
