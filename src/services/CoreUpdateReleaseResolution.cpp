#include "services/CoreUpdateReleaseResolution.h"

#include <QCoreApplication>

#include "common/GitHubMirrorHelper.h"
#include "common/UserAgent.h"
#include "runtime/core/CoreAssetPlatform.h"
#include "services/CoreUpdateInstallFiles.h"
#include "services/CoreUpdateOperations.h"

namespace {

namespace UpdateOps = CoreUpdateOperations;
namespace ReleaseMetadata = CoreUpdateReleaseMetadata;
namespace InstallFiles = CoreUpdateInstallFiles;

CoreUpdateReleaseResolution::ReleaseResolutionResult resolvedRelease(
    const ReleaseMetadata::GitHubRelease& release,
    CoreAssetPlatform platform)
{
    CoreUpdateReleaseResolution::ReleaseResolutionResult result;
    result.release = release;
    result.platform = platform;
    return result;
}

CoreUpdateReleaseResolution::ReleaseResolutionResult failedResolution(
    const OperationResult& error,
    CoreAssetPlatform platform)
{
    CoreUpdateReleaseResolution::ReleaseResolutionResult result;
    result.platform = platform;
    result.error = error;
    result.hasError = true;
    return result;
}

} // namespace

CoreUpdateReleaseResolution::ReleaseResolutionResult CoreUpdateReleaseResolution::resolveRelease(
    const ReleaseResolutionRequest& request)
{
    const ICoreBackend& backend = request.backend;
    const QString displayName = backend.displayName();
    const CoreAssetPlatform platform = currentAssetPlatform();
    const bool noInstalledCore = !InstallFiles::hasAnyInstalledCore(request.targetDirectory);
    const ReleaseMetadata::GitHubRelease builtInFallbackRelease = noInstalledCore
        ? ReleaseMetadata::buildBuiltInFallbackRelease(backend, platform)
        : ReleaseMetadata::GitHubRelease{};
    const CoreUpdateAssetPolicy assetPolicy = backend.updateAssetPolicy(platform);
    const QString directLatestAssetName = platform.sixtyFourBit
        ? assetPolicy.directLatestAssetName64
        : assetPolicy.directLatestAssetName32;
    const QUrl directLatestDownloadUrl = platform.sixtyFourBit
        ? assetPolicy.directLatestDownloadUrl64
        : assetPolicy.directLatestDownloadUrl32;

    if (directLatestDownloadUrl.isValid() && !builtInFallbackRelease.tagName.trimmed().isEmpty()) {
        UpdateOps::reportProgress(
            request.progressHandler,
            QCoreApplication::translate(
                "CoreUpdateService",
                "No local core installation was found. Using built-in bootstrap %1 package: %2 (%3).")
                .arg(displayName)
                .arg(builtInFallbackRelease.assets.constFirst().name)
                .arg(builtInFallbackRelease.tagName));
        return resolvedRelease(builtInFallbackRelease, platform);
    }

    if (directLatestDownloadUrl.isValid() && !directLatestAssetName.isEmpty()) {
        ReleaseMetadata::GitHubRelease release;
        release.tagName = QStringLiteral("latest");
        release.assets.append(ReleaseMetadata::GitHubReleaseAsset{
            directLatestAssetName,
            directLatestDownloadUrl});
        UpdateOps::reportProgress(
            request.progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Using direct latest %1 package: %2")
                .arg(displayName)
                .arg(directLatestAssetName));
        return resolvedRelease(release, platform);
    }

    ReleaseMetadata::GitHubRelease release;
    QString lastError;
    UpdateOps::reportProgress(
        request.progressHandler,
        QCoreApplication::translate("CoreUpdateService", "Checking the latest %1 release...")
            .arg(displayName));

    const QList<QUrl> releaseUrls = buildGitHubMirrorCandidateUrls(backend.releasesApiUrl());
    for (const QUrl& candidateUrl : releaseUrls) {
        if (UpdateOps::isCancellationRequested(request.cancelCheck)) {
            return failedResolution(UpdateOps::cancelledResult(), platform);
        }

        UpdateOps::reportProgress(
            request.progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Requesting release metadata: %1")
                .arg(candidateUrl.toString(QUrl::FullyEncoded)));

        QByteArray payload;
        const OperationResult downloadResult = request.downloadHandler
            ? request.downloadHandler(candidateUrl, &payload)
            : UpdateOps::downloadBytesWithNetwork(
                candidateUrl,
                &payload,
                fallbackUserAgent(),
                request.metadataTimeoutMs,
                request.cancelCheck);
        if (UpdateOps::isCancelledResult(downloadResult)) {
            return failedResolution(downloadResult, platform);
        }
        if (!downloadResult.success) {
            lastError = downloadResult.message;
            UpdateOps::reportProgress(
                request.progressHandler,
                QCoreApplication::translate("CoreUpdateService", "Release metadata request failed: %1")
                    .arg(lastError));
            continue;
        }

        QString parseError;
        if (ReleaseMetadata::parseGitHubReleasePayload(
                payload,
                request.config.checkPreReleaseUpdate,
                &release,
                &parseError,
                nullptr)) {
            lastError.clear();
            break;
        }

        lastError = parseError;
        UpdateOps::reportProgress(
            request.progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Release metadata parsing failed: %1")
                .arg(lastError));
    }

    if (release.tagName.trimmed().isEmpty()
        && !builtInFallbackRelease.tagName.trimmed().isEmpty()) {
        release = builtInFallbackRelease;
        UpdateOps::reportProgress(
            request.progressHandler,
            QCoreApplication::translate(
                "CoreUpdateService",
                "GitHub release metadata was unavailable and no local core installation was found. Falling back to built-in %1 package: %2 (%3).")
                .arg(displayName)
                .arg(release.assets.constFirst().name)
                .arg(release.tagName));
    }

    if (release.tagName.trimmed().isEmpty()) {
        return failedResolution(
            OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Failed to resolve the latest %1 release: %2")
                    .arg(displayName)
                    .arg(lastError.isEmpty()
                        ? QCoreApplication::translate("CoreUpdateService", "Unknown error")
                        : lastError)),
            platform);
    }

    return resolvedRelease(release, platform);
}
