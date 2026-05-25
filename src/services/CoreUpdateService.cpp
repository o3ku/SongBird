#include "services/CoreUpdateService.h"

#include <QCoreApplication>
#include <QDir>

#include <utility>

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
    const bool prefer64Bit = releaseResult.prefer64Bit;
    const GitHubReleaseAsset* asset = selectBestReleaseAsset(backend, release.assets, prefer64Bit);
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
            progressHandler);
    if (downloadResult.hasError) {
        return downloadResult.error;
    }
    const QByteArray& packageBytes = downloadResult.packageBytes;

    if (isCancellationRequested(cancelCheck)) {
        return cancelledResult();
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

    return OperationResult::ok(
        QCoreApplication::translate("CoreUpdateService", "Updated %1 to %2 in %3.")
            .arg(displayName)
            .arg(release.tagName)
            .arg(QDir::toNativeSeparators(normalizedTargetDirectory)),
        true);
}
