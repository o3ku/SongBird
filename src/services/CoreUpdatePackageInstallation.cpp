#include "services/CoreUpdatePackageInstallation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "services/CoreUpdateInstallFiles.h"
#include "services/CoreUpdateOperations.h"

namespace {

QString installedFileNameForGzipAsset(const QString& assetName)
{
    // A bare gzip holds the core itself rather than an archive of it, and the mihomo names carry the
    // platform and microarchitecture instead of the executable name, so the two platforms that have
    // one each get their own mapping.
    const QString normalized = assetName.trimmed().toLower();
    if (normalized.startsWith(QStringLiteral("mihomo-windows-"))) {
        return QStringLiteral("mihomo.exe");
    }
    if (normalized.startsWith(QStringLiteral("mihomo-darwin-"))) {
        return QStringLiteral("mihomo");
    }

    QString fileName = QFileInfo(assetName).fileName();
    if (fileName.endsWith(QStringLiteral(".gz"), Qt::CaseInsensitive)) {
        fileName.chop(3);
    }
    return fileName;
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
    // ".tar.gz" has to be tested before ".gz": it ends with the shorter suffix too, and treating a
    // tarball as a bare gzip stream would decompress it into one file named after the archive.
    const bool isTarGz = normalizedAssetName.endsWith(QStringLiteral(".tar.gz"));
    const bool isZip = normalizedAssetName.endsWith(QStringLiteral(".zip"));

    if (isZip || isTarGz) {
        const QString extractionDirectory = temporaryDirectory.filePath(QStringLiteral("extracted"));
        UpdateOps::reportProgress(
            progressHandler,
            QCoreApplication::translate("CoreUpdateService", "Extracting %1...")
                .arg(asset.name));
        const OperationResult extractResult = archiveExtractor
            ? archiveExtractor(packagePath, extractionDirectory)
            : isTarGz
            ? UpdateOps::extractTarGzArchive(packagePath, extractionDirectory, cancelCheck)
            : UpdateOps::extractArchive(packagePath, extractionDirectory, cancelCheck);
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
            : UpdateOps::decompressGzip(packagePath, targetPath, cancelCheck);
        if (UpdateOps::isCancelledResult(applyResult)) {
            return applyResult;
        }
        if (applyResult.success) {
            // The payload is the core itself, so the file that was just swapped into place needs the
            // bit that makes it runnable.
            InstallFiles::markExecutable(targetPath);
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
