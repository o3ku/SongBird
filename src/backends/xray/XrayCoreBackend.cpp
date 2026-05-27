#include "backends/xray/XrayCoreBackend.h"

#include <optional>

#include <QDir>
#include <QJsonArray>
#include <QRegularExpression>

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
    Q_UNUSED(server)
    return OperationResult::ok();
}

QJsonObject XrayCoreBackend::buildClientRoot(const Config& config, const VmessItem& server) const
{
    QJsonObject root;
    root.insert(QStringLiteral("log"), buildLog(config));
    root.insert(QStringLiteral("inbounds"), buildInbounds(config));
    root.insert(QStringLiteral("outbounds"), buildOutbounds(config, server));

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

CoreUpdateAssetPolicy XrayCoreBackend::updateAssetPolicy() const
{
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

int XrayCoreBackend::scoreReleaseAssetName(const QString& assetName, bool prefer64Bit) const
{
    const QString normalized = assetName.trimmed().toLower();
    if (!normalized.endsWith(QStringLiteral(".zip")) && !normalized.endsWith(QStringLiteral(".exe"))) {
        return -1;
    }
    if (normalized.contains(QStringLiteral("arm64")) || normalized.contains(QStringLiteral("armv"))) {
        return -1;
    }
    if (prefer64Bit && normalized == QStringLiteral("xray-windows-64.zip")) {
        return 400;
    }
    if (!prefer64Bit && normalized == QStringLiteral("xray-windows-32.zip")) {
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
