#include "services/CoreUpdatePackageInstallation.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include "services/CoreUpdateInstallFiles.h"
#include "services/CoreUpdateOperations.h"

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
    if (asset.name.trimmed().toLower().endsWith(QStringLiteral(".zip"))) {
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
