#include "services/CoreUpdateOperations.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTimer>

#include <memory>

namespace {

constexpr int kCancellationPollIntervalMs = 100;

QString quotePowerShellLiteral(QString value)
{
    value.replace(QChar('\''), QStringLiteral("''"));
    return value;
}

} // namespace

namespace CoreUpdateOperations {

void reportProgress(const CoreUpdateService::ProgressHandler& progressHandler, const QString& message)
{
    if (progressHandler && !message.trimmed().isEmpty()) {
        progressHandler(message);
    }
}

bool isCancellationRequested(const CoreUpdateService::CancelCheckHandler& cancelCheck)
{
    return cancelCheck && cancelCheck();
}

OperationResult cancelledResult()
{
    return OperationResult::cancel(
        QCoreApplication::translate("CoreUpdateService", "Core update was canceled."));
}

bool isCancelledResult(const OperationResult& result)
{
    return result.cancelled;
}

OperationResult downloadBytesWithNetwork(
    const QUrl& url,
    QByteArray* content,
    const QString& userAgent,
    int timeoutMs,
    const CoreUpdateService::CancelCheckHandler& cancelCheck,
    const CoreUpdateService::ProgressHandler& progressHandler)
{
    if (content == nullptr) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Core download buffer is unavailable."));
    }

    if (!url.isValid() || url.scheme().trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Core download URL is invalid."));
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);

    bool timedOut = false;
    bool cancelled = false;
    QEventLoop loop;
    QTimer timeoutTimer;
    QTimer cancellationTimer;
    timeoutTimer.setSingleShot(true);
    cancellationTimer.setInterval(kCancellationPollIntervalMs);

    QNetworkReply* reply = manager.get(request);
    auto lastReportedPercent = std::make_shared<int>(-1);
    auto lastReportedBytes = std::make_shared<qint64>(0);
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [progressHandler, lastReportedPercent, lastReportedBytes](qint64 bytesReceived, qint64 bytesTotal) {
        if (!progressHandler || bytesReceived < 0) {
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
                QCoreApplication::translate("CoreUpdateService", "Downloading... %1%")
                    .arg(percent));
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
            QCoreApplication::translate("CoreUpdateService", "Downloading... %1 bytes")
                .arg(bytesReceived));
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    QObject::connect(&cancellationTimer, &QTimer::timeout, &loop, [&]() {
        if (!isCancellationRequested(cancelCheck)) {
            return;
        }

        cancelled = true;
        reply->abort();
        loop.quit();
    });
    timeoutTimer.start(timeoutMs);
    if (cancelCheck) {
        cancellationTimer.start();
    }
    loop.exec();
    timeoutTimer.stop();
    cancellationTimer.stop();

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (cancelled) {
        reply->deleteLater();
        return cancelledResult();
    }

    if (timedOut) {
        reply->deleteLater();
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Core download timed out."));
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString errorText = reply->errorString();
        reply->deleteLater();
        return statusCode > 0
            ? OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "HTTP %1: %2")
                    .arg(statusCode)
                    .arg(errorText))
            : OperationResult::fail(errorText);
    }

    *content = reply->readAll();
    reply->deleteLater();

    if (statusCode >= 400) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "HTTP %1").arg(statusCode));
    }

    return OperationResult::ok();
}

OperationResult runVersionCommandWithProcess(
    const QString& program,
    const QStringList& arguments,
    QString* output,
    const CoreUpdateService::CancelCheckHandler& cancelCheck)
{
    if (output == nullptr) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Core version output buffer is unavailable."));
    }

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(QFileInfo(program).absolutePath());
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();

    if (!process.waitForStarted(1500)) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to start the version check process: %1")
                .arg(process.errorString()));
    }

    int elapsedMs = 0;
    while (!process.waitForFinished(kCancellationPollIntervalMs)) {
        elapsedMs += kCancellationPollIntervalMs;
        if (isCancellationRequested(cancelCheck)) {
            process.kill();
            process.waitForFinished(1500);
            return cancelledResult();
        }

        if (elapsedMs >= 5000) {
            process.kill();
            process.waitForFinished(1500);
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "The version check process timed out."));
        }
    }

    *output = QString::fromUtf8(process.readAll()).trimmed();
    return OperationResult::ok();
}

OperationResult extractArchiveWithPowerShell(
    const QString& archivePath,
    const QString& extractionDirectory,
    const CoreUpdateService::CancelCheckHandler& cancelCheck)
{
    QDir().mkpath(extractionDirectory);

    const QString command = QStringLiteral(
                                "& { "
                                "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
                                "$src = '%1'; "
                                "$dst = '%2'; "
                                "if (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Recurse -Force; } "
                                "[System.IO.Compression.ZipFile]::ExtractToDirectory($src, $dst); "
                                " }")
                                .arg(quotePowerShellLiteral(QDir::toNativeSeparators(archivePath)))
                                .arg(quotePowerShellLiteral(QDir::toNativeSeparators(extractionDirectory)));

    QProcess process;
    process.setProgram(QStringLiteral("powershell"));
    process.setArguments(QStringList{
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        command});
    process.start();

    if (!process.waitForStarted(1500)) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to start PowerShell for archive extraction: %1")
                .arg(process.errorString()));
    }

    int elapsedMs = 0;
    while (!process.waitForFinished(kCancellationPollIntervalMs)) {
        elapsedMs += kCancellationPollIntervalMs;
        if (isCancellationRequested(cancelCheck)) {
            process.kill();
            process.waitForFinished(2000);
            return cancelledResult();
        }

        if (elapsedMs >= 120000) {
            process.kill();
            process.waitForFinished(2000);
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Archive extraction timed out."));
        }
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString errorText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return OperationResult::fail(
            errorText.isEmpty()
                ? QCoreApplication::translate("CoreUpdateService", "Archive extraction failed.")
                : errorText);
    }

    return OperationResult::ok();
}

} // namespace CoreUpdateOperations
