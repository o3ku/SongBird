#pragma once

#include <functional>

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "common/OperationResult.h"

struct AppUpdateCheckResult {
    bool updateAvailable = false;
    QString currentVersion;
    QString latestVersion;
    QString releaseName;
    QUrl releaseUrl;
    QString assetName;
    QUrl assetDownloadUrl;
};

class AppUpdateService {
public:
    using DownloadHandler = std::function<OperationResult(const QUrl& url, QByteArray* content)>;

    explicit AppUpdateService(DownloadHandler downloadHandler = {});

    OperationResult checkForUpdate(
        const QString& currentVersion,
        bool allowPrerelease,
        AppUpdateCheckResult* result) const;
    OperationResult downloadUpdate(
        const QUrl& downloadUrl,
        const QString& assetName,
        const QString& targetDirectory,
        QString* savedPath) const;

private:
    DownloadHandler downloadHandler_;
};
