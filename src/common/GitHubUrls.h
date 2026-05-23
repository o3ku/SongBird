#pragma once

#include <QString>
#include <QUrl>

inline QString githubRepositoryUrl(const QString& repositoryPath)
{
    return QStringLiteral("https://github.com/%1").arg(repositoryPath);
}

inline QString githubReleasesPageUrl(const QString& repositoryPath)
{
    return QStringLiteral("%1/releases").arg(githubRepositoryUrl(repositoryPath));
}

inline QUrl githubReleasesApiUrl(const QString& repositoryPath, int perPage)
{
    return QUrl(QStringLiteral("https://api.github.com/repos/%1/releases?per_page=%2")
                    .arg(repositoryPath)
                    .arg(perPage));
}

inline QUrl githubReleaseDownloadUrl(
    const QString& repositoryPath,
    const QString& releaseTag,
    const QString& assetName)
{
    return QUrl(QStringLiteral("%1/download/%2/%3")
                    .arg(githubReleasesPageUrl(repositoryPath))
                    .arg(releaseTag)
                    .arg(assetName));
}

inline QUrl githubLatestReleaseDownloadUrl(const QString& repositoryPath, const QString& assetName)
{
    return githubReleaseDownloadUrl(repositoryPath, QStringLiteral("latest"), assetName);
}

inline QUrl githubRawContentUrl(const QString& repositoryPath, const QString& branch, const QString& path)
{
    return QUrl(QStringLiteral("https://raw.githubusercontent.com/%1/%2/%3")
                    .arg(repositoryPath)
                    .arg(branch)
                    .arg(path));
}

inline QString songbirdRepositoryPath()
{
    return QStringLiteral("o3ku/songbird");
}

inline QString xrayRepositoryPath()
{
    return QStringLiteral("XTLS/Xray-core");
}

inline QString singBoxRepositoryPath()
{
    return QStringLiteral("SagerNet/sing-box");
}

inline QString v2rayRulesDatRepositoryPath()
{
    return QStringLiteral("Loyalsoldier/v2ray-rules-dat");
}

inline QString singGeositeRepositoryPath()
{
    return QStringLiteral("SagerNet/sing-geosite");
}

inline QString singGeoipRepositoryPath()
{
    return QStringLiteral("SagerNet/sing-geoip");
}

inline QString songbirdProjectPageUrl()
{
    return githubRepositoryUrl(songbirdRepositoryPath());
}

inline QString songbirdReleasePageUrl()
{
    return githubReleasesPageUrl(songbirdRepositoryPath());
}

inline QUrl songbirdReleasesApiUrl()
{
    return githubReleasesApiUrl(songbirdRepositoryPath(), 10);
}

inline QString xrayReleasePageUrl()
{
    return githubReleasesPageUrl(xrayRepositoryPath());
}

inline QString singBoxReleasePageUrl()
{
    return githubReleasesPageUrl(singBoxRepositoryPath());
}

inline QString v2rayRulesDatReleasePageUrl()
{
    return githubReleasesPageUrl(v2rayRulesDatRepositoryPath());
}

inline QUrl singRuleSetDownloadUrl(const QString& repositoryPath, const QString& tag)
{
    return githubRawContentUrl(repositoryPath, QStringLiteral("rule-set"), QStringLiteral("%1.srs").arg(tag));
}
