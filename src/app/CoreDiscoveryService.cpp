#include "app/CoreDiscoveryService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include "common/ProcessRunner.h"
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

    ProcessRunner::Request request;
    request.program = program;
    request.arguments = arguments;
    request.workingDirectory = QFileInfo(program).absolutePath();
    request.startTimeoutMs = 1500;
    request.finishTimeoutMs = 5000;

    // A probe that cannot start or overruns simply means "no version". The exit
    // code stays unchecked on purpose: some cores print the version and exit
    // non-zero.
    const ProcessRunner::Outcome outcome = ProcessRunner::runToCompletion(request);
    if (outcome.status != ProcessRunner::Status::Completed) {
        return {};
    }

    return backend->extractVersionFromOutput(outcome.output);
}
