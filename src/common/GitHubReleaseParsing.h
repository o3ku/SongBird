#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

namespace GitHubReleaseParsing {

struct ReleaseAsset {
    QString name;
    QUrl downloadUrl;
    // The `digest` the releases API returns for the asset ("sha256:<hex>"). The checksum
    // verification trusts this first: it comes with the metadata over the same TLS connection,
    // and every GitHub-hosted asset has one whether or not the upstream publishes a checksum
    // file beside it.
    QString digest;
};

struct Release {
    QString tagName;
    QString name;
    QUrl htmlUrl;
    bool prerelease = false;
    bool draft = false;
    QList<ReleaseAsset> assets;
};

enum class ParseStatus {
    Ok,
    ParseError,
    NotArray,
};

struct ParseResult {
    ParseStatus status = ParseStatus::ParseError;
    QString parseError;
    QString objectMessage;
    QList<Release> releases;
};

// Shared GitHub releases JSON walker for the app-update and core-update
// metadata parsers. Filtering (drafts/prereleases/empty tags) and user-facing
// error text stay with the callers because each owns a distinct translation
// context.
ParseResult parseGitHubReleaseList(const QByteArray& payload);

} // namespace GitHubReleaseParsing
