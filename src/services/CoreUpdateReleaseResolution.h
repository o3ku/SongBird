#pragma once

#include <QString>

#include "common/OperationResult.h"
#include "runtime/core/ICoreBackend.h"
#include "services/CoreUpdateReleaseMetadata.h"
#include "services/CoreUpdateService.h"

namespace CoreUpdateReleaseResolution {

struct ReleaseResolutionRequest {
    const ICoreBackend& backend;
    CoreUpdateConfig config;
    QString targetDirectory;
    CoreUpdateService::DownloadHandler downloadHandler;
    CoreUpdateService::CancelCheckHandler cancelCheck;
    CoreUpdateService::ProgressHandler progressHandler;
    int metadataTimeoutMs = 30000;
};

struct ReleaseResolutionResult {
    CoreUpdateReleaseMetadata::GitHubRelease release;
    bool prefer64Bit = false;
    OperationResult error;
    bool hasError = false;
};

ReleaseResolutionResult resolveRelease(const ReleaseResolutionRequest& request);

} // namespace CoreUpdateReleaseResolution
