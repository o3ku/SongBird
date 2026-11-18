#pragma once

#include <functional>

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "common/OperationResult.h"
#include "runtime/core/CoreAssetPlatform.h"

struct AppUpdateCheckResult {
    bool updateAvailable = false;
    QString currentVersion;
    QString latestVersion;
    QString releaseName;
    QUrl releaseUrl;
    QString assetName;
    QUrl assetDownloadUrl;
    // The `digest` the releases API returns for the asset, normalised to lowercase hex.
    // Empty when the API carried none (older releases predate the field).
    QString assetDigest;
    // The checksum asset for the package in the same release, if one is published
    // ("<asset>.sha256" once the release workflow publishes it, "<asset>.dgst", or
    // "checksums.txt"). The download verifies against the digest first and this second.
    QString checksumAssetName;
    QUrl checksumAssetDownloadUrl;
    // The local name to save the package under. The coordinator sets this: the running
    // executable cannot be overwritten on Windows, so a "*.exe" asset is saved as
    // "SongBird.new.exe". Empty means the asset name. Verification always uses `assetName`,
    // the name the release published the checksum under, never this.
    QString downloadFileName;
};

class AppUpdateService {
public:
    using DownloadHandler = std::function<OperationResult(const QUrl& url, QByteArray* content)>;

    explicit AppUpdateService(DownloadHandler downloadHandler = {});

    OperationResult checkForUpdate(
        const QString& currentVersion,
        bool allowPrerelease,
        const CoreAssetPlatform& platform,
        AppUpdateCheckResult* result) const;
    // Downloads the package `checkResult` describes and verifies its SHA-256 before saving it:
    // the API digest first, then the checksum asset in the same release, then a checksum file
    // derived from the package URL. A mismatch, or no checksum source at all, refuses the
    // download and saves nothing -- an unverified executable is never written to disk.
    OperationResult downloadUpdate(
        const AppUpdateCheckResult& checkResult,
        const QString& targetDirectory,
        QString* savedPath) const;

private:
    DownloadHandler downloadHandler_;
};
