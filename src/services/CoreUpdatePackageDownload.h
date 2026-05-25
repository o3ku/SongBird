#pragma once

#include <QByteArray>
#include <QString>

#include "common/OperationResult.h"
#include "services/CoreUpdateReleaseMetadata.h"
#include "services/CoreUpdateService.h"

namespace CoreUpdatePackageDownload {

struct PackageDownloadResult {
    QByteArray packageBytes;
    OperationResult error;
    bool hasError = false;
};

PackageDownloadResult downloadPackage(
    const QString& displayName,
    const CoreUpdateReleaseMetadata::GitHubReleaseAsset& asset,
    const CoreUpdateService::DownloadHandler& downloadHandler,
    const CoreUpdateService::CancelCheckHandler& cancelCheck,
    const CoreUpdateService::ProgressHandler& progressHandler);

} // namespace CoreUpdatePackageDownload
