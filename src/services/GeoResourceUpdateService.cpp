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

#include <memory>
#include <utility>

#include "common/DataSizeFormatter.h"
#include "common/GitHubMirrorHelper.h"
#include "common/UserAgent.h"

namespace {

constexpr int kGeoDownloadTimeoutMs = 30000;
const QString kSingBoxRuleSetDirectoryName = QStringLiteral("rule-set");

QString normalizeGeoName(const QString& geoName)
{
    return geoName.trimmed().toLower();
}

QString normalizeRuleSetTag(const QString& tag)
{
    return tag.trimmed().toLower();
}

} // namespace

GeoResourceUpdateService::GeoResourceUpdateService(
    QString targetDirectory,
    DownloadHandler downloadHandler,
    ProgressHandler progressHandler)
    : targetDirectory_(std::move(targetDirectory))
    , downloadHandler_(std::move(downloadHandler))
    , progressHandler_(std::move(progressHandler))
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

    const QString fileName = QStringLiteral("%1.dat").arg(normalizedGeoName);
    return downloadAndSave(
        buildDownloadUrl(normalizedGeoName),
        fileName,
        QDir(targetDirectory_).filePath(fileName));
}

OperationResult GeoResourceUpdateService::updateSingBoxRuleSet(const QString& tag) const
{
    const QString normalizedTag = normalizeRuleSetTag(tag);
    if (!normalizedTag.startsWith(QStringLiteral("geosite-"))
        && !normalizedTag.startsWith(QStringLiteral("geoip-"))) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Unsupported sing-box rule-set: %1.")
                .arg(tag.trimmed()));
    }

    if (targetDirectory_.trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Geo target directory is unavailable."));
    }

    const QString fileName = QStringLiteral("%1.srs").arg(normalizedTag);
    return downloadAndSave(
        buildSingBoxRuleSetDownloadUrl(normalizedTag),
        fileName,
        QDir(targetDirectory_).filePath(QStringLiteral("%1/%2").arg(kSingBoxRuleSetDirectoryName, fileName)));
}

OperationResult GeoResourceUpdateService::downloadAndSave(
    const QUrl& url,
    const QString& fileName,
    const QString& targetPath) const
{
    if (targetDirectory_.trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Geo target directory is unavailable."));
    }

    if (!QDir().mkpath(targetDirectory_)) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to create the geo target directory."));
    }

    reportProgress(
        QCoreApplication::translate("GeoResourceUpdateService", "Downloading %1...")
            .arg(fileName));

    QByteArray content;
    const OperationResult downloadResult = downloadBytes(url, fileName, &content);
    if (!downloadResult.success) {
        return downloadResult;
    }

    if (content.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Downloaded %1 is empty.")
                .arg(fileName));
    }

    reportProgress(
        QCoreApplication::translate("GeoResourceUpdateService", "Saving %1...")
            .arg(QDir::toNativeSeparators(targetPath)));
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

void GeoResourceUpdateService::reportProgress(const QString& message) const
{
    if (progressHandler_ && !message.trimmed().isEmpty()) {
        progressHandler_(message);
    }
}

OperationResult GeoResourceUpdateService::downloadBytes(const QUrl& url, const QString& fileName, QByteArray* content) const
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

        reportProgress(
            QCoreApplication::translate("GeoResourceUpdateService", "Trying download source: %1")
                .arg(candidateUrl.toString()));

        if (downloadHandler_) {
            const OperationResult result = downloadHandler_(candidateUrl, content);
            if (result.success) {
                reportProgress(
                    QCoreApplication::translate("GeoResourceUpdateService", "Downloaded %1 (%2).")
                        .arg(fileName, formatByteCount(static_cast<quint64>(content->size()))));
                return result;
            }

            lastError = result.message;
            continue;
        }

        QNetworkAccessManager manager;
        QNetworkRequest request(candidateUrl);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, fallbackUserAgent());

        bool timedOut = false;
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);

        QNetworkReply* reply = manager.get(request);
        auto lastReportedPercent = std::make_shared<int>(-1);
        auto lastReportedBytes = std::make_shared<qint64>(0);
        QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [this, fileName, lastReportedPercent, lastReportedBytes](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesReceived < 0) {
                return;
            }

            if (bytesTotal > 0) {
                const int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
                if (percent < 100
                    && *lastReportedPercent >= 0
                    && percent - *lastReportedPercent < 5) {
                    return;
                }

                *lastReportedPercent = percent;
                reportProgress(
                    QCoreApplication::translate("GeoResourceUpdateService", "Downloading %1... %2% (%3/%4)")
                        .arg(fileName)
                        .arg(percent)
                        .arg(formatByteCount(static_cast<quint64>(bytesReceived)))
                        .arg(formatByteCount(static_cast<quint64>(bytesTotal))));
                return;
            }

            constexpr qint64 kUnknownTotalProgressStepBytes = 512 * 1024;
            if (bytesReceived < kUnknownTotalProgressStepBytes
                || bytesReceived - *lastReportedBytes < kUnknownTotalProgressStepBytes) {
                return;
            }

            *lastReportedBytes = bytesReceived;
            reportProgress(
                QCoreApplication::translate("GeoResourceUpdateService", "Downloading %1... %2")
                    .arg(fileName, formatByteCount(static_cast<quint64>(bytesReceived))));
        });
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

        reportProgress(
            QCoreApplication::translate("GeoResourceUpdateService", "Downloaded %1 (%2).")
                .arg(fileName, formatByteCount(static_cast<quint64>(content->size()))));
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

QUrl GeoResourceUpdateService::buildSingBoxRuleSetDownloadUrl(const QString& tag)
{
    const QString normalizedTag = normalizeRuleSetTag(tag);
    if (normalizedTag.startsWith(QStringLiteral("geosite-"))) {
        return QUrl(QStringLiteral("https://raw.githubusercontent.com/SagerNet/sing-geosite/rule-set/%1.srs")
                        .arg(normalizedTag));
    }
    if (normalizedTag.startsWith(QStringLiteral("geoip-"))) {
        return QUrl(QStringLiteral("https://raw.githubusercontent.com/SagerNet/sing-geoip/rule-set/%1.srs")
                        .arg(normalizedTag));
    }
    return {};
}
