#pragma once

#include <QByteArray>
#include <QString>

#include "common/OperationResult.h"
#include "services/CoreUpdateReleaseMetadata.h"
#include "services/CoreUpdateService.h"

namespace CoreUpdatePackageInstallation {

OperationResult installPackage(
    const QString& targetDirectory,
    const CoreUpdateReleaseMetadata::GitHubReleaseAsset& asset,
    const QByteArray& packageBytes,
    bool ignoreGeoUpdateCore,
    const CoreUpdateService::ArchiveExtractor& archiveExtractor,
    const CoreUpdateService::CancelCheckHandler& cancelCheck,
    const CoreUpdateService::ProgressHandler& progressHandler);

} // namespace CoreUpdatePackageInstallation
