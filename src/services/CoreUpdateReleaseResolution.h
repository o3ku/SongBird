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
    CoreAssetPlatform platform;
    OperationResult error;
    bool hasError = false;
    // True when the release metadata above arrived over the official GitHub channel
    // (api.github.com). The metadata fetch tries GitHub first and the mirrors only as a
    // fallback, so false means GitHub itself was unreachable and the mirrors answered --
    // in which case the API digest and any checksum URL the metadata names are
    // mirror-supplied and cannot anchor verification (see ReleaseChecksum.h).
    bool metadataFromOfficial = false;
    // True when the release was constructed locally (the built-in bootstrap, or the direct
    // latest URL) rather than read from the releases API. Such a release carries no API digest
    // and no checksum asset list, so verification is best-effort there instead of fail-closed:
    // refusing would brick a first run whenever the API is unreachable.
    bool fromBuiltInFallback = false;
};

ReleaseResolutionResult resolveRelease(const ReleaseResolutionRequest& request);

} // namespace CoreUpdateReleaseResolution
