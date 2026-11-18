#include "common/ReleaseChecksum.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QRegularExpression>

#include "common/GitHubMirrorHelper.h"

namespace ReleaseChecksum {

namespace {

bool isHex64(const QString& text)
{
    if (text.size() != 64) {
        return false;
    }
    for (const QChar ch : text) {
        if (!ch.isDigit() && (ch.toLower() < QLatin1Char('a') || ch.toLower() > QLatin1Char('f'))) {
            return false;
        }
    }
    return true;
}

QString baseName(const QString& fileName)
{
    return QFileInfo(fileName).fileName();
}

VerificationOutcome comparedOutcome(
    Verification verifiedStatus,
    const QString& expectedHex,
    const QString& actualHex)
{
    VerificationOutcome outcome;
    outcome.expectedHex = expectedHex;
    outcome.actualHex = actualHex;
    outcome.status = actualHex == expectedHex ? verifiedStatus : Verification::Mismatched;
    return outcome;
}

// Tries each checksum URL in turn and returns the outcome for the first document that
// names the asset, or Unverifiable when none does. `expandUrl` maps one checksum URL to
// the URLs to fetch for it: the identity for the official pass, the mirror candidates
// for the degraded pass.
VerificationOutcome verifyAgainstChecksumUrls(
    Verification verifiedStatus,
    const QString& assetName,
    const QString& actualHex,
    const QList<QUrl>& checksumUrls,
    const std::function<QList<QUrl>(const QUrl&)>& expandUrl,
    const FetchChecksum& fetchChecksum)
{
    if (!fetchChecksum) {
        return VerificationOutcome{};
    }
    for (const QUrl& checksumUrl : checksumUrls) {
        for (const QUrl& fetchUrl : expandUrl(checksumUrl)) {
            QByteArray document;
            if (!fetchChecksum(fetchUrl, &document).success) {
                continue;
            }
            const std::optional<QString> expected = expectedSha256ForAsset(document, assetName);
            if (!expected.has_value()) {
                continue;
            }
            return comparedOutcome(verifiedStatus, *expected, actualHex);
        }
    }
    return VerificationOutcome{};
}

} // namespace

QString sha256Hex(const QByteArray& data)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString digestToHex(const QString& digest)
{
    const QString trimmed = digest.trimmed();
    const QString withoutPrefix = trimmed.startsWith(QStringLiteral("sha256:"), Qt::CaseInsensitive)
        ? trimmed.mid(QStringLiteral("sha256:").size())
        : trimmed;
    const QString lowered = withoutPrefix.toLower();
    return isHex64(lowered) ? lowered : QString();
}

bool matchesSha256(const QByteArray& data, const QString& expectedDigest)
{
    const QString expectedHex = digestToHex(expectedDigest);
    return !expectedHex.isEmpty() && sha256Hex(data) == expectedHex;
}

std::optional<NamedAsset> findChecksumAsset(
    const QList<NamedAsset>& assets,
    const QString& assetName)
{
    const QString sha256Name = assetName + QStringLiteral(".sha256");
    const QString dgstName = assetName + QStringLiteral(".dgst");
    const NamedAsset* sha256Asset = nullptr;
    const NamedAsset* dgstAsset = nullptr;
    const NamedAsset* checksumsTxt = nullptr;
    for (const NamedAsset& asset : assets) {
        if (asset.name == sha256Name && asset.downloadUrl.isValid()) {
            sha256Asset = &asset;
        } else if (asset.name == dgstName && asset.downloadUrl.isValid()) {
            dgstAsset = &asset;
        } else if (asset.name == QStringLiteral("checksums.txt") && asset.downloadUrl.isValid()) {
            checksumsTxt = &asset;
        }
    }
    // A per-asset file wins over the shared checksums.txt, and .sha256 wins over .dgst,
    // regardless of the order the API listed them in.
    if (sha256Asset != nullptr) {
        return *sha256Asset;
    }
    if (dgstAsset != nullptr) {
        return *dgstAsset;
    }
    if (checksumsTxt != nullptr) {
        return *checksumsTxt;
    }
    return std::nullopt;
}

std::optional<QString> expectedSha256ForAsset(const QByteArray& document, const QString& assetName)
{
    // sha256sum / goreleaser: "<hex>  <filename>" or "<hex> *<filename>" (binary mode).
    static const QRegularExpression coreutilsLine(
        QStringLiteral(R"(^([0-9a-fA-F]{64})[ \t]+\*?(\S+)$)"));
    // openssl dgst: "SHA256 (filename) = <hex>" or "SHA2-256 (filename) = <hex>"; the
    // parenthesised filename is optional because Xray's workflow strips it with sed.
    static const QRegularExpression dgstLine(
        QStringLiteral(R"(^SHA-?2?-?256[ \t]*(?:\(([^)]+)\))?[ \t]*=[ \t]*([0-9a-fA-F]{64})$)"),
        QRegularExpression::CaseInsensitiveOption);

    const QStringList lines = QString::fromUtf8(document).split(QChar('\n'));
    QString singleBareHex;
    int nonEmptyLines = 0;
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        ++nonEmptyLines;

        const QRegularExpressionMatch coreutilsMatch = coreutilsLine.match(line);
        if (coreutilsMatch.hasMatch()) {
            if (baseName(coreutilsMatch.captured(2)) == assetName) {
                return coreutilsMatch.captured(1).toLower();
            }
            continue;
        }

        const QRegularExpressionMatch dgstMatch = dgstLine.match(line);
        if (dgstMatch.hasMatch()) {
            const QString fileName = dgstMatch.captured(1);
            if (fileName.isEmpty() || baseName(fileName) == assetName) {
                return dgstMatch.captured(2).toLower();
            }
            continue;
        }

        if (isHex64(line)) {
            singleBareHex = line.toLower();
        }
    }

    // A file holding one bare hash can only name the asset it sits beside.
    if (!singleBareHex.isEmpty() && nonEmptyLines == 1) {
        return singleBareHex;
    }
    return std::nullopt;
}

QList<QUrl> derivedChecksumUrls(const QUrl& packageDownloadUrl)
{
    QList<QUrl> urls;
    if (!packageDownloadUrl.isValid()) {
        return urls;
    }
    const QString packageUrl = packageDownloadUrl.toString(QUrl::FullyEncoded);
    urls.append(QUrl(packageUrl + QStringLiteral(".sha256")));
    urls.append(QUrl(packageUrl + QStringLiteral(".dgst")));
    const int lastSlash = packageUrl.lastIndexOf(QChar('/'));
    if (lastSlash > 0) {
        urls.append(QUrl(packageUrl.left(lastSlash + 1) + QStringLiteral("checksums.txt")));
    }
    return urls;
}

VerificationOutcome verifyDownloadedBytes(
    const VerificationRequest& request,
    const FetchChecksum& fetchChecksum)
{
    // The API digest is only as trustworthy as the metadata that carried it: a mirror
    // can report any digest it likes for the bytes it serves.
    if (request.metadataFromOfficial) {
        const QString expectedFromApi = digestToHex(request.apiDigest);
        if (!expectedFromApi.isEmpty()) {
            return comparedOutcome(
                Verification::Verified, expectedFromApi, sha256Hex(request.packageBytes));
        }
    }

    // Every remaining source is a checksum document fetched over the network. A document
    // only anchors the bytes when it comes from GitHub itself: the checksum asset the
    // metadata named (which a mirror could point anywhere) and the URLs derived from the
    // package URL (whose host a mirror could equally have chosen) are both gated on a
    // GitHub origin host, and on the metadata being official -- or, for the bootstrap,
    // on the package URL coming from code rather than from metadata at all.
    QList<QUrl> checksumUrls;
    if (request.metadataFromOfficial
        && request.checksumAsset.has_value()
        && isGitHubOriginUrl(request.checksumAsset->downloadUrl)) {
        checksumUrls.append(request.checksumAsset->downloadUrl);
    }
    const bool derivedUrlsAnchored =
        request.metadataFromOfficial || request.packageUrlFromCode;
    if (derivedUrlsAnchored) {
        for (const QUrl& derivedUrl : derivedChecksumUrls(request.packageDownloadUrl)) {
            if (isGitHubOriginUrl(derivedUrl) && !checksumUrls.contains(derivedUrl)) {
                checksumUrls.append(derivedUrl);
            }
        }
    }

    // A missing checksum file is normal for upstreams that do not publish one, and only
    // the absence of all of them is Unverifiable. Documents are fetched exactly as named:
    // routing a checksum fetch through a mirror would let the mirror substitute the
    // document the anchor is read from.
    const QString actualHex = sha256Hex(request.packageBytes);
    return verifyAgainstChecksumUrls(
        Verification::Verified,
        request.assetName,
        actualHex,
        checksumUrls,
        [](const QUrl& url) { return QList<QUrl>{url}; },
        fetchChecksum);
}

VerificationOutcome verifyDownloadedBytesViaMirror(
    const VerificationRequest& request,
    const FetchChecksum& fetchChecksum)
{
    // Degraded pass only: GitHub itself is unreachable, so the only checksum available
    // is one a mirror vouches for. The API digest is mirror-supplied here too -- using
    // it proves the bytes agree with the mirror, nothing more -- and every checksum URL
    // is tried through the mirror candidates as well, so a checksum document GitHub
    // would have served can still be reached through the mirror that answers.
    // A match is MirrorVerified, never Verified, and callers must say so loudly.
    const QString expectedFromApi = digestToHex(request.apiDigest);
    if (!expectedFromApi.isEmpty()) {
        return comparedOutcome(
            Verification::MirrorVerified, expectedFromApi, sha256Hex(request.packageBytes));
    }

    QList<QUrl> checksumUrls;
    if (request.checksumAsset.has_value() && request.checksumAsset->downloadUrl.isValid()) {
        checksumUrls.append(request.checksumAsset->downloadUrl);
    }
    for (const QUrl& derivedUrl : derivedChecksumUrls(request.packageDownloadUrl)) {
        if (!checksumUrls.contains(derivedUrl)) {
            checksumUrls.append(derivedUrl);
        }
    }

    const QString actualHex = sha256Hex(request.packageBytes);
    return verifyAgainstChecksumUrls(
        Verification::MirrorVerified,
        request.assetName,
        actualHex,
        checksumUrls,
        [](const QUrl& url) { return buildGitHubMirrorCandidateUrls(url); },
        fetchChecksum);
}

} // namespace ReleaseChecksum
