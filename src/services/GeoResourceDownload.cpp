#include "services/GeoResourceDownload.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>

#include <memory>

#include "common/DataSizeFormatter.h"
#include "common/GitHubMirrorHelper.h"
#include "common/UserAgent.h"

namespace {

constexpr int kGeoDownloadTimeoutMs = 30000;

void reportProgress(
    const GeoResourceUpdateService::ProgressHandler& progressHandler,
    const QString& message)
{
    if (progressHandler && !message.trimmed().isEmpty()) {
        progressHandler(message);
    }
}

} // namespace

OperationResult GeoResourceDownload::downloadBytes(
    const QUrl& url,
    const QString& fileName,
    QByteArray* content,
    const GeoResourceUpdateService::DownloadHandler& downloadHandler,
    const GeoResourceUpdateService::ProgressHandler& progressHandler)
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
            progressHandler,
            QCoreApplication::translate("GeoResourceUpdateService", "Trying download source: %1")
                .arg(candidateUrl.toString()));

        if (downloadHandler) {
            const OperationResult result = downloadHandler(candidateUrl, content);
            if (result.success) {
                reportProgress(
                    progressHandler,
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
        QObject::connect(
            reply,
            &QNetworkReply::downloadProgress,
            reply,
            [fileName, progressHandler, lastReportedPercent, lastReportedBytes](qint64 bytesReceived, qint64 bytesTotal) {
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
                        progressHandler,
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
                    progressHandler,
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
            progressHandler,
            QCoreApplication::translate("GeoResourceUpdateService", "Downloaded %1 (%2).")
                .arg(fileName, formatByteCount(static_cast<quint64>(content->size()))));
        return OperationResult::ok();
    }

    return OperationResult::fail(lastError.isEmpty()
            ? QCoreApplication::translate("GeoResourceUpdateService", "Geo download failed.")
            : lastError);
}
