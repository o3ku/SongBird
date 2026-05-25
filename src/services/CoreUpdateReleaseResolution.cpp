#include "services/CoreUpdateReleaseResolution.h"

#include <QCoreApplication>

#include "common/GitHubMirrorHelper.h"
#include "common/UserAgent.h"
#include "services/CoreUpdateInstallFiles.h"
#include "services/CoreUpdateOperations.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace {

namespace UpdateOps = CoreUpdateOperations;
namespace ReleaseMetadata = CoreUpdateReleaseMetadata;
namespace InstallFiles = CoreUpdateInstallFiles;

bool is64BitOperatingSystem()
{
#if defined(Q_OS_WIN)
    SYSTEM_INFO systemInfo{};
    GetNativeSystemInfo(&systemInfo);
    switch (systemInfo.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
    case PROCESSOR_ARCHITECTURE_ARM64:
        return true;
    default:
        return false;
    }
#else
    return sizeof(void*) >= 8;
#endif
}

CoreUpdateReleaseResolution::ReleaseResolutionResult resolvedRelease(
    const ReleaseMetadata::GitHubRelease& release,
    bool prefer64Bit)
{
    CoreUpdateReleaseResolution::ReleaseResolutionResult result;
    result.release = release;
    result.prefer64Bit = prefer64Bit;
    return result;
}

CoreUpdateReleaseResolution::ReleaseResolutionResult failedResolution(const OperationResult& error, bool prefer64Bit)
{
    CoreUpdateReleaseResolution::ReleaseResolutionResult result;
    result.prefer64Bit = prefer64Bit;
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
    const bool prefer64Bit = is64BitOperatingSystem();
    const bool noInstalledCore = !InstallFiles::hasAnyInstalledCore(request.targetDirectory);
    const ReleaseMetadata::GitHubRelease builtInFallbackRelease = noInstalledCore
        ? ReleaseMetadata::buildBuiltInFallbackRelease(backend, prefer64Bit)
        : ReleaseMetadata::GitHubRelease{};
    const CoreUpdateAssetPolicy assetPolicy = backend.updateAssetPolicy();
    const QString directLatestAssetName = prefer64Bit
        ? assetPolicy.directLatestAssetName64
        : assetPolicy.directLatestAssetName32;
    const QUrl directLatestDownloadUrl = prefer64Bit
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
        return resolvedRelease(builtInFallbackRelease, prefer64Bit);
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
        return resolvedRelease(release, prefer64Bit);
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
            return failedResolution(UpdateOps::cancelledResult(), prefer64Bit);
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
            return failedResolution(downloadResult, prefer64Bit);
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
            prefer64Bit);
    }

    return resolvedRelease(release, prefer64Bit);
}
