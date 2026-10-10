#include "backends/xray/XrayCoreBackend.h"

#include <optional>

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
#include "backends/xray/XrayCoreDescriptor.h"
#include "backends/xray/XrayConfigFragments.h"

namespace {

const QString kDefaultAccessLogFileName = QStringLiteral("Vaccess.log");
const QString kDefaultErrorLogFileName = QStringLiteral("Verror.log");

// Mirrors the transports handled by XrayTransportConfigFragments::appendTransportSettings().
// Any other value leaves streamSettings with a `network` entry and no matching transport
// settings, which xray-core then rejects at startup instead of the UI explaining it.
bool isSupportedXrayNetwork(const QString& network)
{
    static const QSet<QString> supportedNetworks{
        QStringLiteral("tcp"),
        QStringLiteral("kcp"),
        QStringLiteral("quic"),
        QStringLiteral("ws"),
        QStringLiteral("grpc"),
        QStringLiteral("h2"),
        QStringLiteral("httpupgrade"),
        QStringLiteral("xhttp")};
    return supportedNetworks.contains(network);
}

} // namespace

CoreDescriptor XrayCoreBackend::descriptor() const
{
    return xrayCoreDescriptor();
}

CoreType XrayCoreBackend::type() const
{
    return descriptor().type;
}

QString XrayCoreBackend::displayName() const
{
    return descriptor().displayName;
}

bool XrayCoreBackend::supportsConfigType(ConfigType configType) const
{
    return descriptor().supportedConfigTypes.contains(configType);
}

QStringList XrayCoreBackend::executableNames() const
{
    return descriptor().executableNames;
}

QStringList XrayCoreBackend::launchArguments(const QString& configPlaceholder) const
{
    Q_UNUSED(configPlaceholder)
    return {};
}

bool XrayCoreBackend::appendConfigArgument() const
{
    return true;
}

QStringList XrayCoreBackend::configPreflightArguments(const QString& configFilePath) const
{
    return {
        QStringLiteral("run"),
        QStringLiteral("-test"),
        QStringLiteral("-config"),
        QDir::toNativeSeparators(configFilePath)
    };
}

QStringList XrayCoreBackend::versionCommandArguments() const
{
    return {QStringLiteral("-version")};
}

QString XrayCoreBackend::extractVersionFromOutput(const QString& output) const
{
    const QRegularExpressionMatch match =
        QRegularExpression(QStringLiteral("\\bXray\\s+([0-9A-Za-z._-]+)")).match(output.trimmed());
    return match.hasMatch() ? normalizeCoreVersionTag(match.captured(1)) : QString();
}

OperationResult XrayCoreBackend::validateServer(const VmessItem& server) const
{
    // Custom nodes are passed through verbatim by ClientConfigWriter, which never reaches
    // this backend for them, and the generator has no outbound implementation for them
    // either, so they are rejected here the same way MihomoCoreBackend does it.
    if (!supportsConfigType(server.configType) || server.configType == ConfigType::Custom) {
        return OperationResult::fail(QCoreApplication::translate(
            "XrayCoreBackend", "The selected server type is not supported by the current Xray generator."));
    }

    // Hysteria2 always emits a hysteria transport and ignores `network`, so there is
    // nothing to validate for it.
    if (server.configType == ConfigType::Hysteria2) {
        return OperationResult::ok();
    }

    const QString network = server.network.trimmed().isEmpty()
        ? QStringLiteral("tcp")
        : server.network.trimmed().toLower();
    if (!isSupportedXrayNetwork(network)) {
        return OperationResult::fail(
            QCoreApplication::translate("XrayCoreBackend", "Xray config generation does not support network %1 yet.").arg(network));
    }

    return OperationResult::ok();
}

QJsonObject XrayCoreBackend::buildClientRoot(const Config& config, const VmessItem& server) const
{
    const QJsonArray outbounds = buildOutbounds(config, server);
    if (outbounds.isEmpty()) {
        return {};
    }

    QJsonObject root;
    root.insert(QStringLiteral("log"), buildLog(config));
    root.insert(QStringLiteral("inbounds"), buildInbounds(config));
    root.insert(QStringLiteral("outbounds"), outbounds);

    const std::optional<RoutingItem> selectedRouting = RoutingConfigFragments::resolveSelectedRouting(config);
    const RoutingItem* selectedRoutingPtr = selectedRouting.has_value() ? &*selectedRouting : nullptr;
    const QJsonObject routing = RoutingConfigFragments::buildLegacyRouting(config, selectedRoutingPtr);
    if (!routing.isEmpty()) {
        root.insert(QStringLiteral("routing"), routing);
    }

    const QJsonObject dns = DnsConfigFragments::buildLegacyDns(config, selectedRoutingPtr);
    if (!dns.isEmpty()) {
        root.insert(QStringLiteral("dns"), dns);
    }

    return root;
}

QUrl XrayCoreBackend::releasesApiUrl() const
{
    return githubReleasesApiUrl(xrayRepositoryPath(), 20);
}

CoreUpdateAssetPolicy XrayCoreBackend::updateAssetPolicy(CoreAssetPlatform platform) const
{
    // macOS has no 32-bit build, so only the primary slot is ever filled there. The two macOS assets
    // are per-architecture rather than per-bitness: Xray-macos-64.zip is the Intel one and
    // Xray-macos-arm64-v8a.zip the Apple Silicon one.
    if (platform.isMacOS()) {
        const QString macosAssetName = platform.appleSilicon
            ? QStringLiteral("Xray-macos-arm64-v8a.zip")
            : QStringLiteral("Xray-macos-64.zip");
        return CoreUpdateAssetPolicy{
            QStringLiteral("v26.3.27"),
            macosAssetName,
            {},
            QStringLiteral("XTLS/Xray-core"),
            macosAssetName,
            {},
            githubLatestReleaseDownloadUrl(xrayRepositoryPath(), macosAssetName),
            {}
        };
    }

    return CoreUpdateAssetPolicy{
        QStringLiteral("v26.3.27"),
        QStringLiteral("Xray-windows-64.zip"),
        QStringLiteral("Xray-windows-32.zip"),
        QStringLiteral("XTLS/Xray-core"),
        QStringLiteral("Xray-windows-64.zip"),
        QStringLiteral("Xray-windows-32.zip"),
        githubLatestReleaseDownloadUrl(xrayRepositoryPath(), QStringLiteral("Xray-windows-64.zip")),
        githubLatestReleaseDownloadUrl(xrayRepositoryPath(), QStringLiteral("Xray-windows-32.zip"))
    };
}

int XrayCoreBackend::scoreReleaseAssetName(const QString& assetName, CoreAssetPlatform platform) const
{
    const QString normalized = assetName.trimmed().toLower();
    if (!normalized.endsWith(QStringLiteral(".zip")) && !normalized.endsWith(QStringLiteral(".exe"))) {
        return -1;
    }

    if (platform.isMacOS()) {
        // Exactly one of the two macOS assets belongs on this machine, and there is no 32-bit build
        // to fall back to when it is missing. The ".dgst" sidecars beside each asset carry the
        // checksum and end in ".dgst", so they are already excluded by the suffix test above.
        if (!normalized.startsWith(QStringLiteral("xray-macos-"))) {
            return -1;
        }
        const bool assetIsArm64 = normalized.contains(QStringLiteral("arm64"))
            || normalized.contains(QStringLiteral("armv"));
        return assetIsArm64 == platform.appleSilicon ? 400 : -1;
    }

    if (normalized.contains(QStringLiteral("arm64")) || normalized.contains(QStringLiteral("armv"))) {
        return -1;
    }
    if (platform.sixtyFourBit && normalized == QStringLiteral("xray-windows-64.zip")) {
        return 400;
    }
    if (!platform.sixtyFourBit && normalized == QStringLiteral("xray-windows-32.zip")) {
        return 400;
    }
    return -1;
}

QJsonObject XrayCoreBackend::buildLog(const Config& config)
{
    QJsonObject log;
    log.insert(QStringLiteral("loglevel"), config.logLevel.trimmed().isEmpty() ? QStringLiteral("warning") : config.logLevel);
    log.insert(QStringLiteral("access"), config.logEnabled ? kDefaultAccessLogFileName : QString());
    log.insert(QStringLiteral("error"), config.logEnabled ? kDefaultErrorLogFileName : QString());
    return log;
}

QJsonArray XrayCoreBackend::buildInbounds(const Config& config)
{
    QJsonArray inbounds;
    inbounds.append(XrayConfigFragments::buildSocksInbound(config, false, 0));
    inbounds.append(XrayConfigFragments::buildHttpInbound(config, false, 1));
    inbounds.append(XrayConfigFragments::buildHttpInboundWithTag(
        config,
        RoutingConfigFragments::locationProbeTag(),
        RoutingConfigFragments::locationProbePortOffset()));

    if (config.allowLanConnection) {
        inbounds.append(XrayConfigFragments::buildSocksInbound(config, true, 2));
        inbounds.append(XrayConfigFragments::buildHttpInbound(config, true, 3));
    }

    return inbounds;
}

QJsonArray XrayCoreBackend::buildOutbounds(const Config& config, const VmessItem& server)
{
    QJsonArray outbounds;
    QJsonObject primaryOutbound = XrayConfigFragments::buildPrimaryOutbound(config, server);
    if (primaryOutbound.isEmpty()) {
        // No proxy outbound could be produced for this protocol; emitting only
        // direct/blackhole would silently route everything around the proxy.
        return {};
    }
    if (config.dns().enableFragment) {
        const QJsonObject streamSettings = primaryOutbound.value(QStringLiteral("streamSettings")).toObject();
        if (!streamSettings.value(QStringLiteral("security")).toString().trimmed().isEmpty()) {
            QJsonObject updatedStreamSettings = streamSettings;
            QJsonObject sockopt = updatedStreamSettings.value(QStringLiteral("sockopt")).toObject();
            if (sockopt.value(QStringLiteral("dialerProxy")).toString().trimmed().isEmpty()) {
                sockopt.insert(QStringLiteral("dialerProxy"), QStringLiteral("frag-proxy"));
                updatedStreamSettings.insert(QStringLiteral("sockopt"), sockopt);
                primaryOutbound.insert(QStringLiteral("streamSettings"), updatedStreamSettings);
                outbounds.append(XrayConfigFragments::buildFragmentOutbound());
            }
        }
    }
    outbounds.append(primaryOutbound);
    outbounds.append(XrayConfigFragments::buildDirectOutbound(config));
    outbounds.append(XrayConfigFragments::buildBlackholeOutbound());
    return outbounds;
}
