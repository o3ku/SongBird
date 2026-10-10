#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "common/OperationResult.h"
#include "runtime/core/CoreAssetPlatform.h"
#include "services/CoreUpdateService.h"

namespace CoreUpdateOperations {

void reportProgress(const CoreUpdateService::ProgressHandler& progressHandler, const QString& message);
bool isCancellationRequested(const CoreUpdateService::CancelCheckHandler& cancelCheck);
OperationResult cancelledResult();
bool isCancelledResult(const OperationResult& result);

OperationResult downloadBytesWithNetwork(
    const QUrl& url,
    QByteArray* content,
    const QString& userAgent,
    int timeoutMs,
    const CoreUpdateService::CancelCheckHandler& cancelCheck,
    const CoreUpdateService::ProgressHandler& progressHandler = {});

OperationResult runVersionCommandWithProcess(
    const QString& program,
    const QStringList& arguments,
    QString* output,
    const CoreUpdateService::CancelCheckHandler& cancelCheck);

// One helper process invocation, built from the platform rather than behind an #if.
//
// The commands are data for the same reason CoreAssetPlatform is a value: no CI job runs the test
// suite on macOS, so a macOS branch written as a compile-time branch would ship unexecuted by any
// test. As data, the command the other platform would run can be asserted from any host.
struct HelperCommand {
    QString program;
    QStringList arguments;
    // Empty unless the helper writes its payload to stdout, in which case the runner redirects
    // stdout to this path. `gzip -dc` does; the Windows implementation writes its own output file
    // and leaves this empty.
    QString standardOutputFile;
};

// How a helper process ended, reported raw.
//
// Raw rather than as an OperationResult because the caller owns the wording: the strings are
// already in the translation file and differ per operation ("Archive extraction failed." versus
// "Gzip extraction failed."), so collapsing them here would either lose that or force the runner
// to know which of the two it is running.
struct HelperProcessOutcome {
    enum class Status {
        Completed,
        StartFailed,
        Cancelled,
        TimedOut,
        Failed,
    };

    Status status = Status::Completed;
    // The process's own text: errorString() when it could not be started, stderr when it ran and
    // exited non-zero.
    QString errorText;
};

// `platform` selects between the PowerShell implementation and the POSIX one, so the same command
// the macOS build would run can be asserted on a Windows host and vice versa.
HelperCommand archiveExtractionCommand(
    CoreAssetPlatform platform,
    const QString& archivePath,
    const QString& extractionDirectory);
HelperCommand tarGzExtractionCommand(
    CoreAssetPlatform platform,
    const QString& archivePath,
    const QString& extractionDirectory);
HelperCommand gzipDecompressionCommand(
    CoreAssetPlatform platform,
    const QString& archivePath,
    const QString& outputPath);

HelperProcessOutcome runHelperProcess(
    const HelperCommand& command,
    int timeoutMs,
    const CoreUpdateService::CancelCheckHandler& cancelCheck);

OperationResult extractArchive(
    const QString& archivePath,
    const QString& extractionDirectory,
    const CoreUpdateService::CancelCheckHandler& cancelCheck);
OperationResult extractTarGzArchive(
    const QString& archivePath,
    const QString& extractionDirectory,
    const CoreUpdateService::CancelCheckHandler& cancelCheck);
OperationResult decompressGzip(
    const QString& archivePath,
    const QString& targetPath,
    const CoreUpdateService::CancelCheckHandler& cancelCheck);

} // namespace CoreUpdateOperations
