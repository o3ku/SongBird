#include "services/AppUpdateReleaseMetadata.h"

#include <QCoreApplication>

#include "common/GitHubReleaseParsing.h"

const AppUpdateReleaseAsset* selectBestAppUpdateAsset(const QList<AppUpdateReleaseAsset>& assets)
{
    const AppUpdateReleaseAsset* bestAsset = nullptr;
    int bestScore = -1;
    for (const AppUpdateReleaseAsset& asset : assets) {
        const QString normalized = asset.name.trimmed().toLower();
        int score = -1;
        if (normalized.endsWith(QStringLiteral(".exe"))) {
            score = normalized.contains(QStringLiteral("setup"))
                || normalized.contains(QStringLiteral("install"))
                ? 500
                : 300;
        }

        if (!normalized.contains(QStringLiteral("windows"))
            && !normalized.contains(QStringLiteral("win"))
            && !normalized.contains(QStringLiteral("songbird"))) {
            score -= 100;
        }
        if (normalized.contains(QStringLiteral("source"))
            || normalized.contains(QStringLiteral("linux"))
            || normalized.contains(QStringLiteral("macos"))
            || normalized.contains(QStringLiteral("darwin"))
            || normalized.contains(QStringLiteral("android"))) {
            score = -1;
        }

        if (score > bestScore && asset.downloadUrl.isValid()) {
            bestScore = score;
            bestAsset = &asset;
        }
    }
    return bestScore >= 0 ? bestAsset : nullptr;
}

bool parseLatestAppRelease(
    const QByteArray& payload,
    bool allowPrerelease,
    AppUpdateRelease* release,
    QString* errorMessage)
{
    if (release == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("AppUpdateService", "Release output buffer is unavailable.");
        }
        return false;
    }

    const GitHubReleaseParsing::ParseResult parsed = GitHubReleaseParsing::parseGitHubReleaseList(payload);
    if (parsed.status == GitHubReleaseParsing::ParseStatus::ParseError) {
        if (errorMessage != nullptr) {
            *errorMessage = parsed.parseError;
        }
        return false;
    }
    if (parsed.status == GitHubReleaseParsing::ParseStatus::NotArray) {
        if (errorMessage != nullptr) {
            *errorMessage = parsed.objectMessage.isEmpty()
                ? QCoreApplication::translate("AppUpdateService", "Release metadata is invalid.")
                : parsed.objectMessage;
        }
        return false;
    }

    bool skippedOnlyPrerelease = false;
    for (const GitHubReleaseParsing::Release& parsedRelease : parsed.releases) {
        if (parsedRelease.draft) {
            continue;
        }
        if (!allowPrerelease && parsedRelease.prerelease) {
            skippedOnlyPrerelease = true;
            continue;
        }
        if (parsedRelease.tagName.isEmpty()) {
            continue;
        }

        AppUpdateRelease convertedRelease;
        convertedRelease.tagName = parsedRelease.tagName;
        convertedRelease.name = parsedRelease.name;
        convertedRelease.htmlUrl = parsedRelease.htmlUrl;
        convertedRelease.prerelease = parsedRelease.prerelease;
        convertedRelease.draft = parsedRelease.draft;
        for (const GitHubReleaseParsing::ReleaseAsset& asset : parsedRelease.assets) {
            if (asset.name.isEmpty()) {
                continue;
            }
            convertedRelease.assets.append(AppUpdateReleaseAsset{asset.name, asset.downloadUrl});
        }
        *release = convertedRelease;
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = skippedOnlyPrerelease && !allowPrerelease
            ? QCoreApplication::translate("AppUpdateService", "No stable releases were found.")
            : QCoreApplication::translate("AppUpdateService", "No releases were found.");
    }
    return false;
}
