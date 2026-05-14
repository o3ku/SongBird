#include "services/GeoResourceUpdateService.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QThread>
#include <QTimer>

#include <utility>

#include "common/GitHubMirrorHelper.h"

namespace {

constexpr int kGeoDownloadTimeoutMs = 30000;

QString normalizeGeoName(const QString& geoName)
{
    return geoName.trimmed().toLower();
}

} // namespace

GeoResourceUpdateService::GeoResourceUpdateService(QString targetDirectory, DownloadHandler downloadHandler)
    : targetDirectory_(std::move(targetDirectory))
    , downloadHandler_(std::move(downloadHandler))
{
}

OperationResult GeoResourceUpdateService::update(const QString& geoName) const
{
    const QString normalizedGeoName = normalizeGeoName(geoName);
    if (normalizedGeoName != QStringLiteral("geosite") && normalizedGeoName != QStringLiteral("geoip")) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Unsupported geo resource: %1.")
                .arg(geoName.trimmed()));
    }

    if (targetDirectory_.trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Geo target directory is unavailable."));
    }

    if (!QDir().mkpath(targetDirectory_)) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to create the geo target directory."));
    }

    QByteArray content;
    const QUrl url = buildDownloadUrl(normalizedGeoName);
    const OperationResult downloadResult = downloadBytes(url, &content);
    if (!downloadResult.success) {
        return downloadResult;
    }

    if (content.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Downloaded %1.dat is empty.")
                .arg(normalizedGeoName));
    }

    const QString targetPath = QDir(targetDirectory_).filePath(QStringLiteral("%1.dat").arg(normalizedGeoName));
    const QFileInfo targetInfo(targetPath);
    if (!targetInfo.dir().exists() && !QDir().mkpath(targetInfo.dir().absolutePath())) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to create the geo file directory."));
    }

    QSaveFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to open %1 for writing.")
                .arg(QDir::toNativeSeparators(targetPath)));
    }

    if (file.write(content) < 0) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to write %1.")
                .arg(QDir::toNativeSeparators(targetPath)));
    }

    if (!file.commit()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to save %1.")
                .arg(QDir::toNativeSeparators(targetPath)));
    }

    return OperationResult::ok(
        QCoreApplication::translate("GeoResourceUpdateService", "Updated %1.")
            .arg(QDir::toNativeSeparators(targetPath)));
}

OperationResult GeoResourceUpdateService::downloadBytes(const QUrl& url, QByteArray* content) const
{
    if (content == nullptr) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Geo download buffer is unavailable."));
    }

    if (!url.isValid() || url.scheme().trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Geo download URL is invalid."));
    }

    QString lastError;
    const QList<QUrl> candidateUrls = buildGitHubMirrorCandidateUrls(url);
    for (const QUrl& candidateUrl : candidateUrls) {
        // Honor cancellation requested by AppBootstrap::waitForBackgroundThreads()
        // at shutdown so we stop trying additional mirrors when the app is exiting.
        QThread* const currentThread = QThread::currentThread();
        if (currentThread != nullptr && currentThread->isInterruptionRequested()) {
            return OperationResult::fail(lastError.isEmpty()
                ? QCoreApplication::translate("GeoResourceUpdateService", "Geo update cancelled by shutdown.")
                : lastError);
        }

        if (downloadHandler_) {
            const OperationResult result = downloadHandler_(candidateUrl, content);
            if (result.success) {
                return result;
            }

            lastError = result.message;
            continue;
        }

        QNetworkAccessManager manager;
        QNetworkRequest request(candidateUrl);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("v2rayq-geo-update"));

        bool timedOut = false;
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        QNetworkReply* reply = manager.get(request);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
            timedOut = true;
            reply->abort();
            loop.quit();
        });
        timeoutTimer.start(kGeoDownloadTimeoutMs);
        loop.exec();
        timeoutTimer.stop();

        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (timedOut) {
            reply->deleteLater();
            lastError = QCoreApplication::translate("GeoResourceUpdateService", "Geo download timed out.");
            continue;
        }

        if (reply->error() != QNetworkReply::NoError) {
            const QString errorText = reply->errorString();
            reply->deleteLater();
            lastError = statusCode > 0
                ? QCoreApplication::translate("GeoResourceUpdateService", "HTTP %1: %2")
                      .arg(statusCode)
                      .arg(errorText)
                : errorText;
            continue;
        }

        *content = reply->readAll();
        reply->deleteLater();

        if (statusCode >= 400) {
            lastError = QCoreApplication::translate("GeoResourceUpdateService", "HTTP %1").arg(statusCode);
            continue;
        }

        return OperationResult::ok();
    }

    return OperationResult::fail(lastError.isEmpty()
            ? QCoreApplication::translate("GeoResourceUpdateService", "Geo download failed.")
            : lastError);
}

QUrl GeoResourceUpdateService::buildDownloadUrl(const QString& geoName)
{
    return QUrl(QStringLiteral("https://github.com/Loyalsoldier/v2ray-rules-dat/releases/latest/download/%1.dat")
                    .arg(normalizeGeoName(geoName)));
}
