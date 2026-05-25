#include "services/CoreUpdateReleaseMetadata.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

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

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = parseError.errorString();
        }
        return false;
    }

    if (!document.isArray()) {
        if (document.isObject()) {
            const QString message = document.object().value(QStringLiteral("message")).toString().trimmed();
            if (errorMessage != nullptr) {
                *errorMessage = message.isEmpty()
                    ? QCoreApplication::translate("CoreUpdateService", "Release metadata is invalid.")
                    : message;
            }
        } else if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("CoreUpdateService", "Release metadata is invalid.");
        }
        return false;
    }

    const QJsonArray releases = document.array();
    bool skippedOnlyPrerelease = false;
    for (const QJsonValue& value : releases) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        const bool prerelease = object.value(QStringLiteral("prerelease")).toBool(false);
        if (!allowPrerelease && prerelease) {
            skippedOnlyPrerelease = true;
            continue;
        }

        GitHubRelease parsedRelease;
        parsedRelease.tagName = object.value(QStringLiteral("tag_name")).toString().trimmed();
        parsedRelease.prerelease = prerelease;
        const QJsonArray assets = object.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue& assetValue : assets) {
            if (!assetValue.isObject()) {
                continue;
            }

            const QJsonObject assetObject = assetValue.toObject();
            const QUrl downloadUrl(assetObject.value(QStringLiteral("browser_download_url")).toString().trimmed());
            if (!downloadUrl.isValid()) {
                continue;
            }

            parsedRelease.assets.append(GitHubReleaseAsset{
                assetObject.value(QStringLiteral("name")).toString().trimmed(),
                downloadUrl});
        }

        if (!parsedRelease.tagName.isEmpty()) {
            *release = std::move(parsedRelease);
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
