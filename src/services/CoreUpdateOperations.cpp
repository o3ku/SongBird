#include "services/CoreUpdateOperations.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QTimer>

#include <memory>

#include "services/ServiceTimeouts.h"

namespace {

constexpr int kCancellationPollIntervalMs = ServiceTimeouts::kCancellationPollIntervalMs;

// How long an extraction or decompression helper may run before it is killed. Both the Windows and
// the POSIX path use the same budget: the work is the same, only the tool differs.
constexpr int kHelperTimeoutMs = 120000;

QString quotePowerShellLiteral(QString value)
{
    value.replace(QChar('\''), QStringLiteral("''"));
    return value;
}

QString powershellProgram()
{
    return QStringLiteral("powershell");
}

QStringList powershellArguments(const QString& command)
{
    return QStringList{
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"),
        QStringLiteral("Bypass"),
        QStringLiteral("-Command"),
        command};
}

// The tools macOS ships for the two archive formats the cores arrive in. ditto is used rather than
// unzip because it is what restores the permission bits inside a .app-like tree, and the core's
// executable bit is exactly what the extraction has to preserve.
QString macosDittoProgram()
{
    return QStringLiteral("/usr/bin/ditto");
}

QString macosTarProgram()
{
    return QStringLiteral("/usr/bin/tar");
}

QString macosGzipProgram()
{
    return QStringLiteral("/usr/bin/gzip");
}

// Maps a raw helper outcome onto the operation's own wording.
//
// `windowsStartFailureMessage` carries the platform-specific sentence used when the helper could
// not even be launched -- Windows names PowerShell there, because a user may have to act on it --
// and is empty on the other platforms, where the neutral sentence is reused with the operating
// system's reason appended.
OperationResult helperOutcomeToResult(
    const CoreUpdateOperations::HelperProcessOutcome& outcome,
    const QString& windowsStartFailureMessage,
    const QString& failureMessage,
    const QString& timeoutMessage,
    const OperationResult& cancelledOutcome)
{
    using Status = CoreUpdateOperations::HelperProcessOutcome::Status;

    switch (outcome.status) {
    case Status::Completed:
        return OperationResult::ok();
    case Status::StartFailed:
        return OperationResult::fail(
            windowsStartFailureMessage.isEmpty()
                ? failureMessage + QLatin1Char(' ') + outcome.errorText
                : windowsStartFailureMessage.arg(outcome.errorText));
    case Status::Cancelled:
        return cancelledOutcome;
    case Status::TimedOut:
        return OperationResult::fail(timeoutMessage);
    case Status::Failed:
        break;
    }

    return OperationResult::fail(outcome.errorText.isEmpty() ? failureMessage : outcome.errorText);
}

// The Windows message a helper that would not start is reported with, or an empty string on the
// platforms that have no PowerShell to name.
QString windowsStartFailureMessageFor(CoreAssetPlatform platform, const QString& message)
{
    return platform.isWindows() ? message : QString();
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

HelperCommand archiveExtractionCommand(
    CoreAssetPlatform platform,
    const QString& archivePath,
    const QString& extractionDirectory)
{
    if (platform.isWindows()) {
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
        return HelperCommand{powershellProgram(), powershellArguments(command), {}};
    }

    // -x extracts, -k says the archive is a PKZip rather than a directory tree.
    return HelperCommand{
        macosDittoProgram(),
        QStringList{QStringLiteral("-x"), QStringLiteral("-k"), archivePath, extractionDirectory},
        {}};
}

HelperCommand tarGzExtractionCommand(
    CoreAssetPlatform platform,
    const QString& archivePath,
    const QString& extractionDirectory)
{
    // Windows 10 and later ship bsdtar as System32\tar.exe, so one argument list covers both
    // platforms; only the program is resolved differently. None of the three cores publishes a
    // .tar.gz for Windows, so this path exists for the macOS build alone.
    return HelperCommand{
        platform.isWindows() ? QStringLiteral("tar") : macosTarProgram(),
        QStringList{QStringLiteral("-xzf"), archivePath, QStringLiteral("-C"), extractionDirectory},
        {}};
}

HelperCommand gzipDecompressionCommand(
    CoreAssetPlatform platform,
    const QString& archivePath,
    const QString& outputPath)
{
    if (platform.isWindows()) {
        // The Windows form writes the output file itself, so the staging path is embedded in the
        // command rather than handed over as a redirect.
        const QString command = QStringLiteral(
                                    "& { "
                                    "$src = '%1'; "
                                    "$dst = '%2'; "
                                    "$srcStream = [System.IO.File]::OpenRead($src); "
                                    "try { "
                                    "  $gzip = [System.IO.Compression.GZipStream]::new($srcStream, [System.IO.Compression.CompressionMode]::Decompress); "
                                    "  try { "
                                    "    $output = [System.IO.File]::Create($dst); "
                                    "    try { $gzip.CopyTo($output); } finally { $output.Dispose(); } "
                                    "  } finally { $gzip.Dispose(); } "
                                    "} finally { $srcStream.Dispose(); } "
                                    " }")
                                    .arg(quotePowerShellLiteral(QDir::toNativeSeparators(archivePath)))
                                    .arg(quotePowerShellLiteral(QDir::toNativeSeparators(outputPath)));
        return HelperCommand{powershellProgram(), powershellArguments(command), {}};
    }

    // -d decompresses, -c writes to stdout: the payload is the core binary itself, so it has to
    // land in a file rather than beside the archive.
    return HelperCommand{
        macosGzipProgram(),
        QStringList{QStringLiteral("-dc"), archivePath},
        outputPath};
}

HelperProcessOutcome runHelperProcess(
    const HelperCommand& command,
    int timeoutMs,
    const CoreUpdateService::CancelCheckHandler& cancelCheck)
{
    HelperProcessOutcome outcome;

    QProcess process;
    process.setProgram(command.program);
    process.setArguments(command.arguments);
    if (!command.standardOutputFile.isEmpty()) {
        process.setStandardOutputFile(command.standardOutputFile);
    }
    process.start();

    if (!process.waitForStarted(1500)) {
        outcome.status = HelperProcessOutcome::Status::StartFailed;
        outcome.errorText = process.errorString();
        return outcome;
    }

    int elapsedMs = 0;
    while (!process.waitForFinished(kCancellationPollIntervalMs)) {
        elapsedMs += kCancellationPollIntervalMs;
        if (isCancellationRequested(cancelCheck)) {
            process.kill();
            process.waitForFinished(2000);
            outcome.status = HelperProcessOutcome::Status::Cancelled;
            return outcome;
        }

        if (elapsedMs >= timeoutMs) {
            process.kill();
            process.waitForFinished(2000);
            outcome.status = HelperProcessOutcome::Status::TimedOut;
            return outcome;
        }
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        outcome.status = HelperProcessOutcome::Status::Failed;
        outcome.errorText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return outcome;
    }

    return outcome;
}

OperationResult extractArchive(
    const QString& archivePath,
    const QString& extractionDirectory,
    const CoreUpdateService::CancelCheckHandler& cancelCheck)
{
    QDir().mkpath(extractionDirectory);

    const CoreAssetPlatform platform = currentAssetPlatform();
    const HelperProcessOutcome outcome = runHelperProcess(
        archiveExtractionCommand(platform, archivePath, extractionDirectory),
        kHelperTimeoutMs,
        cancelCheck);

    return helperOutcomeToResult(
        outcome,
        windowsStartFailureMessageFor(
            platform,
            QCoreApplication::translate(
                "CoreUpdateService", "Failed to start PowerShell for archive extraction: %1")),
        QCoreApplication::translate("CoreUpdateService", "Archive extraction failed."),
        QCoreApplication::translate("CoreUpdateService", "Archive extraction timed out."),
        cancelledResult());
}

OperationResult extractTarGzArchive(
    const QString& archivePath,
    const QString& extractionDirectory,
    const CoreUpdateService::CancelCheckHandler& cancelCheck)
{
    QDir().mkpath(extractionDirectory);

    const CoreAssetPlatform platform = currentAssetPlatform();
    const HelperProcessOutcome outcome = runHelperProcess(
        tarGzExtractionCommand(platform, archivePath, extractionDirectory),
        kHelperTimeoutMs,
        cancelCheck);

    return helperOutcomeToResult(
        outcome,
        windowsStartFailureMessageFor(
            platform,
            QCoreApplication::translate(
                "CoreUpdateService", "Failed to start PowerShell for archive extraction: %1")),
        QCoreApplication::translate("CoreUpdateService", "Archive extraction failed."),
        QCoreApplication::translate("CoreUpdateService", "Archive extraction timed out."),
        cancelledResult());
}

OperationResult decompressGzip(
    const QString& archivePath,
    const QString& targetPath,
    const CoreUpdateService::CancelCheckHandler& cancelCheck)
{
    // Decompresses into a staging file next to the target and only swaps it into place once
    // extraction succeeds, so a cancelled or failed update never truncates or removes an already
    // installed core executable.
    const QString stagingPath = targetPath + QStringLiteral(".part");
    QFile::remove(stagingPath);

    const CoreAssetPlatform platform = currentAssetPlatform();
    const HelperProcessOutcome outcome = runHelperProcess(
        gzipDecompressionCommand(platform, archivePath, stagingPath),
        kHelperTimeoutMs,
        cancelCheck);

    const OperationResult outcomeResult = helperOutcomeToResult(
        outcome,
        windowsStartFailureMessageFor(
            platform,
            QCoreApplication::translate(
                "CoreUpdateService", "Failed to start PowerShell for gzip extraction: %1")),
        QCoreApplication::translate("CoreUpdateService", "Gzip extraction failed."),
        QCoreApplication::translate("CoreUpdateService", "Gzip extraction timed out."),
        OperationResult::cancel(
            QCoreApplication::translate("CoreUpdateService", "Gzip extraction was canceled.")));

    if (!outcomeResult.success) {
        // Never leave a partial ".part" behind, whatever ended the helper.
        QFile::remove(stagingPath);
        return outcomeResult;
    }

    if (!QFileInfo::exists(stagingPath)) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Gzip extraction produced no output file."));
    }

    QFile::remove(targetPath);
    if (!QFile::rename(stagingPath, targetPath)) {
        QFile::remove(stagingPath);
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to install the extracted file to %1.")
                .arg(QDir::toNativeSeparators(targetPath)));
    }

    return OperationResult::ok();
}

} // namespace CoreUpdateOperations
