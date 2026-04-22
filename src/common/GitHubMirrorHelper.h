#pragma once

#include <QList>
#include <QString>
#include <QUrl>

inline QStringList githubMirrorPrefixes()
{
    return {
        QStringLiteral("https://mirror.ghproxy.com/"),
        QStringLiteral("https://ghproxy.net/"),
        QStringLiteral("https://ghfast.top/")
    };
}

inline bool isGitHubOriginUrl(const QUrl& url)
{
    const QString host = url.host().trimmed().toLower();
    return host == QStringLiteral("github.com")
        || host == QStringLiteral("api.github.com")
        || host.endsWith(QStringLiteral(".github.com"))
        || host.endsWith(QStringLiteral(".githubusercontent.com"));
}

inline QList<QUrl> buildGitHubMirrorCandidateUrls(const QUrl& url)
{
    QList<QUrl> result;
    if (!url.isValid() || url.scheme().trimmed().isEmpty()) {
        return result;
    }

    result.append(url);
    if (!isGitHubOriginUrl(url)) {
        return result;
    }

    const QString encodedUrl = url.toString(QUrl::FullyEncoded);
    for (const QString& prefix : githubMirrorPrefixes()) {
        result.append(QUrl(prefix + encodedUrl));
    }

    return result;
}
