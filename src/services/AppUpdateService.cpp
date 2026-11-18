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
#include "common/JsonFile.h"
#include "common/ReleaseChecksum.h"
#include "common/UserAgent.h"
#include "services/AppUpdateReleaseMetadata.h"
#include "services/CoreUpdateVersion.h"
#include "services/ServiceTimeouts.h"

namespace {

constexpr int kAppUpdateMetadataTimeoutMs = ServiceTimeouts::kDefaultNetworkTimeoutMs;
constexpr int kAppUpdateDownloadTimeoutMs = ServiceTimeouts::kLargeDownloadTimeoutMs;
constexpr int kCancellationPollIntervalMs = ServiceTimeouts::kCancellationPollIntervalMs;

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
    return JsonFile::writeFileAtomically(path, content).ok;
}

} // namespace

AppUpdateService::AppUpdateService(DownloadHandler downloadHandler)
    : downloadHandler_(std::move(downloadHandler))
{
}

OperationResult AppUpdateService::checkForUpdate(
    const QString& currentVersion,
    bool allowPrerelease,
    const CoreAssetPlatform& platform,
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
    // Release metadata decides whether the checksum anchor can be trusted at all, so it
    // is fetched official-first: GitHub first, the mirrors only if GitHub does not
    // answer. A mirror that answered first could hand us a digest and checksum URLs it
    // chose itself, which no later check could tell apart from GitHub's own.
    for (const QUrl& candidateUrl : buildGitHubOfficialFirstCandidateUrls(songbirdReleasesApiUrl())) {
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
            result->metadataFromOfficial = isGitHubOriginUrl(candidateUrl);
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
    if (const AppUpdateReleaseAsset* asset = selectBestAppUpdateAsset(release.assets, platform)) {
        result->assetName = asset->name;
        result->assetDownloadUrl = asset->downloadUrl;
        result->assetDigest = ReleaseChecksum::digestToHex(asset->digest);
        QList<ReleaseChecksum::NamedAsset> releaseAssets;
        releaseAssets.reserve(release.assets.size());
        for (const AppUpdateReleaseAsset& releaseAsset : release.assets) {
            releaseAssets.append(
                ReleaseChecksum::NamedAsset{releaseAsset.name, releaseAsset.downloadUrl});
        }
        if (const std::optional<ReleaseChecksum::NamedAsset> checksumAsset =
                ReleaseChecksum::findChecksumAsset(releaseAssets, asset->name)) {
            result->checksumAssetName = checksumAsset->name;
            result->checksumAssetDownloadUrl = checksumAsset->downloadUrl;
        }
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
    const AppUpdateCheckResult& checkResult,
    const QString& targetDirectory,
    QString* savedPath) const
{
    const QUrl& downloadUrl = checkResult.assetDownloadUrl;
    const QString& assetName = checkResult.assetName;
    if (!downloadUrl.isValid()) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "Update package URL is unavailable."));
    }

    // The published name is what the checksum document lists; the saved name is what the file
    // lands under, which may differ (see AppUpdateCheckResult::downloadFileName).
    const QString publishedName = QFileInfo(assetName.trimmed()).fileName();
    const QString savedName = QFileInfo(checkResult.downloadFileName.trimmed()).fileName();
    const QString normalizedAssetName = savedName.isEmpty() ? publishedName : savedName;
    if (publishedName.isEmpty()) {
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

    // The bytes are verified before anything is written, against the official GitHub
    // channel first: an executable that fails its checksum is refused outright, and one
    // with no official checksum to check against is refused too. SongBird's own releases
    // publish "<asset>.sha256" (see .github/workflows/release.yml) and the API digest
    // covers older releases, so a release with neither is one to fetch by hand from the
    // releases page, not one to install blind.
    //
    // When GitHub itself was unreachable the metadata -- and the package -- came through
    // a mirror, so there is no official anchor at all. The package is then verified
    // against a mirror-supplied checksum instead: a match only proves the bytes agree
    // with the mirror, so the install proceeds with a loud warning rather than silently,
    // and a mismatch still refuses outright. That is the availability tradeoff the
    // mirrors exist for, and the warning is the price of it.
    const std::optional<ReleaseChecksum::NamedAsset> checksumAsset =
        checkResult.checksumAssetDownloadUrl.isValid()
        ? std::optional<ReleaseChecksum::NamedAsset>{ReleaseChecksum::NamedAsset{
            checkResult.checksumAssetName, checkResult.checksumAssetDownloadUrl}}
        : std::nullopt;
    ReleaseChecksum::VerificationRequest request;
    request.packageBytes = packageBytes;
    request.assetName = publishedName;
    request.packageDownloadUrl = downloadUrl;
    request.apiDigest = checkResult.assetDigest;
    request.checksumAsset = checksumAsset;
    request.metadataFromOfficial = checkResult.metadataFromOfficial;
    const auto fetchChecksum = [this](const QUrl& url, QByteArray* content) {
        return downloadHandler_
            ? downloadHandler_(url, content)
            : downloadBytesWithNetwork(url, content, kAppUpdateMetadataTimeoutMs);
    };
    ReleaseChecksum::VerificationOutcome verification =
        ReleaseChecksum::verifyDownloadedBytes(request, fetchChecksum);
    bool mirrorVerified = false;
    if (verification.status == ReleaseChecksum::Verification::Unverifiable
        && !checkResult.metadataFromOfficial) {
        verification = ReleaseChecksum::verifyDownloadedBytesViaMirror(request, fetchChecksum);
        mirrorVerified =
            verification.status == ReleaseChecksum::Verification::MirrorVerified;
    }
    if (verification.status == ReleaseChecksum::Verification::Mismatched) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "SHA-256 checksum mismatch for SongBird %1: the downloaded update does not match the published checksum (expected %2, got %3). The update was not saved.")
                .arg(publishedName)
                .arg(verification.expectedHex)
                .arg(verification.actualHex));
    }
    if (verification.status == ReleaseChecksum::Verification::Unverifiable) {
        return OperationResult::fail(
            QCoreApplication::translate("AppUpdateService", "No SHA-256 checksum is available for SongBird %1, so the downloaded update cannot be verified. The update was not saved; download it manually from the releases page if this persists.")
                .arg(publishedName));
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

    QString savedMessage =
        QCoreApplication::translate("AppUpdateService", "SongBird update downloaded: %1")
            .arg(QDir::toNativeSeparators(packagePath));
    if (mirrorVerified) {
        savedMessage += QStringLiteral(" ") + QCoreApplication::translate(
            "AppUpdateService",
            "Warning: GitHub was unreachable, so this update was verified against a checksum from a download mirror only. A mirror can serve any bytes it chooses, so install it only if you trust the mirror that served it.");
    }

    return OperationResult::ok(savedMessage, true);
}
