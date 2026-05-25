#include "app/AppUpdateCheckPresentation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace {

QString unknownVersionText()
{
    return QCoreApplication::translate("AppBootstrap", "Unknown");
}

} // namespace

namespace AppUpdateCheckPresentation {

QString buildUpdateAvailablePrompt(const AppUpdateCheckResult& updateResult, const QString& resultMessage)
{
    QString prompt = QCoreApplication::translate(
        "AppBootstrap",
        "%1\n\nCurrent version: %2\nLatest version: %3\n\nDownload the update in the background?")
                         .arg(resultMessage)
                         .arg(updateResult.currentVersion.trimmed().isEmpty()
                             ? unknownVersionText()
                             : updateResult.currentVersion)
                         .arg(updateResult.latestVersion);
    if (!updateResult.releaseName.trimmed().isEmpty()) {
        prompt.append(QStringLiteral("\n"));
        prompt.append(QCoreApplication::translate("AppBootstrap", "Release: %1")
                          .arg(updateResult.releaseName.trimmed()));
    }
    if (!updateResult.assetName.trimmed().isEmpty()) {
        prompt.append(QStringLiteral("\n"));
        prompt.append(QCoreApplication::translate("AppBootstrap", "Package: %1")
                          .arg(updateResult.assetName.trimmed()));
    }

    return prompt;
}

QString appendMissingDownloadPrompt(const QString& prompt)
{
    QString message = prompt;
    message.append(QStringLiteral("\n\n"));
    message.append(QCoreApplication::translate(
        "AppBootstrap",
        "No downloadable Windows executable was found. Open the release page instead?"));
    return message;
}

QString resolveDownloadAssetName(const QString& assetName)
{
    return QFileInfo(assetName).suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("SongBird.new.exe")
        : assetName;
}

QString buildDownloadedPackageMessage(const QString& savedPath, const OperationResult& result)
{
    return QCoreApplication::translate(
        "AppBootstrap",
        "%1\n\nRestart SongBird and use the downloaded package to update to the new version.")
            .arg(savedPath.trimmed().isEmpty()
                ? result.message
                : QCoreApplication::translate("AppBootstrap", "Downloaded package: %1")
                      .arg(QDir::toNativeSeparators(savedPath)));
}

} // namespace AppUpdateCheckPresentation
