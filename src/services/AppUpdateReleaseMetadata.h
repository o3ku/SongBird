#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

#include "runtime/core/CoreAssetPlatform.h"

struct AppUpdateReleaseAsset {
    QString name;
    QUrl downloadUrl;
    // The `digest` the releases API returns for the asset ("sha256:<hex>").
    QString digest;
};

struct AppUpdateRelease {
    QString tagName;
    QString name;
    QUrl htmlUrl;
    bool prerelease = false;
    bool draft = false;
    QList<AppUpdateReleaseAsset> assets;
};

const AppUpdateReleaseAsset* selectBestAppUpdateAsset(
    const QList<AppUpdateReleaseAsset>& assets,
    const CoreAssetPlatform& platform);
bool parseLatestAppRelease(
    const QByteArray& payload,
    bool allowPrerelease,
    AppUpdateRelease* release,
    QString* errorMessage);
