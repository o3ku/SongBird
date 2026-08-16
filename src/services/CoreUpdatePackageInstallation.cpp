#include "services/CoreUpdatePackageInstallation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include "services/CoreUpdateInstallFiles.h"
#include "services/CoreUpdateOperations.h"

namespace {

QString quotePowerShellLiteral(QString value)
{
    value.replace(QChar('\''), QStringLiteral("''"));
    return value;
}

QString installedFileNameForGzipAsset(const QString& assetName)
{
    const QString normalized = assetName.trimmed().toLower();
    if (normalized.startsWith(QStringLiteral("mihomo-windows-"))) {
        return QStringLiteral("mihomo.exe");
    }

    QString fileName = QFileInfo(assetName).fileName();
    if (fileName.endsWith(QStringLiteral(".gz"), Qt::CaseInsensitive)) {
        fileName.chop(3);
    }
    return fileName;
}

// Decompresses into a staging file next to the target and only swaps it into place
// once extraction succeeds, so a cancelled or failed update never truncates or
// removes an already installed core executable.
OperationResult extractGzipWithPowerShell(
    const QString& packagePath,
    const QString& targetPath,
    const CoreUpdateService::CancelCheckHandler& cancelCheck)
{
    const QString stagingPath = targetPath + QStringLiteral(".part");
    QFile::remove(stagingPath);

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
                                .arg(quotePowerShellLiteral(QDir::toNativeSeparators(packagePath)))
                                .arg(quotePowerShellLiteral(QDir::toNativeSeparators(stagingPath)));

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
            QCoreApplication::translate("CoreUpdateService", "Failed to start PowerShell for gzip extraction: %1")
                .arg(process.errorString()));
    }

    constexpr int kPollIntervalMs = 100;
    int elapsedMs = 0;
    while (!process.waitForFinished(kPollIntervalMs)) {
        elapsedMs += kPollIntervalMs;
        if (cancelCheck && cancelCheck()) {
            process.kill();
            process.waitForFinished(2000);
            QFile::remove(stagingPath);
            return OperationResult::cancel(
                QCoreApplication::translate("CoreUpdateService", "Gzip extraction was canceled."));
        }

        if (elapsedMs >= 120000) {
            process.kill();
            process.waitForFinished(2000);
            QFile::remove(stagingPath);
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Gzip extraction timed out."));
        }
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString errorText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        QFile::remove(stagingPath);
        return OperationResult::fail(
            errorText.isEmpty()
                ? QCoreApplication::translate("CoreUpdateService", "Gzip extraction failed.")
                : errorText);
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

} // namespace

OperationResult CoreUpdatePackageInstallation::installPackage(
    const QString& targetDirectory,
    const CoreUpdateReleaseMetadata::GitHubReleaseAsset& asset,
    const QByteArray& packageBytes,
    bool ignoreGeoUpdateCore,
    const CoreUpdateService::ArchiveExtractor& archiveExtractor,
    const CoreUpdateService::CancelCheckHandler& cancelCheck,
    const CoreUpdateService::ProgressHandler& progressHandler)
{
    namespace InstallFiles = CoreUpdateInstallFiles;
    namespace UpdateOps = CoreUpdateOperations;

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to create a temporary working directory."));
    }

    const QString packagePath = temporaryDirectory.filePath(asset.name);
    UpdateOps::reportProgress(
        progressHandler,
        QCoreApplication::translate("CoreUpdateService", "Saving the update package to a temporary file..."));
    const OperationResult packageWriteResult = InstallFiles::writeBytesToFile(packagePath, packageBytes);
    if (!packageWriteResult.success) {
        return packageWriteResult;
    }

    if (UpdateOps::isCancellationRequested(cancelCheck)) {
        return UpdateOps::cancelledResult();
    }

    OperationResult applyResult;
    const QString normalizedAssetName = asset.name.trimmed().toLower();
    if (normalizedAssetName.endsWith(QStringLiteral(".zip"))) {
        const QString extractionDirectory = temporaryDirectory.filePath(QStringLiteral("extracted"));
        UpdateOps::reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Extracting %1...")
                .arg(asset.name));
        const OperationResult extractResult = archiveExtractor
            ? archiveExtractor(packagePath, extractionDirectory)
            : UpdateOps::extractArchiveWithPowerShell(packagePath, extractionDirectory, cancelCheck);
        if (UpdateOps::isCancelledResult(extractResult)) {
            return extractResult;
        }
        if (!extractResult.success) {
            return extractResult;
        }

        UpdateOps::reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Installing files to %1")
                .arg(QDir::toNativeSeparators(targetDirectory)));
        applyResult = InstallFiles::copyExtractedFiles(
            extractionDirectory,
            targetDirectory,
            ignoreGeoUpdateCore);
    } else if (normalizedAssetName.endsWith(QStringLiteral(".gz"))) {
        const QString installedFileName = installedFileNameForGzipAsset(asset.name);
        if (installedFileName.trimmed().isEmpty()) {
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "The gzip package name is invalid."));
        }

        const QString targetPath = QDir(targetDirectory).filePath(installedFileName);
        if (!QDir().mkpath(QFileInfo(targetPath).dir().absolutePath())) {
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Failed to create the target directory for %1.")
                    .arg(QDir::toNativeSeparators(targetPath)));
        }

        UpdateOps::reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Extracting %1...")
                .arg(asset.name));
        applyResult = archiveExtractor
            ? archiveExtractor(packagePath, targetPath)
            : extractGzipWithPowerShell(packagePath, targetPath, cancelCheck);
        if (UpdateOps::isCancelledResult(applyResult)) {
            return applyResult;
        }
    } else {
        UpdateOps::reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Installing files to %1")
                .arg(QDir::toNativeSeparators(targetDirectory)));
        applyResult = InstallFiles::writeBytesToFile(
            QDir(targetDirectory).filePath(asset.name),
            packageBytes);
    }

    if (UpdateOps::isCancellationRequested(cancelCheck)) {
        return UpdateOps::cancelledResult();
    }

    return applyResult;
}
