#include "common/GitHubReleaseParsing.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <utility>

namespace GitHubReleaseParsing {

ParseResult parseGitHubReleaseList(const QByteArray& payload)
{
    ParseResult result;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.status = ParseStatus::ParseError;
        result.parseError = parseError.errorString();
        return result;
    }

    if (!document.isArray()) {
        result.status = ParseStatus::NotArray;
        if (document.isObject()) {
            result.objectMessage = document
                .object()
                .value(QStringLiteral("message"))
                .toString()
                .trimmed();
        }
        return result;
    }

    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        Release release;
        release.tagName = object.value(QStringLiteral("tag_name")).toString().trimmed();
        release.name = object.value(QStringLiteral("name")).toString().trimmed();
        release.htmlUrl = QUrl(object.value(QStringLiteral("html_url")).toString().trimmed());
        release.prerelease = object.value(QStringLiteral("prerelease")).toBool(false);
        release.draft = object.value(QStringLiteral("draft")).toBool(false);

        const QJsonArray assets = object.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue& assetValue : assets) {
            if (!assetValue.isObject()) {
                continue;
            }

            const QJsonObject assetObject = assetValue.toObject();
            ReleaseAsset asset;
            asset.name = assetObject.value(QStringLiteral("name")).toString().trimmed();
            asset.downloadUrl = QUrl(
                assetObject.value(QStringLiteral("browser_download_url")).toString().trimmed());
            if (asset.downloadUrl.isValid()) {
                release.assets.append(std::move(asset));
            }
        }

        result.releases.append(std::move(release));
    }

    result.status = ParseStatus::Ok;
    return result;
}

} // namespace GitHubReleaseParsing
