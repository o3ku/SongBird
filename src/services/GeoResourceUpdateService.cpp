#include "services/GeoResourceUpdateService.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QSaveFile>

#include <utility>

#include "common/GitHubUrls.h"
#include "services/GeoResourceDownload.h"

namespace {

const QString kSingBoxRuleSetDirectoryName = QStringLiteral("rule-set");

QString normalizeGeoName(const QString& geoName)
{
    return geoName.trimmed().toLower();
}

QString normalizeRuleSetTag(const QString& tag)
{
    return tag.trimmed().toLower();
}

} // namespace

GeoResourceUpdateService::GeoResourceUpdateService(
    QString targetDirectory,
    DownloadHandler downloadHandler,
    ProgressHandler progressHandler)
    : targetDirectory_(std::move(targetDirectory))
    , downloadHandler_(std::move(downloadHandler))
    , progressHandler_(std::move(progressHandler))
{
}

OperationResult GeoResourceUpdateService::update(const QString& geoName) const
{
    const QString normalizedGeoName = normalizeGeoName(geoName);
    if (normalizedGeoName != QStringLiteral("geosite") && normalizedGeoName != QStringLiteral("geoip")) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Unsupported geo resource: %1.")
                .arg(geoName.trimmed()));
    }

    if (targetDirectory_.trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Geo target directory is unavailable."));
    }

    if (!QDir().mkpath(targetDirectory_)) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to create the geo target directory."));
    }

    const QString fileName = QStringLiteral("%1.dat").arg(normalizedGeoName);
    return downloadAndSave(
        buildDownloadUrl(normalizedGeoName),
        fileName,
        QDir(targetDirectory_).filePath(fileName));
}

OperationResult GeoResourceUpdateService::updateSingBoxRuleSet(const QString& tag) const
{
    const QString normalizedTag = normalizeRuleSetTag(tag);
    if (!normalizedTag.startsWith(QStringLiteral("geosite-"))
        && !normalizedTag.startsWith(QStringLiteral("geoip-"))) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Unsupported sing-box rule-set: %1.")
                .arg(tag.trimmed()));
    }

    if (targetDirectory_.trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Geo target directory is unavailable."));
    }

    const QString fileName = QStringLiteral("%1.srs").arg(normalizedTag);
    return downloadAndSave(
        buildSingBoxRuleSetDownloadUrl(normalizedTag),
        fileName,
        QDir(targetDirectory_).filePath(QStringLiteral("%1/%2").arg(kSingBoxRuleSetDirectoryName, fileName)));
}

OperationResult GeoResourceUpdateService::downloadAndSave(
    const QUrl& url,
    const QString& fileName,
    const QString& targetPath) const
{
    if (targetDirectory_.trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Geo target directory is unavailable."));
    }

    if (!QDir().mkpath(targetDirectory_)) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to create the geo target directory."));
    }

    reportProgress(
        QCoreApplication::translate("GeoResourceUpdateService", "Downloading %1...")
            .arg(fileName));

    QByteArray content;
    const OperationResult downloadResult = GeoResourceDownload::downloadBytes(
        url,
        fileName,
        &content,
        downloadHandler_,
        progressHandler_);
    if (!downloadResult.success) {
        return downloadResult;
    }

    if (content.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Downloaded %1 is empty.")
                .arg(fileName));
    }

    reportProgress(
        QCoreApplication::translate("GeoResourceUpdateService", "Saving %1...")
            .arg(QDir::toNativeSeparators(targetPath)));
    const QFileInfo targetInfo(targetPath);
    if (!targetInfo.dir().exists() && !QDir().mkpath(targetInfo.dir().absolutePath())) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to create the geo file directory."));
    }

    QSaveFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to open %1 for writing.")
                .arg(QDir::toNativeSeparators(targetPath)));
    }

    if (file.write(content) < 0) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to write %1.")
                .arg(QDir::toNativeSeparators(targetPath)));
    }

    if (!file.commit()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to save %1.")
                .arg(QDir::toNativeSeparators(targetPath)));
    }

    return OperationResult::ok(
        QCoreApplication::translate("GeoResourceUpdateService", "Updated %1.")
            .arg(QDir::toNativeSeparators(targetPath)));
}

void GeoResourceUpdateService::reportProgress(const QString& message) const
{
    if (progressHandler_ && !message.trimmed().isEmpty()) {
        progressHandler_(message);
    }
}

QUrl GeoResourceUpdateService::buildDownloadUrl(const QString& geoName)
{
    return githubLatestReleaseDownloadUrl(
        v2rayRulesDatRepositoryPath(),
        QStringLiteral("%1.dat").arg(normalizeGeoName(geoName)));
}

QUrl GeoResourceUpdateService::buildSingBoxRuleSetDownloadUrl(const QString& tag)
{
    const QString normalizedTag = normalizeRuleSetTag(tag);
    if (normalizedTag.startsWith(QStringLiteral("geosite-"))) {
        return singRuleSetDownloadUrl(singGeositeRepositoryPath(), normalizedTag);
    }
    if (normalizedTag.startsWith(QStringLiteral("geoip-"))) {
        return singRuleSetDownloadUrl(singGeoipRepositoryPath(), normalizedTag);
    }
    return {};
}
