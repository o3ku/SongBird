#include "common/ReleaseChecksum.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QRegularExpression>

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
    const QByteArray& packageBytes,
    const QString& assetName,
    const QUrl& packageDownloadUrl,
    const QString& apiDigest,
    const std::optional<NamedAsset>& checksumAsset,
    const std::function<OperationResult(const QUrl& url, QByteArray* content)>& fetchChecksum)
{
    const QString expectedFromApi = digestToHex(apiDigest);
    if (!expectedFromApi.isEmpty()) {
        VerificationOutcome outcome;
        outcome.expectedHex = expectedFromApi;
        outcome.actualHex = sha256Hex(packageBytes);
        outcome.status = outcome.actualHex == outcome.expectedHex
            ? Verification::Verified
            : Verification::Mismatched;
        return outcome;
    }

    // Every remaining source is a checksum document fetched over the network, so each one is
    // tried in turn and a fetch failure just moves on to the next -- a missing checksum file is
    // normal for upstreams that do not publish one, and only the absence of all of them is
    // Unverifiable.
    QList<QUrl> checksumUrls;
    if (checksumAsset.has_value() && checksumAsset->downloadUrl.isValid()) {
        checksumUrls.append(checksumAsset->downloadUrl);
    }
    for (const QUrl& derivedUrl : derivedChecksumUrls(packageDownloadUrl)) {
        if (!checksumUrls.contains(derivedUrl)) {
            checksumUrls.append(derivedUrl);
        }
    }

    const QString actualHex = sha256Hex(packageBytes);
    for (const QUrl& checksumUrl : checksumUrls) {
        if (!fetchChecksum) {
            break;
        }
        QByteArray document;
        if (!fetchChecksum(checksumUrl, &document).success) {
            continue;
        }
        const std::optional<QString> expected = expectedSha256ForAsset(document, assetName);
        if (!expected.has_value()) {
            continue;
        }
        VerificationOutcome outcome;
        outcome.expectedHex = *expected;
        outcome.actualHex = actualHex;
        outcome.status = outcome.actualHex == outcome.expectedHex
            ? Verification::Verified
            : Verification::Mismatched;
        return outcome;
    }

    return VerificationOutcome{};
}

} // namespace ReleaseChecksum
