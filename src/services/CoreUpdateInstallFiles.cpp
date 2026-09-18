#include "services/CoreUpdateInstallFiles.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include "common/JsonFile.h"
#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/ICoreBackend.h"

namespace {

bool containsWildcard(const QString& value)
{
    return value.contains(QChar('*')) || value.contains(QChar('?'));
}

bool isGeoDataFile(const QString& fileName)
{
    const QString normalized = fileName.trimmed().toLower();
    return normalized.startsWith(QStringLiteral("geo")) && normalized.endsWith(QStringLiteral(".dat"));
}

QString resolveExtractionRoot(const QString& extractionDirectory)
{
    QDir dir(extractionDirectory);
    if (!dir.exists()) {
        return extractionDirectory;
    }

    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoSymLinks | QDir::Readable);
    const QFileInfoList directories = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (files.isEmpty() && directories.size() == 1) {
        return directories.constFirst().absoluteFilePath();
    }

    return extractionDirectory;
}

} // namespace

QString CoreUpdateInstallFiles::findFirstExistingFile(const QString& directoryPath, const QStringList& patterns)
{
    for (const QString& pattern : patterns) {
        const QString absolutePattern = QDir(directoryPath).filePath(pattern);
        const QFileInfo info(absolutePattern);
        if (!containsWildcard(info.fileName())) {
            if (QFileInfo::exists(absolutePattern)) {
                return QDir::toNativeSeparators(QFileInfo(absolutePattern).absoluteFilePath());
            }
            continue;
        }

        QDir dir(info.path());
        if (!dir.exists()) {
            continue;
        }

        const QFileInfoList matches = dir.entryInfoList(
            QStringList{info.fileName()},
            QDir::Files | QDir::NoSymLinks | QDir::Readable,
            QDir::Name | QDir::IgnoreCase);
        if (!matches.isEmpty()) {
            return QDir::toNativeSeparators(matches.constFirst().absoluteFilePath());
        }
    }

    return {};
}

bool CoreUpdateInstallFiles::hasAnyInstalledCore(const QString& targetDirectory)
{
    for (const ICoreBackend* backend : coreBackends()) {
        if (backend == nullptr || backend->executableNames().isEmpty()) {
            continue;
        }

        if (!findFirstExistingFile(targetDirectory, backend->executableNames()).isEmpty()) {
            return true;
        }
    }

    return false;
}

OperationResult CoreUpdateInstallFiles::writeBytesToFile(const QString& filePath, const QByteArray& content)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.dir().exists() && !QDir().mkpath(fileInfo.dir().absolutePath())) {
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to create the target directory for %1.")
                .arg(QDir::toNativeSeparators(filePath)));
    }

    const JsonFile::WriteResult written = JsonFile::writeFileAtomically(filePath, content);
    if (!written.ok) {
        const QString path = QDir::toNativeSeparators(filePath);
        switch (written.stage) {
        case JsonFile::WriteFailureStage::Open:
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Failed to open %1 for writing.").arg(path));
        case JsonFile::WriteFailureStage::Write:
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Failed to write %1.").arg(path));
        case JsonFile::WriteFailureStage::Commit:
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Failed to save %1.").arg(path));
        case JsonFile::WriteFailureStage::None:
            break;
        }
        return OperationResult::fail(
            QCoreApplication::translate("CoreUpdateService", "Failed to save %1.").arg(path));
    }

    return OperationResult::ok();
}

OperationResult CoreUpdateInstallFiles::copyExtractedFiles(
    const QString& extractionDirectory,
    const QString& targetDirectory,
    bool ignoreGeoFiles)
{
    const QString sourceRoot = resolveExtractionRoot(extractionDirectory);
    QDirIterator iterator(sourceRoot, QDir::Files | QDir::NoSymLinks | QDir::Readable, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString sourcePath = iterator.next();
        const QFileInfo sourceInfo(sourcePath);
        if (ignoreGeoFiles && isGeoDataFile(sourceInfo.fileName())) {
            continue;
        }

        QFile sourceFile(sourcePath);
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            return OperationResult::fail(
                QCoreApplication::translate("CoreUpdateService", "Failed to open %1 for reading.")
                    .arg(QDir::toNativeSeparators(sourcePath)));
        }

        const QString relativePath = QDir(sourceRoot).relativeFilePath(sourcePath);
        const QString targetPath = QDir(targetDirectory).filePath(relativePath);
        const OperationResult writeResult = writeBytesToFile(targetPath, sourceFile.readAll());
        if (!writeResult.success) {
            return writeResult;
        }
    }

    return OperationResult::ok();
}
