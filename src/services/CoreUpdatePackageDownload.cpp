#include "services/CoreUpdatePackageDownload.h"

#include <QCoreApplication>

#include "common/GitHubMirrorHelper.h"
#include "common/UserAgent.h"
#include "services/CoreUpdateOperations.h"

namespace {

constexpr int kCoreDownloadTimeoutMs = 180000;

CoreUpdatePackageDownload::PackageDownloadResult downloadedPackage(const QByteArray& packageBytes)
{
    CoreUpdatePackageDownload::PackageDownloadResult result;
    result.packageBytes = packageBytes;
    return result;
}

CoreUpdatePackageDownload::PackageDownloadResult failedDownload(const OperationResult& error)
{
    CoreUpdatePackageDownload::PackageDownloadResult result;
    result.error = error;
    result.hasError = true;
    return result;
}

} // namespace

CoreUpdatePackageDownload::PackageDownloadResult CoreUpdatePackageDownload::downloadPackage(
    const QString& displayName,
    const CoreUpdateReleaseMetadata::GitHubReleaseAsset& asset,
    const CoreUpdateService::DownloadHandler& downloadHandler,
    const CoreUpdateService::CancelCheckHandler& cancelCheck,
    const CoreUpdateService::ProgressHandler& progressHandler)
{
    namespace UpdateOps = CoreUpdateOperations;

    if (UpdateOps::isCancellationRequested(cancelCheck)) {
        return failedDownload(UpdateOps::cancelledResult());
    }

    UpdateOps::reportProgress(
        progressHandler,
        QCoreApplication::translate("CoreUpdateService", "Downloading %1 %2...")
            .arg(displayName)
            .arg(asset.name));

    QByteArray packageBytes;
    QString lastError;
    for (const QUrl& candidateUrl : buildGitHubMirrorCandidateUrls(asset.downloadUrl)) {
        if (UpdateOps::isCancellationRequested(cancelCheck)) {
            return failedDownload(UpdateOps::cancelledResult());
        }

        UpdateOps::reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Trying download source: %1")
                .arg(candidateUrl.toString(QUrl::FullyEncoded)));

        const OperationResult downloadResult = downloadHandler
            ? downloadHandler(candidateUrl, &packageBytes)
            : UpdateOps::downloadBytesWithNetwork(
                candidateUrl,
                &packageBytes,
                fallbackUserAgent(),
                kCoreDownloadTimeoutMs,
                cancelCheck,
                progressHandler);
        if (UpdateOps::isCancelledResult(downloadResult)) {
            return failedDownload(downloadResult);
        }
        if (downloadResult.success && !packageBytes.isEmpty()) {
            lastError.clear();
            UpdateOps::reportProgress(
                progressHandler,
                QCoreApplication::translate("CoreUpdateService", "Downloaded %1 (%2 bytes).")
                    .arg(asset.name)
                    .arg(packageBytes.size()));
            return downloadedPackage(packageBytes);
        }

        lastError = downloadResult.success
            ? QCoreApplication::translate("CoreUpdateService", "Downloaded package is empty.")
            : downloadResult.message;
        UpdateOps::reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Download attempt failed: %1")
                .arg(lastError));
        packageBytes.clear();
    }

    return failedDownload(
        OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to download %1: %2")
                .arg(asset.name)
                .arg(lastError.isEmpty()
                    ? QCoreApplication::translate("CoreUpdateService", "Unknown error")
                    : lastError)));
}
