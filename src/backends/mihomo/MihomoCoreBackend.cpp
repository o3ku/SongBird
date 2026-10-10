#include "backends/mihomo/MihomoCoreBackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QSet>

#include "backends/mihomo/MihomoConfigFragments.h"
#include "backends/mihomo/MihomoCoreDescriptor.h"
#include "common/AppPaths.h"
#include "common/GitHubUrls.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/CoreCatalog.h"

namespace {

bool isSupportedNetwork(const QString& network)
{
    static const QSet<QString> supportedNetworks{
        QStringLiteral("tcp"),
        QStringLiteral("ws"),
        QStringLiteral("grpc"),
        QStringLiteral("h2")};
    return supportedNetworks.contains(network);
}

QString mihomoRepositoryPath()
{
    return QStringLiteral("MetaCubeX/mihomo");
}

// The one place the bootstrap version appears, for the same reason sing-box keeps its own in one:
// the version is embedded in every asset name as well as in the tag, and a bump that misses one of
// the names leaves a bootstrap that 404s.
constexpr char kFallbackVersion[] = "1.19.25";

QString fallbackTagName()
{
    return QStringLiteral("v%1").arg(QLatin1String(kFallbackVersion));
}

// `platformPart` is the vendor's own platform-and-architecture token ("windows-amd64", "darwin-arm64")
// and `variantPart` the optional microarchitecture marker that sits in front of the version and
// carries its own trailing dash ("v1-").
QString fallbackAssetName(const QString& platformPart, const QString& variantPart, const QString& extension)
{
    return QStringLiteral("mihomo-%1-%2%3.%4")
        .arg(platformPart)
        .arg(variantPart)
        .arg(fallbackTagName())
        .arg(extension);
}

// `-d` names the core's home directory. Without it mihomo falls back to
// %USERPROFILE%\.config\mihomo, where the app can neither pre-seed the geodata the generated
// config always needs nor tell whether it is there -- and the core would fetch it during the
// startup preflight instead, which is how a first launch ends in a preflight timeout. Both the
// run and the preflight have to carry it, or the preflight would validate a different directory
// from the one the core actually uses.
QStringList dataDirectoryArguments(CoreType coreType)
{
    const QString dataDirectory =
        catalogCoreDataDirectory(coreType, AppPaths::applicationDirectory());
    return dataDirectory.isEmpty()
        ? QStringList{}
        : QStringList{QStringLiteral("-d"), QDir::toNativeSeparators(dataDirectory)};
}

} // namespace

CoreDescriptor MihomoCoreBackend::descriptor() const
{
    return mihomoCoreDescriptor();
}

CoreType MihomoCoreBackend::type() const
{
    return descriptor().type;
}

QString MihomoCoreBackend::displayName() const
{
    return descriptor().displayName;
}

bool MihomoCoreBackend::supportsConfigType(ConfigType configType) const
{
    return descriptor().supportedConfigTypes.contains(configType);
}

QStringList MihomoCoreBackend::executableNames() const
{
    return descriptor().executableNames;
}

QStringList MihomoCoreBackend::launchArguments(const QString& configPlaceholder) const
{
    QStringList arguments = dataDirectoryArguments(type());
    arguments << QStringLiteral("-f") << configPlaceholder;
    return arguments;
}

bool MihomoCoreBackend::appendConfigArgument() const
{
    return false;
}

QStringList MihomoCoreBackend::configPreflightArguments(const QString& configFilePath) const
{
    QStringList arguments = dataDirectoryArguments(type());
    arguments << QStringLiteral("-t") << QStringLiteral("-f")
              << QDir::toNativeSeparators(configFilePath);
    return arguments;
}

QStringList MihomoCoreBackend::versionCommandArguments() const
{
    return {QStringLiteral("-v")};
}

QString MihomoCoreBackend::extractVersionFromOutput(const QString& output) const
{
    const QRegularExpressionMatch match =
        QRegularExpression(QStringLiteral("\\b(?:Mihomo|Clash\\.Meta|Clash Meta)\\b[^0-9vV]*[vV]?([0-9][0-9A-Za-z._-]*)"))
            .match(output.trimmed());
    return match.hasMatch() ? normalizeCoreVersionTag(match.captured(1)) : QString();
}

OperationResult MihomoCoreBackend::validateServer(const VmessItem& server) const
{
    if (!supportsConfigType(server.configType) || server.configType == ConfigType::Custom) {
        return OperationResult::fail(QCoreApplication::translate(
            "MihomoCoreBackend", "The selected server type is not supported by the current Mihomo generator."));
    }

    const QString network = server.network.trimmed().isEmpty()
        ? QStringLiteral("tcp")
        : server.network.trimmed().toLower();
    if (!isSupportedNetwork(network)) {
        return OperationResult::fail(
            QCoreApplication::translate("MihomoCoreBackend", "Mihomo config generation does not support network %1 yet.").arg(network));
    }

    if (network == QStringLiteral("tcp")) {
        const QString headerType = server.headerType.trimmed().toLower();
        if (!headerType.isEmpty() && headerType != QStringLiteral("none")) {
            return OperationResult::fail(
                QCoreApplication::translate("MihomoCoreBackend", "Mihomo config generation does not support tcp headerType %1 yet.").arg(headerType));
        }
    }

    return OperationResult::ok();
}

QJsonObject MihomoCoreBackend::buildClientRoot(const Config& config, const VmessItem& server) const
{
    return MihomoConfigFragments::buildClientRoot(config, server);
}

QUrl MihomoCoreBackend::releasesApiUrl() const
{
    return githubReleasesApiUrl(mihomoRepositoryPath(), 20);
}

CoreUpdateAssetPolicy MihomoCoreBackend::updateAssetPolicy(CoreAssetPlatform platform) const
{
    // mihomo publishes a bare gzip for macOS and a zip for Windows, one file per architecture, and
    // no 32-bit macOS build at all -- so only the primary slot is filled on macOS.
    if (platform.isMacOS()) {
        const QString macosAssetName = fallbackAssetName(
            platform.appleSilicon ? QStringLiteral("darwin-arm64") : QStringLiteral("darwin-amd64"),
            {},
            QStringLiteral("gz"));
        return CoreUpdateAssetPolicy{
            fallbackTagName(),
            macosAssetName,
            {},
            mihomoRepositoryPath(),
            {},
            {},
            {},
            {}
        };
    }

    // The "-v1-" marker is the baseline x86-64 microarchitecture level. It is the asset mihomo
    // documents for amd64, and the plain "-v1.19.25" one beside it is the same build; picking the
    // marked one keeps the Windows name in step with what the scoring below prefers.
    return CoreUpdateAssetPolicy{
        fallbackTagName(),
        fallbackAssetName(QStringLiteral("windows-amd64"), QStringLiteral("v1-"), QStringLiteral("zip")),
        fallbackAssetName(QStringLiteral("windows-386"), {}, QStringLiteral("zip")),
        mihomoRepositoryPath(),
        {},
        {},
        {},
        {}
    };
}

int MihomoCoreBackend::scoreReleaseAssetName(const QString& assetName, CoreAssetPlatform platform) const
{
    const QString normalized = assetName.trimmed().toLower();
    if (!normalized.endsWith(QStringLiteral(".zip"))
        && !normalized.endsWith(QStringLiteral(".exe"))
        && !normalized.endsWith(QStringLiteral(".gz"))) {
        return -1;
    }

    if (platform.isMacOS()) {
        // Only the bare "mihomo-darwin-<arch>-<version>.gz" is a first choice: the other darwin
        // assets beside it carry a Go version, a microarchitecture level or a "compatible" marker in
        // the name, and none of those is a better answer than the generic build.
        if (!normalized.startsWith(QStringLiteral("mihomo-darwin-"))) {
            return -1;
        }
        const bool assetIsArm64 = normalized.contains(QStringLiteral("arm64"));
        if (assetIsArm64 != platform.appleSilicon) {
            return -1;
        }

        const bool isVariantBuild = normalized.contains(QStringLiteral("-go1"))
            || normalized.contains(QStringLiteral("-v1-"))
            || normalized.contains(QStringLiteral("-v2-"))
            || normalized.contains(QStringLiteral("-v3-"))
            || normalized.contains(QStringLiteral("-compatible-"));
        return isVariantBuild ? 300 : 380;
    }

    if (!normalized.startsWith(QStringLiteral("mihomo-windows-"))) {
        return -1;
    }
    if (normalized.contains(QStringLiteral("arm64")) || normalized.contains(QStringLiteral("armv"))) {
        return -1;
    }

    if (platform.sixtyFourBit) {
        if (normalized.contains(QStringLiteral("amd64-v1-"))) {
            return 380;
        }
        return normalized.contains(QStringLiteral("amd64")) ? 320 : -1;
    }

    return normalized.contains(QStringLiteral("windows-386-")) ? 350 : -1;
}
