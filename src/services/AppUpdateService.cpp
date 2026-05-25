#include "services/AppUpdateService.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QIODevice>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QList>
#include <QSaveFile>
#include <QThread>
#include <QTimer>

#include <utility>

#include "common/GitHubMirrorHelper.h"
#include "common/GitHubUrls.h"
#include "common/UserAgent.h"
#include "services/AppUpdateReleaseMetadata.h"
#include "services/CoreUpdateVersion.h"

namespace {

constexpr int kAppUpdateMetadataTimeoutMs = 30000;
constexpr int kAppUpdateDownloadTimeoutMs = 180000;
constexpr int kCancellationPollIntervalMs = 100;

OperationResult downloadBytesWithNetwork(const QUrl& url, QByteArray* content, int timeoutMs)
{
    if (content == nullptr) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "Update metadata buffer is unavailable."));
    }
    if (!url.isValid() || url.scheme().trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "Update metadata URL is invalid."));
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, fallbackUserAgent());

    bool timedOut = false;
    bool cancelled = false;
    QEventLoop loop;
    QTimer timeoutTimer;
    QTimer cancellationTimer;
    timeoutTimer.setSingleShot(true);
    cancellationTimer.setInterval(kCancellationPollIntervalMs);

    QNetworkReply* reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&]() {
        QThread* const currentThread = QThread::currentThread();
        if (currentThread == nullptr || !currentThread->isInterruptionRequested()) {
            return;
        }

        cancelled = true;
        reply->abort();
        loop.quit();
    });

    timeoutTimer.start(timeoutMs);
    cancellationTimer.start();
    loop.exec();
    timeoutTimer.stop();
    cancellationTimer.stop();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (cancelled) {
        reply->deleteLater();
        return OperationResult::cancel(
            QCoreApplication::translate("AppUpdateService", "Update check was canceled."));
    }
    if (timedOut) {
        reply->deleteLater();
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "Update metadata request timed out."));
    }
    if (reply->error() != QNetworkReply::NoError) {
        const QString errorText = reply->errorString();
        reply->deleteLater();
        return statusCode > 0
            ? OperationResult::fail(
                QCoreApplication::translate("AppUpdateService", "HTTP %1: %2")
                    .arg(statusCode)
                    .arg(errorText))
            : OperationResult::fail(errorText);
    }

    *content = reply->readAll();
    reply->deleteLater();
    if (statusCode >= 400) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "HTTP %1").arg(statusCode));
    }

    return OperationResult::ok();
}

bool writeBytesToFile(const QString& path, const QByteArray& content)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(content) != content.size()) {
        return false;
    }
    return file.commit();
}

} // namespace

AppUpdateService::AppUpdateService(DownloadHandler downloadHandler)
    : downloadHandler_(std::move(downloadHandler))
{
}

OperationResult AppUpdateService::checkForUpdate(
    const QString& currentVersion,
    bool allowPrerelease,
    AppUpdateCheckResult* result) const
{
    if (result == nullptr) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "Update check output buffer is unavailable."));
    }

    *result = {};
    result->currentVersion = currentVersion.trimmed();

    QString lastError;
    AppUpdateRelease release;
    for (const QUrl& candidateUrl : buildGitHubMirrorCandidateUrls(songbirdReleasesApiUrl())) {
        QThread* const currentThread = QThread::currentThread();
        if (currentThread != nullptr && currentThread->isInterruptionRequested()) {
            return OperationResult::cancel(
                QCoreApplication::translate("AppUpdateService", "Update check was canceled."));
        }

        QByteArray payload;
        const OperationResult downloadResult = downloadHandler_
            ? downloadHandler_(candidateUrl, &payload)
            : downloadBytesWithNetwork(candidateUrl, &payload, kAppUpdateMetadataTimeoutMs);
        if (downloadResult.cancelled) {
            return downloadResult;
        }
        if (!downloadResult.success) {
            lastError = downloadResult.message;
            continue;
        }

        QString parseError;
        if (parseLatestAppRelease(payload, allowPrerelease, &release, &parseError)) {
            lastError.clear();
            break;
        }
        lastError = parseError;
    }

    if (release.tagName.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "Failed to check the latest SongBird release: %1")
                .arg(lastError.isEmpty()
                    ? QCoreApplication::translate("AppUpdateService", "Unknown error")
                    : lastError));
    }

    result->latestVersion = release.tagName;
    result->releaseName = release.name;
    result->releaseUrl = release.htmlUrl.isValid()
        ? release.htmlUrl
        : QUrl(songbirdReleasePageUrl());
    if (const AppUpdateReleaseAsset* asset = selectBestAppUpdateAsset(release.assets)) {
        result->assetName = asset->name;
        result->assetDownloadUrl = asset->downloadUrl;
    }
    result->updateAvailable = CoreUpdateVersion::isNewerThan(release.tagName, currentVersion);

    if (result->updateAvailable) {
        return OperationResult::ok(
            QCoreApplication::translate("AppUpdateService", "SongBird %1 is available.")
                .arg(release.tagName));
    }

    return OperationResult::ok(
        QCoreApplication::translate("AppUpdateService", "SongBird is up to date: %1.")
            .arg(currentVersion.trimmed().isEmpty() ? release.tagName : currentVersion.trimmed()));
}

OperationResult AppUpdateService::downloadUpdate(
    const QUrl& downloadUrl,
    const QString& assetName,
    const QString& targetDirectory,
    QString* savedPath) const
{
    if (!downloadUrl.isValid()) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "Update package URL is unavailable."));
    }

    const QString normalizedAssetName = QFileInfo(assetName.trimmed()).fileName();
    if (normalizedAssetName.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "Update package name is unavailable."));
    }

    QDir directory(targetDirectory);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "Failed to create update directory: %1")
                .arg(QDir::toNativeSeparators(targetDirectory)));
    }

    QByteArray packageBytes;
    QString lastError;
    for (const QUrl& candidateUrl : buildGitHubMirrorCandidateUrls(downloadUrl)) {
        QThread* const currentThread = QThread::currentThread();
        if (currentThread != nullptr && currentThread->isInterruptionRequested()) {
            return OperationResult::cancel(
                QCoreApplication::translate("AppUpdateService", "Update download was canceled."));
        }

        packageBytes.clear();
        const OperationResult downloadResult = downloadHandler_
            ? downloadHandler_(candidateUrl, &packageBytes)
            : downloadBytesWithNetwork(candidateUrl, &packageBytes, kAppUpdateDownloadTimeoutMs);
        if (downloadResult.cancelled) {
            return downloadResult;
        }
        if (downloadResult.success && !packageBytes.isEmpty()) {
            lastError.clear();
            break;
        }
        lastError = downloadResult.success
            ? QCoreApplication::translate("AppUpdateService", "Downloaded update package is empty.")
            : downloadResult.message;
    }

    if (packageBytes.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "Failed to download SongBird update: %1")
                .arg(lastError.isEmpty()
                    ? QCoreApplication::translate("AppUpdateService", "Unknown error")
                    : lastError));
    }

    const QString packagePath = directory.filePath(normalizedAssetName);
    if (!writeBytesToFile(packagePath, packageBytes)) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "Failed to save update package: %1")
                .arg(QDir::toNativeSeparators(packagePath)));
    }

    if (savedPath != nullptr) {
        *savedPath = packagePath;
    }

    return OperationResult::ok(
        QCoreApplication::translate("AppUpdateService", "SongBird update downloaded: %1")
            .arg(QDir::toNativeSeparators(packagePath)),
        true);
}
