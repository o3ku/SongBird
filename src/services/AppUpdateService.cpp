#include "services/AppUpdateService.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

namespace {

constexpr int kAppUpdateMetadataTimeoutMs = 30000;
constexpr int kAppUpdateDownloadTimeoutMs = 180000;
constexpr int kCancellationPollIntervalMs = 100;

struct GitHubAppAsset {
    QString name;
    QUrl downloadUrl;
};

struct GitHubAppRelease {
    QString tagName;
    QString name;
    QUrl htmlUrl;
    bool prerelease = false;
    bool draft = false;
    QList<GitHubAppAsset> assets;
};

QString normalizeVersionTag(QString value)
{
    value = value.trimmed().toLower();
    if (value.startsWith(QStringLiteral("version "))) {
        value = value.mid(QStringLiteral("version ").size()).trimmed();
    }
    if (!value.isEmpty() && value.front().isDigit()) {
        value.prepend(QChar('v'));
    }
    return value;
}

QList<int> parseVersionParts(QString version)
{
    version = normalizeVersionTag(version);
    if (version.startsWith(QChar('v'))) {
        version = version.mid(1);
    }
    const int dashIndex = version.indexOf(QChar('-'));
    if (dashIndex >= 0) {
        version = version.left(dashIndex);
    }

    QList<int> parts;
    for (const QString& segment : version.split(QChar('.'))) {
        bool ok = false;
        const int value = segment.toInt(&ok);
        parts.append(ok ? value : 0);
    }
    return parts;
}

bool isVersionNewerThan(const QString& candidate, const QString& current)
{
    const QList<int> candidateParts = parseVersionParts(candidate);
    const QList<int> currentParts = parseVersionParts(current);
    const int maxLen = qMax(candidateParts.size(), currentParts.size());
    for (int i = 0; i < maxLen; ++i) {
        const int c = i < candidateParts.size() ? candidateParts.at(i) : 0;
        const int r = i < currentParts.size() ? currentParts.at(i) : 0;
        if (c > r) {
            return true;
        }
        if (c < r) {
            return false;
        }
    }
    return false;
}

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

const GitHubAppAsset* selectBestAsset(const QList<GitHubAppAsset>& assets)
{
    const GitHubAppAsset* bestAsset = nullptr;
    int bestScore = -1;
    for (const GitHubAppAsset& asset : assets) {
        const QString normalized = asset.name.trimmed().toLower();
        int score = -1;
        if (normalized.endsWith(QStringLiteral(".exe"))) {
            score = normalized.contains(QStringLiteral("setup"))
                || normalized.contains(QStringLiteral("install"))
                ? 500
                : 300;
        }

        if (!normalized.contains(QStringLiteral("windows"))
            && !normalized.contains(QStringLiteral("win"))
            && !normalized.contains(QStringLiteral("songbird"))) {
            score -= 100;
        }
        if (normalized.contains(QStringLiteral("source"))
            || normalized.contains(QStringLiteral("linux"))
            || normalized.contains(QStringLiteral("macos"))
            || normalized.contains(QStringLiteral("darwin"))
            || normalized.contains(QStringLiteral("android"))) {
            score = -1;
        }

        if (score > bestScore && asset.downloadUrl.isValid()) {
            bestScore = score;
            bestAsset = &asset;
        }
    }
    return bestScore >= 0 ? bestAsset : nullptr;
}

bool parseLatestRelease(
    const QByteArray& payload,
    bool allowPrerelease,
    GitHubAppRelease* release,
    QString* errorMessage)
{
    if (release == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("AppUpdateService", "Release output buffer is unavailable.");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = parseError.errorString();
        }
        return false;
    }
    if (!document.isArray()) {
        if (errorMessage != nullptr) {
            const QString message = document.isObject()
                ? document.object().value(QStringLiteral("message")).toString().trimmed()
                : QString();
            *errorMessage = message.isEmpty()
                ? QCoreApplication::translate("AppUpdateService", "Release metadata is invalid.")
                : message;
        }
        return false;
    }

    bool skippedOnlyPrerelease = false;
    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        const bool draft = object.value(QStringLiteral("draft")).toBool(false);
        const bool prerelease = object.value(QStringLiteral("prerelease")).toBool(false);
        if (draft) {
            continue;
        }
        if (!allowPrerelease && prerelease) {
            skippedOnlyPrerelease = true;
            continue;
        }

        const QString tagName = object.value(QStringLiteral("tag_name")).toString().trimmed();
        const QUrl htmlUrl(object.value(QStringLiteral("html_url")).toString().trimmed());
        if (tagName.isEmpty()) {
            continue;
        }

        GitHubAppRelease parsedRelease{
            tagName,
            object.value(QStringLiteral("name")).toString().trimmed(),
            htmlUrl,
            prerelease,
            draft,
            {}};
        const QJsonArray assets = object.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue& assetValue : assets) {
            const QJsonObject assetObject = assetValue.toObject();
            const QString assetName = assetObject.value(QStringLiteral("name")).toString().trimmed();
            const QUrl downloadUrl(assetObject.value(QStringLiteral("browser_download_url")).toString().trimmed());
            if (!assetName.isEmpty() && downloadUrl.isValid()) {
                parsedRelease.assets.append(GitHubAppAsset{assetName, downloadUrl});
            }
        }
        *release = parsedRelease;
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = skippedOnlyPrerelease && !allowPrerelease
            ? QCoreApplication::translate("AppUpdateService", "No stable releases were found.")
            : QCoreApplication::translate("AppUpdateService", "No releases were found.");
    }
    return false;
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
    GitHubAppRelease release;
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
        if (parseLatestRelease(payload, allowPrerelease, &release, &parseError)) {
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
    if (const GitHubAppAsset* asset = selectBestAsset(release.assets)) {
        result->assetName = asset->name;
        result->assetDownloadUrl = asset->downloadUrl;
    }
    result->updateAvailable = isVersionNewerThan(release.tagName, currentVersion);

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
