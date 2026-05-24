#include "app/CoreDiscoveryService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include "runtime/core/CoreBackendRegistry.h"
#include "runtime/core/CoreCatalog.h"
#include "runtime/core/ICoreBackend.h"

namespace {

QStringList buildCoreCandidateDirectories(const QString& configPath)
{
    QStringList directories;
    const auto appendDirectory = [&directories](const QString& path) {
        if (path.trimmed().isEmpty()) {
            return;
        }

        const QString directory = QDir(path).absolutePath();
        if (!directory.isEmpty() && !directories.contains(directory, Qt::CaseInsensitive)) {
            directories.append(directory);
        }
    };

    appendDirectory(QDir::currentPath());
    appendDirectory(QCoreApplication::applicationDirPath());
    appendDirectory(QFileInfo(configPath).dir().absolutePath());
    return directories;
}

QStringList coreCandidateFileNames(const QStringList& candidates)
{
    QStringList candidateNames;
    for (const QString& candidate : candidates) {
        const QString fileName = QFileInfo(candidate).fileName();
        if (!fileName.isEmpty() && !candidateNames.contains(fileName, Qt::CaseInsensitive)) {
            candidateNames.append(fileName);
        }
    }
    return candidateNames;
}

} // namespace

QStringList CoreDiscoveryService::resolveCoreCandidates(CoreType coreType, const QString& configPath) const
{
    const QStringList directories = buildCoreCandidateDirectories(configPath);
    const QStringList fileNames = catalogCoreExecutableNames(coreType);

    QStringList candidates;
    for (const QString& directory : directories) {
        for (const QString& fileName : fileNames) {
            candidates.append(QDir(directory).filePath(fileName));
        }
    }
    return candidates;
}

QString CoreDiscoveryService::locateFirstExistingFile(const QStringList& candidates) const
{
    for (const QString& candidate : candidates) {
        const QFileInfo candidateInfo(candidate);
        const QString fileName = candidateInfo.fileName();
        if (fileName.contains(QChar('*')) || fileName.contains(QChar('?'))) {
            QDir candidateDir(candidateInfo.path());
            if (!candidateDir.exists()) {
                continue;
            }

            const QFileInfoList matches = candidateDir.entryInfoList(
                QStringList{fileName},
                QDir::Files | QDir::NoSymLinks | QDir::Readable,
                QDir::Name | QDir::IgnoreCase);
            if (!matches.isEmpty()) {
                return QDir::toNativeSeparators(matches.constFirst().absoluteFilePath());
            }
            continue;
        }

        if (QFileInfo::exists(candidate)) {
            return QDir::toNativeSeparators(QFileInfo(candidate).absoluteFilePath());
        }
    }

    return {};
}

QString CoreDiscoveryService::expectedCoreFilesText(const QStringList& candidates) const
{
    const QStringList candidateNames = coreCandidateFileNames(candidates);
    return candidateNames.isEmpty()
        ? QCoreApplication::translate("AppBootstrap", "(unknown)")
        : candidateNames.join(QStringLiteral(", "));
}

QString CoreDiscoveryService::detectCoreVersion(CoreType coreType, const QString& program) const
{
    const ICoreBackend* backend = coreBackend(coreType);
    const QStringList arguments = backend != nullptr ? backend->versionCommandArguments() : QStringList{};
    if (program.trimmed().isEmpty() || backend == nullptr || arguments.isEmpty()) {
        return {};
    }

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(QFileInfo(program).absolutePath());
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();

    if (!process.waitForStarted(1500)) {
        return {};
    }

    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1500);
        return {};
    }

    return backend->extractVersionFromOutput(QString::fromUtf8(process.readAll()));
}
