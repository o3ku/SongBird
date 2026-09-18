#include "services/CoreUpdateReleaseMetadata.h"

#include <QCoreApplication>

#include <utility>

#include "common/GitHubReleaseParsing.h"
#include "common/GitHubUrls.h"
#include "runtime/core/ICoreBackend.h"

namespace CoreUpdateReleaseMetadata {

GitHubRelease buildBuiltInFallbackRelease(const ICoreBackend& backend, bool prefer64Bit)
{
    const CoreUpdateAssetPolicy policy = backend.updateAssetPolicy();
    if (policy.builtInFallbackTagName.isEmpty() || policy.builtInFallbackRepositoryPath.isEmpty()) {
        return {};
    }

    const QString assetName = prefer64Bit
        ? policy.builtInFallbackAssetName64
        : policy.builtInFallbackAssetName32;
    if (assetName.isEmpty()) {
        return {};
    }

    GitHubRelease release;
    release.tagName = policy.builtInFallbackTagName;
    release.assets.append(GitHubReleaseAsset{
        assetName,
        githubReleaseDownloadUrl(policy.builtInFallbackRepositoryPath, policy.builtInFallbackTagName, assetName)});
    return release;
}

bool parseGitHubReleasePayload(
    const QByteArray& payload,
    bool allowPrerelease,
    GitHubRelease* release,
    QString* errorMessage,
    bool* stableReleaseUnavailable)
{
    if (release == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("CoreUpdateService", "Release output buffer is unavailable.");
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
                ? QCoreApplication::translate("CoreUpdateService", "Release metadata is invalid.")
                : parsed.objectMessage;
        }
        return false;
    }

    bool skippedOnlyPrerelease = false;
    for (const GitHubReleaseParsing::Release& parsedRelease : parsed.releases) {
        if (!allowPrerelease && parsedRelease.prerelease) {
            skippedOnlyPrerelease = true;
            continue;
        }

        GitHubRelease convertedRelease;
        convertedRelease.tagName = parsedRelease.tagName;
        convertedRelease.prerelease = parsedRelease.prerelease;
        for (const GitHubReleaseParsing::ReleaseAsset& asset : parsedRelease.assets) {
            convertedRelease.assets.append(GitHubReleaseAsset{asset.name, asset.downloadUrl});
        }

        if (!convertedRelease.tagName.isEmpty()) {
            *release = std::move(convertedRelease);
            return true;
        }
    }

    if (stableReleaseUnavailable != nullptr) {
        *stableReleaseUnavailable = skippedOnlyPrerelease && !allowPrerelease;
    }

    if (errorMessage != nullptr) {
        *errorMessage = skippedOnlyPrerelease && !allowPrerelease
            ? QCoreApplication::translate("CoreUpdateService", "No stable releases were found.")
            : allowPrerelease
            ? QCoreApplication::translate("CoreUpdateService", "No releases were found.")
            : QCoreApplication::translate("CoreUpdateService", "No stable releases were found.");
    }
    return false;
}

const GitHubReleaseAsset* selectBestReleaseAsset(
    const ICoreBackend& backend,
    const QList<GitHubReleaseAsset>& assets,
    bool prefer64Bit)
{
    const GitHubReleaseAsset* bestAsset = nullptr;
    int bestScore = -1;

    for (const GitHubReleaseAsset& asset : assets) {
        const int score = backend.scoreReleaseAssetName(asset.name, prefer64Bit);
        if (score > bestScore) {
            bestScore = score;
            bestAsset = &asset;
        }
    }

    return bestAsset;
}

} // namespace CoreUpdateReleaseMetadata
