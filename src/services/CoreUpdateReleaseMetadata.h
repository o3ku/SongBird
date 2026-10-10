#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

#include "runtime/core/CoreAssetPlatform.h"

class ICoreBackend;

namespace CoreUpdateReleaseMetadata {

struct GitHubReleaseAsset {
    QString name;
    QUrl downloadUrl;
};

struct GitHubRelease {
    QString tagName;
    bool prerelease = false;
    QList<GitHubReleaseAsset> assets;
};

GitHubRelease buildBuiltInFallbackRelease(const ICoreBackend& backend, CoreAssetPlatform platform);
bool parseGitHubReleasePayload(
    const QByteArray& payload,
    bool allowPrerelease,
    GitHubRelease* release,
    QString* errorMessage,
    bool* stableReleaseUnavailable = nullptr);
const GitHubReleaseAsset* selectBestReleaseAsset(
    const ICoreBackend& backend,
    const QList<GitHubReleaseAsset>& assets,
    CoreAssetPlatform platform);

} // namespace CoreUpdateReleaseMetadata
