#include "services/CoreUpdateService.h"

#include <QCoreApplication>
#include <QDir>

#include <utility>

#include "common/ReleaseChecksum.h"
#include "common/UserAgent.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/ICoreBackend.h"
#include "services/CoreUpdateInstallFiles.h"
#include "services/CoreUpdateOperations.h"
#include "services/CoreUpdatePackageDownload.h"
#include "services/CoreUpdatePackageInstallation.h"
#include "services/CoreUpdateReleaseMetadata.h"
#include "services/CoreUpdateReleaseResolution.h"
#include "services/CoreUpdateVersion.h"

namespace {

constexpr int kCoreMetadataTimeoutMs = 30000;
namespace UpdateOps = CoreUpdateOperations;
using UpdateOps::cancelledResult;
using UpdateOps::isCancellationRequested;
using UpdateOps::isCancelledResult;
using UpdateOps::reportProgress;
using UpdateOps::runVersionCommandWithProcess;
namespace ReleaseMetadata = CoreUpdateReleaseMetadata;
using ReleaseMetadata::GitHubRelease;
using ReleaseMetadata::GitHubReleaseAsset;
using ReleaseMetadata::selectBestReleaseAsset;
namespace InstallFiles = CoreUpdateInstallFiles;
using InstallFiles::findFirstExistingFile;

struct CoreUpdateDefinition {
    const ICoreBackend* backend = nullptr;
};

CoreUpdateDefinition resolveDefinition(CoreType coreType)
{
    return CoreUpdateDefinition{coreBackend(coreType)};
}

} // namespace

CoreUpdateService::CoreUpdateService(
    DownloadHandler downloadHandler,
    ArchiveExtractor archiveExtractor,
    VersionCommandHandler versionCommandHandler)
    : downloadHandler_(std::move(downloadHandler))
    , archiveExtractor_(std::move(archiveExtractor))
    , versionCommandHandler_(std::move(versionCommandHandler))
{
}

OperationResult CoreUpdateService::update(
    CoreType coreType,
    const CoreUpdateConfig& config,
    const QString& targetDirectory) const
{
    return update(coreType, config, targetDirectory, UpdateOptions{});
}

OperationResult CoreUpdateService::update(
    CoreType coreType,
    const CoreUpdateConfig& config,
    const QString& targetDirectory,
    const UpdateOptions& options) const
{
    const auto& confirmDownload = options.confirmDownload;
    const auto& beforeInstall = options.beforeInstall;
    const auto& progressHandler = options.progressHandler;
    const auto& cancelCheck = options.cancelCheck;
    const bool skipLocalVersionCheck = options.skipLocalVersionCheck;

    if (isCancellationRequested(cancelCheck)) {
        return cancelledResult();
    }

    const CoreUpdateDefinition definition = resolveDefinition(coreType);
    if (definition.backend == nullptr || !definition.backend->releasesApiUrl().isValid()) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Unsupported core update target: %1.")
                .arg(coreTypeDisplayName(coreType)));
    }
    const ICoreBackend& backend = *definition.backend;
    const QString displayName = backend.displayName();

    const QString normalizedTargetDirectory = targetDirectory.trimmed();
    if (normalizedTargetDirectory.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "%1 target directory is unavailable.")
                .arg(displayName));
    }

    if (!QDir().mkpath(normalizedTargetDirectory)) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to create the target directory: %1.")
                .arg(QDir::toNativeSeparators(normalizedTargetDirectory)));
    }

    const CoreUpdateReleaseResolution::ReleaseResolutionResult releaseResult =
        CoreUpdateReleaseResolution::resolveRelease(CoreUpdateReleaseResolution::ReleaseResolutionRequest{
            backend,
            config,
            normalizedTargetDirectory,
            downloadHandler_,
            cancelCheck,
            progressHandler,
            kCoreMetadataTimeoutMs});
    if (releaseResult.hasError) {
        return releaseResult.error;
    }

    const GitHubRelease& release = releaseResult.release;
    const GitHubReleaseAsset* asset = selectBestReleaseAsset(backend, release.assets, releaseResult.platform);
    if (asset == nullptr) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "No suitable %1 asset was found in release %2.")
                .arg(displayName)
                .arg(release.tagName));
    }

    reportProgress(
        progressHandler,
        QCoreApplication::translate("CoreUpdateService", "Selected %1 package: %2")
            .arg(displayName)
            .arg(asset->name));

    if (isCancellationRequested(cancelCheck)) {
        return cancelledResult();
    }

    QString currentVersion;
    const QString executablePath = findFirstExistingFile(normalizedTargetDirectory, backend.executableNames());
    if (!skipLocalVersionCheck && !executablePath.isEmpty()) {
        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Checking the current %1 version...")
                .arg(displayName));

        QString versionOutput;
        const QStringList versionArguments = backend.versionCommandArguments();
        const OperationResult versionResult = versionCommandHandler_
            ? versionCommandHandler_(executablePath, versionArguments, &versionOutput)
            : runVersionCommandWithProcess(
                executablePath,
                versionArguments,
                &versionOutput,
                cancelCheck);
        if (isCancelledResult(versionResult)) {
            return versionResult;
        }
        if (versionResult.success) {
            currentVersion = backend.extractVersionFromOutput(versionOutput);
            if (!currentVersion.isEmpty()) {
                reportProgress(
                    progressHandler,
                    QCoreApplication::translate("CoreUpdateService", "Current %1 version: %2")
                        .arg(displayName)
                        .arg(currentVersion));
            }
        }
    }

    const QString normalizedReleaseTag = CoreUpdateVersion::normalizeTag(release.tagName);
    if (!currentVersion.isEmpty()
        && !normalizedReleaseTag.startsWith(QStringLiteral("prerelease"))
        && !CoreUpdateVersion::isNewerThan(normalizedReleaseTag, currentVersion)) {
        return OperationResult::ok(
            QCoreApplication::translate("CoreUpdateService", "%1 is already up to date: %2.")
                .arg(displayName)
                .arg(release.tagName));
    }

    if (confirmDownload) {
        const QString prompt = QCoreApplication::translate("CoreUpdateService", "Download %1 %2?\n%3")
                                   .arg(displayName)
                                   .arg(release.tagName)
                                   .arg(asset->name);
        if (!confirmDownload(prompt)) {
            return OperationResult::ok(
                QCoreApplication::translate("CoreUpdateService", "%1 update was canceled.")
                    .arg(displayName));
        }
    }

    if (isCancellationRequested(cancelCheck)) {
        return cancelledResult();
    }

    const CoreUpdatePackageDownload::PackageDownloadResult downloadResult =
        CoreUpdatePackageDownload::downloadPackage(
            displayName,
            *asset,
            downloadHandler_,
            cancelCheck,
            progressHandler,
            options.downloadProgress);
    if (downloadResult.hasError) {
        return downloadResult.error;
    }
    const QByteArray& packageBytes = downloadResult.packageBytes;

    if (isCancellationRequested(cancelCheck)) {
        return cancelledResult();
    }

    // The package may have come through a mirror, and TLS alone does not prove the bytes
    // are the ones the release published -- a mirror serves whatever it chooses. Verify
    // against the official GitHub channel first: the API digest (only when the metadata
    // that carried it came from GitHub itself), then a checksum asset in the same release,
    // then a checksum file derived from the package URL. When GitHub itself was
    // unreachable the metadata came from a mirror, so there is no official anchor at all
    // and the package is verified against a mirror-supplied checksum instead: a match
    // only proves the bytes agree with the mirror, so the install proceeds with a loud
    // warning rather than silently, and a mismatch still refuses outright.
    bool mirrorVerified = false;
    {
        QList<ReleaseChecksum::NamedAsset> releaseAssets;
        releaseAssets.reserve(release.assets.size());
        for (const GitHubReleaseAsset& releaseAsset : release.assets) {
            releaseAssets.append(ReleaseChecksum::NamedAsset{releaseAsset.name, releaseAsset.downloadUrl});
        }
        const std::optional<ReleaseChecksum::NamedAsset> checksumAsset =
            ReleaseChecksum::findChecksumAsset(releaseAssets, asset->name);
        ReleaseChecksum::VerificationRequest request;
        request.packageBytes = packageBytes;
        request.assetName = asset->name;
        request.packageDownloadUrl = asset->downloadUrl;
        request.apiDigest = asset->digest;
        request.checksumAsset = checksumAsset;
        request.metadataFromOfficial = releaseResult.metadataFromOfficial;
        // The bootstrap release is constructed locally, so its package URL comes from the
        // backend's code rather than from metadata: derived checksum URLs are anchored to
        // that URL's own host instead of to metadata that does not exist.
        request.packageUrlFromCode = releaseResult.fromBuiltInFallback;
        const auto fetchChecksum = [this, &cancelCheck](const QUrl& url, QByteArray* content) {
            if (downloadHandler_) {
                return downloadHandler_(url, content);
            }
            return UpdateOps::downloadBytesWithNetwork(
                url, content, fallbackUserAgent(), kCoreMetadataTimeoutMs, cancelCheck);
        };
        ReleaseChecksum::VerificationOutcome verification =
            ReleaseChecksum::verifyDownloadedBytes(request, fetchChecksum);
        if (verification.status == ReleaseChecksum::Verification::Unverifiable
            && !releaseResult.metadataFromOfficial) {
            verification = ReleaseChecksum::verifyDownloadedBytesViaMirror(request, fetchChecksum);
            mirrorVerified =
                verification.status == ReleaseChecksum::Verification::MirrorVerified;
        }
        if (verification.status == ReleaseChecksum::Verification::Mismatched) {
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "SHA-256 checksum mismatch for %1 %2 in release %3: the downloaded package does not match the published checksum (expected %4, got %5). The package was not installed.")
                    .arg(displayName)
                    .arg(asset->name)
                    .arg(release.tagName)
                    .arg(verification.expectedHex)
                    .arg(verification.actualHex));
        }
        if (verification.status == ReleaseChecksum::Verification::Unverifiable) {
            if (releaseResult.fromBuiltInFallback) {
                // The bootstrap release is constructed locally because the releases API is
                // unreachable, so there is no digest or checksum asset by definition -- and
                // sing-box and Mihomo publish no checksum file at all. Refusing here would
                // leave a first run with no core at all, so the failure is surfaced loudly
                // and the install proceeds. The package still came from github.com over TLS;
                // the residual risk is a compromised release, which no local check can rule
                // out without metadata.
                reportProgress(
                    progressHandler,
                    QCoreApplication::translate("CoreUpdateService", "Warning: no SHA-256 checksum is available for %1 %2, so the downloaded package could not be verified. Installing anyway because no core is installed yet; install a core manually if this is unexpected.")
                        .arg(displayName)
                        .arg(asset->name));
            } else {
                return OperationResult::fail(
                    QCoreApplication::translate("CoreUpdateService", "No SHA-256 checksum is available for %1 %2 in release %3, so the downloaded package cannot be verified. The package was not installed.")
                        .arg(displayName)
                        .arg(asset->name)
                        .arg(release.tagName));
            }
        } else if (mirrorVerified) {
            reportProgress(
                progressHandler,
                QCoreApplication::translate("CoreUpdateService", "Warning: GitHub was unreachable, so %1 %2 was verified against a checksum from a download mirror only. A mirror can serve any bytes it chooses, so keep the core only if you trust the mirror that served it.")
                    .arg(displayName)
                    .arg(asset->name));
        } else {
            reportProgress(
                progressHandler,
                QCoreApplication::translate(
                    "CoreUpdateService", "Verified the SHA-256 checksum for %1 %2.")
                    .arg(displayName)
                    .arg(asset->name));
        }
    }

    if (beforeInstall) {
        reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Preparing to install %1...")
                .arg(displayName));
        const OperationResult prepareResult = beforeInstall();
        if (!prepareResult.success) {
            return prepareResult;
        }
    }

    const OperationResult applyResult = CoreUpdatePackageInstallation::installPackage(
        normalizedTargetDirectory,
        *asset,
        packageBytes,
        config.ignoreGeoUpdateCore,
        archiveExtractor_,
        cancelCheck,
        progressHandler);
    if (!applyResult.success) {
        return applyResult;
    }

    reportProgress(
        progressHandler,
        QCoreApplication::translate("CoreUpdateService", "%1 update completed successfully.")
            .arg(displayName));

    QString completedMessage =
        QCoreApplication::translate("CoreUpdateService", "Updated %1 to %2 in %3.")
            .arg(displayName)
            .arg(release.tagName)
            .arg(QDir::toNativeSeparators(normalizedTargetDirectory));
    if (mirrorVerified) {
        completedMessage += QStringLiteral(" ") + QCoreApplication::translate(
            "CoreUpdateService",
            "Warning: GitHub was unreachable, so this core was verified against a checksum from a download mirror only.");
    }

    return OperationResult::ok(completedMessage, true);
}
