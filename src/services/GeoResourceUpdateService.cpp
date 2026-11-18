#include "services/GeoResourceUpdateService.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QSaveFile>

#include <utility>

#include "common/GitHubMirrorHelper.h"
#include "common/GitHubReleaseParsing.h"
#include "common/GitHubUrls.h"
#include "common/JsonFile.h"
#include "common/ReleaseChecksum.h"
#include "common/SingBoxPaths.h"
#include "services/GeoResourceDownload.h"

namespace {

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
        QDir(targetDirectory_).filePath(fileName),
        v2rayRulesDatRepositoryPath(),
        fileName);
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
        QDir(targetDirectory_).filePath(QStringLiteral("%1/%2").arg(SingBoxPaths::ruleSetDirectoryName(), fileName)),
        normalizedTag.startsWith(QStringLiteral("geosite-"))
            ? singGeositeRepositoryPath()
            : singGeoipRepositoryPath(),
        fileName);
}

OperationResult GeoResourceUpdateService::updateCoreGeoFile(
    const CoreGeoFileRequirement& requirement) const
{
    const QString fileName = requirement.fileName.trimmed();
    if (fileName.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Geo file name is unavailable."));
    }

    if (requirement.sourceFileName.trimmed().isEmpty()
        || requirement.repositoryPath.trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "No download source for geo file %1.")
                .arg(fileName));
    }

    return downloadAndSave(
        githubLatestReleaseDownloadUrl(requirement.repositoryPath, requirement.sourceFileName),
        fileName,
        QDir(targetDirectory_).filePath(fileName),
        requirement.repositoryPath,
        requirement.sourceFileName);
}

GeoResourceUpdateService::ExpectedGeoDigest GeoResourceUpdateService::expectedDigestForGeoFile(
    const QString& repositoryPath,
    const QString& sourceFileName) const
{
    QByteArray payload;
    QUrl answeredFrom;
    // Quiet: the metadata fetch is a precondition for the download, not a download source the
    // user is choosing between. Official first: the digest is only an anchor when GitHub
    // itself reported it -- a mirror could report any digest for the file it serves.
    const OperationResult fetchResult = GeoResourceDownload::downloadBytes(
        githubReleasesApiUrl(repositoryPath, 5),
        QStringLiteral("release metadata"),
        &payload,
        downloadHandler_,
        {},
        /*officialFirst=*/true,
        &answeredFrom);
    if (!fetchResult.success || payload.isEmpty()) {
        return {};
    }

    const GitHubReleaseParsing::ParseResult parsed = GitHubReleaseParsing::parseGitHubReleaseList(payload);
    if (parsed.status != GitHubReleaseParsing::ParseStatus::Ok) {
        return {};
    }
    // The file URL resolves to the latest stable release, so prereleases and drafts are skipped
    // here too -- a prerelease could name a different asset set.
    for (const GitHubReleaseParsing::Release& release : parsed.releases) {
        if (release.draft || release.prerelease || release.tagName.isEmpty()) {
            continue;
        }
        for (const GitHubReleaseParsing::ReleaseAsset& asset : release.assets) {
            if (asset.name == sourceFileName) {
                ExpectedGeoDigest digest;
                digest.hex = ReleaseChecksum::digestToHex(asset.digest);
                digest.fromOfficial = isGitHubOriginUrl(answeredFrom);
                return digest;
            }
        }
    }
    return {};
}

OperationResult GeoResourceUpdateService::downloadAndSave(
    const QUrl& url,
    const QString& fileName,
    const QString& targetPath,
    const QString& repositoryPath,
    const QString& sourceFileName) const
{
    if (targetDirectory_.trimmed().isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Geo target directory is unavailable."));
    }

    if (!QDir().mkpath(targetDirectory_)) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to create the geo target directory."));
    }

    // The expected checksum is fetched before the file so a release that cannot be verified
    // fails fast instead of after a download.
    QString expectedDigest;
    bool digestFromOfficial = false;
    QString digestError;
    if (repositoryPath.trimmed().isEmpty()) {
        digestError = QCoreApplication::translate(
            "GeoResourceUpdateService", "No repository is known for geo file %1.").arg(fileName);
    } else {
        const ExpectedGeoDigest expected = expectedDigestForGeoFile(repositoryPath, sourceFileName);
        expectedDigest = expected.hex;
        digestFromOfficial = expected.fromOfficial;
        if (expectedDigest.isEmpty()) {
            digestError = QCoreApplication::translate(
                "GeoResourceUpdateService",
                "No SHA-256 checksum is available for %1 in the latest %2 release.")
                .arg(sourceFileName)
                .arg(repositoryPath);
        }
    }
    if (!digestError.isEmpty()) {
        return OperationResult::fail(
            QCoreApplication::translate(
                "GeoResourceUpdateService",
                "Geo file %1 cannot be verified: %2 The file was not saved.")
                .arg(fileName)
                .arg(digestError));
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

    if (!ReleaseChecksum::matchesSha256(content, expectedDigest)) {
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "SHA-256 checksum mismatch for geo file %1: the downloaded file does not match the checksum published for %2 (expected %3, got %4). The file was not saved.")
                .arg(fileName)
                .arg(sourceFileName)
                .arg(expectedDigest)
                .arg(ReleaseChecksum::sha256Hex(content)));
    }

    // A digest that arrived through a mirror only proves the file agrees with the mirror,
    // not with GitHub: a geodata file the core trusts blindly is a routing decision made by
    // whoever served the bytes, so the install says so loudly instead of silently.
    if (!digestFromOfficial) {
        reportProgress(
            QCoreApplication::translate("GeoResourceUpdateService", "Warning: GitHub was unreachable, so %1 was verified against a checksum from a download mirror only. A mirror can serve any bytes it chooses, so keep the file only if you trust the mirror that served it.")
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

    const JsonFile::WriteResult written = JsonFile::writeFileAtomically(targetPath, content);
    if (!written.ok) {
        const QString path = QDir::toNativeSeparators(targetPath);
        switch (written.stage) {
        case JsonFile::WriteFailureStage::Open:
            return OperationResult::fail(
                QCoreApplication::translate("GeoResourceUpdateService", "Failed to open %1 for writing.").arg(path));
        case JsonFile::WriteFailureStage::Write:
            return OperationResult::fail(
                QCoreApplication::translate("GeoResourceUpdateService", "Failed to write %1.").arg(path));
        case JsonFile::WriteFailureStage::Commit:
            return OperationResult::fail(
                QCoreApplication::translate("GeoResourceUpdateService", "Failed to save %1.").arg(path));
        case JsonFile::WriteFailureStage::None:
            break;
        }
        return OperationResult::fail(
            QCoreApplication::translate("GeoResourceUpdateService", "Failed to save %1.").arg(path));
    }

    QString updatedMessage =
        QCoreApplication::translate("GeoResourceUpdateService", "Updated %1.")
            .arg(QDir::toNativeSeparators(targetPath));
    if (!digestFromOfficial) {
        updatedMessage += QStringLiteral(" ") + QCoreApplication::translate(
            "GeoResourceUpdateService",
            "Warning: GitHub was unreachable, so this file was verified against a checksum from a download mirror only.");
    }

    return OperationResult::ok(updatedMessage);
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
