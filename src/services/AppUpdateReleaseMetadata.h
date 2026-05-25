#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

struct AppUpdateReleaseAsset {
    QString name;
    QUrl downloadUrl;
};

struct AppUpdateRelease {
    QString tagName;
    QString name;
    QUrl htmlUrl;
    bool prerelease = false;
    bool draft = false;
    QList<AppUpdateReleaseAsset> assets;
};

const AppUpdateReleaseAsset* selectBestAppUpdateAsset(const QList<AppUpdateReleaseAsset>& assets);
bool parseLatestAppRelease(
    const QByteArray& payload,
    bool allowPrerelease,
    AppUpdateRelease* release,
    QString* errorMessage);
