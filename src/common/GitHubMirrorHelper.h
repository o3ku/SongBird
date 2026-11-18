#pragma once

#include <QList>
#include <QString>
#include <QUrl>

// GitHub mirrors, and which downloads may use them.
//
// The three mirrors are INTENTIONAL, not a leftover: in regions where github.com is
// unreachable or unusably slow, downloads fall back to them so updates still work at all.
// A mirror is still a man in the middle -- it serves whatever bytes it chooses -- so a
// mirror may only ever carry BYTES, never trust. The checksum that vouches for those bytes
// is anchored to the official GitHub channel instead (see ReleaseChecksum.h): release
// metadata is fetched official-first, and a checksum is only an official anchor when the
// metadata that named it arrived from github.com. When GitHub itself is unreachable the
// install falls back to a mirror-supplied checksum, which only proves the bytes are
// self-consistent with what the mirror said, and the install says so loudly. That
// availability-over-verification tradeoff is deliberate; see the CHANGELOG entry for fix 3.
inline QStringList githubMirrorPrefixes()
{
    return {
        QStringLiteral("https://ghfast.top/"),
        QStringLiteral("https://mirror.ghproxy.com/"),
        QStringLiteral("https://ghproxy.net/")
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

// Download candidates for a GitHub URL, mirrors first and the official URL last: the
// mirrors exist so a large download finishes at all where GitHub is unreachable, and the
// bytes are verified afterwards against the official anchor, never against the mirror
// that served them.
inline QList<QUrl> buildGitHubMirrorCandidateUrls(const QUrl& url)
{
    QList<QUrl> result;
    if (!url.isValid() || url.scheme().trimmed().isEmpty()) {
        return result;
    }

    if (!isGitHubOriginUrl(url)) {
        result.append(url);
        return result;
    }

    const QString encodedUrl = url.toString(QUrl::FullyEncoded);
    for (const QString& prefix : githubMirrorPrefixes()) {
        result.append(QUrl(prefix + encodedUrl));
    }
    result.append(url);

    return result;
}

// Metadata candidates for a GitHub URL, official first and the mirrors as fallback.
// Release metadata is small, so one official attempt is cheap -- and it is the attempt
// that decides whether the checksum anchor can be trusted at all. A mirror that answers
// first would hand us a digest and checksum URLs it chose itself, which no later check
// could tell apart from GitHub's own. The cost is one metadata timeout in regions where
// GitHub is unreachable, after which the mirrors answer as before.
inline QList<QUrl> buildGitHubOfficialFirstCandidateUrls(const QUrl& url)
{
    QList<QUrl> result;
    if (!url.isValid() || url.scheme().trimmed().isEmpty()) {
        return result;
    }

    if (!isGitHubOriginUrl(url)) {
        result.append(url);
        return result;
    }

    result.append(url);
    const QString encodedUrl = url.toString(QUrl::FullyEncoded);
    for (const QString& prefix : githubMirrorPrefixes()) {
        result.append(QUrl(prefix + encodedUrl));
    }

    return result;
}
