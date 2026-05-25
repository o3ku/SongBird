#include "services/AppUpdateReleaseMetadata.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = parseError.errorString();
        }
        return false;
    }
    if (!document.isArray()) {
        if (errorMessage != nullptr) {
            const QString message = document.isObject()
                ? document.object().value(QStringLiteral("message")).toString().trimmed()
                : QString();
            *errorMessage = message.isEmpty()
                ? QCoreApplication::translate("AppUpdateService", "Release metadata is invalid.")
                : message;
        }
        return false;
    }

    bool skippedOnlyPrerelease = false;
    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        const bool draft = object.value(QStringLiteral("draft")).toBool(false);
        const bool prerelease = object.value(QStringLiteral("prerelease")).toBool(false);
        if (draft) {
            continue;
        }
        if (!allowPrerelease && prerelease) {
            skippedOnlyPrerelease = true;
            continue;
        }

        const QString tagName = object.value(QStringLiteral("tag_name")).toString().trimmed();
        const QUrl htmlUrl(object.value(QStringLiteral("html_url")).toString().trimmed());
        if (tagName.isEmpty()) {
            continue;
        }

        AppUpdateRelease parsedRelease{
            tagName,
            object.value(QStringLiteral("name")).toString().trimmed(),
            htmlUrl,
            prerelease,
            draft,
            {}};
        const QJsonArray assets = object.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue& assetValue : assets) {
            const QJsonObject assetObject = assetValue.toObject();
            const QString assetName = assetObject.value(QStringLiteral("name")).toString().trimmed();
            const QUrl downloadUrl(assetObject.value(QStringLiteral("browser_download_url")).toString().trimmed());
            if (!assetName.isEmpty() && downloadUrl.isValid()) {
                parsedRelease.assets.append(AppUpdateReleaseAsset{assetName, downloadUrl});
            }
        }
        *release = parsedRelease;
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = skippedOnlyPrerelease && !allowPrerelease
            ? QCoreApplication::translate("AppUpdateService", "No stable releases were found.")
            : QCoreApplication::translate("AppUpdateService", "No releases were found.");
    }
    return false;
}
